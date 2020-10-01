//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerProjElem.h
//
//	@doc:
//		SAX parse handler class for parsing projection elements.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerProjElem_H
#define GPDXL_CParseHandlerProjElem_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarProjElem.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerProjElem
//
//	@doc:
//		Parse handler for projection elements
//
//---------------------------------------------------------------------------
class CParseHandlerProjElem : public CParseHandlerScalarOp
{
private:
	// project elem operator
	CDXLScalarProjElem *m_dxl_op;

	// private copy ctor
	CParseHandlerProjElem(const CParseHandlerProjElem &);

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
	CParseHandlerProjElem(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerProjElem_H

// EOF
