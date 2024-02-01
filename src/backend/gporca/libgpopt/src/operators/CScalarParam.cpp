//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 Broadcom
//
//	@filename:
//		CScalarParam.cpp
//
//	@doc:
//		Implementation of scalar parameter
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarParam.h"

#include "gpos/base.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CScalarParam::~CScalarParam
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarParam::~CScalarParam()
{
	m_type->Release();
}


ULONG
CScalarParam::HashValue() const
{
	return Id();
}

BOOL
CScalarParam::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		CScalarParam *popParam = CScalarParam::PopConvert(pop);

		// match if param id is same
		return Id() == popParam->Id();
	}

	return false;
}

BOOL
CScalarParam::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected call of function FInputOrderSensitive");
	return false;
}

// return a copy of the operator with remapped columns
COperator *
CScalarParam::PopCopyWithRemappedColumns(CMemoryPool *,		  //mp,
										 UlongToColRefMap *,  //colref_mapping,
										 BOOL				  //must_exist
)
{
	return PopCopyDefault();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarConst::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CScalarParam::OsPrint(IOstream &os) const
{
	os << SzId() << " (";
	os << Id();
	os << ")";
	return os;
}
// EOF
