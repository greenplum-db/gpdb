//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalLeftAntiSemiHashJoin.h
//
//	@doc:
//		Left anti semi hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftAntiSemiHashJoin_H
#define GPOPT_CPhysicalLeftAntiSemiHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftAntiSemiHashJoin
//
//	@doc:
//		Left anti semi hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftAntiSemiHashJoin : public CPhysicalHashJoin
{
private:
	// private copy ctor
	CPhysicalLeftAntiSemiHashJoin(const CPhysicalLeftAntiSemiHashJoin &);

public:
	// ctor
	CPhysicalLeftAntiSemiHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys,
		IMdIdArray *hash_opfamilies = NULL, BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	virtual ~CPhysicalLeftAntiSemiHashJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalLeftAntiSemiHashJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalLeftAntiSemiHashJoin";
	}

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	// compute required partition propagation spec
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// conversion function
	static CPhysicalLeftAntiSemiHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftAntiSemiHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftAntiSemiHashJoin *>(pop);
	}


};	// class CPhysicalLeftAntiSemiHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftAntiSemiHashJoin_H

// EOF
