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
	// private copy ctor
	CDXLScalarSubqueryNotExists(CDXLScalarSubqueryNotExists &);

public:
	// ctor/dtor
	explicit CDXLScalarSubqueryNotExists(CMemoryPool *mp);

	virtual ~CDXLScalarSubqueryNotExists();

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the operator
	const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarSubqueryNotExists *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryNotExists == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryNotExists *>(dxl_op);
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


#endif	// !GPDXL_CDXLScalarSubqueryNotExists_H

// EOF
