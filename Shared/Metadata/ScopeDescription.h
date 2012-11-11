//
// The Epoch Language Project
// Shared Library Code
//
// Wrapper class for describing the contents of lexical scopes
//

#pragma once


// Dependencies
#include "Metadata/FunctionSignature.h"
#include "Metadata/CompileTimeParams.h"

#include "Utility/Types/EpochTypeIDs.h"
#include "Utility/Types/IDTypes.h"

#include <string>
#include <vector>
#include <map>


enum VariableOrigin
{
	VARIABLE_ORIGIN_LOCAL,
	VARIABLE_ORIGIN_PARAMETER,
	VARIABLE_ORIGIN_RETURN
};


class ScopeDescription
{
// Construction
public:
	ScopeDescription()
		: ParentScope(NULL)
	{ }

	explicit ScopeDescription(ScopeDescription* parentscope)
		: ParentScope(parentscope)
	{ }

// Configuration interface
public:
	void AddVariable(const std::wstring& identifier, StringHandle identifierhandle, StringHandle typenamehandle, VM::EpochTypeID type, bool isreference, VariableOrigin origin);
	void PrependVariable(const std::wstring& identifier, StringHandle identifierhandle, StringHandle typenamehandle, VM::EpochTypeID type, bool isreference, VariableOrigin origin);

// Inspection interface
public:
	bool HasVariable(const std::wstring& identifier) const;
	bool HasVariable(StringHandle identifier) const;

	const std::wstring& GetVariableName(size_t index) const;
	StringHandle GetVariableNameHandle(size_t index) const;
	VM::EpochTypeID GetVariableTypeByID(StringHandle variableid) const;
	VM::EpochTypeID GetVariableTypeByIndex(size_t index) const;
	VariableOrigin GetVariableOrigin(size_t index) const;
	bool IsReference(size_t index) const;
	bool IsReferenceByID(StringHandle variableid) const;

	size_t GetVariableCount() const
	{ return Variables.size(); }

	bool HasReturnVariable() const;

	void Fixup(const std::vector<std::pair<StringHandle, VM::EpochTypeID> >& templateparams, const CompileTimeParameterVector& templateargs, const CompileTimeParameterVector& templateargtypes);

// Public properties
public:
	ScopeDescription* ParentScope;

// Internal helper structures
private:
	struct VariableEntry
	{
		// Constructor for convenience
		VariableEntry(const std::wstring& identifier, StringHandle identifierhandle, StringHandle typenamehandle, VM::EpochTypeID type, bool isreference, VariableOrigin origin)
			: Identifier(identifier),
			  IdentifierHandle(identifierhandle),
			  Type(type),
			  TypeName(typenamehandle),
			  Origin(origin),
			  IsReference(isreference)
		{ }

		std::wstring Identifier;
		StringHandle IdentifierHandle;
		VM::EpochTypeID Type;
		StringHandle TypeName;
		VariableOrigin Origin;
		bool IsReference;
	};

// Internal tracking
private:
	typedef std::vector<VariableEntry> VariableVector;
	VariableVector Variables;

// Permit activated scopes to access internal data
public:
	friend class ActiveScope;
};


// Handy type shortcuts
typedef std::map<StringHandle, ScopeDescription> ScopeMap;
