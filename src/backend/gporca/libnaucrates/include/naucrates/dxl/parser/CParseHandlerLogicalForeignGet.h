//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerLogicalForeignGet.h
//
//	@doc:
//		SAX parse handler class for parsing logical foreign get operator node
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerLogicalForeignGet_H
#define GPDXL_CParseHandlerLogicalForeignGet_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerLogicalGet.h"

namespace gpdxl
{
using namespace gpos;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalForeignGet
//
//	@doc:
//		Parse handler for parsing a logical foreign get operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalForeignGet : public CParseHandlerLogicalGet
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
	CParseHandlerLogicalForeignGet(const CParseHandlerLogicalForeignGet &) =
		delete;

	// ctor
	CParseHandlerLogicalForeignGet(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalForeignGet_H

// EOF
