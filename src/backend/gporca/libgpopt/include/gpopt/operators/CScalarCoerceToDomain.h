//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarCoerceToDomain.h
//
//	@doc:
//		Scalar CoerceToDomain operator,
//		the operator captures coercing a value to a domain type,
//
//		at runtime, the precise set of constraints to be checked against
//		value are determined,
//		if the value passes, it is returned as the result, otherwise an error
//		is raised.

//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarCoerceToDomain_H
#define GPOPT_CScalarCoerceToDomain_H

#include "gpos/base.h"
#include "gpopt/operators/CScalarCoerceBase.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CScalarCoerceToDomain
//
//	@doc:
//		Scalar CoerceToDomain operator
//
//---------------------------------------------------------------------------
class CScalarCoerceToDomain : public CScalarCoerceBase
{
private:
	// does operator return NULL on NULL input?
	BOOL m_returns_null_on_null_input;

public:
	CScalarCoerceToDomain(const CScalarCoerceToDomain &) = delete;

	// ctor
	CScalarCoerceToDomain(CMemoryPool *mp, IMDId *mdid_type, INT type_modifier,
						  ECoercionForm dxl_coerce_format, INT location);

	// dtor
	~CScalarCoerceToDomain() override = default;

	EOperatorId
	Eopid() const override
	{
		return EopScalarCoerceToDomain;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CScalarCoerceToDomain";
	}

	// match function
	BOOL Matches(COperator *) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	// boolean expression evaluation
	EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const override;

	// conversion function
	static CScalarCoerceToDomain *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopScalarCoerceToDomain == pop->Eopid());

		return dynamic_cast<CScalarCoerceToDomain *>(pop);
	}

};	// class CScalarCoerceToDomain

}  // namespace gpopt


#endif	// !GPOPT_CScalarCoerceToDomain_H

// EOF
