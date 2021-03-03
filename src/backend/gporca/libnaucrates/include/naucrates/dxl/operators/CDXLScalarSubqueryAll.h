//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryAll.h
//
//	@doc:
//		Class for representing ALL subqueries
//---------------------------------------------------------------------------



#ifndef GPDXL_CDXLScalarSubqueryAll_H
#define GPDXL_CDXLScalarSubqueryAll_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalarSubqueryQuantified.h"
#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubqueryAll
//
//	@doc:
//		Class for representing ALL subqueries
//
//---------------------------------------------------------------------------
class CDXLScalarSubqueryAll : public CDXLScalarSubqueryQuantified
{
private:
public:
	CDXLScalarSubqueryAll(CDXLScalarSubqueryAll &) = delete;

	// ctor
	CDXLScalarSubqueryAll(CMemoryPool *mp, IMDId *scalar_op_mdid,
						  CMDName *mdname, ULONG colid);

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// conversion function
	static CDXLScalarSubqueryAll *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryAll == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryAll *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL
	HasBoolResult(CMDAccessor *	 //md_accessor
	) const override
	{
		return true;
	}
};
}  // namespace gpdxl


#endif	// !GPDXL_CDXLScalarSubqueryAll_H

// EOF
