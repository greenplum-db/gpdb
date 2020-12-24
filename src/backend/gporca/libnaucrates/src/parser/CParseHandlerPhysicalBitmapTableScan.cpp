//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerPhysicalBitmapTableScan.cpp
//
//	@doc:
//		SAX parse handler class for parsing bitmap table scan operator nodes
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalBitmapTableScan.h"

#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalBitmapTableScan::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalBitmapTableScan::StartElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &	 // attrs
)
{
	StartElementHelper(element_local_name, EdxltokenPhysicalBitmapTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalBitmapTableScan::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalBitmapTableScan::EndElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	EndElementHelper(element_local_name, EdxltokenPhysicalBitmapTableScan);
}
