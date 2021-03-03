//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarExpr.h
//
//	@doc:
//		SAX parse handler class for parsing top level scalar expressions.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarExpr_H
#define GPDXL_CParseHandlerScalarExpr_H

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarExpr
//
//	@doc:
//		Parse handler for parsing a top level scalar expression.
//
//---------------------------------------------------------------------------
class CParseHandlerScalarExpr : public CParseHandlerBase
{
private:
	// the root of the parsed DXL tree constructed by the parse handler
	CDXLNode *m_dxl_node;

protected:
	// returns the parse handler type
	EDxlParseHandlerType GetParseHandlerType() const override;

	// process notification of the beginning of an element.
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process notification of the end of an element.
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerScalarExpr(const CParseHandlerScalarExpr &) = delete;

	// ctor
	CParseHandlerScalarExpr(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerScalarExpr() override;

	// root of constructed DXL expression
	CDXLNode *CreateDXLNode() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarExpr_H

// EOF
