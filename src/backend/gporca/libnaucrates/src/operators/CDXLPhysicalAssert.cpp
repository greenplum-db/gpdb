//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalAssert.cpp
//
//	@doc:
//		Implementation of DXL physical assert operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalAssert.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::CDXLPhysicalAssert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalAssert::CDXLPhysicalAssert(CMemoryPool *mp, const CHAR *sql_state)
	: CDXLPhysical(mp)
{
	GPOS_ASSERT(NULL != sql_state);
	GPOS_ASSERT(GPOS_SQLSTATE_LENGTH == clib::Strlen(sql_state));
	clib::Strncpy(m_sql_state, sql_state, GPOS_SQLSTATE_LENGTH);
	m_sql_state[GPOS_SQLSTATE_LENGTH] = '\0';
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::~CDXLPhysicalAssert
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalAssert::~CDXLPhysicalAssert()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalAssert::GetDXLOperator() const
{
	return EdxlopPhysicalAssert;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalAssert::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalAssert);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalAssert::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenErrorCode),
								 m_sql_state);

	dxlnode->SerializePropertiesToDXL(xml_serializer);
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalAssert::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalAssert::AssertValid(const CDXLNode *dxlnode,
								BOOL validate_children) const
{
	GPOS_ASSERT(3 == dxlnode->Arity());

	CDXLNode *proj_list_dxlnode = (*dxlnode)[EdxlassertIndexProjList];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				proj_list_dxlnode->GetOperator()->GetDXLOperator());

	CDXLNode *predicate_dxlnode = (*dxlnode)[EdxlassertIndexFilter];
	GPOS_ASSERT(EdxlopScalarAssertConstraintList ==
				predicate_dxlnode->GetOperator()->GetDXLOperator());

	CDXLNode *physical_child_dxlnode = (*dxlnode)[EdxlassertIndexChild];
	GPOS_ASSERT(EdxloptypePhysical ==
				physical_child_dxlnode->GetOperator()->GetDXLOperatorType());


	if (validate_children)
	{
		for (ULONG ul = 0; ul < 3; ul++)
		{
			CDXLNode *child_dxlnode = (*dxlnode)[ul];
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
