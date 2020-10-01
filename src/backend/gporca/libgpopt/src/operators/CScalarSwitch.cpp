//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSwitch.cpp
//
//	@doc:
//		Implementation of scalar switch operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CScalarSwitch.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/mdcache/CMDAccessorUtils.h"


using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitch::CScalarSwitch
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSwitch::CScalarSwitch(CMemoryPool *mp, IMDId *mdid_type)
	: CScalar(mp), m_mdid_type(mdid_type), m_fBoolReturnType(false)
{
	GPOS_ASSERT(mdid_type->IsValid());

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	m_fBoolReturnType = CMDAccessorUtils::FBoolType(md_accessor, m_mdid_type);
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitch::~CScalarSwitch
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarSwitch::~CScalarSwitch()
{
	m_mdid_type->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitch::HashValue
//
//	@doc:
//		Operator specific hash function; combined hash of operator id and
//		return type id
//
//---------------------------------------------------------------------------
ULONG
CScalarSwitch::HashValue() const
{
	return gpos::CombineHashes(COperator::HashValue(),
							   m_mdid_type->HashValue());
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSwitch::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSwitch::Matches(COperator *pop) const
{
	if (pop->Eopid() == Eopid())
	{
		CScalarSwitch *popScSwitch = CScalarSwitch::PopConvert(pop);

		// match if return types are identical
		return popScSwitch->MdidType()->Equals(m_mdid_type);
	}

	return false;
}


// EOF
