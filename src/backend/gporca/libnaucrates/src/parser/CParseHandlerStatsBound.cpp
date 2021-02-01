//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatsBound.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing
//	    the bounds of the bucket
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerStatsBound.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsBound::CParseHandlerStatsBound
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerStatsBound::CParseHandlerStatsBound(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_dxl_datum(NULL),
	  m_is_stats_bound_closed(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsBound::~CParseHandlerStatsBound
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerStatsBound::~CParseHandlerStatsBound()
{
	m_dxl_datum->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatsBound::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsBound::StartElement(const XMLCh *const,  // element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const,  // element_qname,
									  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsBucketLowerBound),
				 element_local_name) ||
		0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenStatsBucketUpperBound),
				 element_local_name))
	{
		GPOS_ASSERT(NULL == m_dxl_datum);

		// translate the datum and add it to the datum array
		CDXLDatum *dxl_datum = CDXLOperatorFactory::GetDatumVal(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenDatum);
		m_dxl_datum = dxl_datum;

		if (0 == XMLString::compareString(
					 CDXLTokens::XmlstrToken(EdxltokenStatsBucketLowerBound),
					 element_local_name))
		{
			m_is_stats_bound_closed =
				CDXLOperatorFactory::ExtractConvertAttrValueToBool(
					m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
					EdxltokenStatsBoundClosed, EdxltokenStatsBucketLowerBound);
		}
		else
		{
			m_is_stats_bound_closed =
				CDXLOperatorFactory::ExtractConvertAttrValueToBool(
					m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
					EdxltokenStatsBoundClosed, EdxltokenStatsBucketUpperBound);
		}
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
//		CParseHandlerStatsBound::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatsBound::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
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

	GPOS_ASSERT(NULL != m_dxl_datum);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
