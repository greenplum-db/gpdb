//
// Created by Sanath Kumar Vobilisetty on 7/19/22.
//

#ifndef GPOPT_MASTER_CXFORMINPLACEUPDATE2DML_H
#define GPOPT_MASTER_CXFORMINPLACEUPDATE2DML_H
#include "gpos/base.h"

#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformUpdate2DML
//
//	@doc:
//		Transform Logical Update to Logical DML
//
//---------------------------------------------------------------------------
class CXformInPlaceUpdate2DML : public CXformExploration
{
private:
public:
	CXformInPlaceUpdate2DML(const CXformInPlaceUpdate2DML &) = delete;

	// ctor
	explicit CXformInPlaceUpdate2DML(CMemoryPool *mp);

	// dtor
	~CXformInPlaceUpdate2DML() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfInPlaceUpdate2DML;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformInPlaceUpdate2DML";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformUpdate2DML
}  // namespace gpopt
#endif	//GPOPT_MASTER_CXFORMINPLACEUPDATE2DML_H
