//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalRandomMotion.h
//
//	@doc:
//		Class for representing DXL random motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalRandomMotion_H
#define GPDXL_CDXLPhysicalRandomMotion_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"

namespace gpdxl
{
// indexes of random motion elements in the children array
enum Edxlrandomm
{
	EdxlrandommIndexProjList = 0,
	EdxlrandommIndexFilter,
	EdxlrandommIndexSortColList,
	EdxlrandommIndexChild,
	EdxlrandommIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalRandomMotion
//
//	@doc:
//		Class for representing DXL random motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalRandomMotion : public CDXLPhysicalMotion
{
private:
	// is distribution duplicate sensitive
	BOOL m_is_duplicate_sensitive;

	// private copy ctor
	CDXLPhysicalRandomMotion(const CDXLPhysicalRandomMotion &);

public:
	// ctor
	CDXLPhysicalRandomMotion(CMemoryPool *mp, BOOL is_duplicate_sensitive);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// is operator duplicate sensitive
	BOOL
	IsDuplicateSensitive() const
	{
		return m_is_duplicate_sensitive;
	}

	// index of relational child node in the children array
	virtual ULONG
	GetRelationChildIdx() const
	{
		return EdxlrandommIndexChild;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalRandomMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionRandom == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLPhysicalRandomMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalRandomMotion_H

// EOF
