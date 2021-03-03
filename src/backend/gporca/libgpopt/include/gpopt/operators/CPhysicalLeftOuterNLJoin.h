//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPhysicalLeftOuterNLJoin.h
//
//	@doc:
//		Left outer nested-loops join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalLeftOuterNLJoin_H
#define GPOPT_CPhysicalLeftOuterNLJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalNLJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalLeftOuterNLJoin
//
//	@doc:
//		Left outer nested-loops join operator
//
//---------------------------------------------------------------------------
class CPhysicalLeftOuterNLJoin : public CPhysicalNLJoin
{
private:
public:
	CPhysicalLeftOuterNLJoin(const CPhysicalLeftOuterNLJoin &) = delete;

	// ctor
	explicit CPhysicalLeftOuterNLJoin(CMemoryPool *mp);

	// dtor
	~CPhysicalLeftOuterNLJoin() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalLeftOuterNLJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalLeftOuterNLJoin";
	}


	// conversion function
	static CPhysicalLeftOuterNLJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(EopPhysicalLeftOuterNLJoin == pop->Eopid());

		return dynamic_cast<CPhysicalLeftOuterNLJoin *>(pop);
	}


};	// class CPhysicalLeftOuterNLJoin

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalLeftOuterNLJoin_H

// EOF
