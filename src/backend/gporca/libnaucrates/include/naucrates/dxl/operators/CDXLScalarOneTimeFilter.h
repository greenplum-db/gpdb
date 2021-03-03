//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarOneTimeFilter.h
//
//	@doc:
//		Class for representing a scalar filter that is executed once inside DXL physical operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarOneTimeFilter_H
#define GPDXL_CDXLScalarOneTimeFilter_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarFilter.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarOneTimeFilter
//
//	@doc:
//		Class for representing DXL filter operators
//
//---------------------------------------------------------------------------
class CDXLScalarOneTimeFilter : public CDXLScalarFilter
{
private:
public:
	CDXLScalarOneTimeFilter(CDXLScalarOneTimeFilter &) = delete;

	// ctor
	explicit CDXLScalarOneTimeFilter(CMemoryPool *mp);

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLScalarOneTimeFilter *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarOneTimeFilter == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarOneTimeFilter *>(dxl_op);
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarOneTimeFilter_H

// EOF
