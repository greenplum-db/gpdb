//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLScalarBitmapBoolOp.cpp
//
//	@doc:
//		Implementation of DXL bitmap bool operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarBitmapBoolOp.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/md/IMDType.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::CDXLScalarBitmapBoolOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarBitmapBoolOp::CDXLScalarBitmapBoolOp(CMemoryPool *mp,
											   IMDId *mdid_type,
											   EdxlBitmapBoolOp bitmap_op_type)
	: CDXLScalar(mp), m_mdid_type(mdid_type), m_bitmap_op_type(bitmap_op_type)
{
	GPOS_ASSERT(EdxlbitmapSentinel > bitmap_op_type);
	GPOS_ASSERT(IMDId::IsValid(mdid_type));
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::~CDXLScalarBitmapBoolOp
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarBitmapBoolOp::~CDXLScalarBitmapBoolOp()
{
	m_mdid_type->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarBitmapBoolOp::GetDXLOperator() const
{
	return EdxlopScalarBitmapBoolOp;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::MdidType
//
//	@doc:
//		Return type
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarBitmapBoolOp::MdidType() const
{
	return m_mdid_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::EdxlBitmapBoolOp
//
//	@doc:
//		Bitmap bool type
//
//---------------------------------------------------------------------------
CDXLScalarBitmapBoolOp::EdxlBitmapBoolOp
CDXLScalarBitmapBoolOp::GetDXLBitmapOpType() const
{
	return m_bitmap_op_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::HasBoolResult
//
//	@doc:
//		Is operator returning a boolean value
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarBitmapBoolOp::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_mdid_type)->GetDatumType());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarBitmapBoolOp::GetOpNameStr() const
{
	if (EdxlbitmapAnd == m_bitmap_op_type)
	{
		return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBitmapAnd);
	}

	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBitmapOr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarBitmapBoolOp::SerializeToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *dxlnode) const
{
	GPOS_CHECK_ABORT;

	const CWStringConst *element_name = GetOpNameStr();

	GPOS_ASSERT(NULL != element_name);
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	GPOS_CHECK_ABORT;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapBoolOp::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarBitmapBoolOp::AssertValid(const CDXLNode *dxlnode,
									BOOL validate_children) const
{
	EdxlBitmapBoolOp bitmap_bool_dxlop =
		((CDXLScalarBitmapBoolOp *) dxlnode->GetOperator())
			->GetDXLBitmapOpType();

	GPOS_ASSERT((bitmap_bool_dxlop == EdxlbitmapAnd) ||
				(bitmap_bool_dxlop == EdxlbitmapOr));

	ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(2 == arity);


	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[ul];
		Edxlopid dxl_operator = dxlnode_arg->GetOperator()->GetDXLOperator();

		GPOS_ASSERT(EdxlopScalarBitmapBoolOp == dxl_operator ||
					EdxlopScalarBitmapIndexProbe == dxl_operator);

		if (validate_children)
		{
			dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg,
													validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
