//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CRangeTest.h
//
//	@doc:
//		Test for ranges
//---------------------------------------------------------------------------
#ifndef GPOPT_CRangeTest_H
#define GPOPT_CRangeTest_H

#include "gpos/base.h"

#include "gpopt/base/CRange.h"

#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CRangeTest
//
//	@doc:
//		Static unit tests for ranges
//
//---------------------------------------------------------------------------
class CRangeTest
{
	typedef IDatum *(*PfPdatum)(CMemoryPool *mp, INT i);

private:
	static GPOS_RESULT EresInitAndCheckRanges(CMemoryPool *mp, IMDId *mdid,
											  PfPdatum pf);

	static void TestRangeRelationship(CMemoryPool *mp, CRange *prange1,
									  CRange *prange2, CRange *prange3,
									  CRange *prange4, CRange *prange5);

	static void PrintRange(CMemoryPool *mp, CColRef *colref, CRange *prange);

	// int2 datum
	static IDatum *CreateInt2Datum(CMemoryPool *mp, INT i);

	// int4 datum
	static IDatum *CreateInt4Datum(CMemoryPool *mp, INT i);

	// int8 datum
	static IDatum *CreateInt8Datum(CMemoryPool *mp, INT li);

public:
	// unittests
	static GPOS_RESULT EresUnittest();
	static GPOS_RESULT EresUnittest_CRangeInt2();
	static GPOS_RESULT EresUnittest_CRangeInt4();
	static GPOS_RESULT EresUnittest_CRangeInt8();
	static GPOS_RESULT EresUnittest_CRangeFromScalar();

};	// class CRangeTest
}  // namespace gpopt

#endif	// !GPOPT_CRangeTest_H


// EOF
