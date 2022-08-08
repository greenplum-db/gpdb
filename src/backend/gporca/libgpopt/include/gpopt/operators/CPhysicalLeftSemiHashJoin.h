//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalLeftSemiHashJoin.h
//
//	@doc:
//		Left semi hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftSemiHashJoin_H
#define GPOPT_CPhysicalLeftSemiHashJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalHashJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftSemiHashJoin
//
//	@doc:
//		Left semi hash join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftSemiHashJoin : public CPhysicalHashJoin
{
private:
	// private copy ctor
	CPhysicalLeftSemiHashJoin(const CPhysicalLeftSemiHashJoin &);

public:
	// ctor
	CPhysicalLeftSemiHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys,
		IMdIdArray *hash_opfamilies = NULL, BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	virtual ~CPhysicalLeftSemiHashJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalLeftSemiHashJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalLeftSemiHashJoin";
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
	static CPhysicalLeftSemiHashJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftSemiHashJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftSemiHashJoin *>(pop);
	}


};	// class CPhysicalLeftSemiHashJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftSemiHashJoin_H

// EOF
