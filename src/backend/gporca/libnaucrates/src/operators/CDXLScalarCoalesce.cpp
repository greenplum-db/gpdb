//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarCoalesce.cpp
//
//	@doc:
//		Implementation of DXL Coalesce
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarCoalesce.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::CDXLScalarCoalesce
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarCoalesce::CDXLScalarCoalesce(CMemoryPool *mp, IMDId *mdid_type)
	: CDXLScalar(mp), m_mdid_type(mdid_type)
{
	GPOS_ASSERT(m_mdid_type->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::~CDXLScalarCoalesce
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarCoalesce::~CDXLScalarCoalesce()
{
	m_mdid_type->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarCoalesce::GetDXLOperator() const
{
	return EdxlopScalarCoalesce;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarCoalesce::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarCoalesce);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarCoalesce::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarCoalesce::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_mdid_type)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarCoalesce::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarCoalesce::AssertValid(const CDXLNode *node,
								BOOL validate_children) const
{
	GPOS_ASSERT(0 < node->Arity());

	const ULONG arity = node->Arity();
	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *child_dxlnode = (*node)[ul];
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
