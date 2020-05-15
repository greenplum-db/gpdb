//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformImplementLimit.cpp
//
//	@doc:
//		Implementation of limit operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementLimit.h"

#include "gpopt/operators/CLogicalLimit.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalLimit.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementLimit::CXformImplementLimit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementLimit::CXformImplementLimit
	(
	CMemoryPool *mp
	)
	:
	// pattern
	CXformImplementation
		(
		GPOS_NEW(mp) CExpression
						(
						mp, 
						GPOS_NEW(mp) CLogicalLimit(mp),
						GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)), // relational child
						GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),  // scalar child for offset
						GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))  // scalar child for number of rows

						)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementLimit::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementLimit::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	CMemoryPool *mp = pxfctxt->Pmp();
		
	// extract components
	CLogicalLimit *popLimit = CLogicalLimit::PopConvert(pexpr->Pop());
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalarStart = (*pexpr)[1];
	CExpression *pexprScalarRows = (*pexpr)[2];
	COrderSpec *pos = popLimit->Pos();
	
	// addref all components
	pexprRelational->AddRef();
	pexprScalarStart->AddRef();
	pexprScalarRows->AddRef();
	popLimit->Pos()->AddRef();
	
	// assemble physical operator
	CExpression *pexprLimit = 
		GPOS_NEW(mp) CExpression
					(
					mp, 
					GPOS_NEW(mp) CPhysicalLimit
						(
						mp,
						pos,
						popLimit->FGlobal(),
						popLimit->FHasCount(),
						popLimit->IsTopLimitUnderDMLorCTAS()
						),
					pexprRelational,
					pexprScalarStart,
					pexprScalarRows
					);
	
	// add alternative to results
	pxfres->Add(pexprLimit);
}
	

// EOF

