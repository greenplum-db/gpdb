//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadata.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a DXL document.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMetadata.h"

#include <xercesc/util/XMLStringTokenizer.hpp>

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::CParseHandlerMetadata
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadata::CParseHandlerMetadata(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_cached_obj_array(NULL),
	  m_mdid_array(NULL),
	  m_system_id_array(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::~CParseHandlerMetadata
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerMetadata::~CParseHandlerMetadata()
{
	CRefCount::SafeRelease(m_mdid_cached_obj_array);
	CRefCount::SafeRelease(m_mdid_array);
	CRefCount::SafeRelease(m_system_id_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler. Currently we overload this method to
//		return a specific type for the plann, query and metadata parse handlers.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerMetadata::GetParseHandlerType() const
{
	return EdxlphMetadata;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::GetMdIdCachedObjArray
//
//	@doc:
//		Returns the list of metadata objects constructed by the parser
//
//---------------------------------------------------------------------------
IMDCacheObjectArray *
CParseHandlerMetadata::GetMdIdCachedObjArray()
{
	return m_mdid_cached_obj_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::GetMdIdArray
//
//	@doc:
//		Returns the list of metadata ids constructed by the parser
//
//---------------------------------------------------------------------------
IMdIdArray *
CParseHandlerMetadata::GetMdIdArray()
{
	return m_mdid_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::GetSysidPtrArray
//
//	@doc:
//		Returns the list of metadata source system ids constructed by the parser
//
//---------------------------------------------------------------------------
CSystemIdArray *
CParseHandlerMetadata::GetSysidPtrArray()
{
	return m_system_id_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadata::StartElement(const XMLCh *const element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const element_qname,
									const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(element_local_name,
								 CDXLTokens::XmlstrToken(EdxltokenMetadata)))
	{
		// start of the metadata section in the DXL document
		GPOS_ASSERT(NULL == m_mdid_cached_obj_array);

		m_mdid_cached_obj_array = GPOS_NEW(m_mp) IMDCacheObjectArray(m_mp);
		m_mdid_array = GPOS_NEW(m_mp) IMdIdArray(m_mp);

		m_system_id_array =
			GetSrcSysIdArray(attrs, EdxltokenSysids, EdxltokenMetadata);
	}
	else if (0 ==
			 XMLString::compareString(element_local_name,
									  CDXLTokens::XmlstrToken(EdxltokenMdid)))
	{
		// start of the metadata section in the DXL document
		GPOS_ASSERT(NULL != m_mdid_array);
		IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenValue,
			EdxltokenMdid);
		m_mdid_array->Append(mdid);
	}
	else
	{
		// must be a metadata object
		GPOS_ASSERT(NULL != m_mdid_cached_obj_array);

		// install a parse handler for the given element
		CParseHandlerBase *parse_handler_base =
			CParseHandlerFactory::GetParseHandler(m_mp, element_local_name,
												  m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handler
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name,
										 element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMetadata::EndElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 element_local_name,
				 CDXLTokens::XmlstrToken(EdxltokenMetadata)) &&
		0 != XMLString::compareString(element_local_name,
									  CDXLTokens::XmlstrToken(EdxltokenMdid)))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_mdid_cached_obj_array);

	const ULONG size = this->Length();
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerMetadataObject *metadata_obj_parse_handler =
			dynamic_cast<CParseHandlerMetadataObject *>((*this)[idx]);

		GPOS_ASSERT(NULL != metadata_obj_parse_handler->GetImdObj());

		IMDCacheObject *imdobj = metadata_obj_parse_handler->GetImdObj();
		imdobj->AddRef();
		m_mdid_cached_obj_array->Append(imdobj);
	}

	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMetadata::GetSrcSysIdArray
//
//	@doc:
//		Parse a list of source system ids
//
//---------------------------------------------------------------------------
CSystemIdArray *
CParseHandlerMetadata::GetSrcSysIdArray(const Attributes &attrs,
										Edxltoken dxl_token_attr,
										Edxltoken dxl_token_element)
{
	const XMLCh *xml_str_attrname = CDXLTokens::XmlstrToken(dxl_token_attr);

	// extract systemids
	const XMLCh *xml_str_val = attrs.getValue(xml_str_attrname);

	if (NULL == xml_str_val)
	{
		return NULL;
	}

	CSystemIdArray *src_sys_id_array = GPOS_NEW(m_mp) CSystemIdArray(m_mp);

	// extract separate system ids
	XMLStringTokenizer xml_str_tokenizer(
		xml_str_val, CDXLTokens::XmlstrToken(EdxltokenComma));

	XMLCh *xml_str_sys_id = NULL;
	while (NULL != (xml_str_sys_id = xml_str_tokenizer.nextToken()))
	{
		// get sysid components
		XMLStringTokenizer xml_str_sys_id_tokenizer(
			xml_str_sys_id, CDXLTokens::XmlstrToken(EdxltokenDot));
		GPOS_ASSERT(2 == xml_str_sys_id_tokenizer.countTokens());

		XMLCh *xml_str_type = xml_str_sys_id_tokenizer.nextToken();
		ULONG type = CDXLOperatorFactory::ConvertAttrValueToUlong(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_type,
			dxl_token_attr, dxl_token_element);

		XMLCh *xml_str_name = xml_str_sys_id_tokenizer.nextToken();
		CWStringDynamic *str_name =
			CDXLUtils::CreateDynamicStringFromXMLChArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_name);

		src_sys_id_array->Append(GPOS_NEW(m_mp) CSystemId(
			(IMDId::EMDIdType) type, str_name->GetBuffer(),
			str_name->Length()));

		GPOS_DELETE(str_name);
	}

	return src_sys_id_array;
}

// EOF
