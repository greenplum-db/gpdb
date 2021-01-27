//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalRowTrigger.cpp
//
//	@doc:
//		Implementation of DXL physical row trigger operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalRowTrigger.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::CDXLPhysicalRowTrigger
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalRowTrigger::CDXLPhysicalRowTrigger(CMemoryPool *mp, IMDId *rel_mdid,
											   INT type,
											   ULongPtrArray *colids_old,
											   ULongPtrArray *colids_new)
	: CDXLPhysical(mp),
	  m_rel_mdid(rel_mdid),
	  m_type(type),
	  m_colids_old(colids_old),
	  m_colids_new(colids_new)
{
	GPOS_ASSERT(rel_mdid->IsValid());
	GPOS_ASSERT(0 != type);
	GPOS_ASSERT(NULL != colids_new || NULL != colids_old);
	GPOS_ASSERT_IMP(NULL != colids_new && NULL != colids_old,
					colids_new->Size() == colids_old->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::~CDXLPhysicalRowTrigger
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalRowTrigger::~CDXLPhysicalRowTrigger()
{
	m_rel_mdid->Release();
	CRefCount::SafeRelease(m_colids_old);
	CRefCount::SafeRelease(m_colids_new);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalRowTrigger::GetDXLOperator() const
{
	return EdxlopPhysicalRowTrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalRowTrigger::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalRowTrigger);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRowTrigger::SerializeToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_rel_mdid->Serialize(xml_serializer,
						  CDXLTokens::GetDXLTokenStr(EdxltokenRelationMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenMDType),
								 m_type);

	if (NULL != m_colids_old)
	{
		CWStringDynamic *pstrColsOld = CDXLUtils::Serialize(m_mp, m_colids_old);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenOldCols), pstrColsOld);
		GPOS_DELETE(pstrColsOld);
	}

	if (NULL != m_colids_new)
	{
		CWStringDynamic *pstrColsNew = CDXLUtils::Serialize(m_mp, m_colids_new);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenNewCols), pstrColsNew);
		GPOS_DELETE(pstrColsNew);
	}

	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize project list
	(*dxlnode)[0]->SerializeToDXL(xml_serializer);

	// serialize physical child
	(*dxlnode)[1]->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRowTrigger::AssertValid(const CDXLNode *dxlnode,
									BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG


// EOF
