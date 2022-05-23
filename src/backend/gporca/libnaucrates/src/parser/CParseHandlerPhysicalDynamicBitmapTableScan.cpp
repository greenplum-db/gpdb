//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerPhysicalDynamicBitmapTableScan.cpp
//
//	@doc:
//		SAX parse handler class for parsing dynamic bitmap table scan operator nodes
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalDynamicBitmapTableScan.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalDynamicBitmapTableScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalDynamicBitmapTableScan::StartElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &attrs)
{
	StartElementHelper(element_local_name,
					   EdxltokenPhysicalDynamicBitmapTableScan);

	m_selector_ids = CDXLOperatorFactory::ExtractConvertValuesToArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenSelectorIds,
		EdxltokenPhysicalDynamicTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalDynamicBitmapTableScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalDynamicBitmapTableScan::EndElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	EndElementHelper(element_local_name,
					 EdxltokenPhysicalDynamicBitmapTableScan, m_selector_ids);
}

// EOF
