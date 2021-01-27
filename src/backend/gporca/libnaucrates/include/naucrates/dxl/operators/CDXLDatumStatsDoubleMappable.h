//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLDatumStatsDoubleMappable.h
//
//	@doc:
//		Class for representing DXL datum of types having double mapping
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumStatsDoubleMappable_H
#define GPDXL_CDXLDatumStatsDoubleMappable_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/dxl/operators/CDXLDatumGeneric.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLDatumStatsDoubleMappable
//
//	@doc:
//		Class for representing DXL datum of types having double mapping
//
//---------------------------------------------------------------------------
class CDXLDatumStatsDoubleMappable : public CDXLDatumGeneric
{
private:
	// for statistics computation, map to double
	CDouble m_val;

	// private copy ctor
	CDXLDatumStatsDoubleMappable(const CDXLDatumStatsDoubleMappable &);

public:
	// ctor
	CDXLDatumStatsDoubleMappable(CMemoryPool *mp, IMDId *mdid_type,
								 INT type_modifier, BOOL is_null, BYTE *data,
								 ULONG length, CDouble val);

	// dtor
	virtual ~CDXLDatumStatsDoubleMappable(){};

	// serialize the datum as the given element
	virtual void Serialize(CXMLSerializer *xml_serializer);

	// datum type
	virtual EdxldatumType
	GetDatumType() const
	{
		return CDXLDatum::EdxldatumStatsDoubleMappable;
	}

	// statistics related APIs

	// can datum be mapped to double
	virtual BOOL
	IsDatumMappableToDouble() const
	{
		return true;
	}

	// return the double mapping needed for statistics computation
	virtual CDouble
	GetDoubleMapping() const
	{
		return m_val;
	}

	// conversion function
	static CDXLDatumStatsDoubleMappable *
	Cast(CDXLDatum *dxl_datum)
	{
		GPOS_ASSERT(NULL != dxl_datum);
		GPOS_ASSERT(CDXLDatum::EdxldatumStatsDoubleMappable ==
					dxl_datum->GetDatumType());

		return dynamic_cast<CDXLDatumStatsDoubleMappable *>(dxl_datum);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLDatumStatsDoubleMappable_H

// EOF
