//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLStatsDerivedRelation.h
//
//	@doc:
//		Class representing DXL derived relation statistics
//---------------------------------------------------------------------------

#ifndef GPMD_CDXLStatsDerivedRelation_H
#define GPMD_CDXLStatsDerivedRelation_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/md/CDXLStatsDerivedColumn.h"

namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLStatsDerivedRelation
//
//	@doc:
//		Class representing DXL derived relation statistics
//
//---------------------------------------------------------------------------
class CDXLStatsDerivedRelation : public CRefCount
{
private:
	// number of rows
	CDouble m_rows;

	// flag to indicate if input relation is empty
	BOOL m_empty;

	// array of derived column statistics
	CDXLStatsDerivedColumnArray *m_dxl_stats_derived_col_array;

	// private copy ctor
	CDXLStatsDerivedRelation(const CDXLStatsDerivedRelation &);

public:
	// ctor
	CDXLStatsDerivedRelation(
		CDouble rows, BOOL is_empty,
		CDXLStatsDerivedColumnArray *dxl_stats_derived_col_array);

	// dtor
	virtual ~CDXLStatsDerivedRelation();

	// number of rows
	CDouble
	Rows() const
	{
		return m_rows;
	}

	// is statistics on an empty input
	virtual BOOL
	IsEmpty() const
	{
		return m_empty;
	}

	// derived column statistics
	const CDXLStatsDerivedColumnArray *GetDXLStatsDerivedColArray() const;

	// serialize bucket in DXL format
	void Serialize(gpdxl::CXMLSerializer *) const;

#ifdef GPOS_DEBUG
	// debug print of the bucket
	void DebugPrint(IOstream &os) const;
#endif
};

// array of dxl buckets
typedef CDynamicPtrArray<CDXLStatsDerivedRelation, CleanupRelease>
	CDXLStatsDerivedRelationArray;
}  // namespace gpmd

#endif	// !GPMD_CDXLStatsDerivedRelation_H

// EOF
