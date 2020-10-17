//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDCheckConstraintGPDB.cpp
//
//	@doc:
//		Implementation of the class representing a GPDB check constraint
//		in a metadata cache relation
//---------------------------------------------------------------------------

#include "naucrates/md/CMDCheckConstraintGPDB.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "gpopt/translate/CTranslatorDXLToExpr.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CMDCheckConstraintGPDB::CMDCheckConstraintGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDCheckConstraintGPDB::CMDCheckConstraintGPDB(CMemoryPool *mp, IMDId *mdid,
											   CMDName *mdname, IMDId *rel_mdid,
											   CDXLNode *dxlnode)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_rel_mdid(rel_mdid),
	  m_dxl_node(dxlnode)
{
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(rel_mdid->IsValid());
	GPOS_ASSERT(NULL != mdname);
	GPOS_ASSERT(NULL != dxlnode);

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCheckConstraintGPDB::~CMDCheckConstraintGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDCheckConstraintGPDB::~CMDCheckConstraintGPDB()
{
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_mdid->Release();
	m_rel_mdid->Release();
	m_dxl_node->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCheckConstraintGPDB::GetCheckConstraintExpr
//
//	@doc:
//		Scalar expression of the check constraint
//
//---------------------------------------------------------------------------
CExpression *
CMDCheckConstraintGPDB::GetCheckConstraintExpr(CMemoryPool *mp,
											   CMDAccessor *md_accessor,
											   CColRefArray *colref_array) const
{
	GPOS_ASSERT(NULL != colref_array);

	const IMDRelation *mdrel = md_accessor->RetrieveRel(m_rel_mdid);
#ifdef GPOS_DEBUG
	const ULONG len = colref_array->Size();
	GPOS_ASSERT(len > 0);

	const ULONG arity =
		mdrel->NonDroppedColsCount() - mdrel->SystemColumnsCount();
	GPOS_ASSERT(arity == len);
#endif	// GPOS_DEBUG

	// translate the DXL representation of the check constraint expression
	CTranslatorDXLToExpr dxltr(mp, md_accessor);
	return dxltr.PexprTranslateScalar(m_dxl_node, colref_array,
									  mdrel->NonDroppedColsArray());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCheckConstraintGPDB::Serialize
//
//	@doc:
//		Serialize check constraint in DXL format
//
//---------------------------------------------------------------------------
void
CMDCheckConstraintGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCheckConstraint));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	m_rel_mdid->Serialize(xml_serializer,
						  CDXLTokens::GetDXLTokenStr(EdxltokenRelationMdid));

	// serialize the scalar expression
	m_dxl_node->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenCheckConstraint));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDCheckConstraintGPDB::DebugPrint
//
//	@doc:
//		Prints a MD constraint to the provided output
//
//---------------------------------------------------------------------------
void
CMDCheckConstraintGPDB::DebugPrint(IOstream &os) const
{
	os << "Constraint Id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Constraint Name: " << (Mdname()).GetMDName()->GetBuffer()
	   << std::endl;

	os << "Relation id: ";
	GetRelMdId()->OsPrint(os);
	os << std::endl;
}

#endif	// GPOS_DEBUG

// EOF
