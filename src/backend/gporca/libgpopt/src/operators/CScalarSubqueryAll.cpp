//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSubqueryAll.cpp
//
//	@doc:
//		Implementation of scalar subquery ALL operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarSubqueryAll.h"

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAll::CScalarSubqueryAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryAll::CScalarSubqueryAll(CMemoryPool *mp, CColRefSet *colrefset)
	: CScalarSubqueryQuantified(mp, colrefset)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAll::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CScalarSubqueryAll::PopCopyWithRemappedColumns(CMemoryPool *mp,
											   UlongToColRefMap *colref_mapping,
											   BOOL must_exist)
{
	CColRefSet *colrefset =
		CUtils::PcrsRemap(mp, Pcrs(), colref_mapping, must_exist);

	return GPOS_NEW(mp) CScalarSubqueryAll(mp, colrefset);
}

// EOF
