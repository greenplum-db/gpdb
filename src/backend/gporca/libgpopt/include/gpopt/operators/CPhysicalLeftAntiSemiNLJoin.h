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
public:
	CPhysicalLeftAntiSemiNLJoin(const CPhysicalLeftAntiSemiNLJoin &) = delete;

	// ctor
	explicit CPhysicalLeftAntiSemiNLJoin(CMemoryPool *mp);

	// dtor
	~CPhysicalLeftAntiSemiNLJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftAntiSemiNLJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftAntiSemiNLJoin";
	}

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

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
