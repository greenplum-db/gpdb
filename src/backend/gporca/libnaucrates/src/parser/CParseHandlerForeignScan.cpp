//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerForeignScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing foreign scan
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerForeignScan.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerForeignScan::CParseHandlerForeignScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerForeignScan::CParseHandlerForeignScan(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerTableScan(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerForeignScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerForeignScan::StartElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const,	// element_qname
									   const Attributes &	// attrs
)
{
	CParseHandlerTableScan::StartElement(element_local_name,
										 EdxltokenPhysicalForeignScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerForeignScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerForeignScan::EndElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const	 // element_qname
)
{
	CParseHandlerTableScan::EndElement(element_local_name,
									   EdxltokenPhysicalForeignScan);
}

// EOF
