//
// The Epoch Language Project
// Epoch Standard Library
//
// String pool management for flow control library
//

#include "pch.h"

#include "Library Functionality/Flow Control/StringPooling.h"

#include "Utility/StringPool.h"


extern StringHandle IfHandle;
extern StringHandle ElseIfHandle;
extern StringHandle ElseHandle;

extern StringHandle WhileHandle;


//
// Register all the strings used by the flow control library
//
// This is done here to ensure that strings are always registered in a consistent
// order, which avoids issues with handle collisions when pooling strings in the VM.
//
void FlowControl::PoolStrings(StringPoolManager& stringpool)
{
	IfHandle = stringpool.Pool(L"if");
	ElseIfHandle = stringpool.Pool(L"elseif");
	ElseHandle = stringpool.Pool(L"else");

	WhileHandle = stringpool.Pool(L"while");
	stringpool.Pool(L"do");
}
