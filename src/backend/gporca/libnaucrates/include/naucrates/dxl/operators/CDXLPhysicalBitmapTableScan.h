//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalBitmapTableScan.h
//
//	@doc:
//		Class for representing DXL bitmap table scan operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalBitmapTableScan_H
#define GPDXL_CDXLPhysicalBitmapTableScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalAbstractBitmapScan.h"

namespace gpdxl
{
using namespace gpos;

// fwd declarations
class CDXLTableDescr;
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalBitmapTableScan
//
//	@doc:
//		Class for representing DXL bitmap table scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalBitmapTableScan : public CDXLPhysicalAbstractBitmapScan
{
private:
public:
	CDXLPhysicalBitmapTableScan(const CDXLPhysicalBitmapTableScan &) = delete;

	// ctors
	CDXLPhysicalBitmapTableScan(CMemoryPool *mp, CDXLTableDescr *table_descr)
		: CDXLPhysicalAbstractBitmapScan(mp, table_descr)
	{
	}

	// dtor
	~CDXLPhysicalBitmapTableScan() override = default;

	// operator type
	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopPhysicalBitmapTableScan;
	}

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalBitmapTableScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalBitmapTableScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalBitmapTableScan *>(dxl_op);
	}

};	// class CDXLPhysicalBitmapTableScan
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalBitmapTableScan_H

// EOF
