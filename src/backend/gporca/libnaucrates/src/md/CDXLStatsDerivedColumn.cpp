//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLStatsDerivedColumn.cpp
//
//	@doc:
//		Implementation of the class for representing dxl derived column statistics
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"
#include "naucrates/md/CDXLStatsDerivedColumn.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedColumn::CDXLStatsDerivedColumn
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLStatsDerivedColumn::CDXLStatsDerivedColumn(
	ULONG colid, CDouble width, CDouble null_freq, CDouble distinct_remaining,
	CDouble freq_remaining, CDXLBucketArray *dxl_stats_bucket_array)
	: m_colid(colid),
	  m_width(width),
	  m_null_freq(null_freq),
	  m_distinct_remaining(distinct_remaining),
	  m_freq_remaining(freq_remaining),
	  m_dxl_stats_bucket_array(dxl_stats_bucket_array)
{
	GPOS_ASSERT(0 <= m_width);
	GPOS_ASSERT(0 <= m_null_freq);
	GPOS_ASSERT(0 <= m_distinct_remaining);
	GPOS_ASSERT(0 <= m_freq_remaining);
	GPOS_ASSERT(NULL != m_dxl_stats_bucket_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedColumn::~CDXLStatsDerivedColumn
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLStatsDerivedColumn::~CDXLStatsDerivedColumn()
{
	m_dxl_stats_bucket_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedColumn::TransformHistogramToDXLBucketArray
//
//	@doc:
//		Returns the array of buckets
//
//---------------------------------------------------------------------------
const CDXLBucketArray *
CDXLStatsDerivedColumn::TransformHistogramToDXLBucketArray() const
{
	return m_dxl_stats_bucket_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedColumn::Serialize
//
//	@doc:
//		Serialize bucket in DXL format
//
//---------------------------------------------------------------------------
void
CDXLStatsDerivedColumn::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsDerivedColumn));

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenColId),
								 m_colid);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenWidth),
								 m_width);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColNullFreq), m_null_freq);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColNdvRemain),
		m_distinct_remaining);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenColFreqRemain), m_freq_remaining);


	const ULONG num_of_buckets = m_dxl_stats_bucket_array->Size();
	for (ULONG ul = 0; ul < num_of_buckets; ul++)
	{
		GPOS_CHECK_ABORT;

		CDXLBucket *dxl_bucket = (*m_dxl_stats_bucket_array)[ul];
		dxl_bucket->Serialize(xml_serializer);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsDerivedColumn));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLStatsDerivedColumn::DebugPrint
//
//	@doc:
//		Debug print of the bucket object
//
//---------------------------------------------------------------------------
void
CDXLStatsDerivedColumn::DebugPrint(IOstream &os) const
{
	os << "Column id: " << m_colid;
	os << std::endl;
	os << "Width : " << m_width;
	os << std::endl;

	const ULONG num_of_buckets = m_dxl_stats_bucket_array->Size();
	for (ULONG ul = 0; ul < num_of_buckets; ul++)
	{
		const CDXLBucket *dxl_bucket = (*m_dxl_stats_bucket_array)[ul];
		dxl_bucket->DebugPrint(os);
	}
}

#endif	// GPOS_DEBUG

// EOF
