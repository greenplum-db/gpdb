//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumInt4GPDB.h
//
//	@doc:
//		GPDB-specific int4 representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CDatumInt4GPDB_H
#define GPNAUCRATES_CDatumInt4GPDB_H

#include "gpos/base.h"

#include "naucrates/base/IDatumInt4.h"
#include "naucrates/md/CMDTypeInt4GPDB.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CDatumInt4GPDB
//
//	@doc:
//		GPDB-specific int4 representation
//
//---------------------------------------------------------------------------
class CDatumInt4GPDB : public IDatumInt4
{
private:
	// type information
	IMDId *m_mdid;

	// integer value
	INT m_val;

	// is null
	BOOL m_is_null;

	// private copy ctor
	CDatumInt4GPDB(const CDatumInt4GPDB &);

public:
	// ctors
	CDatumInt4GPDB(CSystemId sysid, INT val, BOOL is_null = false);
	CDatumInt4GPDB(IMDId *mdid, INT val, BOOL is_null = false);

	// dtor
	virtual ~CDatumInt4GPDB();

	// accessor of metadata type id
	virtual IMDId *MDId() const;

	// accessor of size
	virtual ULONG Size() const;

	// accessor of integer value
	virtual INT Value() const;

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

};	// class CDatumInt4GPDB

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_CDatumInt4GPDB_H

// EOF
