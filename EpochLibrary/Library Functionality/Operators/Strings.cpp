//
// The Epoch Language Project
// Epoch Standard Library
//
// Library routines for string-handling operators
//

#include "pch.h"

#include "Library Functionality/Operators/Strings.h"

#include "Utility/StringPool.h"
#include "Utility/NoDupeMap.h"
#include "Utility/Strings.h"

#include "Virtual Machine/VirtualMachine.h"

#include "Metadata/Precedences.h"


extern VM::ExecutionContext* GlobalExecutionContext;


namespace
{

	StringHandle ConcatHandle = 0;
	StringHandle LengthHandle = 0;
	StringHandle NarrowStringHandle = 0;

}


//
// Bind the library to a function metadata table
//
void StringLibrary::RegisterLibraryFunctions(FunctionSignatureSet& signatureset)
{
	{
		FunctionSignature signature;
		signature.AddParameter(L"s1", Metadata::EpochType_String, false);
		signature.AddParameter(L"s2", Metadata::EpochType_String, false);
		signature.SetReturnType(Metadata::EpochType_String);
		AddToMapNoDupe(signatureset, std::make_pair(ConcatHandle, signature));
	}
	{
		FunctionSignature signature;
		signature.AddParameter(L"s", Metadata::EpochType_String, false);
		signature.SetReturnType(Metadata::EpochType_Integer);
		AddToMapNoDupe(signatureset, std::make_pair(LengthHandle, signature));
	}
	{
		FunctionSignature signature;
		signature.AddParameter(L"s", Metadata::EpochType_String, false);
		signature.SetReturnType(Metadata::EpochType_Buffer);
		AddToMapNoDupe(signatureset, std::make_pair(NarrowStringHandle, signature));
	}
}

//
// Bind the library to the infix operator table
//
void StringLibrary::RegisterInfixOperators(StringSet& infixtable, PrecedenceTable& precedences)
{
	AddToSetNoDupe(infixtable, L";");
	AddToMapNoDupe(precedences, std::make_pair(ConcatHandle, PRECEDENCE_CONCATENATION));
}

void StringLibrary::PoolStrings(StringPoolManager& stringpool)
{
	ConcatHandle = stringpool.Pool(L";");
	LengthHandle = stringpool.Pool(L"length");
	NarrowStringHandle = stringpool.Pool(L"narrowstring");
}

void StringLibrary::RegisterJITTable(JIT::JITTable& table)
{
	AddToMapNoDupe(table.LibraryExports, std::make_pair(ConcatHandle, "EpochLib_StrConcat"));
}



extern "C" StringHandle EpochLib_StrConcat(StringHandle a, StringHandle b)
{
	std::wstring result = GlobalExecutionContext->OwnerVM.GetPooledString(a) + GlobalExecutionContext->OwnerVM.GetPooledString(b);
	return GlobalExecutionContext->OwnerVM.PoolString(result);
}

