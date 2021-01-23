//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBAgg.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB aggregates.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDGPDBAgg.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBAgg::CParseHandlerMDGPDBAgg
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMDGPDBAgg::CParseHandlerMDGPDBAgg(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid(NULL),
	  m_mdname(NULL),
	  m_mdid_type_result(NULL),
	  m_mdid_type_intermediate(NULL),
	  m_is_ordered(false),
	  m_is_splittable(true),
	  m_hash_agg_capable(true)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBAgg::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBAgg::StartElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const,  // element_qname
									 const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBAgg),
									  element_local_name))
	{
		// parse agg name
		const XMLCh *xml_str_agg_name = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenName, EdxltokenGPDBAgg);

		CWStringDynamic *str_agg_name =
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_agg_name);

		// create a copy of the string in the CMDName constructor
		m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_agg_name);

		GPOS_DELETE(str_agg_name);

		// parse metadata id info
		m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
			EdxltokenGPDBAgg);

		// parse ordered aggregate info
		const XMLCh *xml_str_ordered_agg =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenGPDBIsAggOrdered));
		if (NULL != xml_str_ordered_agg)
		{
			m_is_ordered = CDXLOperatorFactory::ConvertAttrValueToBool(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_ordered_agg,
				EdxltokenGPDBIsAggOrdered, EdxltokenGPDBAgg);
		}

		// parse splittable aggregate info
		const XMLCh *xml_str_splittable_agg =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenGPDBAggSplittable));
		if (NULL != xml_str_splittable_agg)
		{
			m_is_splittable = CDXLOperatorFactory::ConvertAttrValueToBool(
				m_parse_handler_mgr->GetDXLMemoryManager(),
				xml_str_splittable_agg, EdxltokenGPDBAggSplittable,
				EdxltokenGPDBAgg);
		}

		// parse hash capable aggragate info
		const XMLCh *xml_str_hash_agg_capable = attrs.getValue(
			CDXLTokens::XmlstrToken(EdxltokenGPDBAggHashAggCapable));
		if (NULL != xml_str_hash_agg_capable)
		{
			m_hash_agg_capable = CDXLOperatorFactory::ConvertAttrValueToBool(
				m_parse_handler_mgr->GetDXLMemoryManager(),
				xml_str_hash_agg_capable, EdxltokenGPDBAggHashAggCapable,
				EdxltokenGPDBAgg);
		}
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenGPDBAggResultTypeId),
					  element_local_name))
	{
		// parse result type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_result = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
			EdxltokenGPDBAggResultTypeId);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(
						  EdxltokenGPDBAggIntermediateResultTypeId),
					  element_local_name))
	{
		// parse intermediate result type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_intermediate =
			CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenMdid, EdxltokenGPDBAggIntermediateResultTypeId);
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
//		CParseHandlerMDGPDBAgg::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBAgg::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const  // element_qname
)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBAgg),
									  element_local_name))
	{
		// construct the MD agg object from its part
		GPOS_ASSERT(m_mdid->IsValid() && NULL != m_mdname);

		m_imd_obj = GPOS_NEW(m_mp)
			CMDAggregateGPDB(m_mp, m_mdid, m_mdname, m_mdid_type_result,
							 m_mdid_type_intermediate, m_is_ordered,
							 m_is_splittable, m_hash_agg_capable);

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenGPDBAggResultTypeId),
					  element_local_name) &&
			 0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(
						  EdxltokenGPDBAggIntermediateResultTypeId),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

// EOF
