//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarSwitch.cpp
//
//	@doc:
//		Implementation of DXL Switch
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarSwitch.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::CDXLScalarSwitch
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarSwitch::CDXLScalarSwitch(CMemoryPool *mp, IMDId *mdid_type)
	: CDXLScalar(mp), m_mdid_type(mdid_type)
{
	GPOS_ASSERT(m_mdid_type->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::~CDXLScalarSwitch
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarSwitch::~CDXLScalarSwitch()
{
	m_mdid_type->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarSwitch::GetDXLOperator() const
{
	return EdxlopScalarSwitch;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarSwitch::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSwitch);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::MdidType
//
//	@doc:
//		Return type id
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarSwitch::MdidType() const
{
	return m_mdid_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarSwitch::SerializeToDXL(CXMLSerializer *xml_serializer,
								 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_mdid_type->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	dxlnode->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarSwitch::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_mdid_type)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSwitch::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarSwitch::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	const ULONG arity = dxlnode->Arity();
	GPOS_ASSERT(1 < arity);

	for (ULONG ul = 0; ul < arity; ++ul)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[ul];
		GPOS_ASSERT(EdxloptypeScalar ==
					dxlnode_arg->GetOperator()->GetDXLOperatorType());

		if (validate_children)
		{
			dxlnode_arg->GetOperator()->AssertValid(dxlnode_arg,
													validate_children);
		}
	}
}
#endif	// GPOS_DEBUG

// EOF
