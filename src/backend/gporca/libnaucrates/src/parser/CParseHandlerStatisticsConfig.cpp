//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerStatisticsConfig.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing statistics
//		configuration
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerStatisticsConfig.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/xml/dxltokens.h"

#include "gpopt/engine/CStatisticsConfig.h"

using namespace gpdxl;
using namespace gpopt;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::CParseHandlerStatisticsConfig
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerStatisticsConfig::CParseHandlerStatisticsConfig
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	m_stats_conf(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::~CParseHandlerStatisticsConfig
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerStatisticsConfig::~CParseHandlerStatisticsConfig()
{
	CRefCount::SafeRelease(m_stats_conf);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatisticsConfig::StartElement
	(
	const XMLCh* const , //element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const , //element_qname,
	const Attributes& attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenStatisticsConfig), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// parse statistics configuration options
	CDouble damping_factor_filter = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenDampingFactorFilter, EdxltokenStatisticsConfig);
	CDouble damping_factor_join = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenDampingFactorJoin, EdxltokenStatisticsConfig);
	CDouble damping_factor_groupby = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenDampingFactorGroupBy, EdxltokenStatisticsConfig);
	ULONG max_stats_buckets = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMaxStatsBuckets, EdxltokenStatisticsConfig);

	m_stats_conf = GPOS_NEW(m_mp) CStatisticsConfig(m_mp, damping_factor_filter, damping_factor_join, damping_factor_groupby, max_stats_buckets);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatisticsConfig::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenStatisticsConfig), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE( gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_stats_conf);
	GPOS_ASSERT(0 == this->Length());

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerStatisticsConfig::GetParseHandlerType() const
{
	return EdxlphStatisticsConfig;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatisticsConfig::GetStatsConf
//
//	@doc:
//		Returns the statistics configuration
//
//---------------------------------------------------------------------------
CStatisticsConfig *
CParseHandlerStatisticsConfig::GetStatsConf() const
{
	return m_stats_conf;
}

// EOF
