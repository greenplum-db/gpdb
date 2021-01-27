//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal Inc.
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

#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

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
	// private copy ctor
	CScalarNAryJoinPredList(const CScalarNAryJoinPredList &);

public:
	// ctor
	explicit CScalarNAryJoinPredList(CMemoryPool *mp);

	// dtor
	virtual ~CScalarNAryJoinPredList()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarNAryJoinPredList;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarNAryJoinPredList";
	}

	// match function
	BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const;

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
	static CScalarNAryJoinPredList *
	PopConvert(COperator *pop)
	{
		return dynamic_cast<CScalarNAryJoinPredList *>(pop);
	}

	virtual IMDId *
	MdidType() const
	{
		GPOS_ASSERT(
			!"Invalid function call: CScalarNAryJoinPredList::MdidType()");
		return NULL;
	}

};	// class CScalarNAryJoinPredList

}  // namespace gpopt

#endif	// !GPOPT_CScalarNAryJoinPredList_H

// EOF
