//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLLogicalProject.cpp
//
//	@doc:
//		Implementation of DXL logical project operator
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLLogicalProject.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::CDXLLogicalProject
//
//	@doc:
//		Construct a DXL Logical project node
//
//---------------------------------------------------------------------------
CDXLLogicalProject::CDXLLogicalProject(CMemoryPool *mp)
	: CDXLLogical(mp), m_mdname_alias(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLLogicalProject::GetDXLOperator() const
{
	return EdxlopLogicalProject;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::MdName
//
//	@doc:
//		Returns alias name
//
//---------------------------------------------------------------------------
const CMDName *
CDXLLogicalProject::MdName() const
{
	return m_mdname_alias;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::SetAliasName
//
//	@doc:
//		Set alias name
//
//---------------------------------------------------------------------------
void
CDXLLogicalProject::SetAliasName(CMDName *mdname)
{
	GPOS_ASSERT(NULL == m_mdname_alias);
	GPOS_ASSERT(NULL != mdname);

	m_mdname_alias = mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLLogicalProject::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenLogicalProject);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLLogicalProject::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize alias
	if (NULL != m_mdname_alias)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDerivedTableName),
			m_mdname_alias->GetMDName());
	}

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLLogicalProject::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLLogicalProject::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());

	CDXLNode *proj_list_dxlnode = (*dxlnode)[0];
	CDXLNode *child_dxlnode = (*dxlnode)[1];

	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list_dxlnode->GetOperator()->GetDXLOperator());
	GPOS_ASSERT(EdxloptypeLogical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		proj_list_dxlnode->GetOperator()->AssertValid(proj_list_dxlnode,
													  validate_children);
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}

	const ULONG arity = proj_list_dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *pdxlnPrEl = (*proj_list_dxlnode)[ul];
		GPOS_ASSERT(EdxlopScalarIdent !=
					pdxlnPrEl->GetOperator()->GetDXLOperator());
	}
}
#endif	// GPOS_DEBUG

// EOF
