//
// The Epoch Language Project
// Epoch Standard Library
//
// Conditional flow control entities
//

#include "pch.h"

#include "Library Functionality/Flow Control/Conditionals.h"

#include "Virtual Machine/VirtualMachine.h"

#include "Utility/StringPool.h"
#include "Utility/NoDupeMap.h"


using namespace FlowControl;


namespace
{
	//
	// Meta-control function for if/elseif conditional entities
	//
	// Entities are permitted to have attached meta-control semantics, which invokes
	// a meta-control function such as this one when the entity is encountered in the
	// executing code. How the entity's attached code block (if any) is handled will
	// depend on the return code from the meta-control logic. In this particular case,
	// we pop a boolean off the stack (which is placed there by the expression passed
	// as a parameter to the entity invocation) and conditionally execute the entity
	// code block if the boolean evaluates to true. Otherwise, we indicate that the
	// flow of execution should proceed to the next link in the entity chain, if any.
	// This link may be another conditional (an elseif) or an else clause, which is
	// handled separately by ConditionalElseMetaControl.
	//
	EntityReturnCode ConditionalMetaControl(VM::ExecutionContext& context)
	{
		bool conditionalvalue = context.State.Stack.PopValue<bool>();
		if(conditionalvalue)
			return ENTITYRET_EXECUTE_CURRENT_LINK_IN_CHAIN;

		return ENTITYRET_PASS_TO_NEXT_LINK_IN_CHAIN;
	}

	//
	// Meta-control function for else clauses on conditional entities
	//
	// If we reach the meta-control for a chained else clause, it will only be
	// because the meta-control for the previous links in the chain have all passed
	// on control flow, i.e. their conditions have evaluated to false. Therefore,
	// we need perform no additional checking or logic here, and can unconditionally
	// execute the code attached to the else block.
	//
	EntityReturnCode ConditionalElseMetaControl(VM::ExecutionContext&)
	{
		return ENTITYRET_EXECUTE_CURRENT_LINK_IN_CHAIN;
	}

	void IfJIT(JIT::JITContext& context, bool entry)
	{
		llvm::IRBuilder<>* builder = reinterpret_cast<llvm::IRBuilder<>*>(context.Builder);
		if(entry)
		{
			context.EntityBodies.push(llvm::BasicBlock::Create(*reinterpret_cast<llvm::LLVMContext*>(context.Context), "", context.InnerFunction));
			builder->CreateCondBr(context.ValuesOnStack.top(), context.EntityBodies.top(), context.EntityChains.top());
			context.ValuesOnStack.pop();
			builder->SetInsertPoint(context.EntityBodies.top());
		}
		else
		{
			context.EntityBodies.pop();
			builder->CreateBr(context.EntityChainExits.top());
		}
	}

	void ElseIfJIT(JIT::JITContext& context, bool)
	{
		throw NotImplementedException("Uh oh");
		/*
		llvm::IRBuilder<>* builder = reinterpret_cast<llvm::IRBuilder<>*>(context.Builder);
		builder->CreateCondBr(context.ValuesOnStack.top(), context.EntityBodies.top(), context.EntityExits.top());
		context.ValuesOnStack.pop();
		builder->SetInsertPoint(context.EntityBodies.top());
		*/
		(void)(context);
	}

	void ElseJIT(JIT::JITContext& context, bool entry)
	{
		llvm::IRBuilder<>* builder = reinterpret_cast<llvm::IRBuilder<>*>(context.Builder);
		if(entry)
		{
			builder->SetInsertPoint(context.EntityChains.top());
		}
		else
		{
			builder->CreateBr(context.EntityChainExits.top());
		}
	}
}


static std::map<StringHandle, Bytecode::EntityTag> EntityMap;


//
// Register conditional flow control entities with the compiler/VM
//
void FlowControl::RegisterConditionalEntities(EntityTable& entities, EntityTable& chainedentities, StringPoolManager& stringpool, Bytecode::EntityTag& tagindex)
{
	{
		EntityDescription entity;
		entity.StringName = stringpool.Pool(L"if");
		entity.MetaControl = ConditionalMetaControl;
		entity.Parameters.push_back(CompileTimeParameter(L"condition", Metadata::EpochType_Boolean));
		EntityMap[entity.StringName] = ++tagindex;
		AddToMapNoDupe(entities, std::make_pair(tagindex, entity));
	}
	{
		EntityDescription entity;
		entity.StringName = stringpool.Pool(L"elseif");
		entity.MetaControl = ConditionalMetaControl;
		entity.Parameters.push_back(CompileTimeParameter(L"condition", Metadata::EpochType_Boolean));
		EntityMap[entity.StringName] = ++tagindex;
		AddToMapNoDupe(chainedentities, std::make_pair(tagindex, entity));
	}
	{
		EntityDescription entity;
		entity.StringName = stringpool.Pool(L"else");
		entity.MetaControl = ConditionalElseMetaControl;
		EntityMap[entity.StringName] = ++tagindex;
		AddToMapNoDupe(chainedentities, std::make_pair(tagindex, entity));
	}
}

void FlowControl::RegisterConditionalJITTable(JIT::JITTable& table, StringPoolManager& stringpool)
{
	{
		Bytecode::EntityTag tag = EntityMap[stringpool.Pool(L"if")];
		AddToMapNoDupe(table.EntityHelpers, std::make_pair(tag, &IfJIT));
	}
	{
		Bytecode::EntityTag tag = EntityMap[stringpool.Pool(L"elseif")];
		AddToMapNoDupe(table.EntityHelpers, std::make_pair(tag, &ElseIfJIT));
	}
	{
		Bytecode::EntityTag tag = EntityMap[stringpool.Pool(L"else")];
		AddToMapNoDupe(table.EntityHelpers, std::make_pair(tag, &ElseJIT));
	}
}

