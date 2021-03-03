//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarProjList.h
//
//	@doc:
//		Class for representing DXL projection lists.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarProjList_H
#define GPDXL_CDXLScalarProjList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarProjList
//
//	@doc:
//		Projection list in operators.
//
//---------------------------------------------------------------------------
class CDXLScalarProjList : public CDXLScalar
{
private:
public:
	CDXLScalarProjList(CDXLScalarProjList &) = delete;

	// ctor/dtor
	explicit CDXLScalarProjList(CMemoryPool *mp);

	~CDXLScalarProjList() override = default;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarProjList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarProjectList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarProjList *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		GPOS_ASSERT(!"Invalid function call on a container operator");
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

#endif	// !GPDXL_CDXLScalarProjList_H

// EOF
