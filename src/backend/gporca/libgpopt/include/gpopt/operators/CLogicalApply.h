//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2011 EMC Corp.
//
//	@filename:
//		CLogicalApply.h
//
//	@doc:
//		Logical Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalApply_H
#define GPOPT_CLogicalApply_H

#include "gpos/base.h"

#include "gpopt/operators/CLogical.h"
#include "naucrates/statistics/CStatistics.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CLogicalApply
//
//	@doc:
//		Logical Apply operator; parent of different Apply operators used
//		in subquery transformations
//
//---------------------------------------------------------------------------
class CLogicalApply : public CLogical
{
private:
protected:
	// columns used from Apply's inner child
	CColRefArray *m_pdrgpcrInner;

	// origin subquery id
	EOperatorId m_eopidOriginSubq;

	// ctor
	explicit CLogicalApply(CMemoryPool *mp);

	// ctor
	CLogicalApply(CMemoryPool *mp, CColRefArray *pdrgpcrInner,
				  EOperatorId eopidOriginSubq);

	// dtor
	~CLogicalApply() override;

public:
	CLogicalApply(const CLogicalApply &) = delete;

	// match function
	BOOL Matches(COperator *pop) const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
	}

	// inner column references accessor
	CColRefArray *
	PdrgPcrInner() const
	{
		return m_pdrgpcrInner;
	}

	// return a copy of the operator with remapped columns
	COperator *
	PopCopyWithRemappedColumns(CMemoryPool *,		//mp,
							   UlongToColRefMap *,	//colref_mapping,
							   BOOL					//must_exist
							   ) override
	{
		return PopCopyDefault();
	}

	// derive partition consumer info
	CPartInfo *
	DerivePartitionInfo(CMemoryPool *mp,
						CExpressionHandle &exprhdl) const override
	{
		return PpartinfoDeriveCombine(mp, exprhdl);
	}

	// derive keys
	CKeyCollection *
	DeriveKeyCollection(CMemoryPool *mp,
						CExpressionHandle &exprhdl) const override
	{
		return PkcCombineKeys(mp, exprhdl);
	}

	// derive function properties
	CFunctionProp *
	DeriveFunctionProperties(CMemoryPool *mp,
							 CExpressionHandle &exprhdl) const override
	{
		return PfpDeriveFromScalar(mp, exprhdl);
	}

	//-------------------------------------------------------------------------------------
	// Derived Stats
	//-------------------------------------------------------------------------------------

	// derive statistics
	IStatistics *
	PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
				 IStatisticsArray *	 // stats_ctxt
	) const override
	{
		// we should use stats from the corresponding Join tree if decorrelation succeeds
		return PstatsDeriveDummy(mp, exprhdl, CStatistics::DefaultRelationRows);
	}

	// promise level for stat derivation
	EStatPromise
	Esp(CExpressionHandle &	 // exprhdl
	) const override
	{
		// whenever we can decorrelate an Apply tree, we should use the corresponding Join tree
		return EspLow;
	}

	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *PcrsStat(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CColRefSet *pcrsInput,
						 ULONG child_index) const override;

	// return true if operator is a correlated apply
	virtual BOOL
	FCorrelated() const
	{
		return false;
	}

	// return true if operator is a left semi apply
	virtual BOOL
	FLeftSemiApply() const
	{
		return false;
	}

	// return true if operator is a left anti semi apply
	virtual BOOL
	FLeftAntiSemiApply() const
	{
		return false;
	}

	// return true if operator can select a subset of input tuples based on some predicate
	BOOL
	FSelectionOp() const override
	{
		return true;
	}

	// origin subquery id
	EOperatorId
	EopidOriginSubq() const
	{
		return m_eopidOriginSubq;
	}

	// print function
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CLogicalApply *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(CUtils::FApply(pop));

		return dynamic_cast<CLogicalApply *>(pop);
	}

};	// class CLogicalApply

}  // namespace gpopt


#endif	// !GPOPT_CLogicalApply_H

// EOF
