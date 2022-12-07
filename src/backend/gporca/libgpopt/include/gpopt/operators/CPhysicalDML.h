//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalDML.h
//
//	@doc:
//		Physical DML operator
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalDML_H
#define GPOS_CPhysicalDML_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDML.h"
#include "gpopt/operators/CPhysical.h"


namespace gpopt
{
// fwd declaration
class CDistributionSpec;
class COptimizerConfig;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalDML
//
//	@doc:
//		Physical DML operator
//
//---------------------------------------------------------------------------
class CPhysicalDML : public CPhysical
{
private:
	// dml operator
	CLogicalDML::EDMLOperator m_edmlop;

	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// array of source columns
	CColRefArray *m_pdrgpcrSource;

	// set of modified columns from the target table
	CBitSet *m_pbsModified;

	// action column
	CColRef *m_pcrAction;

	// ctid column
	CColRef *m_pcrCtid;

	// segmentid column
	CColRef *m_pcrSegmentId;

	// target table distribution spec
	CDistributionSpec *m_pds;

	// required order spec
	COrderSpec *m_pos;

	// required columns by local members
	CColRefSet *m_pcrsRequiredLocal;

	// Split Update
	BOOL m_fSplit;

	// compute required order spec
	COrderSpec *PosComputeRequired(CMemoryPool *mp, CTableDescriptor *ptabdesc);

	// compute local required columns
	void ComputeRequiredLocalColumns(CMemoryPool *mp);

public:
	CPhysicalDML(const CPhysicalDML &) = delete;

	// ctor
	CPhysicalDML(CMemoryPool *mp, CLogicalDML::EDMLOperator edmlop,
				 CTableDescriptor *ptabdesc, CColRefArray *pdrgpcrSource,
				 CBitSet *pbsModified, CColRef *pcrAction, CColRef *pcrCtid,
				 CColRef *pcrSegmentId, BOOL fSplit);

	// dtor
	~CPhysicalDML() override;

	// ident accessors
	EOperatorId
	Eopid() const override
	{
		return EopPhysicalDML;
	}

	// return a string for operator name
	const CHAR *
	SzId() const override
	{
		return "CPhysicalDML";
	}

	// dml operator
	CLogicalDML::EDMLOperator
	Edmlop() const
	{
		return m_edmlop;
	}

	// table descriptor
	CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// action column
	CColRef *
	PcrAction() const
	{
		return m_pcrAction;
	}

	// ctid column
	CColRef *
	PcrCtid() const
	{
		return m_pcrCtid;
	}

	// segmentid column
	CColRef *
	PcrSegmentId() const
	{
		return m_pcrSegmentId;
	}

	// source columns
	virtual CColRefArray *
	PdrgpcrSource() const
	{
		return m_pdrgpcrSource;
	}

	// Is update using split
	BOOL
	FSplit() const
	{
		return m_fSplit;
	}

	// match function
	BOOL Matches(COperator *pop) const override;

	// hash function
	ULONG HashValue() const override;

	// sensitivity to order of inputs
	BOOL
	FInputOrderSensitive() const override
	{
		return false;
	}

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required sort columns of the n-th child
	COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
							COrderSpec *posRequired, ULONG child_index,
							CDrvdPropArray *pdrgpdpCtxt,
							ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *PosDerive(CMemoryPool *mp,
						  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const override;

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


	// distribution matching type
	CEnfdDistribution::EDistributionMatching
	Edm(CReqdPropPlan *,   // prppInput
		ULONG,			   // child_index
		CDrvdPropArray *,  //pdrgpdpCtxt
		ULONG			   // ulOptReq
		) override
	{
		if (CDistributionSpec::EdtSingleton == m_pds->Edt())
		{
			// if target table is master only, request simple satisfiability, as it will not introduce duplicates
			return CEnfdDistribution::EdmSatisfy;
		}

		// avoid duplicates by requesting exact matching of non-singleton distributions
		return CEnfdDistribution::EdmExact;
	}

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive rewindability
	CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const override;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	//-------------------------------------------------------------------------------------


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

	// conversion function
	static CPhysicalDML *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(COperator::EopPhysicalDML == pop->Eopid());

		return dynamic_cast<CPhysicalDML *>(pop);
	}

	// debug print
	IOstream &OsPrint(IOstream &os) const override;

};	// class CPhysicalDML

}  // namespace gpopt


#endif	// !GPOS_CPhysicalDML_H

// EOF
