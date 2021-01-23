//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalTVF.cpp
//
//	@doc:
//		Implementation of DXL physical table-valued function
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalTVF.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::CDXLPhysicalTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalTVF::CDXLPhysicalTVF(CMemoryPool *mp, IMDId *mdid_func,
								 IMDId *mdid_return_type, CWStringConst *str)
	: CDXLPhysical(mp),
	  m_func_mdid(mdid_func),
	  m_return_type_mdid(mdid_return_type),
	  func_name(str)
{
	GPOS_ASSERT(NULL != m_func_mdid);
	GPOS_ASSERT(m_func_mdid->IsValid());
	GPOS_ASSERT(NULL != m_return_type_mdid);
	GPOS_ASSERT(m_return_type_mdid->IsValid());
	GPOS_ASSERT(NULL != func_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::~CDXLPhysicalTVF
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalTVF::~CDXLPhysicalTVF()
{
	m_func_mdid->Release();
	m_return_type_mdid->Release();
	GPOS_DELETE(func_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalTVF::GetDXLOperator() const
{
	return EdxlopPhysicalTVF;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalTVF::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalTVF);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalTVF::SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_func_mdid->Serialize(xml_serializer,
						   CDXLTokens::GetDXLTokenStr(EdxltokenFuncId));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 func_name);
	m_return_type_mdid->Serialize(xml_serializer,
								  CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTVF::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalTVF::AssertValid(const CDXLNode *dxlnode,
							 BOOL validate_children) const
{
	// assert validity of function id and return type
	GPOS_ASSERT(NULL != m_func_mdid);
	GPOS_ASSERT(NULL != m_return_type_mdid);

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
