//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLScalarPartOid.cpp
//
//	@doc:
//		Implementation of DXL Part oid expression
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarPartOid.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartOid::CDXLScalarPartOid
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarPartOid::CDXLScalarPartOid(CMemoryPool *mp, ULONG partitioning_level)
	: CDXLScalar(mp), m_partitioning_level(partitioning_level)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartOid::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarPartOid::GetDXLOperator() const
{
	return EdxlopScalarPartOid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartOid::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarPartOid::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarPartOid);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartOid::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarPartOid::SerializeToDXL(CXMLSerializer *xml_serializer,
								  const CDXLNode *	// dxlnode
) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenPartLevel),
								 m_partitioning_level);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarPartOid::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarPartOid::AssertValid(const CDXLNode *dxlnode,
							   BOOL	 // validate_children
) const
{
	GPOS_ASSERT(0 == dxlnode->Arity());
}
#endif	// GPOS_DEBUG

// EOF
