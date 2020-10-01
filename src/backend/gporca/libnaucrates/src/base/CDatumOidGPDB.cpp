//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDatumOidGPDB.cpp
//
//	@doc:
//		Implementation of GPDB oid datum
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/base/CDatumOidGPDB.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeOid.h"
#include "naucrates/md/CMDTypeOidGPDB.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpnaucrates;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::CDatumOidGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumOidGPDB::CDatumOidGPDB(CSystemId sysid, OID oid_val, BOOL is_null)
	: m_mdid(NULL), m_val(oid_val), m_is_null(is_null)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	IMDId *mdid = dynamic_cast<const CMDTypeOidGPDB *>(
					  md_accessor->PtMDType<IMDTypeOid>(sysid))
					  ->MDId();
	mdid->AddRef();

	m_mdid = mdid;

	if (IsNull())
	{
		// needed for hash computation
		m_val = gpos::int_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::CDatumOidGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumOidGPDB::CDatumOidGPDB(IMDId *mdid, OID oid_val, BOOL is_null)
	: m_mdid(mdid), m_val(oid_val), m_is_null(is_null)
{
	GPOS_ASSERT(NULL != m_mdid);
	GPOS_ASSERT(GPDB_OID_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());

	if (IsNull())
	{
		// needed for hash computation
		m_val = gpos::int_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::~CDatumOidGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumOidGPDB::~CDatumOidGPDB()
{
	m_mdid->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::OidValue
//
//	@doc:
//		Accessor of oid value
//
//---------------------------------------------------------------------------
OID
CDatumOidGPDB::OidValue() const
{
	return m_val;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumOidGPDB::IsNull() const
{
	return m_is_null;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumOidGPDB::Size() const
{
	return 4;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::MDId
//
//	@doc:
//		Accessor of type information
//
//---------------------------------------------------------------------------
IMDId *
CDatumOidGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumOidGPDB::HashValue() const
{
	return gpos::CombineHashes(m_mdid->HashValue(),
							   gpos::HashValue<OID>(&m_val));
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumOidGPDB::GetStrRepr(CMemoryPool *mp) const
{
	CWStringDynamic str(mp);
	if (!IsNull())
	{
		str.AppendFormat(GPOS_WSZ_LIT("%d"), m_val);
	}
	else
	{
		str.AppendFormat(GPOS_WSZ_LIT("null"));
	}

	return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumOidGPDB::Matches(const IDatum *datum) const
{
	if (!datum->MDId()->Equals(m_mdid))
	{
		return false;
	}

	const CDatumOidGPDB *datum_cast =
		dynamic_cast<const CDatumOidGPDB *>(datum);

	if (!datum_cast->IsNull() && !IsNull())
	{
		return (datum_cast->OidValue() == OidValue());
	}

	if (datum_cast->IsNull() && IsNull())
	{
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumOidGPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	return GPOS_NEW(mp) CDatumOidGPDB(m_mdid, m_val, m_is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumOidGPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumOidGPDB::OsPrint(IOstream &os) const
{
	if (!IsNull())
	{
		os << m_val;
	}
	else
	{
		os << "null";
	}

	return os;
}

// EOF
