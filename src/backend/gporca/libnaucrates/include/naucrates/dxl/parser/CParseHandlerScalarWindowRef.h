//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarWindowRef.h
//
//	@doc:
//
//		SAX parse handler class for parsing scalar WindowRef
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarWindowRef_H
#define GPDXL_CParseHandlerScalarWindowRef_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarWindowRef.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarWindowRef
//
//	@doc:
//		Parse handler for parsing a scalar WindowRef
//
//---------------------------------------------------------------------------
class CParseHandlerScalarWindowRef : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarWindowRef(const CParseHandlerScalarWindowRef &);

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
	// ctor
	CParseHandlerScalarWindowRef(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// !GPDXL_CParseHandlerScalarWindowRef_H

//EOF
