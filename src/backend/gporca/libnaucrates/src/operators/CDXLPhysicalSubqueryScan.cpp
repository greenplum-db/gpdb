//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalSubqueryScan.cpp
//
//	@doc:
//		Implementation of DXL physical subquery scan operators
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalSubqueryScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::CDXLPhysicalSubqueryScan
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalSubqueryScan::CDXLPhysicalSubqueryScan(CMemoryPool *mp,
												   CMDName *mdname)
	: CDXLPhysical(mp), m_mdname_alias(mdname)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::~CDXLPhysicalSubqueryScan
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLPhysicalSubqueryScan::~CDXLPhysicalSubqueryScan()
{
	GPOS_DELETE(m_mdname_alias);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalSubqueryScan::GetDXLOperator() const
{
	return EdxlopPhysicalSubqueryScan;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalSubqueryScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalSubqueryScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::MdName
//
//	@doc:
//		Name for the subquery
//
//---------------------------------------------------------------------------
const CMDName *
CDXLPhysicalSubqueryScan::MdName()
{
	return m_mdname_alias;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalSubqueryScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSubqueryScan::SerializeToDXL(CXMLSerializer *xml_serializer,
										 const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenAlias),
								 m_mdname_alias->GetMDName());

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
//		CDXLPhysicalSubqueryScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalSubqueryScan::AssertValid(const CDXLNode *dxlnode,
									  BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	// subquery scan has 3 children
	GPOS_ASSERT(EdxlsubqscanIndexSentinel == dxlnode->Arity());

	CDXLNode *child_dxlnode = (*dxlnode)[EdxlsubqscanIndexChild];
	GPOS_ASSERT(EdxloptypePhysical ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}

	// assert validity of table descriptor
	GPOS_ASSERT(NULL != m_mdname_alias);
	GPOS_ASSERT(m_mdname_alias->GetMDName()->IsValid());
}
#endif	// GPOS_DEBUG

// EOF
