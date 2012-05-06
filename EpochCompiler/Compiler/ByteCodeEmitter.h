//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// Helper class for generating bytecode sequences from semantic actions
//

#pragma once


// Dependencies
#include "Metadata/ScopeDescription.h"

#include "Utility/Types/EpochTypeIDs.h"
#include "Utility/Types/IntegerTypes.h"
#include "Utility/Types/RealTypes.h"
#include "Utility/Types/IDTypes.h"

#include "Bytecode/Instructions.h"
#include "Bytecode/EntityTags.h"


// Forward declarations
class FunctionSignature;


// Handy type shortcuts
typedef std::vector<Byte> ByteBuffer;


//
// Wrapper class for generating bytecode sequences
//
// This class is used directly by the parser to create a bytecode stream for a
// program as it is parsed from source code. The resulting stream is suitable for
// serialization either in binary or assembly format, or direct execution.
//
class ByteCodeEmitter
{
// Construction
public:
	explicit ByteCodeEmitter(ByteBuffer& buffer)
		: Buffer(buffer)
	{ }

// Non-copyable
private:
	ByteCodeEmitter(const ByteCodeEmitter& rhs);
	ByteCodeEmitter& operator = (const ByteCodeEmitter& rhs);

// Functions
public:
	void EnterFunction(StringHandle functionname);
	void ExitFunction();

	void SetReturnRegister(StringHandle variablename);

// Stack operations
public:
	void PushIntegerLiteral(Integer32 value);
	void PushInteger16Literal(Integer32 value);
	void PushStringLiteral(StringHandle handle);
	void PushBooleanLiteral(bool value);
	void PushRealLiteral(Real32 value);
	void PushVariableValue(StringHandle variablename, VM::EpochTypeID type);
	void PushVariableValueNoCopy(StringHandle variablename);
	void PushBufferHandle(BufferHandle handle);
	void PushTypeAnnotation(VM::EpochTypeID type);

	void BindReference(StringHandle variablename);
	void BindReferenceIndirect();
	void BindStructureReference(StringHandle membername);
	void BindStructureReferenceByHandle(StringHandle membername);

	void PopStack(VM::EpochTypeID type);

// Flow control
public:
	void Invoke(StringHandle functionname);
	void InvokeIndirect(StringHandle varname);
	void Halt();

// Entities
public:
	void EnterEntity(Bytecode::EntityTag tag, StringHandle name);
	void ExitEntity();

	void BeginChain();
	void EndChain();

	void InvokeMetacontrol(Bytecode::EntityTag tag);

// Lexical scope metadata
public:
	void DefineLexicalScope(StringHandle name, StringHandle parent, size_t variablecount);
	void LexicalScopeEntry(StringHandle varname, VM::EpochTypeID vartype, bool isreference, VariableOrigin origin);

// Pattern matching
public:
	void EnterPatternResolver(StringHandle functionname);
	void ExitPatternResolver();

	void ResolvePattern(StringHandle dispatchfunction, const FunctionSignature& signature);

// Type resolution
public:
	void EnterTypeResolver(StringHandle resolvername);
	void ExitTypeResolver();

	void ResolveTypes(StringHandle dispatchfunction, const FunctionSignature& signature);

// Structures
public:
	void AllocateStructure(VM::EpochTypeID descriptiontype);
	void DefineStructure(VM::EpochTypeID type, size_t nummembers);
	void StructureMember(StringHandle identifier, VM::EpochTypeID type);
	void CopyFromStructure(StringHandle structurevariable, StringHandle membervariable);
	void AssignStructure(StringHandle structurevariable, StringHandle membername);
	void CopyStructure();

// Buffers
public:
	void CopyBuffer();

// Utility instructions
public:
	void AssignVariable();
	void AssignVariableThroughIdentifier();
	void ReadReferenceOntoStack();

	void PoolString(StringHandle handle, const std::wstring& literalvalue);

	void TagData(StringHandle entityname, const std::wstring& tag, const std::vector<std::wstring>& tagdata);

	void DefineSumType(VM::EpochTypeID sumtypeid, const std::set<VM::EpochTypeID>& basetypes);

// Additional helpers for writing to the data stream
public:
	void EmitBuffer(const ByteBuffer& buffer);

// Internal helpers
private:
	void EmitInstruction(Bytecode::Instruction instruction);
	void EmitTerminatedString(const std::wstring& value);

	void EmitTypeAnnotation(VM::EpochTypeID type);
	void EmitEntityTag(Bytecode::EntityTag tag);

	void EmitRawValue(bool value);
	void EmitRawValue(Byte value);
	void EmitRawValue(Integer32 value);
	void EmitRawValue(Integer16 value);
	void EmitRawValue(HandleType value);
	void EmitRawValue(const std::wstring& value);
	void EmitRawValue(Real32 value);

	void PrependBytes(unsigned numbytes, const void* bytes);

	//
	// Prepend any number of bytes to the stream
	//
	// The original byte ordering is preserved; i.e. the bytes will not
	// be reversed by the prepend operation.
	//
	template <typename T>
	void PrependRawValue(const T& value)
	{
		PrependBytes(sizeof(T), &value);
	}

	void PrependRawValue(const std::wstring& value);

	void PrependTypeAnnotation(VM::EpochTypeID type);

	void PrependInstruction(Bytecode::Instruction instruction);

	void PrependEntityTag(Bytecode::EntityTag tag);


// Internal tracking
private:
	ByteBuffer& Buffer;
};



