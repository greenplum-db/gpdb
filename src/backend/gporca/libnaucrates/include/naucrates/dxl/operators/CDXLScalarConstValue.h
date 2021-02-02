//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarConstValue.h
//
//	@doc:
//		Class for representing DXL Const Value
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarConstValue_H
#define GPDXL_CDXLScalarConstValue_H

#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLDatum.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarConstValue
//
//	@doc:
//		Class for representing DXL scalar Const value
//
//---------------------------------------------------------------------------
class CDXLScalarConstValue : public CDXLScalar
{
private:
	CDXLDatum *m_dxl_datum;

public:
	CDXLScalarConstValue(const CDXLScalarConstValue &) = delete;

	// ctor/dtor
	CDXLScalarConstValue(CMemoryPool *mp, CDXLDatum *dxl_datum);

	~CDXLScalarConstValue() override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// return the datum value
	const CDXLDatum *
	GetDatumVal() const
	{
		return m_dxl_datum;
	}

	// DXL Operator ID
	Edxlopid GetDXLOperator() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarConstValue *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarConstValue == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarConstValue *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarConstValue_H

// EOF
