//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPartitionPropagationSpec.h
//
//	@doc:
//		Partition Propagation spec in required properties
//---------------------------------------------------------------------------
#ifndef GPOPT_CPartitionPropagationSpec_H
#define GPOPT_CPartitionPropagationSpec_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CPropSpec.h"


namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CPartitionPropagationSpec
//
//	@doc:
//		Partition Propagation specification
//
//---------------------------------------------------------------------------
class CPartitionPropagationSpec : public CPropSpec
{
public:
	enum EPartPropSpecInfoType
	{
		EpptPropagator,
		EpptConsumer,
		EpptSentinel
	};

private:
	struct SPartPropSpecInfo : public CRefCount
	{
		// scan id of the DynamicScan
		ULONG m_scan_id;

		// info type: consumer or propagator
		EPartPropSpecInfoType m_type;

		// relation id of the DynamicScan
		IMDId *m_root_rel_mdid;

		//  partition selector ids to use (reqd only)
		CBitSet *m_selector_ids = nullptr;

		// filter expressions to generate partition pruning data in the translator (reqd only)
		CExpression *m_filter_expr = nullptr;

		SPartPropSpecInfo(ULONG scan_id, EPartPropSpecInfoType type,
						  IMDId *rool_rel_mdid)
			: m_scan_id(scan_id), m_type(type), m_root_rel_mdid(rool_rel_mdid)
		{
			GPOS_ASSERT(m_root_rel_mdid != nullptr);

			CMemoryPool *mp = COptCtxt::PoctxtFromTLS()->Pmp();
			m_selector_ids = GPOS_NEW(mp) CBitSet(mp);
		}

		~SPartPropSpecInfo() override
		{
			m_root_rel_mdid->Release();
			CRefCount::SafeRelease(m_selector_ids);
			CRefCount::SafeRelease(m_filter_expr);
		}

		IOstream &OsPrint(IOstream &os) const;

		// used for determining equality in memo (e.g in optimization contexts)
		BOOL Equals(const SPartPropSpecInfo *) const;

		BOOL FSatisfies(const SPartPropSpecInfo *) const;

		// used for sorting SPartPropSpecInfo in an array
		static INT CmpFunc(const void *val1, const void *val2);
	};

	using SPartPropSpecInfoArray =
		CDynamicPtrArray<SPartPropSpecInfo, CleanupRelease>;

	// partition required/derived info, sorted by scanid
	SPartPropSpecInfoArray *m_part_prop_spec_infos = nullptr;

	// Present scanids (for easy lookup)
	CBitSet *m_scan_ids = nullptr;

public:
	CPartitionPropagationSpec(const CPartitionPropagationSpec &) = delete;

	// ctor
	CPartitionPropagationSpec(CMemoryPool *mp);

	// dtor
	~CPartitionPropagationSpec() override;

	// append enforcers to dynamic array for the given plan properties
	void AppendEnforcers(CMemoryPool *mp, CExpressionHandle &exprhdl,
						 CReqdPropPlan *prpp, CExpressionArray *pdrgpexpr,
						 CExpression *pexpr) override;

	// hash function
	ULONG
	HashValue() const override
	{
		// GPDB_12_MERGE_FIXME: Implement this, even if it's unused
		GPOS_RTL_ASSERT(!"Unused");
	}

	// extract columns used by the partition propagation spec
	CColRefSet *
	PcrsUsed(CMemoryPool *mp) const override
	{
		// return an empty set
		return GPOS_NEW(mp) CColRefSet(mp);
	}

	// property type
	EPropSpecType
	Epst() const override
	{
		return EpstPartPropagation;
	}

	BOOL
	Contains(ULONG scan_id) const
	{
		return m_scan_ids->Get(scan_id);
	}

	BOOL ContainsAnyConsumers() const;

	// equality check to determine compatibility of derived & required properties
	BOOL Equals(const CPartitionPropagationSpec *ppps) const;

	// satisfies function
	BOOL FSatisfies(const CPartitionPropagationSpec *pps_reqd) const;


	SPartPropSpecInfo *FindPartPropSpecInfo(ULONG scan_id) const;

	void Insert(ULONG scan_id, EPartPropSpecInfoType type, IMDId *rool_rel_mdid,
				CBitSet *selector_ids, CExpression *expr);

	void Insert(SPartPropSpecInfo *other);

	void InsertAll(CPartitionPropagationSpec *pps);

	void InsertAllowedConsumers(CPartitionPropagationSpec *pps,
								CBitSet *allowed_scan_ids);

	void InsertAllExcept(CPartitionPropagationSpec *pps, ULONG scan_id);

	const CBitSet *SelectorIds(ULONG scan_id) const;

	// is partition propagation required
	BOOL
	FPartPropagationReqd() const
	{
		return true;
	}

	// print
	IOstream &OsPrint(IOstream &os) const override;

	void InsertAllResolve(CPartitionPropagationSpec *pSpec);
};	// class CPartitionPropagationSpec

}  // namespace gpopt

#endif	// !GPOPT_CPartitionPropagationSpec_H

// EOF
