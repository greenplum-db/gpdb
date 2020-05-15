//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc
//
//	@filename:
//		CParseHandlerPhysicalCTAS.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing physical
//		CTAS operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalCTAS.h"

#include "naucrates/dxl/parser/CParseHandlerColDescr.h"
#include "naucrates/dxl/parser/CParseHandlerCtasStorageOptions.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLPhysicalCTAS.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalCTAS::CParseHandlerPhysicalCTAS
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerPhysicalCTAS::CParseHandlerPhysicalCTAS
	(
	CMemoryPool *mp,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root),
	m_mdname_schema(NULL),
	m_mdname(NULL),	
	m_distr_column_pos_array(NULL),
	m_src_colids_array(NULL),
	m_is_temp_table(false),
	m_has_oids(false)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalCTAS::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalCTAS::StartElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const, // element_qname
	const Attributes &attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalCTAS), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	// parse table name
	const XMLCh *table_name_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenName, EdxltokenPhysicalCTAS);
	m_mdname = CDXLUtils::CreateMDNameFromXMLChar(m_parse_handler_mgr->GetDXLMemoryManager(), table_name_xml);
	
	const XMLCh *schema_xml = attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenSchema));
	if (NULL != schema_xml)
	{
		m_mdname_schema = CDXLUtils::CreateMDNameFromXMLChar(m_parse_handler_mgr->GetDXLMemoryManager(), schema_xml);
	}
	
	// parse distribution policy
	const XMLCh *rel_distr_policy_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenRelDistrPolicy, EdxltokenPhysicalCTAS);
	m_rel_distr_policy = CDXLOperatorFactory::ParseRelationDistPolicy(rel_distr_policy_xml);

	if (IMDRelation::EreldistrHash == m_rel_distr_policy)
	{
		// parse distribution columns
		const XMLCh *rel_distr_cols_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenDistrColumns, EdxltokenPhysicalCTAS);
		m_distr_column_pos_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), rel_distr_cols_xml, EdxltokenDistrColumns, EdxltokenPhysicalCTAS);
	}
	
	// parse storage type
	const XMLCh *rel_storage_type_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenRelStorageType, EdxltokenPhysicalCTAS);
	m_rel_storage_type = CDXLOperatorFactory::ParseRelationStorageType(rel_storage_type_xml);

	const XMLCh *src_colids_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenInsertCols, EdxltokenPhysicalCTAS);
	m_src_colids_array = CDXLOperatorFactory::ExtractIntsToUlongArray(m_parse_handler_mgr->GetDXLMemoryManager(), src_colids_xml, EdxltokenInsertCols, EdxltokenPhysicalCTAS);
	
	const XMLCh *vartypemod_xml = CDXLOperatorFactory::ExtractAttrValue(attrs, EdxltokenVarTypeModList, EdxltokenPhysicalCTAS);
	m_vartypemod_array =
			CDXLOperatorFactory::ExtractIntsToIntArray(m_parse_handler_mgr->GetDXLMemoryManager(), vartypemod_xml, EdxltokenVarTypeModList, EdxltokenPhysicalCTAS);

	m_is_temp_table = CDXLOperatorFactory::ExtractConvertAttrValueToBool(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRelTemporary, EdxltokenPhysicalCTAS);
	m_has_oids = CDXLOperatorFactory::ExtractConvertAttrValueToBool(m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenRelHasOids, EdxltokenPhysicalCTAS);

	// create child node parsers

	// parse handler for logical operator
	CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenLogical), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	// parse handler for the proj list
	CParseHandlerBase *proj_list_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);
	
	//parse handler for the storage options
	CParseHandlerBase *ctas_options_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenCTASOptions), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(ctas_options_parse_handler);
	
	//parse handler for the column descriptors
	CParseHandlerBase *col_descr_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenColumns), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(col_descr_parse_handler);
	
	//parse handler for the properties of the operator
	CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	// store child parse handler in array
	this->Append(prop_parse_handler);
	this->Append(col_descr_parse_handler);
	this->Append(ctas_options_parse_handler);
	this->Append(proj_list_parse_handler);	
	this->Append(child_parse_handler);	
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerPhysicalCTAS::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerPhysicalCTAS::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalCTAS), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(5 == this->Length());

	CParseHandlerProperties *prop_parse_handler = dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerColDescr *col_descr_parse_handler = dynamic_cast<CParseHandlerColDescr *>((*this)[1]);
	CParseHandlerCtasStorageOptions *ctas_options_parse_handler = dynamic_cast<CParseHandlerCtasStorageOptions *>((*this)[2]);
	CParseHandlerProjList *proj_list_parse_handler = dynamic_cast<CParseHandlerProjList*>((*this)[3]);
	GPOS_ASSERT(NULL != proj_list_parse_handler->CreateDXLNode());
	CParseHandlerPhysicalOp *child_parse_handler = dynamic_cast<CParseHandlerPhysicalOp*>((*this)[4]);

	GPOS_ASSERT(NULL != prop_parse_handler->GetProperties());
	GPOS_ASSERT(NULL != col_descr_parse_handler->GetDXLColumnDescrArray());
	GPOS_ASSERT(NULL != ctas_options_parse_handler->GetDxlCtasStorageOption());
	GPOS_ASSERT(NULL != proj_list_parse_handler->CreateDXLNode());
	GPOS_ASSERT(NULL != child_parse_handler->CreateDXLNode());
	
	CDXLColDescrArray *dxl_col_descr_array = col_descr_parse_handler->GetDXLColumnDescrArray();
	dxl_col_descr_array->AddRef();
	
	CDXLCtasStorageOptions *ctas_options = ctas_options_parse_handler->GetDxlCtasStorageOption();
	ctas_options->AddRef();
	
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode
							(
							m_mp,
							GPOS_NEW(m_mp) CDXLPhysicalCTAS
									(
									m_mp,
									m_mdname_schema,
									m_mdname,
														  dxl_col_descr_array,
									ctas_options,
									m_rel_distr_policy,
									m_distr_column_pos_array,
									m_is_temp_table,
									m_has_oids,
									m_rel_storage_type,
									m_src_colids_array,
									m_vartypemod_array
									)
							);
	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);
	
	AddChildFromParseHandler(proj_list_parse_handler);
	AddChildFromParseHandler(child_parse_handler);

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
