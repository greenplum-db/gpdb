//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarFuncExpr.h
//
//	@doc:
//		Class for representing DXL scalar FuncExpr
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarFuncExpr_H
#define GPDXL_CDXLScalarFuncExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarFuncExpr
//
//	@doc:
//		Class for representing DXL scalar FuncExpr
//
//---------------------------------------------------------------------------
class CDXLScalarFuncExpr : public CDXLScalar
{
private:
	// catalog id of the function
	IMDId *m_func_mdid;

	// return type
	IMDId *m_return_type_mdid;

	const INT m_return_type_modifier;

	// does the func return a set
	BOOL m_returns_set;

	// private copy ctor
	CDXLScalarFuncExpr(const CDXLScalarFuncExpr &);

public:
	// ctor
	CDXLScalarFuncExpr(CMemoryPool *mp, IMDId *mdid_func,
					   IMDId *mdid_return_type, INT return_type_modifier,
					   BOOL returns_set);

	//dtor
	virtual ~CDXLScalarFuncExpr();

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const;

	// function id
	IMDId *FuncMdId() const;

	// return type
	IMDId *ReturnTypeMdId() const;

	INT TypeModifier() const;

	// does function return a set
	BOOL ReturnsSet() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarFuncExpr *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarFuncExpr == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarFuncExpr *>(dxl_op);
	}

	// does the operator return a boolean result
	virtual BOOL HasBoolResult(CMDAccessor *md_accessor) const;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarFuncExpr_H

// EOF
