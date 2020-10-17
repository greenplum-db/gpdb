//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumBoolGPDB.h
//
//	@doc:
//		GPDB-specific bool representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CDatumBoolGPDB_H
#define GPNAUCRATES_CDatumBoolGPDB_H

#include "gpos/base.h"
#include "naucrates/base/IDatumBool.h"

#include "naucrates/md/CMDTypeBoolGPDB.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CDatumBoolGPDB
//
//	@doc:
//		GPDB-specific bool representation
//
//---------------------------------------------------------------------------
class CDatumBoolGPDB : public IDatumBool
{
private:
	// type information
	IMDId *m_mdid;

	// boolean value
	BOOL m_value;

	// is null
	BOOL m_is_null;

	// private copy ctor
	CDatumBoolGPDB(const CDatumBoolGPDB &);

public:
	// ctors
	CDatumBoolGPDB(CSystemId sysid, BOOL value, BOOL is_null = false);
	CDatumBoolGPDB(IMDId *mdid, BOOL value, BOOL is_null = false);

	// dtor
	virtual ~CDatumBoolGPDB();

	// accessor of metadata type mdid
	virtual IMDId *MDId() const;

	// accessor of boolean value
	virtual BOOL GetValue() const;

	// accessor of size
	virtual ULONG Size() const;

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

};	// class CDatumBoolGPDB
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CDatumBoolGPDB_H

// EOF
