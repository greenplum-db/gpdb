//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerXform.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing xform
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerXform.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "gpopt/xforms/CXform.h"
#include "gpopt/xforms/CXformFactory.h"


using namespace gpdxl;
using namespace gpopt;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerXform::CParseHandlerXform
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerXform::CParseHandlerXform(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(mp, parse_handler_mgr, parse_handler_root),
	  m_xform(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerXform::~CParseHandlerXform
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CParseHandlerXform::~CParseHandlerXform()
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerXform::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerXform::StartElement(const XMLCh *const,  // element_uri,
								 const XMLCh *const element_local_name,
								 const XMLCh *const,  // element_qname
								 const Attributes &attrs)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenXform),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	const XMLCh *xml_str_xform_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenXform);
	CWStringDynamic *str_xform_name =
		CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_xform_name);
	CHAR *char_str_xform_name =
		CDXLUtils::CreateMultiByteCharStringFromWCString(
			m_mp, str_xform_name->GetBuffer());
	m_xform = CXformFactory::Pxff()->Pxf(char_str_xform_name);
	GPOS_ASSERT(NULL != m_xform);

	GPOS_DELETE(str_xform_name);
	GPOS_DELETE_ARRAY(char_str_xform_name);
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerXform::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerXform::EndElement(const XMLCh *const,	// element_uri,
							   const XMLCh *const element_local_name,
							   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenXform),
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
