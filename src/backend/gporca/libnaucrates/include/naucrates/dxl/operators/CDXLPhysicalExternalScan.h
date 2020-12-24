//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalExternalScan.h
//
//	@doc:
//		Class for representing DXL external scan operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalExternalScan_H
#define GPDXL_CDXLPhysicalExternalScan_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysicalTableScan.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalExternalScan
//
//	@doc:
//		Class for representing DXL external scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalExternalScan : public CDXLPhysicalTableScan
{
private:
public:
	CDXLPhysicalExternalScan(CDXLPhysicalExternalScan &) = delete;

	// ctors
	explicit CDXLPhysicalExternalScan(CMemoryPool *mp);

	CDXLPhysicalExternalScan(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLPhysicalExternalScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalExternalScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalExternalScan *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalExternalScan_H

// EOF
