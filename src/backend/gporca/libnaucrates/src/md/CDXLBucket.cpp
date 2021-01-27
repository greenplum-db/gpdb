//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLBucket.cpp
//
//	@doc:
//		Implementation of the class for representing buckets in DXL column stats
//---------------------------------------------------------------------------


#include "naucrates/md/CDXLBucket.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::CDXLBucket
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLBucket::CDXLBucket(CDXLDatum *dxl_datum_lower, CDXLDatum *dxl_datum_upper,
					   BOOL is_lower_closed, BOOL is_upper_closed,
					   CDouble frequency, CDouble distinct)
	: m_lower_bound_dxl_datum(dxl_datum_lower),
	  m_upper_bound_dxl_datum(dxl_datum_upper),
	  m_is_lower_closed(is_lower_closed),
	  m_is_upper_closed(is_upper_closed),
	  m_frequency(frequency),
	  m_distinct(distinct)
{
	GPOS_ASSERT(NULL != dxl_datum_lower);
	GPOS_ASSERT(NULL != dxl_datum_upper);
	GPOS_ASSERT(m_frequency >= 0.0 && m_frequency <= 1.0);
	GPOS_ASSERT(m_distinct >= 0);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::~CDXLBucket
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLBucket::~CDXLBucket()
{
	m_lower_bound_dxl_datum->Release();
	m_upper_bound_dxl_datum->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::GetDXLDatumLower
//
//	@doc:
//		Returns the lower bound for the bucket
//
//---------------------------------------------------------------------------
const CDXLDatum *
CDXLBucket::GetDXLDatumLower() const
{
	return m_lower_bound_dxl_datum;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::GetDXLDatumUpper
//
//	@doc:
//		Returns the upper bound for the bucket
//
//---------------------------------------------------------------------------
const CDXLDatum *
CDXLBucket::GetDXLDatumUpper() const
{
	return m_upper_bound_dxl_datum;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::GetFrequency
//
//	@doc:
//		Returns the frequency for this bucket
//
//---------------------------------------------------------------------------
CDouble
CDXLBucket::GetFrequency() const
{
	return m_frequency;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::GetNumDistinct
//
//	@doc:
//		Returns the number of distinct in this bucket
//
//---------------------------------------------------------------------------
CDouble
CDXLBucket::GetNumDistinct() const
{
	return m_distinct;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::Serialize
//
//	@doc:
//		Serialize bucket in DXL format
//
//---------------------------------------------------------------------------
void
CDXLBucket::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnStatsBucket));

	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsFrequency), m_frequency);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsDistinct), m_distinct);

	SerializeBoundaryValue(
		xml_serializer,
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsBucketLowerBound),
		m_lower_bound_dxl_datum, m_is_lower_closed);
	SerializeBoundaryValue(
		xml_serializer,
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsBucketUpperBound),
		m_upper_bound_dxl_datum, m_is_upper_closed);

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumnStatsBucket));

	GPOS_CHECK_ABORT;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::Serialize
//
//	@doc:
//		Serialize the bucket boundary
//
//---------------------------------------------------------------------------
void
CDXLBucket::SerializeBoundaryValue(CXMLSerializer *xml_serializer,
								   const CWStringConst *elem_str,
								   CDXLDatum *dxl_datum,
								   BOOL is_bound_closed) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), elem_str);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenStatsBoundClosed), is_bound_closed);
	dxl_datum->Serialize(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), elem_str);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLBucket::DebugPrint
//
//	@doc:
//		Debug print of the bucket object
//
//---------------------------------------------------------------------------
void
CDXLBucket::DebugPrint(IOstream &  //os
) const
{
	// TODO:  - Feb 13, 2012; implement
}

#endif	// GPOS_DEBUG

// EOF
