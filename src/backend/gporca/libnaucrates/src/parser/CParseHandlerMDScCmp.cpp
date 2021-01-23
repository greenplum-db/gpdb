//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerMDScCmp.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB scalar comparison operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDScCmp.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CMDScCmpGPDB.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDScCmp::CParseHandlerMDScCmp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDScCmp::CParseHandlerMDScCmp(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDScCmp::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDScCmp::StartElement(const XMLCh *const,	// element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const,	// element_qname
								   const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBMDScCmp),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse operator name
	const XMLCh *xml_str_op_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenGPDBMDScCmp);

	CMDName *mdname = CDXLUtils::CreateMDNameFromXMLChar(
		m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_op_name);


	// parse scalar comparison properties
	IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenGPDBMDScCmp);

	IMDId *mdid_left = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBScalarOpLeftTypeId, EdxltokenGPDBMDScCmp);

	IMDId *mdid_right = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBScalarOpRightTypeId, EdxltokenGPDBMDScCmp);

	IMDId *mdid_op = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenOpNo,
		EdxltokenGPDBMDScCmp);

	// parse comparison type
	const XMLCh *xml_str_comp_type = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenGPDBScalarOpCmpType, EdxltokenGPDBMDScCmp);

	IMDType::ECmpType comparision_type =
		CDXLOperatorFactory::ParseCmpType(xml_str_comp_type);

	m_imd_obj = GPOS_NEW(m_mp) CMDScCmpGPDB(
		m_mp, mdid, mdname, mdid_left, mdid_right, comparision_type, mdid_op);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDScCmp::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDScCmp::EndElement(const XMLCh *const,  // element_uri,
								 const XMLCh *const element_local_name,
								 const XMLCh *const	 // element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBMDScCmp),
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
