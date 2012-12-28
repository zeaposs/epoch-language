#pragma once


#include <stack>
#include <map>

#include "Bytecode/EntityTags.h"

#include "Utility/Types/IDTypes.h"


namespace llvm
{
	class Value;
	class BasicBlock;
}


namespace JIT
{

	struct JITContext
	{
		std::stack<llvm::Value*> ValuesOnStack;

		std::stack<Bytecode::EntityTag> EntityTypes;

		std::map<size_t, llvm::Value*> VariableMap;
		std::map<StringHandle, size_t> NameToIndexMap;
		std::map<size_t, llvm::Value*> SumTypeTagMap;

		llvm::Module* MyModule;

		std::stack<llvm::BasicBlock*> EntityChecks;
		std::stack<llvm::BasicBlock*> EntityBodies;
		std::stack<llvm::BasicBlock*> EntityChains;
		std::stack<llvm::BasicBlock*> EntityChainExits;

		llvm::Value* VMContextPtr;

		llvm::Function* InnerFunction;
		llvm::Function* VMGetBuffer;
		llvm::Function* SqrtIntrinsic;

		std::map<llvm::Value*, llvm::Value*> BufferLookupCache;

		void* Context;
		void* Builder;
	};


	typedef void (*JITHelper)(JITContext& context, bool entry);

	struct JITTable
	{
		std::map<StringHandle, JITHelper> InvokeHelpers;
		std::map<Bytecode::EntityTag, JITHelper> EntityHelpers;
	};

}

