//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerMDGPDBFunc.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing metadata for
//		GPDB functions.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDGPDBFunc.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"


using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBFunc::CParseHandlerMDGPDBFunc
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDGPDBFunc::CParseHandlerMDGPDBFunc
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
	m_mdid(NULL),
	m_mdname(NULL),
	m_mdid_type_result(NULL),
	m_mdid_types_array(NULL),
	m_func_stability(CMDFunctionGPDB::EfsSentinel)
{}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBFunc::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBFunc::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes& attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFunc), element_local_name))
	{
		// parse func name
		const XMLCh *xml_str_func_name = CDXLOperatorFactory::ExtractAttrValue
															(
															attrs,
															EdxltokenName,
															EdxltokenGPDBFunc
															);

		CWStringDynamic *str_func_name = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_func_name);
		
		// create a copy of the string in the CMDName constructor
		m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_func_name);
		
		GPOS_DELETE(str_func_name);

		// parse metadata id info
		m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
										(
										m_parse_handler_mgr->GetDXLMemoryManager(),
										attrs,
										EdxltokenMdid,
										EdxltokenGPDBFunc
										);
		
		// parse whether func returns a set
		m_returns_set = CDXLOperatorFactory::ExtractConvertAttrValueToBool
												(
												m_parse_handler_mgr->GetDXLMemoryManager(),
												attrs,
												EdxltokenGPDBFuncReturnsSet,
												EdxltokenGPDBFunc
												);
		// parse whether func is strict
		m_is_strict = CDXLOperatorFactory::ExtractConvertAttrValueToBool
											(
											m_parse_handler_mgr->GetDXLMemoryManager(),
											attrs,
											EdxltokenGPDBFuncStrict,
											EdxltokenGPDBFunc
											);
		
		// parse whether func is NDV-preserving
		m_is_ndv_preserving = CDXLOperatorFactory::ExtractConvertAttrValueToBool
											(
											m_parse_handler_mgr->GetDXLMemoryManager(),
											attrs,
											EdxltokenGPDBFuncNDVPreserving,
											EdxltokenGPDBFunc,
											true, // optional
											false // default is false
											);

		// parse func stability property
		const XMLCh *xmlszStbl = CDXLOperatorFactory::ExtractAttrValue
														(
														attrs,
														EdxltokenGPDBFuncStability,
														EdxltokenGPDBFunc
														);
		
		m_func_stability = ParseFuncStability(xmlszStbl);

		// parse func data access property
		const XMLCh *xmlszDataAcc = CDXLOperatorFactory::ExtractAttrValue
														(
														attrs,
														EdxltokenGPDBFuncDataAccess,
														EdxltokenGPDBFunc
														);

		m_func_data_access = ParseFuncDataAccess(xmlszDataAcc);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncResultTypeId), element_local_name))
	{
		// parse result type
		GPOS_ASSERT(NULL != m_mdname);

		m_mdid_type_result = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													attrs,
													EdxltokenMdid,
													EdxltokenGPDBFuncResultTypeId
													);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenOutputCols), element_local_name))
	{
		// parse output column type
		GPOS_ASSERT(NULL != m_mdname);
		GPOS_ASSERT(NULL == m_mdid_types_array);

		const XMLCh *xmlszTypes = CDXLOperatorFactory::ExtractAttrValue
															(
															attrs,
															EdxltokenTypeIds,
															EdxltokenOutputCols
															);

		m_mdid_types_array = CDXLOperatorFactory::ExtractConvertMdIdsToArray
													(
													m_parse_handler_mgr->GetDXLMemoryManager(),
													xmlszTypes,
													EdxltokenTypeIds,
													EdxltokenOutputCols
													);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBFunc::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDGPDBFunc::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFunc), element_local_name))
	{
		// construct the MD func object from its part
		GPOS_ASSERT(m_mdid->IsValid() && NULL != m_mdname);
		
		m_imd_obj = GPOS_NEW(m_mp) CMDFunctionGPDB(m_mp,
												m_mdid,
												m_mdname,
												m_mdid_type_result,
												m_mdid_types_array,
												m_returns_set,
												m_func_stability,
												m_func_data_access,
												m_is_strict,
												m_is_ndv_preserving);
		
		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();

	}
	else if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncResultTypeId), element_local_name) &&
			0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenOutputCols), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBFunc::ParseFuncStability
//
//	@doc:
//		Parses function stability property from XML string
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncStbl 
CParseHandlerMDGPDBFunc::ParseFuncStability
	(
	const XMLCh *xml_val
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncStable), xml_val))
	{
		return CMDFunctionGPDB::EfsStable;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncImmutable), xml_val))
	{
		return CMDFunctionGPDB::EfsImmutable;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncVolatile), xml_val))
	{
		return CMDFunctionGPDB::EfsVolatile;
	}

	GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncStability)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFunc)->GetBuffer()
		);

	return CMDFunctionGPDB::EfsSentinel;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDGPDBFunc::ParseFuncDataAccess
//
//	@doc:
//		Parses function data access property from XML string
//
//---------------------------------------------------------------------------
CMDFunctionGPDB::EFuncDataAcc
CParseHandlerMDGPDBFunc::ParseFuncDataAccess
	(
	const XMLCh *xml_val
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncNoSQL), xml_val))
	{
		return CMDFunctionGPDB::EfdaNoSQL;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncContainsSQL), xml_val))
	{
		return CMDFunctionGPDB::EfdaContainsSQL;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncReadsSQLData), xml_val))
	{
		return CMDFunctionGPDB::EfdaReadsSQLData;
	}

	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenGPDBFuncModifiesSQLData), xml_val))
	{
		return CMDFunctionGPDB::EfdaModifiesSQLData;
	}

	GPOS_RAISE
		(
		gpdxl::ExmaDXL,
		gpdxl::ExmiDXLInvalidAttributeValue,
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFuncDataAccess)->GetBuffer(),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBFunc)->GetBuffer()
		);

	return CMDFunctionGPDB::EfdaSentinel;
}

// EOF
