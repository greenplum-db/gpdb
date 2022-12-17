//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsInfo.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a extended
//		stat metadata object
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerExtStatsInfo.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerExtStatsDependency.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLExtStatsInfo.h"
#include "naucrates/md/CMDExtStatInfo.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

CParseHandlerExtStatsInfo::CParseHandlerExtStatsInfo(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_extinfo(nullptr)
{
}

CParseHandlerExtStatsInfo::~CParseHandlerExtStatsInfo()
{
	m_extinfo->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsInfo::GetInfo
//
//	@doc:
//		The metadata (info) constructed by the parse handler
//
//---------------------------------------------------------------------------
CMDExtStatInfo *
CParseHandlerExtStatsInfo::GetInfo() const
{
	return m_extinfo;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsInfo::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsInfo::StartElement(
	const XMLCh *const element_uri GPOS_UNUSED,
	const XMLCh *const element_local_name,
	const XMLCh *const element_qname GPOS_UNUSED, const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenExtendedStatsInfo),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
	OID stat_oid = CDXLOperatorFactory::ExtractConvertAttrValueToOid(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenOid,
		EdxltokenExtendedStatsInfo);

	const XMLCh *parsed_stat_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenExtendedStatsInfo);

	CWStringDynamic *stat_name_str =
		CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), parsed_stat_name);

	// create a copy of the string in the CMDName constructor
	CMDName *stat_name = GPOS_NEW(m_mp) CMDName(m_mp, stat_name_str);
	GPOS_DELETE(stat_name_str);

	ULongPtrArray *keys = CDXLOperatorFactory::ExtractConvertValuesToArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenKeys,
		EdxltokenExtendedStatsInfo);

	CBitSet *bs_keys = GPOS_NEW(m_mp) CBitSet(m_mp);
	for (ULONG i = 0; i < keys->Size(); i++)
	{
		bs_keys->ExchangeSet(*(*keys)[i]);
	}
	keys->Release();

	// TODO: parse kind..
	m_extinfo = GPOS_NEW(m_mp) CMDExtStatInfo(
		m_mp, stat_oid, stat_name, CMDExtStatInfo::EstatDependencies, bs_keys);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerExtStatsInfo::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerExtStatsInfo::EndElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenExtendedStatsInfo),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
