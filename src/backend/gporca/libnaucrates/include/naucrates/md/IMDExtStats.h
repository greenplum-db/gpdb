//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2022 Greenplum, Inc.
//
//	@filename:
//		IMDExtStats.h
//
//	@doc:
//		Interface for extended statistic in the metadata cache
//---------------------------------------------------------------------------

#ifndef GPMD_IMDExtStats_H
#define GPMD_IMDExtStats_H

#include "gpos/base.h"

#include "naucrates/md/CMDDependency.h"
#include "naucrates/md/IMDCacheObject.h"
#include "naucrates/statistics/IStatistics.h"

namespace gpdxl
{
//fwd declaration
class CXMLSerializer;
}  // namespace gpdxl

namespace gpmd
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		IMDExtStats
//
//	@doc:
//		Interface for extended statistic in the metadata cache
//
//---------------------------------------------------------------------------
class IMDExtStats : public IMDCacheObject
{
public:
	Emdtype
	MDType() const override
	{
		return EmdtExtStats;
	}

	virtual CMDDependencyArray *GetDependencies() const = 0;
};

}  // namespace gpmd



#endif	// !GPMD_IMDExtStats_H

// EOF
