//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CScalarSubqueryQuantified.cpp
//
//	@doc:
//		Implementation of quantified subquery operator
//---------------------------------------------------------------------------

#include "gpopt/operators/CScalarSubqueryQuantified.h"

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDrvdPropScalar.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/xforms/CSubqueryHandler.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDTypeBool.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::CScalarSubqueryQuantified
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryQuantified::CScalarSubqueryQuantified(CMemoryPool *mp,
													 CColRefSet *colrefs)
	: CScalar(mp), m_pcrs(colrefs)
{
	GPOS_ASSERT(nullptr != m_pcrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::~CScalarSubqueryQuantified
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CScalarSubqueryQuantified::~CScalarSubqueryQuantified()
{
	m_pcrs->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::MdidType
//
//	@doc:
//		Type of scalar's value
//
//---------------------------------------------------------------------------
IMDId *
CScalarSubqueryQuantified::MdidType() const
{
	GPOS_ASSERT(
		!"Invalid function call: CScalarSubqueryQuantified::MdidType()");
	GPOS_RAISE(
		CException::ExmaInvalid, CException::ExmiInvalid,
		GPOS_WSZ_LIT(
			"MdidType() should not be called for CScalarSubqueryQuantified"));

	return nullptr;
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CScalarSubqueryQuantified::HashValue() const
{
	return gpos::CombineHashes(COperator::HashValue(), m_pcrs->HashValue());
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarSubqueryQuantified::Matches(COperator *pop) const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	// match if contents are identical
	CScalarSubqueryQuantified *popSsq =
		CScalarSubqueryQuantified::PopConvert(pop);
	return popSsq->Pcrs()->Equals(m_pcrs);
	;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::PcrsUsed
//
//	@doc:
//		Locally used columns
//
//---------------------------------------------------------------------------
CColRefSet *
CScalarSubqueryQuantified::PcrsUsed(CMemoryPool *mp, CExpressionHandle &exprhdl)
{
	// inner / relational child colrefs
	CColRefSet *pcrsChildOutput =
		exprhdl.DeriveOutputColumns(0 /* child_index */);

	CColRefSet *pcrsOuterColrefs = GPOS_NEW(mp) CColRefSet(mp, *m_pcrs);
	// used columns is an empty set unless subquery column is an outer reference
	pcrsOuterColrefs->Exclude(pcrsChildOutput);

	return pcrsOuterColrefs;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::DerivePartitionInfo
//
//	@doc:
//		Derive partition consumers
//
//---------------------------------------------------------------------------
CPartInfo *
CScalarSubqueryQuantified::PpartinfoDerive(CMemoryPool *,  // mp,
										   CExpressionHandle &exprhdl) const
{
	CPartInfo *ppartinfoChild = exprhdl.DerivePartitionInfo(0);
	GPOS_ASSERT(nullptr != ppartinfoChild);
	ppartinfoChild->AddRef();
	return ppartinfoChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryQuantified::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CScalarSubqueryQuantified::OsPrint(IOstream &os) const
{
	os << SzId();
	os << "[";
	m_pcrs->OsPrint(os);
	os << "]";

	return os;
}


// EOF
