//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
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
	// private copy ctor
	CDXLScalarRecheckCondFilter(CDXLScalarRecheckCondFilter &);

public:
	// ctor
	explicit CDXLScalarRecheckCondFilter(CMemoryPool *mp) : CDXLScalarFilter(mp)
	{
	}

	// operator identity
	virtual Edxlopid
	GetDXLOperator() const
	{
		return EdxlopScalarRecheckCondFilter;
	}

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
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
