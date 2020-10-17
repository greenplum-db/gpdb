//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
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

	// private copy ctor
	CScalarCoerceToDomain(const CScalarCoerceToDomain &);

public:
	// ctor
	CScalarCoerceToDomain(CMemoryPool *mp, IMDId *mdid_type, INT type_modifier,
						  ECoercionForm dxl_coerce_format, INT location);

	// dtor
	virtual ~CScalarCoerceToDomain()
	{
	}

	virtual EOperatorId
	Eopid() const
	{
		return EopScalarCoerceToDomain;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CScalarCoerceToDomain";
	}

	// match function
	virtual BOOL Matches(COperator *) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return false;
	}

	// boolean expression evaluation
	virtual EBoolEvalResult Eber(ULongPtrArray *pdrgpulChildren) const;

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
