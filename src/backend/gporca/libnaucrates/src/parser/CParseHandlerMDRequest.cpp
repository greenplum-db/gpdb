//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDRequest.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing MD requests
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDRequest.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;
XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::CParseHandlerMDRequest
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDRequest::CParseHandlerMDRequest(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid_array(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::~CParseHandlerMDRequest
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerMDRequest::~CParseHandlerMDRequest()
{
	CRefCount::SafeRelease(m_mdid_array);
	CRefCount::SafeRelease(m_mdtype_request_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRequest::StartElement(const XMLCh *const,  // element_uri,
									 const XMLCh *const element_local_name,
									 const XMLCh *const,  // element_qname
									 const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMDRequest),
								 element_local_name))
	{
		// start of MD request section
		GPOS_ASSERT(NULL == m_mdid_array);
		m_mdid_array = GPOS_NEW(m_mp) IMdIdArray(m_mp);
		m_mdtype_request_array =
			GPOS_NEW(m_mp) CMDRequest::SMDTypeRequestArray(m_mp);

		return;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMdid),
									  element_local_name))
	{
		GPOS_ASSERT(NULL != m_mdid_array);

		// parse mdid
		IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenValue,
			EdxltokenMdid);
		m_mdid_array->Append(mdid);

		return;
	}

	GPOS_ASSERT(0 == XMLString::compareString(
						 CDXLTokens::XmlstrToken(EdxltokenMDTypeRequest),
						 element_local_name));
	GPOS_ASSERT(NULL != m_mdtype_request_array);

	CSystemId sysid = CDXLOperatorFactory::Sysid(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenSysid,
		EdxltokenMDTypeRequest);

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenMDTypeRequest),
				 element_local_name))
	{
		// parse type request
		IMDType::ETypeInfo type_info = (IMDType::ETypeInfo)
			CDXLOperatorFactory::ExtractConvertAttrValueToUlong(
				m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
				EdxltokenTypeInfo, EdxltokenMDTypeRequest);
		m_mdtype_request_array->Append(
			GPOS_NEW(m_mp) CMDRequest::SMDTypeRequest(sysid, type_info));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRequest::EndElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const  // element_qname
)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenMDRequest),
								 element_local_name))
	{
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerMDRequest::GetParseHandlerType() const
{
	return EdxlphMetadataRequest;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::GetMdIdArray
//
//	@doc:
//		Parsed array of mdids
//
//---------------------------------------------------------------------------
IMdIdArray *
CParseHandlerMDRequest::GetMdIdArray() const
{
	return m_mdid_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRequest::GetMDTypeRequestArray
//
//	@doc:
//		Parsed array of type requests
//
//---------------------------------------------------------------------------
CMDRequest::SMDTypeRequestArray *
CParseHandlerMDRequest::GetMDTypeRequestArray() const
{
	return m_mdtype_request_array;
}

// EOF
