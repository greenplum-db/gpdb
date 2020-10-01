//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftSemiNLJoin.h
//
//	@doc:
//		Left semi nested-loops join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftSemiNLJoin_H
#define GPOPT_CPhysicalLeftSemiNLJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftSemiNLJoin
//
//	@doc:
//		Left semi nested-loops join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftSemiNLJoin : public CPhysicalNLJoin
{
private:
	// private copy ctor
	CPhysicalLeftSemiNLJoin(const CPhysicalLeftSemiNLJoin &);

public:
	// ctor
	explicit CPhysicalLeftSemiNLJoin(CMemoryPool *mp);

	// dtor
	virtual ~CPhysicalLeftSemiNLJoin();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalLeftSemiNLJoin;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalLeftSemiNLJoin";
	}

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	// conversion function
	static CPhysicalLeftSemiNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftSemiNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftSemiNLJoin *>(pop);
	}


};	// class CPhysicalLeftSemiNLJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftSemiNLJoin_H

// EOF
