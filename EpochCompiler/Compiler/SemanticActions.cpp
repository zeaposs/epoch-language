//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// Wrapper for handling semantic actions invoked by the parser
//
// Semantic actions are processed in two phases, one "pre-pass" phase and one
// compilation phase. During the pre-pass phase, information on lexical scopes,
// their contents, and general identifier usage is gathered. This data is then
// used to construct the scope metadata tables and other metadata used by the
// compilation phase itself, which actually generates bytecode from the source,
// using the pre-pass metadata for validation, type checking, and so on.
//

#include "pch.h"

#include "Compiler/SemanticActions.h"
#include "Compiler/ByteCodeEmitter.h"
#include "Compiler/Exceptions.h"

#include "Metadata/FunctionSignature.h"

#include "Utility/Strings.h"

#include <boost/spirit/include/classic_exceptions.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>

#include <sstream>


// Handy type shortcuts
typedef boost::spirit::classic::position_iterator<const char*> PosIteratorT;
typedef boost::spirit::classic::parser_error<TypeMismatchException, PosIteratorT> SpiritCompatibleTypeMismatchException;


//-------------------------------------------------------------------------------
// Storage of parsed tokens
//-------------------------------------------------------------------------------

//
// Shift a string token onto the general string stack
//
// Tokens can represent anything from function calls to flow control keywords to
// variable accesses; the specific significance of a string token is derived from
// its context within the code. As a result the general string stack is used for
// a wide variety of purposes, and accessed throughout the semantic action code.
//
void CompilationSemantics::StoreString(const std::wstring& name)
{
	Strings.push(name);
	PushedItemTypes.push(ITEMTYPE_STRING);
}

//
// Shift an integer literal onto the integer literal stack
//
void CompilationSemantics::StoreIntegerLiteral(Integer32 value)
{
	IntegerLiterals.push(value);
	PushedItemTypes.push(ITEMTYPE_INTEGERLITERAL);
}

//
// Shift a string literal onto the string literal stack
//
void CompilationSemantics::StoreStringLiteral(const std::wstring& value)
{
	StringLiterals.push(Session.StringPool.Pool(value));
	PushedItemTypes.push(ITEMTYPE_STRINGLITERAL);
}

//
// Shift an entity type tag onto the entity type tag stack
//
// Entity type tags are used to help generate prolog and epilog code for each entity,
// as well as track boundaries between execution contexts for entities that trigger
// them (such as hardware acceleration, multithreading, etc.).
//
void CompilationSemantics::StoreEntityType(Bytecode::EntityTag typetag)
{
	switch(typetag)
	{
	//
	// The entity is a function. In this case, we need to track the
	// identifier of the function itself, and if applicable generate
	// the prolog code during the compilation pass.
	//
	case Bytecode::EntityTags::Function:
		CurrentEntities.push(Strings.top());
		if(!IsPrepass)
			EmitterStack.top()->EnterFunction(LexicalScopeStack.top());
		Strings.pop();
		PushedItemTypes.pop();
		break;

	//
	// Handle the error case where the type tag is not recognized. Non-fatal.
	//
	default:
		throw RecoverableException("Invalid entity type tag");
	}

	// Push the tag onto the list now that it is known to be valid
	EntityTypeTags.push(typetag);
}

//
// Flag the end of an entity code block and update parser state accordingly
//
// This routine emits any necessary epilog code for the entity, as well as shifts the internal
// state so that the current lexical scope and entity metadata are cleaned up and we return to
// the original parent context. In general this should be kept as flexible as possible so that
// we can support features like inner functions, nested anonymous scopes, and so on.
//
void CompilationSemantics::StoreEntityCode()
{
	if(!IsPrepass)
	{
		switch(EntityTypeTags.top())
		{
		//
		// The entity being processed is a function. In this case we need to generate some epilog
		// code to handle return values for functions that are not void. Should we fail to look
		// up the function signature of the entity we are exiting, we must raise a fatal error.
		//
		case Bytecode::EntityTags::Function:
			{
				FunctionSignatureSet::const_iterator iter = Session.FunctionSignatures.find(Session.StringPool.Pool(CurrentEntities.top()));
				if(iter == Session.FunctionSignatures.end())
					throw FatalException("Failed to locate function being finalized");

				if(iter->second.GetReturnType() != VM::EpochType_Void)
				{
					EmitterStack.top()->SetReturnRegister(FunctionReturnVars.top());
					FunctionReturnVars.pop();
					FunctionReturnTypeNames.pop();
				}

				EmitterStack.top()->ExitFunction();
			}
			break;

		//
		// Error case: the entity tag is not recognized. Fatal.
		//
		default:
			throw FatalException("Invalid entity type tag");
		}
	}

	EntityTypeTags.pop();
	CurrentEntities.pop();
	LexicalScopeStack.pop();
}



//-------------------------------------------------------------------------------
// Infix expression handling
//-------------------------------------------------------------------------------

//
// Record the parsing of an infix operator
//
// At this point we have already parsed the first term of the infix expression (be it
// a single variable/literal or another expression), and we now have parsed the operator
// itself (given in the parameter to the function). We need to save off the identifier
// so that once we parse the second half of the expression we can use it to assemble the
// final infix expression (respecting precedence rules) and then emit the corresponding
// bytecode.
//
void CompilationSemantics::StoreInfix(const std::wstring& identifier)
{
	StatementNames.push(identifier);
	StatementParamCount.push(0);

	if(!IsPrepass)
		CompileTimeParameters.push(std::vector<CompileTimeParameter>());

	PushParam(L"@@infixresult");
}

//
// Record the end of an infix expression
//
// This may not be the actual end of the expression per se, but rather simply signals
// that both terms and the infix operator itself have all been parsed and saved off into
// the state data. Finalization of the expression and handling of operator precedence
// rules is delegated to the FinalizeInfix() function.
//
void CompilationSemantics::CompleteInfix()
{
	std::wstring infixstatementname = StatementNames.top();
	StringHandle infixstatementnamehandle = Session.StringPool.Pool(infixstatementname);

	StatementNames.pop();
	StatementParamCount.pop();
	PushedItemTypes.push(ITEMTYPE_STATEMENT);

	if(!StatementNames.empty())
	{
		if(!IsPrepass)
		{
			InfixOperands.top().push_back(CompileTimeParameters.top());
			CompileTimeParameters.pop();
			if(!CompileTimeParameters.empty())
			{
				FunctionSignatureSet::const_iterator iter = Session.FunctionSignatures.find(infixstatementnamehandle);
				if(iter == Session.FunctionSignatures.end())
					throw FatalException("Unknown statement, cannot complete parsing");

				StatementTypes.push(iter->second.GetReturnType());
			}
		}
	}
	else
	{
		// We are in a special location such as the initializer of a return value
		FunctionSignatureSet::const_iterator iter = Session.FunctionSignatures.find(infixstatementnamehandle);
		if(iter == Session.FunctionSignatures.end())
			throw FatalException("Unknown statement, cannot complete parsing");

		StatementTypes.push(iter->second.GetReturnType());
		if(!IsPrepass)
			InfixOperands.top().push_back(CompileTimeParameters.top());
	}

	if(!IsPrepass)
		InfixOperators.top().push_back(infixstatementnamehandle);
}

//
// Finalize an infix expression
//
void CompilationSemantics::FinalizeInfix()
{
	if(!IsPrepass)
	{
		if(!InfixOperators.empty() && !InfixOperators.top().empty())
		{
			// TODO - sort operands and operators by infix precedence rules

			std::vector<StringHandle>::const_iterator operatoriter = InfixOperators.top().begin();
			for(std::vector<std::vector<CompileTimeParameter> >::const_iterator operandsetiter = InfixOperands.top().begin(); operandsetiter != InfixOperands.top().end(); ++operandsetiter)
			{
				for(std::vector<CompileTimeParameter>::const_iterator operanditer = operandsetiter->begin(); operanditer != operandsetiter->end(); ++operanditer)
				{
					switch(operanditer->Type)
					{
					case VM::EpochType_Integer:
						PendingEmitters.top().PushIntegerLiteral(operanditer->Payload.IntegerValue);
						break;

					case VM::EpochType_String:
						PendingEmitters.top().PushIntegerLiteral(operanditer->Payload.StringHandleValue);
						break;

					case VM::EpochType_Identifier:
						PendingEmitters.top().PushVariableValue(operanditer->Payload.StringHandleValue);
						break;

					case VM::EpochType_Expression:
						PendingEmitters.top().EmitBuffer(operanditer->ExpressionContents);
						break;

					default:
						throw FatalException("Unsupported operand type in infix expression");
					}
				}

				PendingEmitters.top().Invoke(*operatoriter);
				++operatoriter;
			}

			CompileTimeParameter ctparam(L"@@infixresult", VM::EpochType_Expression);
			ctparam.ExpressionType = StatementTypes.top();
			ctparam.ExpressionContents = PendingEmissionBuffers.top();
			CompileTimeParameters.top().push_back(ctparam);
		}
	}
}


//-------------------------------------------------------------------------------
// Entity parameter definitions
//-------------------------------------------------------------------------------

//
// Begin parsing the definition list of parameters passed to an entity (usually a function)
//
void CompilationSemantics::BeginParameterSet()
{
	NeedsPatternResolver = false;
	InsideParameterList = true;
	FunctionSignatureStack.push(FunctionSignature());
	if(IsPrepass)
	{
		StringHandle overload = AllocateNewOverloadedFunctionName(Session.StringPool.Pool(Strings.top()));
		AddLexicalScope(overload);
		OverloadDefinitions.push_back(std::make_pair(ParsePosition, overload));
	}
	else
	{
		for(std::list<std::pair<boost::spirit::classic::position_iterator<const char*>, StringHandle> >::const_iterator iter = OverloadDefinitions.begin(); iter != OverloadDefinitions.end(); ++iter)
		{
			if(iter->first == ParsePosition)
			{
				LexicalScopeStack.push(iter->second);
				return;
			}
		}

		throw FatalException("Lost track of a function overload definition somewhere along the line");
	}
}

//
// Finish parsing the definition list of parameters passed to an entity
//
void CompilationSemantics::EndParameterSet()
{
	InsideParameterList = false;
}

//
// Register the type of a function/entity parameter
//
void CompilationSemantics::RegisterParameterType(const std::wstring& type)
{
	ParamType = LookupTypeName(type);
}

//
// Register the name of a function/entity parameter
//
// Adds the parameter to the entity's signature, and optionally in the compilation phase
// adds the corresponding variable to the entity's local lexical scope.
//
void CompilationSemantics::RegisterParameterName(const std::wstring& name)
{
	FunctionSignatureStack.top().AddParameter(name, ParamType);
	
	if(!IsPrepass)
	{
		std::map<StringHandle, ScopeDescription>::iterator iter = LexicalScopeDescriptions.find(LexicalScopeStack.top());
		if(iter == LexicalScopeDescriptions.end())
			throw FatalException("No lexical scope has been registered for the identifier \"" + narrow(Session.StringPool.GetPooledString(LexicalScopeStack.top())) + "\"");
		
		iter->second.AddVariable(name, Session.StringPool.Pool(name), ParamType, VARIABLE_ORIGIN_PARAMETER);
	}
}

//
// Register that a function parameter requires pattern matching
//
// Pattern matching allows function overloads to be invoked depending on the values of the function
// parameters rather than just the types.
//
void CompilationSemantics::RegisterPatternMatchedParameter()
{
	VM::EpochTypeID paramtype = VM::EpochType_Error;

	switch(PushedItemTypes.top())
	{
	case ITEMTYPE_INTEGERLITERAL:
		{
			FunctionSignatureStack.top().AddPatternMatchedParameter(IntegerLiterals.top());
			IntegerLiterals.pop();
			paramtype = VM::EpochType_Integer;
		}
		break;

	case ITEMTYPE_STRINGLITERAL:
		throw NotImplementedException("TODO");
		StringLiterals.pop();
		break;

	case ITEMTYPE_STATEMENT:
		throw NotImplementedException("TODO");
		StatementNames.pop();
		StatementTypes.pop();
		break;

	default:
		throw NotImplementedException("Unsupported expression in pattern matched function parameter");
	}

	PushedItemTypes.pop();

	NeedsPatternResolver = true;

	if(!IsPrepass)
	{
		std::map<StringHandle, ScopeDescription>::iterator iter = LexicalScopeDescriptions.find(LexicalScopeStack.top());
		if(iter == LexicalScopeDescriptions.end())
			throw FatalException("No lexical scope has been registered for the identifier \"" + narrow(Session.StringPool.GetPooledString(LexicalScopeStack.top())) + "\"");
		
		iter->second.AddVariable(L"@@patternmatched", Session.StringPool.Pool(L"@@patternmatched"), paramtype, VARIABLE_ORIGIN_PARAMETER);
	}
}


//-------------------------------------------------------------------------------
// Entity return definitions
//-------------------------------------------------------------------------------

//
// Register the start of an entity's return value definition
//
// During the compilation process, we need to set up a temporary bytecode emitter and buffer,
// which receive the code generated by the expression in the initializer. Otherwise, the code
// will be emitted at the scope above the entity being defined, which is incorrect. Since the
// code needs access to the local variables defined by the entity's parameters, the code must
// be within the entity itself as prolog instructions; this also ensures that the initializer
// is called correctly every time the entity is invoked. To enforce this we redirect all code
// emission to a temporary buffer and then flush that buffer into the parent stream after all
// prolog code has been generated.
//
void CompilationSemantics::BeginReturnSet()
{
	ReturnsIncludedStatement.push(false);
	if(!IsPrepass)
	{
		PendingEmissionBuffers.push(std::vector<Byte>());
		PendingEmitters.push(ByteCodeEmitter(PendingEmissionBuffers.top()));
		EmitterStack.push(&PendingEmitters.top());

		InfixOperators.push(std::vector<StringHandle>());
		InfixOperands.push(std::vector<std::vector<CompileTimeParameter> >());
	}
}

//
// End the definition of an entity's return value
//
// This routine's responsibilities are two-fold. During the pre-pass phase, it handles
// adding the entity's parameter/return signature to the appropriate scope. During the
// compilation phase, it cleans up from any expression that appears in the initializer
// of the return value itself, as well as flushes the output buffer as described above
// in the remarks on BeginReturnSet.
//
void CompilationSemantics::EndReturnSet()
{
	if(ReturnsIncludedStatement.top())
	{
		if(!IsPrepass)
			CompileTimeParameters.c.clear();
	}

	if(IsPrepass)
	{
		Session.FunctionSignatures.insert(std::make_pair(LexicalScopeStack.top(), FunctionSignatureStack.top()));
		if(NeedsPatternResolver)
		{
			StringHandle resolvernamehandle = Session.StringPool.Pool(GetPatternMatchResolverName(Strings.top()));
			if(Session.FunctionSignatures.find(resolvernamehandle) == Session.FunctionSignatures.end())
			{
				Session.FunctionSignatures.insert(std::make_pair(resolvernamehandle, FunctionSignatureStack.top()));
				NeededPatternResolvers[resolvernamehandle] = FunctionSignatureStack.top();
			}
			OriginalFunctionsForPatternResolution.insert(std::make_pair(resolvernamehandle, LexicalScopeStack.top()));
		}
		else
		{
			// See if this is the default pattern matcher for any other pattern-matched overloads
			for(FunctionSignatureSet::const_iterator iter = Session.FunctionSignatures.begin(); iter != Session.FunctionSignatures.end(); ++iter)
			{
				if(Session.StringPool.GetPooledString(iter->first) == GetPatternMatchResolverName(Strings.top()))
				{
					OriginalFunctionsForPatternResolution.insert(std::make_pair(iter->first, LexicalScopeStack.top()));
					break;
				}
			}
		}
	}
	else
	{
		EmitPendingCode();
		EmitterStack.pop();

		InfixOperators.pop();
		InfixOperands.pop();
	}

	FunctionSignatureStack.pop();
	ReturnsIncludedStatement.pop();
}

//
// Track the return type of an entity, if it is not void
//
void CompilationSemantics::RegisterReturnType(const std::wstring& type)
{
	if(!IsPrepass)
		FunctionReturnTypeNames.push(type);
	FunctionSignatureStack.top().SetReturnType(LookupTypeName(type));
}

//
// Track the name of an entity's return value
//
// This identifier is used to create a local variable within the entity's lexical
// scope that holds the entity's return value during execution. Note that we also
// emit an instruction tagging the identifier for construction in the prolog code
// of the entity itself.
//
void CompilationSemantics::RegisterReturnName(const std::wstring& name)
{
	if(!IsPrepass)
	{
		StringHandle namehandle = Session.StringPool.Pool(name);
		FunctionReturnVars.push(namehandle);
		PendingEmitters.top().PushStringLiteral(namehandle);
		LexicalScopeDescriptions.find(LexicalScopeStack.top())->second.AddVariable(name, namehandle, FunctionSignatureStack.top().GetReturnType(), VARIABLE_ORIGIN_RETURN);
	}
}

//
// Register that the expression initializing an entity's return value has been fully parsed
//
// Once the initializer is parsed, we need to check the parsed expression for validity (i.e. type
// consistency) as well as emit construction code during the compilation phase. We also need some
// additional housekeeping work done to keep the parser state tidy.
//
void CompilationSemantics::RegisterReturnValue()
{
	// Shift output into a temporary buffer so it can be injected at the top of the entity's code
	if(!IsPrepass)
	{
		PendingEmissionBuffers.push(std::vector<Byte>());
		PendingEmitters.push(ByteCodeEmitter(PendingEmissionBuffers.top()));
	}

	// Perform type validation and housekeeping
	switch(FunctionSignatureStack.top().GetReturnType())
	{
	case VM::EpochType_Integer:
		if(PushedItemTypes.top() == ITEMTYPE_INTEGERLITERAL)
		{
			if(!IsPrepass)
				PendingEmitters.top().PushIntegerLiteral(IntegerLiterals.top());
			IntegerLiterals.pop();
		}
		else if(PushedItemTypes.top() == ITEMTYPE_STRING)
		{
			// TODO - do we need to validate the return value type here??
			if(!IsPrepass)
				PendingEmitters.top().PushVariableValue(Session.StringPool.Pool(Strings.top()));
			Strings.pop();
		}
		else if(PushedItemTypes.top() == ITEMTYPE_STATEMENT)
		{
			if(!IsPrepass)
			{
				if(StatementTypes.top() != VM::EpochType_Integer)
					throw TypeMismatchException("The function is defined as returning an integer but the provided default return value is not of an integer type.");
			}

			ReturnsIncludedStatement.top() = true;
		}
		else
			throw TypeMismatchException("The function is defined as returning an integer but the provided default return value is not of an integer type.");
		break;

	case VM::EpochType_String:
		if(PushedItemTypes.top() == ITEMTYPE_STRINGLITERAL)
		{
			if(!IsPrepass)
				PendingEmitters.top().PushStringLiteral(StringLiterals.top());
			StringLiterals.pop();
		}
		else if(PushedItemTypes.top() == ITEMTYPE_STRING)
		{
			// TODO - do we need to validate the return value type here??
			if(!IsPrepass)
				PendingEmitters.top().PushVariableValue(Session.StringPool.Pool(Strings.top()));
			Strings.pop();
		}
		else if(PushedItemTypes.top() == ITEMTYPE_STATEMENT)
		{
			if(!IsPrepass)
			{
				if(StatementTypes.top() != VM::EpochType_String)
					throw TypeMismatchException("The function is defined as returning a string but the provided default return value is not of string type.");
			}

			ReturnsIncludedStatement.top() = true;
		}
		else
			throw TypeMismatchException("The function is defined as returning a string but the provided default return value is not of string type.");
		break;

	default:
		throw NotImplementedException("Unsupported function return type in CompilationSemantics::RegisterReturnValue");
	}

	// Emit code to invoke the type constructor for the return variable
	if(!IsPrepass)
		PendingEmitters.top().Invoke(Session.StringPool.Pool(FunctionReturnTypeNames.top()));

	// Final cleanup
	StatementTypes.c.clear();
	PushedItemTypes.pop();
}



//-------------------------------------------------------------------------------
// Statements
//-------------------------------------------------------------------------------

//
// Speculatively parse the beginning of a statement
//
// Note that we can't yet shift anything onto the parse stacks, because we may actually
// get called here multiple times per statement, as the parser itself attempts to match
// the statement to the appropriate grammar rule. Instead we simply store the passed in
// token in a temporary slot, then shift it onto the appropriate stack later when it is
// clear what grammar rule we're actually trying to handle.
//
void CompilationSemantics::BeginStatement(const std::wstring& statementname)
{
	TemporaryString = statementname;
}

//
// Begin parsing the parameters passed to a statement
//
// At this point it is clear that we are parsing a function/entity invocation statement
// (as opposed to, for example, an assignment operation) and therefore we can shift the
// temporary string token onto the statement name stack accordingly. During the compile
// phase we also need to set up storage space for tracking the parameters themselves.
//
void CompilationSemantics::BeginStatementParams()
{
	StatementNames.push(TemporaryString);
	TemporaryString.clear();
	StatementParamCount.push(0);
	if(!IsPrepass)
	{
		CompileTimeParameters.push(std::vector<CompileTimeParameter>());
		InfixOperators.push(std::vector<StringHandle>());
		InfixOperands.push(std::vector<std::vector<CompileTimeParameter> >());

		PendingEmissionBuffers.push(std::vector<Byte>());
		PendingEmitters.push(ByteCodeEmitter(PendingEmissionBuffers.top()));
	}
}

//
// Finish parsing a subexpression/statement
//
// Note that this may not necessarily be the outermost statement, i.e. it may be nested
// within other statements, such as in the expression "foo(bar(baz() + 42)))". See also
// the FinalizeStatement function for handling the outermost statement.
//
// The primary goal of this routine is to maintain a record of the types of each nested
// expression so that parameter validation can be performed correctly. The routine also
// emits the actual code for invoking the associated function/entity.
//
void CompilationSemantics::CompleteStatement()
{
	std::wstring statementname = StatementNames.top();
	StringHandle statementnamehandle = Session.StringPool.Pool(statementname);

	StatementNames.pop();
	StatementParamCount.pop();
	PushedItemTypes.push(ITEMTYPE_STATEMENT);

	if(!IsPrepass)
	{
		InfixOperators.pop();
		InfixOperands.pop();

		VM::EpochTypeID outerexpectedtype = WalkCallChainForExpectedType(StatementNames.size() - 1);

		RemapFunctionToOverload(CompileTimeParameters.top(), outerexpectedtype, false, statementname, statementnamehandle);

		FunctionCompileHelperTable::const_iterator fchiter = CompileTimeHelpers.find(statementname);
		if(fchiter != CompileTimeHelpers.end())
			fchiter->second(GetLexicalScopeDescription(LexicalScopeStack.top()), CompileTimeParameters.top());

		FunctionSignatureSet::const_iterator fsiter = Session.FunctionSignatures.find(statementnamehandle);
		if(fsiter == Session.FunctionSignatures.end())
			Throw(RecoverableException("The function \"" + narrow(statementname) + "\" is not defined in this scope"));

		// Validate the parameters we've been given against the parameters to this overload
		if(CompileTimeParameters.top().size() == fsiter->second.GetNumParameters())
		{
			for(size_t i = 0; i < CompileTimeParameters.top().size(); ++i)
			{
				switch(CompileTimeParameters.top()[i].Type)
				{
				case VM::EpochType_Error:
					throw FatalException("Parameter's type is explicitly flagged as invalid");

				case VM::EpochType_Void:
					Throw(RecoverableException("Parameter has no type; cannot be passed to this function"));		// TODO - test case for this, and improved error message

				case VM::EpochType_Identifier:
					if(fsiter->second.GetParameter(i).Type == VM::EpochType_Identifier)
						PendingEmitters.top().PushStringLiteral(CompileTimeParameters.top()[i].Payload.StringHandleValue);
					else
					{
						std::map<StringHandle, ScopeDescription>::const_iterator iter = LexicalScopeDescriptions.find(LexicalScopeStack.top());
						if(iter == LexicalScopeDescriptions.end())
							throw FatalException("No lexical scope has been registered for the identifier \"" + narrow(Session.StringPool.GetPooledString(LexicalScopeStack.top())) + "\"");

						if(!iter->second.HasVariable(CompileTimeParameters.top()[i].StringPayload))
							Throw(RecoverableException("No variable by the name \"" + narrow(CompileTimeParameters.top()[i].StringPayload) + "\" was found in this scope"));

						if(iter->second.GetVariableTypeByID(CompileTimeParameters.top()[i].Payload.StringHandleValue) != fsiter->second.GetParameter(i).Type)
							Throw(RecoverableException("The variable \"" + narrow(CompileTimeParameters.top()[i].StringPayload) + "\" has the wrong type to be used for this parameter"));

						PendingEmitters.top().PushVariableValue(CompileTimeParameters.top()[i].Payload.StringHandleValue);
					}
					break;

				case VM::EpochType_Expression:
					if(fsiter->second.GetParameter(i).Type == CompileTimeParameters.top()[i].ExpressionType)
						PendingEmitters.top().EmitBuffer(CompileTimeParameters.top()[i].ExpressionContents);
					else
					{
						std::wostringstream errormsg;
						errormsg << L"Parameter " << (i + 1) << L" to function \"" << statementname << L"\" is of the wrong type";
						Throw(RecoverableException(narrow(errormsg.str())));
					}
					break;

				case VM::EpochType_Integer:
					if(fsiter->second.GetParameter(i).Type == VM::EpochType_Integer)
						PendingEmitters.top().PushIntegerLiteral(CompileTimeParameters.top()[i].Payload.IntegerValue);
					else
					{
						std::wostringstream errormsg;
						errormsg << L"Parameter " << (i + 1) << L" to function \"" << statementname << L"\" is of the wrong type";
						Throw(RecoverableException(narrow(errormsg.str())));
					}
					break;

				case VM::EpochType_String:
					if(fsiter->second.GetParameter(i).Type == VM::EpochType_String)
						PendingEmitters.top().PushStringLiteral(CompileTimeParameters.top()[i].Payload.StringHandleValue);
					else
					{
						std::wostringstream errormsg;
						errormsg << L"Parameter " << (i + 1) << L" to function \"" << statementname << L"\" is of the wrong type";
						Throw(RecoverableException(narrow(errormsg.str())));
					}
					break;

				default:
					throw NotImplementedException("Support for parameters of this type is not implemented");
				}
			}
		}
		else
		{
			std::wostringstream errormsg;
			errormsg << L"The function \"" << statementname << L"\" expects " << fsiter->second.GetNumParameters() << L" parameters, but ";
			errormsg << CompileTimeParameters.top().size() << L" were provided";
			Throw(RecoverableException(narrow(errormsg.str())));
		}

		StatementTypes.push(fsiter->second.GetReturnType());

		PendingEmitters.top().Invoke(statementnamehandle);

		CompileTimeParameters.pop();
		if(!CompileTimeParameters.empty())
		{
			CompileTimeParameter ctparam(L"@@expression", VM::EpochType_Expression);
			ctparam.ExpressionType = fsiter->second.GetReturnType();
			ctparam.ExpressionContents = PendingEmissionBuffers.top();
			CompileTimeParameters.top().push_back(ctparam);
		}
		else
			EmitterStack.top()->EmitBuffer(PendingEmissionBuffers.top());

		PendingEmitters.pop();
		PendingEmissionBuffers.pop();
	}
}

//
// Clean up parsing when a statement has been completely parsed
//
// This is invoked primarily when the outermost element of a statement has been located,
// meaning that no further code will be parsed without opening up a new statement.
//
void CompilationSemantics::FinalizeStatement()
{
	if(!IsPrepass)
		StatementTypes.c.clear();

	PushedItemTypes.pop();
}


//-------------------------------------------------------------------------------
// Assignment operations
//-------------------------------------------------------------------------------

//
// Begin parsing an assignment operation
//
// Note that by this point we already have parsed the left hand side of the assignment expression,
// so all we need to do is record that we're invoking the assignment operator, and prepare for
// parsing the right hand side of the expression.
//
void CompilationSemantics::BeginAssignment()
{
	if(!IsPrepass)
	{
		if(!LexicalScopeDescriptions[LexicalScopeStack.top()].HasVariable(TemporaryString))
			Throw(RecoverableException("The variable \"" + narrow(TemporaryString) + "\" is not defined in this scope"));
	}

	StatementNames.push(L"=");
	StatementParamCount.push(0);
	AssignmentTargets.push(Session.StringPool.Pool(TemporaryString));
	TemporaryString.clear();
	if(!IsPrepass)
	{
		CompileTimeParameters.push(std::vector<CompileTimeParameter>());
		InfixOperators.push(std::vector<StringHandle>());
		InfixOperands.push(std::vector<std::vector<CompileTimeParameter> >());

		PendingEmissionBuffers.push(std::vector<Byte>());
		PendingEmitters.push(ByteCodeEmitter(PendingEmissionBuffers.top()));
	}
}

//
// Finish parsing an assignment operation
//
// This function is invoked once both the left and right sides of the expression have been parsed,
// meaning that we can safely emit the assignment operator invocation code, and clean up.
//
void CompilationSemantics::CompleteAssignment()
{
	StatementNames.pop();
	if(!IsPrepass)
	{
		EmitterStack.top()->EmitBuffer(CompileTimeParameters.top()[0].ExpressionContents);

		PendingEmitters.pop();
		PendingEmissionBuffers.pop();
		InfixOperands.pop();
		InfixOperators.pop();

		// TODO - type checking
		EmitterStack.top()->AssignVariable(AssignmentTargets.top());
		CompileTimeParameters.pop();
		StatementTypes.c.clear();
	}
	AssignmentTargets.pop();
	StatementParamCount.pop();
}


//-------------------------------------------------------------------------------
// Parameter validation
//-------------------------------------------------------------------------------

//
// Queue a parameter passed to a statement for later validation, incrementing the passed parameter count as we go
//
void CompilationSemantics::PushStatementParam()
{
	if(!StatementParamCount.empty())
	{
		++StatementParamCount.top();
		PushParam(L"@@passedparam");
	}

	if(!IsPrepass)
	{
		InfixOperators.top().clear();
		InfixOperands.top().clear();
		PendingEmissionBuffers.top().clear();
	}
}

//
// Queue a parameter passed to a statement for later validation, incrementing the passed parameter count as we go
//
void CompilationSemantics::PushInfixParam()
{
	if(!StatementParamCount.empty())
	{
		++StatementParamCount.top();
		PushParam(L"@@infixoperand");
	}
}

//
// Actually enqueue a parameter for future validation
//
void CompilationSemantics::PushParam(const std::wstring& paramname)
{
	// Set up the compile-time parameter payload
	switch(PushedItemTypes.top())
	{
	case ITEMTYPE_STATEMENT:
		// Nothing to do, the nested statement handler has already taken care of things
		break;

	case ITEMTYPE_STRING:
		{
			CompileTimeParameter ctparam(paramname, VM::EpochType_Identifier);
			StringHandle handle = Session.StringPool.Pool(Strings.top());
			ctparam.StringPayload = Strings.top();
			ctparam.Payload.StringHandleValue = handle;
			Strings.pop();
			if(!IsPrepass)
				CompileTimeParameters.top().push_back(ctparam);
		}
		break;

	case ITEMTYPE_STRINGLITERAL:
		{
			CompileTimeParameter ctparam(paramname, VM::EpochType_String);
			ctparam.Payload.StringHandleValue = StringLiterals.top();
			StringLiterals.pop();
			if(!IsPrepass)
				CompileTimeParameters.top().push_back(ctparam);
		}
		break;

	case ITEMTYPE_INTEGERLITERAL:
		{
			CompileTimeParameter ctparam(paramname, VM::EpochType_Integer);
			ctparam.Payload.IntegerValue = IntegerLiterals.top();
			IntegerLiterals.pop();
			if(!IsPrepass)
				CompileTimeParameters.top().push_back(ctparam);
		}
		break;

	default:
		throw NotImplementedException("The parser stack contains something we aren't ready to handle in CompilationSemantics::PushParam");
	}

	PushedItemTypes.pop();
}

//
// Helper routine for type-checking parameters
//
bool CompilationSemantics::CheckParameterValidity(VM::EpochTypeID expectedtype)
{
	if(PushedItemTypes.empty())
		throw ParameterException("Expected at least one parameter here, but none have been provided");

	switch(PushedItemTypes.top())
	{
	case ITEMTYPE_STATEMENT:
		if(StatementTypes.empty())
			return false;
		else if(StatementTypes.top() != expectedtype)
		{
			StatementTypes.pop();
			return false;
		}
		break;

	case ITEMTYPE_STRING:
		if(expectedtype != VM::EpochType_Identifier)
		{
			if(GetLexicalScopeDescription(LexicalScopeStack.top()).HasVariable(Strings.top()))
				return (GetLexicalScopeDescription(LexicalScopeStack.top()).GetVariableTypeByID(Session.StringPool.Pool(Strings.top())) == expectedtype);

			return false;
		}
		break;

	case ITEMTYPE_STRINGLITERAL:
		if(expectedtype != VM::EpochType_String)
			return false;
		break;
	
	case ITEMTYPE_INTEGERLITERAL:
		if(expectedtype != VM::EpochType_Integer)
			return false;
		break;

	default:
		throw FatalException("Unrecognized ItemType value in LastPushedItemType, parser is probably broken");
	}

	return true;
}


//-------------------------------------------------------------------------------
// Finalization of parsing process
//-------------------------------------------------------------------------------

//
// Signal the end of the parsed source code file, and emit the final metadata instructions for the module
//
void CompilationSemantics::Finalize()
{
	if(!IsPrepass)
	{
		// Generate any pattern-matching resolver functions needed
		for(std::map<StringHandle, FunctionSignature>::const_iterator iter = NeededPatternResolvers.begin(); iter != NeededPatternResolvers.end(); ++iter)
		{
			EmitterStack.top()->DefineLexicalScope(iter->first, iter->second.GetNumParameters());
			for(size_t i = 0; i < iter->second.GetNumParameters(); ++i)
				EmitterStack.top()->LexicalScopeEntry(Session.StringPool.Pool(iter->second.GetParameter(i).Name), iter->second.GetParameter(i).Type, VARIABLE_ORIGIN_PARAMETER);

			EmitterStack.top()->EnterPatternResolver(iter->first);
			std::pair<std::multimap<StringHandle, StringHandle>::const_iterator, std::multimap<StringHandle, StringHandle>::const_iterator> range = OriginalFunctionsForPatternResolution.equal_range(iter->first);
			for(std::multimap<StringHandle, StringHandle>::const_iterator originalfunciter = range.first; originalfunciter != range.second; ++originalfunciter)
				EmitterStack.top()->ResolvePattern(originalfunciter->second, Session.FunctionSignatures.find(originalfunciter->second)->second);
			EmitterStack.top()->ExitPatternResolver();
		}

		for(std::map<StringHandle, std::wstring>::const_iterator iter = Session.StringPool.GetInternalPool().begin(); iter != Session.StringPool.GetInternalPool().end(); ++iter)
			EmitterStack.top()->PoolString(iter->first, iter->second);

		for(std::map<StringHandle, ScopeDescription>::const_iterator iter = LexicalScopeDescriptions.begin(); iter != LexicalScopeDescriptions.end(); ++iter)
		{
			EmitterStack.top()->DefineLexicalScope(iter->first, iter->second.GetVariableCount());
			for(size_t i = 0; i < iter->second.GetVariableCount(); ++i)
				EmitterStack.top()->LexicalScopeEntry(Session.StringPool.Pool(iter->second.GetVariableName(i)), iter->second.GetVariableTypeByIndex(i), iter->second.GetVariableOrigin(i));
		}
	}
}


//-------------------------------------------------------------------------------
// Lexical scope metadata
//-------------------------------------------------------------------------------

//
// Track the creation of a lexical scope; used to emit metadata for the scope's contents later on
//
void CompilationSemantics::AddLexicalScope(StringHandle scopename)
{
	if(LexicalScopeDescriptions.find(scopename) != LexicalScopeDescriptions.end())
		Throw(RecoverableException("An entity with the identifier \"" + narrow(Session.StringPool.GetPooledString(scopename)) + "\" has already been defined"));
	
	LexicalScopeDescriptions.insert(std::make_pair(scopename, ScopeDescription()));
	LexicalScopeStack.push(scopename);
}

//
// Retrieve the description of the given lexical scope
//
ScopeDescription& CompilationSemantics::GetLexicalScopeDescription(StringHandle scopename)
{
	std::map<StringHandle, ScopeDescription>::iterator iter = LexicalScopeDescriptions.find(scopename);
	if(iter == LexicalScopeDescriptions.end())
		throw InvalidIdentifierException("No lexical scope has been attached to this identifier");

	return iter->second;
}


//-------------------------------------------------------------------------------
// Function overload management
//-------------------------------------------------------------------------------

//
// Allocate an internal alias for an overloaded function name
//
// When a function is overloaded, the overloads are given magic name suffixes to help distinguish
// them from other function overloads. This function creates and pools a new alias if needed; for
// non-overload situations it just returns the given original name handle.
//
StringHandle CompilationSemantics::AllocateNewOverloadedFunctionName(StringHandle originalname)
{
	std::set<StringHandle>& overloadednames = Session.FunctionOverloadNames[originalname];
	if(overloadednames.empty())
	{
		overloadednames.insert(originalname);
		return originalname;
	}

	std::wostringstream mangled;
	mangled << Session.StringPool.GetPooledString(originalname) << L"@@overload@@" << overloadednames.size();
	StringHandle ret = Session.StringPool.Pool(mangled.str());
	overloadednames.insert(ret);
	return ret;
}

//
// Given a set of parameters, look up the appropriate matching function overload
//
void CompilationSemantics::RemapFunctionToOverload(const std::vector<CompileTimeParameter>& params, VM::EpochTypeID expectedreturntype, bool allowpartialparamsets, std::wstring& out_remappedname, StringHandle& out_remappednamehandle) const
{
	std::map<StringHandle, std::set<StringHandle> >::const_iterator overloadsiter = Session.FunctionOverloadNames.find(out_remappednamehandle);
	if(overloadsiter == Session.FunctionOverloadNames.end())
		return;

	bool patternmatching = false;
	const std::set<StringHandle>& overloadednames = overloadsiter->second;
	for(std::set<StringHandle>::const_iterator iter = overloadednames.begin(); iter != overloadednames.end(); ++iter)
	{
		FunctionSignatureSet::const_iterator signatureiter = Session.FunctionSignatures.find(*iter);
		if(signatureiter == Session.FunctionSignatures.end())
			throw FatalException("Tried to map a function overload to an undefined function signature");

		if(allowpartialparamsets)
		{
			if(params.size() > signatureiter->second.GetNumParameters())
				continue;
		}
		else
		{
			if(params.size() != signatureiter->second.GetNumParameters())
				continue;
		}

		if(expectedreturntype != VM::EpochType_Error && signatureiter->second.GetReturnType() != expectedreturntype)
			continue;

		bool matched = true;
		bool patternsucceeded = true;
		for(size_t i = 0; i < params.size(); ++i)
		{
			if(params[i].Type == VM::EpochType_Identifier)
			{
				if(signatureiter->second.GetParameter(i).Type != VM::EpochType_Identifier)
				{
					std::map<StringHandle, ScopeDescription>::const_iterator iter = LexicalScopeDescriptions.find(LexicalScopeStack.top());
					if(iter == LexicalScopeDescriptions.end())
						throw FatalException("No lexical scope has been registered for the identifier \"" + narrow(Session.StringPool.GetPooledString(LexicalScopeStack.top())) + "\"");

					if(!iter->second.HasVariable(params[i].StringPayload))
						Throw(RecoverableException("No variable by the name \"" + narrow(params[i].StringPayload) + "\" was found in this scope"));

					if(iter->second.GetVariableTypeByID(params[i].Payload.StringHandleValue) != signatureiter->second.GetParameter(i).Type)
					{
						patternsucceeded = false;
						matched = false;
						break;
					}
				}
			}
			else if(params[i].Type == VM::EpochType_Expression)
			{
				if(signatureiter->second.GetParameter(i).Type != params[i].ExpressionType)
				{
					patternsucceeded = false;
					matched = false;
					break;
				}
			}
			else if(params[i].Type != signatureiter->second.GetParameter(i).Type)
			{
				patternsucceeded = false;
				matched = false;
				break;
			}

			if(signatureiter->second.GetParameter(i).Name == L"@@patternmatched")
			{
				patternmatching = true;
				switch(params[i].Type)
				{
				case VM::EpochType_Integer:
					if(signatureiter->second.GetParameter(i).Payload.IntegerValue != params[i].Payload.IntegerValue)
						patternsucceeded = false;
					break;

				default:
					throw NotImplementedException("Unsupported pattern-matched parameter type");
				}

				if(!matched || !patternsucceeded)
					break;
			}
		}

		if(matched)
		{
			if(patternmatching && !patternsucceeded)
			{
				out_remappedname = GetPatternMatchResolverName(out_remappedname);
				out_remappednamehandle = Session.StringPool.Pool(out_remappedname);
			}
			else
			{
				out_remappednamehandle = *iter;
				out_remappedname = Session.StringPool.GetPooledString(out_remappednamehandle);
			}
			return;
		}
	}

	if(patternmatching)
		Throw(RecoverableException("No function matches this parameter pattern"));
		
	Throw(RecoverableException("No function overload takes a matching parameter set"));
}


//-------------------------------------------------------------------------------
// Additional helpers
//-------------------------------------------------------------------------------

//
// Emit the contents of the most recent pending emitter into the current emitter
//
// Pending emitters are used to buffer emitted code for injection into another buffer. For example,
// when generating prolog code for an entity, it is necessary to buffer that prolog code during the
// compilation process so that it can be injected inside the entity's code block.
//
void CompilationSemantics::EmitPendingCode()
{
	if(!IsPrepass)
	{
		if(!PendingEmitters.empty())
		{
			EmitterStack.top()->EmitBuffer(PendingEmissionBuffers.top());
			PendingEmitters.pop();
			PendingEmissionBuffers.pop();
		}
	}
}

//
// Convert a type name into an internal type annotation constant
//
VM::EpochTypeID CompilationSemantics::LookupTypeName(const std::wstring& type) const
{
	if(type == L"integer")
		return VM::EpochType_Integer;
	else if(type == L"string")
		return VM::EpochType_String;

	throw NotImplementedException("Cannot map the type name \"" + narrow(type) + "\" to an internal type ID");
}

//
// Decorate a function name to produce the internal name of the function used to resolve pattern matches for the wrapped function
//
std::wstring CompilationSemantics::GetPatternMatchResolverName(const std::wstring& originalname) const
{
	return originalname + L"@@resolve_pattern_match";
}


VM::EpochTypeID CompilationSemantics::WalkCallChainForExpectedType(size_t index) const
{
	if(StatementNames.empty() || StatementNames.c.size() <= index)
		return VM::EpochType_Error;

	std::wstring name = StatementNames.c.at(index);
	StringHandle namehandle = Session.StringPool.Pool(name);
	unsigned paramindex = StatementParamCount.c.at(index);

	VM::EpochTypeID outerexpectedtype = VM::EpochType_Error;
	if(index > 0)
		outerexpectedtype = WalkCallChainForExpectedType(index - 1);


	RemapFunctionToOverload(CompileTimeParameters.c.at(index), outerexpectedtype, true, name, namehandle);

	// TODO - this doesn't handle assignments yet
	FunctionSignatureSet::const_iterator iter = Session.FunctionSignatures.find(namehandle);
	if(iter == Session.FunctionSignatures.end())
		Throw(RecoverableException("The function \"" + narrow(name) + "\" is not defined in this scope"));

	return iter->second.GetParameter(paramindex).Type;
}


//-------------------------------------------------------------------------------
// Safety/debug checks
//-------------------------------------------------------------------------------

//
// Exception throwing wrapper
//
// We wrap certain exceptions in boost::spirit's exception type so they can be caught by the guard
// clauses provided in the grammar itself. This allows us to throw internal errors that cause the
// parser to skip certain tokens and resume parsing in a new position, for instance.
//
template <typename ExceptionT>
void CompilationSemantics::Throw(const ExceptionT& exception) const
{
	boost::spirit::classic::throw_<ExceptionT, PosIteratorT>(ParsePosition, exception);
}

//
// Perform a simple set of sanity checks to make sure the parser state is consistent and cleaned up
//
// Invoke this after each parsing pass to ensure that we didn't leave something on the parser stack
// accidentally, or otherwise lose track of something important during the parse process.
//
void CompilationSemantics::SanityCheck() const
{
	if(!Strings.empty() || !EntityTypeTags.empty() || !IntegerLiterals.empty() || !StringLiterals.empty() || !FunctionSignatureStack.empty()
	|| !StatementNames.empty() || !StatementParamCount.empty() || !StatementTypes.empty() || !LexicalScopeStack.empty() || !CompileTimeParameters.empty()
	|| !CurrentEntities.empty() || !FunctionReturnVars.empty() || !PendingEmissionBuffers.empty() || !PendingEmitters.empty() || !AssignmentTargets.empty()
	|| !PushedItemTypes.empty() || !ReturnsIncludedStatement.empty() || !FunctionReturnTypeNames.empty() || !InfixOperators.empty() || !InfixOperands.empty())
		throw FatalException("Parser leaked a resource");
}
