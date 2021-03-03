//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDatumOidGPDB.h
//
//	@doc:
//		GPDB-specific oid representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CDatumOidGPDB_H
#define GPNAUCRATES_CDatumOidGPDB_H

#include "gpos/base.h"

#include "naucrates/base/IDatumOid.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CDatumOidGPDB
//
//	@doc:
//		GPDB-specific oid representation
//
//---------------------------------------------------------------------------
class CDatumOidGPDB : public IDatumOid
{
private:
	// type information
	IMDId *m_mdid;

	// oid value
	OID m_val;

	// is null
	BOOL m_is_null;

public:
	CDatumOidGPDB(const CDatumOidGPDB &) = delete;

	// ctors
	CDatumOidGPDB(CSystemId sysid, OID oid_val, BOOL is_null = false);
	CDatumOidGPDB(IMDId *mdid, OID oid_val, BOOL is_null = false);

	// dtor
	~CDatumOidGPDB() override;

	// accessor of metadata type id
	IMDId *MDId() const override;

	// accessor of size
	ULONG Size() const override;

	// accessor of oid value
	OID OidValue() const override;

	// accessor of is null
	BOOL IsNull() const override;

	// return string representation
	const CWStringConst *GetStrRepr(CMemoryPool *mp) const override;

	// hash function
	ULONG HashValue() const override;

	// match function for datums
	BOOL Matches(const IDatum *) const override;

	// copy datum
	IDatum *MakeCopy(CMemoryPool *mp) const override;

	// print function
	IOstream &OsPrint(IOstream &os) const override;

};	// class CDatumOidGPDB
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CDatumOidGPDB_H

// EOF
