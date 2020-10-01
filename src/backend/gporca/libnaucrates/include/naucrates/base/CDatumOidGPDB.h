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

	// private copy ctor
	CDatumOidGPDB(const CDatumOidGPDB &);

public:
	// ctors
	CDatumOidGPDB(CSystemId sysid, OID oid_val, BOOL is_null = false);
	CDatumOidGPDB(IMDId *mdid, OID oid_val, BOOL is_null = false);

	// dtor
	virtual ~CDatumOidGPDB();

	// accessor of metadata type id
	virtual IMDId *MDId() const;

	// accessor of size
	virtual ULONG Size() const;

	// accessor of oid value
	virtual OID OidValue() const;

	// accessor of is null
	virtual BOOL IsNull() const;

	// return string representation
	virtual const CWStringConst *GetStrRepr(CMemoryPool *mp) const;

	// hash function
	virtual ULONG HashValue() const;

	// match function for datums
	virtual BOOL Matches(const IDatum *) const;

	// copy datum
	virtual IDatum *MakeCopy(CMemoryPool *mp) const;

	// print function
	virtual IOstream &OsPrint(IOstream &os) const;

};	// class CDatumOidGPDB
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CDatumOidGPDB_H

// EOF
