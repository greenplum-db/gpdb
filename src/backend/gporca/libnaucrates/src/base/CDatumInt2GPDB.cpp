//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDatumInt2GPDB.cpp
//
//	@doc:
//		Implementation of GPDB int2
//---------------------------------------------------------------------------

#include "naucrates/base/CDatumInt2GPDB.h"

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeInt2.h"

using namespace gpnaucrates;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::CDatumInt2GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt2GPDB::CDatumInt2GPDB(CSystemId sysid, SINT val, BOOL is_null)
	: m_mdid(NULL), m_val(val), m_is_null(is_null)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	IMDId *mdid = dynamic_cast<const CMDTypeInt2GPDB *>(
					  md_accessor->PtMDType<IMDTypeInt2>(sysid))
					  ->MDId();
	mdid->AddRef();

	m_mdid = mdid;

	if (IsNull())
	{
		// needed for hash computation
		m_val = gpos::sint_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::CDatumInt2GPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumInt2GPDB::CDatumInt2GPDB(IMDId *mdid, SINT val, BOOL is_null)
	: m_mdid(mdid), m_val(val), m_is_null(is_null)
{
	GPOS_ASSERT(NULL != m_mdid);
	GPOS_ASSERT(GPDB_INT2_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());

	if (IsNull())
	{
		// needed for hash computation
		m_val = gpos::sint_max;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::~CDatumInt2GPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumInt2GPDB::~CDatumInt2GPDB()
{
	m_mdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::Value
//
//	@doc:
//		Accessor of integer value
//
//---------------------------------------------------------------------------
SINT
CDatumInt2GPDB::Value() const
{
	return m_val;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumInt2GPDB::IsNull() const
{
	return m_is_null;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumInt2GPDB::Size() const
{
	return 2;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::MDId
//
//	@doc:
//		Accessor of type information
//
//---------------------------------------------------------------------------
IMDId *
CDatumInt2GPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumInt2GPDB::HashValue() const
{
	return gpos::CombineHashes(m_mdid->HashValue(),
							   gpos::HashValue<SINT>(&m_val));
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumInt2GPDB::GetStrRepr(CMemoryPool *mp) const
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
//		CDatumInt2GPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumInt2GPDB::Matches(const IDatum *datum) const
{
	if (!datum->MDId()->Equals(m_mdid))
	{
		return false;
	}

	const CDatumInt2GPDB *datum_cast =
		dynamic_cast<const CDatumInt2GPDB *>(datum);

	if (!datum_cast->IsNull() && !IsNull())
	{
		return (datum_cast->Value() == Value());
	}

	return datum_cast->IsNull() && IsNull();
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumInt2GPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	return GPOS_NEW(mp) CDatumInt2GPDB(m_mdid, m_val, m_is_null);
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumInt2GPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumInt2GPDB::OsPrint(IOstream &os) const
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
