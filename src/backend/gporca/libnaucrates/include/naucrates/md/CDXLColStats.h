//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLColStats.h
//
//	@doc:
//		Class representing column stats
//---------------------------------------------------------------------------

#ifndef GPMD_CDXLColStats_H
#define GPMD_CDXLColStats_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CDXLBucket.h"
#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/IMDColStats.h"

namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CDXLColStats
//
//	@doc:
//		Class representing column stats
//
//---------------------------------------------------------------------------
class CDXLColStats : public IMDColStats
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// metadata id of the object
	CMDIdColStats *m_mdid_col_stats;

	// column name
	CMDName *m_mdname;

	// column width
	CDouble m_width;

	// null fraction
	CDouble m_null_freq;

	// ndistinct of remaining tuples
	CDouble m_distinct_remaining;

	// frequency of remaining tuples
	CDouble m_freq_remaining;

	// histogram buckets
	CDXLBucketArray *m_dxl_stats_bucket_array;

	// is column statistics missing in the database
	BOOL m_is_col_stats_missing;

	// DXL string for object
	CWStringDynamic *m_dxl_str;

public:
	CDXLColStats(const CDXLColStats &) = delete;

	// ctor
	CDXLColStats(CMemoryPool *mp, CMDIdColStats *mdid_col_stats,
				 CMDName *mdname, CDouble width, CDouble null_freq,
				 CDouble distinct_remaining, CDouble freq_remaining,
				 CDXLBucketArray *dxl_stats_bucket_array,
				 BOOL is_col_stats_missing);

	// dtor
	~CDXLColStats() override;

	// the metadata id
	IMDId *MDId() const override;

	// relation name
	CMDName Mdname() const override;

	// DXL string representation of cache object
	const CWStringDynamic *GetStrRepr() const override;

	// number of buckets
	ULONG Buckets() const override;

	// width
	CDouble
	Width() const override
	{
		return m_width;
	}

	// null fraction
	CDouble
	GetNullFreq() const override
	{
		return m_null_freq;
	}

	// ndistinct of remaining tuples
	CDouble
	GetDistinctRemain() const override
	{
		return m_distinct_remaining;
	}

	// frequency of remaining tuples
	CDouble
	GetFreqRemain() const override
	{
		return m_freq_remaining;
	}

	// is the column statistics missing in the database
	BOOL
	IsColStatsMissing() const override
	{
		return m_is_col_stats_missing;
	}

	// get the bucket at the given position
	const CDXLBucket *GetDXLBucketAt(ULONG ul) const override;

	// serialize column stats in DXL format
	void Serialize(gpdxl::CXMLSerializer *) const override;

#ifdef GPOS_DEBUG
	// debug print of the column stats
	void DebugPrint(IOstream &os) const override;
#endif

	// dummy colstats
	static CDXLColStats *CreateDXLDummyColStats(CMemoryPool *mp, IMDId *mdid,
												CMDName *mdname, CDouble width);
};

// array of dxl column stats
using CDXLColStatsArray = CDynamicPtrArray<CDXLColStats, CleanupRelease>;
}  // namespace gpmd



#endif	// !GPMD_CDXLColStats_H

// EOF
