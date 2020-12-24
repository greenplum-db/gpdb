//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerPhysicalDynamicBitmapTableScan.h
//
//	@doc:
//		SAX parse handler class for parsing dynamic bitmap table scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPhysicalDynamicBitmapTableScan_H
#define GPDXL_CParseHandlerPhysicalDynamicBitmapTableScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalAbstractBitmapScan.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalDynamicBitmapTableScan
//
//	@doc:
//		Parse handler for parsing dynamic bitmap table scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalDynamicBitmapTableScan
	: public CParseHandlerPhysicalAbstractBitmapScan
{
private:
	// part index id
	ULONG m_part_index_id;

	// printable partition index id
	ULONG m_part_index_id_printable;

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
	CParseHandlerPhysicalDynamicBitmapTableScan(
		const CParseHandlerPhysicalDynamicBitmapTableScan &) = delete;

	// ctor
	CParseHandlerPhysicalDynamicBitmapTableScan(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root)
		: CParseHandlerPhysicalAbstractBitmapScan(mp, parse_handler_mgr,
												  parse_handler_root),
		  m_part_index_id(0),
		  m_part_index_id_printable(0)
	{
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPhysicalDynamicBitmapTableScan_H

// EOF
