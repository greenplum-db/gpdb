//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CParseHandlerScalarSubquery.h
//
//	@doc:
//		SAX parse handler class for parsing a scalar subquery operator node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubquery_H
#define GPDXL_CParseHandlerScalarSubquery_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarSubquery.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubquery
//
//	@doc:
//		Parse handler for parsing a scalar subquery operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubquery : public CParseHandlerScalarOp
{
private:
	// scalar subquery operator
	CDXLScalarSubquery *m_dxl_op;

	// private copy ctor
	CParseHandlerScalarSubquery(const CParseHandlerScalarSubquery &);

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
	CParseHandlerScalarSubquery(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSubquery_H

// EOF
