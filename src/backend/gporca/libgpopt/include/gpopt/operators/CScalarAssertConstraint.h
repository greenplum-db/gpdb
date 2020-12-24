//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarAssertConstraint.h
//
//	@doc:
//		Class for representing a single scalar assert constraints in physical
//	    assert operators. Each node contains a constraint to be checked at
//		runtime, and an error message to print in case the constraint is violated.
//
//		For example:
//            +--CScalarAssertConstraint (ErrorMsg: Check constraint r_c_check for table r violated)
//               +--CScalarIsDistinctFrom (=)
//                  |--CScalarCmp (>)
//                  |  |--CScalarIdent "c" (3)
//                  |  +--CScalarConst (0)
//                  +--CScalarConst (0)
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarAssertConstraint_H
#define GPOPT_CScalarAssertConstraint_H

#include "gpos/base.h"
#include "naucrates/md/IMDId.h"

#include "gpopt/operators/CScalar.h"
#include "gpopt/base/COptCtxt.h"

namespace gpopt
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CScalarAssertConstraint
//
//	@doc:
//		Scalar assert constraint
//
//---------------------------------------------------------------------------
class CScalarAssertConstraint : public CScalar
{
private:
	// error message
	CWStringBase *m_pstrErrorMsg;

public:
	CScalarAssertConstraint(const CScalarAssertConstraint &) = delete;

	// ctor
	CScalarAssertConstraint(CMemoryPool *mp, CWStringBase *pstrErrorMsg);

	// dtor
	~CScalarAssertConstraint() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarAssertConstraint;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarAssertConstraint";
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// type of expression's result
	IMDId *MdidType() const override;

	// error message
	CWStringBase *
	PstrErrorMsg() const
	{
		return m_pstrErrorMsg;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CScalarAssertConstraint *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarAssertConstraint == pop->Eopid());

		return dynamic_cast<CScalarAssertConstraint *>(pop);
	}

};	// class CScalarAssertConstraint
}  // namespace gpopt

#endif	// !GPOPT_CScalarAssertConstraint_H

// EOF
