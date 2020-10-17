//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Inc.
//
//	@filename:
//		CDXLScalarArrayCoerceExpr.h
//
//	@doc:
//		Class for representing DXL ArrayCoerceExpr operation,
//		the operator will apply type casting for each element in this array
//		using the given element coercion function.
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLScalarArrayCoerceExpr_H
#define GPDXL_CDXLScalarArrayCoerceExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLScalarCoerceBase.h"
#include "naucrates/md/IMDId.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@class:
//		CDXLScalarArrayCoerceExpr
//
//	@doc:
//		Class for representing DXL array coerce operator
//---------------------------------------------------------------------------
class CDXLScalarArrayCoerceExpr : public CDXLScalarCoerceBase
{
private:
	// catalog MDId of element coerce function
	IMDId *m_coerce_func_mdid;

	// conversion semantics flag to pass to func
	BOOL m_explicit;

	// private copy ctor
	CDXLScalarArrayCoerceExpr(const CDXLScalarArrayCoerceExpr &);

public:
	CDXLScalarArrayCoerceExpr(CMemoryPool *mp, IMDId *coerce_func_mdid,
							  IMDId *result_type_mdid, INT type_modifier,
							  BOOL is_explicit, EdxlCoercionForm coerce_format,
							  INT location);

	virtual ~CDXLScalarArrayCoerceExpr()
	{
		m_coerce_func_mdid->Release();
	}

	// ident accessor
	virtual Edxlopid
	GetDXLOperator() const
	{
		return EdxlopScalarArrayCoerceExpr;
	}

	// return metadata id of element coerce function
	IMDId *
	GetCoerceFuncMDid() const
	{
		return m_coerce_func_mdid;
	}

	BOOL
	IsExplicit() const
	{
		return m_explicit;
	}

	// name of the DXL operator name
	virtual const CWStringConst *GetOpNameStr() const;

	// serialize operator in DXL format
	virtual void SerializeToDXL(CXMLSerializer *xml_serializer,
								const CDXLNode *dxlnode) const;

	// conversion function
	static CDXLScalarArrayCoerceExpr *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(NULL != dxl_op);
		GPOS_ASSERT(EdxlopScalarArrayCoerceExpr == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLScalarArrayCoerceExpr *>(dxl_op);
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLScalarArrayCoerceExpr_H

// EOF
