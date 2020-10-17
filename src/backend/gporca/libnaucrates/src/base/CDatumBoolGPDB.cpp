//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumBoolGPDB.cpp
//
//	@doc:
//		Implementation of GPDB bool
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/base/CDatumBoolGPDB.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/mdcache/CMDAccessor.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpnaucrates;
using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::CDatumBoolGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumBoolGPDB::CDatumBoolGPDB(CSystemId sysid, BOOL value, BOOL is_null)
	: m_value(value), m_is_null(is_null)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	IMDId *mdid = dynamic_cast<const CMDTypeBoolGPDB *>(
					  md_accessor->PtMDType<IMDTypeBool>(sysid))
					  ->MDId();
	mdid->AddRef();

	m_mdid = mdid;

	if (IsNull())
	{
		// needed for hash computation
		m_value = false;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::CDatumBoolGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDatumBoolGPDB::CDatumBoolGPDB(IMDId *mdid, BOOL value, BOOL is_null)
	: m_mdid(mdid), m_value(value), m_is_null(is_null)
{
	GPOS_ASSERT(NULL != m_mdid);
	GPOS_ASSERT(GPDB_BOOL_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());

	if (IsNull())
	{
		// needed for hash computation
		m_value = false;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::~CDatumBoolGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDatumBoolGPDB::~CDatumBoolGPDB()
{
	m_mdid->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::MakeCopyOfValue
//
//	@doc:
//		Accessor of boolean value
//
//---------------------------------------------------------------------------
BOOL
CDatumBoolGPDB::GetValue() const
{
	return m_value;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::IsNull
//
//	@doc:
//		Accessor of is null
//
//---------------------------------------------------------------------------
BOOL
CDatumBoolGPDB::IsNull() const
{
	return m_is_null;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::Size
//
//	@doc:
//		Accessor of size
//
//---------------------------------------------------------------------------
ULONG
CDatumBoolGPDB::Size() const
{
	return 1;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::MDId
//
//	@doc:
//		Accessor of type information (MDId)
//
//---------------------------------------------------------------------------
IMDId *
CDatumBoolGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CDatumBoolGPDB::HashValue() const
{
	return gpos::CombineHashes(m_mdid->HashValue(),
							   gpos::HashValue<BOOL>(&m_value));
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::GetMDName
//
//	@doc:
//		Return string representation
//
//---------------------------------------------------------------------------
const CWStringConst *
CDatumBoolGPDB::GetStrRepr(CMemoryPool *mp) const
{
	CWStringDynamic str(mp);
	if (!IsNull())
	{
		str.AppendFormat(GPOS_WSZ_LIT("%d"), m_value);
	}
	else
	{
		str.AppendFormat(GPOS_WSZ_LIT("null"));
	}

	return GPOS_NEW(mp) CWStringConst(mp, str.GetBuffer());
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::Matches
//
//	@doc:
//		Matches the values of datums
//
//---------------------------------------------------------------------------
BOOL
CDatumBoolGPDB::Matches(const IDatum *other) const
{
	if (!other->MDId()->Equals(m_mdid))
	{
		return false;
	}

	const CDatumBoolGPDB *other_cast =
		dynamic_cast<const CDatumBoolGPDB *>(other);

	if (!other_cast->IsNull() && !IsNull())
	{
		return (other_cast->GetValue() == GetValue());
	}

	if (other_cast->IsNull() && IsNull())
	{
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::MakeCopy
//
//	@doc:
//		Returns a copy of the datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumBoolGPDB::MakeCopy(CMemoryPool *mp) const
{
	m_mdid->AddRef();
	return GPOS_NEW(mp) CDatumBoolGPDB(m_mdid, m_value, m_is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumBoolGPDB::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CDatumBoolGPDB::OsPrint(IOstream &os) const
{
	if (!IsNull())
	{
		os << m_value;
	}
	else
	{
		os << "null";
	}

	return os;
}

// EOF
