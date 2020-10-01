//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalRowTrigger.h
//
//	@doc:
//		Physical row-level trigger operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalRowTrigger_H
#define GPOPT_CPhysicalRowTrigger_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
//---------------------------------------------------------------------------
//	@class:
//		CPhysicalRowTrigger
//
//	@doc:
//		Physical row-level trigger operator
//
//---------------------------------------------------------------------------
class CPhysicalRowTrigger : public CPhysical
{
private:
	// relation id on which triggers are to be executed
	IMDId *m_rel_mdid;

	// trigger type
	INT m_type;

	// old columns
	CColRefArray *m_pdrgpcrOld;

	// new columns
	CColRefArray *m_pdrgpcrNew;

	// required columns by local members
	CColRefSet *m_pcrsRequiredLocal;

	// private copy ctor
	CPhysicalRowTrigger(const CPhysicalRowTrigger &);

public:
	// ctor
	CPhysicalRowTrigger(CMemoryPool *mp, IMDId *rel_mdid, INT type,
						CColRefArray *pdrgpcrOld, CColRefArray *pdrgpcrNew);

	// dtor
	virtual ~CPhysicalRowTrigger();

	// ident accessors
	virtual EOperatorId
	Eopid() const
	{
		return EopPhysicalRowTrigger;
	}

	// return a string for operator name
	virtual const CHAR *
	SzId() const
	{
		return "CPhysicalRowTrigger";
	}

	// relation id
	IMDId *
	GetRelMdId() const
	{
		return m_rel_mdid;
	}

	// trigger type
	INT
	GetType() const
	{
		return m_type;
	}

	// old columns
	CColRefArray *
	PdrgpcrOld() const
	{
		return m_pdrgpcrOld;
	}

	// new columns
	CColRefArray *
	PdrgpcrNew() const
	{
		return m_pdrgpcrNew;
	}

	// operator specific hash function
	virtual ULONG HashValue() const;

	// match function
	virtual BOOL Matches(COperator *pop) const;

	// sensitivity to order of inputs
	virtual BOOL
	FInputOrderSensitive() const
	{
		return false;
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

	// compute required output columns of the n-th child
	virtual CColRefSet *PcrsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
		ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// compute required ctes of the n-th child
	virtual CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								  CCTEReq *pcter, ULONG child_index,
								  CDrvdPropArray *pdrgpdpCtxt,
								  ULONG ulOptReq) const;

	// compute required distribution of the n-th child
	virtual CDistributionSpec *PdsRequired(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CDistributionSpec *pdsRequired,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq) const;

	// compute required rewindability of the n-th child
	virtual CRewindabilitySpec *PrsRequired(CMemoryPool *mp,
											CExpressionHandle &exprhdl,
											CRewindabilitySpec *prsRequired,
											ULONG child_index,
											CDrvdPropArray *pdrgpdpCtxt,
											ULONG ulOptReq) const;

	// check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const;


	// compute required partition propagation of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive distribution
	virtual CDistributionSpec *PdsDerive(CMemoryPool *mp,
										 CExpressionHandle &exprhdl) const;

	// derive rewindability
	virtual CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
										  CExpressionHandle &exprhdl) const;

	// derive partition index map
	virtual CPartIndexMap *
	PpimDerive(CMemoryPool *,  // mp
			   CExpressionHandle &exprhdl,
			   CDrvdPropCtxt *	//pdpctxt
	) const
	{
		return PpimPassThruOuter(exprhdl);
	}

	// derive partition filter map
	virtual CPartFilterMap *
	PpfmDerive(CMemoryPool *,  // mp
			   CExpressionHandle &exprhdl) const
	{
		return PpfmPassThruOuter(exprhdl);
	}

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return rewindability property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &,		// exprhdl
		const CEnfdRewindability *	// per
	) const;

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	virtual BOOL
	FPassThruStats() const
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// conversion function
	static CPhysicalRowTrigger *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(NULL != pop);
		GPOS_ASSERT(EopPhysicalRowTrigger == pop->Eopid());

		return dynamic_cast<CPhysicalRowTrigger *>(pop);
	}

	// debug print
	virtual IOstream &OsPrint(IOstream &) const;

};	// class CPhysicalRowTrigger
}  // namespace gpopt

#endif	// !GPOPT_CPhysicalRowTrigger_H

// EOF
