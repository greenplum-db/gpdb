//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalCTEConsumer.cpp
//
//	@doc:
//		Implementation of DXL physical CTE Consumer operator
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLPhysicalCTEConsumer.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::CDXLPhysicalCTEConsumer
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalCTEConsumer::CDXLPhysicalCTEConsumer(
	CMemoryPool *mp, ULONG id, ULongPtrArray *output_colids_array)
	: CDXLPhysical(mp), m_id(id), m_output_colids_array(output_colids_array)
{
	GPOS_ASSERT(NULL != output_colids_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::~CDXLPhysicalCTEConsumer
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalCTEConsumer::~CDXLPhysicalCTEConsumer()
{
	m_output_colids_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalCTEConsumer::GetDXLOperator() const
{
	return EdxlopPhysicalCTEConsumer;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalCTEConsumer::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalCTEConsumer);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalCTEConsumer::SerializeToDXL(CXMLSerializer *xml_serializer,
										const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenCTEId),
								 Id());

	CWStringDynamic *str_colids =
		CDXLUtils::Serialize(m_mp, m_output_colids_array);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColumns),
								 str_colids);
	GPOS_DELETE(str_colids);

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalCTEConsumer::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalCTEConsumer::AssertValid(const CDXLNode *dxlnode,
									 BOOL validate_children) const
{
	GPOS_ASSERT(1 == dxlnode->Arity());

	CDXLNode *dxlnode_proj_list = (*dxlnode)[0];
	GPOS_ASSERT(EdxlopScalarProjectList ==
				dxlnode_proj_list->GetOperator()->GetDXLOperator());

	if (validate_children)
	{
		dxlnode_proj_list->GetOperator()->AssertValid(dxlnode_proj_list,
													  validate_children);
	}
}
#endif	// GPOS_DEBUG

// EOF
