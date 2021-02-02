//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalSequenceProject.h
//
//	@doc:
//		Physical Sequence Project operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalSequenceProject_H
#define GPOPT_CPhysicalSequenceProject_H

#include "gpos/base.h"

#include "gpopt/base/CWindowFrame.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
// fwd declarations
class CDistributionSpec;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalSequenceProject
//
//	@doc:
//		Physical Sequence Project operator
//
//---------------------------------------------------------------------------
class CPhysicalSequenceProject : public CPhysical
{
private:
	// partition by keys
	CDistributionSpec *m_pds;

	// order specs of child window functions
	COrderSpecArray *m_pdrgpos;

	// frames of child window functions
	CWindowFrameArray *m_pdrgpwf;

	// order spec to request from child
	COrderSpec *m_pos;

	// required columns in order/frame specs
	CColRefSet *m_pcrsRequiredLocal;

	// create local order spec
	void CreateOrderSpec(CMemoryPool *mp);

	// compute local required columns
	void ComputeRequiredLocalColumns(CMemoryPool *mp);

public:
	CPhysicalSequenceProject(const CPhysicalSequenceProject &) = delete;

	// ctor
	CPhysicalSequenceProject(CMemoryPool *mp, CDistributionSpec *pds,
							 COrderSpecArray *pdrgpos,
							 CWindowFrameArray *pdrgpwf);

	// dtor
	~CPhysicalSequenceProject() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalSequenceProject;
	}

	// operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalSequenceProject";
	}

	// partition by keys
	CDistributionSpec *
	Pds() const
	{
		return m_pds;
	}

	// order by keys
	COrderSpecArray *
	Pdrgpos() const
	{
		return m_pdrgpos;
	}

	// frame specifications
	CWindowFrameArray *
	Pdrgpwf() const
	{
		return m_pdrgpwf;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hashing function
	ULONG HashValue() const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return true;
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

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return false;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// print
	IOstream &OsPrint(IOstream &os) const override;

	// conversion function
	static CPhysicalSequenceProject *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(EopPhysicalSequenceProject == pop->Eopid());

		return dynamic_cast<CPhysicalSequenceProject *>(pop);
	}

};	// class CPhysicalSequenceProject

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalSequenceProject_H

// EOF
