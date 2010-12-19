//
// The Epoch Language Project
// Shared Library Code
//
// Interface for defining built-in library functions
//

#pragma once


// Dependencies
#include "Metadata/EntityDescription.h"
#include "Metadata/CompileTimeParams.h"
#include "Metadata/IdentifierTable.h"
#include "Utility/Types/IDTypes.h"

#include <map>


// Forward declarations
class ScopeDescription;
class SemanticActionInterface;

namespace VM
{
	class ExecutionContext;
}


// Handy type shortcuts
typedef void (*EpochFunctionPtr)(StringHandle namehandle, VM::ExecutionContext& context);
typedef std::map<StringHandle, EpochFunctionPtr> FunctionInvocationTable;

typedef void (*CompilerHelperPtr)(const std::wstring& functionname, SemanticActionInterface& semantics, ScopeDescription& scope, const CompileTimeParameterVector& compiletimeparams);
typedef std::map<std::wstring, CompilerHelperPtr> FunctionCompileHelperTable;

typedef std::multimap<int, StringHandle> PrecedenceTable;

typedef std::map<Bytecode::EntityTag, EntityDescription> EntityTable;

typedef std::set<StringHandle> StringHandleSet;

typedef std::map<StringHandle, StringHandleSet> OverloadMap;


//
// This structure encapsulates the data associated with function tags, allowing
// the tag parser helper to attach metadata to a function for storage in final
// compiled bytecode. It also allows runtime functions to be invoked magically
// when the function is called, which provides a handy mechanism for marshaling
// calls to external APIs.
//
struct TagHelperReturn
{
	std::wstring InvokeRuntimeFunction;
	std::wstring MetaTag;
	std::vector<std::wstring> MetaTagData;
};


typedef TagHelperReturn (*FunctionTagHelperPtr)(StringHandle functionname, const CompileTimeParameterVector& compiletimeparams, bool isprepass);
typedef std::map<std::wstring, FunctionTagHelperPtr> FunctionTagHelperTable;


//
// Convenience structure holding a set of information needed for compilation
//
// Note that in most cases this will consist purely of pointers bound to another
// object, such as a CompileSession, which stores all the data indirectly. This
// is mainly useful for providing a succinct interface for passing the various
// pointers into external library DLLs.
//
struct CompilerInfoTable
{
	FunctionCompileHelperTable* FunctionHelpers;
	StringSet* InfixOperators;
	StringSet* UnaryPrefixes;
	StringSet* PreOperators;
	StringSet* PostOperators;
	StringSet* OpAssignOperators;
	PrecedenceTable* Precedences;
	OverloadMap* Overloads;
	EntityTable* Entities;
	EntityTable* ChainedEntities;
	EntityTable* PostfixEntities;
	EntityTable* PostfixClosers;
	FunctionTagHelperTable* FunctionTagHelpers;
};