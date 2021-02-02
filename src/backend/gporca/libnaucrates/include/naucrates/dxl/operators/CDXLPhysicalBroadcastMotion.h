//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalBroadcastMotion.h
//
//	@doc:
//		Class for representing DXL Broadcast motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalBroadcastMotion_H
#define GPDXL_CDXLPhysicalBroadcastMotion_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"

namespace gpdxl
{
// indices of broadcast motion elements in the children array
enum Edxlbm
{
	EdxlbmIndexProjList = 0,
	EdxlbmIndexFilter,
	EdxlbmIndexSortColList,
	EdxlbmIndexChild,
	EdxlbmIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalBroadcastMotion
//
//	@doc:
//		Class for representing DXL broadcast motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalBroadcastMotion : public CDXLPhysicalMotion
{
private:
public:
	CDXLPhysicalBroadcastMotion(const CDXLPhysicalBroadcastMotion &) = delete;

	// ctor/dtor
	explicit CDXLPhysicalBroadcastMotion(CMemoryPool *mp);

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;

	// index of relational child node in the children array
	ULONG
	GetRelationChildIdx() const override
	{
		return EdxlbmIndexChild;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLPhysicalBroadcastMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionBroadcast == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLPhysicalBroadcastMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalBroadcastMotion_H

// EOF
