//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerDynamicTableScan.h
//
//	@doc:
//		SAX parse handler class for parsing dynamic table scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerDynamicTableScan_H
#define GPDXL_CParseHandlerDynamicTableScan_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// fwd decl
class CDXLPhysicalDynamicTableScan;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerDynamicTableScan
//
//	@doc:
//		Parse handler for parsing a dynamic table scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerDynamicTableScan : public CParseHandlerPhysicalOp
{
private:
	ULongPtrArray *m_selector_ids;

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
	CParseHandlerDynamicTableScan(const CParseHandlerDynamicTableScan &) =
		delete;

	// ctor
	CParseHandlerDynamicTableScan(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *pph);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDynamicTableScan_H

// EOF
