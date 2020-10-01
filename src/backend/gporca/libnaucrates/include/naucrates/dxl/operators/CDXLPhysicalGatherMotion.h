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
	// private copy ctor
	CDXLPhysicalGatherMotion(const CDXLPhysicalGatherMotion &);

public:
	// ctor/dtor
	CDXLPhysicalGatherMotion(CMemoryPool *mp);

	virtual ~CDXLPhysicalGatherMotion(){};

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;
	INT IOutputSegIdx() const;

	// index of relational child node in the children array
	virtual ULONG
	GetRelationChildIdx() const
	{
		return EdxlgmIndexChild;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLPhysicalGatherMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionGather == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalGatherMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalGatherMotion_H

// EOF
