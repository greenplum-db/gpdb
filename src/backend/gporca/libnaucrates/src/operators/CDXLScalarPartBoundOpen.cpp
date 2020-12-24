//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarPartBoundOpen.cpp
//
//	@doc:
//		Implementation of DXL Part bound openness expression
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarPartBoundOpen.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartBoundOpen::CDXLScalarPartBoundOpen
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarPartBoundOpen::CDXLScalarPartBoundOpen(CMemoryPool *mp,
												 ULONG partitioning_level,
												 BOOL is_lower_bound)
	: CDXLScalar(mp),
	  m_partitioning_level(partitioning_level),
	  m_is_lower_bound(is_lower_bound)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartBoundOpen::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarPartBoundOpen::GetDXLOperator() const
{
	return EdxlopScalarPartBoundOpen;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartBoundOpen::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarPartBoundOpen::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarPartBoundOpen);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartBoundOpen::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarPartBoundOpen::SerializeToDXL(CXMLSerializer *xml_serializer,
										const CDXLNode *  // dxlnode
) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenPartLevel),
								 m_partitioning_level);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenScalarPartBoundLower),
		m_is_lower_bound);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartBoundOpen::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarPartBoundOpen::AssertValid(const CDXLNode *dxlnode,
									 BOOL  // validate_children
) const
{
	GPOS_ASSERT(0 == dxlnode->Arity());
}
#endif	// GPOS_DEBUG

// EOF
