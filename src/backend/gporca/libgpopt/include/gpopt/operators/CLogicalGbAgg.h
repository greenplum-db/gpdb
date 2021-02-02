//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalGbAgg.h
//
//	@doc:
//		Group Aggregate operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalGbAgg_H
#define GPOS_CLogicalGbAgg_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnary.h"

namespace gpopt
{
// fwd declaration
class CColRefSet;

class CLogicalGbAgg;

//---------------------------------------------------------------------------
//	@class:
//		CLogicalGbAgg
//
//	@doc:
//		aggregate operator
//
//---------------------------------------------------------------------------
class CLogicalGbAgg : public CLogicalUnary
{
protected:
	// does local / intermediate / global aggregate generate duplicate values for the same group
	BOOL m_fGeneratesDuplicates;

	// array of columns used in distinct qualified aggregates (DQA)
	// used only in the case of intermediate aggregates
	CColRefArray *m_pdrgpcrArgDQA;

	// compute required stats columns for a GbAgg
	CColRefSet *PcrsStatGbAgg(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CColRefSet *pcrsInput, ULONG child_index,
							  CColRefArray *pdrgpcrGrp) const;

public:
	CLogicalGbAgg(const CLogicalGbAgg &) = delete;

	// the below enum specifically covers only 2 & 3 stage
	// scalar dqa, as they are used to find the one having
	// better cost context, rest of the aggs falls in others
	// category.
	enum EAggStage
	{
		EasTwoStageScalarDQA,
		EasThreeStageScalarDQA,
		EasOthers,

		EasSentinel
	};

	// ctor
	explicit CLogicalGbAgg(CMemoryPool *mp);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  COperator::EGbAggType egbaggtype, EAggStage aggStage);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  COperator::EGbAggType egbaggtype, BOOL fGeneratesDuplicates,
				  CColRefArray *pdrgpcrArgDQA, EAggStage aggStage);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  COperator::EGbAggType egbaggtype);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  COperator::EGbAggType egbaggtype, BOOL fGeneratesDuplicates,
				  CColRefArray *pdrgpcrArgDQA);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  CColRefArray *pdrgpcrMinimal,
				  COperator::EGbAggType egbaggtype);

	// ctor
	CLogicalGbAgg(CMemoryPool *mp, CColRefArray *colref_array,
				  CColRefArray *pdrgpcrMinimal,
				  COperator::EGbAggType egbaggtype, BOOL fGeneratesDuplicates,
				  CColRefArray *pdrgpcrArgDQA);

	// is this part of Two Stage Scalar DQA
	BOOL IsTwoStageScalarDQA() const;

	// is this part of Three Stage Scalar DQA
	BOOL IsThreeStageScalarDQA() const;

	// return the m_aggStage
	EAggStage
	AggStage() const
	{
		return m_aggStage;
	}

	// dtor
	~CLogicalGbAgg() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopLogicalGbAgg;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CLogicalGbAgg";
	}

	// does this aggregate generate duplicate values for the same group
	virtual BOOL
	FGeneratesDuplicates() const
	{
		return m_fGeneratesDuplicates;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hash function
	ULONG HashValue() const override;

	// grouping columns accessor
	CColRefArray *
	Pdrgpcr() const
	{
		return m_pdrgpcr;
	}

	// array of columns used in distinct qualified aggregates (DQA)
	CColRefArray *
	PdrgpcrArgDQA() const
	{
		return m_pdrgpcrArgDQA;
	}

	// aggregate type
	COperator::EGbAggType
	Egbaggtype() const
	{
		return m_egbaggtype;
	}

	// is a global aggregate?
	BOOL
	FGlobal() const
	{
		return (COperator::EgbaggtypeGlobal == m_egbaggtype);
	}

	// minimal grouping columns accessor
	CColRefArray *
	PdrgpcrMinimal() const
	{
		return m_pdrgpcrMinimal;
	}

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	//-------------------------------------------------------------------------------------
	// Derived Relational Properties
	//-------------------------------------------------------------------------------------

	// derive output columns
	CColRefSet *DeriveOutputColumns(CMemoryPool *,
									CExpressionHandle &) override;

	// derive outer references
	CColRefSet *DeriveOuterReferences(CMemoryPool *mp,
									  CExpressionHandle &exprhdl) override;

	// derive not null columns
	CColRefSet *DeriveNotNullColumns(CMemoryPool *mp,
									 CExpressionHandle &exprhdl) const override;

	// derive key collections
	CKeyCollection *DeriveKeyCollection(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// derive max card
	CMaxCard DeriveMaxCard(CMemoryPool *mp,
						   CExpressionHandle &exprhdl) const override;

	// derive constraint property
	CPropConstraint *DerivePropertyConstraint(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const override;

	// compute required stats columns of the n-th child
	//-------------------------------------------------------------------------------------
	// Required Relational Properties
	//-------------------------------------------------------------------------------------

	// compute required stat columns of the n-th child
	CColRefSet *PcrsStat(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CColRefSet *pcrsInput,
						 ULONG child_index) const override;

	//-------------------------------------------------------------------------------------
	// Transformations
	//-------------------------------------------------------------------------------------

	// candidate set of xforms
	CXformSet *PxfsCandidates(CMemoryPool *mp) const override;

	// derive statistics
	IStatistics *PstatsDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  IStatisticsArray *stats_ctxt) const override;

	// stat promise
	EStatPromise
	Esp(CExpressionHandle &) const override
	{
		return CLogical::EspHigh;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CLogicalGbAgg *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopLogicalGbAgg == pop->Eopid() ||
					EopLogicalGbAggDeduplicate == pop->Eopid());

		return dynamic_cast<CLogicalGbAgg *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

	// derive statistics
	static IStatistics *PstatsDerive(CMemoryPool *mp, IStatistics *child_stats,
									 CColRefArray *pdrgpcrGroupingCols,
									 ULongPtrArray *pdrgpulComputedCols,
									 CBitSet *keys);

	// print group by aggregate type
	static IOstream &OsPrintGbAggType(IOstream &os,
									  COperator::EGbAggType egbaggtype);

private:
	// array of grouping columns
	CColRefArray *m_pdrgpcr;

	// minimal grouping columns based on FD's
	CColRefArray *m_pdrgpcrMinimal;

	// local / intermediate / global aggregate
	COperator::EGbAggType m_egbaggtype;

	// which type of multi-stage agg it is
	EAggStage m_aggStage;

};	// class CLogicalGbAgg

}  // namespace gpopt


#endif	// !GPOS_CLogicalGbAgg_H

// EOF
