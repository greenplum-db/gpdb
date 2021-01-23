//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDPartConstraintGPDB.cpp
//
//	@doc:
//		Implementation of part constraints in the MD cache
//---------------------------------------------------------------------------

#include "naucrates/md/CMDPartConstraintGPDB.h"

#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::CMDPartConstraintGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB::CMDPartConstraintGPDB(
	CMemoryPool *mp, ULongPtrArray *level_with_default_part_array,
	BOOL is_unbounded, CDXLNode *dxlnode)
	: m_mp(mp),
	  m_level_with_default_part_array(level_with_default_part_array),
	  m_is_unbounded(is_unbounded),
	  m_dxl_node(dxlnode)
{
	GPOS_ASSERT(NULL != level_with_default_part_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::~CMDPartConstraintGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDPartConstraintGPDB::~CMDPartConstraintGPDB()
{
	if (NULL != m_dxl_node)
		m_dxl_node->Release();
	m_level_with_default_part_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::GetPartConstraintExpr
//
//	@doc:
//		Scalar expression of the check constraint
//
//---------------------------------------------------------------------------
CExpression *
CMDPartConstraintGPDB::GetPartConstraintExpr(CMemoryPool *mp,
											 CMDAccessor *md_accessor,
											 CColRefArray *colref_array) const
{
	GPOS_ASSERT(NULL != colref_array);

	// translate the DXL representation of the part constraint expression
	CTranslatorDXLToExpr dxltr(mp, md_accessor);
	return dxltr.PexprTranslateScalar(m_dxl_node, colref_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::GetDefaultPartsArray
//
//	@doc:
//		Included default partitions
//
//---------------------------------------------------------------------------
ULongPtrArray *
CMDPartConstraintGPDB::GetDefaultPartsArray() const
{
	return m_level_with_default_part_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::IsConstraintUnbounded
//
//	@doc:
//		Is constraint unbounded
//
//---------------------------------------------------------------------------
BOOL
CMDPartConstraintGPDB::IsConstraintUnbounded() const
{
	return m_is_unbounded;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDPartConstraintGPDB::Serialize
//
//	@doc:
//		Serialize part constraint in DXL format
//
//---------------------------------------------------------------------------
void
CMDPartConstraintGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenPartConstraint));

	// serialize default parts
	CWStringDynamic *default_part_array =
		CDXLUtils::Serialize(m_mp, m_level_with_default_part_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenDefaultPartition),
		default_part_array);
	GPOS_DELETE(default_part_array);

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenPartConstraintUnbounded),
		m_is_unbounded);

	// serialize the scalar expression
	if (NULL != m_dxl_node)
		m_dxl_node->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenPartConstraint));

	GPOS_CHECK_ABORT;
}

// EOF
