//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CPhysicalPartitionSelectorDML.h
//
//	@doc:
//		Physical partition selector operator used for DML on partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalPartitionSelectorDML_H
#define GPOPT_CPhysicalPartitionSelectorDML_H

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPhysicalPartitionSelector.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalPartitionSelectorDML
//
//	@doc:
//		Physical partition selector operator used for DML on partitioned tables
//
//---------------------------------------------------------------------------
class CPhysicalPartitionSelectorDML : public CPhysicalPartitionSelector
{
private:
	// oid column - holds the OIDs for leaf parts
	CColRef *m_pcrOid;

public:
	CPhysicalPartitionSelectorDML(const CPhysicalPartitionSelectorDML &) =
		delete;

	// ctor
	CPhysicalPartitionSelectorDML(CMemoryPool *mp, IMDId *mdid,
								  UlongToExprMap *phmulexprEqPredicates,
								  CColRef *pcrOid);

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalPartitionSelectorDML;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalPartitionSelectorDML";
	}

	// oid column
	CColRef *
	PcrOid() const
	{
		return m_pcrOid;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hash function
	ULONG HashValue() const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required distribution of the n-th child
	CDistributionSpec *PdsRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CDistributionSpec *pdsRequired,
								   ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulOptReq) const override;

	// compute required sort order of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	// compute required partition propagation of the n-th child
	CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) override;

	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive partition index map
	CPartIndexMap *PpimDerive(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CDrvdPropCtxt *pdpctxt) const override;

	// derive partition filter map
	CPartFilterMap *PpfmDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

	// return distribution property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl,
		const CEnfdDistribution *ped) const override;

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalPartitionSelectorDML *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalPartitionSelectorDML == pop->Eopid());

		return dynamic_cast<CPhysicalPartitionSelectorDML *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CPhysicalPartitionSelectorDML

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalPartitionSelectorDML_H

// EOF
