//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalStreamAggDeduplicate.h
//
//	@doc:
//		Sort-based stream Aggregate operator for deduplicating join outputs
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalStreamAggDeduplicate_H
#define GPOS_CPhysicalStreamAggDeduplicate_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalStreamAgg.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalStreamAggDeduplicate
//
//	@doc:
//		Sort-based aggregate operator for deduplicating join outputs
//
//---------------------------------------------------------------------------
class CPhysicalStreamAggDeduplicate : public CPhysicalStreamAgg
{
private:
	// array of keys from the join's child
	CColRefArray *m_pdrgpcrKeys;

public:
	CPhysicalStreamAggDeduplicate(const CPhysicalStreamAggDeduplicate &) =
		delete;

	// ctor
	CPhysicalStreamAggDeduplicate(CMemoryPool *mp, CColRefArray *colref_array,
								  CColRefArray *pdrgpcrMinimal,
								  COperator::EGbAggType egbaggtype,
								  CColRefArray *pdrgpcrKeys,
								  BOOL fGeneratesDuplicates, BOOL fMultiStage,
								  BOOL isAggFromSplitDQA,
								  CLogicalGbAgg::EAggStage aggStage,
								  BOOL should_enforce_distribution);

	// dtor
	~CPhysicalStreamAggDeduplicate() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalStreamAggDeduplicate;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalStreamAggDeduplicate";
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

	// compute required sort columns of the n-th child
	COrderSpec *
	PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				COrderSpec *posRequired, ULONG child_index,
				CDrvdPropArray *,  //pdrgpdpCtxt,
				ULONG			   //ulOptReq
	) const override
	{
		return PosRequiredStreamAgg(mp, exprhdl, posRequired, child_index,
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
	static CPhysicalStreamAggDeduplicate *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalStreamAggDeduplicate == pop->Eopid());

		return reinterpret_cast<CPhysicalStreamAggDeduplicate *>(pop);
	}

};	// class CPhysicalStreamAggDeduplicate

}  // namespace gpopt


#endif	// !GPOS_CPhysicalStreamAggDeduplicate_H

// EOF
