//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarArrayCoerceExpr.cpp
//
//	@doc:
//		Implementation of scalar array coerce expr operator
//
//	@owner:
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarArrayCoerceExpr.h"

#include "gpos/base.h"

using namespace gpopt;
using namespace gpmd;


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::CScalarArrayCoerceExpr
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarArrayCoerceExpr::CScalarArrayCoerceExpr(CMemoryPool *mp,
											   IMDId *result_type_mdid,
											   INT type_modifier,
											   ECoercionForm ecf, INT location)
	: CScalarCoerceBase(mp, result_type_mdid, type_modifier, ecf, location)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::Eopid
//
//	@doc:
//		Return operator identifier
//
//---------------------------------------------------------------------------
CScalar::EOperatorId
CScalarArrayCoerceExpr::Eopid() const
{
	return EopScalarArrayCoerceExpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::SzId
//
//	@doc:
//		Return a string for operator name
//
//---------------------------------------------------------------------------
const CHAR *
CScalarArrayCoerceExpr::SzId() const
{
	return "CScalarArrayCoerceExpr";
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarArrayCoerceExpr::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CScalarArrayCoerceExpr *popCoerce = CScalarArrayCoerceExpr::PopConvert(pop);

	return popCoerce->MdidType()->Equals(MdidType()) &&
		   popCoerce->TypeModifier() == TypeModifier() &&
		   popCoerce->Ecf() == Ecf() && popCoerce->Location() == Location();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::FInputOrderSensitive
//
//	@doc:
//		Sensitivity to order of inputs
//
//---------------------------------------------------------------------------
BOOL
CScalarArrayCoerceExpr::FInputOrderSensitive() const
{
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayCoerceExpr::PopConvert
//
//	@doc:
//		Conversion function
//
//---------------------------------------------------------------------------
CScalarArrayCoerceExpr *
CScalarArrayCoerceExpr::PopConvert(COperator *pop)
{
	GPOS_ASSERT(nullptr != pop);
	GPOS_ASSERT(EopScalarArrayCoerceExpr == pop->Eopid());

	return dynamic_cast<CScalarArrayCoerceExpr *>(pop);
}


// EOF
