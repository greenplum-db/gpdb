//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerHashExpr.h
//
//	@doc:
//		SAX parse handler class for parsing a hash expressions
//		in the hash expr list of a redistribute motion node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerHashExpr_H
#define GPDXL_CParseHandlerHashExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalarHashExpr.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerHashExpr
//
//	@doc:
//		SAX parse handler class for parsing the list of hash expressions
//		in the hash expr list of a redistribute motion node.
//
//---------------------------------------------------------------------------
class CParseHandlerHashExpr : public CParseHandlerScalarOp
{
private:
	// hash expr operator
	CDXLScalarHashExpr *m_dxl_op;

	// private copy ctor
	CParseHandlerHashExpr(const CParseHandlerHashExpr &);

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
	CParseHandlerHashExpr(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerHashExpr_H

// EOF
