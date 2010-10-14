//
// The Epoch Language Project
// Shared Library Code
//
// Wrapper class for containing a lexical scope and its contents
//

#include "pch.h"

#include "Metadata/ActiveScope.h"
#include "Metadata/ScopeDescription.h"

#include "Virtual Machine/VirtualMachine.h"
#include "Virtual Machine/TypeInfo.h"

#include "Utility/Memory/Stack.h"

#include "Utility/Types/RealTypes.h"


//
// Attach the parameters defined in the current scope to a given stack space
//
// Each variable within the scope which has its origin specified as an entity parameter is
// bound to a storage location within the given stack space. The stack space is assumed to
// contain all of the parameters to the entity in order as per the calling convention.
//
void ActiveScope::BindParametersToStack(const VM::ExecutionContext& context)
{
	char* stackpointer = reinterpret_cast<char*>(context.State.Stack.GetCurrentTopOfStack());

	for(ScopeDescription::VariableVector::const_reverse_iterator iter = OriginalScope.Variables.rbegin(); iter != OriginalScope.Variables.rend(); ++iter)
	{
		if(iter->Origin == VARIABLE_ORIGIN_PARAMETER)
		{
			VariableStorageLocations[context.OwnerVM.GetPooledStringHandle(iter->Identifier)] = stackpointer;
			stackpointer += VM::GetStorageSize(iter->Type);
		}
	}
}

//
// Allocate stack space for all local variables (including return value holders) on the given stack
//
void ActiveScope::PushLocalsOntoStack(VM::ExecutionContext& context)
{
	for(ScopeDescription::VariableVector::const_iterator iter = OriginalScope.Variables.begin(); iter != OriginalScope.Variables.end(); ++iter)
	{
		if(iter->Origin == VARIABLE_ORIGIN_LOCAL || iter->Origin == VARIABLE_ORIGIN_RETURN)
		{
			size_t size = VM::GetStorageSize(iter->Type);
			context.State.Stack.Push(size);

			VariableStorageLocations[context.OwnerVM.GetPooledStringHandle(iter->Identifier)] = context.State.Stack.GetCurrentTopOfStack();
		}
	}
}

//
// Pop a stack so as to reset it after holding parameters and local variables from the current scope
//
void ActiveScope::PopScopeOffStack(VM::ExecutionContext& context)
{
	size_t usedspace = 0;

	for(ScopeDescription::VariableVector::const_iterator iter = OriginalScope.Variables.begin(); iter != OriginalScope.Variables.end(); ++iter)
		usedspace += VM::GetStorageSize(iter->Type);

	context.State.Stack.Pop(usedspace);
}

//
// Write the topmost stack entry into the given variable's storage location
//
void ActiveScope::WriteFromStack(StringHandle variableid, StackSpace& stack)
{
	switch(OriginalScope.GetVariableTypeByID(variableid))
	{
	case VM::EpochType_Integer:
		Write(variableid, stack.PopValue<Integer32>());
		break;

	case VM::EpochType_String:
		Write(variableid, stack.PopValue<StringHandle>());
		break;

	case VM::EpochType_Boolean:
		Write(variableid, stack.PopValue<bool>());
		break;

	case VM::EpochType_Real:
		Write(variableid, stack.PopValue<Real32>());
		break;

	case VM::EpochType_Buffer:
		Write(variableid, stack.PopValue<BufferHandle>());
		break;

	default:
		throw NotImplementedException("Unsupported data type in ActiveScope::WriteFromStack");
	}
}

//
// Push a variable's value onto the top of the given stack
//
void ActiveScope::PushOntoStack(StringHandle variableid, StackSpace& stack) const
{
	switch(OriginalScope.GetVariableTypeByID(variableid))
	{
	case VM::EpochType_Integer:
		{
			Integer32* value = reinterpret_cast<Integer32*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_String:
		{
			StringHandle* value = reinterpret_cast<StringHandle*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_Boolean:
		{
			bool* value = reinterpret_cast<bool*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_Real:
		{
			Real32* value = reinterpret_cast<Real32*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_Buffer:
		{
			BufferHandle* value = reinterpret_cast<BufferHandle*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_Function:
		{
			StringHandle* value = reinterpret_cast<StringHandle*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;

	case VM::EpochType_Error:
	case VM::EpochType_Expression:
	case VM::EpochType_Void:
	case VM::EpochType_CustomBase:
		throw NotImplementedException("Unsupported data type in ActiveScope::PushOntoStack");

	default:
		{
			StructureHandle* value = reinterpret_cast<StructureHandle*>(GetVariableStorageLocation(variableid));
			stack.PushValue(*value);
		}
		break;
	}
}


//
// Retrieve the storage location in memory of the given variable
//
// This may reside on the stack or the freestore or even be a reference to another
// variable's storage position, depending on the context. In general it is not safe
// to assume that a variable resides in any one particular location.
//
void* ActiveScope::GetVariableStorageLocation(StringHandle variableid) const
{
	std::map<StringHandle, void*>::const_iterator iter = VariableStorageLocations.find(variableid);
	if(iter == VariableStorageLocations.end())
	{
		if(ParentScope)
			return ParentScope->GetVariableStorageLocation(variableid);

		throw InvalidIdentifierException("Variable ID has not been mapped to a storage location in this scope");
	}

	return iter->second;
}

//
// Copy a variable's value into the provided virtual machine register
//
void ActiveScope::CopyToRegister(StringHandle variableid, Register& targetregister) const
{
	switch(OriginalScope.GetVariableTypeByID(variableid))
	{
	case VM::EpochType_Integer:
		{
			Integer32* value = reinterpret_cast<Integer32*>(GetVariableStorageLocation(variableid));
			targetregister.Set(*value);
		}
		break;

	case VM::EpochType_String:
		{
			StringHandle* value = reinterpret_cast<StringHandle*>(GetVariableStorageLocation(variableid));
			targetregister.SetString(*value);
		}
		break;

	case VM::EpochType_Boolean:
		{
			bool* value = reinterpret_cast<bool*>(GetVariableStorageLocation(variableid));
			targetregister.Set(*value);
		}
		break;

	case VM::EpochType_Real:
		{
			Real32* value = reinterpret_cast<Real32*>(GetVariableStorageLocation(variableid));
			targetregister.Set(*value);
		}
		break;

	case VM::EpochType_Buffer:
		{
			BufferHandle* value = reinterpret_cast<BufferHandle*>(GetVariableStorageLocation(variableid));
			targetregister.SetBuffer(*value);
		}
		break;

	default:
		throw NotImplementedException("Unsupported data type in ActiveScope::CopyToRegister");
	}
}

//
// Determine if the current scope contains a variable bound to an entity return value
//
bool ActiveScope::HasReturnVariable() const
{
	for(ScopeDescription::VariableVector::const_iterator iter = OriginalScope.Variables.begin(); iter != OriginalScope.Variables.end(); ++iter)
	{
		if(iter->Origin == VARIABLE_ORIGIN_RETURN)
			return true;
	}

	return false;
}


