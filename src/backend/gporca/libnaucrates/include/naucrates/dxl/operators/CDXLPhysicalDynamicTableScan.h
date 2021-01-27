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

	// id of partition index structure
	ULONG m_part_index_id;

	// printable partition index id
	ULONG m_part_index_id_printable;

	// private copy ctor
	CDXLPhysicalDynamicTableScan(CDXLPhysicalDynamicTableScan &);

public:
	// ctor
	CDXLPhysicalDynamicTableScan(CMemoryPool *mp, CDXLTableDescr *table_descr,
								 ULONG part_idx_id,
								 ULONG part_idx_id_printable);

	// dtor
	virtual ~CDXLPhysicalDynamicTableScan();

	// operator type
	Edxlopid GetDXLOperator() const;

	// operator name
	const CWStringConst *GetOpNameStr() const;

	// table descriptor
	const CDXLTableDescr *GetDXLTableDescr() const;

	// partition index id
	ULONG GetPartIndexId() const;

	// printable partition index id
	ULONG GetPartIndexIdPrintable() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLPhysicalDynamicTableScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalDynamicTableScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalDynamicTableScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalDynamicTableScan_H

// EOF
