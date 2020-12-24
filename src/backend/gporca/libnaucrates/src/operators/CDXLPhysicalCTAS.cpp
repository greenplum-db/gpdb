//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalCTAS.cpp
//
//	@doc:
//		Implementation of DXL physical CTAS operator
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLPhysicalCTAS.h"
#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::CDXLPhysicalCTAS
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalCTAS::CDXLPhysicalCTAS(
	CMemoryPool *mp, CMDName *mdname_schema, CMDName *mdname_rel,
	CDXLColDescrArray *dxl_col_descr_array,
	CDXLCtasStorageOptions *dxl_ctas_opt,
	IMDRelation::Ereldistrpolicy rel_distr_policy,
	ULongPtrArray *distr_column_pos_array, IMdIdArray *distr_opclasses,
	BOOL is_temporary, BOOL has_oids,
	IMDRelation::Erelstoragetype rel_storage_type,
	ULongPtrArray *src_colids_array, IntPtrArray *vartypemod_array)
	: CDXLPhysical(mp),
	  m_mdname_schema(mdname_schema),
	  m_mdname_rel(mdname_rel),
	  m_col_descr_array(dxl_col_descr_array),
	  m_dxl_ctas_storage_option(dxl_ctas_opt),
	  m_rel_distr_policy(rel_distr_policy),
	  m_distr_column_pos_array(distr_column_pos_array),
	  m_distr_opclasses(distr_opclasses),
	  m_is_temp_table(is_temporary),
	  m_has_oids(has_oids),
	  m_rel_storage_type(rel_storage_type),
	  m_src_colids_array(src_colids_array),
	  m_vartypemod_array(vartypemod_array)
{
	GPOS_ASSERT(NULL != mdname_rel);
	GPOS_ASSERT(NULL != dxl_col_descr_array);
	GPOS_ASSERT(NULL != dxl_ctas_opt);
	GPOS_ASSERT_IFF(IMDRelation::EreldistrHash == rel_distr_policy,
					NULL != distr_column_pos_array);
	GPOS_ASSERT(NULL != src_colids_array);
	GPOS_ASSERT(NULL != vartypemod_array);
	GPOS_ASSERT(dxl_col_descr_array->Size() == vartypemod_array->Size());
	GPOS_ASSERT(IMDRelation::ErelstorageSentinel > rel_storage_type);
	GPOS_ASSERT(IMDRelation::EreldistrSentinel > rel_distr_policy);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::~CDXLPhysicalCTAS
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalCTAS::~CDXLPhysicalCTAS()
{
	GPOS_DELETE(m_mdname_schema);
	GPOS_DELETE(m_mdname_rel);
	m_col_descr_array->Release();
	m_dxl_ctas_storage_option->Release();
	CRefCount::SafeRelease(m_distr_column_pos_array);
	m_src_colids_array->Release();
	m_vartypemod_array->Release();
	m_distr_opclasses->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalCTAS::GetDXLOperator() const
{
	return EdxlopPhysicalCTAS;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalCTAS::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalCTAS);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalCTAS::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
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
	CWStringDynamic *str_input_cols =
		CDXLUtils::Serialize(m_mp, m_src_colids_array);
	GPOS_ASSERT(NULL != str_input_cols);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenInsertCols), str_input_cols);
	GPOS_DELETE(str_input_cols);

	// serialize vartypmod list
	CWStringDynamic *str_vartypmod_list =
		CDXLUtils::Serialize(m_mp, m_vartypemod_array);
	GPOS_ASSERT(NULL != str_vartypmod_list);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenVarTypeModList),
		str_vartypmod_list);
	GPOS_DELETE(str_vartypmod_list);

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize opclasses list

	IMDCacheObject::SerializeMDIdList(
		xml_serializer, m_distr_opclasses,
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpclasses),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpclass));

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

	// serialize arguments
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTAS::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalCTAS::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());

	CDXLNode *proj_list_dxlnode = (*dxlnode)[0];
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list_dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
