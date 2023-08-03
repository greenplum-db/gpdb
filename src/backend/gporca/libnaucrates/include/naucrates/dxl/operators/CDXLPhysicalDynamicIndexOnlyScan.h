//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CDXLPhysicalDynamicIndexOnlyScan.h
//
//	@doc:
//		Class for representing DXL dynamic index only scan operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalDynamicIndexOnlyScan_H
#define GPDXL_CDXLPhysicalDynamicIndexOnlyScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalDynamicIndexScan.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalDynamicIndexOnlyScan
//
//	@doc:
//		Class for representing DXL dynamic index only scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalDynamicIndexOnlyScan : public CDXLPhysicalDynamicIndexScan
{
public:
	CDXLPhysicalDynamicIndexOnlyScan(CDXLPhysicalDynamicIndexOnlyScan &) =
		delete;

	//ctor
	CDXLPhysicalDynamicIndexOnlyScan(CMemoryPool *mp,
									 CDXLTableDescr *table_descr,
									 CDXLIndexDescr *dxl_index_descr,
									 EdxlIndexScanDirection idx_scan_direction,
									 IMdIdArray *part_mdids,
									 ULongPtrArray *selector_ids)
		: CDXLPhysicalDynamicIndexScan(mp, table_descr, dxl_index_descr,
									   idx_scan_direction, part_mdids,
									   selector_ids)
	{
	}

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLPhysicalDynamicIndexOnlyScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDynamicIndexOnlyScan ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDynamicIndexOnlyScan *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalDynamicIndexOnlyScan_H

// EOF
