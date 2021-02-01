//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IBucket.h
//
//	@doc:
//		Simple bucket interface
//---------------------------------------------------------------------------

#ifndef GPNAUCRATES_IBucket_H
#define GPNAUCRATES_IBucket_H

#include "gpos/base.h"

#include "naucrates/statistics/CPoint.h"

namespace gpnaucrates
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		IBucket
//
//	@doc:
//		Simple bucket interface. Has a lower point and upper point
//
//---------------------------------------------------------------------------

class IBucket
{
private:
public:
	IBucket &operator=(const IBucket &) = delete;

	IBucket(const IBucket &) = delete;

	// c'tor
	IBucket() = default;

	// lower point
	virtual CPoint *GetLowerBound() const = 0;

	// upper point
	virtual CPoint *GetUpperBound() const = 0;

	// is bucket singleton?
	BOOL
	IsSingleton() const
	{
		return GetLowerBound()->Equals(GetUpperBound());
	}

	// d'tor
	virtual ~IBucket() = default;
};
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_IBucket_H

// EOF
