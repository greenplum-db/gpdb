//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalDynamicBitmapTableScan.h
//
//	@doc:
//		Class for representing dynamic DXL bitmap table scan operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalDynamicBitmapTableScan_H
#define GPDXL_CDXLPhysicalDynamicBitmapTableScan_H

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
//		CDXLPhysicalDynamicBitmapTableScan
//
//	@doc:
//		Class for representing DXL bitmap table scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalDynamicBitmapTableScan : public CDXLPhysicalAbstractBitmapScan
{
private:
	IMdIdArray *m_part_mdids;

	ULongPtrArray *m_selector_ids = nullptr;

public:
	CDXLPhysicalDynamicBitmapTableScan(
		const CDXLPhysicalDynamicBitmapTableScan &) = delete;

	// ctor
	CDXLPhysicalDynamicBitmapTableScan(CMemoryPool *mp,
									   CDXLTableDescr *table_descr,
									   IMdIdArray *part_mdids,
									   ULongPtrArray *selector_ids)
		: CDXLPhysicalAbstractBitmapScan(mp, table_descr),
		  m_part_mdids(part_mdids),
		  m_selector_ids(selector_ids)
	{
		GPOS_ASSERT(nullptr != table_descr);
	}

	// dtor
	~CDXLPhysicalDynamicBitmapTableScan() override;

	// operator type
	Edxlopid
	GetDXLOperator() const override
	{
		return EdxlopPhysicalDynamicBitmapTableScan;
	}

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	IMdIdArray *GetParts() const;

	const ULongPtrArray *
	GetSelectorIds() const
	{
		return m_selector_ids;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLPhysicalDynamicBitmapTableScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDynamicBitmapTableScan ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDynamicBitmapTableScan *>(dxl_op);
	}

};	// class CDXLPhysicalDynamicBitmapTableScan
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalDynamicBitmapTableScan_H

// EOF
