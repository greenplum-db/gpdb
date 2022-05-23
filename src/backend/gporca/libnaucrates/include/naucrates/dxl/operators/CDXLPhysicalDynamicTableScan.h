//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalDynamicTableScan.h
//
//	@doc:
//		Class for representing DXL dynamic table scan operators
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalDynamicTableScan_H
#define GPDXL_CDXLPhysicalDynamicTableScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"


namespace gpdxl
{
// indices of dynamic table scan elements in the children array
enum Edxldts
{
	EdxldtsIndexProjList = 0,
	EdxldtsIndexFilter,
	EdxldtsSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalDynamicTableScan
//
//	@doc:
//		Class for representing DXL dynamic table scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalDynamicTableScan : public CDXLPhysical
{
private:
	// table descriptor for the scanned table
	CDXLTableDescr *m_dxl_table_descr;

	IMdIdArray *m_part_mdids;

	ULongPtrArray *m_selector_ids = nullptr;

public:
	CDXLPhysicalDynamicTableScan(CDXLPhysicalDynamicTableScan &) = delete;

	// ctor
	CDXLPhysicalDynamicTableScan(CMemoryPool *mp, CDXLTableDescr *table_descr,
								 IMdIdArray *part_mdids,
								 ULongPtrArray *selector_ids);

	// dtor
	~CDXLPhysicalDynamicTableScan() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// table descriptor
	const CDXLTableDescr *GetDXLTableDescr() const;

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
	static CDXLPhysicalDynamicTableScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDynamicTableScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDynamicTableScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalDynamicTableScan_H

// EOF
