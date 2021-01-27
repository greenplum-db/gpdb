//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarArray.cpp
//
//	@doc:
//		Implementation of DXL arrays
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarArray.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::CDXLScalarArray
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarArray::CDXLScalarArray(CMemoryPool *mp, IMDId *elem_type_mdid,
								 IMDId *array_type_mdid,
								 BOOL multi_dimensional_array)
	: CDXLScalar(mp),
	  m_elem_type_mdid(elem_type_mdid),
	  m_array_type_mdid(array_type_mdid),
	  m_multi_dimensional_array(multi_dimensional_array)
{
	GPOS_ASSERT(m_elem_type_mdid->IsValid());
	GPOS_ASSERT(m_array_type_mdid->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::~CDXLScalarArray
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarArray::~CDXLScalarArray()
{
	m_elem_type_mdid->Release();
	m_array_type_mdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarArray::GetDXLOperator() const
{
	return EdxlopScalarArray;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarArray::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarArray);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::PmdidElem
//
//	@doc:
//		Id of base element type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarArray::ElementTypeMDid() const
{
	return m_elem_type_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::PmdidArray
//
//	@doc:
//		Id of array type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarArray::ArrayTypeMDid() const
{
	return m_array_type_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::FMultiDimensional
//
//	@doc:
//		Is this a multi-dimensional array
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarArray::IsMultiDimensional() const
{
	return m_multi_dimensional_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarArray::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_array_type_mdid->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenArrayType));
	m_elem_type_mdid->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenArrayElementType));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenArrayMultiDim),
		m_multi_dimensional_array);

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarArray::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarArray::AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *child_dxlnode = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					child_dxlnode->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
													  validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
