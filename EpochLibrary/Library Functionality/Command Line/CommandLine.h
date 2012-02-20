//
// The Epoch Language Project
// Epoch Standard Library
//
// Library routines for command line processing
//

#pragma once


// Dependencies
#include "Metadata/FunctionSignature.h"
#include "Libraries/Library.h"


// Forward declarations
class StringPoolManager;

namespace VM
{
	class ExecutionContext;
}


namespace CommandLineLibrary
{

	void RegisterLibraryFunctions(FunctionSignatureSet& signatureset, StringPoolManager& stringpool);
	void RegisterLibraryFunctions(FunctionInvocationTable& table, StringPoolManager& stringpool);

}

