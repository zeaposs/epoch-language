//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// IR class containing a structure definition
//

#include "pch.h"

#include "Compiler/Intermediate Representations/Semantic Validation/Structure.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Expression.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Assignment.h"
#include "Compiler/Intermediate Representations/Semantic Validation/CodeBlock.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Function.h"
#include "Compiler/Intermediate Representations/Semantic Validation/Program.h"

#include "Compiler/Session.h"


using namespace IRSemantics;


//
// Destruct and clean up a structure definition wrapper
//
Structure::~Structure()
{
	for(std::vector<std::pair<StringHandle, StructureMember*> >::iterator iter = Members.begin(); iter != Members.end(); ++iter)
		delete iter->second;
}

//
// Add a member to a structure definition
//
void Structure::AddMember(StringHandle name, StructureMember* member)
{
	for(std::vector<std::pair<StringHandle, StructureMember*> >::iterator iter = Members.begin(); iter != Members.end(); ++iter)
	{
		if(name == iter->first)
		{
			delete member;
			throw std::runtime_error("Duplicate structure member");		// TODO - this should not be an exception
		}
	}

	Members.push_back(std::make_pair(name, member));
}

//
// Validate a structure definition
//
bool Structure::Validate(const Program& program) const
{
	bool valid = true;

	for(std::vector<std::pair<StringHandle, StructureMember*> >::const_iterator iter = Members.begin(); iter != Members.end(); ++iter)
	{
		if(!iter->second->Validate(program))
			valid = false;
	}

	return valid;
}


bool Structure::CompileTimeCodeExecution(StringHandle myname, Program& program)
{
	FunctionSignature signature;
	signature.AddParameter(L"id", VM::EpochType_Identifier, true);
	for(std::vector<std::pair<StringHandle, StructureMember*> >::const_iterator iter = Members.begin(); iter != Members.end(); ++iter)
		signature.AddParameter(program.GetString(iter->first), iter->second->GetEpochType(program), false);

	program.Session.FunctionSignatures.insert(std::make_pair(myname, signature));


	for(std::vector<std::pair<StringHandle, StructureMember*> >::const_iterator iter = Members.begin(); iter != Members.end(); ++iter)
	{
		StringHandle funcname = program.FindStructureMemberAccessOverload(myname, iter->first);

		VM::EpochTypeID type = iter->second->GetEpochType(program);

		std::auto_ptr<Function> func(new Function);
		std::auto_ptr<Expression> retexpr(new Expression);
		retexpr->AddAtom(new ExpressionAtomCopyFromStructure(type, iter->first));
		func->SetReturnExpression(retexpr.release());

		std::auto_ptr<ScopeDescription> scope(new ScopeDescription(program.GetGlobalScope()));
		scope->AddVariable(L"identifier", program.AddString(L"identifier"), program.LookupType(myname), true, VARIABLE_ORIGIN_PARAMETER);
		scope->AddVariable(L"ret", program.AddString(L"ret"), type, false, VARIABLE_ORIGIN_RETURN);
		std::auto_ptr<CodeBlock> codeblock(new CodeBlock(scope.release()));
		program.AllocateLexicalScopeName(codeblock.get());
		func->SetCode(codeblock.release());
		func->SetName(funcname);
		func->AddParameter(program.FindString(L"identifier"), new FunctionParamNamedTyped(type, true));
		func->SuppressReturnRegister();

		program.AddFunction(funcname, func.release());
	}

	return true;
}


//
// Validate a variable structure member
//
bool StructureMemberVariable::Validate(const Program& program) const
{
	return (program.LookupType(MyType) != VM::EpochType_Error);
}


VM::EpochTypeID StructureMemberVariable::GetEpochType(const Program& program) const
{
	return program.LookupType(MyType);
}



//
// Validate a function reference structure member
//
bool StructureMemberFunctionReference::Validate(const Program& program) const
{
	bool valid = true;

	if(program.LookupType(ReturnType) == VM::EpochType_Error)
		valid = false;

	for(std::vector<StringHandle>::const_iterator iter = ParamTypes.begin(); iter != ParamTypes.end(); ++iter)
	{
		if(program.LookupType(*iter) == VM::EpochType_Error)
			valid = false;
	}
	
	return valid;
}

