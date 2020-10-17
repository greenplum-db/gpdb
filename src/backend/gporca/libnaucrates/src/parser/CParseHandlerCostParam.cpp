//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerCostParam
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing xform
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerCostParam.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

using namespace gpdxl;
using namespace gpopt;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParam::CParseHandlerCostParam
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerCostParam::CParseHandlerCostParam(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_param_name(NULL),
	  m_value(0),
	  m_lower_bound_val(0),
	  m_upper_bound_val(0)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParam::~CParseHandlerCostParam
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerCostParam::~CParseHandlerCostParam()
{
	GPOS_DELETE_ARRAY(m_param_name);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParam::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCostParam::StartElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const,  // element_qname
									 const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCostParam),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const XMLCh *xmlstrName = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenCostParam);
	CWStringDynamic *str_name = CDXLUtils::CreateDynamicStringFromXMLChArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), xmlstrName);
	m_param_name = CDXLUtils::CreateMultiByteCharStringFromWCString(
		m_mp, str_name->GetBuffer());
	GPOS_DELETE(str_name);

	m_value = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenValue,
		EdxltokenCostParam);
	m_lower_bound_val = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenCostParamLowerBound, EdxltokenCostParam);
	m_upper_bound_val = CDXLOperatorFactory::ExtractConvertAttrValueToDouble(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenCostParamUpperBound, EdxltokenCostParam);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCostParam::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCostParam::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCostParam),
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
