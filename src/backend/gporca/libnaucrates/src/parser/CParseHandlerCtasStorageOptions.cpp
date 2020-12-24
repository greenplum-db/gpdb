//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerCtasStorageOptions.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing CTAS storage
//		options
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerCtasStorageOptions.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCtasStorageOptions::CParseHandlerCtasStorageOptions
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerCtasStorageOptions::CParseHandlerCtasStorageOptions(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_mdname_tablespace(NULL),
	  m_dxl_ctas_storage_option(NULL),
	  m_ctas_storage_option_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCtasStorageOptions::~CParseHandlerCtasStorageOptions
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerCtasStorageOptions::~CParseHandlerCtasStorageOptions()
{
	CRefCount::SafeRelease(m_dxl_ctas_storage_option);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCtasStorageOptions::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCtasStorageOptions::StartElement(
	const XMLCh *const,	 // element_uri
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCTASOptions),
								 element_local_name))
	{
		const XMLCh *xml_str_tablespace =
			attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenTablespace));
		if (NULL != xml_str_tablespace)
		{
			m_mdname_tablespace = CDXLUtils::CreateMDNameFromXMLChar(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_tablespace);
		}

		m_ctas_on_commit_action =
			CDXLOperatorFactory::ParseOnCommitActionSpec(attrs);
	}
	else if (0 == XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenCTASOption),
					  element_local_name))
	{
		// parse option name and m_bytearray_value
		ULONG ctas_option_type =
			CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenCtasOptionType, EdxltokenCTASOption);
		CWStringBase *ctas_option_name =
			CDXLOperatorFactory::ExtractConvertAttrValueToStr(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenName, EdxltokenCTASOption);
		CWStringBase *ctas_option_val =
			CDXLOperatorFactory::ExtractConvertAttrValueToStr(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenValue, EdxltokenCTASOption);
		BOOL is_null = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenIsNull,
			EdxltokenCTASOption);

		if (NULL == m_ctas_storage_option_array)
		{
			m_ctas_storage_option_array = GPOS_NEW(m_mp)
				CDXLCtasStorageOptions::CDXLCtasOptionArray(m_mp);
		}
		m_ctas_storage_option_array->Append(
			GPOS_NEW(m_mp) CDXLCtasStorageOptions::CDXLCtasOption(
				ctas_option_type, ctas_option_name, ctas_option_val, is_null));
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
//		CParseHandlerCtasStorageOptions::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerCtasStorageOptions::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenCTASOptions),
								 element_local_name))
	{
		m_dxl_ctas_storage_option = GPOS_NEW(m_mp)
			CDXLCtasStorageOptions(m_mdname_tablespace, m_ctas_on_commit_action,
								   m_ctas_storage_option_array);
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
	else if (0 != XMLString::compareString(
					  CDXLTokens::XmlstrToken(EdxltokenCTASOption),
					  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerCtasStorageOptions::GetDxlCtasStorageOption
//
//	@doc:
//		Return parsed storage options
//
//---------------------------------------------------------------------------
CDXLCtasStorageOptions *
CParseHandlerCtasStorageOptions::GetDxlCtasStorageOption() const
{
	return m_dxl_ctas_storage_option;
}

// EOF
