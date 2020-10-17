//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLColStats.cpp
//
//	@doc:
//		Implementation of the class for representing column stats in DXL
//---------------------------------------------------------------------------


#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CDXLColStats.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "gpos/common/CAutoRef.h"

#include "naucrates/statistics/CStatistics.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::CDXLColStats
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLColStats::CDXLColStats(CMemoryPool *mp, CMDIdColStats *mdid_col_stats,
						   CMDName *mdname, CDouble width, CDouble null_freq,
						   CDouble distinct_remaining, CDouble freq_remaining,
						   CDXLBucketArray *dxl_stats_bucket_array,
						   BOOL is_col_stats_missing)
	: m_mp(mp),
	  m_mdid_col_stats(mdid_col_stats),
	  m_mdname(mdname),
	  m_width(width),
	  m_null_freq(null_freq),
	  m_distinct_remaining(distinct_remaining),
	  m_freq_remaining(freq_remaining),
	  m_dxl_stats_bucket_array(dxl_stats_bucket_array),
	  m_is_col_stats_missing(is_col_stats_missing)
{
	GPOS_ASSERT(mdid_col_stats->IsValid());
	GPOS_ASSERT(NULL != dxl_stats_bucket_array);
	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::~CDXLColStats
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLColStats::~CDXLColStats()
{
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_mdid_col_stats->Release();
	m_dxl_stats_bucket_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::MDId
//
//	@doc:
//		Returns the metadata id of this column stats object
//
//---------------------------------------------------------------------------
IMDId *
CDXLColStats::MDId() const
{
	return m_mdid_col_stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::Mdname
//
//	@doc:
//		Returns the name of this column
//
//---------------------------------------------------------------------------
CMDName
CDXLColStats::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::GetMDName
//
//	@doc:
//		Returns the DXL string for this object
//
//---------------------------------------------------------------------------
const CWStringDynamic *
CDXLColStats::GetStrRepr() const
{
	return m_dxl_str;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::Buckets
//
//	@doc:
//		Returns the number of buckets in the histogram
//
//---------------------------------------------------------------------------
ULONG
CDXLColStats::Buckets() const
{
	return m_dxl_stats_bucket_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::GetDXLBucketAt
//
//	@doc:
//		Returns the bucket at the given position
//
//---------------------------------------------------------------------------
const CDXLBucket *
CDXLColStats::GetDXLBucketAt(ULONG pos) const
{
	return (*m_dxl_stats_bucket_array)[pos];
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::Serialize
//
//	@doc:
//		Serialize column stats in DXL format
//
//---------------------------------------------------------------------------
void
CDXLColStats::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnStats));

	m_mdid_col_stats->Serialize(xml_serializer,
								CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenWidth),
								 m_width);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColNullFreq), m_null_freq);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColNdvRemain),
		m_distinct_remaining);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColFreqRemain), m_freq_remaining);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColStatsMissing),
		m_is_col_stats_missing);

	GPOS_CHECK_ABORT;

	ULONG num_of_buckets = Buckets();
	for (ULONG ul = 0; ul < num_of_buckets; ul++)
	{
		const CDXLBucket *dxl_bucket = GetDXLBucketAt(ul);
		dxl_bucket->Serialize(xml_serializer);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnStats));
}



#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::DebugPrint
//
//	@doc:
//		Dbug print of the column stats object
//
//---------------------------------------------------------------------------
void
CDXLColStats::DebugPrint(IOstream &os) const
{
	os << "Column id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Column name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;

	for (ULONG ul = 0; ul < Buckets(); ul++)
	{
		const CDXLBucket *dxl_bucket = GetDXLBucketAt(ul);
		dxl_bucket->DebugPrint(os);
	}
}

#endif	// GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CDXLColStats::CreateDXLDummyColStats
//
//	@doc:
//		Dummy statistics
//
//---------------------------------------------------------------------------
CDXLColStats *
CDXLColStats::CreateDXLDummyColStats(CMemoryPool *mp, IMDId *mdid,
									 CMDName *mdname, CDouble width)
{
	CMDIdColStats *mdid_col_stats = CMDIdColStats::CastMdid(mdid);

	CAutoRef<CDXLBucketArray> dxl_bucket_array;
	dxl_bucket_array = GPOS_NEW(mp) CDXLBucketArray(mp);
	CAutoRef<CDXLColStats> dxl_col_stats;
	dxl_col_stats = GPOS_NEW(mp) CDXLColStats(
		mp, mdid_col_stats, mdname, width, CHistogram::DefaultNullFreq,
		CHistogram::DefaultNDVRemain, CHistogram::DefaultNDVFreqRemain,
		dxl_bucket_array.Value(), true /* is_col_stats_missing */
	);
	dxl_bucket_array.Reset();
	return dxl_col_stats.Reset();
}

// EOF
