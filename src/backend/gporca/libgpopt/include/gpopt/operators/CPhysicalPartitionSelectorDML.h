//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
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

	// private copy ctor
	CPhysicalPartitionSelectorDML(const CPhysicalPartitionSelectorDML &);

public:
	// ctor
	CPhysicalPartitionSelectorDML(CMemoryPool *mp, IMDId *mdid,
								  UlongToExprMap *phmulexprEqPredicates,
								  CColRef *pcrOid);

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalPartitionSelectorDML;
	}

	// operator name
	virtual const CHAR *
	SzId() const
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
	virtual BOOL Matches(COperator *pop) const;

	// hash function
	virtual ULONG HashValue() const;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required distribution of the n-th child
	virtual CDistributionSpec *PdsRequired(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CDistributionSpec *pdsRequired,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq) const;

	// compute required sort order of the n-th child
	virtual COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posRequired, ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const;

	// compute required partition propagation of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive partition index map
	virtual CPartIndexMap *PpimDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  CDrvdPropCtxt *pdpctxt) const;

	// derive partition filter map
	virtual CPartFilterMap *PpfmDerive(CMemoryPool *mp,
									   CExpressionHandle &exprhdl) const;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const;

	// return distribution property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl, const CEnfdDistribution *ped) const;

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
	virtual IOstream &OsPrint(IOstream &os) const;

};	// class CPhysicalPartitionSelectorDML

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalPartitionSelectorDML_H

// EOF
