//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalHashJoin.h
//
//	@doc:
//		Class for representing DXL hashjoin operators.
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLPhysicalHashJoin_H
#define GPDXL_CDXLPhysicalHashJoin_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalJoin.h"

namespace gpdxl
{
// indices of hash join elements in the children array
enum Edxlhj
{
	EdxlhjIndexProjList = 0,
	EdxlhjIndexFilter,
	EdxlhjIndexJoinFilter,
	EdxlhjIndexHashCondList,
	EdxlhjIndexHashLeft,
	EdxlhjIndexHashRight,
	EdxlhjIndexSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLPhysicalHashJoin
//
//	@doc:
//		Class for representing DXL hashjoin operators
//
//---------------------------------------------------------------------------
class CDXLPhysicalHashJoin : public CDXLPhysicalJoin
{
private:
	// private copy ctor
	CDXLPhysicalHashJoin(const CDXLPhysicalHashJoin &);

public:
	// ctor/dtor
	CDXLPhysicalHashJoin(CMemoryPool *mp, EdxlJoinType join_type);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLPhysicalHashJoin *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalHashJoin == dxl_op->GetDXLOperator());
		return dynamic_cast<CDXLPhysicalHashJoin *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLPhysicalHashJoin_H

// EOF
