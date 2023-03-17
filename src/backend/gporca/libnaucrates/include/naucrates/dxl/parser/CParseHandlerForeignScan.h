//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerForeignScan.h
//
//	@doc:
//		SAX parse handler class for parsing foreign scan operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerForeignScan_H
#define GPDXL_CParseHandlerForeignScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalForeignScan.h"
#include "naucrates/dxl/parser/CParseHandlerTableScan.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerForeignScan
//
//	@doc:
//		Parse handler for parsing foreign scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerForeignScan : public CParseHandlerTableScan
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
	CParseHandlerForeignScan(const CParseHandlerForeignScan &) = delete;

	// ctor
	CParseHandlerForeignScan(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerForeignScan_H

// EOF
