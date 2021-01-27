//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CParseHandlerScalarSubqueryQuantified.h
//
//	@doc:
//		SAX parse handler class for parsing scalar subquery ALL and ANY operator node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubqueryAny_H
#define GPDXL_CParseHandlerScalarSubqueryAny_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarSubqueryAny.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubqueryQuantified
//
//	@doc:
//		Parse handler for parsing scalar subquery ALL and ANY operators
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubqueryQuantified : public CParseHandlerScalarOp
{
private:
	// scalar subquery operator
	CDXLScalar *m_dxl_op;

	// private copy ctor
	CParseHandlerScalarSubqueryQuantified(
		const CParseHandlerScalarSubqueryQuantified &);

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
	CParseHandlerScalarSubqueryQuantified(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarSubqueryAny_H

// EOF
