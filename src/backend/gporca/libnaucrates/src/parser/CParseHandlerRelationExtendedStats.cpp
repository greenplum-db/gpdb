//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerRelationExtendedStats.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing relation
//		extended statistics.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerRelationExtendedStats.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsInfo.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLExtStatsInfo.h"
#include "naucrates/md/CMDExtStatsInfo.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelationExtendedStats::CParseHandlerRelationExtendedStats
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerRelationExtendedStats::CParseHandlerRelationExtendedStats(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelationExtendedStats::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerRelationExtendedStats::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenRelationExtendedStats),
				 element_local_name) &&
		0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenExtendedStatsInfo),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenExtendedStatsInfo),
					  element_local_name))
	{
		CParseHandlerBase *extinfos_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenExtendedStatsInfo),
				m_parse_handler_mgr, this);
		this->Append(extinfos_parse_handler);

		m_parse_handler_mgr->ActivateParseHandler(extinfos_parse_handler);
		extinfos_parse_handler->startElement(element_uri, element_local_name,
											 element_qname, attrs);
	}
	else
	{
		// parse table name
		const XMLCh *xml_str_table_name = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenName, EdxltokenRelationExtendedStats);

		CWStringDynamic *str_table_name =
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_table_name);

		// create a copy of the string in the CMDName constructor
		m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_table_name);

		GPOS_DELETE(str_table_name);

		// parse metadata id info
		m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
			EdxltokenRelationExtendedStats);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerRelationExtendedStats::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerRelationExtendedStats::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenRelationExtendedStats),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CMDExtStatsInfoArray *extstatsinfo_array =
		GPOS_NEW(m_mp) CMDExtStatsInfoArray(m_mp);


	for (ULONG ul = 0; ul < this->Length(); ul++)
	{
		CParseHandlerExtStatsInfo *parse_handler_ext_stats_info =
			dynamic_cast<CParseHandlerExtStatsInfo *>((*this)[ul]);
		CMDExtStatsInfo *info = parse_handler_ext_stats_info->GetInfo();
		info->AddRef();

		extstatsinfo_array->Append(info);
	}

	m_imd_obj = GPOS_NEW(m_mp)
		CDXLExtStatsInfo(m_mp, m_mdid, m_mdname, extstatsinfo_array);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
