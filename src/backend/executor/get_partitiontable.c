
#include "postgres.h"

#include "parser/parsetree.h"
#include "cdb/cdbpartition.h"


/*get the partition table count*/
static void
GetMemberNodePartition(List *plans, PlanState **planstates,List*rtable,int*partition);
static void GetNodePartition(Plan *plan, Index rti,List*rtable,int* partition);
static void GetScanNodePartition(Scan *plan, List*rtable,int* partition);
static void GetModifyNodePartition(ModifyTable *plan, List*rtable,int* partition);
static void GetSubplanPartitionCnt(List *plans,List*rtable, int*partition);
void GetPlanPartitionCnt(PlanState *planstate,List*rtable,int *partition);

void GetPlanPartitionCnt(PlanState *planstate,List*rtable,int *partition)
{
	Plan	   *plan = planstate->plan;
	bool		haschildren;


	switch (nodeTag(plan))
	{
		/*scan node*/
		case T_SeqScan:
		case T_DynamicSeqScan:
		case T_ExternalScan:
		case T_DynamicIndexScan:
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_TableFunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
		case T_ForeignScan:
			GetScanNodePartition((Scan*) plan,rtable,partition);	
			break;
		case T_ModifyTable:
			GetModifyNodePartition((ModifyTable *) plan, rtable,partition);
			break;
		case T_PartitionSelector:
			{
				PartitionSelector *ps = (PartitionSelector*)plan;
				if (ps->staticSelection)
				{
				   *partition = list_length(ps->staticPartOids);
				}
				break;
			}
		default:
			break;
	}

	/* Get ready to display the child plans */
	haschildren = planstate->initPlan ||
		outerPlanState(planstate) ||
		innerPlanState(planstate) ||
		IsA(plan, ModifyTable) ||
		IsA(plan, Append) ||
		IsA(plan, MergeAppend) ||
		IsA(plan, Sequence) ||
		IsA(plan, BitmapAnd) ||
		IsA(plan, BitmapOr) ||
		IsA(plan, SubqueryScan) ||
		planstate->subPlan;
	/* initPlan-s */
	if (plan->initPlan)
		GetSubplanPartitionCnt(planstate->initPlan, rtable, partition);
	/* lefttree */
	if (outerPlan(plan))
	{
		GetPlanPartitionCnt(outerPlanState(planstate), rtable,partition);
	}

	/* righttree */
	if (innerPlanState(planstate))
		GetPlanPartitionCnt(innerPlanState(planstate), rtable,partition);

	/* special child plans */
	switch (nodeTag(plan))
	{
		case T_ModifyTable:
			GetMemberNodePartition(((ModifyTable *) plan)->plans,
							   ((ModifyTableState *) planstate)->mt_plans,
							   rtable,partition);
			break;
		case T_Append:
			GetMemberNodePartition(((Append *) plan)->appendplans,
							   ((AppendState *) planstate)->appendplans,
							    rtable,partition);
			break;
		case T_MergeAppend:
			GetMemberNodePartition(((MergeAppend *) plan)->mergeplans,
							   ((MergeAppendState *) planstate)->mergeplans,
							   rtable,partition);
			
			break;
		case T_Sequence:
			GetMemberNodePartition(((Sequence *) plan)->subplans,
							   ((SequenceState *) planstate)->subplans,
							   rtable,partition);
			break;
		case T_BitmapAnd:
			GetMemberNodePartition(((BitmapAnd *) plan)->bitmapplans,
							   ((BitmapAndState *) planstate)->bitmapplans,
							   rtable,partition);
			break;
		case T_BitmapOr:
			GetMemberNodePartition(((BitmapOr *) plan)->bitmapplans,
							   ((BitmapOrState *) planstate)->bitmapplans,
							   rtable,partition);
			break;
		case T_SubqueryScan:
			GetPlanPartitionCnt(((SubqueryScanState *) planstate)->subplan, rtable,partition);
			break;
		default:
			break;
	}
	/* subPlan-s */
	if (planstate->subPlan)
		GetSubplanPartitionCnt(planstate->initPlan, rtable, partition);



}

static void GetSubplanPartitionCnt(List *plans,List*rtable, int*partition)
{
	ListCell   *lst;

	foreach(lst, plans)
	{
		SubPlanState *sps = (SubPlanState *) lfirst(lst);
		GetPlanPartitionCnt(sps->planstate,rtable,partition);
	}



}
static void
GetMemberNodePartition(List *plans, PlanState **planstates, List*rtable,int*partition)
{
	int nplans = list_length(plans);
	int j;

	for (j = 0; j < nplans; j++)
		GetPlanPartitionCnt(planstates[j], rtable,partition);
}

static void
GetNodePartition(Plan *plan, Index rti,List*rtable,int* partition)
{
	RangeTblEntry *rte;
	rte = rt_fetch(rti,rtable);
	/*is a partition table?*/
	if (rel_is_child_partition(rte->relid))
		(*partition)++;
}

static void 
GetScanNodePartition(Scan *plan, List*rtable,int* partition)
{
	GetNodePartition((Plan*)plan,plan->scanrelid,rtable,partition);
}

static void
GetModifyNodePartition(ModifyTable *plan, List*rtable,int* partition)
{
	Index		rti;

	Assert(plan->resultRelations != NIL);
	rti = linitial_int(plan->resultRelations);
	
	GetNodePartition((Plan *) plan, rti, rtable ,partition);
}

