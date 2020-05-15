//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc
//
//	@filename:
//		CParseHandlerLogicalCTAS.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing logical
//		CTAS operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerLogicalCTAS.h"

#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/parser/CParseHandlerCtasStorageOptions.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"

#include "naucrates/dxl/operators/CDXLLogicalCTAS.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalCTAS::CParseHandlerLogicalCTAS
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerLogicalCTAS::CParseHandlerLogicalCTAS
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerLogicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_mdid(NULL),
	m_mdname_schema(NULL),	
	m_mdname(NULL),	
	m_distr_column_pos_array(NULL),
	m_src_colids_array(NULL),
	m_vartypemod_array(NULL),
	m_is_temp_table(false),
	m_opfamilies_parse_handler(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalCTAS::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalCTAS::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes &attrs
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenRelDistrOpfamilies), element_local_name))
	{
		// parse handler for check constraints
		m_opfamilies_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
																		   m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(m_opfamilies_parse_handler);
		this->Append(m_opfamilies_parse_handler);
		m_opfamilies_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);

		return;
	}

	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalCTAS), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	// parse metadata id
	m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId
						(
						m_parse_handler_mgr->GetDXLMemoryManager(),
						attrs,
						EdxltokenMdid,
						EdxltokenLogicalCTAS
						);
	
	GPOS_ASSERT(IMDId::EmdidGPDBCtas == m_mdid->MdidType());
	
	// parse table name
	const XMLCh *xml_str_table_name = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenName, EdxltokenLogicalCTAS);
	m_mdname = CDXLUtils::CreateMDNameFromXMLChar(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_table_name);
	
	const XMLCh *xml_str_schema_name = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenSchema));
	if (NULL != xml_str_schema_name)
	{
		m_mdname_schema = CDXLUtils::CreateMDNameFromXMLChar(m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_schema_name);
	}
	
	// parse distribution policy
	const XMLCh *rel_distr_policy_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenRelDistrPolicy, EdxltokenLogicalCTAS);
	m_rel_distr_policy = CDXLOperatorFactory::ParseRelationDistPolicy(rel_distr_policy_xml);

	if (IMDRelation::EreldistrHash == m_rel_distr_policy)
	{
		// parse distribution columns
		const XMLCh *rel_distr_cols_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenDistrColumns, EdxltokenLogicalCTAS);
		m_distr_column_pos_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), rel_distr_cols_xml, EdxltokenDistrColumns, EdxltokenLogicalCTAS);
	}
	
	// parse storage type
	const XMLCh *rel_storage_type_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenRelStorageType, EdxltokenLogicalCTAS);
	m_rel_storage_type = CDXLOperatorFactory::ParseRelationStorageType(rel_storage_type_xml);

	const XMLCh *src_colids_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenInsertCols, EdxltokenLogicalCTAS);
	m_src_colids_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), src_colids_xml, EdxltokenInsertCols, EdxltokenLogicalCTAS);

	const XMLCh *vartypemod_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenVarTypeModList, EdxltokenLogicalCTAS);
	m_vartypemod_array =
			CDXLOperatorFactory::ExtractIntsToIntArray(m_parse_handler_mgr->GetDXLMemoryManager(), vartypemod_xml, EdxltokenVarTypeModList, EdxltokenLogicalCTAS);
	
	m_is_temp_table = CDXLOperatorFactory::ExtractConvertAttrValueToBool(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRelTemporary, EdxltokenLogicalCTAS);
	m_has_oids = CDXLOperatorFactory::ExtractConvertAttrValueToBool(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRelHasOids, EdxltokenLogicalCTAS);

	// create child node parsers

	// parse handler for logical operator
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	//parse handler for the storage options
	CParseHandlerBase *ctas_options_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenCTASOptions), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(ctas_options_parse_handler);
	
	//parse handler for the column descriptors
	CParseHandlerBase *col_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(col_descr_parse_handler);

	// store child parse handler in array
	this->Append(col_descr_parse_handler);
	this->Append(ctas_options_parse_handler);
	this->Append(child_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerLogicalCTAS::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerLogicalCTAS::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenLogicalCTAS), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(3 == this->Length());

	CParseHandlerColDescr *col_descr_parse_handler = dynamic_cast<CParseHandlerColDescr *>((*this)[0]);
	CParseHandlerCtasStorageOptions *ctas_options_parse_handler = dynamic_cast<CParseHandlerCtasStorageOptions *>((*this)[1]);
	CParseHandlerLogicalOp *child_parse_handler = dynamic_cast<CParseHandlerLogicalOp*>((*this)[2]);

	GPOS_ASSERT(NULL != col_descr_parse_handler->GetDXLColumnDescrArray());
	GPOS_ASSERT(NULL != ctas_options_parse_handler->GetDxlCtasStorageOption());
	GPOS_ASSERT(NULL != child_parse_handler->CreateDXLNode());
	
	CDXLColDescrArray *dxl_column_descr_array = col_descr_parse_handler->GetDXLColumnDescrArray();
	dxl_column_descr_array->AddRef();
	
	CDXLCtasStorageOptions *dxl_ctas_storage_opt = ctas_options_parse_handler->GetDxlCtasStorageOption();
	dxl_ctas_storage_opt->AddRef();

	IMdIdArray *distr_opfamilies = NULL;
	if (m_opfamilies_parse_handler != NULL)
	{
		distr_opfamilies = dynamic_cast<CParseHandlerMetadataIdList*>(m_opfamilies_parse_handler)->GetMdIdArray();
		distr_opfamilies->AddRef();
	}
	
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode
							(
							m_mp,
							GPOS_NEW(m_mp) CDXLLogicalCTAS
										(
										m_mp, 
										m_mdid, 
										m_mdname_schema, 
										m_mdname, 
										dxl_column_descr_array,
										dxl_ctas_storage_opt,
										m_rel_distr_policy, 
										m_distr_column_pos_array,
										distr_opfamilies,
										m_is_temp_table, 
										m_has_oids, 
										m_rel_storage_type, 
										m_src_colids_array,
										m_vartypemod_array
										)
							);
	
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
