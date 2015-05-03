//
// The Epoch Language Project
// Epoch Development Tools - LLVM wrapper library
//
// CODEGENCONTEXT.CPP
// Implementation of the code emission context wrapper
//


#include "Pch.h"

#include "CodeGenContext.h"
#include <sstream>


using namespace CodeGen;
using namespace llvm;


////////////////////////
class TrivialMemoryManager : public RTDyldMemoryManager {
public:
	TrivialMemoryManager(CodeGen::ThunkCallbackT funcptr, CodeGen::StringCallbackT strptr, uint64_t* outAddr, size_t* outSize)
		: ThunkCallback(funcptr),
		  StringCallback(strptr),
		  OutAddr(outAddr),
		  OutSize(outSize)
	{ }

	ThunkCallbackT ThunkCallback;
	StringCallbackT StringCallback;

	uint64_t* OutAddr;
	size_t* OutSize;



  SmallVector<sys::MemoryBlock, 16> FunctionMemory;
  SmallVector<sys::MemoryBlock, 16> DataMemory;

  uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                               unsigned SectionID, StringRef SectionName);
  uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                               unsigned SectionID, StringRef SectionName,
                               bool IsReadOnly);

  virtual void *getPointerToNamedFunction(const std::string &Name,
                                          bool AbortOnFailure = true) {
    return 0;
  }

  bool finalizeMemory(std::string *ErrMsg) { return false; }

  // Invalidate instruction cache for sections with execute permissions.
  // Some platforms with separate data cache and instruction cache require
  // explicit cache flush, otherwise JIT code manipulations (like resolved
  // relocations) will get to the data cache but not to the instruction cache.
  virtual void invalidateInstructionCache();


	virtual uint64_t getSymbolAddress(const std::string & foo) override
	{
		if(foo.substr(0, 21) == "@epoch_static_string:")
		{
			size_t handle = 0;

			std::stringstream convert;
			convert << foo.substr(21);
			convert >> handle;

			return StringCallback(handle);
		}
		else
		{
			std::wstring wide(foo.begin(), foo.end());
			size_t offset = ThunkCallback(wide.c_str());
			return offset;
		}
	}
};

uint8_t *TrivialMemoryManager::allocateCodeSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID,
                                                   StringRef SectionName) {
  sys::MemoryBlock MB = sys::Memory::AllocateRWX(Size, 0, 0);
  FunctionMemory.push_back(MB);

  *OutAddr = (uint64_t)MB.base();
  *OutSize = Size;


  return (uint8_t*)MB.base();
}

uint8_t *TrivialMemoryManager::allocateDataSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID,
                                                   StringRef SectionName,
                                                   bool IsReadOnly) {
  sys::MemoryBlock MB = sys::Memory::AllocateRWX(Size, 0, 0);
  DataMemory.push_back(MB);
  return (uint8_t*)MB.base();
}

void TrivialMemoryManager::invalidateInstructionCache() {
  for (int i = 0, e = FunctionMemory.size(); i != e; ++i)
    sys::Memory::InvalidateInstructionCache(FunctionMemory[i].base(),
                                            FunctionMemory[i].size());

  for (int i = 0, e = DataMemory.size(); i != e; ++i)
    sys::Memory::InvalidateInstructionCache(DataMemory[i].base(),
                                            DataMemory[i].size());
}


////////////////////////



Context::Context()
	: ThunkCallback(nullptr),
	  StringCallback(nullptr),
	  LLVMBuilder(getGlobalContext()),
	  EntryPointFunction(nullptr),
	  LLVMModule(new Module("EpochModule", getGlobalContext()))
{
	FunctionType* initfunctiontype = FunctionType::get(Type::getInt32Ty(getGlobalContext()), false);
	InitFunction = Function::Create(initfunctiontype, GlobalValue::InternalLinkage, "@init", LLVMModule.get());
}


Context::~Context()
{
}


void Context::FunctionQueueParamType(llvm::Type* ty)
{
	PendingParamTypes.push_back(ty);
}


llvm::FunctionType* Context::FunctionTypeCreate(llvm::Type* rettype)
{
	llvm::FunctionType* fty = FunctionType::get(rettype, PendingParamTypes, false);
	PendingParamTypes.clear();

	return fty;
}

llvm::Function* Context::FunctionCreate(const char* name, llvm::FunctionType* fty)
{
	return Function::Create(fty, GlobalValue::ExternalLinkage, name, LLVMModule.get());
}

llvm::GlobalVariable* Context::FunctionCreateThunk(const char* name, llvm::FunctionType* fty)
{
	llvm::GlobalVariable* var = CachedThunkFunctions[name];
	if(!var)
	{
		var = new GlobalVariable(*LLVMModule, fty->getPointerTo(), true, GlobalValue::ExternalWeakLinkage, NULL, name, NULL, GlobalVariable::NotThreadLocal, 0, true);
		CachedThunkFunctions[name] = var;
	}

	return var;
}


void Context::SetEntryFunction(llvm::Function* entryfunc)
{
	EntryPointFunction = entryfunc;
}


void Context::SetThunkCallback(void* funcptr)
{
	ThunkCallbackT castptr = reinterpret_cast<ThunkCallbackT>(funcptr);
	ThunkCallback = castptr;
}

void Context::SetStringCallback(void* funcptr)
{
	StringCallbackT castptr = reinterpret_cast<StringCallbackT>(funcptr);
	StringCallback = castptr;
}


llvm::Type* Context::TypeGetBoolean()
{
	return Type::getInt1Ty(getGlobalContext());
}

llvm::Type* Context::TypeGetString()
{
	return Type::getInt8PtrTy(getGlobalContext());
}

llvm::Type* Context::TypeGetVoid()
{
	return Type::getVoidTy(getGlobalContext());
}



extern "C" void LLVMLinkInMCJIT();



size_t Context::EmitBinaryObject(char* buffer, size_t maxoutput)
{
	LLVMLinkInMCJIT();
	
	BasicBlock* bb = BasicBlock::Create(getGlobalContext(), "InitBlock", InitFunction);
	LLVMBuilder.SetInsertPoint(bb);
	// TODO - init globals here
	LLVMBuilder.CreateCall(EntryPointFunction);
	LLVMBuilder.CreateRet(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0));

	// HACK!
	Module* wat = LLVMModule.get();

	std::string errstr;

	TargetOptions opts;
	opts.LessPreciseFPMADOption = true;
	opts.UnsafeFPMath = true;
	opts.AllowFPOpFusion = FPOpFusion::Fast;
	opts.DisableTailCalls = false;
	opts.EnableFastISel = false;
	//opts.EnableSegmentedStacks = false;
	opts.GuaranteedTailCallOpt = true;
	opts.NoFramePointerElim = true;

	uint64_t emissionaddr = 0;
	size_t s = 0;
	std::unique_ptr<TrivialMemoryManager> blobmgr = std::make_unique<TrivialMemoryManager>(ThunkCallback, StringCallback, &emissionaddr, &s);

	EngineBuilder eb(std::move(LLVMModule));
	eb.setErrorStr(&errstr);
	eb.setTargetOptions(opts);
	//eb.setUseMCJIT(true);
	//eb.setRelocationModel(Reloc::Static);
	eb.setMCJITMemoryManager(std::move(blobmgr));

	SmallVector<std::string, 2> emptyvec;
	TargetMachine* machine = eb.selectTarget(Triple("x86_64-pc-windows-elf"), "", "", emptyvec);
	
	ExecutionEngine* ee = eb.create(machine);
	if(!ee)
	{
		return 0;
	}

	/*
	class : public JITEventListener
	{
		void NotifyObjectEmitted(const object::ObjectFile& img, const RuntimeDyld::LoadedObjectInfo& info) override
		{
		}
	} listener;

	ee->RegisterJITEventListener(&listener);*/

	ee->DisableLazyCompilation(true);
	ee->generateCodeForModule(wat);
	ee->mapSectionAddress((void*)(emissionaddr), 0x0404000);
	ee->finalizeObject();

	memcpy(buffer, (void*)(emissionaddr), s);
	return s;
}



llvm::BasicBlock* Context::CodeCreateBasicBlock(llvm::Function* parent)
{
	BasicBlock* bb = BasicBlock::Create(getGlobalContext(), "", parent);
	LLVMBuilder.SetInsertPoint(bb);
	return bb;
}

llvm::CallInst* Context::CodeCreateCall(llvm::Function* target)
{
	llvm::CallInst* inst = LLVMBuilder.CreateCall(target, PendingValues);

	llvm::FunctionType* fty = target->getFunctionType();
	for(size_t i = 0; i < fty->getNumParams(); ++i)
		PendingValues.pop_back();

	return inst;
}

llvm::CallInst* Context::CodeCreateCallThunk(llvm::GlobalVariable* target)
{
	llvm::Value* loadedTarget = LLVMBuilder.CreateLoad(target);
	llvm::CallInst* inst = LLVMBuilder.CreateCall(loadedTarget, PendingValues);

	llvm::FunctionType* fty = llvm::cast<llvm::FunctionType>(loadedTarget->getType()->getContainedType(0));
	for(size_t i = 0; i < fty->getNumParams(); ++i)
		PendingValues.pop_back();

	return inst;
}

void Context::CodeCreateRetVoid()
{
	LLVMBuilder.CreateRetVoid();
}


void Context::CodePushBoolean(bool value)
{
	llvm::Value* val = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), value);
	PendingValues.push_back(val);
}

void Context::CodePushString(unsigned handle)
{
	llvm::GlobalVariable* val = CachedStrings[handle];
	if(!val)
	{
		std::ostringstream name;
		name << "@epoch_static_string:" << handle;
		val = new GlobalVariable(*LLVMModule, Type::getInt8Ty(getGlobalContext()), true, GlobalValue::ExternalWeakLinkage, NULL, name.str(), NULL, GlobalVariable::NotThreadLocal, 0, true);

		CachedStrings[handle] = val;
	}

	PendingValues.push_back(val);
}
