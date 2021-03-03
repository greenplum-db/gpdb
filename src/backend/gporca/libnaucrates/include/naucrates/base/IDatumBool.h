//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IDatumBool.h
//
//	@doc:
//		Base abstract class for bool representation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_IDatumBool_H
#define GPNAUCRATES_IDatumBool_H

#include "gpos/base.h"

#include "naucrates/base/IDatum.h"

namespace gpnaucrates
{
//---------------------------------------------------------------------------
//	@class:
//		IDatumBool
//
//	@doc:
//		Base abstract class for bool representation
//
//---------------------------------------------------------------------------
class IDatumBool : public IDatum
{
private:
public:
	IDatumBool(const IDatumBool &) = delete;

	// ctor
	IDatumBool() = default;

	// dtor
	~IDatumBool() override = default;

	// accessor for datum type
	IMDType::ETypeInfo
	GetDatumType() override
	{
		return IMDType::EtiBool;
	}

	// accessor of boolean value
	virtual BOOL GetValue() const = 0;

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
		if (GetValue())
		{
			return CDouble(1.0);
		}

		return CDouble(0.0);
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
		if (GetValue())
		{
			return LINT(1);
		}
		return LINT(0);
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
};	// class IDatumBool

}  // namespace gpnaucrates


#endif	// !GPNAUCRATES_IDatumBool_H

// EOF
