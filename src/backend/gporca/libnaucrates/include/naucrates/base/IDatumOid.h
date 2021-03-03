//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		IDatumOid.h
//
//	@doc:
//		Base abstract class for oid representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IDatumOid_H
#define GPNAUCRATES_IDatumOid_H

#include "gpos/base.h"

#include "naucrates/base/IDatum.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		IDatumOid
//
//	@doc:
//		Base abstract class for oid representation
//
//---------------------------------------------------------------------------
class IDatumOid : public IDatum
{
private:
public:
	IDatumOid(const IDatumOid &) = delete;

	// ctor
	IDatumOid() = default;

	// dtor
	~IDatumOid() override = default;

	// accessor for datum type
	IMDType::ETypeInfo
	GetDatumType() override
	{
		return IMDType::EtiOid;
	}

	// accessor of oid value
	virtual OID OidValue() const = 0;

	// can datum be mapped to a double
	BOOL
	IsDatumMappableToDouble() const override
	{
		return true;
	}

	// map to double for stats computation
	CDouble
	GetDoubleMapping() const override
	{
		return CDouble(GetLINTMapping());
	}

	// can datum be mapped to LINT
	BOOL
	IsDatumMappableToLINT() const override
	{
		return true;
	}

	// map to LINT for statistics computation
	LINT
	GetLINTMapping() const override
	{
		return LINT(OidValue());
	}

	// byte array representation of datum
	const BYTE *
	GetByteArrayValue() const override
	{
		GPOS_ASSERT(!"Invalid invocation of MakeCopyOfValue");
		return nullptr;
	}

	// does the datum need to be padded before statistical derivation
	BOOL
	NeedsPadding() const override
	{
		return false;
	}

	// return the padded datum
	IDatum *
	MakePaddedDatum(CMemoryPool *,	// mp,
					ULONG			// col_len
	) const override
	{
		GPOS_ASSERT(!"Invalid invocation of MakePaddedDatum");
		return nullptr;
	}

	// does datum support like predicate
	BOOL
	SupportsLikePredicate() const override
	{
		return false;
	}

	// return the default scale factor of like predicate
	CDouble
	GetLikePredicateScaleFactor() const override
	{
		GPOS_ASSERT(!"Invalid invocation of DLikeSelectivity");
		return false;
	}
};	// class IDatumOid
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_IDatumOid_H

// EOF
