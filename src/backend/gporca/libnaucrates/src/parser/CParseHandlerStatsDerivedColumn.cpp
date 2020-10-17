//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatsDerivedColumn.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing statistics of
//		derived column.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerStatsDerivedColumn.h"
#include "naucrates/dxl/parser/CParseHandlerColStatsBucket.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedColumn::CParseHandlerStatsDerivedColumn
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerStatsDerivedColumn::CParseHandlerStatsDerivedColumn(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_colid(0),
	  m_width(CStatistics::DefaultColumnWidth),
	  m_null_freq(0.0),
	  m_distinct_remaining(0.0),
	  m_freq_remaining(0.0),
	  m_dxl_stats_derived_col(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedColumn::~CParseHandlerStatsDerivedColumn
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerStatsDerivedColumn::~CParseHandlerStatsDerivedColumn()
{
	m_dxl_stats_derived_col->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsDerivedColumn::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsDerivedColumn::StartElement(
	const XMLCh *const element_uri, const XMLCh *const element_local_name,
	const XMLCh *const element_qname, const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsDerivedColumn),
				 element_local_name))
	{
		// must have not seen a bucket yet
		GPOS_ASSERT(0 == this->Length());

		// parse column id
		m_colid = CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenColId,
			EdxltokenStatsDerivedColumn);

		// parse column width
		m_width = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenWidth,
			EdxltokenStatsDerivedColumn);

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
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
					  element_local_name))
	{
		// install a parse handler for the given element
		CParseHandlerBase *parse_handler_base =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handler
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name,
										 element_qname, attrs);
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
//		CParseHandlerStatsDerivedColumn::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsDerivedColumn::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsDerivedColumn),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CDXLBucketArray *dxl_stats_bucket_array =
		GPOS_NEW(m_mp) CDXLBucketArray(m_mp);

	const ULONG num_of_buckets = this->Length();
	// add constructed children from child parse handlers
	for (ULONG idx = 0; idx < num_of_buckets; idx++)
	{
		CParseHandlerColStatsBucket *col_stats_bucket_parse_handler =
			dynamic_cast<CParseHandlerColStatsBucket *>((*this)[idx]);
		CDXLBucket *dxl_bucket =
			col_stats_bucket_parse_handler->GetDXLBucketAt();
		dxl_bucket->AddRef();
		dxl_stats_bucket_array->Append(dxl_bucket);
	}

	m_dxl_stats_derived_col = GPOS_NEW(m_mp) CDXLStatsDerivedColumn(
		m_colid, m_width, m_null_freq, m_distinct_remaining, m_freq_remaining,
		dxl_stats_bucket_array);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
