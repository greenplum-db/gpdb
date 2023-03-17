//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysical.h
//
//	@doc:
//		Base class for all physical operators
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysical_H
#define GPOPT_CPhysical_H

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CDrvdPropPlan.h"
#include "gpopt/base/CEnfdDistribution.h"
#include "gpopt/base/CEnfdOrder.h"
#include "gpopt/base/CEnfdPartitionPropagation.h"
#include "gpopt/base/CEnfdRewindability.h"
#include "gpopt/base/COrderSpec.h"
#include "gpopt/base/CPartitionPropagationSpec.h"
#include "gpopt/base/CRewindabilitySpec.h"
#include "gpopt/operators/COperator.h"

// number of plan properties requested during optimization, currently, there are 4 properties:
// order, distribution, rewindability and partition propagation
#define GPOPT_PLAN_PROPS 4

namespace gpopt
{
using namespace gpos;

// arrays of unsigned integer arrays
using UlongPtrArray = CDynamicPtrArray<ULONG_PTR, CleanupDeleteArray>;

class CTableDescriptor;
class CCTEMap;

//---------------------------------------------------------------------------
//	@class:
//		CPhysical
//
//	@doc:
//		base class for all physical operators
//
//---------------------------------------------------------------------------
class CPhysical : public COperator
{
public:
	// the order in which operator triggers the execution of its children
	enum EChildExecOrder
	{
		EceoLeftToRight,  // children execute in left to right order
		EceoRightToLeft,  // children execute in right to left order

		EceoSentinel
	};

	enum EPropogatePartConstraint
	{
		EppcAllowed,
		EppcProhibited,

		EppcSentinel
	};

private:
	//---------------------------------------------------------------------------
	//	@class:
	//		CReqdColsRequest
	//
	//	@doc:
	//		Representation of incoming column requests during optimization
	//
	//---------------------------------------------------------------------------
	class CReqdColsRequest : public CRefCount
	{
	private:
		// incoming required columns
		CColRefSet *m_pcrsRequired;

		// index of target physical child for which required columns need to be computed
		ULONG m_ulChildIndex;

		// index of scalar child to be used when computing required columns
		ULONG m_ulScalarChildIndex;

	public:
		CReqdColsRequest(const CReqdColsRequest &) = delete;

		// ctor
		CReqdColsRequest(CColRefSet *pcrsRequired, ULONG child_index,
						 ULONG ulScalarChildIndex)
			: m_pcrsRequired(pcrsRequired),
			  m_ulChildIndex(child_index),
			  m_ulScalarChildIndex(ulScalarChildIndex)
		{
			GPOS_ASSERT(nullptr != pcrsRequired);
		}

		// dtor
		~CReqdColsRequest() override
		{
			m_pcrsRequired->Release();
		}

		// required columns
		CColRefSet *
		GetColRefSet() const
		{
			return m_pcrsRequired;
		}

		// child index to push requirements to
		ULONG
		UlChildIndex() const
		{
			return m_ulChildIndex;
		}

		// scalar child index
		ULONG
		UlScalarChildIndex() const
		{
			return m_ulScalarChildIndex;
		}

		// hash function
		static ULONG HashValue(const CReqdColsRequest *prcr);

		// equality function
		static BOOL Equals(const CReqdColsRequest *prcrFst,
						   const CReqdColsRequest *prcrSnd);

	};	// class CReqdColsRequest

	// map of incoming required columns request to computed column sets
	using ReqdColsReqToColRefSetMap =
		CHashMap<CReqdColsRequest, CColRefSet, CReqdColsRequest::HashValue,
				 CReqdColsRequest::Equals, CleanupRelease<CReqdColsRequest>,
				 CleanupRelease<CColRefSet>>;

	// hash map of child columns requests
	ReqdColsReqToColRefSetMap *m_phmrcr;

	// given an optimization context, the elements in this array represent is the
	// number of requests that operator will create for its child,
	// array entries correspond to order, distribution, rewindability and partition
	// propagation, respectively
	ULONG m_rgulOptReqs[GPOPT_PLAN_PROPS];

	// array of expanded requests
	UlongPtrArray *m_pdrgpulpOptReqsExpanded;

	// total number of optimization requests
	ULONG m_ulTotalOptRequests;

	// update number of requests of a given property
	void UpdateOptRequests(ULONG ulPropIndex, ULONG ulRequests);

	// check whether we can push a part table requirement to a given child, given
	// the knowledge of where the part index id is defined
	static BOOL FCanPushPartReqToChild(CBitSet *pbsPartConsumer,
									   ULONG child_index);

protected:
	// set number of order requests that operator creates for its child
	void
	SetOrderRequests(ULONG ulOrderReqs)
	{
		UpdateOptRequests(0 /*ulPropIndex*/, ulOrderReqs);
	}

	// set number of distribution requests that operator creates for its child
	void
	SetDistrRequests(ULONG ulDistrReqs)
	{
		UpdateOptRequests(1 /*ulPropIndex*/, ulDistrReqs);
	}

	// set number of rewindability requests that operator creates for its child
	void
	SetRewindRequests(ULONG ulRewindReqs)
	{
		UpdateOptRequests(2 /*ulPropIndex*/, ulRewindReqs);
	}

	// set number of partition propagation requests that operator creates for its child
	void
	SetPartPropagateRequests(ULONG ulPartPropagationReqs)
	{
		UpdateOptRequests(3 /*ulPropIndex*/, ulPartPropagationReqs);
	}

	// pass cte requirement to the n-th child
	CCTEReq *PcterNAry(CMemoryPool *mp, CExpressionHandle &exprhdl,
					   CCTEReq *pcter, ULONG child_index,
					   CDrvdPropArray *pdrgpdpCtxt) const;

	// helper for computing required columns of the n-th child by including used
	// columns and excluding defined columns of the scalar child
	CColRefSet *PcrsChildReqd(CMemoryPool *mp, CExpressionHandle &exprhdl,
							  CColRefSet *pcrsInput, ULONG child_index,
							  ULONG ulScalarIndex);


	// helper for a simple case of computing child's required sort order
	static COrderSpec *PosPassThru(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   COrderSpec *posInput, ULONG child_index);

	// helper for a simple case of computing child's required distribution
	static CDistributionSpec *PdsPassThru(CMemoryPool *mp,
										  CExpressionHandle &exprhdl,
										  CDistributionSpec *pdsInput,
										  ULONG child_index);

	// helper for computing child's required distribution when Singleton/Replicated
	// distributions must be requested
	static CDistributionSpec *PdsRequireSingletonOrReplicated(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CDistributionSpec *pdsInput, ULONG child_index, ULONG ulOptReq);

	// helper for computing child's required distribution in unary operators
	// with a single scalar child
	static CDistributionSpec *PdsUnary(CMemoryPool *mp,
									   CExpressionHandle &exprhdl,
									   CDistributionSpec *pdsInput,
									   ULONG child_index, ULONG ulOptReq);

	// helper for a simple case of computing child's required rewindability
	static CRewindabilitySpec *PrsPassThru(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CRewindabilitySpec *prsRequired,
										   ULONG child_index);

	// pass cte requirement to the child
	static CCTEReq *PcterPushThru(CCTEReq *pcter);

	// combine the derived CTE maps of the first n children
	// of the given expression handle
	static CCTEMap *PcmCombine(CMemoryPool *mp, CDrvdPropArray *pdrgpdpCtxt);

	// helper for common case of sort order derivation
	static COrderSpec *PosDerivePassThruOuter(CExpressionHandle &exprhdl);

	// helper for common case of distribution derivation
	static CDistributionSpec *PdsDerivePassThruOuter(
		CExpressionHandle &exprhdl);

	// helper for common case of rewindability derivation
	static CRewindabilitySpec *PrsDerivePassThruOuter(
		CMemoryPool *mp, CExpressionHandle &exprhdl);

	// helper for checking if output columns of a unary operator
	// that defines no new columns include the required columns
	static BOOL FUnaryProvidesReqdCols(CExpressionHandle &exprhdl,
									   CColRefSet *pcrsRequired);

	// Generate a singleton distribution spec request
	static CDistributionSpec *PdsRequireSingleton(CMemoryPool *mp,
												  CExpressionHandle &exprhdl,
												  CDistributionSpec *pds,
												  ULONG child_index);

	// return true if the given column set includes any of the columns defined by
	// the unary node, as given by the handle
	static BOOL FUnaryUsesDefinedColumns(CColRefSet *pcrs,
										 CExpressionHandle &exprhdl);

	// compute required distribution of the n-th child
	virtual CDistributionSpec *PdsRequired(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CDistributionSpec *pdsRequired,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq) const = 0;

	// distribution matching type
	virtual CEnfdDistribution::EDistributionMatching Edm(
		CReqdPropPlan *prppInput, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

public:
	CPhysical(const CPhysical &) = delete;

	// ctor
	explicit CPhysical(CMemoryPool *mp);

	// dtor
	~CPhysical() override
	{
		CRefCount::SafeRelease(m_phmrcr);
		CRefCount::SafeRelease(m_pdrgpulpOptReqsExpanded);
	}

	// helper to compute skew estimate based on given stats and distribution spec
	static CDouble GetSkew(IStatistics *stats, CDistributionSpec *pds);

	// type of operator
	BOOL
	FPhysical() const override
	{
		GPOS_ASSERT(!FLogical() && !FScalar() && !FPattern());
		return true;
	}

	// create base container of derived properties
	CDrvdProp *PdpCreate(CMemoryPool *mp) const override;

	// create base container of required properties
	CReqdProp *PrpCreate(CMemoryPool *mp) const override;

	//-------------------------------------------------------------------------------------
	// Required Plan Properties
	//-------------------------------------------------------------------------------------

	// compute required output columns of the n-th child
	virtual CColRefSet *PcrsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl, CColRefSet *pcrsRequired,
		ULONG child_index, CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) = 0;

	// compute required ctes of the n-th child
	virtual CCTEReq *PcteRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
								  CCTEReq *pcter, ULONG child_index,
								  CDrvdPropArray *pdrgpdpCtxt,
								  ULONG ulOptReq) const = 0;

	// compute distribution spec from the table descriptor
	static CDistributionSpec *PdsCompute(CMemoryPool *mp,
										 const CTableDescriptor *ptabdesc,
										 CColRefArray *pdrgpcrOutput,
										 CColRef *gp_segment_id);

	// compute required sort order of the n-th child
	virtual COrderSpec *PosRequired(CMemoryPool *mp, CExpressionHandle &exprhdl,
									COrderSpec *posRequired, ULONG child_index,
									CDrvdPropArray *pdrgpdpCtxt,
									ULONG ulOptReq) const = 0;

	// compute required rewindability of the n-th child
	virtual CRewindabilitySpec *PrsRequired(CMemoryPool *mp,
											CExpressionHandle &exprhdl,
											CRewindabilitySpec *prsRequired,
											ULONG child_index,
											CDrvdPropArray *pdrgpdpCtxt,
											ULONG ulOptReq) const = 0;

	// compute required partition propoagation spec of the n-th child
	virtual CPartitionPropagationSpec *PppsRequired(
		CMemoryPool *mp, CExpressionHandle &exprhdl,
		CPartitionPropagationSpec *pppsRequired, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq) const;


	// required properties: check if required columns are included in output columns
	virtual BOOL FProvidesReqdCols(CExpressionHandle &exprhdl,
								   CColRefSet *pcrsRequired,
								   ULONG ulOptReq) const = 0;

	// required properties: check if required CTEs are included in derived CTE map
	virtual BOOL FProvidesReqdCTEs(CExpressionHandle &exprhdl,
								   const CCTEReq *pcter) const;

	//-------------------------------------------------------------------------------------
	// Derived Plan Properties
	//-------------------------------------------------------------------------------------

	// derive sort order
	virtual COrderSpec *PosDerive(CMemoryPool *mp,
								  CExpressionHandle &exprhdl) const = 0;

	// dderive distribution
	virtual CDistributionSpec *PdsDerive(CMemoryPool *mp,
										 CExpressionHandle &exprhdl) const = 0;

	// derived properties: derive rewindability
	virtual CRewindabilitySpec *PrsDerive(CMemoryPool *mp,
										  CExpressionHandle &exprhdl) const = 0;

	// derived properties: derive partition propagation spec
	virtual CPartitionPropagationSpec *PppsDerive(
		CMemoryPool *mp, CExpressionHandle &exprhdl) const;

	// derive cte map
	virtual CCTEMap *PcmDerive(CMemoryPool *mp,
							   CExpressionHandle &exprhdl) const;

	//-------------------------------------------------------------------------------------
	// Enforced Properties
	// See CEngine::FCheckEnfdProps() for comments on usage.
	//-------------------------------------------------------------------------------------

	// return order property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetOrder(
		CExpressionHandle &exprhdl, const CEnfdOrder *peo) const = 0;

	// return distribution property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetDistribution(
		CExpressionHandle &exprhdl, const CEnfdDistribution *ped) const;

	// return rewindability property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetRewindability(
		CExpressionHandle &exprhdl, const CEnfdRewindability *per) const = 0;

	// return partition propagation property enforcing type for this operator
	virtual CEnfdProp::EPropEnforcingType EpetPartitionPropagation(
		CExpressionHandle &exprhdl, const CEnfdPartitionPropagation *per) const;

	// order matching type
	virtual CEnfdOrder::EOrderMatching Eom(CReqdPropPlan *prppInput,
										   ULONG child_index,
										   CDrvdPropArray *pdrgpdpCtxt,
										   ULONG ulOptReq);

	// rewindability matching type
	virtual CEnfdRewindability::ERewindabilityMatching Erm(
		CReqdPropPlan *prppInput, ULONG child_index,
		CDrvdPropArray *pdrgpdpCtxt, ULONG ulOptReq);

	// check if optimization contexts is valid
	virtual BOOL
	FValidContext(CMemoryPool *,			   // mp
				  COptimizationContext *,	   // poc,
				  COptimizationContextArray *  // pdrgpocChild
	) const
	{
		return true;
	}

	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------

	// execution order of children
	virtual EChildExecOrder
	Eceo() const
	{
		// by default, children execute in left to right order
		return EceoLeftToRight;
	}

	// number of order requests that operator creates for its child
	ULONG
	UlOrderRequests() const
	{
		return m_rgulOptReqs[0];
	}

	// number of distribution requests that operator creates for its child
	ULONG
	UlDistrRequests() const
	{
		return m_rgulOptReqs[1];
	}

	// number of rewindability requests that operator creates for its child
	ULONG
	UlRewindRequests() const
	{
		return m_rgulOptReqs[2];
	}

	// number of partition propagation requests that operator creates for its child
	ULONG
	UlPartPropagateRequests() const
	{
		return m_rgulOptReqs[3];
	}

	// return total number of optimization requests
	ULONG
	UlOptRequests() const
	{
		return m_ulTotalOptRequests;
	}

	// map request number to order, distribution, rewindability and partition propagation requests
	void LookupRequest(
		ULONG ulReqNo,		  // input: request number
		ULONG *pulOrderReq,	  // output: order request number
		ULONG *pulDistrReq,	  // output: distribution request number
		ULONG *pulRewindReq,  // output: rewindability request number
		ULONG *
			pulPartPropagateReq	 // output: partition propagation request number
	);

	// return true if operator passes through stats obtained from children,
	// this is used when computing stats during costing
	virtual BOOL FPassThruStats() const = 0;

	// true iff the delivered distributions of the children are compatible among themselves
	virtual BOOL FCompatibleChildrenDistributions(
		const CExpressionHandle &exprhdl) const;

	// return a copy of the operator with remapped columns
	COperator *PopCopyWithRemappedColumns(CMemoryPool *mp,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist) override;

	// conversion function
	static CPhysical *
	PopConvert(COperator *pop)
	{
		GPOS_ASSERT(nullptr != pop);
		GPOS_ASSERT(pop->FPhysical());

		return dynamic_cast<CPhysical *>(pop);
	}

	// helper for computing a singleton distribution matching the given distribution
	static CDistributionSpecSingleton *PdssMatching(
		CMemoryPool *mp, CDistributionSpecSingleton *pdss);

	virtual CEnfdDistribution *Ped(CMemoryPool *mp, CExpressionHandle &exprhdl,
								   CReqdPropPlan *prppInput, ULONG child_index,
								   CDrvdPropArray *pdrgpdpCtxt,
								   ULONG ulDistrReq);

};	// class CPhysical

}  // namespace gpopt


#endif	// !GPOPT_CPhysical_H

// EOF
