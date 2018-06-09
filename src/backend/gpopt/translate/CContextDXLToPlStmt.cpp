//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CContextDXLToPlStmt.cpp
//
//	@doc:
//		Implementation of the functions that provide
//		access to CIdGenerators (needed to number initplans, motion
//		nodes as well as params), list of RangeTableEntires and Subplans
//		generated so far during DXL-->PlStmt translation.
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "postgres.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"

#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "gpopt/gpdbwrappers.h"
#include "gpos/base.h"
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::CContextDXLToPlStmt
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CContextDXLToPlStmt::CContextDXLToPlStmt
	(
	IMemoryPool *mp,
	CIdGenerator *plan_id_counter,
	CIdGenerator *motion_id_counter,
	CIdGenerator *param_id_counter,
	List **rtable_entries_list,
	List **subplan_entries_list
	)
	:
	m_mp(mp),
	m_plan_id_counter(plan_id_counter),
	m_motion_id_counter(motion_id_counter),
	m_param_id_counter(param_id_counter),
	m_rtable_entries_list(rtable_entries_list),
	m_partitioned_tables_list(NULL),
	m_num_partition_selectors_array(NULL),
	m_subplan_entries_list(subplan_entries_list),
	m_result_relation_index(0),
	m_into_clause(NULL),
	m_distribution_policy(NULL)
{
	m_cte_consumer_info = GPOS_NEW(m_mp) HMUlCTEConsumerInfo(m_mp);
	m_num_partition_selectors_array = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::~CContextDXLToPlStmt
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CContextDXLToPlStmt::~CContextDXLToPlStmt()
{
	m_cte_consumer_info->Release();
	m_num_partition_selectors_array->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetNextPlanId
//
//	@doc:
//		Get the next plan id
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetNextPlanId()
{
	return m_plan_id_counter->next_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetCurrentMotionId
//
//	@doc:
//		Get the current motion id
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetCurrentMotionId()
{
	return m_motion_id_counter->current_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetNextMotionId
//
//	@doc:
//		Get the next motion id
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetNextMotionId()
{
	return m_motion_id_counter->next_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetNextParamId
//
//	@doc:
//		Get the next plan id
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetNextParamId()
{
	return m_param_id_counter->next_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetCurrentParamId
//
//	@doc:
//		Get the current param id
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetCurrentParamId()
{
	return m_param_id_counter->current_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddCTEConsumerInfo
//
//	@doc:
//		Add information about the newly found CTE entry
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::AddCTEConsumerInfo
	(
	ULONG cte_id,
	ShareInputScan *share_input_scan
	)
{
	GPOS_ASSERT(NULL != share_input_scan);

	SCTEConsumerInfo *cte_info = m_cte_consumer_info->Find(&cte_id);
	if (NULL != cte_info)
	{
		cte_info->AddCTEPlan(share_input_scan);
		return;
	}

	List *cte_plan = ListMake1(share_input_scan);

	ULONG *key = GPOS_NEW(m_mp) ULONG(cte_id);
#ifdef GPOS_DEBUG
	BOOL result =
#endif
			m_cte_consumer_info->Insert(key, GPOS_NEW(m_mp) SCTEConsumerInfo(cte_plan));

	GPOS_ASSERT(result);
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetCTEConsumerList
//
//	@doc:
//		Return the list of GPDB plan nodes representing the CTE consumers
//		with the given CTE identifier
//---------------------------------------------------------------------------
List *
CContextDXLToPlStmt::GetCTEConsumerList
	(
	ULONG cte_id
	)
	const
{
	SCTEConsumerInfo *cte_info = m_cte_consumer_info->Find(&cte_id);
	if (NULL != cte_info)
	{
		return cte_info->m_cte_consumer_list;
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetRTableEntriesList
//
//	@doc:
//		Return the list of RangeTableEntries
//
//---------------------------------------------------------------------------
List *
CContextDXLToPlStmt::GetRTableEntriesList()
{
	return (*(m_rtable_entries_list));
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetSubplanEntriesList
//
//	@doc:
//		Return the list of subplans generated so far
//
//---------------------------------------------------------------------------
List *
CContextDXLToPlStmt::GetSubplanEntriesList()
{
	return (*(m_subplan_entries_list));
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddRTE
//
//	@doc:
//		Add a RangeTableEntries
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::AddRTE
	(
	RangeTblEntry *rte,
	BOOL is_result_relation
	)
{
	(* (m_rtable_entries_list)) = gpdb::LAppend((*(m_rtable_entries_list)), rte);

	rte->inFromCl = true;

	if (is_result_relation)
	{
		GPOS_ASSERT(0 == m_result_relation_index && "Only one result relation supported");
		rte->inFromCl = false;
		m_result_relation_index = gpdb::ListLength(*(m_rtable_entries_list));
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddPartitionedTable
//
//	@doc:
//		Add a partitioned table oid
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::AddPartitionedTable
	(
	OID oid
	)
{
	if (!gpdb::ListMemberOid(m_partitioned_tables_list, oid))
	{
		m_partitioned_tables_list = gpdb::LAppendOid(m_partitioned_tables_list, oid);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::IncrementPartitionSelectors
//
//	@doc:
//		Increment the number of partition selectors for the given scan id
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::IncrementPartitionSelectors
	(
	ULONG scan_id
	)
{
	// add extra elements to the array if necessary
	const ULONG len = m_num_partition_selectors_array->Size();
	for (ULONG ul = len; ul <= scan_id; ul++)
	{
		ULONG *pul = GPOS_NEW(m_mp) ULONG(0);
		m_num_partition_selectors_array->Append(pul);
	}

	ULONG *ul = (*m_num_partition_selectors_array)[scan_id];
	(*ul) ++;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetNumPartitionSelectorsList
//
//	@doc:
//		Return list containing number of partition selectors for every scan id
//
//---------------------------------------------------------------------------
List *
CContextDXLToPlStmt::GetNumPartitionSelectorsList() const
{
	List *partition_selectors_list = NIL;
	const ULONG len = m_num_partition_selectors_array->Size();
	for (ULONG ul = 0; ul < len; ul++)
	{
		ULONG *num_partition_selectors = (*m_num_partition_selectors_array)[ul];
		partition_selectors_list = gpdb::LAppendInt(partition_selectors_list, *num_partition_selectors);
	}

	return partition_selectors_list;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddSubplan
//
//	@doc:
//		Add a subplan
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::AddSubplan(Plan *plan)
{
	(* (m_subplan_entries_list)) = gpdb::LAppend((*(m_subplan_entries_list)), plan);
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddCtasInfo
//
//	@doc:
//		Add CTAS info
//
//---------------------------------------------------------------------------
void
CContextDXLToPlStmt::AddCtasInfo
	(
	IntoClause *into_clause,
	GpPolicy *distribution_policy
	)
{
	GPOS_ASSERT(NULL != into_clause);
	GPOS_ASSERT(NULL != distribution_policy);
	
	m_into_clause = into_clause;
	m_distribution_policy = distribution_policy;
}

// EOF
