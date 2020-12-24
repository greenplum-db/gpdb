//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
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

public:
	CDatumInt2GPDB(const CDatumInt2GPDB &) = delete;

	// ctors
	CDatumInt2GPDB(CSystemId sysid, SINT val, BOOL is_null = false);
	CDatumInt2GPDB(IMDId *mdid, SINT val, BOOL is_null = false);

	// dtor
	~CDatumInt2GPDB() override;

	// accessor of metadata type id
	IMDId *MDId() const override;

	// accessor of size
	ULONG Size() const override;

	// accessor of integer value
	SINT Value() const override;

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

};	// class CDatumInt2GPDB

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_CDatumInt2GPDB_H

// EOF
