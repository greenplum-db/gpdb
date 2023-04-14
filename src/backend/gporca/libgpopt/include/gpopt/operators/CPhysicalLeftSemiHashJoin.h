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
public:
	CPhysicalLeftSemiHashJoin(const CPhysicalLeftSemiHashJoin &) = delete;

	// ctor
	CPhysicalLeftSemiHashJoin(
		CMemoryPool *mp, CExpressionArray *pdrgpexprOuterKeys,
		CExpressionArray *pdrgpexprInnerKeys, IMdIdArray *hash_opfamilies,
		BOOL is_null_aware = true,
		CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CPhysicalLeftSemiHashJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftSemiHashJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftSemiHashJoin";
	}

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

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
