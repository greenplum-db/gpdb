//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerMDArrayCoerceCast.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB array coerce cast functions
//---------------------------------------------------------------------------

#include "naucrates/md/CMDArrayCoerceCastGPDB.h"

#include "naucrates/dxl/parser/CParseHandlerMDArrayCoerceCast.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

// ctor
CParseHandlerMDArrayCoerceCast::CParseHandlerMDArrayCoerceCast(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root)
{
}

// invoked by Xerces to process an opening tag
void
CParseHandlerMDArrayCoerceCast::StartElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const,	 // element_qname
	const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenGPDBArrayCoerceCast),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse func name
	const XMLCh *xml_str_fun_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenGPDBArrayCoerceCast);

	CMDName *mdname = CDXLUtils::CreateMDNameFromXMLChar(
		m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_fun_name);

	// parse cast properties
	IMDId *mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenGPDBArrayCoerceCast);

	IMDId *mdid_src = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastSrcType, EdxltokenGPDBArrayCoerceCast);

	IMDId *mdid_dest = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastDestType, EdxltokenGPDBArrayCoerceCast);

	IMDId *mdid_cast_func = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenGPDBCastFuncId, EdxltokenGPDBArrayCoerceCast);

	// parse whether func returns a set
	BOOL is_binary_coercible =
		CDXLOperatorFactory::ExtractConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBCastBinaryCoercible, EdxltokenGPDBArrayCoerceCast);

	// parse coercion path type
	IMDCast::EmdCoercepathType coerce_path_type = (IMDCast::EmdCoercepathType)
		CDXLOperatorFactory::ExtractConvertAttrValueToInt(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenGPDBCastCoercePathType, EdxltokenGPDBArrayCoerceCast);

	INT type_modifier = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenTypeMod,
		EdxltokenGPDBArrayCoerceCast, true, default_type_modifier);

	BOOL is_explicit = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenIsExplicit,
		EdxltokenGPDBArrayCoerceCast);

	EdxlCoercionForm dxl_coercion_form =
		(EdxlCoercionForm) CDXLOperatorFactory::ExtractConvertAttrValueToInt(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenCoercionForm, EdxltokenGPDBArrayCoerceCast);

	INT location = CDXLOperatorFactory::ExtractConvertAttrValueToInt(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenLocation,
		EdxltokenGPDBArrayCoerceCast);

	m_imd_obj = GPOS_NEW(m_mp) CMDArrayCoerceCastGPDB(
		m_mp, mdid, mdname, mdid_src, mdid_dest, is_binary_coercible,
		mdid_cast_func, coerce_path_type, type_modifier, is_explicit,
		dxl_coercion_form, location);
}

// invoked by Xerces to process a closing tag
void
CParseHandlerMDArrayCoerceCast::EndElement(
	const XMLCh *const,	 // element_uri,
	const XMLCh *const element_local_name,
	const XMLCh *const	// element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenGPDBArrayCoerceCast),
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
