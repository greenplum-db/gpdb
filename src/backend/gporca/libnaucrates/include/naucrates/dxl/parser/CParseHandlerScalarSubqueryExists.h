//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CParseHandlerScalarSubqueryExists.h
//
//	@doc:
//		SAX parse handler class for parsing EXISTS and NOT EXISTS subquery operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubqueryExists_H
#define GPDXL_CParseHandlerScalarSubqueryExists_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarSubqueryExists.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubqueryExists
//
//	@doc:
//		Parse handler for parsing EXISTS and NOT EXISTS subquery operators
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubqueryExists : public CParseHandlerScalarOp
{
private:
	// scalar subquery operator
	CDXLScalar *m_dxl_op;

	// private copy ctor
	CParseHandlerScalarSubqueryExists(
		const CParseHandlerScalarSubqueryExists &);

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
	CParseHandlerScalarSubqueryExists(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSubqueryExists_H

// EOF
