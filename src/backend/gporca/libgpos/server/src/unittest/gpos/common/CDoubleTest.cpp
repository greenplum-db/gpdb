//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDoubleTest.cpp
//
//	@doc:
//		Tests for the floating-point wrapper class.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/error/CAutoTrace.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpos/common/CDoubleTest.h"


using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CDoubleTest::EresUnittest
//
//	@doc:
//		Driver for unittests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDoubleTest::EresUnittest()
{
	CUnittest rgut[] = {
		GPOS_UNITTEST_FUNC(CDoubleTest::EresUnittest_Arithmetic),
		GPOS_UNITTEST_FUNC(CDoubleTest::EresUnittest_Bool),
		GPOS_UNITTEST_FUNC(CDoubleTest::EresUnittest_Convert),
		GPOS_UNITTEST_FUNC(CDoubleTest::EresUnittest_Limits),
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDoubleTest::EresUnittest_Arithmetic
//
//	@doc:
//		Test arithmetic operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDoubleTest::EresUnittest_Arithmetic()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CDouble fp1(2.5);
	CDouble fp2(3.5);

	CDouble fpAdd(fp1 + fp2);
	CDouble fpSubtract(fpAdd - fp2);
	CDouble fpMultiply(fp1 * fp2);
	CDouble fpDivide(fpMultiply / fp2);
	CDouble fpAbs(fp1.Absolute());
	CDouble fpFloor(fp1.Floor());
	CDouble fpCeil(fp1.Ceil());
	CDouble fpPow(fp1.Pow(fp2));
	CDouble fpLog2(fp2.Log2());

	GPOS_RTL_ASSERT(fp1.Get() + fp2.Get() == fpAdd.Get());
	GPOS_RTL_ASSERT(fpAdd.Get() - fp2.Get() == fpSubtract.Get());
	GPOS_RTL_ASSERT(fp1.Get() == fpSubtract.Get());
	GPOS_RTL_ASSERT(fp1.Get() * fp2.Get() == fpMultiply.Get());
	GPOS_RTL_ASSERT(fpMultiply.Get() / fp2.Get() == fpDivide.Get());
	GPOS_RTL_ASSERT(fp1.Get() == fpDivide.Get());
	GPOS_RTL_ASSERT(fp1.Get() == fpAbs);
	GPOS_RTL_ASSERT(1.0 == fpCeil - fpFloor);
	GPOS_RTL_ASSERT(fpLog2 > 1.0 && fpLog2 < 2.0);

	CDouble fp3(10.0);
	fp3 = fp1 + fp2;

	CAutoTrace trace(mp);
	IOstream &os(trace.Os());

	os << "Arithmetic operations: " << std::endl
	   << fp1 << " + " << fp2 << " = " << fpAdd << std::endl
	   << fpAdd << " - " << fp2 << " = " << fpSubtract << std::endl
	   << fp1 << " * " << fp2 << " = " << fpMultiply << std::endl
	   << fpMultiply << " / " << fp2 << " = " << fpDivide << std::endl
	   << "Absolute(" << fp1 << ") = " << fpAbs << std::endl
	   << "Floor(" << fp1 << ") = " << fpCeil << std::endl
	   << "Ceil(" << fp1 << ") = " << fpCeil << std::endl
	   << "Pow(" << fp1 << "," << fp2 << ") = " << fpPow << std::endl
	   << "Log2(" << fp2 << ") = " << fpLog2 << std::endl
	   << fp1 << " + " << fp2 << " = " << fp3 << std::endl;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDoubleTest::EresUnittest_Bool
//
//	@doc:
//		Test comparison operations
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDoubleTest::EresUnittest_Bool()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CDouble fp1(2.5);
	CDouble fp2(3.5);
	CDouble fp3(3.5);

	GPOS_ASSERT(fp1 < fp2);
	GPOS_ASSERT(fp1 <= fp2);
	GPOS_ASSERT(fp1 != fp2);
	GPOS_ASSERT(fp2 > fp1);
	GPOS_ASSERT(fp2 >= fp1);
	GPOS_ASSERT(fp2 == fp3);
	GPOS_ASSERT(fp2 >= fp3);
	GPOS_ASSERT(fp2 <= fp3);

	CAutoTrace trace(mp);
	IOstream &os(trace.Os());

	os << "Boolean operations: " << std::endl
	   << fp1 << " < " << fp2 << " = " << (fp1 < fp2) << std::endl
	   << fp1 << " <= " << fp2 << " = " << (fp1 <= fp2) << std::endl
	   << fp1 << " != " << fp2 << " = " << (fp1 != fp2) << std::endl
	   << fp2 << " > " << fp1 << " = " << (fp2 > fp1) << std::endl
	   << fp2 << " >= " << fp1 << " = " << (fp2 >= fp1) << std::endl
	   << fp2 << " == " << fp3 << " = " << (fp2 == fp3) << std::endl
	   << fp2 << " >= " << fp3 << " = " << (fp2 >= fp2) << std::endl
	   << fp2 << " <= " << fp3 << " = " << (fp2 <= fp2) << std::endl;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDoubleTest::EresUnittest_Convert
//
//	@doc:
//		Test conversions
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDoubleTest::EresUnittest_Convert()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CDouble fp(3.5);

	CAutoTrace trace(mp);
	IOstream &os(trace.Os());

	os << "Conversions:" << std::endl
	   << ULONG(10) << "u + " << fp << " = " << (ULONG(10) + fp) << std::endl
	   << ULLONG(10) << "ul - " << fp << " = " << (ULLONG(10) - fp) << std::endl
	   << INT(10) << " * " << fp << " = " << (INT(10) * fp) << std::endl
	   << LINT(10) << "l / " << fp << " = " << (LINT(10) / fp) << std::endl
	   << "-'10.0' = " << (-CDouble(clib::Strtod("10.0"))) << std::endl
	   << "Pow(" << ULONG(3) << ") = " << fp.Pow(ULONG(3)) << std::endl;

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDoubleTest::EresUnittest_Limits
//
//	@doc:
//		Test underflow and overflow limits
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDoubleTest::EresUnittest_Limits()
{
	CAutoMemoryPool amp;
	CMemoryPool *mp = amp.Pmp();

	CDouble fpZero(0);
	CDouble fpInf(1e10 / fpZero);

	CAutoTrace trace(mp);
	IOstream &os(trace.Os());

	GPOS_ASSERT(fpZero == fpZero / fpInf);
	GPOS_ASSERT(fpZero == fpZero / 2);
	GPOS_ASSERT(fpInf == fpInf / fpZero);
	GPOS_ASSERT(fpInf == fpInf * fpInf);
	GPOS_ASSERT(fpInf == fpInf * 2);
	GPOS_ASSERT(1.0 == fpInf * fpZero);
	GPOS_ASSERT(1.0 == (fpInf * fpZero) * (fpInf * fpZero));
	GPOS_ASSERT(1.0 == (fpInf * fpZero) / (fpInf * fpZero));

	os << "Limits:" << std::endl
	   << "zero = " << fpZero << std::endl
	   << "inf = " << fpInf << std::endl
	   << "zero / inf = " << (fpZero / fpInf) << std::endl
	   << "zero / 2 = " << (fpZero / 2) << std::endl
	   << "inf / zero = " << (fpInf / fpZero) << std::endl
	   << "inf * inf = " << (fpInf * fpInf) << std::endl
	   << "inf * 2 = " << (fpInf * fpInf) << std::endl
	   << "inf * zero = " << (fpInf * fpZero) << std::endl
	   << "(inf * zero) * (inf * zero) = "
	   << (fpInf * fpZero) * (fpInf * fpZero) << std::endl
	   << "(inf * zero) / (inf * zero) = "
	   << (fpInf * fpZero) / (fpInf * fpZero) << std::endl;

	return GPOS_OK;
}

// EOF
