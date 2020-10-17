//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for the index only scan operator
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerIndexOnlyScan.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexOnlyScan::CParseHandlerIndexOnlyScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerIndexOnlyScan::CParseHandlerIndexOnlyScan(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerIndexScan(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexOnlyScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexOnlyScan::StartElement(const XMLCh *const,  // element_uri,
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname
										 const Attributes &attrs)
{
	StartElementHelper(element_local_name, attrs,
					   EdxltokenPhysicalIndexOnlyScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerIndexOnlyScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerIndexOnlyScan::EndElement(const XMLCh *const,	// element_uri,
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	EndElementHelper(element_local_name, EdxltokenPhysicalIndexOnlyScan);
}

// EOF
