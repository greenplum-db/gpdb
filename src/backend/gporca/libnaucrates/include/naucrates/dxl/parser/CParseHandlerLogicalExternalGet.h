//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
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
	CParseHandlerLogicalExternalGet(const CParseHandlerLogicalExternalGet &) =
		delete;

	// ctor
	CParseHandlerLogicalExternalGet(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalExternalGet_H

// EOF
