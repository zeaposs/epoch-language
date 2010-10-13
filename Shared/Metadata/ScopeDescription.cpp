//
// The Epoch Language Project
// Shared Library Code
//
// Wrapper class for describing the contents of lexical scopes
//

#include "pch.h"

#include "Metadata/ScopeDescription.h"

#include "Utility/Strings.h"


//
// Add a variable to a lexical scope
//
void ScopeDescription::AddVariable(const std::wstring& identifier, StringHandle identifierhandle, VM::EpochTypeID type, VariableOrigin origin)
{
	if(HasVariable(identifier))
		throw InvalidIdentifierException("Duplicate/shadowed identifiers are not permitted - the identifier \"" + narrow(identifier) + "\" is already in use in this scope or some containing scope.");

	Variables.push_back(VariableEntry(identifier, identifierhandle, type, origin));
}

//
// Determine if the scope contains a variable with the given identifier
//
bool ScopeDescription::HasVariable(const std::wstring& identifier) const
{
	for(VariableVector::const_iterator iter = Variables.begin(); iter != Variables.end(); ++iter)
	{
		if(iter->Identifier == identifier)
			return true;
	}

	if(ParentScope)
		return ParentScope->HasVariable(identifier);

	return false;
}

//
// Retrieve the identifier of the variable at the given index in the scope
//
const std::wstring& ScopeDescription::GetVariableName(size_t index) const
{
	return Variables[index].Identifier;
}

//
// Retrieve the string handle of the identifier of the variable at the given index in the scope
//
StringHandle ScopeDescription::GetVariableNameHandle(size_t index) const
{
	return Variables[index].IdentifierHandle;
}

//
// Retrieve the type of a variable given its identifier handle
//
VM::EpochTypeID ScopeDescription::GetVariableTypeByID(StringHandle variableid) const
{
	for(VariableVector::const_iterator iter = Variables.begin(); iter != Variables.end(); ++iter)
	{
		if(iter->IdentifierHandle == variableid)
			return iter->Type;
	}

	if(ParentScope)
		return ParentScope->GetVariableTypeByID(variableid);

	throw InvalidIdentifierException("Could not retrieve the variable's type - identifier is not valid in this scope");
}

//
// Retrieve the type of the variable at the given index in the scope
//
VM::EpochTypeID ScopeDescription::GetVariableTypeByIndex(size_t index) const
{
	return Variables[index].Type;
}

//
// Retrieve the origin of the variable at the given index in the scope
//
VariableOrigin ScopeDescription::GetVariableOrigin(size_t index) const
{
	return Variables[index].Origin;
}

