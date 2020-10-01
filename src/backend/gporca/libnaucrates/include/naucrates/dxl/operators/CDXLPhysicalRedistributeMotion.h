//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalRedistributeMotion.h
//
//	@doc:
//		Class for representing DXL Redistribute motion operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalRedistributeMotion_H
#define GPDXL_CDXLPhysicalRedistributeMotion_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"

namespace gpdxl
{
// indices of redistribute motion elements in the children array
enum Edxlrm
{
	EdxlrmIndexProjList = 0,
	EdxlrmIndexFilter,
	EdxlrmIndexSortColList,
	EdxlrmIndexHashExprList,
	EdxlrmIndexChild,
	EdxlrmIndexSentinel
};



//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalRedistributeMotion
//
//	@doc:
//		Class for representing DXL redistribute motion operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalRedistributeMotion : public CDXLPhysicalMotion
{
private:
	// is this a duplicate sensitive redistribute motion
	BOOL m_is_duplicate_sensitive;

	// private copy ctor
	CDXLPhysicalRedistributeMotion(const CDXLPhysicalRedistributeMotion &);


public:
	// ctor
	CDXLPhysicalRedistributeMotion(CMemoryPool *mp,
								   BOOL is_duplicate_sensitive);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// does motion remove duplicates
	BOOL
	IsDuplicateSensitive() const
	{
		return m_is_duplicate_sensitive;
	}

	// index of relational child node in the children array
	virtual ULONG
	GetRelationChildIdx() const
	{
		return EdxlrmIndexChild;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalRedistributeMotion *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMotionRedistribute ==
					dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalRedistributeMotion *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalRedistributeMotion_H

// EOF
