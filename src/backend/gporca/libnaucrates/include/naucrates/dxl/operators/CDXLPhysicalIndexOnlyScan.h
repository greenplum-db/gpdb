//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalIndexOnlyScan.h
//
//	@doc:
//		Class for representing DXL index only scan operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalIndexOnlyScan_H
#define GPDXL_CDXLPhysicalIndexOnlyScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLPhysicalIndexScan.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalIndexOnlyScan
//
//	@doc:
//		Class for representing DXL index only scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalIndexOnlyScan : public CDXLPhysicalIndexScan
{
private:
public:
	CDXLPhysicalIndexOnlyScan(CDXLPhysicalIndexOnlyScan &) = delete;

	//ctor
	CDXLPhysicalIndexOnlyScan(CMemoryPool *mp, CDXLTableDescr *table_descr,
							  CDXLIndexDescr *dxl_index_descr,
							  EdxlIndexScanDirection idx_scan_direction);

	//dtor
	~CDXLPhysicalIndexOnlyScan() override = default;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLPhysicalIndexOnlyScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalIndexOnlyScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalIndexOnlyScan *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalIndexOnlyScan_H

// EOF
