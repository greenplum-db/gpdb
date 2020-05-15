//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementCTEProducer.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementCTEProducer.h"

#include "gpopt/operators/CLogicalCTEProducer.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalCTEProducer.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementCTEProducer::CXformImplementCTEProducer
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementCTEProducer::CXformImplementCTEProducer
	(
	CMemoryPool *mp
	)
	:
	CXformImplementation
		(
		 // pattern
		GPOS_NEW(mp) CExpression
				(
				mp,
				GPOS_NEW(mp) CLogicalCTEProducer(mp),
				GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp))
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementCTEProducer::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformImplementCTEProducer::Exfp
	(
	CExpressionHandle & // exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementCTEProducer::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementCTEProducer::Transform
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

	CLogicalCTEProducer *popCTEProducer = CLogicalCTEProducer::PopConvert(pexpr->Pop());
	CMemoryPool *mp = pxfctxt->Pmp();

	// extract components for alternative
	ULONG id = popCTEProducer->UlCTEId();

	CColRefArray *colref_array = popCTEProducer->Pdrgpcr();
	colref_array->AddRef();

	// child of CTEProducer operator
	CExpression *pexprChild = (*pexpr)[0];
	pexprChild->AddRef();

	// create physical CTE Producer
	CExpression *pexprAlt =
		GPOS_NEW(mp) CExpression
			(
			mp,
			GPOS_NEW(mp) CPhysicalCTEProducer(mp, id, colref_array),
			pexprChild
			);

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
