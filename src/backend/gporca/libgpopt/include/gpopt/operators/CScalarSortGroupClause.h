//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarSortGroupClause.h
//
//	@doc:
//		An operator class that wraps a sort group clause
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarSortGroupClause_H
#define GPOPT_CScalarSortGroupClause_H

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdProp.h"
#include "gpopt/operators/CScalar.h"

namespace gpopt
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CScalarSortGroupClause
//
//	@doc:
//		A wrapper operator for sort group clause
//
//---------------------------------------------------------------------------
class CScalarSortGroupClause : public CScalar,
							   DbgPrintMixin<CScalarSortGroupClause>
{
private:
	INT m_tle_sort_group_ref;
	INT m_eqop;
	INT m_sortop;
	BOOL m_nulls_first;
	BOOL m_hashable;

	// private copy ctor
	CScalarSortGroupClause(const CScalarSortGroupClause &);

public:
	// ctor
	CScalarSortGroupClause(CMemoryPool *mp, INT tle_sort_group_ref, INT eqop,
						   INT sortop, BOOL nulls_first, BOOL hashable);

	virtual ~CScalarSortGroupClause()
	{
	}

	INT
	Index() const
	{
		return m_tle_sort_group_ref;
	}
	INT
	EqOp() const
	{
		return m_eqop;
	}
	INT
	SortOp() const
	{
		return m_sortop;
	}
	BOOL
	NullsFirst() const
	{
		return m_nulls_first;
	}
	BOOL
	IsHashable() const
	{
		return m_hashable;
	}

	// identity accessor
	virtual EOperatorId
	Eopid() const
	{
		return EopScalarSortGroupClause;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarSortGroupClause";
	}

	// match function
	virtual BOOL Matches(COperator *op) const;

	// conversion function
	static CScalarSortGroupClause *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarSortGroupClause == pop->Eopid());

		return dynamic_cast<CScalarSortGroupClause *>(pop);
	}

	// the type of the scalar expression
	virtual IMDId *
	MdidType() const
	{
		return NULL;
	}

	virtual INT TypeModifier() const;

	// boolean expression evaluation
	virtual EBoolEvalResult Eber(ULongPtrArray *) const;

	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
	)
	{
		return PopCopyDefault();
	}

	virtual BOOL
	FInputOrderSensitive() const
	{
		return false;
	}

	// print
	virtual IOstream &OsPrint(IOstream &io) const;

};	// class CScalarSortGroupClause

}  // namespace gpopt


#endif	// !GPOPT_CScalarSortGroupClause_H

// EOF
