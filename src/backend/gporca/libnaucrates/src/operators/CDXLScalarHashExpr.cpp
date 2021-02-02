//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarHashExpr.cpp
//
//	@doc:
//		Implementation of DXL hash expressions for redistribute operators
//---------------------------------------------------------------------------
#include "naucrates/dxl/operators/CDXLScalarHashExpr.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/traceflags/traceflags.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::CDXLScalarHashExpr
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarHashExpr::CDXLScalarHashExpr(CMemoryPool *mp, IMDId *opfamily)
	: CDXLScalar(mp), m_mdid_opfamily(opfamily)
{
	GPOS_ASSERT_IMP(GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution),
					m_mdid_opfamily->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::~CDXLScalarHashExpr
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarHashExpr::~CDXLScalarHashExpr()
{
	CRefCount::SafeRelease(m_mdid_opfamily);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarHashExpr::GetDXLOperator() const
{
	return EdxlopScalarHashExpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarHashExpr::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarHashExpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::MdidType
//
//	@doc:
//		Hash expression type from the catalog
//
//---------------------------------------------------------------------------
IMDId *
CDXLScalarHashExpr::MdidOpfamily() const
{
	return m_mdid_opfamily;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarHashExpr::SerializeToDXL(CXMLSerializer *xml_serializer,
								   const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	if (nullptr != m_mdid_opfamily)
	{
		m_mdid_opfamily->Serialize(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenOpfamily));
	}

	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarHashExpr::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarHashExpr::AssertValid(const CDXLNode *node,
								BOOL validate_children) const
{
	GPOS_ASSERT(1 == node->Arity());
	CDXLNode *child_dxlnode = (*node)[0];

	GPOS_ASSERT(EdxloptypeScalar ==
				child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode,
												  validate_children);
	}
}

#endif	// GPOS_DEBUG


// EOF
