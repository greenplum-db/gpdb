//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerColStats.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing column
//		statistics.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerColStats.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerColStatsBucket.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStats::CParseHandlerColStats
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerColStats::CParseHandlerColStats(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_base),
	  m_mdid(NULL),
	  m_md_name(NULL),
	  m_width(0.0),
	  m_null_freq(0.0),
	  m_distinct_remaining(0.0),
	  m_freq_remaining(0.0),
	  m_is_column_stats_missing(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStats::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerColStats::StartElement(const XMLCh *const element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const element_qname,
									const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumnStats),
								 element_local_name))
	{
		// new column stats object
		GPOS_ASSERT(NULL == m_mdid);

		// parse mdid and name
		IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
			EdxltokenColumnStats);
		m_mdid = CMDIdColStats::CastMdid(mdid);

		// parse column name
		const XMLCh *parsed_column_name = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenName, EdxltokenColumnStats);

		CWStringDynamic *column_name =
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), parsed_column_name);

		// create a copy of the string in the CMDName constructor
		m_md_name = GPOS_NEW(m_mp) CMDName(m_mp, column_name);
		GPOS_DELETE(column_name);

		m_width = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenWidth,
			EdxltokenColumnStats);

		const XMLCh *parsed_null_freq =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColNullFreq));
		if (NULL != parsed_null_freq)
		{
			m_null_freq = CDXLOperatorFactory::ConvertAttrValueToDouble(
				m_parse_handler_mgr->GetDXLMemoryManager(), parsed_null_freq,
				EdxltokenColNullFreq, EdxltokenColumnStats);
		}

		const XMLCh *parsed_distinct_remaining =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColNdvRemain));
		if (NULL != parsed_distinct_remaining)
		{
			m_distinct_remaining =
				CDXLOperatorFactory::ConvertAttrValueToDouble(
					m_parse_handler_mgr->GetDXLMemoryManager(),
					parsed_distinct_remaining, EdxltokenColNdvRemain,
					EdxltokenColumnStats);
		}

		const XMLCh *parsed_freq_remaining =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColFreqRemain));
		if (NULL != parsed_freq_remaining)
		{
			m_freq_remaining = CDXLOperatorFactory::ConvertAttrValueToDouble(
				m_parse_handler_mgr->GetDXLMemoryManager(),
				parsed_freq_remaining, EdxltokenColFreqRemain,
				EdxltokenColumnStats);
		}

		const XMLCh *parsed_is_column_stats_missing =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenColStatsMissing));
		if (NULL != parsed_is_column_stats_missing)
		{
			m_is_column_stats_missing =
				CDXLOperatorFactory::ConvertAttrValueToBool(
					m_parse_handler_mgr->GetDXLMemoryManager(),
					parsed_is_column_stats_missing, EdxltokenColStatsMissing,
					EdxltokenColumnStats);
		}
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
					  element_local_name))
	{
		// new bucket
		CParseHandlerBase *parse_handler_base_stats_bucket =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
				m_parse_handler_mgr, this);
		this->Append(parse_handler_base_stats_bucket);

		m_parse_handler_mgr->ActivateParseHandler(
			parse_handler_base_stats_bucket);
		parse_handler_base_stats_bucket->startElement(
			element_uri, element_local_name, element_qname, attrs);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStats::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerColStats::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenColumnStats),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// get histogram buckets from child parse handlers

	CDXLBucketArray *dxl_stats_bucket_array =
		GPOS_NEW(m_mp) CDXLBucketArray(m_mp);

	for (ULONG ul = 0; ul < this->Length(); ul++)
	{
		CParseHandlerColStatsBucket *parse_handler_col_stats_bucket =
			dynamic_cast<CParseHandlerColStatsBucket *>((*this)[ul]);

		CDXLBucket *dxl_bucket =
			parse_handler_col_stats_bucket->GetDXLBucketAt();
		dxl_bucket->AddRef();

		dxl_stats_bucket_array->Append(dxl_bucket);
	}

	m_imd_obj = GPOS_NEW(m_mp) CDXLColStats(
		m_mp, m_mdid, m_md_name, m_width, m_null_freq, m_distinct_remaining,
		m_freq_remaining, dxl_stats_bucket_array, m_is_column_stats_missing);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
