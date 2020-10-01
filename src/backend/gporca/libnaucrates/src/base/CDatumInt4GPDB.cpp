//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumInt4GPDB.cpp
//
//	@doc:
//		Implementation of GPDB int4
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/base/CDatumInt4GPDB.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpnaucrates;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::CDatumInt4GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt4GPDB::CDatumInt4GPDB(CSystemId sysid, INT val, BOOL is_null)
	: m_mdid(NULL), m_val(val), m_is_null(is_null)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	IMDId *mdid = dynamic_cast<const CMDTypeInt4GPDB *>(
					  md_accessor->PtMDType<IMDTypeInt4>(sysid))
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
//		CDatumInt4GPDB::CDatumInt4GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt4GPDB::CDatumInt4GPDB(IMDId *mdid, INT val, BOOL is_null)
	: m_mdid(mdid), m_val(val), m_is_null(is_null)
{
	GPOS_ASSERT(NULL != m_mdid);
	GPOS_ASSERT(GPDB_INT4_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());

	if (IsNull())
	{
		// needed for hash computation
		m_val = gpos::int_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::~CDatumInt4GPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumInt4GPDB::~CDatumInt4GPDB()
{
	m_mdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::Value
//
//	@doc:
//		Accessor of integer value
//
//---------------------------------------------------------------------------
INT
CDatumInt4GPDB::Value() const
{
	return m_val;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumInt4GPDB::IsNull() const
{
	return m_is_null;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumInt4GPDB::Size() const
{
	return 4;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::MDId
//
//	@doc:
//		Accessor of type information
//
//---------------------------------------------------------------------------
IMDId *
CDatumInt4GPDB::MDId() const
{
	return m_mdid;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumInt4GPDB::HashValue() const
{
	return gpos::CombineHashes(m_mdid->HashValue(),
							   gpos::HashValue<INT>(&m_val));
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumInt4GPDB::GetStrRepr(CMemoryPool *mp) const
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
//		CDatumInt4GPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumInt4GPDB::Matches(const IDatum *datum) const
{
	if (!datum->MDId()->Equals(m_mdid))
	{
		return false;
	}

	const CDatumInt4GPDB *datum_cast =
		dynamic_cast<const CDatumInt4GPDB *>(datum);

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
//		CDatumInt4GPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumInt4GPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	return GPOS_NEW(mp) CDatumInt4GPDB(m_mdid, m_val, m_is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt4GPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumInt4GPDB::OsPrint(IOstream &os) const
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
