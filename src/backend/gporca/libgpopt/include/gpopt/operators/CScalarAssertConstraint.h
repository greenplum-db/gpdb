//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
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

	// private copy ctor
	CScalarAssertConstraint(const CScalarAssertConstraint &);

public:
	// ctor
	CScalarAssertConstraint(CMemoryPool *mp, CWStringBase *pstrErrorMsg);

	// dtor
	~CScalarAssertConstraint();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarAssertConstraint;
	}

	// operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarAssertConstraint";
	}

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL
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

	// type of expression's result
	virtual IMDId *MdidType() const;

	// error message
	CWStringBase *
	PstrErrorMsg() const
	{
		return m_pstrErrorMsg;
	}

	// print
	virtual IOstream &OsPrint(IOstream &os) const;

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
