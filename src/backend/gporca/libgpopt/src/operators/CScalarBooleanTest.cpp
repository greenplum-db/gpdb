//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarBooleanTest.cpp
//
//	@doc:
//		Implementation of scalar boolean test operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CScalarBooleanTest.h"
#include "gpopt/operators/CExpressionHandle.h"

#include "naucrates/md/IMDTypeBool.h"

using namespace gpopt;
using namespace gpmd;

const WCHAR CScalarBooleanTest::m_rgwszBoolTest[EbtSentinel][30] = {
	GPOS_WSZ_LIT("Is True"),	GPOS_WSZ_LIT("Is Not True"),
	GPOS_WSZ_LIT("Is False"),	GPOS_WSZ_LIT("Is Not False"),
	GPOS_WSZ_LIT("Is Unknown"), GPOS_WSZ_LIT("Is Not Unknown"),
};


// mapping operator type and child value to the corresponding result value for Boolean expression evaluation,
// in each entry, we have three values:
// (1) operator type: IS_TRUE / IS_NOT_TRUE / IS_FALSE / IS_NOT_FALSE / IS_UNKNOWN / IS_NOT_UNKNOWN
// (2) child value: EberTrue / EberFalse / EberNull / EberAny
// (3) expected result:  EberTrue / EberFalse / EberNull / EberAny

const BYTE CScalarBooleanTest::m_rgBoolEvalMap[][3] = {
	{EbtIsTrue, EberTrue, EberTrue},	  // IS_TRUE(True) = True
	{EbtIsTrue, EberFalse, EberFalse},	  // IS_TRUE(False) = False
	{EbtIsTrue, EberNull, EberFalse},	  // IS_TRUE(Null) = False
	{EbtIsTrue, EberNotTrue, EberFalse},  // IS_TRUE(NotTrue) = False
	{EbtIsTrue, EberAny, EberAny},		  // IS_TRUE(Any) = Any

	{EbtIsNotTrue, EberTrue, EberFalse},	// IS_NOT_TRUE(True) = False
	{EbtIsNotTrue, EberFalse, EberTrue},	// IS_NOT_TRUE(False) = True
	{EbtIsNotTrue, EberNull, EberTrue},		// IS_NOT_TRUE(Null) = True
	{EbtIsNotTrue, EberNotTrue, EberTrue},	// IS_NOT_TRUE(NotTrue) = True
	{EbtIsNotTrue, EberAny, EberAny},		// IS_NOT_TRUE(Any) = Any

	{EbtIsFalse, EberTrue, EberFalse},	 // IS_FALSE(True) = False
	{EbtIsFalse, EberFalse, EberTrue},	 // IS_FALSE(False) = True
	{EbtIsFalse, EberNull, EberFalse},	 // IS_FALSE(Null) = False
	{EbtIsFalse, EberNotTrue, EberAny},	 // IS_FALSE(NotTrue) = Any
	{EbtIsFalse, EberAny, EberAny},		 // IS_FALSE(Any) = Any

	{EbtIsNotFalse, EberTrue, EberTrue},	// IS_NOT_FALSE(True) = True
	{EbtIsNotFalse, EberFalse, EberFalse},	// IS_NOT_FALSE(False) = False
	{EbtIsNotFalse, EberNull, EberTrue},	// IS_NOT_FALSE(Null) = True
	{EbtIsNotFalse, EberNotTrue, EberAny},	// IS_NOT_FALSE(NotTrue) = Any
	{EbtIsNotFalse, EberAny, EberAny},		// IS_NOT_FALSE(Any) = Any

	{EbtIsUnknown, EberTrue, EberFalse},   // IS_UNKNOWN(True) = False
	{EbtIsUnknown, EberFalse, EberFalse},  // IS_UNKNOWN(False) = False
	{EbtIsUnknown, EberNull,
	 EberTrue},	 // IS_UNKNOWN(Null) = True  ---> Note that UNKNOWN in BooleanTest means NULL
	{EbtIsUnknown, EberNotTrue, EberAny},  // IS_UNKNOWN(NotTrue) = Any
	{EbtIsUnknown, EberAny, EberAny},	   // IS_UNKNOWN(Any) = Any

	{EbtIsNotUnknown, EberTrue, EberTrue},	 // IS_NOT_UNKNOWN(True) = True
	{EbtIsNotUnknown, EberFalse, EberTrue},	 // IS_NOT_UNKNOWN(False) = True
	{EbtIsNotUnknown, EberNull,
	 EberFalse},  // IS_NOT_UNKNOWN(Null) = False ---> Note that NOT_UNKNOWN in BooleanTest means NOT-NULL
	{EbtIsNotUnknown, EberNotTrue, EberAny},  // IS_NOT_UNKNOWN(NotTrue) = Any
	{EbtIsNotUnknown, EberAny, EberAny},	  // IS_NOT_UNKNOWN(Any) = Any
};

//---------------------------------------------------------------------------
//	@function:
//		CScalarBooleanTest::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarBooleanTest::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		return m_ebt == CScalarBooleanTest::PopConvert(pop)->Ebt();
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarBooleanTest::MdidType
//
//	@doc:
//		Expression type
//
//---------------------------------------------------------------------------
IMDId *
CScalarBooleanTest::MdidType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	return md_accessor->PtMDType<IMDTypeBool>()->MDId();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarBooleanTest::Eber
//
//	@doc:
//		Perform boolean expression evaluation
//
//---------------------------------------------------------------------------
CScalar::EBoolEvalResult
CScalarBooleanTest::Eber(ULongPtrArray *pdrgpulChildren) const
{
	GPOS_ASSERT(NULL != pdrgpulChildren);
	GPOS_ASSERT(1 == pdrgpulChildren->Size());

	EBoolEvalResult eber = (EBoolEvalResult) * ((*pdrgpulChildren)[0]);
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(m_rgBoolEvalMap); ul++)
	{
		if (m_ebt == m_rgBoolEvalMap[ul][0] && eber == m_rgBoolEvalMap[ul][1])
		{
			return (CScalar::EBoolEvalResult) m_rgBoolEvalMap[ul][2];
		}
	}

	return EberAny;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarBooleanTest::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CScalarBooleanTest::OsPrint(IOstream &os) const
{
	os << SzId() << " (";
	os << m_rgwszBoolTest[m_ebt];
	os << ")";

	return os;
}



// EOF
