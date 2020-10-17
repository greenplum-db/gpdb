//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
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

	// private copy ctor
	CPhysicalStreamAggDeduplicate(const CPhysicalStreamAggDeduplicate &);

public:
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
	virtual ~CPhysicalStreamAggDeduplicate();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalStreamAggDeduplicate;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
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
	virtual CColRefSet *
	PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				 CColRefSet *pcrsRequired, ULONG child_index,
				 CDrvdPropArray *,	//pdrgpdpCtxt,
				 ULONG				//ulOptReq
	)
	{
		return PcrsRequiredAgg(mp, exprhdl, pcrsRequired, child_index,
							   m_pdrgpcrKeys);
	}

	// compute required sort columns of the n-th child
	virtual COrderSpec *
	PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				COrderSpec *posRequired, ULONG child_index,
				CDrvdPropArray *,  //pdrgpdpCtxt,
				ULONG			   //ulOptReq
	) const
	{
		return PosRequiredStreamAgg(mp, exprhdl, posRequired, child_index,
									m_pdrgpcrKeys);
	}

	// compute required distribution of the n-th child
	virtual CDistributionSpec *
	PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired, ULONG child_index,
				CDrvdPropArray *,  //pdrgpdpCtxt,
				ULONG ulOptReq) const
	{
		return PdsRequiredAgg(mp, exprhdl, pdsRequired, child_index, ulOptReq,
							  m_pdrgpcrKeys, m_pdrgpcrKeys);
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// debug print
	virtual IOstream &OsPrint(IOstream &os) const;

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
