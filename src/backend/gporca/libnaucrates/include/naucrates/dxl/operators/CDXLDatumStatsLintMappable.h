//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLDatumStatsLintMappable.h
//
//	@doc:
//		Class for representing DXL datum of types having LINT mapping
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumStatsLintMappable_H
#define GPDXL_CDXLDatumStatsLintMappable_H

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
//		CDXLDatumStatsLintMappable
//
//	@doc:
//		Class for representing DXL datum of types having LINT mapping
//
//---------------------------------------------------------------------------
class CDXLDatumStatsLintMappable : public CDXLDatumGeneric
{
private:
	// for statistics computation, map to LINT
	LINT m_val;

public:
	CDXLDatumStatsLintMappable(const CDXLDatumStatsLintMappable &) = delete;

	// ctor
	CDXLDatumStatsLintMappable(CMemoryPool *mp, IMDId *mdid_type,
							   INT type_modifier, BOOL is_null,
							   BYTE *byte_array, ULONG length, LINT value);

	// dtor
	~CDXLDatumStatsLintMappable() override = default;

	// serialize the datum as the given element
	void Serialize(CXMLSerializer *xml_serializer) override;

	// datum type
	EdxldatumType
	GetDatumType() const override
	{
		return CDXLDatum::EdxldatumStatsLintMappable;
	}

	// conversion function
	static CDXLDatumStatsLintMappable *
	Cast(CDXLDatum *dxl_datum)
	{
		GPOS_ASSERT(NULL != dxl_datum);
		GPOS_ASSERT(CDXLDatum::EdxldatumStatsLintMappable ==
					dxl_datum->GetDatumType());

		return dynamic_cast<CDXLDatumStatsLintMappable *>(dxl_datum);
	}

	// statistics related APIs

	// can datum be mapped to LINT
	BOOL
	IsDatumMappableToLINT() const override
	{
		return true;
	}

	// return the LINT mapping needed for statistics computation
	LINT
	GetLINTMapping() const override
	{
		return m_val;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLDatumStatsLintMappable_H

// EOF
