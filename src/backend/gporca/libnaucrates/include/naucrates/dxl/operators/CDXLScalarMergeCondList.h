//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarMergeCondList.h
//
//	@doc:
//		Class for representing the list of merge conditions in DXL Merge join nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarMergeCondList_H
#define GPDXL_CDXLScalarMergeCondList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarMergeCondList
//
//	@doc:
//		Class for representing the list of merge conditions in DXL Merge join nodes.
//
//---------------------------------------------------------------------------
class CDXLScalarMergeCondList : public CDXLScalar
{
private:
public:
	CDXLScalarMergeCondList(CDXLScalarMergeCondList &) = delete;

	// ctor
	explicit CDXLScalarMergeCondList(CMemoryPool *mp);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarMergeCondList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarMergeCondList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarMergeCondList *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarMergeCondList_H

// EOF
