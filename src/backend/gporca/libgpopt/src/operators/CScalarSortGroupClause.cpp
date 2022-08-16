//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSortGroupClause.cpp
//
//	@doc:
//		Implementation of scalar constant operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarSortGroupClause.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "naucrates/base/IDatumBool.h"

using namespace gpopt;
using namespace gpnaucrates;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSortGroupClause::CScalarSortGroupClause
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSortGroupClause::CScalarSortGroupClause(CMemoryPool *mp,
											   INT tle_sort_group_ref, INT eqop,
											   INT sortop, BOOL nulls_first,
											   BOOL hashable)
	: CScalar(mp),
	  m_tle_sort_group_ref(tle_sort_group_ref),
	  m_eqop(eqop),
	  m_sortop(sortop),
	  m_nulls_first(nulls_first),
	  m_hashable(hashable)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSortGroupClause::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSortGroupClause::Matches(COperator *other) const
{
	if (Eopid() != other->Eopid())
	{
		return false;
	}

	CScalarSortGroupClause *op = CScalarSortGroupClause::PopConvert(other);

	return Index() == op->Index() && EqOp() == op->EqOp() &&
		   SortOp() == op->SortOp() && NullsFirst() == op->NullsFirst() &&
		   IsHashable() == op->IsHashable();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSortGroupClause::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CScalarSortGroupClause::OsPrint(IOstream &os) const
{
	os << SzId();
	os << "(tleSortGroupRef:" << m_tle_sort_group_ref << ",";
	os << "eqop:" << m_eqop << ",";
	os << "sortop:" << m_sortop << ",";
	os << "nulls_first:" << (m_nulls_first ? "true" : "false") << ",";
	os << "hashable:" << (m_hashable ? "true" : "false") << ")";
	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSortGroupClause::Eber
//
//	@doc:
//		Perform boolean expression evaluation
//
//---------------------------------------------------------------------------
CScalar::EBoolEvalResult
CScalarSortGroupClause::Eber(ULongPtrArray *  //pdrgpulChildren
) const
{
	return EberAny;
}

INT
CScalarSortGroupClause::TypeModifier() const
{
	return 0;
}


// EOF
