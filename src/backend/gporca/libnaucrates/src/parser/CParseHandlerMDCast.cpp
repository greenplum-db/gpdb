//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerMDCast.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB cast functions
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDCast.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/md/CMDCastGPDB.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDCast::CParseHandlerMDCast
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDCast::CParseHandlerMDCast(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDCast::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDCast::StartElement(const XMLCh *const,  // element_uri,
								  const XMLCh *const element_local_name,
								  const XMLCh *const,  // element_qname
								  const Attributes &attrs)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBCast),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse func name
	const XMLCh *xml_str_func_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenGPDBCast);

	CMDName *mdname = CDXLUtils::CreateMDNameFromXMLChar(
		m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_func_name);


	// parse cast properties
	IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenGPDBCast);

	IMDId *mdid_src = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastSrcType, EdxltokenGPDBCast);

	IMDId *mdid_dest = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastDestType, EdxltokenGPDBCast);

	IMDId *mdid_cast_func = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastFuncId, EdxltokenGPDBCast);

	// parse whether func returns a set
	BOOL is_binary_coercible =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBCastBinaryCoercible, EdxltokenGPDBCast);

	IMDCast::EmdCoercepathType coerce_path_type = (IMDCast::EmdCoercepathType)
		CDXLOperatorFactory::ExtractConvertAttrValueToInt(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBCastCoercePathType, EdxltokenGPDBCast,
			true  // coerce_path_type is optional
		);

	m_imd_obj = GPOS_NEW(m_mp)
		CMDCastGPDB(m_mp, mdid, mdname, mdid_src, mdid_dest,
					is_binary_coercible, mdid_cast_func, coerce_path_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDCast::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDCast::EndElement(const XMLCh *const,	 // element_uri,
								const XMLCh *const element_local_name,
								const XMLCh *const	// element_qname
)
{
	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBCast),
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
