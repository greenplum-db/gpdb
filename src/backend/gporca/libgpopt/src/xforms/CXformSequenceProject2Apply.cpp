//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSequenceProject2Apply.cpp
//
//	@doc:
//		Implementation of Sequence Project to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalSequenceProject.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/xforms/CXformSequenceProject2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformSequenceProject2Apply::CXformSequenceProject2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSequenceProject2Apply::CXformSequenceProject2Apply
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
				GPOS_NEW(mp) CLogicalSequenceProject(mp),
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),	// relational child
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))	// project list
				)
		)
{}

// EOF

