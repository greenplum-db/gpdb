//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLLogicalForeignGet.h
//
//	@doc:
//		Class for representing DXL logical foreign get operator, for reading
//		from foreign tables
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLLogicalForeignGet_H
#define GPDXL_CDXLLogicalForeignGet_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogicalGet.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLLogicalForeignGet
//
//	@doc:
//		Class for representing DXL logical foreign get operator
//
//---------------------------------------------------------------------------
class CDXLLogicalForeignGet : public CDXLLogicalGet
{
private:
public:
	CDXLLogicalForeignGet(CDXLLogicalForeignGet &) = delete;

	// ctor
	CDXLLogicalForeignGet(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLLogicalForeignGet *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopLogicalForeignGet == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLLogicalForeignGet *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLLogicalForeignGet_H

// EOF
