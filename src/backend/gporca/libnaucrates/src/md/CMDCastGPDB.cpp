//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CMDCastGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing GPDB-specific casts
//		in the MD cache
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDCastGPDB.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpmd;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::CMDCastGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDCastGPDB::CMDCastGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
						 IMDId *mdid_src, IMDId *mdid_dest,
						 BOOL is_binary_coercible, IMDId *mdid_cast_func,
						 EmdCoercepathType path_type)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_mdid_src(mdid_src),
	  m_mdid_dest(mdid_dest),
	  m_is_binary_coercible(is_binary_coercible),
	  m_mdid_cast_func(mdid_cast_func),
	  m_path_type(path_type)
{
	GPOS_ASSERT(m_mdid->IsValid());
	GPOS_ASSERT(m_mdid_src->IsValid());
	GPOS_ASSERT(m_mdid_dest->IsValid());
	GPOS_ASSERT_IMP(!is_binary_coercible, m_mdid_cast_func->IsValid());

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::~CMDCastGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDCastGPDB::~CMDCastGPDB()
{
	m_mdid->Release();
	m_mdid_src->Release();
	m_mdid_dest->Release();
	CRefCount::SafeRelease(m_mdid_cast_func);
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::MDId
//
//	@doc:
//		Mdid of cast object
//
//---------------------------------------------------------------------------
IMDId *
CMDCastGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::Mdname
//
//	@doc:
//		Func name
//
//---------------------------------------------------------------------------
CMDName
CMDCastGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::MdidSrc
//
//	@doc:
//		Source type id
//
//---------------------------------------------------------------------------
IMDId *
CMDCastGPDB::MdidSrc() const
{
	return m_mdid_src;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::MdidDest
//
//	@doc:
//		Destination type id
//
//---------------------------------------------------------------------------
IMDId *
CMDCastGPDB::MdidDest() const
{
	return m_mdid_dest;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::GetCastFuncMdId
//
//	@doc:
//		Cast function id
//
//---------------------------------------------------------------------------
IMDId *
CMDCastGPDB::GetCastFuncMdId() const
{
	return m_mdid_cast_func;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::IsBinaryCoercible
//
//	@doc:
//		Returns whether this is a cast between binary coercible types, i.e. the
//		types are binary compatible
//
//---------------------------------------------------------------------------
BOOL
CMDCastGPDB::IsBinaryCoercible() const
{
	return m_is_binary_coercible;
}

// returns coercion path type
IMDCast::EmdCoercepathType
CMDCastGPDB::GetMDPathType() const
{
	return m_path_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::Serialize
//
//	@doc:
//		Serialize function metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDCastGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCast));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastBinaryCoercible),
		m_is_binary_coercible);
	m_mdid_src->Serialize(xml_serializer,
						  CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastSrcType));
	m_mdid_dest->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastDestType));
	m_mdid_cast_func->Serialize(
		xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastFuncId));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCastCoercePathType),
		m_path_type);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenGPDBCast));
}


#ifdef GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMDCastGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDCastGPDB::DebugPrint(IOstream &os) const
{
	os << "Cast " << (Mdname()).GetMDName()->GetBuffer() << ": ";
	MdidSrc()->OsPrint(os);
	os << "->";
	MdidDest()->OsPrint(os);
	os << std::endl;

	if (m_is_binary_coercible)
	{
		os << ", binary-coercible";
	}

	if (IMDId::IsValid(m_mdid_cast_func))
	{
		os << ", Cast func id: ";
		GetCastFuncMdId()->OsPrint(os);
	}

	os << std::endl;
}

#endif	// GPOS_DEBUG

// EOF
