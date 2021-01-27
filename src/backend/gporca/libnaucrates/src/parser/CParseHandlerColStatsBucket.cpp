//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerColStatsBucket.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a bucket in
//		a col stats object
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerColStatsBucket.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CDXLColStats.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStatsBucket::CParseHandlerColStatsBucket
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerColStatsBucket::CParseHandlerColStatsBucket(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_base)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_base),
	  m_frequency(0.0),
	  m_distinct(0.0),
	  m_lower_bound_dxl_datum(NULL),
	  m_upper_bound_dxl_datum(NULL),
	  m_is_lower_closed(false),
	  m_is_upper_closed(false),
	  m_dxl_bucket(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStatsBucket::~CParseHandlerColStatsBucket
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerColStatsBucket::~CParseHandlerColStatsBucket()
{
	m_dxl_bucket->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStatsBucket::GetDXLBucketAt
//
//	@doc:
//		The bucket constructed by the parse handler
//
//---------------------------------------------------------------------------
CDXLBucket *
CParseHandlerColStatsBucket::GetDXLBucketAt() const
{
	return m_dxl_bucket;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerColStatsBucket::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerColStatsBucket::StartElement(const XMLCh *const,  // element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const,  // element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
				 element_local_name))
	{
		// new column stats bucket

		// parse frequency and distinct values
		m_frequency = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsFrequency, EdxltokenColumnStatsBucket);
		m_distinct = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsDistinct, EdxltokenColumnStatsBucket);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenStatsBucketLowerBound),
					  element_local_name))
	{
		// parse lower bound
		m_lower_bound_dxl_datum = CDXLOperatorFactory::GetDatumVal(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsBucketLowerBound);
		m_is_lower_closed = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsBoundClosed, EdxltokenStatsBucketLowerBound);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenStatsBucketUpperBound),
					  element_local_name))
	{
		// parse upper bound
		m_upper_bound_dxl_datum = CDXLOperatorFactory::GetDatumVal(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsBucketUpperBound);
		m_is_upper_closed = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenStatsBoundClosed, EdxltokenStatsBucketUpperBound);
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
//		CParseHandlerColStatsBucket::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerColStatsBucket::EndElement(const XMLCh *const,	 // element_uri,
										const XMLCh *const element_local_name,
										const XMLCh *const	// element_qname
)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenColumnStatsBucket),
				 element_local_name))
	{
		m_dxl_bucket = GPOS_NEW(m_mp) CDXLBucket(
			m_lower_bound_dxl_datum, m_upper_bound_dxl_datum, m_is_lower_closed,
			m_is_upper_closed, m_frequency, m_distinct);

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenStatsBucketLowerBound),
					  element_local_name) &&
			 0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenStatsBucketUpperBound),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// EOF
