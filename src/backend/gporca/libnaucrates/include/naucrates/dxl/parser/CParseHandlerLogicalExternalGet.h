//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerLogicalExternalGet.h
//
//	@doc:
//		SAX parse handler class for parsing logical external get operator node
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerLogicalExternalGet_H
#define GPDXL_CParseHandlerLogicalExternalGet_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerLogicalGet.h"

namespace gpdxl
{
using namespace gpos;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalExternalGet
//
//	@doc:
//		Parse handler for parsing a logical external get operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalExternalGet : public CParseHandlerLogicalGet
{
private:
	// private copy ctor
	CParseHandlerLogicalExternalGet(const CParseHandlerLogicalExternalGet &);

	// process the start of an element
	virtual void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	virtual void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerLogicalExternalGet(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalExternalGet_H

// EOF
