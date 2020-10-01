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
	// private copy ctor
	CDXLScalarHashExprList(CDXLScalarHashExprList &);

public:
	// ctor/dtor
	explicit CDXLScalarHashExprList(CMemoryPool *mp);

	virtual ~CDXLScalarHashExprList(){};

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the operator
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *node) const;

	// conversion function
	static CDXLScalarHashExprList *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarHashExprList == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarHashExprList *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		GPOS_ASSERT(!"Invalid function call on a container operator");
		return false;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure
	void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarHashExprList_H

// EOF
