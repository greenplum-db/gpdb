//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubquery.h
//
//	@doc:
//		Class for representing subqueries computing scalar values
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSubquery_H
#define GPDXL_CDXLScalarSubquery_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubquery
//
//	@doc:
//		Class for representing subqueries computing scalar values
//
//---------------------------------------------------------------------------
class CDXLScalarSubquery : public CDXLScalar
{
private:
	// id of column computed by the subquery
	ULONG m_colid;

public:
	CDXLScalarSubquery(CDXLScalarSubquery &) = delete;

	// ctor/dtor
	CDXLScalarSubquery(CMemoryPool *mp, ULONG colid);

	~CDXLScalarSubquery() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// colid of subquery column
	ULONG
	GetColId() const
	{
		return m_colid;
	}

	// name of the operator
	const CWStringConst *GetOpNameStr() const override;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const override;

	// conversion function
	static CDXLScalarSubquery *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubquery == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubquery *>(dxl_op);
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

#endif	// !GPDXL_CDXLScalarSubquery_H

// EOF
