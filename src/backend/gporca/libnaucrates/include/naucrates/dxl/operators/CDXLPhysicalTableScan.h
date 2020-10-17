//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalTableScan.h
//
//	@doc:
//		Class for representing DXL table scan operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalTableScan_H
#define GPDXL_CDXLPhysicalTableScan_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"


namespace gpdxl
{
// indices of table scan elements in the children array
enum Edxlts
{
	EdxltsIndexProjList = 0,
	EdxltsIndexFilter,
	EdxltsSentinel
};
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalTableScan
//
//	@doc:
//		Class for representing DXL table scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalTableScan : public CDXLPhysical
{
private:
	// table descriptor for the scanned table
	CDXLTableDescr *m_dxl_table_descr;

	// private copy ctor
	CDXLPhysicalTableScan(CDXLPhysicalTableScan &);

public:
	// ctors
	explicit CDXLPhysicalTableScan(CMemoryPool *mp);

	CDXLPhysicalTableScan(CMemoryPool *mp, CDXLTableDescr *table_descr);

	// dtor
	virtual ~CDXLPhysicalTableScan();

	// setters
	void SetTableDescriptor(CDXLTableDescr *);

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// table descriptor
	const CDXLTableDescr *GetDXLTableDescr();

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalTableScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalTableScan == dxl_op->GetDXLOperator() ||
					EdxlopPhysicalExternalScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalTableScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalTableScan_H

// EOF
