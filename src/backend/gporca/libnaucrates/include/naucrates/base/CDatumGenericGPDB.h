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

	// long int value used for statistic computation
	LINT m_stats_comp_val_int;

	// double value used for statistic computation
	CDouble m_stats_comp_val_double;

	// private copy ctor
	CDatumGenericGPDB(const CDatumGenericGPDB &);

public:
	// ctor
	CDatumGenericGPDB(CMemoryPool *mp, IMDId *mdid, INT type_modifier,
					  const void *src, ULONG size, BOOL is_null,
					  LINT stats_comp_val_int, CDouble stats_comp_val_double);

	// dtor
	virtual ~CDatumGenericGPDB();

	// accessor of metadata type id
	virtual IMDId *MDId() const;

	virtual INT TypeModifier() const;

	// accessor of size
	virtual ULONG Size() const;

	// accessor of is null
	virtual BOOL IsNull() const;

	// return string representation
	virtual const CWStringConst *GetStrRepr(CMemoryPool *mp) const;

	// hash function
	virtual ULONG HashValue() const;

	// match function for datums
	virtual BOOL Matches(const IDatum *datum) const;

	// copy datum
	virtual IDatum *MakeCopy(CMemoryPool *mp) const;

	// print function
	virtual IOstream &OsPrint(IOstream &os) const;

	// accessor to bytearray, creates a copy
	virtual BYTE *MakeCopyOfValue(CMemoryPool *mp, ULONG *pulLength) const;

	// statistics related APIs

	// can datum be mapped to a double
	virtual BOOL IsDatumMappableToDouble() const;

	// map to double for stats computation
	virtual CDouble
	GetDoubleMapping() const
	{
		GPOS_ASSERT(IsDatumMappableToDouble());

		return m_stats_comp_val_double;
	}

	// can datum be mapped to LINT
	virtual BOOL IsDatumMappableToLINT() const;

	// map to LINT for statistics computation
	virtual LINT
	GetLINTMapping() const
	{
		GPOS_ASSERT(IsDatumMappableToLINT());

		return m_stats_comp_val_int;
	}

	// byte array representation of datum
	virtual const BYTE *GetByteArrayValue() const;

	// stats equality
	virtual BOOL StatsAreEqual(const IDatum *datum) const;

	// does the datum need to be padded before statistical derivation
	virtual BOOL NeedsPadding() const;

	// return the padded datum
	virtual IDatum *MakePaddedDatum(CMemoryPool *mp, ULONG col_len) const;

	// does datum support like predicate
	virtual BOOL
	SupportsLikePredicate() const
	{
		return true;
	}

	// return the default scale factor of like predicate
	virtual CDouble GetLikePredicateScaleFactor() const;

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
