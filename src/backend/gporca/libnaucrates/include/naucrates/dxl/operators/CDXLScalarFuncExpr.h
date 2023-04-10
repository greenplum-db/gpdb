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

	//  true if in the function, variadic arguments have been
	//  combined into an array last argument
	BOOL m_funcvariadic;

public:
	CDXLScalarFuncExpr(const CDXLScalarFuncExpr &) = delete;

	// ctor
	CDXLScalarFuncExpr(CMemoryPool *mp, IMDId *mdid_func,
					   IMDId *mdid_return_type, INT return_type_modifier,
					   BOOL returns_set, BOOL funcvariadic);

	//dtor
	~CDXLScalarFuncExpr() override;

	// ident accessors
	Edxlopid GetDXLOperator() const override;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const override;

	// function id
	IMDId *FuncMdId() const;

	// return type
	IMDId *ReturnTypeMdId() const;

	INT TypeModifier() const;

	// does function return a set
	BOOL ReturnsSet() const;

	// Is the variadic flag set
	BOOL IsFuncVariadic() const;

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *dxlnode) const override;

	// conversion function
	static CDXLScalarFuncExpr *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopScalarFuncExpr == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarFuncExpr *>(dxl_op);
	}

	// does the operator return a boolean result
	BOOL HasBoolResult(CMDAccessor *md_accessor) const override;

#ifdef GPOS_DEBUG
	// checks whether the operator has valid structure, i.e. number and
	// types of child nodes
	void AssertValid(const CDXLNode *dxlnode,
					 BOOL validate_children) const override;
#endif	// GPOS_DEBUG
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarFuncExpr_H

// EOF
