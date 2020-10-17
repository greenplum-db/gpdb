//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLScalarOpExpr.h
//
//	@doc:
//		Class for representing DXL scalar OpExpr such as A.a1 + (B.b1 * C.c1)
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarOpExpr_H
#define GPDXL_CDXLScalarOpExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalar.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarOpExpr
//
//	@doc:
//		Class for representing DXL scalar OpExpr
//
//---------------------------------------------------------------------------
class CDXLScalarOpExpr : public CDXLScalar
{
private:
	// operator number in the catalog
	IMDId *m_mdid;

	// return type (or invalid if type can be infered from the metadata)
	IMDId *m_return_type_mdid;

	// operator name
	const CWStringConst *m_str_opname;

	// private copy ctor
	CDXLScalarOpExpr(const CDXLScalarOpExpr &);

public:
	// ctor/dtor
	CDXLScalarOpExpr(CMemoryPool *mp, IMDId *mdid_op, IMDId *return_type_mdid,
					 const CWStringConst *str_opname);

	virtual ~CDXLScalarOpExpr();

	// ident accessors
	Edxlopid GetDXLOperator() const;

	// name of the DXL operator
	const CWStringConst *GetOpNameStr() const;

	// name of the operator
	const CWStringConst *GetScalarOpNameStr() const;

	// operator id
	IMDId *MDId() const;

	// operator return type
	IMDId *GetReturnTypeMdId() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *, const CDXLNode *) const;

	// conversion function
	static CDXLScalarOpExpr *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarOpExpr == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarOpExpr *>(dxl_op);
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

#endif	// !GPDXL_CDXLScalarOpExpr_H

// EOF
