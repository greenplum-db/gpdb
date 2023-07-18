//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		IMDExtStatsInfo.h
//
//	@doc:
//		Interface for extended statistic metadata in the metadata cache
//---------------------------------------------------------------------------
#ifndef GPMD_IMDExtStatsInfo_H
#define GPMD_IMDExtStatsInfo_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/md/CMDExtStatsInfo.h"
#include "naucrates/md/IMDCacheObject.h"

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		IMDExtStatsInfo
//
//	@doc:
//		Interface for extended statistic metadata in the metadata cache
//
//---------------------------------------------------------------------------
class IMDExtStatsInfo : public IMDCacheObject
{
public:
	// object type
	Emdtype
	MDType() const override
	{
		return EmdtExtStatsInfo;
	}

	virtual CMDExtStatsInfoArray *GetExtStatInfoArray() const = 0;
};
}  // namespace gpmd

#endif	// !GPMD_IMDExtStatsInfo_H

// EOF
