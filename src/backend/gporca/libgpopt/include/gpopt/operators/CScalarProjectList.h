//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CScalarProjectList.h
//
//	@doc:
//		Projection list
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarProjectList_H
#define GPOPT_CScalarProjectList_H

#include "gpos/base.h"
#include "gpopt/operators/CScalar.h"
#include "gpopt/base/CDrvdProp.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarProjectList
//
//	@doc:
//		Projection list operator
//
//---------------------------------------------------------------------------
class CScalarProjectList : public CScalar
{
private:
	// private copy ctor
	CScalarProjectList(const CScalarProjectList &);

public:
	// ctor
	explicit CScalarProjectList(CMemoryPool *mp);

	// dtor
	virtual ~CScalarProjectList()
	{
	}

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarProjectList;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarProjectList";
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
	static CScalarProjectList *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarProjectList == pop->Eopid());

		return reinterpret_cast<CScalarProjectList *>(pop);
	}

	virtual IMDId *
	MdidType() const
	{
		GPOS_ASSERT(!"Invalid function call: CScalarProjectList::MdidType()");
		return NULL;
	}

	// return number of distinct aggs in project list attached to given handle
	static ULONG UlDistinctAggs(CExpressionHandle &exprhdl);

	// check if a project list has multiple distinct aggregates
	static BOOL FHasMultipleDistinctAggs(CExpressionHandle &exprhdl);

};	// class CScalarProjectList

}  // namespace gpopt


#endif	// !GPOPT_CScalarProjectList_H

// EOF
