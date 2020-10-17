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

#include "naucrates/md/IMDColStats.h"
#include "naucrates/md/CMDIdColStats.h"
#include "naucrates/md/CDXLBucket.h"

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

	// private copy ctor
	CDXLColStats(const CDXLColStats &);

public:
	// ctor
	CDXLColStats(CMemoryPool *mp, CMDIdColStats *mdid_col_stats,
				 CMDName *mdname, CDouble width, CDouble null_freq,
				 CDouble distinct_remaining, CDouble freq_remaining,
				 CDXLBucketArray *dxl_stats_bucket_array,
				 BOOL is_col_stats_missing);

	// dtor
	virtual ~CDXLColStats();

	// the metadata id
	virtual IMDId *MDId() const;

	// relation name
	virtual CMDName Mdname() const;

	// DXL string representation of cache object
	virtual const CWStringDynamic *GetStrRepr() const;

	// number of buckets
	virtual ULONG Buckets() const;

	// width
	virtual CDouble
	Width() const
	{
		return m_width;
	}

	// null fraction
	virtual CDouble
	GetNullFreq() const
	{
		return m_null_freq;
	}

	// ndistinct of remaining tuples
	CDouble
	GetDistinctRemain() const
	{
		return m_distinct_remaining;
	}

	// frequency of remaining tuples
	CDouble
	GetFreqRemain() const
	{
		return m_freq_remaining;
	}

	// is the column statistics missing in the database
	BOOL
	IsColStatsMissing() const
	{
		return m_is_col_stats_missing;
	}

	// get the bucket at the given position
	virtual const CDXLBucket *GetDXLBucketAt(ULONG ul) const;

	// serialize column stats in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the column stats
	virtual void DebugPrint(IOstream &os) const;
#endif

	// dummy colstats
	static CDXLColStats *CreateDXLDummyColStats(CMemoryPool *mp, IMDId *mdid,
												CMDName *mdname, CDouble width);
};

// array of dxl column stats
typedef CDynamicPtrArray<CDXLColStats, CleanupRelease> CDXLColStatsArray;
}  // namespace gpmd



#endif	// !GPMD_CDXLColStats_H

// EOF
