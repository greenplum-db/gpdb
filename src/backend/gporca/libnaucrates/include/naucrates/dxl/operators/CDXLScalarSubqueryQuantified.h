//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryQuantified.h
//
//	@doc:
//		Base class for ANY/ALL subqueries
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSubqueryQuantified_H
#define GPDXL_CDXLScalarSubqueryQuantified_H

#include "gpos/base.h"

#include "naucrates/md/IMDId.h"
#include "naucrates/md/CMDName.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalar.h"

namespace gpdxl
{
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubqueryQuantified
//
//	@doc:
//		Base class for ANY/ALL subqueries
//
//---------------------------------------------------------------------------
class CDXLScalarSubqueryQuantified : public CDXLScalar
{
public:
	// indices of the subquery elements in the children array
	enum Edxlsqquantified
	{
		EdxlsqquantifiedIndexScalar,
		EdxlsqquantifiedIndexRelational,
		EdxlsqquantifiedIndexSentinel
	};

private:
	// id of the scalar comparison operator
	IMDId *m_scalar_op_mdid;

	// name of scalar comparison operator
	CMDName *m_scalar_op_mdname;

	// colid produced by the relational child of the AnySubquery operator
	ULONG m_colid;

	// private copy ctor
	CDXLScalarSubqueryQuantified(CDXLScalarSubqueryQuantified &);

public:
	// ctor
	CDXLScalarSubqueryQuantified(CMemoryPool *mp, IMDId *scalar_op_mdid,
								 CMDName *mdname, ULONG colid);

	// dtor
	virtual ~CDXLScalarSubqueryQuantified();

	// scalar operator id
	IMDId *
	GetScalarOpMdId() const
	{
		return m_scalar_op_mdid;
	}

	// scalar operator name
	const CMDName *
	GetScalarOpMdName() const
	{
		return m_scalar_op_mdname;
	}

	// subquery colid
	ULONG
	GetColId() const
	{
		return m_colid;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarSubqueryQuantified *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubqueryAll == dxl_op->GetDXLOperator() ||
					EdxlopScalarSubqueryAny == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubqueryQuantified *>(dxl_op);
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

#endif	// !GPDXL_CDXLScalarSubqueryQuantified_H

// EOF
