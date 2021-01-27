//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarJoinFilter.h
//
//	@doc:
//		Class for representing a join filter node inside DXL join operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarJoinFilter_H
#define GPDXL_CDXLScalarJoinFilter_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarFilter.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarJoinFilter
//
//	@doc:
//		Class for representing DXL join condition operators
//
//---------------------------------------------------------------------------
class CDXLScalarJoinFilter : public CDXLScalarFilter
{
private:
	// private copy ctor
	CDXLScalarJoinFilter(CDXLScalarJoinFilter &);

public:
	// ctor/dtor
	explicit CDXLScalarJoinFilter(CMemoryPool *mp);

	// accessors
	Edxlopid GetDXLOperator() const;
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarJoinFilter *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarJoinFilter == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarJoinFilter *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		GPOS_ASSERT(!"Invalid function call for a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl
#endif	// !GPDXL_CDXLScalarJoinFilter_H

// EOF
