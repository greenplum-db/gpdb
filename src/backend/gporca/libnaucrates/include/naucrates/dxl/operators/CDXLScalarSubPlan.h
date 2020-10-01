//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLScalarSubPlan.h
//
//	@doc:
//		Class for representing DXL Scalar SubPlan operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarSubPlan_H
#define GPDXL_CDXLScalarSubPlan_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/dxl/operators/CDXLColRef.h"

namespace gpdxl
{
using namespace gpos;

// indices of SubPlan elements in the children array
enum EdxlSubPlan
{
	EdxlSubPlanIndexChildPlan,
	EdxlSubPlanIndexSentinel
};

// subplan type
enum EdxlSubPlanType
{
	EdxlSubPlanTypeScalar = 0,	// subplan for scalar subquery
	EdxlSubPlanTypeExists,		// subplan for exists subquery
	EdxlSubPlanTypeNotExists,	// subplan for not exists subquery
	EdxlSubPlanTypeAny,			// subplan for quantified (ANY/IN) subquery
	EdxlSubPlanTypeAll,			// subplan for quantified (ALL/NOT IN) subquery

	EdxlSubPlanTypeSentinel
};

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarSubPlan
//
//	@doc:
//		Class for representing DXL scalar SubPlan operator
//
//---------------------------------------------------------------------------
class CDXLScalarSubPlan : public CDXLScalar
{
private:
	// catalog MDId of the first column type
	IMDId *m_first_col_type_mdid;

	// array of outer column references
	CDXLColRefArray *m_dxl_colref_array;

	// subplan type
	EdxlSubPlanType m_dxl_subplan_type;

	// test expression -- not null if quantified/existential subplan
	CDXLNode *m_dxlnode_test_expr;

	// private copy ctor
	CDXLScalarSubPlan(CDXLScalarSubPlan &);

public:
	// ctor/dtor
	CDXLScalarSubPlan(CMemoryPool *mp, IMDId *first_col_type_mdid,
					  CDXLColRefArray *dxl_colref_array,
					  EdxlSubPlanType dxl_subplan_type,
					  CDXLNode *dxlnode_test_expr);

	virtual ~CDXLScalarSubPlan();

	// Operator type
	Edxlopid
	GetDXLOperator() const
	{
		return EdxlopScalarSubPlan;
	}

	// Operator name
	const CWStringConst *GetOpNameStr() const;

	// type of first output column
	IMDId *GetFirstColTypeMdId() const;

	// outer references
	const CDXLColRefArray *
	GetDxlOuterColRefsArray() const
	{
		return m_dxl_colref_array;
	}

	// return subplan type
	EdxlSubPlanType
	GetDxlSubplanType() const
	{
		return m_dxl_subplan_type;
	}

	// return test expression
	CDXLNode *
	GetDxlTestExpr() const
	{
		return m_dxlnode_test_expr;
	}

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarSubPlan *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarSubPlan == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarSubPlan *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

	// return a string representation of Subplan type
	const CWStringConst *GetSubplanTypeStr() const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarSubPlan_H

//EOF
