//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarNAryJoinPredList.h
//
//	@doc:
//		A list of join predicates for an NAry join that contains join
//		types other than inner joins
//		(for now we only handle inner joins + LOJs)
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarNAryJoinPredList_H
#define GPOPT_CScalarNAryJoinPredList_H

#include "gpos/base.h"
#include "gpopt/operators/CScalar.h"
#include "gpopt/base/CDrvdProp.h"

// child number of CScalarNAryJoinPredList expression that contains inner join predicates, must be zero
#define GPOPT_ZERO_INNER_JOIN_PRED_INDEX 0

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	class CScalarNAryJoinPredList
//---------------------------------------------------------------------------
class CScalarNAryJoinPredList : public CScalar
{
private:
public:
	CScalarNAryJoinPredList(const CScalarNAryJoinPredList &) = delete;

	// ctor
	explicit CScalarNAryJoinPredList(CMemoryPool *mp);

	// dtor
	~CScalarNAryJoinPredList() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopScalarNAryJoinPredList;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarNAryJoinPredList";
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// conversion function
	static CScalarNAryJoinPredList *
	PopConvert(COperator *pop)
	{
		return dynamic_cast<CScalarNAryJoinPredList *>(pop);
	}

	IMDId *
	MdidType() const override
	{
		GPOS_ASSERT(
			!"Invalid function call: CScalarNAryJoinPredList::MdidType()");
		return NULL;
	}

};	// class CScalarNAryJoinPredList

}  // namespace gpopt

#endif	// !GPOPT_CScalarNAryJoinPredList_H

// EOF
