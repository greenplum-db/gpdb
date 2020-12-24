//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
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
public:
	CScalarAssertConstraintList(const CScalarAssertConstraintList &) = delete;

	// ctor
	CScalarAssertConstraintList(CMemoryPool *mp);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarAssertConstraintList;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarAssertConstraintList";
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
