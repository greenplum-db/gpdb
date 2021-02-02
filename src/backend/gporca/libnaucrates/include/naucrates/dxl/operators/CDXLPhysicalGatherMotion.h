//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalGatherMotion.h
//
//	@doc:
//		Class for representing DXL Gather motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalGatherMotion_H
#define GPDXL_CDXLPhysicalGatherMotion_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"

namespace gpdxl
{
// indices of gather motion elements in the children array
enum Edxlgm
{
	EdxlgmIndexProjList = 0,
	EdxlgmIndexFilter,
	EdxlgmIndexSortColList,
	EdxlgmIndexChild,
	EdxlgmIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalGatherMotion
//
//	@doc:
//		Class for representing DXL gather motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalGatherMotion : public CDXLPhysicalMotion
{
private:
public:
	CDXLPhysicalGatherMotion(const CDXLPhysicalGatherMotion &) = delete;

	// ctor/dtor
	CDXLPhysicalGatherMotion(CMemoryPool *mp);

	~CDXLPhysicalGatherMotion() override = default;

	// accessors
	Edxlopid GetDXLOperator() const override;
	const CWStringConst *GetOpNameStr() const override;
	INT IOutputSegIdx() const;

	// index of relational child node in the children array
	ULONG
	GetRelationChildIdx() const override
	{
		return EdxlgmIndexChild;
	}

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLPhysicalGatherMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionGather == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalGatherMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalGatherMotion_H

// EOF
