//
// The Epoch Language Project
// EPOCHVM Virtual Machine
//
// Declaration of the VM wrapper class
//

#pragma once

// Dependencies
#include "Virtual Machine/ExportDef.h"

#include "Metadata/ScopeDescription.h"
#include "Metadata/Register.h"
#include "Metadata/ActiveStructure.h"
#include "Metadata/StructureDefinition.h"
#include "Metadata/Variant.h"

#include "Libraries/Library.h"
#include "Libraries/LibraryJIT.h"

#include "Utility/Memory/Stack.h"

#include "Utility/Types/EpochTypeIDs.h"
#include "Utility/Types/IntegerTypes.h"
#include "Utility/Types/IDTypes.h"

#include "Utility/StringPool.h"

#include "Utility/Threading/Synchronization.h"

#include "Bytecode/Instructions.h"

#include "JIT/JIT.h"

#include <map>
#include <set>
#include <stack>
#include <string>

#include <boost/unordered_map.hpp>


// Forward declarations
class ActiveScope;
class StructureDefinition;

struct fasthash : std::unary_function<StringHandle, size_t>
{
	size_t operator() (StringHandle a) const
	{
		a = (a ^ 61) ^ (a >> 16);
		a = a + (a << 3);
		a = a ^ (a >> 4);
		a = a * 0x27d4eb2d;
		a = a ^ (a >> 15);
		return a;
	}
};


namespace VM
{

	// Handy type shortcuts
	typedef boost::unordered_map<StringHandle, ScopeDescription, fasthash> ScopeMap;


	//
	// Helper structure for storing information about a completed execution run
	//
	struct ExecutionResult
	{
		enum ExecResultType
		{
			EXEC_RESULT_OK,			// Code terminated normally
			EXEC_RESULT_HALT,		// VM encountered a HALT instruction
			EXEC_RESULT_RETURN,		// Return to calling context
		};

		ExecResultType ResultType;
	};


	//
	// Wrapper for an instance of a single virtual "machine"
	//
	class VirtualMachine
	{
	// Special access for enabling the visual debug thread
	public:
		static void EnableVisualDebugger();

	// Construction
	public:
		VirtualMachine();

	// Initialization
	public:
		void InitStandardLibraries(unsigned* testharness);

	// Code execution
	public:
		ExecutionResult ExecuteByteCode(Bytecode::Instruction* buffer, size_t size);

	// Handle-managed resources
	public:
		EPOCHVM StringHandle PoolString(const std::wstring& stringdata);
		void PoolString(StringHandle handle, const std::wstring& stringdata);
		EPOCHVM const std::wstring& GetPooledString(StringHandle handle) const;
		StringHandle GetPooledStringHandle(const std::wstring& value);

		EPOCHVM void* GetBuffer(BufferHandle handle);
		EPOCHVM size_t GetBufferSize(BufferHandle handle) const;
		EPOCHVM BufferHandle AllocateBuffer(size_t size);
		BufferHandle CloneBuffer(BufferHandle handle);

		StructureHandle AllocateStructure(const StructureDefinition& description);
		StructureHandle DeepCopy(StructureHandle handle);
		ActiveStructure& FindStructureMetadata(StructureHandle handle);

		EPOCHVM const StructureDefinition& GetStructureDefinition(Metadata::EpochTypeID vartype) const;

	// Functions
	public:
		void AddFunction(StringHandle name, EpochFunctionPtr funcptr);
		void AddFunction(StringHandle name, size_t instructionoffset);

		void InvokeFunction(StringHandle functionname, ExecutionContext& context);

		size_t GetFunctionInstructionOffset(StringHandle functionname) const;
		size_t GetFunctionInstructionOffsetNoThrow(StringHandle functionname) const;

	// Entity management
	public:
		void MapEntityBeginEndOffsets(size_t beginoffset, size_t endoffset);
		void MapChainBeginEndOffsets(size_t beginoffset, size_t endoffset);
		size_t GetEntityEndOffset(size_t beginoffset) const;
		size_t GetChainEndOffset(size_t beginoffset) const;

		EntityMetaControl GetEntityMetaControl(Bytecode::EntityTag tag) const;

	// Lexical scopes
	public:
		void AddLexicalScope(StringHandle name);

		const ScopeDescription& GetScopeDescription(StringHandle name) const;
		ScopeDescription& GetScopeDescription(StringHandle name);

	// Garbage collection
	public:
		void GarbageCollectBuffers(const boost::unordered_set<BufferHandle>& livehandles);
		void GarbageCollectStructures(const boost::unordered_set<StructureHandle>& livehandles);

	// Use sparingly! Only intended for access by the garbage collector
	public:
		StringPoolManager& PrivateGetRawStringPool()
		{ return PrivateStringPool; }

		const boost::unordered_map<StructureHandle, ActiveStructure*>& PrivateGetStructurePool()
		{ return ActiveStructures; }


	// Debug assistance
	public:
		std::wstring DebugSnapshot() const;


	// Public tracking
	public:
		std::map<Metadata::EpochTypeID, StructureDefinition> StructureDefinitions;
		JIT::JITTable JITHelpers;
		boost::unordered_map<StringHandle, EpochToJITWrapperFunc, fasthash> JITExecs;

		boost::unordered_map<Metadata::EpochTypeID, VariantDefinition, fasthash> VariantDefinitions;
		std::map<StringHandle, size_t> TypeMatcherParamCount;
		std::map<StringHandle, Metadata::EpochTypeID> TypeMatcherRetType;

	// Handy type shortcuts
	private:
		typedef boost::unordered_map<StringHandle, size_t, fasthash> OffsetMap;
		typedef boost::unordered_map<size_t, size_t, fasthash> BeginEndOffsetMap;

	// Internal state tracking
	private:
		StringPoolManager PrivateStringPool;
		FunctionInvocationTable GlobalFunctions;
		OffsetMap GlobalFunctionOffsets;
		ScopeMap LexicalScopeDescriptions;
		EntityTable Entities;
		BeginEndOffsetMap EntityOffsets;
		BeginEndOffsetMap ChainOffsets;

		HandleAllocator<BufferHandle> BufferHandleAlloc;
		boost::unordered_map<BufferHandle, std::vector<Byte>, fasthash> Buffers;

		boost::unordered_map<StructureHandle, ActiveStructure*> ActiveStructures;

		Threads::CriticalSection BufferCritSec;
		Threads::CriticalSection StructureCritSec;

	// Static state tracking
	private:
		static bool VisualDebuggerEnabled;
	};


	//
	// Helper class for isolating a running chunk of code on a VM
	//
	struct MachineState
	{
		ExecutionResult Result;
		StackSpace Stack;
		Register ReturnValueRegister;
	};

	class ExecutionContext
	{
	// Construction
	public:
		ExecutionContext(VirtualMachine& owner, Bytecode::Instruction* codebuffer, size_t codesize);

	// Non-copyable
	private:
		ExecutionContext(const ExecutionContext& rhs);
		ExecutionContext& operator = (const ExecutionContext& rhs);

	// Execution interface
	public:
		void Execute(const ScopeDescription* scope, bool returnonfunctionexit);
		void Execute(size_t offset, const ScopeDescription& scope, bool returnonfunctionexit);

	// State accessors
	public:
		ExecutionResult GetExecutionResult() const
		{ return State.Result; }

	// Internal helpers for initializing the context
	private:
		void Load();

	// Internal helpers for working with the bytecode stream
	private:
		template <typename T>
		__forceinline T Fetch()
		{
			const T* data = reinterpret_cast<const T*>(&CodeBuffer[InstructionOffset]);
			InstructionOffset += sizeof(T);
			return static_cast<T>(*data);
		}

		template <>
		std::wstring Fetch()
		{
			const wchar_t* data = reinterpret_cast<const wchar_t*>(&CodeBuffer[InstructionOffset]);
			std::wstring strvalue(data);
			InstructionOffset += (strvalue.length() + 1) * sizeof(wchar_t);
			return strvalue;
		}

	// Available state (for functions to operate on)
	public:
		MachineState State;
		ActiveScope* Variables;
		VirtualMachine& OwnerVM;
		std::deque<StringHandle> InvokedFunctionStack;

	// Internal helpers for garbage collection
	public:
		void CollectGarbage();

		void CollectGarbage_Buffers();
		void CollectGarbage_Strings();
		void CollectGarbage_Structures();

		EPOCHVM void TickBufferGarbageCollector();
		EPOCHVM void TickStringGarbageCollector();
		EPOCHVM void TickStructureGarbageCollector();

		template <typename HandleType, typename ValidatorT>
		void MarkAndSweep(ValidatorT validator, boost::unordered_set<HandleType>& livehandles);

	private:
		void WriteStructureMember(ActiveStructure& structure, size_t memberindex, Metadata::EpochTypeID membertype);

	// Internal state
	private:
	    Bytecode::Instruction* CodeBuffer;
		size_t CodeBufferSize;
		size_t InstructionOffset;

		std::vector<size_t> InstructionOffsetStack;

		size_t GarbageTick_Buffers;
		size_t GarbageTick_Strings;
		size_t GarbageTick_Structures;

		boost::unordered_set<StringHandle> StaticallyReferencedStrings;
	};

}

