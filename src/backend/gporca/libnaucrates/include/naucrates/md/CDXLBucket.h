//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLBucket.h
//
//	@doc:
//		Class representing buckets in a DXL column stats histogram
//---------------------------------------------------------------------------



#ifndef GPMD_CDXLBucket_H
#define GPMD_CDXLBucket_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "naucrates/dxl/operators/CDXLDatum.h"

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
//		CDXLBucket
//
//	@doc:
//		Class representing a bucket in DXL column stats
//
//---------------------------------------------------------------------------
class CDXLBucket : public CRefCount
{
private:
	// lower bound value for the bucket
	CDXLDatum *m_lower_bound_dxl_datum;

	// max value for the bucket
	CDXLDatum *m_upper_bound_dxl_datum;

	// is lower bound closed (i.e., the boundary point is included in the bucket)
	BOOL m_is_lower_closed;

	// is upper bound closed (i.e., the boundary point is included in the bucket)
	BOOL m_is_upper_closed;

	// frequency
	CDouble m_frequency;

	// distinct values
	CDouble m_distinct;

	// serialize the bucket boundary
	static void SerializeBoundaryValue(CXMLSerializer *xml_serializer,
									   const CWStringConst *elem_str,
									   CDXLDatum *dxl_datum,
									   BOOL is_bound_closed);

public:
	CDXLBucket(const CDXLBucket &) = delete;

	// ctor
	CDXLBucket(CDXLDatum *dxl_datum_lower, CDXLDatum *dxl_datum_upper,
			   BOOL is_lower_closed, BOOL is_upper_closed, CDouble frequency,
			   CDouble distinct);

	// dtor
	~CDXLBucket() override;

	// is lower bound closed
	BOOL
	IsLowerClosed() const
	{
		return m_is_lower_closed;
	}

	// is upper bound closed
	BOOL
	IsUpperClosed() const
	{
		return m_is_upper_closed;
	}

	// min value for the bucket
	const CDXLDatum *GetDXLDatumLower() const;

	// max value for the bucket
	const CDXLDatum *GetDXLDatumUpper() const;

	// frequency
	CDouble GetFrequency() const;

	// distinct values
	CDouble GetNumDistinct() const;

	// serialize bucket in DXL format
	void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the bucket
	void DebugPrint(IOstream &os) const;
#endif
};

// array of dxl buckets
using CDXLBucketArray = CDynamicPtrArray<CDXLBucket, CleanupRelease>;
}  // namespace gpmd

#endif	// !GPMD_CDXLBucket_H

// EOF
