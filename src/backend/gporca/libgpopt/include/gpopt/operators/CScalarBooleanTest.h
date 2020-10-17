//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarBooleanTest.h
//
//	@doc:
//		Scalar boolean test
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarBooleanTest_H
#define GPOPT_CScalarBooleanTest_H

#include "gpos/base.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarBooleanTest
//
//	@doc:
//		Scalar boolean test operator
//
//---------------------------------------------------------------------------
class CScalarBooleanTest : public CScalar
{
public:
	// different boolean test types
	enum EBoolTest
	{
		EbtIsTrue = 0,
		EbtIsNotTrue,
		EbtIsFalse,
		EbtIsNotFalse,
		EbtIsUnknown,
		EbtIsNotUnknown,

		EbtSentinel
	};

	static const WCHAR m_rgwszBoolTest[EbtSentinel][30];

	// mapping operator type and child value into the corresponding result value
	static const BYTE m_rgBoolEvalMap[][3];

private:
	// boolean test
	EBoolTest m_ebt;

	// private copy ctor
	CScalarBooleanTest(const CScalarBooleanTest &);

public:
	// ctor
	CScalarBooleanTest(CMemoryPool *mp, EBoolTest ebt) : CScalar(mp), m_ebt(ebt)
	{
		GPOS_ASSERT(0 <= ebt && EbtSentinel > ebt);
	}

	// dtor
	virtual ~CScalarBooleanTest()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarBooleanTest;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarBooleanTest";
	}

	// accessor of boolean test type
	EBoolTest
	Ebt() const
	{
		return m_ebt;
	}

	// match function
	BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const
	{
		return false;
	}

	// return a copy of the operator with remapped columns
	virtual COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}

	// the type of the scalar expression
	virtual IMDId *MdidType() const;

	// boolean expression evaluation
	virtual EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const;

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

	// conversion function
	static CScalarBooleanTest *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarBooleanTest == pop->Eopid());

		return dynamic_cast<CScalarBooleanTest *>(pop);
	}

};	// class CScalarBooleanTest

}  // namespace gpopt


#endif	// !GPOPT_CScalarBooleanTest_H

// EOF
