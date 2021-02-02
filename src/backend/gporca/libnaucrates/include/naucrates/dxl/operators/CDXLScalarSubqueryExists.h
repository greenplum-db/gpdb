//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryExists.h
//
//	@doc:
//		Class for representing EXISTS subqueries
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSubqueryExists_H
#define GPDXL_CDXLScalarSubqueryExists_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubqueryExists
//
//	@doc:
//		Class for representing EXISTS subqueries
//
//---------------------------------------------------------------------------
class CDXLScalarSubqueryExists : public CDXLScalar
{
private:
public:
	CDXLScalarSubqueryExists(CDXLScalarSubqueryExists &) = delete;

	// ctor/dtor
	explicit CDXLScalarSubqueryExists(CMemoryPool *mp);

	~CDXLScalarSubqueryExists() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarSubqueryExists *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryExists == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryExists *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarSubqueryExists_H

// EOF
