//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalCTEAnchor.h
//
//	@doc:
//		Parse handler for parsing a logical CTE anchor operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalCTEAnchor_H
#define GPDXL_CParseHandlerLogicalCTEAnchor_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalCTEAnchor
//
//	@doc:
//		Parse handler for parsing a logical CTE anchor operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalCTEAnchor : public CParseHandlerLogicalOp
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
	CParseHandlerLogicalCTEAnchor(const CParseHandlerLogicalCTEAnchor &) =
		delete;

	// ctor
	CParseHandlerLogicalCTEAnchor(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalCTEAnchor_H

// EOF
