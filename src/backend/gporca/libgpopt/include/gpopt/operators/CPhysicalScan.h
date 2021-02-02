//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp
//
//	@filename:
//		CPhysicalScan.h
//
//	@doc:
//		Base class for physical scan operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalScan_H
#define GPOPT_CPhysicalScan_H

#include "gpos/base.h"

#include "gpopt/base/CCTEMap.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysical.h"

namespace gpopt
{
// fwd declarations
class CTableDescriptor;
class CName;

//---------------------------------------------------------------------------
//	@class:
//		CPhysicalScan
//
//	@doc:
//		Base class for physical scan operators
//
//---------------------------------------------------------------------------
class CPhysicalScan : public CPhysical
{
protected:
	// alias for table
	const CName *m_pnameAlias;

	// table descriptor
	CTableDescriptor *m_ptabdesc;

	// output columns
	CColRefArray *m_pdrgpcrOutput;

	// distribution
	CDistributionSpec *m_pds;

	// stats of base table -- used for costing
	// if operator is index scan, this is the stats of table on which index is created
	IStatistics *m_pstatsBaseTable;

private:
	// compute stats of underlying table
	void ComputeTableStats(CMemoryPool *mp);

	// search the given array of predicates for an equality predicate that has
	// one side equal to given expression
	static CExpression *PexprMatchEqualitySide(
		CExpression *pexprToMatch,
		CExpressionArray *pdrgpexpr	 // array of predicates to inspect
	);

	// private copy ctor
	CPhysicalScan(const CPhysicalScan &);

public:
	// ctors
	CPhysicalScan(CMemoryPool *mp, const CName *pname, CTableDescriptor *,
				  CColRefArray *colref_array);

	// dtor
	~CPhysicalScan() override;

	// return table descriptor
	virtual CTableDescriptor *
	Ptabdesc() const
	{
		return m_ptabdesc;
	}

	// output columns
	virtual CColRefArray *
	PdrgpcrOutput() const
	{
		return m_pdrgpcrOutput;
	}

	// sensitivity to order of inputs
	BOOL FInputOrderSensitive() const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	CColRefSet *
	PcrsRequired(CMemoryPool *,		   // mp
				 CExpressionHandle &,  // exprhdl
				 CColRefSet *,		   // pcrsRequired
				 ULONG,				   // child_index
				 CDrvdPropArray *,	   // pdrgpdpCtxt
				 ULONG				   // ulOptReq
				 ) override
	{
		GPOS_ASSERT(!"CPhysicalScan has no children");
		return nullptr;
	}

	// compute required ctes of the n-th child
	CCTEReq *
	PcteRequired(CMemoryPool *,		   //mp,
				 CExpressionHandle &,  //exprhdl,
				 CCTEReq *,			   //pcter,
				 ULONG,				   //child_index,
				 CDrvdPropArray *,	   //pdrgpdpCtxt,
				 ULONG				   //ulOptReq
	) const override
	{
		GPOS_ASSERT(!"CPhysicalScan has no children");
		return nullptr;
	}

	// compute required sort columns of the n-th child
	COrderSpec *
	PosRequired(CMemoryPool *,		  // mp
				CExpressionHandle &,  // exprhdl
				COrderSpec *,		  // posRequired
				ULONG,				  // child_index
				CDrvdPropArray *,	  // pdrgpdpCtxt
				ULONG				  // ulOptReq
	) const override
	{
		GPOS_ASSERT(!"CPhysicalScan has no children");
		return nullptr;
	}

	// compute required distribution of the n-th child
	CDistributionSpec *
	PdsRequired(CMemoryPool *,		  // mp
				CExpressionHandle &,  // exprhdl
				CDistributionSpec *,  // pdsRequired
				ULONG,				  // child_index
				CDrvdPropArray *,	  // pdrgpdpCtxt
				ULONG				  // ulOptReq
	) const override
	{
		GPOS_ASSERT(!"CPhysicalScan has no children");
		return nullptr;
	}

	// compute required rewindability of the n-th child
	CRewindabilitySpec *
	PrsRequired(CMemoryPool *,		   //mp
				CExpressionHandle &,   //exprhdl
				CRewindabilitySpec *,  //prsRequired
				ULONG,				   // child_index
				CDrvdPropArray *,	   // pdrgpdpCtxt
				ULONG				   // ulOptReq
	) const override
	{
		GPOS_ASSERT(!"CPhysicalScan has no children");
		return nullptr;
	}


	// check if required columns are included in output columns
	BOOL FProvidesReqdCols(CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
						   ULONG ulOptReq) const override;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	COrderSpec *
	PosDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const override
	{
		// return empty sort order
		return GPOS_NEW(mp) COrderSpec(mp);
	}

	// derive distribution
	CDistributionSpec *PdsDerive(CMemoryPool *mp,
								 CExpressionHandle &exprhdl) const override;

	// derive cte map
	CCTEMap *
	PcmDerive(CMemoryPool *mp,
			  CExpressionHandle &  //exprhdl
	) const override
	{
		return GPOS_NEW(mp) CCTEMap(mp);
	}

	// derive rewindability
	CRewindabilitySpec *
	PrsDerive(CMemoryPool *mp,
			  CExpressionHandle &  // exprhdl
	) const override
	{
		// rewindability of output is always true
		return GPOS_NEW(mp)
			CRewindabilitySpec(CRewindabilitySpec::ErtRewindable,
							   CRewindabilitySpec::EmhtNoMotion);
	}

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

	// return rewindability property enforcing type for this operator
	CEnfdProp::EPropEnforcingType
	EpetRewindability(CExpressionHandle &,		  // exprhdl
					  const CEnfdRewindability *  // per
	) const override
	{
		// no need for enforcing rewindability on output
		return CEnfdProp::EpetUnnecessary;
	}

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	BOOL
	FPassThruStats() const override
	{
		return false;
	}

	// return true if operator is dynamic scan
	virtual BOOL
	FDynamicScan() const
	{
		return false;
	}

	// stats of underlying table
	IStatistics *
	PstatsBaseTable() const
	{
		return m_pstatsBaseTable;
	}

	// statistics derivation during costing
	virtual IStatistics *PstatsDerive(CMemoryPool *mp,
									  CExpressionHandle &exprhdl,
									  CReqdPropPlan *prpplan,
									  IStatisticsArray *stats_ctxt) const = 0;

	// conversion function
	static CPhysicalScan *PopConvert(COperator *pop);

};	// class CPhysicalScan

}  // namespace gpopt

#endif	// !GPOPT_CPhysicalScan_H

// EOF
