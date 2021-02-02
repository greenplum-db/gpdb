//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		IDatumInt8.h
//
//	@doc:
//		Base abstract class for int8 representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IDatumInt8_H
#define GPNAUCRATES_IDatumInt8_H

#include "gpos/base.h"

#include "naucrates/base/IDatum.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		IDatumInt8
//
//	@doc:
//		Base abstract class for int8 representation
//
//---------------------------------------------------------------------------
class IDatumInt8 : public IDatum
{
private:
public:
	IDatumInt8(const IDatumInt8 &) = delete;

	// ctor
	IDatumInt8() = default;

	// dtor
	~IDatumInt8() override = default;

	// accessor for datum type
	IMDType::ETypeInfo
	GetDatumType() override
	{
		return IMDType::EtiInt8;
	}

	// accessor of integer value
	virtual LINT Value() const = 0;

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
		return CDouble(Value());
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
		return Value();
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
};	// class IDatumInt8

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_IDatumInt8_H

// EOF
