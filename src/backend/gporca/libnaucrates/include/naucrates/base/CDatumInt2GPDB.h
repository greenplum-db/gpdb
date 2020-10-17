//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDatumInt2GPDB.h
//
//	@doc:
//		GPDB-specific int2 representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CDatumInt2GPDB_H
#define GPNAUCRATES_CDatumInt2GPDB_H

#include "gpos/base.h"

#include "naucrates/base/IDatumInt2.h"
#include "naucrates/md/CMDTypeInt2GPDB.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CDatumInt2GPDB
//
//	@doc:
//		GPDB-specific int2 representation
//
//---------------------------------------------------------------------------
class CDatumInt2GPDB : public IDatumInt2
{
private:
	// type information
	IMDId *m_mdid;

	// integer value
	SINT m_val;

	// is null
	BOOL m_is_null;

	// private copy ctor
	CDatumInt2GPDB(const CDatumInt2GPDB &);

public:
	// ctors
	CDatumInt2GPDB(CSystemId sysid, SINT val, BOOL is_null = false);
	CDatumInt2GPDB(IMDId *mdid, SINT val, BOOL is_null = false);

	// dtor
	virtual ~CDatumInt2GPDB();

	// accessor of metadata type id
	virtual IMDId *MDId() const;

	// accessor of size
	virtual ULONG Size() const;

	// accessor of integer value
	virtual SINT Value() const;

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

};	// class CDatumInt2GPDB

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_CDatumInt2GPDB_H

// EOF
