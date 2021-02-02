//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumInt8.h
//
//	@doc:
//		Class for representing DXL datums of type long int
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumInt8_H
#define GPDXL_CDXLDatumInt8_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLDatumInt8
//
//	@doc:
//		Class for representing DXL datums of type long int
//
//---------------------------------------------------------------------------
class CDXLDatumInt8 : public CDXLDatum
{
private:
	// long int value
	LINT m_val;

public:
	CDXLDatumInt8(const CDXLDatumInt8 &) = delete;

	// ctor
	CDXLDatumInt8(CMemoryPool *mp, IMDId *mdid_type, BOOL is_null, LINT val);

	// dtor
	~CDXLDatumInt8() override = default;

	// accessor of value
	LINT Value() const;

	// serialize the datum as the given element
	void Serialize(CXMLSerializer *xml_serializer) override;

	// datum type
	EdxldatumType
	GetDatumType() const override
	{
		return CDXLDatum::EdxldatumInt8;
	}

	// conversion function
	static CDXLDatumInt8 *
	Cast(CDXLDatum *dxl_datum)
	{
		GPOS_ASSERT(nullptr != dxl_datum);
		GPOS_ASSERT(CDXLDatum::EdxldatumInt8 == dxl_datum->GetDatumType());

		return dynamic_cast<CDXLDatumInt8 *>(dxl_datum);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLDatumInt8_H

// EOF
