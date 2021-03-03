//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalRoutedDistributeMotion.h
//
//	@doc:
//		Class for representing DXL routed redistribute motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalRoutedDistributeMotion_H
#define GPDXL_CDXLPhysicalRoutedDistributeMotion_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalRoutedDistributeMotion
//
//	@doc:
//		Class for representing DXL routed redistribute motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalRoutedDistributeMotion : public CDXLPhysicalMotion
{
private:
	// indexes of the routed motion elements in the children array
	enum Edxlroutedm
	{
		EdxlroutedmIndexProjList = 0,
		EdxlroutedmIndexFilter,
		EdxlroutedmIndexSortColList,
		EdxlroutedmIndexChild,
		EdxlroutedmIndexSentinel
	};

	// segment id column
	ULONG m_segment_id_col;

public:
	CDXLPhysicalRoutedDistributeMotion(
		const CDXLPhysicalRoutedDistributeMotion &) = delete;

	// ctor
	CDXLPhysicalRoutedDistributeMotion(CMemoryPool *mp, ULONG segment_id_col);

	// operator type
	Edxlopid GetDXLOperator() const override;

	// operator name
	const CWStringConst *GetOpNameStr() const override;

	// segment id column
	ULONG
	SegmentIdCol() const
	{
		return m_segment_id_col;
	}

	// index of relational child node in the children array
	ULONG
	GetRelationChildIdx() const override
	{
		return EdxlroutedmIndexChild;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalRoutedDistributeMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionRoutedDistribute ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalRoutedDistributeMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalRoutedDistributeMotion_H

// EOF
