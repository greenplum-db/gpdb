//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalIndexScan.h
//
//	@doc:
//		Class for representing DXL index scan operators
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalIndexScan_H
#define GPDXL_CDXLPhysicalIndexScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"

namespace gpdxl
{
// indices of index scan elements in the children array
enum Edxlis
{
	EdxlisIndexProjList = 0,
	EdxlisIndexFilter,
	EdxlisIndexCondition,
	EdxlisSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalIndexScan
//
//	@doc:
//		Class for representing DXL index scan operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalIndexScan : public CDXLPhysical
{
private:
	// table descriptor for the scanned table
	CDXLTableDescr *m_dxl_table_descr;

	// index descriptor associated with the scanned table
	CDXLIndexDescr *m_dxl_index_descr;

	// scan direction of the index
	EdxlIndexScanDirection m_index_scan_dir;

	// private copy ctor
	CDXLPhysicalIndexScan(CDXLPhysicalIndexScan &);

public:
	//ctor
	CDXLPhysicalIndexScan(CMemoryPool *mp, CDXLTableDescr *table_descr,
						  CDXLIndexDescr *dxl_index_descr,
						  EdxlIndexScanDirection idx_scan_direction);

	//dtor
	virtual ~CDXLPhysicalIndexScan();

	// operator type
	virtual Edxlopid GetDXLOperator() const;

	// operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// index descriptor
	virtual const CDXLIndexDescr *GetDXLIndexDescr() const;

	//table descriptor
	virtual const CDXLTableDescr *GetDXLTableDescr() const;

	// scan direction
	virtual EdxlIndexScanDirection GetIndexScanDir() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLPhysicalIndexScan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalIndexScan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalIndexScan *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalIndexScan_H

// EOF
