//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		COptCtxt.cpp
//
//	@doc:
//		Implementation of optimizer context
//---------------------------------------------------------------------------

#include "gpopt/base/COptCtxt.h"

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"

#include "gpopt/base/CDefaultComparator.h"
#include "gpopt/cost/ICostModel.h"
#include "gpopt/eval/IConstExprEvaluator.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "naucrates/traceflags/traceflags.h"


using namespace gpopt;

// value of the first value part id
ULONG COptCtxt::m_ulFirstValidPartId = 1;

//---------------------------------------------------------------------------
//	@function:
//		COptCtxt::COptCtxt
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
COptCtxt::COptCtxt(CMemoryPool *mp, CColumnFactory *col_factory,
				   CMDAccessor *md_accessor, IConstExprEvaluator *pceeval,
				   COptimizerConfig *optimizer_config)
	: CTaskLocalStorageObject(CTaskLocalStorage::EtlsidxOptCtxt),
	  m_mp(mp),
	  m_pcf(col_factory),
	  m_pmda(md_accessor),
	  m_pceeval(pceeval),
	  m_pcomp(GPOS_NEW(m_mp) CDefaultComparator(pceeval)),
	  m_auPartId(m_ulFirstValidPartId),
	  m_pcteinfo(nullptr),
	  m_pdrgpcrSystemCols(nullptr),
	  m_optimizer_config(optimizer_config),
	  m_fDMLQuery(false),
	  m_has_coordinator_only_tables(false),
	  m_has_replicated_tables(false),
	  m_scanid_to_part_map(nullptr),
	  m_selector_id_counter(0)
{
	GPOS_ASSERT(nullptr != mp);
	GPOS_ASSERT(nullptr != col_factory);
	GPOS_ASSERT(nullptr != md_accessor);
	GPOS_ASSERT(nullptr != pceeval);
	GPOS_ASSERT(nullptr != m_pcomp);
	GPOS_ASSERT(nullptr != optimizer_config);
	GPOS_ASSERT(nullptr != optimizer_config->GetCostModel());

	m_pcteinfo = GPOS_NEW(m_mp) CCTEInfo(m_mp);
	m_cost_model = optimizer_config->GetCostModel();
	m_direct_dispatchable_filters = GPOS_NEW(mp) CExpressionArray(mp);
	m_scanid_to_part_map = GPOS_NEW(m_mp) UlongToBitSetMap(m_mp);
	m_part_selector_info = GPOS_NEW(m_mp) SPartSelectorInfo(m_mp);
}


//---------------------------------------------------------------------------
//	@function:
//		COptCtxt::~COptCtxt
//
//	@doc:
//		dtor
//		Does not de-allocate memory pool!
//
//---------------------------------------------------------------------------
COptCtxt::~COptCtxt()
{
	GPOS_DELETE(m_pcf);
	GPOS_DELETE(m_pcomp);
	m_pceeval->Release();
	m_pcteinfo->Release();
	m_optimizer_config->Release();
	CRefCount::SafeRelease(m_pdrgpcrSystemCols);
	CRefCount::SafeRelease(m_direct_dispatchable_filters);
	m_scanid_to_part_map->Release();
	m_part_selector_info->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		COptCtxt::PoctxtCreate
//
//	@doc:
//		Factory method for optimizer context
//
//---------------------------------------------------------------------------
COptCtxt *
COptCtxt::PoctxtCreate(CMemoryPool *mp, CMDAccessor *md_accessor,
					   IConstExprEvaluator *pceeval,
					   COptimizerConfig *optimizer_config)
{
	GPOS_ASSERT(nullptr != optimizer_config);

	// CONSIDER:  - 1/5/09; allocate column factory out of given mem pool
	// instead of having it create its own;
	CColumnFactory *col_factory = GPOS_NEW(mp) CColumnFactory;

	COptCtxt *poctxt = nullptr;
	{
		// safe handling of column factory; since it owns a pool that would be
		// leaked if below allocation fails
		CAutoP<CColumnFactory> a_pcf;
		a_pcf = col_factory;
		a_pcf.Value()->Initialize();

		poctxt = GPOS_NEW(mp)
			COptCtxt(mp, col_factory, md_accessor, pceeval, optimizer_config);

		// detach safety
		(void) a_pcf.Reset();
	}
	return poctxt;
}


//---------------------------------------------------------------------------
//	@function:
//		COptCtxt::FAllEnforcersEnabled
//
//	@doc:
//		Return true if all enforcers are enabled
//
//---------------------------------------------------------------------------
BOOL
COptCtxt::FAllEnforcersEnabled()
{
	BOOL fEnforcerDisabled =
		GPOS_FTRACE(EopttraceDisableMotions) ||
		GPOS_FTRACE(EopttraceDisableMotionBroadcast) ||
		GPOS_FTRACE(EopttraceDisableMotionGather) ||
		GPOS_FTRACE(EopttraceDisableMotionHashDistribute) ||
		GPOS_FTRACE(EopttraceDisableMotionRandom) ||
		GPOS_FTRACE(EopttraceDisableMotionRountedDistribute) ||
		GPOS_FTRACE(EopttraceDisableSort) ||
		GPOS_FTRACE(EopttraceDisableSpool) ||
		GPOS_FTRACE(EopttraceDisablePartPropagation);

	return !fEnforcerDisabled;
}

void
COptCtxt::AddPartForScanId(ULONG scanid, ULONG index)
{
	CBitSet *parts = m_scanid_to_part_map->Find(&scanid);
	if (nullptr == parts)
	{
		parts = GPOS_NEW(m_mp) CBitSet(m_mp);
		m_scanid_to_part_map->Insert(GPOS_NEW(m_mp) ULONG(scanid), parts);
	}
	parts->ExchangeSet(index);
}

const SPartSelectorInfoEntry *
COptCtxt::GetPartSelectorInfo(ULONG selector_id) const
{
	return m_part_selector_info->Find(&selector_id);
}

BOOL
COptCtxt::AddPartSelectorInfo(ULONG selector_id, SPartSelectorInfoEntry *entry)
{
	ULONG *key = GPOS_NEW(m_mp) ULONG(selector_id);
	return m_part_selector_info->Insert(key, entry);
}
