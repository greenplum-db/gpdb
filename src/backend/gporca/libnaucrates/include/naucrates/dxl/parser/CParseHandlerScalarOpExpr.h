//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarOpExpr.h
//
//	@doc:
//
//		SAX parse handler class for parsing scalar OpExpr.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarOpExpr_H
#define GPDXL_CParseHandlerScalarOpExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarOpExpr.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarOpExpr
//
//	@doc:
//		Parse handler for parsing a scalar op expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarOpExpr : public CParseHandlerScalarOp
{
private:
	ULONG m_num_of_children;
	// private copy ctor
	CParseHandlerScalarOpExpr(const CParseHandlerScalarOpExpr &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor/dtor
	CParseHandlerScalarOpExpr(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarOpExpr_H

//EOF
