//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
//
//	@filename:
//		CScalarFieldSelect.cpp
//
//	@doc:
//		Implementation of scalar FIELDSELECT
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarFieldSelect.h"

#include "gpos/base.h"

using namespace gpopt;
using namespace gpmd;


//---------------------------------------------------------------------------
//	@function:
//		CScalarFieldSelect::CScalarFieldSelect
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarFieldSelect::CScalarFieldSelect(CMemoryPool *mp, IMDId *field_type,
									   IMDId *field_collation,
									   INT type_modifier, SINT field_number)
	: CScalar(mp),
	  m_field_type(field_type),
	  m_field_collation(field_collation),
	  m_type_modifier(type_modifier),
	  m_field_number(field_number)
{
	GPOS_ASSERT(m_field_type->IsValid());
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarFieldSelect::~CScalarFieldSelect
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarFieldSelect::~CScalarFieldSelect()
{
	m_field_type->Release();
	m_field_collation->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarFieldSelect::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CScalarFieldSelect::HashValue() const
{
	return gpos::CombineHashes(
		COperator::HashValue(),
		CombineHashes(CombineHashes(m_field_type->HashValue(),
									m_field_collation->HashValue()),
					  gpos::HashValue<SINT>(&m_field_number)));
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarFieldSelect::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarFieldSelect::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}
	CScalarFieldSelect *popFieldSelect = CScalarFieldSelect::PopConvert(pop);

	// match attribute field number and type of the field
	return popFieldSelect->MdidType()->Equals(MdidType()) &&
		   popFieldSelect->FieldCollation()->Equals(FieldCollation()) &&
		   popFieldSelect->FieldNumber() == FieldNumber();
}


// EOF
