//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTranslatorExprToDXLTest.h
//
//	@doc:
//		Test for translating CExpression to DXL
//---------------------------------------------------------------------------
#ifndef GPOPT_CTranslatorExprToDXLTest_H
#define GPOPT_CTranslatorExprToDXLTest_H

#include "gpos/base.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CTranslatorExprToDXLTest
//
//	@doc:
//		Unittests
//
//---------------------------------------------------------------------------
class CTranslatorExprToDXLTest
{
private:
	// counter used to mark last successful test
	static ULONG m_ulTestCounter;

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_RunTests();
	static GPOS_RESULT EresUnittest_RunMinidumpTests();

};	// class CTranslatorExprToDXLTest
}  // namespace gpopt

#endif	// !GPOPT_CTranslatorExprToDXLTest_H

// EOF
