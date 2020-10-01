//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal, Inc.
//
//	@filename:
//		CScalarAssertConstraint.cpp
//
//	@doc:
//		Implementation of scalar assert constraint
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "naucrates/md/IMDTypeBool.h"
#include "gpopt/operators/CScalarAssertConstraint.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraint::CScalarAssertConstraint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarAssertConstraint::CScalarAssertConstraint(CMemoryPool *mp,
												 CWStringBase *pstrErrorMsg)
	: CScalar(mp), m_pstrErrorMsg(pstrErrorMsg)
{
	GPOS_ASSERT(NULL != pstrErrorMsg);
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraint::CScalarAssertConstraint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarAssertConstraint::~CScalarAssertConstraint()
{
	GPOS_DELETE(m_pstrErrorMsg);
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraint::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarAssertConstraint::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	return m_pstrErrorMsg->Equals(
		CScalarAssertConstraint::PopConvert(pop)->PstrErrorMsg());
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraint::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CScalarAssertConstraint::OsPrint(IOstream &os) const
{
	os << SzId() << " (ErrorMsg: ";
	os << PstrErrorMsg()->GetBuffer();
	os << ")";

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarAssertConstraint::MdidType
//
//	@doc:
//		Type of expression's result
//
//---------------------------------------------------------------------------
IMDId *
CScalarAssertConstraint::MdidType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	return md_accessor->PtMDType<IMDTypeBool>()->MDId();
}


// EOF
