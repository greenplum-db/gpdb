//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPoint.h
//
//	@doc:
//		Point in the datum space
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CPoint_H
#define GPNAUCRATES_CPoint_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/base/IDatum.h"

namespace gpopt
{
class CMDAccessor;
}

namespace gpnaucrates
{
using namespace gpos;
using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@class:
//		CPoint
//
//	@doc:
//		One dimensional point in the datum space
//---------------------------------------------------------------------------
class CPoint : public CRefCount, public DbgPrintMixin<CPoint>
{
private:
	// datum corresponding to the point
	IDatum *m_datum;

public:
	CPoint &operator=(CPoint &) = delete;

	CPoint(const CPoint &) = delete;

	// c'tor
	explicit CPoint(IDatum *);

	// get underlying datum
	IDatum *
	GetDatum() const
	{
		return m_datum;
	}

	// is this point equal to another
	BOOL Equals(const CPoint *) const;

	// is this point not equal to another
	BOOL IsNotEqual(const CPoint *) const;

	// less than
	BOOL IsLessThan(const CPoint *) const;

	// less than or equals
	BOOL IsLessThanOrEqual(const CPoint *) const;

	// greater than
	BOOL IsGreaterThan(const CPoint *) const;

	// greater than or equals
	BOOL IsGreaterThanOrEqual(const CPoint *) const;

	// distance between two points
	CDouble Distance(const CPoint *) const;

	// distance between two points, taking bounds into account
	CDouble Width(const CPoint *, BOOL include_lower, BOOL include_upper) const;

	// print function
	IOstream &OsPrint(IOstream &os) const;

	// d'tor
	~CPoint() override
	{
		m_datum->Release();
	}

	// translate the point into its DXL representation
	CDXLDatum *GetDatumVal(CMemoryPool *mp, CMDAccessor *md_accessor) const;

	// minimum of two points using <=
	static CPoint *MinPoint(CPoint *point1, CPoint *point2);

	// maximum of two points using >=
	static CPoint *MaxPoint(CPoint *point1, CPoint *point2);
};	// class CPoint

// array of CPoints
using CPointArray = CDynamicPtrArray<CPoint, CleanupRelease>;
}  // namespace gpnaucrates

#endif	// !GPNAUCRATES_CPoint_H

// EOF
