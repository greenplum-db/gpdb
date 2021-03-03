//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumGenericGPDB.h
//
//	@doc:
//		GPDB-specific generic datum representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CDatumGenericGPDB_H
#define GPNAUCRATES_CDatumGenericGPDB_H

#include "gpos/base.h"

#include "naucrates/base/IDatumGeneric.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

#define GPDB_DATUM_HDRSZ 4

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		CDatumGenericGPDB
//
//	@doc:
//		GPDB-specific generic datum representation
//
//---------------------------------------------------------------------------
class CDatumGenericGPDB : public IDatumGeneric
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// size in bytes
	ULONG m_size;

	// a pointer to datum value
	BYTE *m_bytearray_value;

	// is null
	BOOL m_is_null;

	// type information
	IMDId *m_mdid;

	INT m_type_modifier;

	// cached type information (can be set from const methods)
	mutable const IMDType *m_cached_type;

	// long int value used for statistic computation
	LINT m_stats_comp_val_int;

	// double value used for statistic computation
	CDouble m_stats_comp_val_double;

public:
	CDatumGenericGPDB(const CDatumGenericGPDB &) = delete;

	// ctor
	CDatumGenericGPDB(CMemoryPool *mp, IMDId *mdid, INT type_modifier,
					  const void *src, ULONG size, BOOL is_null,
					  LINT stats_comp_val_int, CDouble stats_comp_val_double);

	// dtor
	~CDatumGenericGPDB() override;

	// accessor of metadata type id
	IMDId *MDId() const override;

	INT TypeModifier() const override;

	// accessor of size
	ULONG Size() const override;

	// accessor of is null
	BOOL IsNull() const override;

	// return string representation
	const CWStringConst *GetStrRepr(CMemoryPool *mp) const override;

	// hash function
	ULONG HashValue() const override;

	// match function for datums
	BOOL Matches(const IDatum *datum) const override;

	// copy datum
	IDatum *MakeCopy(CMemoryPool *mp) const override;

	// print function
	IOstream &OsPrint(IOstream &os) const override;

	// accessor to bytearray, creates a copy
	virtual BYTE *MakeCopyOfValue(CMemoryPool *mp, ULONG *pulLength) const;

	// statistics related APIs

	// can datum be mapped to a double
	BOOL IsDatumMappableToDouble() const override;

	// map to double for stats computation
	CDouble
	GetDoubleMapping() const override
	{
		GPOS_ASSERT(IsDatumMappableToDouble());

		return m_stats_comp_val_double;
	}

	// can datum be mapped to LINT
	BOOL IsDatumMappableToLINT() const override;

	// map to LINT for statistics computation
	LINT
	GetLINTMapping() const override
	{
		GPOS_ASSERT(IsDatumMappableToLINT());

		return m_stats_comp_val_int;
	}

	// byte array representation of datum
	const BYTE *GetByteArrayValue() const override;

	// stats equality
	BOOL StatsAreEqual(const IDatum *datum) const override;

	// does the datum need to be padded before statistical derivation
	BOOL NeedsPadding() const override;

	// return the padded datum
	IDatum *MakePaddedDatum(CMemoryPool *mp, ULONG col_len) const override;

	// does datum support like predicate
	BOOL
	SupportsLikePredicate() const override
	{
		return true;
	}

	// return the default scale factor of like predicate
	CDouble GetLikePredicateScaleFactor() const override;

	// default selectivity of the trailing wildcards
	virtual CDouble GetTrailingWildcardSelectivity(const BYTE *pba,
												   ULONG ulPos) const;

	// selectivities needed for LIKE predicate statistics evaluation
	static const CDouble DefaultFixedCharSelectivity;
	static const CDouble DefaultCharRangeSelectivity;
	static const CDouble DefaultAnyCharSelectivity;
	static const CDouble DefaultCdbRanchorSelectivity;
	static const CDouble DefaultCdbRolloffSelectivity;

};	// class CDatumGenericGPDB
}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_CDatumGenericGPDB_H

// EOF
