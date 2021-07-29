//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLeftSemiJoin.h
//
//	@doc:
//		Left semi join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalLeftSemiJoin_H
#define GPOS_CLogicalLeftSemiJoin_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalLeftSemiJoin
//
//	@doc:
//		Left semi join operator
//
//---------------------------------------------------------------------------
class CLogicalLeftSemiJoin : public CLogicalJoin
{
private:
public:
	CLogicalLeftSemiJoin(const CLogicalLeftSemiJoin &) = delete;

	// ctor
	explicit CLogicalLeftSemiJoin(
		CMemoryPool *mp, CXform::EXformId origin_xform = CXform::ExfSentinel);

	// dtor
	~CLogicalLeftSemiJoin() override = default;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalLeftSemiJoin;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalLeftSemiJoin";
	}

	// return true if we can pull projections up past this operator from its given child
	BOOL
	FCanPullProjectionsUp(ULONG child_index) const override
	{
		return (0 == child_index);
	}

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *mp,
									CExpressionHandle &hdl) override;

	// derive not nullable output columns
	CColRefSet *
	DeriveNotNullColumns(CMemoryPool *,	 // mp
						 CExpressionHandle &exprhdl) const override
	{
		return PcrsDeriveNotNullPassThruOuter(exprhdl);
	}

	// dervive keys
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *
	DerivePropertyConstraint(CMemoryPool *,	 //mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
	}

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// promise level for stat derivation
	EStatPromise
	Esp(CExpressionHandle &	 // exprhdl
	) const override
	{
		// semi join can be converted to inner join, which is used for stat derivation
		return EspMedium;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalLeftSemiJoin *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalLeftSemiJoin == pop->Eopid());

		return dynamic_cast<CLogicalLeftSemiJoin *>(pop);
	}

	// derive statistics
	static IStatistics *PstatsDerive(CMemoryPool *mp,
									 CStatsPredJoinArray *join_preds_stats,
									 IStatistics *outer_stats,
									 IStatistics *inner_side_stats);

};	// class CLogicalLeftSemiJoin

}  // namespace gpopt


#endif	// !GPOS_CLogicalLeftSemiJoin_H

// EOF
