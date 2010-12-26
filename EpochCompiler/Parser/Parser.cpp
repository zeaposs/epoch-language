//
// The Epoch Language Project
// EPOCHCOMPILER Compiler Toolchain
//
// Wrapper class for source code parsing functionality
//

#include "pch.h"

#include "Parser/Parser.h"
#include "Parser/Grammars.h"

#include "Compilation/SemanticActionInterface.h"

#include "Metadata/FunctionSignature.h"

#include "Utility/Strings.h"


using namespace boost::spirit::classic;


//
// Parse a given block of code, invoking the bound set of
// semantic actions during the parse process
//
bool Parser::Parse(const std::wstring& code, const std::wstring& filename)
{
	ByteBuffer memblock;
	memblock.reserve(code.length() + 20);		// paranoia padding

	std::string narrowedcode = narrow(code);
	std::copy(narrowedcode.begin(), narrowedcode.end(), std::back_inserter(memblock));

	// The parser prefers to have trailing whitespace, for whatever reason.
	memblock.push_back('\n');

	position_iterator<const char*> start(&memblock[0], &memblock[0] + memblock.size() - 1, narrow(filename));
    position_iterator<const char*> end;

	SemanticActions.SetPrepassMode(true);
	FundamentalGrammar grammar(SemanticActions, Identifiers);
    SkipGrammar skip;

	parse_info<position_iterator<const char*> > result;

	try
	{
		// First pass: build up the list of entities defined in the code
		result = parse(start, end, grammar >> end_p, skip);
		if(!result.full)
			return false;

		// Sanity check to make sure the parser is in a clean state
		SemanticActions.SanityCheck();

		// Don't do the second pass if prepass failed
		if(SemanticActions.DidFail())
			return false;

		// Second pass: traverse into each function and generate the corresponding bytecode
		do
		{
			SemanticActions.SetPrepassMode(false);
			result = parse(start, end, grammar >> end_p, skip);
			if(!result.full)
				return false;
		} while(!SemanticActions.InferenceComplete());
	}
	catch(const parser_error<ParameterException, position_iterator<const char*> >& e)
	{
		throw e.descriptor;
	}
	catch(const parser_error<TypeMismatchException, position_iterator<const char*> >& e)
	{
		throw e.descriptor;
	}

	return true;
}

