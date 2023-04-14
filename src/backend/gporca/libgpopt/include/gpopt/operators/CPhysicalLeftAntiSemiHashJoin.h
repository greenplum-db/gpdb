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
public:
	CPhysicalLeftAntiSemiHashJoin(const CPhysicalLeftAntiSemiHashJoin &) =
		delete;

	// ctor
	CPhysicalLeftAntiSemiHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
		BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CPhysicalLeftAntiSemiHashJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftAntiSemiHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftAntiSemiHashJoin";
	}

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

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
