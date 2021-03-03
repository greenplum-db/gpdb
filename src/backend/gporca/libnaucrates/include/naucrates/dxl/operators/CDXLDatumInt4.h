//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumInt4.h
//
//	@doc:
//		Class for representing DXL integer datum
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumInt4_H
#define GPDXL_CDXLDatumInt4_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLDatumInt4
//
//	@doc:
//		Class for representing DXL integer datums
//
//---------------------------------------------------------------------------
class CDXLDatumInt4 : public CDXLDatum
{
private:
	// int4 value
	INT m_val;

public:
	CDXLDatumInt4(const CDXLDatumInt4 &) = delete;

	// ctor
	CDXLDatumInt4(CMemoryPool *mp, IMDId *mdid_type, BOOL is_null, INT val);

	// dtor
	~CDXLDatumInt4() override = default;

	// accessor of int value
	INT Value() const;

	// serialize the datum as the given element
	void Serialize(CXMLSerializer *xml_serializer) override;

	// datum type
	EdxldatumType
	GetDatumType() const override
	{
		return CDXLDatum::EdxldatumInt4;
	}

	// conversion function
	static CDXLDatumInt4 *
	Cast(CDXLDatum *dxl_datum)
	{
		GPOS_ASSERT(nullptr != dxl_datum);
		GPOS_ASSERT(CDXLDatum::EdxldatumInt4 == dxl_datum->GetDatumType());

		return dynamic_cast<CDXLDatumInt4 *>(dxl_datum);
	}
};
}  // namespace gpdxl



#endif	// !GPDXL_CDXLDatumInt4_H

// EOF
