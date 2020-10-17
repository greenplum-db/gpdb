//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarConstValue.cpp
//
//	@doc:
//		Implementation of DXL scalar const
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLScalarConstValue.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

#include "gpopt/mdcache/CMDAccessor.h"

using namespace gpopt;
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::CDXLScalarConstValue
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarConstValue::CDXLScalarConstValue(CMemoryPool *mp,
										   CDXLDatum *dxl_datum)
	: CDXLScalar(mp), m_dxl_datum(dxl_datum)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::~CDXLScalarConstValue
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLScalarConstValue::~CDXLScalarConstValue()
{
	m_dxl_datum->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarConstValue::GetDXLOperator() const
{
	return EdxlopScalarConstValue;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarConstValue::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarConstValue);
	;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarConstValue::SerializeToDXL(CXMLSerializer *xml_serializer,
									 const CDXLNode *  //node
) const
{
	const CWStringConst *element_name = GetOpNameStr();
	m_dxl_datum->Serialize(xml_serializer, element_name);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::HasBoolResult
//
//	@doc:
//		Does the operator return boolean result
//
//---------------------------------------------------------------------------
BOOL
CDXLScalarConstValue::HasBoolResult(CMDAccessor *md_accessor) const
{
	return (IMDType::EtiBool ==
			md_accessor->RetrieveType(m_dxl_datum->MDId())->GetDatumType());
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarConstValue::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarConstValue::AssertValid(const CDXLNode *node,
								  BOOL	// validate_children
) const
{
	GPOS_ASSERT(0 == node->Arity());
	GPOS_ASSERT(m_dxl_datum->MDId()->IsValid());
}
#endif	// GPOS_DEBUG

// EOF
