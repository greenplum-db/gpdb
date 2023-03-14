//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPartPruneStepsBuilder.cpp
//
//	@doc:
//		Utility class to construct PartPruneInfos with appropriate
// 		PartPruningSteps from partitioning filter expressions
//---------------------------------------------------------------------------

extern "C" {
#include "postgres.h"

#include "nodes/parsenodes.h"
#include "partitioning/partdesc.h"
#include "utils/partcache.h"
#include "utils/rel.h"
}

#include "gpopt/gpdbwrappers.h"
#include "gpopt/translate/CPartPruneStepsBuilder.h"
#include "naucrates/dxl/operators/CDXLScalarBoolExpr.h"
#include "naucrates/dxl/operators/CDXLScalarCast.h"
#include "naucrates/dxl/operators/CDXLScalarComp.h"
#include "naucrates/exception.h"

using namespace gpdxl;

// ctor
CPartPruneStepsBuilder::CPartPruneStepsBuilder(
	Relation relation, Index rtindex, ULongPtrArray *part_indexes,
	CMappingColIdVarPlStmt *colid_var_mapping,
	CTranslatorDXLToScalar *translator_dxl_to_scalar)
	: m_relation(relation),
	  m_rtindex(rtindex),
	  m_part_indexes(part_indexes),
	  m_colid_var_mapping(colid_var_mapping),
	  m_translator_dxl_to_scalar(translator_dxl_to_scalar)
{
}

List *
CPartPruneStepsBuilder::CreatePartPruneInfos(
	CDXLNode *filterNode, Relation relation, Index rtindex,
	ULongPtrArray *part_indexes, CMappingColIdVarPlStmt *colid_var_mapping,
	CTranslatorDXLToScalar *translator_dxl_to_scalar)
{
	CPartPruneStepsBuilder builder(relation, rtindex, part_indexes,
								   colid_var_mapping, translator_dxl_to_scalar);

	// See comments over PartitionPruneInfo::prune_infos for more details.

	// ORCA only supports single-level partitioned tables for which only one
	// list of pruning steps is needed.
	// So, size of 2nd dimension of (prune_infos) = 1
	PartitionedRelPruneInfo *pinfo =
		builder.CreatePartPruneInfoForOneLevel(filterNode);
	List *prune_info_per_hierarchy = ListMake1(pinfo);

	// Since ORCA translates each DynamicTableScan to a different Append node,
	// there is always only one partition hierarchy per Append/ PartitionSelector
	// node. So, size of 1st dimension of (prune_infos) = 1
	return ListMake1(prune_info_per_hierarchy);
}

PartitionedRelPruneInfo *
CPartPruneStepsBuilder::CreatePartPruneInfoForOneLevel(CDXLNode *filterNode)
{
	PartitionedRelPruneInfo *pinfo = MakeNode(PartitionedRelPruneInfo);
	pinfo->rtindex = m_rtindex;
	pinfo->nparts = m_relation->rd_partdesc->nparts;

	pinfo->subpart_map = (int *) palloc(sizeof(int) * pinfo->nparts);
	pinfo->subplan_map = (int *) palloc(sizeof(int) * pinfo->nparts);
	pinfo->relid_map = (Oid *) palloc(sizeof(int) * pinfo->nparts);

	// m_part_indexes contains the indexes (into m_relation->rd_pardesc) of the
	// partitions that survived static partition pruning; iterate over this list
	// to populate pinfo->subplan_map, pinfo->relid_map & pinfo->present_parts
	ULONG part_ptr = 0;
	for (ULONG i = 0; (int) i < pinfo->nparts; ++i)
	{
		pinfo->subpart_map[i] = -1;
		if (part_ptr < m_part_indexes->Size() &&
			i == *(*m_part_indexes)[part_ptr])
		{
			// partition did survive pruning
			pinfo->subplan_map[i] = part_ptr;
			pinfo->relid_map[i] = m_relation->rd_partdesc->oids[i];
			pinfo->present_parts = bms_add_member(pinfo->present_parts, i);
			++part_ptr;
		}
		else
		{
			// partition did not survive pruning
			pinfo->subplan_map[i] = part_ptr;
			pinfo->subplan_map[i] = -1;
			pinfo->relid_map[i] = 0;
		}
	}

	INT step_id = 0;
	pinfo->exec_pruning_steps = PartPruneStepsFromFilter(
		filterNode, &step_id, pinfo->exec_pruning_steps);
	return pinfo;
}

List *
CPartPruneStepsBuilder::PartPruneStepFromScalarCmp(CDXLNode *node, int *step_id,
												   List *steps_list)
{
	GPOS_ASSERT(nullptr != node);
	CDXLScalarComp *dxlop = CDXLScalarComp::Cast(node->GetOperator());
	Oid opno = CMDIdGPDB::CastMdid(dxlop->MDId())->Oid();
	Oid opfamily = m_relation->rd_partkey->partopfamily[0 /* col */];

	StrategyNumber strategy_num;
	Oid righttype = InvalidOid;

	// extract the strategy (<, >, = etc) of the operator in the scalar cmp
	// and confirm that it's usable given the partition column's opfamily
	gpdb::IndexOpProperties(opno, opfamily, &strategy_num, &righttype);

	if (InvalidOid == righttype)
	{
		GPOS_RAISE(
			gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,
			GPOS_WSZ_LIT("Could not find op in partition table's opfamily"));
	}

	// CPredicateUtils::ValidatePartPruningExpr() ensures that the LHS contains
	// the partition column, and RHS contains the translatable expression
	Expr *expr = m_translator_dxl_to_scalar->TranslateDXLToScalar(
		(*node)[1], m_colid_var_mapping);

	PartitionPruneStepOp *step = MakeNode(PartitionPruneStepOp);
	step->step.step_id = (*step_id)++;
	step->opstrategy = strategy_num;

	// Use cmpfns from the partitioned table, since the op was confirmed
	// to be part of partitioning column opfamily above.
	// ORCA doesn't support multi-key (a.k.a composite) partition keys. So these
	// lists will be of size 1.
	step->cmpfns = ListMake1Oid(m_relation->rd_partkey->partsupfunc[0].fn_oid);
	step->exprs = ListMake1(expr);

	return gpdb::LAppend(steps_list, (PartitionPruneStep *) step);
}

List *
CPartPruneStepsBuilder::PartPruneStepFromScalarBoolExpr(CDXLNode *node,
														int *step_id,
														List *steps_list)
{
	GPOS_ASSERT(nullptr != node);
	CDXLScalarBoolExpr *dxlop = CDXLScalarBoolExpr::Cast(node->GetOperator());

	PartitionPruneCombineOp combineOp;
	switch (dxlop->GetDxlBoolTypeStr())
	{
		case Edxlnot:
		{
			GPOS_RAISE(
				gpdxl::ExmaDXL, gpdxl::ExmiDXL2PlStmtConversion,
				GPOS_WSZ_LIT("NOT expressions in DPE filter expr unsupported"));
			break;
		}
		case Edxland:
		{
			GPOS_ASSERT(2 <= node->Arity());
			combineOp = PARTPRUNE_COMBINE_INTERSECT;
			break;
		}
		case Edxlor:
		{
			GPOS_ASSERT(2 <= node->Arity());
			combineOp = PARTPRUNE_COMBINE_UNION;
			break;
		}
		default:
		{
			GPOS_RTL_ASSERT(!"Boolean Operation: Must be either or/ and / not");
		}
	}

	List *stepids = NIL;
	for (ULONG ul = 0; ul < node->Arity(); ul++)
	{
		CDXLNode *child_node = (*node)[ul];
		steps_list = PartPruneStepsFromFilter(child_node, step_id, steps_list);

		PartitionPruneStep *last_step =
			(PartitionPruneStep *) lfirst(gpdb::ListTail(steps_list));
		stepids = gpdb::LAppendInt(stepids, last_step->step_id);
	}

	PartitionPruneStepCombine *step = MakeNode(PartitionPruneStepCombine);
	step->step.step_id = (*step_id)++;
	step->source_stepids = stepids;
	step->combineOp = combineOp;

	return gpdb::LAppend(steps_list, (PartitionPruneStep *) step);
}

List *
CPartPruneStepsBuilder::PartPruneStepsFromFilter(CDXLNode *node, INT *step_id,
												 List *steps_list)
{
	GPOS_ASSERT(nullptr != node);
	Edxlopid eopid = node->GetOperator()->GetDXLOperator();

	switch (eopid)
	{
		case EdxlopScalarCmp:
		{
			steps_list = PartPruneStepFromScalarCmp(node, step_id, steps_list);
			break;
		}
		case EdxlopScalarBoolExpr:
		{
			steps_list =
				PartPruneStepFromScalarBoolExpr(node, step_id, steps_list);
			break;
		}
		default:
			GPOS_RTL_ASSERT(
				!"Unsupported operator in PartPruneStepsFromFilter");
			break;
	}
	return steps_list;
}
// EOF
