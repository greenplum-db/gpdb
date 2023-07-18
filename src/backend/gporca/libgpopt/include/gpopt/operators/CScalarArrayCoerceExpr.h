//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarArrayCoerceExpr.h
//
//	@doc:
//		Scalar Array Coerce Expr operator,
//		the operator will apply type casting for each element in this array
//		using the given element coercion function.
//
//	@owner:
//
//	@test:
//
//
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarArrayCoerceExpr_H
#define GPOPT_CScalarArrayCoerceExpr_H

#include "gpos/base.h"

#include "gpopt/operators/CScalarCoerceBase.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarArrayCoerceExpr
//
//	@doc:
//		Scalar Array Coerce Expr operator
//
//---------------------------------------------------------------------------
class CScalarArrayCoerceExpr : public CScalarCoerceBase
{
private:
public:
	CScalarArrayCoerceExpr(const CScalarArrayCoerceExpr &) = delete;

	// ctor
	CScalarArrayCoerceExpr(CMemoryPool *mp, IMDId *result_type_mdid,
						   INT type_modifier, ECoercionForm dxl_coerce_format,
						   INT location);

	// dtor
	~CScalarArrayCoerceExpr() override = default;

	EOperatorId Eopid() const override;

	// return a string for operator name
	const CHAR *SzId() const override;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// conversion function
	static CScalarArrayCoerceExpr *PopConvert(COperator *pop);

};	// class CScalarArrayCoerceExpr

}  // namespace gpopt


#endif	// !GPOPT_CScalarArrayCoerceExpr_H

// EOF
