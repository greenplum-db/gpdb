//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CParseHandlerExternalScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing external scan
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExternalScan.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExternalScan::CParseHandlerExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerExternalScan::CParseHandlerExternalScan(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerTableScan(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExternalScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExternalScan::StartElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const,	 // element_qname
										const Attributes &	 // attrs
)
{
	CParseHandlerTableScan::StartElement(element_local_name,
										 EdxltokenPhysicalExternalScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExternalScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExternalScan::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	CParseHandlerTableScan::EndElement(element_local_name,
									   EdxltokenPhysicalExternalScan);
}

// EOF
