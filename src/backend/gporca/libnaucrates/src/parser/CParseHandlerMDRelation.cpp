//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMDRelation.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing relation metadata.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDRelation.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerMDIndexInfoList.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataColumns.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelation::CParseHandlerMDRelation
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDRelation::CParseHandlerMDRelation(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
	  m_mdid(nullptr),
	  m_mdname_schema(nullptr),
	  m_mdname(nullptr),
	  m_is_temp_table(false),
	  m_rel_storage_type(IMDRelation::ErelstorageSentinel),
	  m_rel_distr_policy(IMDRelation::EreldistrSentinel),
	  m_distr_col_array(nullptr),
	  m_convert_hash_to_random(false),
	  m_partition_cols_array(nullptr),
	  m_str_part_types_array(nullptr),
	  m_key_sets_arrays(nullptr),
	  m_part_constraint(nullptr),
	  m_opfamilies_parse_handler(nullptr),
	  m_child_partitions_parse_handler(nullptr),
	  m_foreign_server(nullptr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelation::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelation::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPartConstraint),
				 element_local_name))
	{
		GPOS_ASSERT(nullptr == m_part_constraint);

		// parse handler for part constraints
		CParseHandlerBase *pphPartConstraint =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenScalar),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(pphPartConstraint);
		this->Append(pphPartConstraint);

		return;
	}

	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenRelDistrOpfamilies),
				 element_local_name))
	{
		// parse handler for check constraints
		m_opfamilies_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(m_opfamilies_parse_handler);
		this->Append(m_opfamilies_parse_handler);
		m_opfamilies_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);

		return;
	}

	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPartitions),
								 element_local_name))
	{
		// parse handler for child_partitions
		m_child_partitions_parse_handler =
			CParseHandlerFactory::GetParseHandler(
				m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
				m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			m_child_partitions_parse_handler);
		this->Append(m_child_partitions_parse_handler);
		m_child_partitions_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);

		return;
	}

	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenRelation),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// parse main relation attributes: name, id, distribution policy and keys
	ParseRelationAttributes(attrs, EdxltokenRelation);

	// parse whether relation is temporary
	m_is_temp_table = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenRelTemporary, EdxltokenRelation);

	// parse storage type
	const XMLCh *xmlszStorageType = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenRelStorageType, EdxltokenRelation);

	m_rel_storage_type =
		CDXLOperatorFactory::ParseRelationStorageType(xmlszStorageType);

	const XMLCh *xmlszPartColumns =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenPartKeys));

	if (nullptr != xmlszPartColumns)
	{
		m_partition_cols_array = CDXLOperatorFactory::ExtractIntsToUlongArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xmlszPartColumns,
			EdxltokenPartKeys, EdxltokenRelation);
	}

	const XMLCh *xmlszPartTypes =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenPartTypes));

	if (nullptr != xmlszPartTypes)
	{
		m_str_part_types_array =
			CDXLOperatorFactory::ExtractConvertPartitionTypeToArray(
				m_parse_handler_mgr->GetDXLMemoryManager(), xmlszPartTypes,
				EdxltokenPartTypes, EdxltokenRelation);
	}

	// parse whether a hash distributed relation needs to be considered as random distributed
	const XMLCh *xmlszConvertHashToRandom =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenConvertHashToRandom));
	if (nullptr != xmlszConvertHashToRandom)
	{
		m_convert_hash_to_random = CDXLOperatorFactory::ConvertAttrValueToBool(
			m_parse_handler_mgr->GetDXLMemoryManager(),
			xmlszConvertHashToRandom, EdxltokenConvertHashToRandom,
			EdxltokenRelation);
	}

	// parse children
	ParseChildNodes();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelation::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelation::EndElement(const XMLCh *const,	 // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const	// element_qname
)
{
	CParseHandlerMDIndexInfoList *pphMdlIndexInfo =
		dynamic_cast<CParseHandlerMDIndexInfoList *>((*this)[1]);
	if (0 == XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPartConstraint),
				 element_local_name))
	{
		CParseHandlerScalarOp *pphPartCnstr =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[Length() - 1]);
		m_part_constraint = pphPartCnstr->CreateDXLNode();
		m_part_constraint->AddRef();
		return;
	}

	if (0 !=
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenRelation),
								 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// construct metadata object from the created child elements
	CParseHandlerMetadataColumns *md_cols_parse_handler =
		dynamic_cast<CParseHandlerMetadataColumns *>((*this)[0]);
	CParseHandlerMetadataIdList *pphMdidlCheckConstraints =
		dynamic_cast<CParseHandlerMetadataIdList *>((*this)[2]);

	GPOS_ASSERT(nullptr != md_cols_parse_handler->GetMdColArray());
	GPOS_ASSERT(nullptr != pphMdlIndexInfo->GetMdIndexInfoArray());
	GPOS_ASSERT(nullptr != pphMdidlCheckConstraints->GetMdIdArray());

	// refcount child objects
	CMDColumnArray *md_col_array = md_cols_parse_handler->GetMdColArray();
	CMDIndexInfoArray *md_index_info_array =
		pphMdlIndexInfo->GetMdIndexInfoArray();
	IMdIdArray *mdid_check_constraint_array =
		pphMdidlCheckConstraints->GetMdIdArray();

	md_col_array->AddRef();
	md_index_info_array->AddRef();
	mdid_check_constraint_array->AddRef();

	IMdIdArray *distr_opfamilies = nullptr;
	if (m_rel_distr_policy == IMDRelation::EreldistrHash &&
		m_opfamilies_parse_handler != nullptr)
	{
		distr_opfamilies = dynamic_cast<CParseHandlerMetadataIdList *>(
							   m_opfamilies_parse_handler)
							   ->GetMdIdArray();
		distr_opfamilies->AddRef();
	}

	IMdIdArray *child_partitions = nullptr;
	if (nullptr != m_child_partitions_parse_handler)
	{
		child_partitions = dynamic_cast<CParseHandlerMetadataIdList *>(
							   m_child_partitions_parse_handler)
							   ->GetMdIdArray();
		child_partitions->AddRef();
	}

	m_imd_obj = GPOS_NEW(m_mp) CMDRelationGPDB(
		m_mp, m_mdid, m_mdname, m_is_temp_table, m_rel_storage_type,
		m_rel_distr_policy, md_col_array, m_distr_col_array, distr_opfamilies,
		m_partition_cols_array, m_str_part_types_array, child_partitions,
		m_convert_hash_to_random, m_key_sets_arrays, md_index_info_array,
		mdid_check_constraint_array, m_part_constraint, m_foreign_server);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelation::ParseRelationAttributes
//
//	@doc:
//		Helper function to parse relation attributes: name, id,
//		distribution policy and keys
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelation::ParseRelationAttributes(const Attributes &attrs,
												 Edxltoken dxl_token_element)
{
	// parse table name
	const XMLCh *xml_str_table_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, dxl_token_element);
	CWStringDynamic *str_table_name =
		CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_table_name);
	m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, str_table_name);
	GPOS_DELETE(str_table_name);

	// parse metadata id info
	m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		dxl_token_element);

	// parse distribution policy
	const XMLCh *rel_distr_policy_xml = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenRelDistrPolicy, dxl_token_element);
	m_rel_distr_policy =
		CDXLOperatorFactory::ParseRelationDistPolicy(rel_distr_policy_xml);

	if (m_rel_distr_policy == IMDRelation::EreldistrHash)
	{
		// parse distribution columns
		const XMLCh *rel_distr_cols_xml = CDXLOperatorFactory::ExtractAttrValue(
			attrs, EdxltokenDistrColumns, dxl_token_element);
		m_distr_col_array = CDXLOperatorFactory::ExtractIntsToUlongArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), rel_distr_cols_xml,
			EdxltokenDistrColumns, dxl_token_element);
	}

	// parse keys
	const XMLCh *xml_str_keys =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenKeys));
	if (nullptr != xml_str_keys)
	{
		m_key_sets_arrays = CDXLOperatorFactory::ExtractConvertUlongTo2DArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), xml_str_keys,
			EdxltokenKeys, dxl_token_element);
	}
	else
	{
		// construct an empty keyset
		m_key_sets_arrays = GPOS_NEW(m_mp) ULongPtr2dArray(m_mp);
	}

	const XMLCh *xmlszForeignServer =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenRelForeignServer));
	if (nullptr != xmlszForeignServer)
	{
		m_foreign_server = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
			EdxltokenRelForeignServer, dxl_token_element);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDRelation::ParseChildNodes
//
//	@doc:
//		Create and activate the parse handler for the children nodes
//
//---------------------------------------------------------------------------
void
CParseHandlerMDRelation::ParseChildNodes()
{
	// parse handler for check constraints
	CParseHandlerBase *check_constraint_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(
		check_constraint_list_parse_handler);

	// parse handler for index info list
	CParseHandlerBase *index_info_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenIndexInfoList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(index_info_list_parse_handler);

	// parse handler for the columns
	CParseHandlerBase *columns_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataColumns),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(columns_parse_handler);

	// store parse handlers
	this->Append(columns_parse_handler);
	this->Append(index_info_list_parse_handler);
	this->Append(check_constraint_list_parse_handler);
}


// EOF
