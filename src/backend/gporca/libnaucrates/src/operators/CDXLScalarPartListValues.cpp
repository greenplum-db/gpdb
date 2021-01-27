//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	Implementation of DXL Part List Values expression
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarPartListValues.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpos;
using namespace gpdxl;

// Ctor
CDXLScalarPartListValues::CDXLScalarPartListValues(CMemoryPool *mp,
												   ULONG partitioning_level,
												   IMDId *result_type_mdid,
												   IMDId *elem_type_mdid)
	: CDXLScalar(mp),
	  m_partitioning_level(partitioning_level),
	  m_result_type_mdid(result_type_mdid),
	  m_elem_type_mdid(elem_type_mdid)
{
	GPOS_ASSERT(result_type_mdid->IsValid());
	GPOS_ASSERT(elem_type_mdid->IsValid());
}


// Dtor
CDXLScalarPartListValues::~CDXLScalarPartListValues()
{
	m_result_type_mdid->Release();
	m_elem_type_mdid->Release();
}

// Operator type
Edxlopid
CDXLScalarPartListValues::GetDXLOperator() const
{
	return EdxlopScalarPartListValues;
}

// Operator name
const CWStringConst *
CDXLScalarPartListValues::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarPartListValues);
}

// partitioning level
ULONG
CDXLScalarPartListValues::GetPartitioningLevel() const
{
	return m_partitioning_level;
}

// result type
IMDId *
CDXLScalarPartListValues::GetResultTypeMdId() const
{
	return m_result_type_mdid;
}

// element type
IMDId *
CDXLScalarPartListValues::GetElemTypeMdId() const
{
	return m_elem_type_mdid;
}

// does the operator return a boolean result
BOOL
CDXLScalarPartListValues::HasBoolResult(CMDAccessor *  //md_accessor
) const
{
	return false;
}

// Serialize operator in DXL format
void
CDXLScalarPartListValues::SerializeToDXL(CXMLSerializer *xml_serializer,
										 const CDXLNode *  // dxlnode
) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenPartLevel),
								 m_partitioning_level);
	m_result_type_mdid->Serialize(
		xml_serializer,
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpResultTypeId));
	m_elem_type_mdid->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenArrayElementType));
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
// Checks whether operator node is well-structured
void
CDXLScalarPartListValues::AssertValid(const CDXLNode *dxlnode,
									  BOOL	// validate_children
) const
{
	GPOS_ASSERT(0 == dxlnode->Arity());
}
#endif	// GPOS_DEBUG

// conversion function
CDXLScalarPartListValues *
CDXLScalarPartListValues::Cast(CDXLOperator *dxl_op)
{
	GPOS_ASSERT(NULL != dxl_op);
	GPOS_ASSERT(EdxlopScalarPartListValues == dxl_op->GetDXLOperator());

	return dynamic_cast<CDXLScalarPartListValues *>(dxl_op);
}

// EOF
