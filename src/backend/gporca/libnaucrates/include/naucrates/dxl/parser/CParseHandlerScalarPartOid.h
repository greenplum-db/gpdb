//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarPartOid.h
//
//	@doc:
//		SAX parse handler class for parsing scalar part oid
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarScalarPartOid_H
#define GPDXL_CParseHandlerScalarScalarPartOid_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarPartOid
//
//	@doc:
//		Parse handler class for parsing scalar part oid
//
//---------------------------------------------------------------------------
class CParseHandlerScalarPartOid : public CParseHandlerScalarOp
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
	CParseHandlerScalarPartOid(const CParseHandlerScalarPartOid &) = delete;

	// ctor
	CParseHandlerScalarPartOid(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarScalarPartOid_H

// EOF
