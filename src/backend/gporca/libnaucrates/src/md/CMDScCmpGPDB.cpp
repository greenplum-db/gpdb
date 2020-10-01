//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDScCmpGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific comparisons
//		in the MD cache
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDScCmpGPDB.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpmd;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::CMDScCmpGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDScCmpGPDB::CMDScCmpGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
						   IMDId *left_mdid, IMDId *right_mdid,
						   IMDType::ECmpType cmp_type, IMDId *mdid_op)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_mdid_left(left_mdid),
	  m_mdid_right(right_mdid),
	  m_comparision_type(cmp_type),
	  m_mdid_op(mdid_op)
{
	GPOS_ASSERT(m_mdid->IsValid());
	GPOS_ASSERT(m_mdid_left->IsValid());
	GPOS_ASSERT(m_mdid_right->IsValid());
	GPOS_ASSERT(m_mdid_op->IsValid());
	GPOS_ASSERT(IMDType::EcmptOther != m_comparision_type);

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::~CMDScCmpGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDScCmpGPDB::~CMDScCmpGPDB()
{
	m_mdid->Release();
	m_mdid_left->Release();
	m_mdid_right->Release();
	m_mdid_op->Release();
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::MDId
//
//	@doc:
//		Mdid of comparison object
//
//---------------------------------------------------------------------------
IMDId *
CMDScCmpGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::Mdname
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
CMDName
CMDScCmpGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::GetLeftMdid
//
//	@doc:
//		Left type id
//
//---------------------------------------------------------------------------
IMDId *
CMDScCmpGPDB::GetLeftMdid() const
{
	return m_mdid_left;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::GetRightMdid
//
//	@doc:
//		Destination type id
//
//---------------------------------------------------------------------------
IMDId *
CMDScCmpGPDB::GetRightMdid() const
{
	return m_mdid_right;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::MdIdOp
//
//	@doc:
//		Cast function id
//
//---------------------------------------------------------------------------
IMDId *
CMDScCmpGPDB::MdIdOp() const
{
	return m_mdid_op;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::IsBinaryCoercible
//
//	@doc:
//		Returns whether this is a cast between binary coercible types, i.e. the
//		types are binary compatible
//
//---------------------------------------------------------------------------
IMDType::ECmpType
CMDScCmpGPDB::ParseCmpType() const
{
	return m_comparision_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::Serialize
//
//	@doc:
//		Serialize comparison operator metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDScCmpGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBMDScCmp));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpCmpType),
		IMDType::GetCmpTypeStr(m_comparision_type));

	m_mdid_left->Serialize(
		xml_serializer,
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpLeftTypeId));
	m_mdid_right->Serialize(
		xml_serializer,
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBScalarOpRightTypeId));
	m_mdid_op->Serialize(xml_serializer,
						 CDXLTokens::GetDXLTokenStr(EdxltokenOpNo));

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBMDScCmp));

	GPOS_CHECK_ABORT;
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMDScCmpGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDScCmpGPDB::DebugPrint(IOstream &os) const
{
	os << "ComparisonOp ";
	GetLeftMdid()->OsPrint(os);
	os << (Mdname()).GetMDName()->GetBuffer() << "(";
	MdIdOp()->OsPrint(os);
	os << ") ";
	GetLeftMdid()->OsPrint(os);


	os << ", type: " << IMDType::GetCmpTypeStr(m_comparision_type);

	os << std::endl;
}

#endif	// GPOS_DEBUG

// EOF
