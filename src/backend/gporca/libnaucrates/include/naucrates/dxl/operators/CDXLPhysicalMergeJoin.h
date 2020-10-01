//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalMergeJoin.h
//
//	@doc:
//		Class for representing DXL merge join operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalMergeJoin_H
#define GPDXL_CDXLPhysicalMergeJoin_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysicalJoin.h"

namespace gpdxl
{
// indices of merge join elements in the children array
enum Edxlmj
{
	EdxlmjIndexProjList = 0,
	EdxlmjIndexFilter,
	EdxlmjIndexJoinFilter,
	EdxlmjIndexMergeCondList,
	EdxlmjIndexLeftChild,
	EdxlmjIndexRightChild,
	EdxlmjIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalMergeJoin
//
//	@doc:
//		Class for representing DXL merge operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalMergeJoin : public CDXLPhysicalJoin
{
private:
	// true if outer relation has unique values for the merge key
	BOOL m_is_unique_outer;

	// private copy ctor
	CDXLPhysicalMergeJoin(const CDXLPhysicalMergeJoin &);

public:
	// ctor
	CDXLPhysicalMergeJoin(CMemoryPool *mp, EdxlJoinType join_type,
						  BOOL is_unique_outer);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalMergeJoin *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalMergeJoin == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalMergeJoin *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalMergeJoin_H

// EOF
