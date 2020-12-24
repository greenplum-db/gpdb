//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLLogicalCTAS.cpp
//
//	@doc:
//		Implementation of DXL logical "CREATE TABLE AS" (CTAS) operator
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLLogicalCTAS.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::CDXLLogicalCTAS
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLLogicalCTAS::CDXLLogicalCTAS(
	CMemoryPool *mp, IMDId *mdid, CMDName *mdname_schema, CMDName *mdname_rel,
	CDXLColDescrArray *dxl_col_descr_array,
	CDXLCtasStorageOptions *dxl_ctas_storage_options,
	IMDRelation::Ereldistrpolicy rel_distr_policy,
	ULongPtrArray *distr_column_pos_array, IMdIdArray *distr_opfamilies,
	IMdIdArray *distr_opclasses, BOOL is_temporary, BOOL has_oids,
	IMDRelation::Erelstoragetype rel_storage_type,
	ULongPtrArray *src_colids_array, IntPtrArray *vartypemod_array)
	: CDXLLogical(mp),
	  m_mdid(mdid),
	  m_mdname_schema(mdname_schema),
	  m_mdname_rel(mdname_rel),
	  m_col_descr_array(dxl_col_descr_array),
	  m_dxl_ctas_storage_option(dxl_ctas_storage_options),
	  m_rel_distr_policy(rel_distr_policy),
	  m_distr_column_pos_array(distr_column_pos_array),
	  m_distr_opfamilies(distr_opfamilies),
	  m_distr_opclasses(distr_opclasses),
	  m_is_temp_table(is_temporary),
	  m_has_oids(has_oids),
	  m_rel_storage_type(rel_storage_type),
	  m_src_colids_array(src_colids_array),
	  m_vartypemod_array(vartypemod_array)
{
	GPOS_ASSERT(NULL != mdid && mdid->IsValid());
	GPOS_ASSERT(NULL != mdname_rel);
	GPOS_ASSERT(NULL != dxl_col_descr_array);
	GPOS_ASSERT(NULL != dxl_ctas_storage_options);
	GPOS_ASSERT_IFF(IMDRelation::EreldistrHash == rel_distr_policy,
					NULL != distr_column_pos_array);
	GPOS_ASSERT(NULL != src_colids_array);
	GPOS_ASSERT(NULL != vartypemod_array);
	GPOS_ASSERT(dxl_col_descr_array->Size() == vartypemod_array->Size());
	GPOS_ASSERT(IMDRelation::ErelstorageSentinel > rel_storage_type);
	GPOS_ASSERT(IMDRelation::EreldistrSentinel > rel_distr_policy);
	GPOS_ASSERT(NULL == m_distr_column_pos_array ||
				m_distr_opfamilies->Size() == m_distr_column_pos_array->Size());
	GPOS_ASSERT(NULL == m_distr_column_pos_array ||
				m_distr_opclasses->Size() == m_distr_column_pos_array->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::~CDXLLogicalCTAS
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLLogicalCTAS::~CDXLLogicalCTAS()
{
	m_mdid->Release();
	GPOS_DELETE(m_mdname_schema);
	GPOS_DELETE(m_mdname_rel);
	m_col_descr_array->Release();
	m_dxl_ctas_storage_option->Release();
	CRefCount::SafeRelease(m_distr_column_pos_array);
	m_src_colids_array->Release();
	m_vartypemod_array->Release();
	m_distr_opfamilies->Release();
	m_distr_opclasses->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalCTAS::GetDXLOperator() const
{
	return EdxlopLogicalCTAS;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalCTAS::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalCTAS);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::IsColDefined
//
//	@doc:
//		Check if given column is defined by operator
//
//---------------------------------------------------------------------------
BOOL
CDXLLogicalCTAS::IsColDefined(ULONG colid) const
{
	const ULONG size = m_col_descr_array->Size();
	for (ULONG idx = 0; idx < size; idx++)
	{
		ULONG id = (*m_col_descr_array)[idx]->Id();
		if (id == colid)
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalCTAS::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	if (NULL != m_mdname_schema)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenSchema),
			m_mdname_schema->GetMDName());
	}
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname_rel->GetMDName());

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelTemporary), m_is_temp_table);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelHasOids), m_has_oids);

	GPOS_ASSERT(NULL != IMDRelation::GetStorageTypeStr(m_rel_storage_type));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelStorageType),
		IMDRelation::GetStorageTypeStr(m_rel_storage_type));

	// serialize distribution columns
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrPolicy),
		IMDRelation::GetDistrPolicyStr(m_rel_distr_policy));

	if (IMDRelation::EreldistrHash == m_rel_distr_policy)
	{
		GPOS_ASSERT(NULL != m_distr_column_pos_array);

		// serialize distribution columns
		CWStringDynamic *str_distribution_columns =
			CDXLUtils::Serialize(m_mp, m_distr_column_pos_array);
		GPOS_ASSERT(NULL != str_distribution_columns);

		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDistrColumns),
			str_distribution_columns);
		GPOS_DELETE(str_distribution_columns);
	}

	// serialize input columns
	CWStringDynamic *str_input_columns =
		CDXLUtils::Serialize(m_mp, m_src_colids_array);
	GPOS_ASSERT(NULL != str_input_columns);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInsertCols), str_input_columns);
	GPOS_DELETE(str_input_columns);

	// serialize vartypmod list
	CWStringDynamic *str_vartypemod_list =
		CDXLUtils::Serialize(m_mp, m_vartypemod_array);
	GPOS_ASSERT(NULL != str_vartypemod_list);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenVarTypeModList),
		str_vartypemod_list);
	GPOS_DELETE(str_vartypemod_list);

	// serialize column descriptors
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	const ULONG arity = m_col_descr_array->Size();
	for (ULONG idx = 0; idx < arity; idx++)
	{
		CDXLColDescr *dxl_col_descr = (*m_col_descr_array)[idx];
		dxl_col_descr->SerializeToDXL(xml_serializer);
	}
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	m_dxl_ctas_storage_option->Serialize(xml_serializer);



	IMDCacheObject::SerializeMDIdList(
		xml_serializer, m_distr_opfamilies,
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpfamilies),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpfamily));

	IMDCacheObject::SerializeMDIdList(
		xml_serializer, m_distr_opclasses,
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpclasses),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpclass));

	// serialize arguments
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalCTAS::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalCTAS::AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const
{
	GPOS_ASSERT(1 == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[0];
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG


// EOF
