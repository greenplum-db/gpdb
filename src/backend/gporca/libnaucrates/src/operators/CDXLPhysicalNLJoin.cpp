//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalNLJoin.cpp
//
//	@doc:
//		Implementation of DXL physical nested loop join operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalNLJoin.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::CDXLPhysicalNLJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalNLJoin::CDXLPhysicalNLJoin(CMemoryPool *mp, EdxlJoinType join_type,
									   BOOL is_index_nlj,
									   BOOL nest_params_exists)
	: CDXLPhysicalJoin(mp, join_type),
	  m_is_index_nlj(is_index_nlj),
	  m_nest_params_exists(nest_params_exists)
{
	m_nest_params_col_refs = NULL;
}

CDXLPhysicalNLJoin::~CDXLPhysicalNLJoin()
{
	CRefCount::SafeRelease(m_nest_params_col_refs);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalNLJoin::GetDXLOperator() const
{
	return EdxlopPhysicalNLJoin;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalNLJoin::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalNLJoin);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalNLJoin::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenJoinType),
								 GetJoinTypeNameStr());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalNLJoinIndex),
		m_is_index_nlj);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenNLJIndexOuterRefAsParam),
		m_nest_params_exists);


	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	// serialize nestloop params
	SerializeNestLoopParamsToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

void
CDXLPhysicalNLJoin::SerializeNestLoopParamsToDXL(
	CXMLSerializer *xml_serializer) const
{
	if (!m_nest_params_exists)
	{
		return;
	}

	// Serialize NLJ index paramlist
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenNLJIndexParamList));

	for (ULONG ul = 0; ul < m_nest_params_col_refs->Size(); ul++)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenNLJIndexParam));

		ULONG id = (*m_nest_params_col_refs)[ul]->Id();
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
									 id);

		const CMDName *md_name = (*m_nest_params_col_refs)[ul]->MdName();
		const IMDId *mdid_type = (*m_nest_params_col_refs)[ul]->MdidType();
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenColName), md_name->GetMDName());
		mdid_type->Serialize(xml_serializer,
							 CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenNLJIndexParam));
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenNLJIndexParamList));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalNLJoin::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	GPOS_ASSERT(EdxlnljIndexSentinel == dxlnode->Arity());
	GPOS_ASSERT(EdxljtSentinel > GetJoinType());

	CDXLNode *dxlnode_join_filter = (*dxlnode)[EdxlnljIndexJoinFilter];
	CDXLNode *dxlnode_left = (*dxlnode)[EdxlnljIndexLeftChild];
	CDXLNode *dxlnode_right = (*dxlnode)[EdxlnljIndexRightChild];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarJoinFilter ==
				dxlnode_join_filter->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxloptypePhysical ==
				dxlnode_left->GetOperator()->GetDXLOperatorType());
	GPOS_ASSERT(EdxloptypePhysical ==
				dxlnode_right->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		dxlnode_join_filter->GetOperator()->AssertValid(dxlnode_join_filter,
														validate_children);
		dxlnode_left->GetOperator()->AssertValid(dxlnode_left,
												 validate_children);
		dxlnode_right->GetOperator()->AssertValid(dxlnode_right,
												  validate_children);
	}
}
#endif	// GPOS_DEBUG

void
CDXLPhysicalNLJoin::SetNestLoopParamsColRefs(
	CDXLColRefArray *nest_params_col_refs)
{
	m_nest_params_col_refs = nest_params_col_refs;
}

BOOL
CDXLPhysicalNLJoin::NestParamsExists() const
{
	return m_nest_params_exists;
}

CDXLColRefArray *
CDXLPhysicalNLJoin::GetNestLoopParamsColRefs() const
{
	return m_nest_params_col_refs;
}
// EOF
