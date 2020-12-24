//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CDXLPhysicalPartitionSelector.h
//
//	@doc:
//		Class for representing DXL partition selector
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalPartitionSelector_H
#define GPDXL_CDXLPhysicalPartitionSelector_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
// indices of partition selector elements in the children array
enum Edxlps
{
	EdxlpsIndexProjList = 0,
	EdxlpsIndexEqFilters,
	EdxlpsIndexFilters,
	EdxlpsIndexResidualFilter,
	EdxlpsIndexPropExpr,
	EdxlpsIndexPrintableFilter,
	EdxlpsIndexChild,
	EdxlpsIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalPartitionSelector
//
//	@doc:
//		Class for representing DXL partition selector
//
//---------------------------------------------------------------------------
class CDXLPhysicalPartitionSelector : public CDXLPhysical
{
private:
	// table id
	IMDId *m_rel_mdid;

	// number of partitioning levels
	ULONG m_num_of_part_levels;

	// scan id
	ULONG m_scan_id;

public:
	CDXLPhysicalPartitionSelector(CDXLPhysicalPartitionSelector &) = delete;

	// ctor
	CDXLPhysicalPartitionSelector(CMemoryPool *mp, IMDId *mdid_rel,
								  ULONG num_of_part_levels, ULONG scan_id);

	// dtor
	~CDXLPhysicalPartitionSelector() override;

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// table id
	IMDId *
	GetRelMdId() const
	{
		return m_rel_mdid;
	}

	// number of partitioning levels
	ULONG
	GetPartitioningLevel() const
	{
		return m_num_of_part_levels;
	}

	// scan id
	ULONG
	ScanId() const
	{
		return m_scan_id;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG

	// conversion function
	static CDXLPhysicalPartitionSelector *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalPartitionSelector ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalPartitionSelector *>(dxl_op);
	}
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalPartitionSelector_H

// EOF
