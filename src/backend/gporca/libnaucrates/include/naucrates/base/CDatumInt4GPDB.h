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

public:
	CDatumInt4GPDB(const CDatumInt4GPDB &) = delete;

	// ctors
	CDatumInt4GPDB(CSystemId sysid, INT val, BOOL is_null = false);
	CDatumInt4GPDB(IMDId *mdid, INT val, BOOL is_null = false);

	// dtor
	~CDatumInt4GPDB() override;

	// accessor of metadata type id
	IMDId *MDId() const override;

	// accessor of size
	ULONG Size() const override;

	// accessor of integer value
	INT Value() const override;

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

};	// class CDatumInt4GPDB

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_CDatumInt4GPDB_H

// EOF
