//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalHashAgg.h
//
//	@doc:
//		Hash Aggregate operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalHashAgg_H
#define GPOS_CPhysicalHashAgg_H

#include "gpos/base.h"

#include "gpopt/operators/CPhysicalAgg.h"

namespace gpopt
{
// fwd declaration
class CDistributionSpec;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalHashAgg
//
//	@doc:
//		Hash-based aggregate operator
//
//---------------------------------------------------------------------------
class CPhysicalHashAgg : public CPhysicalAgg
{
private:
	// private copy ctor
	CPhysicalHashAgg(const CPhysicalHashAgg &);

public:
	// ctor
	CPhysicalHashAgg(CMemoryPool *mp, CColRefArray *colref_array,
					 CColRefArray *pdrgpcrMinimal,
					 COperator::EGbAggType egbaggtype,
					 BOOL fGeneratesDuplicates, CColRefArray *pdrgpcrArgDQA,
					 BOOL fMultiStage, BOOL isAggFromSplitDQA,
					 CLogicalGbAgg::EAggStage aggStage,
					 BOOL should_enforce_distribution = true
					 // should_enforce_distribution should be set to false if
					 // 'local' and 'global' splits don't need to have different
					 // distributions. This flag is set to false if the local
					 // aggregate has been created by CXformEagerAgg.
	);

	// dtor
	virtual ~CPhysicalHashAgg();


	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalHashAgg;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalHashAgg";
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required sort columns of the n-th child
	virtual COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posRequired, ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	virtual COrderSpec *PosDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalHashAgg *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalHashAgg == pop->Eopid() ||
					EopPhysicalHashAggDeduplicate == pop->Eopid());

		return reinterpret_cast<CPhysicalHashAgg *>(pop);
	}

};	// class CPhysicalHashAgg

}  // namespace gpopt


#endif	// !GPOS_CPhysicalHashAgg_H

// EOF
