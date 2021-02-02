//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryNotExists.h
//
//	@doc:
//		Class for representing NOT EXISTS subqueries
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSubqueryNotExists_H
#define GPDXL_CDXLScalarSubqueryNotExists_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubqueryNotExists
//
//	@doc:
//		Class for representing NOT EXISTS subqueries
//
//---------------------------------------------------------------------------
class CDXLScalarSubqueryNotExists : public CDXLScalar
{
private:
public:
	CDXLScalarSubqueryNotExists(CDXLScalarSubqueryNotExists &) = delete;

	// ctor/dtor
	explicit CDXLScalarSubqueryNotExists(CMemoryPool *mp);

	~CDXLScalarSubqueryNotExists() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarSubqueryNotExists *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryNotExists == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryNotExists *>(dxl_op);
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


#endif	// !GPDXL_CDXLScalarSubqueryNotExists_H

// EOF
