//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLScalarRecheckCondFilter.h
//
//	@doc:
//		Filter for rechecking an index condition on the operator upstream of the index scan
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarRecheckCondFilter_H
#define GPDXL_CDXLScalarRecheckCondFilter_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarFilter.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarRecheckCondFilter
//
//	@doc:
//		Filter for rechecking an index condition on the operator upstream of the bitmap index scan
//
//---------------------------------------------------------------------------
class CDXLScalarRecheckCondFilter : public CDXLScalarFilter
{
private:
public:
	CDXLScalarRecheckCondFilter(CDXLScalarRecheckCondFilter &) = delete;

	// ctor
	explicit CDXLScalarRecheckCondFilter(CMemoryPool *mp) : CDXLScalarFilter(mp)
	{
	}

	// operator identity
	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopScalarRecheckCondFilter;
	}

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

	// conversion function
	static CDXLScalarRecheckCondFilter *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarRecheckCondFilter == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarRecheckCondFilter *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarRecheckCondFilter_H

// EOF
