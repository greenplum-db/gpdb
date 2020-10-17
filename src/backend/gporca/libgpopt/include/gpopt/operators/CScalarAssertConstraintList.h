//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CScalarAssertConstraintList.h
//
//	@doc:
//		Class for scalar assert constraint list representing the predicate
//		of Assert operators. For example:
//
//         +--CScalarAssertConstraintList
//            |--CScalarAssertConstraint (ErrorMsg: Check constraint r_check for table r violated)
//            |  +--CScalarIsDistinctFrom (=)
//            |     |--CScalarCmp (<)
//            |     |  |--CScalarIdent "d" (4)
//            |     |  +--CScalarIdent "c" (3)
//            |     +--CScalarConst (0)
//            +--CScalarAssertConstraint (ErrorMsg: Check constraint r_c_check for table r violated)
//               +--CScalarIsDistinctFrom (=)
//                  |--CScalarCmp (>)
//                  |  |--CScalarIdent "c" (3)
//                  |  +--CScalarConst (0)
//                  +--CScalarConst (0)
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarAssertConstraintList_H
#define GPOPT_CScalarAssertConstraintList_H

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
//		CScalarAssertConstraintList
//
//	@doc:
//		Scalar assert constraint list
//
//---------------------------------------------------------------------------
class CScalarAssertConstraintList : public CScalar
{
private:
	// private copy ctor
	CScalarAssertConstraintList(const CScalarAssertConstraintList &);

public:
	// ctor
	CScalarAssertConstraintList(CMemoryPool *mp);

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarAssertConstraintList;
	}

	// operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarAssertConstraintList";
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

	// conversion function
	static CScalarAssertConstraintList *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarAssertConstraintList == pop->Eopid());

		return dynamic_cast<CScalarAssertConstraintList *>(pop);
	}

};	// class CScalarAssertConstraintList
}  // namespace gpopt

#endif	// !GPOPT_CScalarAssertConstraintList_H

// EOF
