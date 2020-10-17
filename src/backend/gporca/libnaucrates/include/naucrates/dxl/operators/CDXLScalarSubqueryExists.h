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
	// private copy ctor
	CDXLScalarSubqueryExists(CDXLScalarSubqueryExists &);

public:
	// ctor/dtor
	explicit CDXLScalarSubqueryExists(CMemoryPool *mp);

	virtual ~CDXLScalarSubqueryExists();

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the operator
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarSubqueryExists *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryExists == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryExists *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const
	{
		return true;
	}

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarSubqueryExists_H

// EOF
