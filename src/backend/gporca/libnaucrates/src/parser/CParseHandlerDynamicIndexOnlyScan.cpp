//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CParseHandlerDynamicIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for dynamic index only scan
//		operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerDynamicIndexOnlyScan.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDynamicIndexOnlyScan::CParseHandlerDynamicIndexOnlyScan
//
//---------------------------------------------------------------------------
CParseHandlerDynamicIndexOnlyScan::CParseHandlerDynamicIndexOnlyScan(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerIndexOnlyScan(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDynamicIndexOnlyScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerDynamicIndexOnlyScan::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &attrs)
{
	StartElementHelper(element_local_name, attrs,
					   EdxltokenPhysicalDynamicIndexOnlyScan);

	m_selector_ids = CDXLOperatorFactory::ExtractConvertValuesToArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenSelectorIds,
		EdxltokenPhysicalDynamicIndexOnlyScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDynamicIndexOnlyScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerDynamicIndexOnlyScan::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	EndElementHelper(element_local_name, EdxltokenPhysicalDynamicIndexOnlyScan,
					 m_selector_ids /*m_selector_ids*/);
}


// EOF
