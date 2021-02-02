//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarSortColList.h
//
//	@doc:
//		Class for representing a list of sorting columns in a DXL Sort and Motion nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSortColList_H
#define GPDXL_CDXLScalarSortColList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSortColList
//
//	@doc:
//		Sorting column lists in DXL Sort And Motion nodes
//
//---------------------------------------------------------------------------
class CDXLScalarSortColList : public CDXLScalar
{
private:
public:
	CDXLScalarSortColList(CDXLScalarSortColList &) = delete;

	// ctor/dtor
	explicit CDXLScalarSortColList(CMemoryPool *mp);

	~CDXLScalarSortColList() override = default;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarSortColList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSortColList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSortColList *>(dxl_op);
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
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarSortColList_H

// EOF
