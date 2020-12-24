//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CScalarArrayRef.cpp
//
//	@doc:
//		Implementation of scalar arrayref
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CScalarArrayRef.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRef::CScalarArrayRef
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarArrayRef::CScalarArrayRef(CMemoryPool *mp, IMDId *elem_type_mdid,
								 INT type_modifier, IMDId *array_type_mdid,
								 IMDId *return_type_mdid)
	: CScalar(mp),
	  m_pmdidElem(elem_type_mdid),
	  m_type_modifier(type_modifier),
	  m_pmdidArray(array_type_mdid),
	  m_mdid_type(return_type_mdid)
{
	GPOS_ASSERT(elem_type_mdid->IsValid());
	GPOS_ASSERT(array_type_mdid->IsValid());
	GPOS_ASSERT(return_type_mdid->IsValid());
	GPOS_ASSERT(return_type_mdid->Equals(elem_type_mdid) ||
				return_type_mdid->Equals(array_type_mdid));
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRef::~CScalarArrayRef
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarArrayRef::~CScalarArrayRef()
{
	m_pmdidElem->Release();
	m_pmdidArray->Release();
	m_mdid_type->Release();
}


INT
CScalarArrayRef::TypeModifier() const
{
	return m_type_modifier;
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRef::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CScalarArrayRef::HashValue() const
{
	return gpos::CombineHashes(
		CombineHashes(m_pmdidElem->HashValue(), m_pmdidArray->HashValue()),
		m_mdid_type->HashValue());
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarArrayRef::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarArrayRef::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CScalarArrayRef *popArrayRef = CScalarArrayRef::PopConvert(pop);

	return m_mdid_type->Equals(popArrayRef->MdidType()) &&
		   m_pmdidElem->Equals(popArrayRef->PmdidElem()) &&
		   m_pmdidArray->Equals(popArrayRef->PmdidArray());
}

// EOF
