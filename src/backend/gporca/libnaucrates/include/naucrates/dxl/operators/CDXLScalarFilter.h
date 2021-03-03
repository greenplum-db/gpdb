//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarFilter.h
//
//	@doc:
//		Class for representing a scalar filter node inside DXL physical operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarFilter_H
#define GPDXL_CDXLScalarFilter_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarFilter
//
//	@doc:
//		Class for representing DXL filter operators
//
//---------------------------------------------------------------------------
class CDXLScalarFilter : public CDXLScalar
{
private:
public:
	CDXLScalarFilter(CDXLScalarFilter &) = delete;

	// ctor/dtor
	explicit CDXLScalarFilter(CMemoryPool *mp);

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarFilter *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarFilter == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarFilter *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarFilter_H

// EOF
