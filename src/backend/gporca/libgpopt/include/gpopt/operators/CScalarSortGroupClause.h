//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
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
class CScalarSortGroupClause : public CScalar
{
private:
	INT m_tle_sort_group_ref;
	INT m_eqop;
	INT m_sortop;
	BOOL m_nulls_first;
	BOOL m_hashable;

public:
	// private copy ctor
	CScalarSortGroupClause(const CScalarSortGroupClause &) = delete;

	// ctor
	CScalarSortGroupClause(CMemoryPool *mp, INT tle_sort_group_ref, INT eqop,
						   INT sortop, BOOL nulls_first, BOOL hashable);

	~CScalarSortGroupClause() override = default;

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
	EOperatorId
	Eopid() const override
	{
		return EopScalarSortGroupClause;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarSortGroupClause";
	}

	// match function
	BOOL Matches(COperator *op) const override;

	// conversion function
	static CScalarSortGroupClause *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopScalarSortGroupClause == pop->Eopid());

		return dynamic_cast<CScalarSortGroupClause *>(pop);
	}

	// the type of the scalar expression
	IMDId *
	MdidType() const override
	{
		return nullptr;
	}

	INT TypeModifier() const override;

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *) const override;

	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// print
	IOstream &OsPrint(IOstream &io) const override;

};	// class CScalarSortGroupClause

}  // namespace gpopt


#endif	// !GPOPT_CScalarSortGroupClause_H

// EOF
