//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarSubPlanTestExpr.h
//
//	@doc:
//		SAX parse handler class for parsing sub plan test expression
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubPlanTestExpr_H
#define GPDXL_CParseHandlerScalarSubPlanTestExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubPlanTestExpr
//
//	@doc:
//		Parse handler for parsing subplan test expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubPlanTestExpr : public CParseHandlerScalarOp
{
private:
	// child test expression
	CDXLNode *m_dxl_test_expr;

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerScalarSubPlanTestExpr(
		const CParseHandlerScalarSubPlanTestExpr &) = delete;

	// ctor/dtor
	CParseHandlerScalarSubPlanTestExpr(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_root);

	~CParseHandlerScalarSubPlanTestExpr() override;

	// return test expression
	CDXLNode *
	GetDXLTestExpr() const
	{
		return m_dxl_test_expr;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSubPlanTestExpr_H

// EOF
