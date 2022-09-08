//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPartitionPropagationSpec.cpp
//
//	@doc:
//		Specification of partition propagation requirements
//---------------------------------------------------------------------------

#include "gpopt/base/CPartitionPropagationSpec.h"

#include "gpopt/exception.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalPartitionSelector.h"

using namespace gpos;
using namespace gpopt;

// used for determining equality in memo (e.g in optimization contexts)
BOOL
CPartitionPropagationSpec::SPartPropSpecInfo::Equals(
	const SPartPropSpecInfo *other) const
{
	GPOS_ASSERT_IMP(m_scan_id == other->m_scan_id,
					m_root_rel_mdid->Equals(other->m_root_rel_mdid));
	return m_scan_id == other->m_scan_id && m_type == other->m_type &&
		   m_selector_ids->Equals(other->m_selector_ids);
}

BOOL
CPartitionPropagationSpec::SPartPropSpecInfo::FSatisfies(
	const SPartPropSpecInfo *other) const
{
	GPOS_ASSERT_IMP(m_scan_id == other->m_scan_id,
					m_root_rel_mdid->Equals(other->m_root_rel_mdid));
	return m_scan_id == other->m_scan_id && m_type == other->m_type;
}

// used for sorting SPartPropSpecInfo in an array
// NB: this serves a different purpose than  Equals(); it is used only to
// maintain a sorted array in CPartitionPropagationSpec.
// Eg, consumer<1>(10) and consumer<1>(10,11) will be treated as equal by
// CmpFunc, but as non-equal by Equals
INT
CPartitionPropagationSpec::SPartPropSpecInfo::CmpFunc(const void *val1,
													  const void *val2)
{
	const SPartPropSpecInfo *info1 = *(const SPartPropSpecInfo **) val1;
	const SPartPropSpecInfo *info2 = *(const SPartPropSpecInfo **) val2;

	return info1->m_scan_id - info2->m_scan_id;
}


CPartitionPropagationSpec::CPartitionPropagationSpec(CMemoryPool *mp)
{
	m_part_prop_spec_infos = GPOS_NEW(mp) UlongToSPartPropSpecInfoMap(mp);
	m_scan_ids = GPOS_NEW(mp) CBitSet(mp);
}

// dtor
CPartitionPropagationSpec::~CPartitionPropagationSpec()
{
	m_part_prop_spec_infos->Release();
	m_scan_ids->Release();
}

BOOL
CPartitionPropagationSpec::Equals(const CPartitionPropagationSpec *pps) const
{
	if ((m_part_prop_spec_infos == nullptr) &&
		(pps->m_part_prop_spec_infos == nullptr))
	{
		return true;
	}

	if ((m_part_prop_spec_infos == nullptr) ^
		(pps->m_part_prop_spec_infos == nullptr))
	{
		return false;
	}

	GPOS_ASSERT(m_part_prop_spec_infos != nullptr &&
				pps->m_part_prop_spec_infos != nullptr);

	if (m_part_prop_spec_infos->Size() != pps->m_part_prop_spec_infos->Size())
	{
		return false;
	}

	UlongToSPartPropSpecInfoMapIter hmulpi(m_part_prop_spec_infos);
	UlongToSPartPropSpecInfoMapIter hmulpi_other(pps->m_part_prop_spec_infos);
	while (hmulpi.Advance() && hmulpi_other.Advance())
	{
		const SPartPropSpecInfo *info = hmulpi.Value();
		const SPartPropSpecInfo *info_other = hmulpi_other.Value();
		if (!info->Equals(info_other))
		{
			return false;
		}
	}
	return hmulpi.Advance() == hmulpi_other.Advance();
}

CPartitionPropagationSpec::SPartPropSpecInfo *
CPartitionPropagationSpec::FindPartPropSpecInfo(ULONG scan_id) const
{
	if (!Contains(scan_id))
	{
		return nullptr;
	}

	SPartPropSpecInfo *info = m_part_prop_spec_infos->Find(&scan_id);
	GPOS_RTL_ASSERT(info != nullptr);

	return info;
}

const CBitSet *
CPartitionPropagationSpec::SelectorIds(ULONG scan_id) const
{
	SPartPropSpecInfo *found_info = FindPartPropSpecInfo(scan_id);

	if (found_info == nullptr)
	{
		GPOS_RTL_ASSERT(!"Scan id not found in CPartitionPropagationSpec!");
	}

	return found_info->m_selector_ids;
}

void
CPartitionPropagationSpec::Insert(ULONG scan_id, EPartPropSpecInfoType type,
								  IMDId *rool_rel_mdid, CBitSet *selector_ids,
								  CExpression *expr)
{
	GPOS_RTL_ASSERT(!Contains(scan_id));

	CMemoryPool *mp = COptCtxt::PoctxtFromTLS()->Pmp();
	rool_rel_mdid->AddRef();
	SPartPropSpecInfo *info =
		GPOS_NEW(mp) SPartPropSpecInfo(scan_id, type, rool_rel_mdid);

	if (selector_ids != nullptr)
	{
		info->m_selector_ids->Union(selector_ids);
	}

	if (expr != nullptr)
	{
		expr->AddRef();
		info->m_filter_expr = expr;
	}

	m_scan_ids->ExchangeSet(scan_id);
	m_part_prop_spec_infos->Insert(GPOS_NEW(mp) ULONG(scan_id), info);
}

void
CPartitionPropagationSpec::Insert(SPartPropSpecInfo *other)
{
	Insert(other->m_scan_id, other->m_type, other->m_root_rel_mdid,
		   other->m_selector_ids, other->m_filter_expr);
}

void
CPartitionPropagationSpec::InsertAll(CPartitionPropagationSpec *pps)
{
	UlongToSPartPropSpecInfoMapIter hmulpi(pps->m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		SPartPropSpecInfo *other_info =
			const_cast<SPartPropSpecInfo *>(hmulpi.Value());

		SPartPropSpecInfo *found_info =
			FindPartPropSpecInfo(other_info->m_scan_id);

		if (found_info == nullptr)
		{
			Insert(other_info);
			continue;
		}

		GPOS_ASSERT(found_info->m_root_rel_mdid == other_info->m_root_rel_mdid);

		// for a given scan-id, only a consumer request can be merged with an
		// existing consumer request; so bail in all other cases; eg: merging a
		// a propagator or merging a consumer when a propagator was already inserted
		GPOS_RTL_ASSERT(found_info->m_type == EpptConsumer &&
						other_info->m_type == EpptConsumer);

		found_info->m_selector_ids->Union(other_info->m_selector_ids);
	}
}

void
CPartitionPropagationSpec::InsertAllowedConsumers(
	CPartitionPropagationSpec *pps, CBitSet *allowed_scan_ids)
{
	UlongToSPartPropSpecInfoMapIter hmulpi(pps->m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		SPartPropSpecInfo *other_info =
			const_cast<SPartPropSpecInfo *>(hmulpi.Value());

		// only process allowed_scan_ids ...
		if (allowed_scan_ids != nullptr &&
			!allowed_scan_ids->Get(other_info->m_scan_id))
		{
			continue;
		}

		// ... and consumers
		if (other_info->m_type != EpptConsumer)
		{
			continue;
		}

		SPartPropSpecInfo *found_info =
			FindPartPropSpecInfo(other_info->m_scan_id);

		if (found_info == nullptr)
		{
			Insert(other_info);
			continue;
		}

		GPOS_ASSERT(found_info->m_root_rel_mdid == other_info->m_root_rel_mdid);

		// for a given scan-id, only a consumer request can be merged with an
		// existing consumer request; so bail in all other cases; eg: merging a
		// a propagator or merging a consumer when a propagator was already inserted
		GPOS_RTL_ASSERT(found_info->m_type == EpptConsumer &&
						other_info->m_type == EpptConsumer);

		found_info->m_selector_ids->Union(other_info->m_selector_ids);
	}
}

void
CPartitionPropagationSpec::InsertAllExcept(CPartitionPropagationSpec *pps,
										   ULONG scan_id)
{
	UlongToSPartPropSpecInfoMapIter hmulpi(pps->m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		SPartPropSpecInfo *other_info =
			const_cast<SPartPropSpecInfo *>(hmulpi.Value());

		if (other_info->m_scan_id == scan_id)
		{
			continue;
		}

		SPartPropSpecInfo *found_info =
			FindPartPropSpecInfo(other_info->m_scan_id);

		if (found_info == nullptr)
		{
			Insert(other_info);
			continue;
		}

		GPOS_ASSERT(found_info->m_root_rel_mdid == other_info->m_root_rel_mdid);

		// for a given scan-id, only a consumer request can be merged with an
		// existing consumer request; so bail in all other cases; eg: merging a
		// a propagator or merging a consumer when a propagator was already inserted
		GPOS_RTL_ASSERT(found_info->m_type == EpptConsumer &&
						other_info->m_type == EpptConsumer);

		found_info->m_selector_ids->Union(other_info->m_selector_ids);
	}
}

void
CPartitionPropagationSpec::InsertAllResolve(CPartitionPropagationSpec *pps)
{
	UlongToSPartPropSpecInfoMapIter hmulpi(pps->m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		SPartPropSpecInfo *other_info =
			const_cast<SPartPropSpecInfo *>(hmulpi.Value());

		SPartPropSpecInfo *found_info =
			FindPartPropSpecInfo(other_info->m_scan_id);

		if (found_info == nullptr)
		{
			Insert(other_info);
			continue;
		}

		GPOS_ASSERT(found_info->m_scan_id == other_info->m_scan_id);

		if (found_info->m_type == EpptConsumer &&
			other_info->m_type == EpptPropagator)
		{
			// this scan_id is resolved (used in joins), don't add it to the result
			continue;
		}

		if (found_info->m_type == EpptPropagator &&
			other_info->m_type == EpptConsumer)
		{
			// Currently unreachable because it is only called from HJ which calls
			// this method on the pps derived from its outer child
			// implement this with a Delete(found_info)
			GPOS_ASSERT(!"Unreachable");
		}

		// when resolving, ignore requests of the same type; eg: same
		// consumer<1> or propagator<1> requests from both children of a join
		GPOS_ASSERT(!"Cannot resolve requests of the same type!");
	}
}


BOOL
CPartitionPropagationSpec::FSatisfies(
	const CPartitionPropagationSpec *pps_reqd) const
{
	if (pps_reqd->m_part_prop_spec_infos == nullptr)
	{
		return true;
	}

	UlongToSPartPropSpecInfoMapIter hmulpi(pps_reqd->m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		const SPartPropSpecInfo *reqd_info = hmulpi.Value();
		SPartPropSpecInfo *found_info =
			FindPartPropSpecInfo(reqd_info->m_scan_id);

		if (found_info == nullptr || !found_info->FSatisfies(reqd_info))
		{
			return false;
		}
	}
	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CPartitionPropagationSpec::AppendEnforcers
//
//	@doc:
//		Add required enforcers to dynamic array
//
//---------------------------------------------------------------------------
void
CPartitionPropagationSpec::AppendEnforcers(CMemoryPool *mp,
										   CExpressionHandle &exprhdl,
										   CReqdPropPlan *,
										   CExpressionArray *pdrgpexpr,
										   CExpression *expr)
{
	UlongToSPartPropSpecInfoMapIter hmulpi(m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		const SPartPropSpecInfo *info = hmulpi.Value();

		if (info->m_type != CPartitionPropagationSpec::EpptPropagator)
		{
			continue;
		}

		COptCtxt *opt_ctxt = COptCtxt::PoctxtFromTLS();
		ULONG selector_id = opt_ctxt->NextPartSelectorId();

		info->m_root_rel_mdid->AddRef();
		info->m_filter_expr->AddRef();
		expr->AddRef();

		CExpression *part_selector = GPOS_NEW(mp)
			CExpression(mp,
						GPOS_NEW(mp) CPhysicalPartitionSelector(
							mp, info->m_scan_id, selector_id,
							info->m_root_rel_mdid, info->m_filter_expr),
						expr);

		IStatistics *stats = exprhdl.Pstats();

		info->m_filter_expr->AddRef();
		stats->AddRef();
		opt_ctxt->AddPartSelectorInfo(
			selector_id, GPOS_NEW(mp) SPartSelectorInfoEntry(
							 selector_id, info->m_filter_expr, stats));


		pdrgpexpr->Append(part_selector);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPartitionPropagationSpec::OsPrint
//
//	@doc:
//		Print function
//
//---------------------------------------------------------------------------
IOstream &
CPartitionPropagationSpec::OsPrint(IOstream &os) const
{
	if (nullptr == m_part_prop_spec_infos ||
		m_part_prop_spec_infos->Size() == 0)
	{
		os << "<empty>";
		return os;
	}

	ULONG ul = 0;
	UlongToSPartPropSpecInfoMapIter hmulpi(m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		const SPartPropSpecInfo *part_info = hmulpi.Value();

		switch (part_info->m_type)
		{
			case EpptConsumer:
				os << "consumer";
				break;
			case EpptPropagator:
				os << "propagator";
			default:
				break;
		}

		os << "<" << part_info->m_scan_id << ">";
		os << "(";
		part_info->m_selector_ids->OsPrint(os);
		os << ")";

		if (ul < (m_part_prop_spec_infos->Size() - 1))
		{
			os << ", ";
		}

		ul += 1;
	}
	return os;
}

BOOL
CPartitionPropagationSpec::ContainsAnyConsumers() const
{
	if (nullptr == m_part_prop_spec_infos)
	{
		return false;
	}

	UlongToSPartPropSpecInfoMapIter hmulpi(m_part_prop_spec_infos);
	while (hmulpi.Advance())
	{
		const SPartPropSpecInfo *info = hmulpi.Value();

		if (info->m_type == CPartitionPropagationSpec::EpptConsumer)
		{
			return true;
		}
	}

	return false;
}

// EOF
