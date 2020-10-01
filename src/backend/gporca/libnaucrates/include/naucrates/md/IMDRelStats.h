//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		IMDRelStats.h
//
//	@doc:
//		Interface for relation stats
//---------------------------------------------------------------------------



#ifndef GPMD_IMDRelStats_H
#define GPMD_IMDRelStats_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/md/IMDCacheObject.h"

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		IMDRelStats
//
//	@doc:
//		Interface for relation stats
//
//---------------------------------------------------------------------------
class IMDRelStats : public IMDCacheObject
{
public:
	// object type
	virtual Emdtype
	MDType() const
	{
		return EmdtRelStats;
	}

	// number of rows
	virtual CDouble Rows() const = 0;

	// is statistics on an empty input
	virtual BOOL IsEmpty() const = 0;
};
}  // namespace gpmd

#endif	// !GPMD_IMDRelStats_H

// EOF
