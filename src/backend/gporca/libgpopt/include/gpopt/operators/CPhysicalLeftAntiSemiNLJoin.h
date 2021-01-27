//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiNLJoin.h
//
//	@doc:
//		Left anti semi nested-loops join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftAntiSemiNLJoin_H
#define GPOPT_CPhysicalLeftAntiSemiNLJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftAntiSemiNLJoin
//
//	@doc:
//		Left anti semi nested-loops join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftAntiSemiNLJoin : public CPhysicalNLJoin
{
private:
	// private copy ctor
	CPhysicalLeftAntiSemiNLJoin(const CPhysicalLeftAntiSemiNLJoin &);

public:
	// ctor
	explicit CPhysicalLeftAntiSemiNLJoin(CMemoryPool *mp);

	// dtor
	virtual ~CPhysicalLeftAntiSemiNLJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalLeftAntiSemiNLJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalLeftAntiSemiNLJoin";
	}

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	// compute required partition propagation of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// conversion function
	static CPhysicalLeftAntiSemiNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftAntiSemiNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftAntiSemiNLJoin *>(pop);
	}


};	// class CPhysicalLeftAntiSemiNLJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftAntiSemiNLJoin_H

// EOF
