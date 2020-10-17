//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDatumInt8GPDB.cpp
//
//	@doc:
//		Implementation of GPDB Int8
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/base/CDatumInt8GPDB.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::CDatumInt8GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt8GPDB::CDatumInt8GPDB(CSystemId sysid, LINT val, BOOL is_null)
	: m_mdid(NULL), m_val(val), m_is_null(is_null)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	IMDId *mdid = dynamic_cast<const CMDTypeInt8GPDB *>(
					  md_accessor->PtMDType<IMDTypeInt8>(sysid))
					  ->MDId();
	mdid->AddRef();
	m_mdid = mdid;

	if (IsNull())
	{
		// needed for hash computation
		m_val = LINT(gpos::ulong_max >> 1);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::CDatumInt8GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt8GPDB::CDatumInt8GPDB(IMDId *mdid, LINT val, BOOL is_null)
	: m_mdid(mdid), m_val(val), m_is_null(is_null)
{
	GPOS_ASSERT(NULL != m_mdid);
	GPOS_ASSERT(GPDB_INT8_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());

	if (IsNull())
	{
		// needed for hash computation
		m_val = LINT(gpos::ulong_max >> 1);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::~CDatumInt8GPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumInt8GPDB::~CDatumInt8GPDB()
{
	m_mdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::Value
//
//	@doc:
//		Accessor of integer value
//
//---------------------------------------------------------------------------
LINT
CDatumInt8GPDB::Value() const
{
	return m_val;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumInt8GPDB::IsNull() const
{
	return m_is_null;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumInt8GPDB::Size() const
{
	return 8;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::MDId
//
//	@doc:
//		Accessor of type information
//
//---------------------------------------------------------------------------
IMDId *
CDatumInt8GPDB::MDId() const
{
	return m_mdid;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumInt8GPDB::HashValue() const
{
	return gpos::CombineHashes(m_mdid->HashValue(),
							   gpos::HashValue<LINT>(&m_val));
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumInt8GPDB::GetStrRepr(CMemoryPool *mp) const
{
	CWStringDynamic str(mp);
	if (!IsNull())
	{
		str.AppendFormat(GPOS_WSZ_LIT("%ld"), m_val);
	}
	else
	{
		str.AppendFormat(GPOS_WSZ_LIT("null"));
	}

	return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumInt8GPDB::Matches(const IDatum *datum) const
{
	if (!m_mdid->Equals(datum->MDId()))
	{
		return false;
	}

	const CDatumInt8GPDB *datum_cast =
		dynamic_cast<const CDatumInt8GPDB *>(datum);

	if (!datum_cast->IsNull() && !IsNull())
	{
		return (datum_cast->Value() == Value());
	}

	if (datum_cast->IsNull() && IsNull())
	{
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumInt8GPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	return GPOS_NEW(mp) CDatumInt8GPDB(m_mdid, m_val, m_is_null);
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt8GPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumInt8GPDB::OsPrint(IOstream &os) const
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
