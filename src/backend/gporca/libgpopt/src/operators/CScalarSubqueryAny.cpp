//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSubqueryAny.cpp
//
//	@doc:
//		Implementation of scalar subquery ANY operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarSubqueryAny.h"

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAny::CScalarSubqueryAny
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryAny::CScalarSubqueryAny(CMemoryPool *mp, CColRefSet *colrefs)
	: CScalarSubqueryQuantified(mp, colrefs)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAny::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CScalarSubqueryAny::PopCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist)
{
	CColRefSet *colrefset =
		CUtils::PcrsRemap(mp, Pcrs(), colref_mapping, must_exist);

	return GPOS_NEW(mp) CScalarSubqueryAny(mp, colrefset);
}

// EOF
