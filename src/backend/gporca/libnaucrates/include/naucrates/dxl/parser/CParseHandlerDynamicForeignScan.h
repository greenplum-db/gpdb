//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerDynamicForeignScan.h
//
//	@doc:
//		parse handler class for parsing dynamic foreign scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerDynamicForeignScan_H
#define GPDXL_CParseHandlerDynamicForeignScan_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// fwd decl
class CDXLPhysicalDynamicForeignScan;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerDynamicForeignScan
//
//	@doc:
//		Parse handler for parsing a dynamic foreign scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerDynamicForeignScan : public CParseHandlerPhysicalOp
{
private:
	ULongPtrArray *m_selector_ids;

	OID m_foreign_server_oid;

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
	CParseHandlerDynamicForeignScan(const CParseHandlerDynamicForeignScan &) =
		delete;

	// ctor
	CParseHandlerDynamicForeignScan(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *pph);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDynamicForeignScan_H

// EOF
