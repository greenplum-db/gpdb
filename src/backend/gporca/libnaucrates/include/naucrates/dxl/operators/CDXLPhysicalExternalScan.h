//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
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
	// private copy ctor
	CDXLPhysicalExternalScan(CDXLPhysicalExternalScan &);

public:
	// ctors
	explicit CDXLPhysicalExternalScan(CMemoryPool *mp);

	CDXLPhysicalExternalScan(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

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
