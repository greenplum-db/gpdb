//	Greenplum Database
//	Copyright (C) 2016 VMware, Inc. or its affiliates.

#ifndef GPOPT_CPhysicalUnionAll_H
#define GPOPT_CPhysicalUnionAll_H

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CDistributionSpecHashed.h"

#include "gpopt/operators/CPhysical.h"
#include "gpopt/operators/COperator.h"


namespace gpopt
{
class CPhysicalUnionAll : public CPhysical
{
private:
	// output column array
	CColRefArray *const m_pdrgpcrOutput;

	// input column array
	CColRef2dArray *const m_pdrgpdrgpcrInput;

	// if this union is needed for partial indexes then store the scan
	// id, otherwise this will be gpos::ulong_max
	const ULONG m_ulScanIdPartialIndex;

	// set representation of input columns
	CColRefSetArray *m_pdrgpcrsInput;

	// array of child hashed distributions -- used locally for distribution derivation
	CDistributionSpecArray *m_pdrgpds;

	void PopulateDistrSpecs(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
							CColRef2dArray *pdrgpdrgpcrInput);

	// map given array of scalar ident expressions to positions of UnionAll input columns in the given child;
	ULongPtrArray *PdrgpulMap(CMemoryPool *mp, CExpressionArray *pdrgpexpr,
							  ULONG child_index) const;

	// map given ColRefSet, expressed in terms of outputs,
	// into an equivalent ColRefSet, expressed in terms
	// of input number n
	CColRefSet *MapOutputColRefsToInput(CMemoryPool *mp,
										CColRefSet *out_col_refs,
										ULONG child_index);

	// derive hashed distribution from child operators
	CDistributionSpecHashed *PdshashedDerive(CMemoryPool *mp,
											 CExpressionHandle &exprhdl) const;

	// derive strict random distribution spec if all the children of the parallel union all
	// node derive strict random; derive null spec otherwise
	CDistributionSpecRandom *PdsStrictRandomParallelUnionAllChildren(
		CMemoryPool *mp, CExpressionHandle &expr_handle) const;

	// compute output hashed distribution matching the outer child's hashed distribution
	CDistributionSpecHashed *PdsMatching(
		CMemoryPool *mp, const ULongPtrArray *pdrgpulOuter) const;

	// derive output distribution based on child distribution
	CDistributionSpec *PdsDeriveFromChildren(CMemoryPool *mp,
											 CExpressionHandle &exprhdl) const;

protected:
	// compute required hashed distribution of the n-th child
	CDistributionSpecHashed *PdshashedPassThru(
		CMemoryPool *mp, CDistributionSpecHashed *pdshashedRequired,
		ULONG child_index) const;

public:
	CPhysicalUnionAll(CMemoryPool *mp, CColRefArray *pdrgpcrOutput,
					  CColRef2dArray *pdrgpdrgpcrInput,
					  ULONG ulScanIdPartialIndex);

	~CPhysicalUnionAll() override;

	// match function
	BOOL Matches(COperator *) const override;

	// ident accessors
	EOperatorId Eopid() const override = 0;

	const CHAR *SzId() const override = 0;

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	// accessor of output column array
	CColRefArray *PdrgpcrOutput() const;

	// accessor of input column array
	CColRef2dArray *PdrgpdrgpcrInput() const;

	// if this unionall is needed for partial indexes then return the scan
	// id, otherwise return gpos::ulong_max
	ULONG UlScanIdPartialIndex() const;

	// is this unionall needed for a partial index
	BOOL IsPartialIndex() const;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL FPassThruStats() const override;

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

	// compute required rewindability of the n-th child
	CRewindabilitySpec *PrsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									CRewindabilitySpec *prsRequired,
									ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const override;

	// compute required partition propagation of the n-th child
	CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) override;

	// conversion function
	static CPhysicalUnionAll *PopConvert(COperator *pop);


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

	// derive partition index map
	CPartIndexMap *PpimDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CDrvdPropCtxt *pdpctxt) const override;

	// derive partition filter map
	CPartFilterMap *PpfmDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------


	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &,		// exprhdl
		const CEnfdRewindability *	// per
	) const override;

	// return partition propagation property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetPartitionPropagation(
		CExpressionHandle &exprhdl,
		const CEnfdPartitionPropagation *pepp) const override;
};
}  // namespace gpopt

#endif
