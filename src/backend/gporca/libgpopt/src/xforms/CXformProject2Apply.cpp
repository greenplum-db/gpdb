//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformProject2Apply.cpp
//
//	@doc:
//		Implementation of Project to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalProject.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformProject2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformProject2Apply::CXformProject2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformProject2Apply::CXformProject2Apply
	(
	CMemoryPool *mp
	)
	:
	// pattern
	CXformSubqueryUnnest
		(
		GPOS_NEW(mp) CExpression
				(
				mp,
				GPOS_NEW(mp) CLogicalProject(mp),
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),	// relational child
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))	// scalar project list
				)
		)
{}


// EOF

