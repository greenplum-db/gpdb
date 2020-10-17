//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CDXLScalarMinMax.cpp
//
//	@doc:
//		Implementation of DXL MinMax
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarMinMax.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::CDXLScalarMinMax
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarMinMax::CDXLScalarMinMax(CMemoryPool *mp, IMDId *mdid_type,
								   EdxlMinMaxType min_max_type)
	: CDXLScalar(mp), m_mdid_type(mdid_type), m_min_max_type(min_max_type)
{
	GPOS_ASSERT(m_mdid_type->IsValid());
	GPOS_ASSERT(EmmtSentinel > min_max_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::~CDXLScalarMinMax
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarMinMax::~CDXLScalarMinMax()
{
	m_mdid_type->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarMinMax::GetDXLOperator() const
{
	return EdxlopScalarMinMax;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarMinMax::GetOpNameStr() const
{
	switch (m_min_max_type)
	{
		case EmmtMin:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarMin);
		case EmmtMax:
			return CDXLTokens::GetDXLTokenStr(EdxltokenScalarMax);
		default:
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarMinMax::SerializeToDXL(CXMLSerializer *xml_serializer,
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
//		CDXLScalarMinMax::HasBoolResult
//
//	@doc:
//		Does the operator return a boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarMinMax::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_mdid_type)->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarMinMax::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarMinMax::AssertValid(const CDXLNode *dxlnode,
							  BOOL validate_children) const
{
	GPOS_ASSERT(0 < dxlnode->Arity());

	const ULONG arity = dxlnode->Arity();
	for (ULONG idx = 0; idx < arity; ++idx)
	{
		CDXLNode *dxlnode_arg = (*dxlnode)[idx];
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
