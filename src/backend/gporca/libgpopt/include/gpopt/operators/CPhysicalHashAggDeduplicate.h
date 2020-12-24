//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalHashAggDeduplicate.h
//
//	@doc:
//		Hash Aggregate operator for deduplicating join outputs
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalHashAggDeduplicate_H
#define GPOS_CPhysicalHashAggDeduplicate_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalHashAgg.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalHashAggDeduplicate
//
//	@doc:
//		Hash-based aggregate operator for deduplicating join outputs
//
//---------------------------------------------------------------------------
class CPhysicalHashAggDeduplicate : public CPhysicalHashAgg
{
private:
	// array of keys from the join's child
	CColRefArray *m_pdrgpcrKeys;

public:
	CPhysicalHashAggDeduplicate(const CPhysicalHashAggDeduplicate &) = delete;

	// ctor
	CPhysicalHashAggDeduplicate(CMemoryPool *mp, CColRefArray *colref_array,
								CColRefArray *pdrgpcrMinimal,
								COperator::EGbAggType egbaggtype,
								CColRefArray *pdrgpcrKeys,
								BOOL fGeneratesDuplicates, BOOL fMultiStage,
								BOOL isAggFromSplitDQA,
								CLogicalGbAgg::EAggStage aggStage,
								BOOL should_enforce_distribution);

	// dtor
	~CPhysicalHashAggDeduplicate() override;


	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalHashAggDeduplicate;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalHashAggDeduplicate";
	}

	// array of keys from the join's child
	CColRefArray *
	PdrgpcrKeys() const
	{
		return m_pdrgpcrKeys;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *
	PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				 CColRefSet *pcrsRequired, ULONG child_index,
				 CDrvdPropArray *,	//pdrgpdpCtxt,
				 ULONG				//ulOptReq
				 ) override
	{
		return PcrsRequiredAgg(mp, exprhdl, pcrsRequired, child_index,
							   m_pdrgpcrKeys);
	}

	// compute required distribution of the n-th child
	CDistributionSpec *
	PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired, ULONG child_index,
				CDrvdPropArray *,  //pdrgpdpCtxt,
				ULONG ulOptReq) const override
	{
		return PdsRequiredAgg(mp, exprhdl, pdsRequired, child_index, ulOptReq,
							  m_pdrgpcrKeys, m_pdrgpcrKeys);
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CPhysicalHashAggDeduplicate *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalHashAggDeduplicate == pop->Eopid());

		return reinterpret_cast<CPhysicalHashAggDeduplicate *>(pop);
	}

};	// class CPhysicalHashAggDeduplicate

}  // namespace gpopt


#endif	// !GPOS_CPhysicalHashAggDeduplicate_H

// EOF
