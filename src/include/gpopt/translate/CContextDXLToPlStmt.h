//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CContextDXLToPlStmt.h
//
//	@doc:
//		Class providing access to CIdGenerators (needed to number initplans, motion
//		nodes as well as params), list of RangeTableEntries and Subplans
//		generated so far during DXL-->PlStmt translation.
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CContextDXLToPlStmt_H
#define GPDXL_CContextDXLToPlStmt_H

#include <vector>

#include "gpos/base.h"

#include "gpopt/gpdbwrappers.h"
#include "gpopt/translate/CDXLTranslateContext.h"
#include "gpopt/translate/CDXLTranslateContextBaseTable.h"
#include "gpopt/translate/CTranslatorUtils.h"
#include "naucrates/dxl/CIdGenerator.h"
#include "naucrates/dxl/gpdb_types.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"

extern "C" {
#include "nodes/plannodes.h"
}

namespace gpdxl
{
// fwd decl
class CDXLTranslateContext;

using HMUlDxltrctx =
	CHashMap<ULONG, CDXLTranslateContext, gpos::HashValue<ULONG>,
			 gpos::Equals<ULONG>, CleanupDelete<ULONG>,
			 CleanupDelete<CDXLTranslateContext>>;

//---------------------------------------------------------------------------
//	@class:
//		CContextDXLToPlStmt
//
//	@doc:
//		Class providing access to CIdGenerators (needed to number initplans, motion
//		nodes as well as params), list of RangeTableEntries and Subplans
//		generated so far during DXL-->PlStmt translation.
//
//---------------------------------------------------------------------------
class CContextDXLToPlStmt
{
private:
	// cte consumer information
	struct SCTEConsumerInfo
	{
		// list of ShareInputScan represent cte consumers
		List *m_cte_consumer_list;

		// ctor
		SCTEConsumerInfo(List *plan_cte) : m_cte_consumer_list(plan_cte)
		{
		}

		void
		AddCTEPlan(ShareInputScan *share_input_scan)
		{
			GPOS_ASSERT(nullptr != share_input_scan);
			m_cte_consumer_list =
				gpdb::LAppend(m_cte_consumer_list, share_input_scan);
		}

		~SCTEConsumerInfo()
		{
			gpdb::ListFree(m_cte_consumer_list);
		}
	};

	// hash maps mapping ULONG -> SCTEConsumerInfo
	using HMUlCTEConsumerInfo =
		CHashMap<ULONG, SCTEConsumerInfo, gpos::HashValue<ULONG>,
				 gpos::Equals<ULONG>, CleanupDelete<ULONG>,
				 CleanupDelete<SCTEConsumerInfo>>;

	using HMUlIndex =
		CHashMap<ULONG, Index, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
				 CleanupDelete<ULONG>, CleanupDelete<Index>>;

	CMemoryPool *m_mp;

	// counter for generating plan ids
	CIdGenerator *m_plan_id_counter;

	// counter for generating motion ids
	CIdGenerator *m_motion_id_counter;

	// counter for generating unique param ids
	CIdGenerator *m_param_id_counter;
	List *m_param_types_list;

	// What operator classes to use for distribution keys?
	DistributionHashOpsKind m_distribution_hashops;

	// list of all rtable entries
	List *m_rtable_entries_list;

	// list of all subplan entries
	List *m_subplan_entries_list;
	List *m_subplan_sliceids_list;

	// List of PlanSlices
	List *m_slices_list;

	PlanSlice *m_current_slice;

	// index of the target relation in the rtable or 0 if not a DML statement
	ULONG m_result_relation_index;

	// hash map of the cte identifiers and the cte consumers with the same cte identifier
	HMUlCTEConsumerInfo *m_cte_consumer_info;

	// into clause
	IntoClause *m_into_clause;

	// CTAS distribution policy
	GpPolicy *m_distribution_policy;

	UlongToUlongMap *m_part_selector_to_param_map;

	// hash map of the queryid (of DML query) and the target relation index
	HMUlIndex *m_used_rte_indexes;

public:
	// ctor/dtor
	CContextDXLToPlStmt(CMemoryPool *mp, CIdGenerator *plan_id_counter,
						CIdGenerator *motion_id_counter,
						CIdGenerator *param_id_counter,
						DistributionHashOpsKind distribution_hashops);

	// dtor
	~CContextDXLToPlStmt();

	// retrieve the next plan id
	ULONG GetNextPlanId();

	// retrieve the current motion id
	ULONG GetCurrentMotionId();

	// retrieve the next motion id
	ULONG GetNextMotionId();

	// retrieve the current parameter type list
	List *GetParamTypes();

	// retrieve the next parameter id
	ULONG GetNextParamId(OID typeoid);

	// add a newly found CTE consumer
	void AddCTEConsumerInfo(ULONG cte_id, ShareInputScan *share_input_scan);

	// return the list of shared input scan plans representing the CTE consumers
	List *GetCTEConsumerList(ULONG cte_id) const;

	// return list of range table entries
	List *
	GetRTableEntriesList() const
	{
		return m_rtable_entries_list;
	}

	List *
	GetSubplanEntriesList() const
	{
		return m_subplan_entries_list;
	}

	// index of result relation in the rtable
	ULONG
	GetResultRelationIndex() const
	{
		return m_result_relation_index;
	}


	int *GetSubplanSliceIdArray();

	PlanSlice *GetSlices(int *numSlices_p);

	// add a range table entry
	void AddRTE(RangeTblEntry *rte, BOOL is_result_relation = false);

	void AddSubplan(Plan *);

	// add a slice table entry
	int AddSlice(PlanSlice *);

	PlanSlice *
	GetCurrentSlice() const
	{
		return m_current_slice;
	}

	void
	SetCurrentSlice(PlanSlice *slice)
	{
		m_current_slice = slice;
	}

	// add CTAS information
	void AddCtasInfo(IntoClause *into_clause, GpPolicy *distribution_policy);

	// into clause
	IntoClause *
	GetIntoClause() const
	{
		return m_into_clause;
	}

	// CTAS distribution policy
	GpPolicy *
	GetDistributionPolicy() const
	{
		return m_distribution_policy;
	}

	// Get the hash opclass or hash function for given datatype,
	// based on decision made by DetermineDistributionHashOpclasses()
	Oid GetDistributionHashOpclassForType(Oid typid);
	Oid GetDistributionHashFuncForType(Oid typid);

	ULONG GetParamIdForSelector(OID oid_type, const ULONG selectorId);

	Index FindRTE(Oid reloid);

	// used by internal GPDB functions to build the RelOptInfo when creating foreign scans
	Query *m_orig_query;

	// get rte from m_rtable_entries_list by given index
	RangeTblEntry *GetRTEByIndex(Index index);

	Index GetRTEIndexByTableDescr(const CDXLTableDescr *table_descr,
								  BOOL *is_rte_exists);
};

}  // namespace gpdxl
#endif	// !GPDXL_CContextDXLToPlStmt_H

//EOF
