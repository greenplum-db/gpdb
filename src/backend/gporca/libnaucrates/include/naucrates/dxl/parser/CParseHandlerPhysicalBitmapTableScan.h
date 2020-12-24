//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerPhysicalBitmapTableScan.h
//
//	@doc:
//		SAX parse handler class for parsing dynamic bitmap table scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPhysicalBitmapTableScan_H
#define GPDXL_CParseHandlerPhysicalBitmapTableScan_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalAbstractBitmapScan.h"

namespace gpdxl
{
using namespace gpos;

// forward declarations
class CDXLPhysicalBitmapTableScan;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalBitmapTableScan
//
//	@doc:
//		Parse handler for parsing bitmap table scan operator
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalBitmapTableScan
	: public CParseHandlerPhysicalAbstractBitmapScan
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
	CParseHandlerPhysicalBitmapTableScan(
		const CParseHandlerPhysicalBitmapTableScan &) = delete;

	// ctor
	CParseHandlerPhysicalBitmapTableScan(
		CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
		CParseHandlerBase *parse_handler_root)
		: CParseHandlerPhysicalAbstractBitmapScan(mp, parse_handler_mgr,
												  parse_handler_root)
	{
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPhysicalBitmapTableScan_H

// EOF
