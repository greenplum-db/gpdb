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
	// the id of the partition index structure
	ULONG m_part_index_id;

	// printable partition index id
	ULONG m_part_index_id_printable;

	// private copy ctor
	CParseHandlerDynamicTableScan(const CParseHandlerDynamicTableScan &);

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
	CParseHandlerDynamicTableScan(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *pph);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerDynamicTableScan_H

// EOF
