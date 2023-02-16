//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDIdGPDB.cpp
//
//	@doc:
//		Implementation of metadata identifiers
//---------------------------------------------------------------------------

#include "naucrates/md/CMDIdGPDB.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpmd;


// initialize static members
// invalid key
CMDIdGPDB CMDIdGPDB::m_mdid_invalid_key(IMDId::EmdidGeneral, 0, 0, 0);

// int2 mdid
CMDIdGPDB CMDIdGPDB::m_mdid_int2(IMDId::EmdidGeneral, GPDB_INT2);

// int4 mdid
CMDIdGPDB CMDIdGPDB::m_mdid_int4(IMDId::EmdidGeneral, GPDB_INT4);

// int8 mdid
CMDIdGPDB CMDIdGPDB::m_mdid_int8(IMDId::EmdidGeneral, GPDB_INT8);

// bool mdid
CMDIdGPDB CMDIdGPDB::m_mdid_bool(IMDId::EmdidGeneral, GPDB_BOOL);

// oid mdid
CMDIdGPDB CMDIdGPDB::m_mdid_oid(IMDId::EmdidGeneral, GPDB_OID);

// numeric mdid
CMDIdGPDB CMDIdGPDB::m_mdid_numeric(IMDId::EmdidGeneral, GPDB_NUMERIC);

// date mdid
CMDIdGPDB CMDIdGPDB::m_mdid_date(IMDId::EmdidGeneral, GPDB_DATE);

// time mdid
CMDIdGPDB CMDIdGPDB::m_mdid_time(IMDId::EmdidGeneral, GPDB_TIME);

// time with time zone mdid
CMDIdGPDB CMDIdGPDB::m_mdid_timeTz(IMDId::EmdidGeneral, GPDB_TIMETZ);

// timestamp mdid
CMDIdGPDB CMDIdGPDB::m_mdid_timestamp(IMDId::EmdidGeneral, GPDB_TIMESTAMP);

// timestamp with time zone mdid
CMDIdGPDB CMDIdGPDB::m_mdid_timestampTz(IMDId::EmdidGeneral, GPDB_TIMESTAMPTZ);

// absolute time mdid
CMDIdGPDB CMDIdGPDB::m_mdid_abs_time(IMDId::EmdidGeneral, GPDB_ABSTIME);

// relative time mdid
CMDIdGPDB CMDIdGPDB::m_mdid_relative_time(IMDId::EmdidGeneral, GPDB_RELTIME);

// interval mdid
CMDIdGPDB CMDIdGPDB::m_mdid_interval(IMDId::EmdidGeneral, GPDB_INTERVAL);

// time interval mdid
CMDIdGPDB CMDIdGPDB::m_mdid_time_interval(IMDId::EmdidGeneral,
										  GPDB_TIMEINTERVAL);

// char mdid
CMDIdGPDB CMDIdGPDB::m_mdid_char(IMDId::EmdidGeneral, GPDB_SINGLE_CHAR);

// bpchar mdid
CMDIdGPDB CMDIdGPDB::m_mdid_bpchar(IMDId::EmdidGeneral, GPDB_CHAR);

// varchar mdid
CMDIdGPDB CMDIdGPDB::m_mdid_varchar(IMDId::EmdidGeneral, GPDB_VARCHAR);

// text mdid
CMDIdGPDB CMDIdGPDB::m_mdid_text(IMDId::EmdidGeneral, GPDB_TEXT);

// text mdid
CMDIdGPDB CMDIdGPDB::m_mdid_name(IMDId::EmdidGeneral, GPDB_NAME);

// float4 mdid
CMDIdGPDB CMDIdGPDB::m_mdid_float4(IMDId::EmdidGeneral, GPDB_FLOAT4);

// float8 mdid
CMDIdGPDB CMDIdGPDB::m_mdid_float8(IMDId::EmdidGeneral, GPDB_FLOAT8);

// cash mdid
CMDIdGPDB CMDIdGPDB::m_mdid_cash(IMDId::EmdidGeneral, GPDB_CASH);

// inet mdid
CMDIdGPDB CMDIdGPDB::m_mdid_inet(IMDId::EmdidGeneral, GPDB_INET);

// cidr mdid
CMDIdGPDB CMDIdGPDB::m_mdid_cidr(IMDId::EmdidGeneral, GPDB_CIDR);

// macaddr mdid
CMDIdGPDB CMDIdGPDB::m_mdid_macaddr(IMDId::EmdidGeneral, GPDB_MACADDR);

// count(*) mdid
CMDIdGPDB CMDIdGPDB::m_mdid_count_star(IMDId::EmdidGeneral, GPDB_COUNT_STAR);

// count(Any) mdid
CMDIdGPDB CMDIdGPDB::m_mdid_count_any(IMDId::EmdidGeneral, GPDB_COUNT_ANY);

// uuid mdid
CMDIdGPDB CMDIdGPDB::m_mdid_uuid(IMDId::EmdidGeneral, GPDB_UUID);

// unknown mdid
CMDIdGPDB CMDIdGPDB::m_mdid_unknown(IMDId::EmdidGeneral, GPDB_UNKNOWN);

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::CMDIdGPDB
//
//	@doc:
//		Constructs a metadata identifier with specified oid and default version
//		of 1.0
//
//---------------------------------------------------------------------------
CMDIdGPDB::CMDIdGPDB(CSystemId sysid, OID oid)
	: m_sysid(sysid),
	  m_oid(oid),
	  m_major_version(1),
	  m_minor_version(0),
	  m_str(m_mdid_array, GPOS_ARRAY_SIZE(m_mdid_array))
{
	if (CMDIdGPDB::m_mdid_invalid_key.Oid() == oid)
	{
		// construct an invalid mdid 0.0.0
		m_major_version = 0;
	}

	// serialize mdid into static string
	Serialize();
}

CMDIdGPDB::CMDIdGPDB(EMDIdType mdIdType, OID oid, ULONG version_major,
					 ULONG version_minor)
	: m_sysid(mdIdType, GPMD_GPDB_SYSID),
	  m_oid(oid),
	  m_major_version(version_major),
	  m_minor_version(version_minor),
	  m_str(m_mdid_array, GPOS_ARRAY_SIZE(m_mdid_array))
{
	GPOS_RTL_ASSERT(
		mdIdType == IMDId::EmdidGeneral ||
		(mdIdType >= IMDId::EmdidRel && mdIdType < IMDId::EmdidSentinel));
	if (CMDIdGPDB::m_mdid_invalid_key.Oid() == oid)
	{
		// construct an invalid mdid 0.0.0
		m_major_version = 0;
	}
	// serialize mdid into static string
	Serialize();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::CMDIdGPDB
//
//	@doc:
//		Copy constructor
//
//---------------------------------------------------------------------------
CMDIdGPDB::CMDIdGPDB(const CMDIdGPDB &mdid_source)
	: IMDId(),
	  m_sysid(mdid_source.Sysid()),
	  m_oid(mdid_source.Oid()),
	  m_major_version(mdid_source.VersionMajor()),
	  m_minor_version(mdid_source.VersionMinor()),
	  m_str(m_mdid_array, GPOS_ARRAY_SIZE(m_mdid_array))
{
	// serialize mdid into static string
	Serialize();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::Serialize
//
//	@doc:
//		Serialize mdid into static string
//
//---------------------------------------------------------------------------
void
CMDIdGPDB::Serialize()
{
	m_str.Reset();
	// serialize mdid as SystemType.Oid.Major.Minor
	m_str.AppendFormat(GPOS_WSZ_LIT("%d.%d.%d.%d"), MdidType(), m_oid,
					   m_major_version, m_minor_version);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::GetBuffer
//
//	@doc:
//		Returns the string representation of the mdid
//
//---------------------------------------------------------------------------
const WCHAR *
CMDIdGPDB::GetBuffer() const
{
	return m_str.GetBuffer();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::Oid
//
//	@doc:
//		Returns the object id
//
//---------------------------------------------------------------------------
OID
CMDIdGPDB::Oid() const
{
	return m_oid;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::VersionMajor
//
//	@doc:
//		Returns the object's major version
//
//---------------------------------------------------------------------------
ULONG
CMDIdGPDB::VersionMajor() const
{
	return m_major_version;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::VersionMinor
//
//	@doc:
//		Returns the object's minor version
//
//---------------------------------------------------------------------------
ULONG
CMDIdGPDB::VersionMinor() const
{
	return m_minor_version;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::Equals
//
//	@doc:
//		Checks if the version of the current object is compatible with another version
//		of the same object
//
//---------------------------------------------------------------------------
BOOL
CMDIdGPDB::Equals(const IMDId *mdid) const
{
	if (nullptr == mdid || MdidType() != mdid->MdidType())
	{
		return false;
	}

	const CMDIdGPDB *mdidGPDB =
		static_cast<CMDIdGPDB *>(const_cast<IMDId *>(mdid));

	return (m_oid == mdidGPDB->Oid() &&
			m_major_version == mdidGPDB->VersionMajor() &&
			m_minor_version == mdidGPDB->VersionMinor());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::IsValid
//
//	@doc:
//		Is the mdid valid
//
//---------------------------------------------------------------------------
BOOL
CMDIdGPDB::IsValid() const
{
	const CMDIdGPDB *invalid = &CMDIdGPDB::m_mdid_invalid_key;
	return !(m_oid == invalid->Oid() &&
			 m_major_version == invalid->VersionMajor() &&
			 m_minor_version == invalid->VersionMinor());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::Serialize
//
//	@doc:
//		Serializes the mdid as the value of the given attribute
//
//---------------------------------------------------------------------------
void
CMDIdGPDB::Serialize(CXMLSerializer *xml_serializer,
					 const CWStringConst *attribute_str) const
{
	xml_serializer->AddAttribute(attribute_str, &m_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDB::OsPrint
//
//	@doc:
//		Debug print of the id in the provided stream
//
//---------------------------------------------------------------------------
IOstream &
CMDIdGPDB::OsPrint(IOstream &os) const
{
	os << "(" << m_str.GetBuffer() << ")";
	return os;
}

// EOF
