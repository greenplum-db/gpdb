//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CMDIdGPDBCtas.cpp
//
//	@doc:
//		Implementation of metadata identifiers for GPDB CTAS queries
//---------------------------------------------------------------------------

#include "naucrates/md/CMDIdGPDBCtas.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpmd;

#define GPMD_GPDB_CTAS_SYSID GPOS_WSZ_LIT("CTAS")

// invalid key
CMDIdGPDBCtas CMDIdGPDBCtas::m_mdid_invalid_key(0);

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDBCtas::CMDIdGPDBCtas
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDIdGPDBCtas::CMDIdGPDBCtas(OID oid)
	: CMDIdGPDB(CSystemId(IMDId::EmdidGPDB, GPMD_GPDB_CTAS_SYSID), oid)
{
	Serialize();
}


//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDBCtas::CMDIdGPDBCtas
//
//	@doc:
//		Copy constructor
//
//---------------------------------------------------------------------------
CMDIdGPDBCtas::CMDIdGPDBCtas(const CMDIdGPDBCtas &mdid_source)
	: CMDIdGPDB(mdid_source.Sysid(), mdid_source.Oid())
{
	GPOS_ASSERT(mdid_source.IsValid());
	GPOS_ASSERT(IMDId::EmdidGPDBCtas == mdid_source.MdidType());
	Serialize();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDBCtas::Equals
//
//	@doc:
//		Checks if the version of the current object is compatible with another version
//		of the same object
//
//---------------------------------------------------------------------------
BOOL
CMDIdGPDBCtas::Equals(const IMDId *mdid) const
{
	if (NULL == mdid || EmdidGPDBCtas != mdid->MdidType())
	{
		return false;
	}

	const CMDIdGPDBCtas *mdidGPDBCTAS =
		CMDIdGPDBCtas::CastMdid(const_cast<IMDId *>(mdid));

	return m_oid == mdidGPDBCTAS->Oid();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDBCtas::IsValid
//
//	@doc:
//		Is the mdid valid
//
//---------------------------------------------------------------------------
BOOL
CMDIdGPDBCtas::IsValid() const
{
	return !Equals(&CMDIdGPDBCtas::m_mdid_invalid_key);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIdGPDBCtas::OsPrint
//
//	@doc:
//		Debug print of the id in the provided stream
//
//---------------------------------------------------------------------------
IOstream &
CMDIdGPDBCtas::OsPrint(IOstream &os) const
{
	os << "(" << Oid() << "," << VersionMajor() << "." << VersionMinor() << ")";
	return os;
}

// EOF
