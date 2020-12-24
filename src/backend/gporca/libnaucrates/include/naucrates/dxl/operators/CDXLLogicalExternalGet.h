//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLLogicalExternalGet.h
//
//	@doc:
//		Class for representing DXL logical external get operator, for reading
//		from external tables
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalExternalGet_H
#define GPDXL_CDXLLogicalExternalGet_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLLogicalGet.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalExternalGet
//
//	@doc:
//		Class for representing DXL logical external get operator
//
//---------------------------------------------------------------------------
class CDXLLogicalExternalGet : public CDXLLogicalGet
{
private:
public:
	CDXLLogicalExternalGet(CDXLLogicalExternalGet &) = delete;

	// ctor
	CDXLLogicalExternalGet(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLLogicalExternalGet *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopLogicalExternalGet == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalExternalGet *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalExternalGet_H

// EOF
