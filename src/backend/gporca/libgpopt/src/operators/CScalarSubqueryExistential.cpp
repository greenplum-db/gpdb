//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryExistential.cpp
//
//	@doc:
//		Implementation of existential subquery operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CScalarSubqueryExistential.h"

#include "naucrates/md/IMDTypeBool.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::CScalarSubqueryExistential
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryExistential::CScalarSubqueryExistential
	(
	CMemoryPool *mp
	)
	:
	CScalar(mp)
{}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::~CScalarSubqueryExistential
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarSubqueryExistential::~CScalarSubqueryExistential()
{}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::MdidType
//
//	@doc:
//		Type of scalar's value
//
//---------------------------------------------------------------------------
IMDId *
CScalarSubqueryExistential::MdidType() const
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	return md_accessor->PtMDType<IMDTypeBool>()->MDId();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSubqueryExistential::Matches
	(
	COperator *pop
	)
	const
{
	GPOS_ASSERT(NULL != pop);

	return pop->Eopid() == Eopid();
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryExistential::DerivePartitionInfo
//
//	@doc:
//		Derive partition consumers
//
//---------------------------------------------------------------------------
CPartInfo *
CScalarSubqueryExistential::PpartinfoDerive
	(
	CMemoryPool *, // mp, 
	CExpressionHandle &exprhdl
	)
	const
{
	CPartInfo *ppartinfoChild = exprhdl.DerivePartitionInfo(0);
	GPOS_ASSERT(NULL != ppartinfoChild);
	ppartinfoChild->AddRef();
	return ppartinfoChild;
}

// EOF

