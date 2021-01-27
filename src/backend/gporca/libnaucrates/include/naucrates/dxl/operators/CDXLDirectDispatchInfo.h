//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLDirectDispatchInfo.h
//
//	@doc:
//		Class for representing the specification of directly dispatchable plans
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDirectDispatchInfo_H
#define GPDXL_CDXLDirectDispatchInfo_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLDirectDispatchInfo
//
//	@doc:
//		Class for representing the specification of directly dispatchable plans
//
//---------------------------------------------------------------------------
class CDXLDirectDispatchInfo : public CRefCount
{
private:
	// constants for determining segments to dispatch to
	CDXLDatum2dArray *m_dispatch_identifer_datum_array;

	// true indicates m_dispatch_identifer_datum_array contains raw
	// gp_segment_id values rather than hashable datums
	BOOL m_contains_raw_values;

	// private copy ctor
	CDXLDirectDispatchInfo(const CDXLDirectDispatchInfo &);

public:
	// ctor
	explicit CDXLDirectDispatchInfo(
		CDXLDatum2dArray *dispatch_identifer_datum_array,
		BOOL contains_raw_values);

	// dtor
	virtual ~CDXLDirectDispatchInfo();

	BOOL
	FContainsRawValues() const
	{
		return m_contains_raw_values;
	}

	// accessor to array of datums
	CDXLDatum2dArray *
	GetDispatchIdentifierDatumArray() const
	{
		return m_dispatch_identifer_datum_array;
	}

	// serialize the datum as the given element
	void Serialize(CXMLSerializer *xml_serializer);
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLDirectDispatchInfo_H

// EOF
