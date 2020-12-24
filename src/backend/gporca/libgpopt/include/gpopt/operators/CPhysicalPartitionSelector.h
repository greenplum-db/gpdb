//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalPartitionSelector.h
//
//	@doc:
//		Physical partition selector operator used for property enforcement
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalPartitionSelector_H
#define GPOPT_CPhysicalPartitionSelector_H

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPhysical.h"


namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalPartitionSelector
//
//	@doc:
//		Physical partition selector operator used for property enforcement
//
//---------------------------------------------------------------------------
class CPhysicalPartitionSelector : public CPhysical
{
protected:
	// Scan id
	ULONG m_scan_id;

	// mdid of partitioned table
	IMDId *m_mdid;

	// partition keys
	CColRef2dArray *m_pdrgpdrgpcr;

	// part constraint map
	UlongToPartConstraintMap *m_ppartcnstrmap;

	// relation part constraint
	CPartConstraint *m_part_constraint;

	// expressions used in equality filters; for a filter of the form
	// pk1 = expr, we only store the expr
	UlongToExprMap *m_phmulexprEqPredicates;

	// expressions used in general predicates; we store the whole predicate
	// in this case (e.g. pk1 > 50)
	UlongToExprMap *m_phmulexprPredicates;

	// residual partition selection expression that cannot be split to
	// individual levels (e.g. pk1 < 5 OR pk2 = 6)
	CExpression *m_pexprResidual;

	// combined partition selection predicate
	CExpression *m_pexprCombinedPredicate;

	// ctor
	CPhysicalPartitionSelector(CMemoryPool *mp, IMDId *mdid,
							   UlongToExprMap *phmulexprEqPredicates);

	// return a single combined partition selection predicate
	CExpression *PexprCombinedPartPred(CMemoryPool *mp) const;

	// check whether two expression maps match
	static BOOL FMatchExprMaps(UlongToExprMap *phmulexprFst,
							   UlongToExprMap *phmulexprSnd);

private:
	// check whether part constraint maps match
	BOOL FMatchPartCnstr(UlongToPartConstraintMap *ppartcnstrmap) const;

	// check whether this operator has a partition selection filter
	BOOL FHasFilter() const;

	// check whether first part constraint map is contained in the second one
	static BOOL FSubsetPartCnstr(UlongToPartConstraintMap *ppartcnstrmapFst,
								 UlongToPartConstraintMap *ppartcnstrmapSnd);

public:
	CPhysicalPartitionSelector(const CPhysicalPartitionSelector &) = delete;

	// ctor
	CPhysicalPartitionSelector(CMemoryPool *mp, ULONG scan_id, IMDId *mdid,
							   CColRef2dArray *pdrgpdrgpcr,
							   UlongToPartConstraintMap *ppartcnstrmap,
							   CPartConstraint *ppartcnstr,
							   UlongToExprMap *phmulexprEqPredicates,
							   UlongToExprMap *phmulexprPredicates,
							   CExpression *pexprResidual);

	// dtor
	~CPhysicalPartitionSelector() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalPartitionSelector;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalPartitionSelector";
	}

	// scan id
	ULONG
	ScanId() const
	{
		return m_scan_id;
	}

	// partitioned table mdid
	IMDId *
	MDId() const
	{
		return m_mdid;
	}

	// partition keys
	CColRef2dArray *
	Pdrgpdrgpcr() const
	{
		return m_pdrgpdrgpcr;
	}

	// number of partitioning levels
	virtual ULONG UlPartLevels() const;

	// return a combined printable version of the partition selection predicate
	CExpression *
	PexprCombinedPred() const
	{
		return m_pexprCombinedPredicate;
	}

	// return the equality filter expression for the given level
	CExpression *PexprEqFilter(ULONG ulPartLevel) const;

	// return the filter expression for the given level
	CExpression *PexprFilter(ULONG ulPartLevel) const;

	// return the partition selection predicate for the given level
	CExpression *PexprPartPred(CMemoryPool *mp, ULONG ulPartLevel) const;

	// return the residual predicate
	CExpression *
	PexprResidualPred() const
	{
		return m_pexprResidual;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hash function
	ULONG HashValue() const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		// operator has one child
		return false;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *PcrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							 CColRefSet *pcrsRequired, ULONG child_index,
							 CDrvdPropArray *pdrgpdpCtxt,
							 ULONG ulOptReq) override;

	// compute required ctes of the n-th child
	CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
						  CCTEReq *pcter, ULONG child_index,
						  CDrvdPropArray *pdrgpdpCtxt,
						  ULONG ulOptReq) const override;

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required partition propagation of the n-th child
	CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) override;

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	// derive partition index map
	CPartIndexMap *PpimDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CDrvdPropCtxt *pdpctxt) const override;

	// derive partition filter map
	CPartFilterMap *PpfmDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return distribution property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl,
		const CEnfdDistribution *ped) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl,
		const CEnfdRewindability *per) const override;

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalPartitionSelector *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalPartitionSelector == pop->Eopid() ||
					EopPhysicalPartitionSelectorDML == pop->Eopid());

		return dynamic_cast<CPhysicalPartitionSelector *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CPhysicalPartitionSelector

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalPartitionSelector_H

// EOF
