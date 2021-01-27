//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarCaseTest.h
//
//	@doc:
//		Scalar case test operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarCaseTest_H
#define GPOPT_CScalarCaseTest_H

#include "gpos/base.h"

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarCaseTest
//
//	@doc:
//		Scalar case test operator
//
//---------------------------------------------------------------------------
class CScalarCaseTest : public CScalar
{
private:
	// type id
	IMDId *m_mdid_type;

	// private copy ctor
	CScalarCaseTest(const CScalarCaseTest &);

public:
	// ctor
	CScalarCaseTest(CMemoryPool *mp, IMDId *mdid_type);

	// dtor
	virtual ~CScalarCaseTest();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarCaseTest;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarCaseTest";
	}

	// the type of the scalar expression
	virtual IMDId *
	MdidType() const
	{
		return m_mdid_type;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL FInputOrderSensitive() const;

	// return a copy of the operator with remapped columns
	virtual COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}

	// conversion function
	static CScalarCaseTest *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarCaseTest == pop->Eopid());

		return dynamic_cast<CScalarCaseTest *>(pop);
	}

};	// class CScalarCaseTest

}  // namespace gpopt


#endif	// !GPOPT_CScalarCaseTest_H

// EOF
