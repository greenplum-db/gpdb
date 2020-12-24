//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		IDatumInt2.h
//
//	@doc:
//		Base abstract class for int2 representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IDatumInt2_H
#define GPNAUCRATES_IDatumInt2_H

#include "gpos/base.h"
#include "naucrates/base/IDatum.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		IDatumInt2
//
//	@doc:
//		Base abstract class for int2 representation
//
//---------------------------------------------------------------------------
class IDatumInt2 : public IDatum
{
private:
public:
	IDatumInt2(const IDatumInt2 &) = delete;

	// ctor
	IDatumInt2() = default;

	// dtor
	~IDatumInt2() override = default;

	// accessor for datum type
	IMDType::ETypeInfo
	GetDatumType() override
	{
		return IMDType::EtiInt2;
	}

	// accessor of integer value
	virtual SINT Value() const = 0;

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
		return LINT(Value());
	}

	// byte array representation of datum
	const BYTE *
	GetByteArrayValue() const override
	{
		GPOS_ASSERT(!"Invalid invocation of MakeCopyOfValue");
		return NULL;
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
		return NULL;
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

};	// class IDatumInt2

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_IDatumInt2_H

// EOF
