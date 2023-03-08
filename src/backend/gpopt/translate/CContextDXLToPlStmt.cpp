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

extern "C" {
#include "postgres.h"

#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "utils/rel.h"
}

#include "gpos/base.h"

#include "gpopt/gpdbwrappers.h"
#include "gpopt/translate/CContextDXLToPlStmt.h"
#include "naucrates/exception.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::CContextDXLToPlStmt
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CContextDXLToPlStmt::CContextDXLToPlStmt(
	CMemoryPool *mp, CIdGenerator *plan_id_counter,
	CIdGenerator *motion_id_counter, CIdGenerator *param_id_counter,
	DistributionHashOpsKind distribution_hashops)
	: m_mp(mp),
	  m_plan_id_counter(plan_id_counter),
	  m_motion_id_counter(motion_id_counter),
	  m_param_id_counter(param_id_counter),
	  m_param_types_list(NIL),
	  m_distribution_hashops(distribution_hashops),
	  m_rtable_entries_list(nullptr),
	  m_subplan_entries_list(nullptr),
	  m_subplan_sliceids_list(nullptr),
	  m_slices_list(nullptr),
	  m_result_relation_index(0),
	  m_into_clause(nullptr),
	  m_distribution_policy(nullptr),
	  m_part_selector_to_param_map(nullptr)
{
	m_cte_consumer_info = GPOS_NEW(m_mp) HMUlCTEConsumerInfo(m_mp);
	m_part_selector_to_param_map = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);
	m_used_rte_indexes = GPOS_NEW(m_mp) HMUlIndex(m_mp);
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
	m_part_selector_to_param_map->Release();
	m_used_rte_indexes->Release();
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
//		Get the next param id, for a parameter of type 'typeoid'
//
//---------------------------------------------------------------------------
ULONG
CContextDXLToPlStmt::GetNextParamId(OID typeoid)
{
	m_param_types_list = gpdb::LAppendOid(m_param_types_list, typeoid);

	return m_param_id_counter->next_id();
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetParamTypes
//
//	@doc:
//		Get the current param types list
//
//---------------------------------------------------------------------------
List *
CContextDXLToPlStmt::GetParamTypes()
{
	return m_param_types_list;
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
CContextDXLToPlStmt::AddCTEConsumerInfo(ULONG cte_id,
										ShareInputScan *share_input_scan)
{
	GPOS_ASSERT(nullptr != share_input_scan);

	SCTEConsumerInfo *cte_info = m_cte_consumer_info->Find(&cte_id);
	if (nullptr != cte_info)
	{
		cte_info->AddCTEPlan(share_input_scan);
		return;
	}

	List *cte_plan = ListMake1(share_input_scan);

	ULONG *key = GPOS_NEW(m_mp) ULONG(cte_id);
	BOOL result GPOS_ASSERTS_ONLY = m_cte_consumer_info->Insert(
		key, GPOS_NEW(m_mp) SCTEConsumerInfo(cte_plan));

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
CContextDXLToPlStmt::GetCTEConsumerList(ULONG cte_id) const
{
	SCTEConsumerInfo *cte_info = m_cte_consumer_info->Find(&cte_id);
	if (nullptr != cte_info)
	{
		return cte_info->m_cte_consumer_list;
	}

	return nullptr;
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
CContextDXLToPlStmt::AddRTE(RangeTblEntry *rte, BOOL is_result_relation)
{
	m_rtable_entries_list = gpdb::LAppend(m_rtable_entries_list, rte);

	rte->inFromCl = true;

	if (is_result_relation)
	{
		GPOS_ASSERT(0 == m_result_relation_index &&
					"Only one result relation supported");
		rte->inFromCl = false;
		m_result_relation_index = gpdb::ListLength(m_rtable_entries_list);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetSubplanSliceIdArray
//
//	@doc:
//		Get the slice IDs of each subplan as an array.
//
//---------------------------------------------------------------------------
int *
CContextDXLToPlStmt::GetSubplanSliceIdArray()
{
	int numSubplans = list_length(m_subplan_entries_list);
	int *sliceIdArray;
	ListCell *lc;
	int i;

	sliceIdArray = (int *) gpdb::GPDBAlloc(numSubplans * sizeof(int));

	i = 0;
	foreach (lc, m_subplan_sliceids_list)
	{
		sliceIdArray[i++] = lfirst_int(lc);
	}

	return sliceIdArray;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetSlices
//
//	@doc:
//		Get the slice table as an array
//
//---------------------------------------------------------------------------
PlanSlice *
CContextDXLToPlStmt::GetSlices(int *numSlices_p)
{
	int numSlices = list_length(m_slices_list);
	PlanSlice *sliceArray;
	ListCell *lc;
	int i;

	sliceArray = (PlanSlice *) gpdb::GPDBAlloc(numSlices * sizeof(PlanSlice));

	i = 0;
	foreach (lc, m_slices_list)
	{
		PlanSlice *src = (PlanSlice *) lfirst(lc);

		memcpy(&sliceArray[i], src, sizeof(PlanSlice));

		i++;
	}

	m_current_slice = nullptr;
	gpdb::ListFreeDeep(m_slices_list);

	*numSlices_p = numSlices;
	return sliceArray;
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
	m_subplan_entries_list = gpdb::LAppend(m_subplan_entries_list, plan);
	m_subplan_sliceids_list =
		gpdb::LAppendInt(m_subplan_sliceids_list, m_current_slice->sliceIndex);
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddSlice
//
//	@doc:
//		Add a plan slice
//
//---------------------------------------------------------------------------
int
CContextDXLToPlStmt::AddSlice(PlanSlice *slice)
{
	slice->sliceIndex = list_length(m_slices_list);
	m_slices_list = gpdb::LAppend(m_slices_list, slice);

	return slice->sliceIndex;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::AddCtasInfo
//
//	@doc:
//		Add CTAS info
//
//---------------------------------------------------------------------------
// GPDB_92_MERGE_FIXME: we really should care about intoClause
// But planner cheats. FIX that and re-enable ORCA's handling of intoClause
void
CContextDXLToPlStmt::AddCtasInfo(IntoClause *into_clause,
								 GpPolicy *distribution_policy)
{
	//	GPOS_ASSERT(NULL != into_clause);
	GPOS_ASSERT(nullptr != distribution_policy);

	m_into_clause = into_clause;
	m_distribution_policy = distribution_policy;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetDistributionHashOpclassForType
//
//	@doc:
//		Return a hash operator class to use for computing
//		distribution key values for the given datatype.
//
//		This returns either the default opclass, or the legacy
//		opclass of the type, depending on what opclasses we have
//		seen being used in tables so far.
//---------------------------------------------------------------------------
Oid
CContextDXLToPlStmt::GetDistributionHashOpclassForType(Oid typid)
{
	Oid opclass = InvalidOid;

	switch (m_distribution_hashops)
	{
		case DistrUseDefaultHashOps:
			opclass = gpdb::GetDefaultDistributionOpclassForType(typid);
			break;

		case DistrUseLegacyHashOps:
			opclass = gpdb::GetLegacyCdbHashOpclassForBaseType(typid);
			break;

		case DistrHashOpsNotDeterminedYet:
			// None of the tables we have seen so far have been
			// hash distributed, so we haven't made up our mind
			// on which opclasses to use yet. But we have to
			// pick something now.
			//
			// FIXME: It's quite unoptimal that this ever happens.
			// To avoid this we should make a pass over the tree to
			// determine the opclasses, before translating
			// anything. But there is no convenient way to "walk"
			// the DXL representation AFAIK.
			//
			// Example query where this happens:
			// select * from dd_singlecol_1 t1,
			//               generate_series(1,10) g
			// where t1.a=g.g and t1.a=1 ;
			//
			// The ORCA plan consists of a join between
			// Result+FunctionScan and TableScan. The
			// Result+FunctionScan side is processed first, and
			// this gets called to generate a "hash filter" for
			// it Result. The TableScan is encountered and
			// added to the range table only later. If it uses
			// legacy ops, we have already decided to use default
			// ops here, and we fall back unnecessarily.
			//
			// On the other hand, when the opclass is not specified in the
			// distributed-by clause one should be decided according to the
			// gp_use_legacy_hashops setting.
			opclass = gpdb::GetColumnDefOpclassForType(NIL, typid);
			// update m_distribution_hashops accordingly
			if (opclass == gpdb::GetDefaultDistributionOpclassForType(typid))
			{
				m_distribution_hashops = DistrUseDefaultHashOps;
			}
			else if (opclass == gpdb::GetLegacyCdbHashOpclassForBaseType(typid))
			{
				m_distribution_hashops = DistrUseLegacyHashOps;
			}
			else
			{
				GPOS_RAISE(
					gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
					GPOS_WSZ_LIT("Unsupported distribution hashops policy"));
			}
			break;
	}

	return opclass;
}

//---------------------------------------------------------------------------
//	@function:
//		CContextDXLToPlStmt::GetDistributionHashFuncForType
//
//	@doc:
//		Return a hash function to use for computing distribution key
//		values for the given datatype.
//
//		This returns the hash function either from the default
//		opclass, or the legacy opclass of the type, depending on
//		what opclasses we have seen being used in tables so far.
//---------------------------------------------------------------------------
Oid
CContextDXLToPlStmt::GetDistributionHashFuncForType(Oid typid)
{
	Oid opclass;
	Oid opfamily;
	Oid hashproc;

	opclass = GetDistributionHashOpclassForType(typid);

	if (opclass == InvalidOid)
	{
		GPOS_RAISE(gpdxl::ExmaMD, gpdxl::ExmiMDObjUnsupported,
				   GPOS_WSZ_LIT("no default hash opclasses found"));
	}

	opfamily = gpdb::GetOpclassFamily(opclass);
	hashproc = gpdb::GetHashProcInOpfamily(opfamily, typid);

	return hashproc;
}

ULONG
CContextDXLToPlStmt::GetParamIdForSelector(OID oid_type, ULONG selectorId)
{
	ULONG *param_id = m_part_selector_to_param_map->Find(&selectorId);
	if (nullptr == param_id)
	{
		param_id = GPOS_NEW(m_mp) ULONG(GetNextParamId(oid_type));
		ULONG *selector_id = GPOS_NEW(m_mp) ULONG(selectorId);
		m_part_selector_to_param_map->Insert(selector_id, param_id);
	}
	return *param_id;
}

Index
CContextDXLToPlStmt::FindRTE(Oid reloid)
{
	ListCell *lc;
	int idx = 0;

	ForEachWithCount(lc, m_rtable_entries_list, idx)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
		if (rte->relid == reloid)
		{
			return idx + 1;
		}
	}
	return -1;
}

RangeTblEntry *
CContextDXLToPlStmt::GetRTEByIndex(Index index)
{
	return (RangeTblEntry *) gpdb::ListNth(m_rtable_entries_list,
										   int(index - 1));
}

//---------------------------------------------------------------------------
//	@function: of associated
//		CContextDXLToPlStmt::GetRTEIndexByTableDescr
//
//	@doc:
//
//		For given table descriptor this function returns index of rte in
//		m_rtable_entries_list for furhter processing and set a flag that
//		rte was processed.
//		In case of DML operations there is more than one table descr pointing
//		to the result relation and to detect position of already processed rte
//		`assigned_query_id_for_target_rel` of table descriptor is used.
//---------------------------------------------------------------------------
Index
CContextDXLToPlStmt::GetRTEIndexByTableDescr(const CDXLTableDescr *table_descr,
											 BOOL *is_rte_exists)
{
	*is_rte_exists = false;

	//	`assigned_query_id_for_target_rel` is a "tag" of table descriptors, it
	//	shows id of query structure which contains result relation. If table
	//	descriptors have the same `assigned_query_id_for_target_rel` - these
	//	table descriptors point to the same result relation in `ModifyTable`
	//	operation. It's not zero (0) value (which equal to `UNASSIGNED_QUERYID`
	//	define) if: user query is a INSERT/UPDATE/DELETE (`ModifyTable`
	//	operation) and this table descriptor points to the result relation of
	//	operation, for ex.:
	//	```sql
	//	create table b (i int, j int);
	//	create table c (i int);
	//	insert into b(i,j) values (1,2), (2,3), (3,4);
	//	insert into c(i) values (1), (2);
	//	delete from b where i in (select i from c);
	//	```
	//	where `b` is a result relation (table descriptors pointing to it
	//	will have the same `assigned_query_id_for_target_rel` > 0), and
	//	`c` is not (all table descriptors which points to `c` will have
	//	`assigned_query_id_for_target_rel`=0 (equal to `UNASSIGNED_QUERYID`)
	ULONG assigned_query_id_for_target_rel =
		table_descr->GetAssignedQueryIdForTargetRel();
	if (assigned_query_id_for_target_rel == UNASSIGNED_QUERYID)
	{
		return gpdb::ListLength(m_rtable_entries_list) + 1;
	}

	Index *usedIndex =
		m_used_rte_indexes->Find(&assigned_query_id_for_target_rel);

	//	`usedIndex` is a non NULL value in next case: table descriptor with
	//	the same `assigned_query_id_for_target_rel` was processed previously
	//	(so no need to create a new index for result relation like the relation
	//	itself)
	if (usedIndex)
	{
		*is_rte_exists = true;
		return *usedIndex;
	}

	//	`assigned_query_id_for_target_rel` of table descriptor which points to
	//	result relation wasn't previously processed - create a new index.
	Index new_index = gpdb::ListLength(m_rtable_entries_list) + 1;
	m_used_rte_indexes->Insert(GPOS_NEW(m_mp)
								   ULONG(assigned_query_id_for_target_rel),
							   GPOS_NEW(m_mp) Index(new_index));

	return new_index;
}

// EOF
