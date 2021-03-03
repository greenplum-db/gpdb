//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarHashExprList.h
//
//	@doc:
//		Class for representing hash expressions list in DXL Redistribute motion nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarHashExprList_H
#define GPDXL_CDXLScalarHashExprList_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalar.h"


namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarHashExprList
//
//	@doc:
//		Hash expressions list in Redistribute motion nodes
//
//---------------------------------------------------------------------------
class CDXLScalarHashExprList : public CDXLScalar
{
private:
public:
	CDXLScalarHashExprList(CDXLScalarHashExprList &) = delete;

	// ctor/dtor
	explicit CDXLScalarHashExprList(CMemoryPool *mp);

	~CDXLScalarHashExprList() override = default;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	// conversion function
	static CDXLScalarHashExprList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarHashExprList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarHashExprList *>(dxl_op);
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
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarHashExprList_H

// EOF
