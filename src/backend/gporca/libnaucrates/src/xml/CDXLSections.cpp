//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDXLSections.cpp
//
//	@doc:
//		DXL section header and footer definitions
//
//---------------------------------------------------------------------------


#include "naucrates/dxl/xml/CDXLSections.h"

using namespace gpdxl;

// static member definitions

const WCHAR *CDXLSections::m_wszDocumentHeader = GPOS_WSZ_LIT(
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?><dxl:DXLMessage xmlns:dxl=\"http://greenplum.com/dxl/2010/12/\">");

const WCHAR *CDXLSections::m_wszDocumentFooter =
	GPOS_WSZ_LIT("</dxl:DXLMessage>");

const WCHAR *CDXLSections::m_wszThreadHeaderTemplate =
	GPOS_WSZ_LIT("<dxl:Thread Id=\"%d\">");

const WCHAR *CDXLSections::m_wszThreadFooter = GPOS_WSZ_LIT("</dxl:Thread>");

const WCHAR *CDXLSections::m_wszOptimizerConfigHeader =
	GPOS_WSZ_LIT("<dxl:OptimizerConfig>");

const WCHAR *CDXLSections::m_wszOptimizerConfigFooter =
	GPOS_WSZ_LIT("</dxl:OptimizerConfig>");

const WCHAR *CDXLSections::m_wszMetadataHeaderPrefix =
	GPOS_WSZ_LIT("<dxl:Metadata SystemIds=\"");

const WCHAR *CDXLSections::m_wszMetadataHeaderSuffix = GPOS_WSZ_LIT("\">");

const WCHAR *CDXLSections::m_wszMetadataFooter =
	GPOS_WSZ_LIT("</dxl:Metadata>");

const WCHAR *CDXLSections::m_wszTraceFlagsSectionPrefix =
	GPOS_WSZ_LIT("<dxl:TraceFlags Value=\"");

const WCHAR *CDXLSections::m_wszTraceFlagsSectionSuffix = GPOS_WSZ_LIT("\"/>");

const WCHAR *CDXLSections::m_wszStackTraceHeader =
	GPOS_WSZ_LIT("<dxl:Stacktrace>");

const WCHAR *CDXLSections::m_wszStackTraceFooter =
	GPOS_WSZ_LIT("</dxl:Stacktrace>");

// EOF
