/*-------------------------------------------------------------------------
 *
 * relnode.c
 *	  Relation-node lookup/construction routines
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/relnode.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "miscadmin.h"
#include "catalog/pg_class_d.h"
#include "catalog/pg_constraint.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/inherit.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/planner.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "partitioning/partbounds.h"
#include "utils/hsearch.h"
#include "parser/parse_oper.h"
#include "utils/hsearch.h"
#include "utils/selfuncs.h"
#include "utils/lsyscache.h"

#include "access/sysattr.h"
#include "cdb/cdbutil.h"

/*
 * An entry of a hash table that we use to make lookup for RelOptInfo
 * structures more efficient.
 */
typedef struct RelInfoEntry
{
	Relids		relids;			/* hash key --- MUST BE FIRST */
	void	   *data;
} RelInfoEntry;

static void build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel,
					RelOptInfo *input_rel);
static List *build_joinrel_restrictlist(PlannerInfo *root,
										RelOptInfo *joinrel,
										RelOptInfo *outer_rel,
										RelOptInfo *inner_rel);
static void build_joinrel_joinlist(RelOptInfo *joinrel,
								   RelOptInfo *outer_rel,
								   RelOptInfo *inner_rel);
static List *subbuild_joinrel_restrictlist(RelOptInfo *joinrel,
										   List *joininfo_list,
										   List *new_restrictlist);
static List *subbuild_joinrel_joinlist(RelOptInfo *joinrel,
									   List *joininfo_list,
									   List *new_joininfo);
static void set_foreign_rel_properties(RelOptInfo *joinrel,
									   RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static void add_join_rel(PlannerInfo *root, RelOptInfo *joinrel);
static void build_joinrel_partition_info(RelOptInfo *joinrel,
										 RelOptInfo *outer_rel, RelOptInfo *inner_rel,
										 List *restrictlist, JoinType jointype);
static void build_child_join_reltarget(PlannerInfo *root,
									   RelOptInfo *parentrel,
									   RelOptInfo *childrel,
									   int nappinfos,
									   AppendRelInfo **appinfos);
static bool init_grouping_targets(PlannerInfo *root, RelOptInfo *rel,
								  PathTarget *target, PathTarget *agg_input,
								  List *gvis, List **group_exprs_extra_p);
static bool is_var_in_aggref_only(PlannerInfo *root, Var *var);
static bool is_var_needed_by_join(PlannerInfo *root, Var *var, RelOptInfo *rel);


/*
 * setup_simple_rel_arrays
 *	  Prepare the arrays we use for quickly accessing base relations.
 */
void
setup_simple_rel_arrays(PlannerInfo *root)
{
	Index		rti;
	ListCell   *lc;

	/* Arrays are accessed using RT indexes (1..N) */
	root->simple_rel_array_size = list_length(root->parse->rtable) + 1;

	/* simple_rel_array is initialized to all NULLs */
	root->simple_rel_array = (RelOptInfo **)
		palloc0(root->simple_rel_array_size * sizeof(RelOptInfo *));

	/* simple_rte_array is an array equivalent of the rtable list */
	root->simple_rte_array = (RangeTblEntry **)
		palloc0(root->simple_rel_array_size * sizeof(RangeTblEntry *));
	rti = 1;
	foreach(lc, root->parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);

		root->simple_rte_array[rti++] = rte;
	}
}

/*
 * setup_append_rel_array
 *		Populate the append_rel_array to allow direct lookups of
 *		AppendRelInfos by child relid.
 *
 * The array remains unallocated if there are no AppendRelInfos.
 */
void
setup_append_rel_array(PlannerInfo *root)
{
	ListCell   *lc;
	int			size = list_length(root->parse->rtable) + 1;

	if (root->append_rel_list == NIL)
	{
		root->append_rel_array = NULL;
		return;
	}

	root->append_rel_array = (AppendRelInfo **)
		palloc0(size * sizeof(AppendRelInfo *));

	foreach(lc, root->append_rel_list)
	{
		AppendRelInfo *appinfo = lfirst_node(AppendRelInfo, lc);
		int			child_relid = appinfo->child_relid;

		/* Sanity check */
		Assert(child_relid < size);

		if (root->append_rel_array[child_relid])
			elog(ERROR, "child relation already exists");

		root->append_rel_array[child_relid] = appinfo;
	}
}

/*
 * expand_planner_arrays
 *		Expand the PlannerInfo's per-RTE arrays by add_size members
 *		and initialize the newly added entries to NULLs
 */
void
expand_planner_arrays(PlannerInfo *root, int add_size)
{
	int			new_size;

	Assert(add_size > 0);

	new_size = root->simple_rel_array_size + add_size;

	root->simple_rte_array = (RangeTblEntry **)
		repalloc(root->simple_rte_array,
				 sizeof(RangeTblEntry *) * new_size);
	MemSet(root->simple_rte_array + root->simple_rel_array_size,
		   0, sizeof(RangeTblEntry *) * add_size);

	root->simple_rel_array = (RelOptInfo **)
		repalloc(root->simple_rel_array,
				 sizeof(RelOptInfo *) * new_size);
	MemSet(root->simple_rel_array + root->simple_rel_array_size,
		   0, sizeof(RelOptInfo *) * add_size);

	if (root->append_rel_array)
	{
		root->append_rel_array = (AppendRelInfo **)
			repalloc(root->append_rel_array,
					 sizeof(AppendRelInfo *) * new_size);
		MemSet(root->append_rel_array + root->simple_rel_array_size,
			   0, sizeof(AppendRelInfo *) * add_size);
	}
	else
	{
		root->append_rel_array = (AppendRelInfo **)
			palloc0(sizeof(AppendRelInfo *) * new_size);
	}

	root->simple_rel_array_size = new_size;
}

/*
 * build_simple_rel
 *	  Construct a new RelOptInfo for a base relation or 'other' relation.
 */
RelOptInfo *
build_simple_rel(PlannerInfo *root, int relid, RelOptInfo *parent)
{
	RelOptInfo *rel;
	RangeTblEntry *rte;

	/* Rel should not exist already */
	Assert(relid > 0 && relid < root->simple_rel_array_size);
	if (root->simple_rel_array[relid] != NULL)
		elog(ERROR, "rel %d already exists", relid);

	/* Fetch RTE for relation */
	rte = root->simple_rte_array[relid];
	Assert(rte != NULL);

	rel = makeNode(RelOptInfo);
	rel->reloptkind = parent ? RELOPT_OTHER_MEMBER_REL : RELOPT_BASEREL;
	rel->relids = bms_make_singleton(relid);
	rel->rows = 0;
	/* cheap startup cost is interesting iff not all tuples to be retrieved */
	rel->consider_startup = (root->tuple_fraction > 0);
	rel->consider_param_startup = false;	/* might get changed later */
	rel->consider_parallel = false; /* might get changed later */
	rel->reltarget = create_empty_pathtarget();
	rel->pathlist = NIL;
	rel->ppilist = NIL;
	rel->partial_pathlist = NIL;
	rel->cheapest_startup_path = NULL;
	rel->cheapest_total_path = NULL;
	rel->cheapest_unique_path = NULL;
	rel->cheapest_parameterized_paths = NIL;
	rel->relid = relid;
	rel->rtekind = rte->rtekind;
	/* min_attr, max_attr, attr_needed, attr_widths are set below */
	rel->lateral_vars = NIL;
	rel->indexlist = NIL;
	rel->statlist = NIL;
	rel->pages = 0;
	rel->tuples = 0;
	rel->allvisfrac = 0;
	rel->subroot = NULL;
	rel->subplan_params = NIL;
	rel->rel_parallel_workers = -1; /* set up in get_relation_info */
	rel->serverid = InvalidOid;
	rel->userid = rte->checkAsUser;
	rel->useridiscurrent = false;
	rel->exec_location = FTEXECLOCATION_NOT_DEFINED;
	rel->fdwroutine = NULL;
	rel->fdw_private = NULL;
	rel->unique_for_rels = NIL;
	rel->non_unique_for_rels = NIL;
	rel->baserestrictinfo = NIL;
	rel->baserestrictcost.startup = 0;
	rel->baserestrictcost.per_tuple = 0;
	rel->baserestrict_min_security = UINT_MAX;
	rel->joininfo = NIL;
	rel->has_eclass_joins = false;
	rel->consider_partitionwise_join = false;	/* might get changed later */
	rel->part_scheme = NULL;
	rel->nparts = 0;
	rel->boundinfo = NULL;
	rel->partition_qual = NIL;
	rel->part_rels = NULL;
	rel->partexprs = NULL;
	rel->nullable_partexprs = NULL;
	rel->partitioned_child_rels = NIL;

	/*
	 * Pass assorted information down the inheritance hierarchy.
	 */
	if (parent)
	{
		/*
		 * Each direct or indirect child wants to know the relids of its
		 * topmost parent.
		 */
		if (parent->top_parent_relids)
			rel->top_parent_relids = parent->top_parent_relids;
		else
			rel->top_parent_relids = bms_copy(parent->relids);

		/*
		 * Also propagate lateral-reference information from appendrel parent
		 * rels to their child rels.  We intentionally give each child rel the
		 * same minimum parameterization, even though it's quite possible that
		 * some don't reference all the lateral rels.  This is because any
		 * append path for the parent will have to have the same
		 * parameterization for every child anyway, and there's no value in
		 * forcing extra reparameterize_path() calls.  Similarly, a lateral
		 * reference to the parent prevents use of otherwise-movable join rels
		 * for each child.
		 *
		 * It's possible for child rels to have their own children, in which
		 * case the topmost parent's lateral info propagates all the way down.
		 */
		rel->direct_lateral_relids = parent->direct_lateral_relids;
		rel->lateral_relids = parent->lateral_relids;
		rel->lateral_referencers = parent->lateral_referencers;
	}
	else
	{
		rel->top_parent_relids = NULL;
		rel->direct_lateral_relids = NULL;
		rel->lateral_relids = NULL;
		rel->lateral_referencers = NULL;
	}

	/* Check type of rtable entry */
	switch (rte->rtekind)
	{
		case RTE_RELATION:
			/* Table --- retrieve statistics from the system catalogs */

			get_relation_info(root, rte->relid, rte->inh, rel);

			/* if we've been asked to, force the dist-policy to be partitioned-randomly. */
			if (rte->forceDistRandom)
			{
				GpPolicy   *origpolicy = GpPolicyFetch(rte->relid);
				int			numsegments;

				if (origpolicy->ptype != POLICYTYPE_ENTRY)
					numsegments = origpolicy->numsegments;
				else
					numsegments = getgpsegmentCount();

				rel->cdbpolicy = createRandomPartitionedPolicy(numsegments);
			}

			if ((root->parse->commandType == CMD_UPDATE ||
				 root->parse->commandType == CMD_DELETE) &&
				root->parse->resultRelation == relid &&
				GpPolicyIsReplicated(rel->cdbpolicy))
			{
				root->upd_del_replicated_table = relid;
			}
			break;
		case RTE_SUBQUERY:
		case RTE_FUNCTION:
		case RTE_TABLEFUNCTION:
		case RTE_TABLEFUNC:
		case RTE_VALUES:
		case RTE_CTE:
		case RTE_NAMEDTUPLESTORE:

			/*
			 * Subquery, function, tablefunc, values list, CTE, or ENR --- set
			 * up attr range and arrays
			 *
			 * Note: 0 is included in range to support whole-row Vars
			 */
            /* CDB: Allow internal use of sysattrs (<0) for subquery dedup. */
        	rel->min_attr = FirstLowInvalidHeapAttributeNumber + 1;     /*CDB*/
			rel->max_attr = list_length(rte->eref->colnames);
			rel->attr_needed = (Relids *)
				palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(Relids));
			rel->attr_widths = (int32 *)
				palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(int32));
			break;
		case RTE_RESULT:
			/* RTE_RESULT has no columns, nor could it have whole-row Var */
			rel->min_attr = 0;
			rel->max_attr = -1;
			rel->attr_needed = NULL;
			rel->attr_widths = NULL;
			break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d",
				 (int) rte->rtekind);
			break;
	}

	/*
	 * Copy the parent's quals to the child, with appropriate substitution of
	 * variables.  If any constant false or NULL clauses turn up, we can mark
	 * the child as dummy right away.  (We must do this immediately so that
	 * pruning works correctly when recursing in expand_partitioned_rtentry.)
	 */
	if (parent)
	{
		AppendRelInfo *appinfo = root->append_rel_array[relid];

		Assert(appinfo != NULL);
		if (!apply_child_basequals(root, parent, rel, rte, appinfo))
		{
			/*
			 * Some restriction clause reduced to constant FALSE or NULL after
			 * substitution, so this child need not be scanned.
			 */
			mark_dummy_rel(root, rel);
		}
	}

	/* Save the finished struct in the query's simple_rel_array */
	root->simple_rel_array[relid] = rel;

	return rel;
}

/*
 * build_simple_grouped_rel
 *	  Construct a new RelOptInfo for a grouped base relation out of an
 *	  existing non-grouped relation. On success, pointer to the corresponding
 *	  RelAggInfo is stored in *agg_info_p in addition to returning the grouped
 *	  relation.
 */
RelOptInfo *
build_simple_grouped_rel(PlannerInfo *root, int relid,
						 RelAggInfo **agg_info_p)
{
	RangeTblEntry *rte;
	RelOptInfo *rel_plain,
			   *rel_grouped;
	RelAggInfo *agg_info;

	/* Isn't there any grouping expression to be pushed down? */
	if (root->grouped_var_list == NIL)
		return NULL;

	rel_plain = root->simple_rel_array[relid];

	/* Caller should only pass rti that represents base relation. */
	Assert(rel_plain != NULL);

	/*
	 * Not all RTE kinds are supported when grouping is considered.
	 *
	 * TODO Consider relaxing some of these restrictions.
	 */
	rte = root->simple_rte_array[rel_plain->relid];
	if (rte->rtekind != RTE_RELATION ||
		rte->relkind == RELKIND_FOREIGN_TABLE ||
		rte->tablesample != NULL)
		return NULL;

	/*
	 * Grouped append relation is not supported yet.
	 */
	if (rte->inh)
		return NULL;

	/*
	 * Currently we do not support child relations ("other rels").
	 */
	if (rel_plain->reloptkind != RELOPT_BASEREL)
		return NULL;

	/*
	 * Prepare the information we need for aggregation of the rel contents.
	 */
	agg_info = create_rel_agg_info(root, rel_plain);
	if (agg_info == NULL)
		return NULL;

	/*
	 * TODO Consider if 1) a flat copy is o.k., 2) it's safer in terms of
	 * adding new fields to RelOptInfo) to copy everything and then reset some
	 * fields, or to zero the structure and copy individual fields.
	 */
	rel_grouped = makeNode(RelOptInfo);
	memcpy(rel_grouped, rel_plain, sizeof(RelOptInfo));

	/*
	 * Note on consider_startup: while the AGG_HASHED strategy needs the whole
	 * relation, AGG_SORTED does not. Therefore we do not force
	 * consider_startup to false.
	 */

	/*
	 * Set the appropriate target for grouped paths.
	 *
	 * reltarget should match the target of partially aggregated paths.
	 */
	rel_grouped->reltarget = agg_info->target;

	/*
	 * Grouped paths must not be mixed with the plain ones.
	 */
	rel_grouped->pathlist = NIL;
	rel_grouped->partial_pathlist = NIL;
	rel_grouped->cheapest_startup_path = NULL;
	rel_grouped->cheapest_total_path = NULL;
	rel_grouped->cheapest_unique_path = NULL;
	rel_grouped->cheapest_parameterized_paths = NIL;

	/*
	 * The number of aggregation input rows is simply the number of rows of
	 * the non-grouped relation, which should have been estimated by now.
	 */
	agg_info->input_rows = rel_plain->rows;

	/*
	 * The number of output rows is supposedly different (lower) due to
	 * grouping.
	 */
	rel_grouped->rows = estimate_num_groups(root, agg_info->group_exprs,
											agg_info->input_rows, NULL);

	*agg_info_p = agg_info;
	return rel_grouped;
}

/*
 * find_base_rel
 *	  Find a base or other relation entry, which must already exist.
 */
RelOptInfo *
find_base_rel(PlannerInfo *root, int relid)
{
	RelOptInfo *rel;

	Assert(relid > 0);

	if (relid < root->simple_rel_array_size)
	{
		rel = root->simple_rel_array[relid];
		if (rel)
			return rel;
	}

	elog(ERROR, "no relation entry for relid %d", relid);

	return NULL;				/* keep compiler quiet */
}

/*
 * build_rel_hash
 *	  Construct the auxiliary hash table for relation specific data.
 */
static void
build_rel_hash(RelInfoList *list)
{
	HTAB	   *hashtab;
	HASHCTL		hash_ctl;
	ListCell   *l;

	/* Create the hash table */
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(Relids);
	hash_ctl.entrysize = sizeof(RelInfoEntry);
	hash_ctl.hash = bitmap_hash;
	hash_ctl.match = bitmap_match;
	hash_ctl.hcxt = CurrentMemoryContext;
	hashtab = hash_create("RelHashTable",
						  256L,
						  &hash_ctl,
						  HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

	/* Insert all the already-existing joinrels */
	foreach(l, list->items)
	{
		void	   *item = lfirst(l);
		RelInfoEntry *hentry;
		bool		found;
		Relids		relids;

		Assert(IsA(item, RelOptInfo));
		relids = ((RelOptInfo *) item)->relids;

		hentry = (RelInfoEntry *) hash_search(hashtab,
											  &relids,
											  HASH_ENTER,
											  &found);
		Assert(!found);
		hentry->data = item;
	}

	list->hash = hashtab;
}

/*
 * find_rel_info
 *	  Find a base or join relation entry.
 */
static void *
find_rel_info(RelInfoList *list, Relids relids)
{
	if (list == NULL)
		return NULL;

	/*
	 * Switch to using hash lookup when list grows "too long".  The threshold
	 * is arbitrary and is known only here.
	 */
	if (!list->hash && list_length(list->items) > 32)
		build_rel_hash(list);

	/*
	 * Use either hashtable lookup or linear search, as appropriate.
	 *
	 * Note: the seemingly redundant hashkey variable is used to avoid taking
	 * the address of relids; unless the compiler is exceedingly smart, doing
	 * so would force relids out of a register and thus probably slow down the
	 * list-search case.
	 */
	if (list->hash)
	{
		Relids		hashkey = relids;
		RelInfoEntry *hentry;

		hentry = (RelInfoEntry *) hash_search(list->hash,
											  &hashkey,
											  HASH_FIND,
											  NULL);
		if (hentry)
			return hentry->data;
	}
	else
	{
		ListCell   *l;

		foreach(l, list->items)
		{
			void	   *item = lfirst(l);
			Relids		item_relids = NULL;

			Assert(IsA(item, RelOptInfo) || IsA(item, RelAggInfo));

			if (IsA(item, RelOptInfo))
				item_relids = ((RelOptInfo *) item)->relids;
			else if (IsA(item, RelAggInfo))
				item_relids = ((RelAggInfo *) item)->relids;

			if (bms_equal(item_relids, relids))
				return item;
		}
	}

	return NULL;
}

/*
 * find_join_rel
 *	  Returns relation entry corresponding to 'relids' (a set of RT indexes),
 *	  or NULL if none exists.  This is for join relations.
 */
RelOptInfo *
find_join_rel(PlannerInfo *root, Relids relids)
{
	return (RelOptInfo *) find_rel_info(root->join_rel_list, relids);
}

/*
 * add_rel_info
 *		Add relation specific info to a list, and also add it to the auxiliary
 *		hashtable if there is one.
 */
static void
add_rel_info(RelInfoList *list, void *data)
{
	Assert(IsA(data, RelOptInfo) || IsA(data, RelAggInfo));

	/* GEQO requires us to append the new joinrel to the end of the list! */
	list->items = lappend(list->items, data);

	/* store it into the auxiliary hashtable if there is one. */
	if (list->hash)
	{
		Relids		relids;
		RelInfoEntry *hentry;
		bool		found;

		if (IsA(data, RelOptInfo))
			relids = ((RelOptInfo *) data)->relids;
		else if (IsA(data, RelAggInfo))
			relids = ((RelAggInfo *) data)->relids;

		hentry = (RelInfoEntry *) hash_search(list->hash,
											  &relids,
											  HASH_ENTER,
											  &found);
		Assert(!found);
		hentry->data = data;
	}
}

/*
 * add_join_rel
 *		Add given join relation to the list of join relations in the given
 *		PlannerInfo.
 */
static void
add_join_rel(PlannerInfo *root, RelOptInfo *joinrel)
{
	add_rel_info(root->join_rel_list, joinrel);
}

/*
 * add_grouped_rel
 *		Add grouped base or join relation to the list of grouped relations in
 *		the given PlannerInfo. Also add the corresponding RelAggInfo to
 *		agg_info_list.
 */
void
add_grouped_rel(PlannerInfo *root, RelOptInfo *rel, RelAggInfo *agg_info)
{
	add_rel_info(&root->upper_rels[UPPERREL_PARTIAL_GROUP_AGG], rel);
	add_rel_info(root->agg_info_list, agg_info);
}

/*
 * find_grouped_rel
 *	  Returns grouped relation entry (base or join relation) corresponding to
 *	  'relids' or NULL if none exists.
 *
 * If agg_info_p is a valid pointer, then pointer to RelAggInfo that
 * corresponds to the relation returned is assigned to *agg_info_p.
 *
 * The call fetch_upper_rel(root, UPPERREL_PARTIAL_GROUP_AGG, ...) should
 * return the same relation if it exists, however the behavior is different if
 * the relation is not there. find_grouped_rel() should be used in
 * query_planner() and subroutines.
 */
RelOptInfo *
find_grouped_rel(PlannerInfo *root, Relids relids, RelAggInfo **agg_info_p)
{
	RelOptInfo *rel;

	rel = (RelOptInfo *) find_rel_info(&root->upper_rels[UPPERREL_PARTIAL_GROUP_AGG],
									   relids);
	if (rel == NULL)
	{
		if (agg_info_p)
			*agg_info_p = NULL;

		return NULL;
	}

	/* Is caller interested in RelAggInfo? */
	if (agg_info_p)
	{
		RelAggInfo *agg_info;

		agg_info = (RelAggInfo *) find_rel_info(root->agg_info_list, relids);

		/* The relation exists, so the agg_info should be there too. */
		Assert(agg_info != NULL);

		*agg_info_p = agg_info;
	}

	return rel;
}

/*
 * set_foreign_rel_properties
 *		Set up foreign-join fields if outer and inner relation are foreign
 *		tables (or joins) belonging to the same server and assigned to the same
 *		user to check access permissions as.
 *
 * In addition to an exact match of userid, we allow the case where one side
 * has zero userid (implying current user) and the other side has explicit
 * userid that happens to equal the current user; but in that case, pushdown of
 * the join is only valid for the current user.  The useridiscurrent field
 * records whether we had to make such an assumption for this join or any
 * sub-join.
 *
 * Otherwise these fields are left invalid, so GetForeignJoinPaths will not be
 * called for the join relation.
 *
 * GPDB: Also, EXECUTE ON must match. (Perhaps we shouldn't allow EXECUTE
 * ON on individual tables? Then it would be enough to compare server id)
 */
static void
set_foreign_rel_properties(RelOptInfo *joinrel, RelOptInfo *outer_rel,
						   RelOptInfo *inner_rel)
{
	if (OidIsValid(outer_rel->serverid) &&
		inner_rel->serverid == outer_rel->serverid &&
		inner_rel->exec_location == outer_rel->exec_location)
	{
		if (inner_rel->userid == outer_rel->userid)
		{
			joinrel->serverid = outer_rel->serverid;
			joinrel->userid = outer_rel->userid;
			joinrel->useridiscurrent = outer_rel->useridiscurrent || inner_rel->useridiscurrent;
			joinrel->fdwroutine = outer_rel->fdwroutine;
			joinrel->exec_location = outer_rel->exec_location;
		}
		else if (!OidIsValid(inner_rel->userid) &&
				 outer_rel->userid == GetUserId())
		{
			joinrel->serverid = outer_rel->serverid;
			joinrel->userid = outer_rel->userid;
			joinrel->useridiscurrent = true;
			joinrel->fdwroutine = outer_rel->fdwroutine;
			joinrel->exec_location = outer_rel->exec_location;
		}
		else if (!OidIsValid(outer_rel->userid) &&
				 inner_rel->userid == GetUserId())
		{
			joinrel->serverid = outer_rel->serverid;
			joinrel->userid = inner_rel->userid;
			joinrel->useridiscurrent = true;
			joinrel->fdwroutine = outer_rel->fdwroutine;
			joinrel->exec_location = outer_rel->exec_location;
		}
	}
}

/*
 * build_join_rel
 *	  Returns relation entry corresponding to the union of two given rels,
 *	  creating a new relation entry if none already exists.
 *
 * 'joinrelids' is the Relids set that uniquely identifies the join
 * 'outer_rel' and 'inner_rel' are relation nodes for the relations to be
 *		joined
 * 'sjinfo': join context info
 * 'restrictlist_ptr': result variable.  If not NULL, *restrictlist_ptr
 *		receives the list of RestrictInfo nodes that apply to this
 *		particular pair of joinable relations.
 * 'agg_info' indicates that grouped join relation should be created.
 *
 * restrictlist_ptr makes the routine's API a little grotty, but it saves
 * duplicated calculation of the restrictlist...
 */
RelOptInfo *
build_join_rel(PlannerInfo *root,
			   Relids joinrelids,
			   RelOptInfo *outer_rel,
			   RelOptInfo *inner_rel,
			   SpecialJoinInfo *sjinfo,
			   List **restrictlist_ptr,
			   RelAggInfo *agg_info)
{
	RelOptInfo *joinrel;
	List	   *restrictlist;
	bool		grouped = agg_info != NULL;

	/* This function should be used only for join between parents. */
	Assert(!IS_OTHER_REL(outer_rel) && !IS_OTHER_REL(inner_rel));

	/*
	 * See if we already have a joinrel for this set of base rels.
	 */
	joinrel = !grouped ? find_join_rel(root, joinrelids) :
		find_grouped_rel(root, joinrelids, NULL);

	if (joinrel)
	{
		/*
		 * Yes, so we only need to figure the restrictlist for this particular
		 * pair of component relations.
		 */
		if (restrictlist_ptr)
			*restrictlist_ptr = build_joinrel_restrictlist(root,
														   joinrel,
														   outer_rel,
														   inner_rel);

		return joinrel;
	}

	/*
	 * Nope, so make one.
	 */
	joinrel = makeNode(RelOptInfo);
	joinrel->reloptkind = RELOPT_JOINREL;
	joinrel->relids = bms_copy(joinrelids);
	joinrel->rows = 0;
	/* cheap startup cost is interesting iff not all tuples to be retrieved */
	joinrel->consider_startup = (root->tuple_fraction > 0);
	joinrel->consider_param_startup = false;
	joinrel->consider_parallel = false;
	joinrel->reltarget = create_empty_pathtarget();
	joinrel->pathlist = NIL;
	joinrel->ppilist = NIL;
	joinrel->partial_pathlist = NIL;
	joinrel->cheapest_startup_path = NULL;
	joinrel->cheapest_total_path = NULL;
	joinrel->cheapest_unique_path = NULL;
	joinrel->cheapest_parameterized_paths = NIL;
	/* init direct_lateral_relids from children; we'll finish it up below */
	joinrel->direct_lateral_relids =
		bms_union(outer_rel->direct_lateral_relids,
				  inner_rel->direct_lateral_relids);
	joinrel->lateral_relids = min_join_parameterization(root, joinrel->relids,
														outer_rel, inner_rel);
	joinrel->relid = 0;			/* indicates not a baserel */
	joinrel->rtekind = RTE_JOIN;
	joinrel->min_attr = 0;
	joinrel->max_attr = 0;
	joinrel->attr_needed = NULL;
	joinrel->attr_widths = NULL;
	joinrel->lateral_vars = NIL;
	joinrel->lateral_referencers = NULL;
	joinrel->indexlist = NIL;
	joinrel->statlist = NIL;
	joinrel->pages = 0;
	joinrel->tuples = 0;
	joinrel->allvisfrac = 0;
	joinrel->subroot = NULL;
	joinrel->subplan_params = NIL;
	joinrel->rel_parallel_workers = -1;
	joinrel->serverid = InvalidOid;
	joinrel->userid = InvalidOid;
	joinrel->useridiscurrent = false;
	joinrel->exec_location = FTEXECLOCATION_NOT_DEFINED;
	joinrel->fdwroutine = NULL;
	joinrel->fdw_private = NULL;
	joinrel->unique_for_rels = NIL;
	joinrel->non_unique_for_rels = NIL;
	joinrel->baserestrictinfo = NIL;
	joinrel->baserestrictcost.startup = 0;
	joinrel->baserestrictcost.per_tuple = 0;
	joinrel->baserestrict_min_security = UINT_MAX;
	joinrel->joininfo = NIL;
	joinrel->has_eclass_joins = false;
	joinrel->consider_partitionwise_join = false;	/* might get changed later */
	joinrel->top_parent_relids = NULL;
	joinrel->part_scheme = NULL;
	joinrel->nparts = 0;
	joinrel->boundinfo = NULL;
	joinrel->partition_qual = NIL;
	joinrel->part_rels = NULL;
	joinrel->partexprs = NULL;
	joinrel->nullable_partexprs = NULL;
	joinrel->partitioned_child_rels = NIL;

	/* Compute information relevant to the foreign relations. */
	set_foreign_rel_properties(joinrel, outer_rel, inner_rel);

	/*
	 * Create a new tlist containing just the vars that need to be output from
	 * this join (ie, are needed for higher joinclauses or final output).
	 *
	 * NOTE: the tlist order for a join rel will depend on which pair of outer
	 * and inner rels we first try to build it from.  But the contents should
	 * be the same regardless.
	 */
	if (!grouped)
	{
		joinrel->reltarget = create_empty_pathtarget();
		build_joinrel_tlist(root, joinrel, outer_rel);
		build_joinrel_tlist(root, joinrel, inner_rel);
		add_placeholders_to_joinrel(root, joinrel, outer_rel, inner_rel);
	}
	else
	{
		/*
		 * The target for grouped join should already have its cost and width
		 * computed, see create_rel_agg_info().
		 */
		joinrel->reltarget = agg_info->target;
	}

	/*
	 * add_placeholders_to_joinrel also took care of adding the ph_lateral
	 * sets of any PlaceHolderVars computed here to direct_lateral_relids, so
	 * now we can finish computing that.  This is much like the computation of
	 * the transitively-closed lateral_relids in min_join_parameterization,
	 * except that here we *do* have to consider the added PHVs.
	 */
	joinrel->direct_lateral_relids =
		bms_del_members(joinrel->direct_lateral_relids, joinrel->relids);
	if (bms_is_empty(joinrel->direct_lateral_relids))
		joinrel->direct_lateral_relids = NULL;

	/* GPDB_96_MERGE_FIXME: The 'width' is now in joinrel->reltarget. But I
	 * don't think this is the right place to set it. Do we actually care
	 * about doing this? PostgreSQL doesn't bother..
	 */
	/* cap width of output row by sum of its inputs */
	//joinrel->width = Min(joinrel->width, outer_rel->width + inner_rel->width);

	/*
	 * Construct restrict and join clause lists for the new joinrel. (The
	 * caller might or might not need the restrictlist, but I need it anyway
	 * for set_joinrel_size_estimates().)
	 */
	restrictlist = build_joinrel_restrictlist(root, joinrel,
											  outer_rel, inner_rel);
	if (restrictlist_ptr)
		*restrictlist_ptr = restrictlist;
	build_joinrel_joinlist(joinrel, outer_rel, inner_rel);

	/*
	 * This is also the right place to check whether the joinrel has any
	 * pending EquivalenceClass joins.
	 */
	joinrel->has_eclass_joins = has_relevant_eclass_joinclause(root, joinrel);

	/* Store the partition information. */
	if (!grouped)
		build_joinrel_partition_info(joinrel, outer_rel, inner_rel,
									 restrictlist, sjinfo->jointype);

	/*
	 * Set estimates of the joinrel's size.
	 */
	if (!grouped)
		set_joinrel_size_estimates(root, joinrel, outer_rel, inner_rel,
								   sjinfo, restrictlist);
	else
	{
		/*
		 * Grouping essentially changes the number of rows.
		 *
		 * XXX We do not distinguish whether two plain rels are joined and the
		 * result is aggregated, or the aggregation has been already applied
		 * to one of the input rels. Is this worth extra effort, e.g.
		 * maintaining a separate RelOptInfo for each case (one difficulty
		 * that would introduce is construction of AppendPath)?
		 */
		joinrel->rows = estimate_num_groups(root, agg_info->group_exprs,
											agg_info->input_rows, NULL);
	}

	/*
	 * Set the consider_parallel flag if this joinrel could potentially be
	 * scanned within a parallel worker.  If this flag is false for either
	 * inner_rel or outer_rel, then it must be false for the joinrel also.
	 * Even if both are true, there might be parallel-restricted expressions
	 * in the targetlist or quals.
	 *
	 * Note that if there are more than two rels in this relation, they could
	 * be divided between inner_rel and outer_rel in any arbitrary way.  We
	 * assume this doesn't matter, because we should hit all the same baserels
	 * and joinclauses while building up to this joinrel no matter which we
	 * take; therefore, we should make the same decision here however we get
	 * here.
	 */
	if (inner_rel->consider_parallel && outer_rel->consider_parallel &&
		is_parallel_safe(root, (Node *) restrictlist) &&
		is_parallel_safe(root, (Node *) joinrel->reltarget->exprs))
		joinrel->consider_parallel = true;

	/* Add the joinrel to the PlannerInfo. */
	if (!grouped)
		add_join_rel(root, joinrel);
	else
		add_grouped_rel(root, joinrel, agg_info);

	/*
	 * Also, if dynamic-programming join search is active, add the new
	 * joinrelset to the appropriate sublist.  Note: you might think the
	 * Assert on number of members should be for equality, but some of the
	 * level 1 rels might have been joinrels already, so we can only assert
	 * <=.
	 *
	 * Do noting for grouped relation as it's stored aside from
	 * join_rel_level.
	 */
	if (root->join_rel_level && !grouped)
	{
		Assert(root->join_cur_level > 0);
		Assert(root->join_cur_level <= bms_num_members(joinrelids));
		root->join_rel_level[root->join_cur_level] =
			lappend(root->join_rel_level[root->join_cur_level],
					joinrel);
	}

	return joinrel;
}

/*
 * build_child_join_rel
 *	  Builds RelOptInfo representing join between given two child relations.
 *
 * 'outer_rel' and 'inner_rel' are the RelOptInfos of child relations being
 *		joined
 * 'parent_joinrel' is the RelOptInfo representing the join between parent
 *		relations. Some of the members of new RelOptInfo are produced by
 *		translating corresponding members of this RelOptInfo
 * 'sjinfo': child-join context info
 * 'restrictlist': list of RestrictInfo nodes that apply to this particular
 *		pair of joinable relations
 * 'jointype' is the join type (inner, left, full, etc)
 */
RelOptInfo *
build_child_join_rel(PlannerInfo *root, RelOptInfo *outer_rel,
					 RelOptInfo *inner_rel, RelOptInfo *parent_joinrel,
					 List *restrictlist, SpecialJoinInfo *sjinfo,
					 JoinType jointype)
{
	RelOptInfo *joinrel = makeNode(RelOptInfo);
	AppendRelInfo **appinfos;
	int			nappinfos;

	/* Only joins between "other" relations land here. */
	Assert(IS_OTHER_REL(outer_rel) && IS_OTHER_REL(inner_rel));

	/* The parent joinrel should have consider_partitionwise_join set. */
	Assert(parent_joinrel->consider_partitionwise_join);

	joinrel->reloptkind = RELOPT_OTHER_JOINREL;
	joinrel->relids = bms_union(outer_rel->relids, inner_rel->relids);
	joinrel->rows = 0;
	/* cheap startup cost is interesting iff not all tuples to be retrieved */
	joinrel->consider_startup = (root->tuple_fraction > 0);
	joinrel->consider_param_startup = false;
	joinrel->consider_parallel = false;
	joinrel->reltarget = create_empty_pathtarget();
	joinrel->pathlist = NIL;
	joinrel->ppilist = NIL;
	joinrel->partial_pathlist = NIL;
	joinrel->cheapest_startup_path = NULL;
	joinrel->cheapest_total_path = NULL;
	joinrel->cheapest_unique_path = NULL;
	joinrel->cheapest_parameterized_paths = NIL;
	joinrel->direct_lateral_relids = NULL;
	joinrel->lateral_relids = NULL;
	joinrel->relid = 0;			/* indicates not a baserel */
	joinrel->rtekind = RTE_JOIN;
	joinrel->min_attr = 0;
	joinrel->max_attr = 0;
	joinrel->attr_needed = NULL;
	joinrel->attr_widths = NULL;
	joinrel->lateral_vars = NIL;
	joinrel->lateral_referencers = NULL;
	joinrel->indexlist = NIL;
	joinrel->pages = 0;
	joinrel->tuples = 0;
	joinrel->allvisfrac = 0;
	joinrel->subroot = NULL;
	joinrel->subplan_params = NIL;
	joinrel->serverid = InvalidOid;
	joinrel->userid = InvalidOid;
	joinrel->useridiscurrent = false;
	joinrel->fdwroutine = NULL;
	joinrel->fdw_private = NULL;
	joinrel->baserestrictinfo = NIL;
	joinrel->baserestrictcost.startup = 0;
	joinrel->baserestrictcost.per_tuple = 0;
	joinrel->joininfo = NIL;
	joinrel->has_eclass_joins = false;
	joinrel->consider_partitionwise_join = false;	/* might get changed later */
	joinrel->top_parent_relids = NULL;
	joinrel->part_scheme = NULL;
	joinrel->nparts = 0;
	joinrel->boundinfo = NULL;
	joinrel->partition_qual = NIL;
	joinrel->part_rels = NULL;
	joinrel->partexprs = NULL;
	joinrel->nullable_partexprs = NULL;
	joinrel->partitioned_child_rels = NIL;

	joinrel->top_parent_relids = bms_union(outer_rel->top_parent_relids,
										   inner_rel->top_parent_relids);

	/* Compute information relevant to foreign relations. */
	set_foreign_rel_properties(joinrel, outer_rel, inner_rel);

	appinfos = find_appinfos_by_relids(root, joinrel->relids, &nappinfos);

	/* Set up reltarget struct */
	build_child_join_reltarget(root, parent_joinrel, joinrel,
							   nappinfos, appinfos);

	/* Construct joininfo list. */
	joinrel->joininfo = (List *) adjust_appendrel_attrs(root,
														(Node *) parent_joinrel->joininfo,
														nappinfos,
														appinfos);
	pfree(appinfos);

	/*
	 * Lateral relids referred in child join will be same as that referred in
	 * the parent relation. Throw any partial result computed while building
	 * the targetlist.
	 */
	bms_free(joinrel->direct_lateral_relids);
	bms_free(joinrel->lateral_relids);
	joinrel->direct_lateral_relids = (Relids) bms_copy(parent_joinrel->direct_lateral_relids);
	joinrel->lateral_relids = (Relids) bms_copy(parent_joinrel->lateral_relids);

	/*
	 * If the parent joinrel has pending equivalence classes, so does the
	 * child.
	 */
	joinrel->has_eclass_joins = parent_joinrel->has_eclass_joins;

	/* Is the join between partitions itself partitioned? */
	build_joinrel_partition_info(joinrel, outer_rel, inner_rel, restrictlist,
								 jointype);

	/* Child joinrel is parallel safe if parent is parallel safe. */
	joinrel->consider_parallel = parent_joinrel->consider_parallel;

	/* Set estimates of the child-joinrel's size. */
	set_joinrel_size_estimates(root, joinrel, outer_rel, inner_rel,
							   sjinfo, restrictlist);

	/* We build the join only once. */
	Assert(!find_join_rel(root, joinrel->relids));

	/* Add the relation to the PlannerInfo. */
	add_join_rel(root, joinrel);

	return joinrel;
}

/*
 * min_join_parameterization
 *
 * Determine the minimum possible parameterization of a joinrel, that is, the
 * set of other rels it contains LATERAL references to.  We save this value in
 * the join's RelOptInfo.  This function is split out of build_join_rel()
 * because join_is_legal() needs the value to check a prospective join.
 */
Relids
min_join_parameterization(PlannerInfo *root,
						  Relids joinrelids,
						  RelOptInfo *outer_rel,
						  RelOptInfo *inner_rel)
{
	Relids		result;

	/*
	 * Basically we just need the union of the inputs' lateral_relids, less
	 * whatever is already in the join.
	 *
	 * It's not immediately obvious that this is a valid way to compute the
	 * result, because it might seem that we're ignoring possible lateral refs
	 * of PlaceHolderVars that are due to be computed at the join but not in
	 * either input.  However, because create_lateral_join_info() already
	 * charged all such PHV refs to each member baserel of the join, they'll
	 * be accounted for already in the inputs' lateral_relids.  Likewise, we
	 * do not need to worry about doing transitive closure here, because that
	 * was already accounted for in the original baserel lateral_relids.
	 */
	result = bms_union(outer_rel->lateral_relids, inner_rel->lateral_relids);
	result = bms_del_members(result, joinrelids);

	/* Maintain invariant that result is exactly NULL if empty */
	if (bms_is_empty(result))
		result = NULL;

	return result;
}

/*
 * build_joinrel_tlist
 *	  Builds a join relation's target list from an input relation.
 *	  (This is invoked twice to handle the two input relations.)
 *
 * The join's targetlist includes all Vars of its member relations that
 * will still be needed above the join.  This subroutine adds all such
 * Vars from the specified input rel's tlist to the join rel's tlist.
 *
 * We also compute the expected width of the join's output, making use
 * of data that was cached at the baserel level by set_rel_width().
 */
static void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel,
					RelOptInfo *input_rel)
{
	Relids		relids = joinrel->relids;
	ListCell   *vars;

	foreach(vars, input_rel->reltarget->exprs)
	{
		Var		   *var = (Var *) lfirst(vars);
		RelOptInfo *baserel;
		int			ndx;

		/*
		 * Ignore PlaceHolderVars in the input tlists; we'll make our own
		 * decisions about whether to copy them.
		 */
		if (IsA(var, PlaceHolderVar))
			continue;

		/*
		 * Otherwise, anything in a baserel or joinrel targetlist ought to be
		 * a Var.  (More general cases can only appear in appendrel child
		 * rels, which will never be seen here.)
		 */
		if (!IsA(var, Var))
			elog(ERROR, "unexpected node type in rel targetlist: %d",
				 (int) nodeTag(var));

		/* Get the Var's original base rel */
		baserel = find_base_rel(root, var->varno);

        /* System-defined attribute, whole row, or user-defined attribute */
        Assert(var->varattno >= baserel->min_attr &&
               var->varattno <= baserel->max_attr);

		/* Is it still needed above this joinrel? */
		ndx = var->varattno - baserel->min_attr;
		if (bms_nonempty_difference(baserel->attr_needed[ndx], relids))
		{
			/* Yup, add it to the output */
			joinrel->reltarget->exprs = lappend(joinrel->reltarget->exprs, var);
			/* Vars have cost zero, so no need to adjust reltarget->cost */
			joinrel->reltarget->width += baserel->attr_widths[ndx];
		}
	}
}

/*
 * build_joinrel_restrictlist
 * build_joinrel_joinlist
 *	  These routines build lists of restriction and join clauses for a
 *	  join relation from the joininfo lists of the relations it joins.
 *
 *	  These routines are separate because the restriction list must be
 *	  built afresh for each pair of input sub-relations we consider, whereas
 *	  the join list need only be computed once for any join RelOptInfo.
 *	  The join list is fully determined by the set of rels making up the
 *	  joinrel, so we should get the same results (up to ordering) from any
 *	  candidate pair of sub-relations.  But the restriction list is whatever
 *	  is not handled in the sub-relations, so it depends on which
 *	  sub-relations are considered.
 *
 *	  If a join clause from an input relation refers to base rels still not
 *	  present in the joinrel, then it is still a join clause for the joinrel;
 *	  we put it into the joininfo list for the joinrel.  Otherwise,
 *	  the clause is now a restrict clause for the joined relation, and we
 *	  return it to the caller of build_joinrel_restrictlist() to be stored in
 *	  join paths made from this pair of sub-relations.  (It will not need to
 *	  be considered further up the join tree.)
 *
 *	  In many case we will find the same RestrictInfos in both input
 *	  relations' joinlists, so be careful to eliminate duplicates.
 *	  Pointer equality should be a sufficient test for dups, since all
 *	  the various joinlist entries ultimately refer to RestrictInfos
 *	  pushed into them by distribute_restrictinfo_to_rels().
 *
 * 'joinrel' is a join relation node
 * 'outer_rel' and 'inner_rel' are a pair of relations that can be joined
 *		to form joinrel.
 *
 * build_joinrel_restrictlist() returns a list of relevant restrictinfos,
 * whereas build_joinrel_joinlist() stores its results in the joinrel's
 * joininfo list.  One or the other must accept each given clause!
 *
 * NB: Formerly, we made deep(!) copies of each input RestrictInfo to pass
 * up to the join relation.  I believe this is no longer necessary, because
 * RestrictInfo nodes are no longer context-dependent.  Instead, just include
 * the original nodes in the lists made for the join relation.
 */
static List *
build_joinrel_restrictlist(PlannerInfo *root,
						   RelOptInfo *joinrel,
						   RelOptInfo *outer_rel,
						   RelOptInfo *inner_rel)
{
	List	   *result;

	/*
	 * Collect all the clauses that syntactically belong at this level,
	 * eliminating any duplicates (important since we will see many of the
	 * same clauses arriving from both input relations).
	 */
	result = subbuild_joinrel_restrictlist(joinrel, outer_rel->joininfo, NIL);
	result = subbuild_joinrel_restrictlist(joinrel, inner_rel->joininfo, result);

	/*
	 * Add on any clauses derived from EquivalenceClasses.  These cannot be
	 * redundant with the clauses in the joininfo lists, so don't bother
	 * checking.
	 */
	result = list_concat(result,
						 generate_join_implied_equalities(root,
														  joinrel->relids,
														  outer_rel->relids,
														  inner_rel));

	return result;
}

static void
build_joinrel_joinlist(RelOptInfo *joinrel,
					   RelOptInfo *outer_rel,
					   RelOptInfo *inner_rel)
{
	List	   *result;

	/*
	 * Collect all the clauses that syntactically belong above this level,
	 * eliminating any duplicates (important since we will see many of the
	 * same clauses arriving from both input relations).
	 */
	result = subbuild_joinrel_joinlist(joinrel, outer_rel->joininfo, NIL);
	result = subbuild_joinrel_joinlist(joinrel, inner_rel->joininfo, result);

	joinrel->joininfo = result;
}

static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel,
							  List *joininfo_list,
							  List *new_restrictlist)
{
	ListCell   *l;

	foreach(l, joininfo_list)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

		if (bms_is_subset(rinfo->required_relids, joinrel->relids))
		{
			/*
			 * This clause becomes a restriction clause for the joinrel, since
			 * it refers to no outside rels.  Add it to the list, being
			 * careful to eliminate duplicates. (Since RestrictInfo nodes in
			 * different joinlists will have been multiply-linked rather than
			 * copied, pointer equality should be a sufficient test.)
			 */
			new_restrictlist = list_append_unique_ptr(new_restrictlist, rinfo);
		}
		else
		{
			/*
			 * This clause is still a join clause at this level, so we ignore
			 * it in this routine.
			 */
		}
	}

	return new_restrictlist;
}

static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel,
						  List *joininfo_list,
						  List *new_joininfo)
{
	ListCell   *l;

	/* Expected to be called only for join between parent relations. */
	Assert(joinrel->reloptkind == RELOPT_JOINREL);

	foreach(l, joininfo_list)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

		if (bms_is_subset(rinfo->required_relids, joinrel->relids))
		{
			/*
			 * This clause becomes a restriction clause for the joinrel, since
			 * it refers to no outside rels.  So we can ignore it in this
			 * routine.
			 */
		}
		else
		{
			/*
			 * This clause is still a join clause at this level, so add it to
			 * the new joininfo list, being careful to eliminate duplicates.
			 * (Since RestrictInfo nodes in different joinlists will have been
			 * multiply-linked rather than copied, pointer equality should be
			 * a sufficient test.)
			 */
			new_joininfo = list_append_unique_ptr(new_joininfo, rinfo);
		}
	}

	return new_joininfo;
}


/*
 * fetch_upper_rel
 *		Build a RelOptInfo describing some post-scan/join query processing,
 *		or return a pre-existing one if somebody already built it.
 *
 * An "upper" relation is identified by an UpperRelationKind and a Relids set.
 * The meaning of the Relids set is not specified here, and very likely will
 * vary for different relation kinds.
 *
 * Most of the fields in an upper-level RelOptInfo are not used and are not
 * set here (though makeNode should ensure they're zeroes).  We basically only
 * care about fields that are of interest to add_path() and set_cheapest().
 */
RelOptInfo *
fetch_upper_rel(PlannerInfo *root, UpperRelationKind kind, Relids relids)
{
	RelInfoList *list = &root->upper_rels[kind];
	RelOptInfo *upperrel;

	/* If we already made this upperrel for the query, return it */
	if (list)
	{
		upperrel = find_rel_info(list, relids);
		if (upperrel)
			return upperrel;
	}

	upperrel = makeNode(RelOptInfo);
	upperrel->reloptkind = RELOPT_UPPER_REL;
	upperrel->relids = bms_copy(relids);

	/* cheap startup cost is interesting iff not all tuples to be retrieved */
	upperrel->consider_startup = (root->tuple_fraction > 0);
	upperrel->consider_param_startup = false;
	upperrel->consider_parallel = false;	/* might get changed later */
	upperrel->reltarget = create_empty_pathtarget();
	upperrel->pathlist = NIL;
	upperrel->cheapest_startup_path = NULL;
	upperrel->cheapest_total_path = NULL;
	upperrel->cheapest_unique_path = NULL;
	upperrel->cheapest_parameterized_paths = NIL;

	add_rel_info(&root->upper_rels[kind], upperrel);

	return upperrel;
}


/*
 * find_childrel_parents
 *		Compute the set of parent relids of an appendrel child rel.
 *
 * Since appendrels can be nested, a child could have multiple levels of
 * appendrel ancestors.  This function computes a Relids set of all the
 * parent relation IDs.
 */
Relids
find_childrel_parents(PlannerInfo *root, RelOptInfo *rel)
{
	Relids		result = NULL;

	Assert(rel->reloptkind == RELOPT_OTHER_MEMBER_REL);
	Assert(rel->relid > 0 && rel->relid < root->simple_rel_array_size);

	do
	{
		AppendRelInfo *appinfo = root->append_rel_array[rel->relid];
		Index		prelid = appinfo->parent_relid;

		result = bms_add_member(result, prelid);

		/* traverse up to the parent rel, loop if it's also a child rel */
		rel = find_base_rel(root, prelid);
	} while (rel->reloptkind == RELOPT_OTHER_MEMBER_REL);

	Assert(rel->reloptkind == RELOPT_BASEREL);

	return result;
}


/*
 * get_baserel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for a base relation,
 *		constructing one if we don't have one already.
 *
 * This centralizes estimating the rowcounts for parameterized paths.
 * We need to cache those to be sure we use the same rowcount for all paths
 * of the same parameterization for a given rel.  This is also a convenient
 * place to determine which movable join clauses the parameterized path will
 * be responsible for evaluating.
 */
ParamPathInfo *
get_baserel_parampathinfo(PlannerInfo *root, RelOptInfo *baserel,
						  Relids required_outer)
{
	ParamPathInfo *ppi;
	Relids		joinrelids;
	List	   *pclauses;
	double		rows;
	ListCell   *lc;

	/* If rel has LATERAL refs, every path for it should account for them */
	Assert(bms_is_subset(baserel->lateral_relids, required_outer));

	/* Unparameterized paths have no ParamPathInfo */
	if (bms_is_empty(required_outer))
		return NULL;

	Assert(!bms_overlap(baserel->relids, required_outer));

	/* If we already have a PPI for this parameterization, just return it */
	if ((ppi = find_param_path_info(baserel, required_outer)))
		return ppi;

	/*
	 * Identify all joinclauses that are movable to this base rel given this
	 * parameterization.
	 */
	joinrelids = bms_union(baserel->relids, required_outer);
	pclauses = NIL;
	foreach(lc, baserel->joininfo)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		if (join_clause_is_movable_into(rinfo,
										baserel->relids,
										joinrelids))
			pclauses = lappend(pclauses, rinfo);
	}

	/*
	 * Add in joinclauses generated by EquivalenceClasses, too.  (These
	 * necessarily satisfy join_clause_is_movable_into.)
	 */
	pclauses = list_concat(pclauses,
						   generate_join_implied_equalities(root,
															joinrelids,
															required_outer,
															baserel));

	/* Estimate the number of rows returned by the parameterized scan */
	rows = get_parameterized_baserel_size(root, baserel, pclauses);

	/* And now we can build the ParamPathInfo */
	ppi = makeNode(ParamPathInfo);
	ppi->ppi_req_outer = required_outer;
	ppi->ppi_rows = rows;
	ppi->ppi_clauses = pclauses;
	baserel->ppilist = lappend(baserel->ppilist, ppi);

	return ppi;
}

/*
 * get_joinrel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for a join relation,
 *		constructing one if we don't have one already.
 *
 * This centralizes estimating the rowcounts for parameterized paths.
 * We need to cache those to be sure we use the same rowcount for all paths
 * of the same parameterization for a given rel.  This is also a convenient
 * place to determine which movable join clauses the parameterized path will
 * be responsible for evaluating.
 *
 * outer_path and inner_path are a pair of input paths that can be used to
 * construct the join, and restrict_clauses is the list of regular join
 * clauses (including clauses derived from EquivalenceClasses) that must be
 * applied at the join node when using these inputs.
 *
 * Unlike the situation for base rels, the set of movable join clauses to be
 * enforced at a join varies with the selected pair of input paths, so we
 * must calculate that and pass it back, even if we already have a matching
 * ParamPathInfo.  We handle this by adding any clauses moved down to this
 * join to *restrict_clauses, which is an in/out parameter.  (The addition
 * is done in such a way as to not modify the passed-in List structure.)
 *
 * Note: when considering a nestloop join, the caller must have removed from
 * restrict_clauses any movable clauses that are themselves scheduled to be
 * pushed into the right-hand path.  We do not do that here since it's
 * unnecessary for other join types.
 */
ParamPathInfo *
get_joinrel_parampathinfo(PlannerInfo *root, RelOptInfo *joinrel,
						  Path *outer_path,
						  Path *inner_path,
						  SpecialJoinInfo *sjinfo,
						  Relids required_outer,
						  List **restrict_clauses)
{
	ParamPathInfo *ppi;
	Relids		join_and_req;
	Relids		outer_and_req;
	Relids		inner_and_req;
	List	   *pclauses;
	List	   *eclauses;
	List	   *dropped_ecs;
	double		rows;
	ListCell   *lc;

	/* If rel has LATERAL refs, every path for it should account for them */
	Assert(bms_is_subset(joinrel->lateral_relids, required_outer));

	/* Unparameterized paths have no ParamPathInfo or extra join clauses */
	if (bms_is_empty(required_outer))
		return NULL;

	Assert(!bms_overlap(joinrel->relids, required_outer));

	/*
	 * Identify all joinclauses that are movable to this join rel given this
	 * parameterization.  These are the clauses that are movable into this
	 * join, but not movable into either input path.  Treat an unparameterized
	 * input path as not accepting parameterized clauses (because it won't,
	 * per the shortcut exit above), even though the joinclause movement rules
	 * might allow the same clauses to be moved into a parameterized path for
	 * that rel.
	 */
	join_and_req = bms_union(joinrel->relids, required_outer);
	if (outer_path->param_info)
		outer_and_req = bms_union(outer_path->parent->relids,
								  PATH_REQ_OUTER(outer_path));
	else
		outer_and_req = NULL;	/* outer path does not accept parameters */
	if (inner_path->param_info)
		inner_and_req = bms_union(inner_path->parent->relids,
								  PATH_REQ_OUTER(inner_path));
	else
		inner_and_req = NULL;	/* inner path does not accept parameters */

	pclauses = NIL;
	foreach(lc, joinrel->joininfo)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		if (join_clause_is_movable_into(rinfo,
										joinrel->relids,
										join_and_req) &&
			!join_clause_is_movable_into(rinfo,
										 outer_path->parent->relids,
										 outer_and_req) &&
			!join_clause_is_movable_into(rinfo,
										 inner_path->parent->relids,
										 inner_and_req))
			pclauses = lappend(pclauses, rinfo);
	}

	/* Consider joinclauses generated by EquivalenceClasses, too */
	eclauses = generate_join_implied_equalities(root,
												join_and_req,
												required_outer,
												joinrel);
	/* We only want ones that aren't movable to lower levels */
	dropped_ecs = NIL;
	foreach(lc, eclauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		/*
		 * In principle, join_clause_is_movable_into() should accept anything
		 * returned by generate_join_implied_equalities(); but because its
		 * analysis is only approximate, sometimes it doesn't.  So we
		 * currently cannot use this Assert; instead just assume it's okay to
		 * apply the joinclause at this level.
		 */
#ifdef NOT_USED
		Assert(join_clause_is_movable_into(rinfo,
										   joinrel->relids,
										   join_and_req));
#endif
		if (join_clause_is_movable_into(rinfo,
										outer_path->parent->relids,
										outer_and_req))
			continue;			/* drop if movable into LHS */
		if (join_clause_is_movable_into(rinfo,
										inner_path->parent->relids,
										inner_and_req))
		{
			/* drop if movable into RHS, but remember EC for use below */
			Assert(rinfo->left_ec == rinfo->right_ec);
			dropped_ecs = lappend(dropped_ecs, rinfo->left_ec);
			continue;
		}
		pclauses = lappend(pclauses, rinfo);
	}

	/*
	 * EquivalenceClasses are harder to deal with than we could wish, because
	 * of the fact that a given EC can generate different clauses depending on
	 * context.  Suppose we have an EC {X.X, Y.Y, Z.Z} where X and Y are the
	 * LHS and RHS of the current join and Z is in required_outer, and further
	 * suppose that the inner_path is parameterized by both X and Z.  The code
	 * above will have produced either Z.Z = X.X or Z.Z = Y.Y from that EC,
	 * and in the latter case will have discarded it as being movable into the
	 * RHS.  However, the EC machinery might have produced either Y.Y = X.X or
	 * Y.Y = Z.Z as the EC enforcement clause within the inner_path; it will
	 * not have produced both, and we can't readily tell from here which one
	 * it did pick.  If we add no clause to this join, we'll end up with
	 * insufficient enforcement of the EC; either Z.Z or X.X will fail to be
	 * constrained to be equal to the other members of the EC.  (When we come
	 * to join Z to this X/Y path, we will certainly drop whichever EC clause
	 * is generated at that join, so this omission won't get fixed later.)
	 *
	 * To handle this, for each EC we discarded such a clause from, try to
	 * generate a clause connecting the required_outer rels to the join's LHS
	 * ("Z.Z = X.X" in the terms of the above example).  If successful, and if
	 * the clause can't be moved to the LHS, add it to the current join's
	 * restriction clauses.  (If an EC cannot generate such a clause then it
	 * has nothing that needs to be enforced here, while if the clause can be
	 * moved into the LHS then it should have been enforced within that path.)
	 *
	 * Note that we don't need similar processing for ECs whose clause was
	 * considered to be movable into the LHS, because the LHS can't refer to
	 * the RHS so there is no comparable ambiguity about what it might
	 * actually be enforcing internally.
	 */
	if (dropped_ecs)
	{
		Relids		real_outer_and_req;

		real_outer_and_req = bms_union(outer_path->parent->relids,
									   required_outer);
		eclauses =
			generate_join_implied_equalities_for_ecs(root,
													 dropped_ecs,
													 real_outer_and_req,
													 required_outer,
													 outer_path->parent);
		foreach(lc, eclauses)
		{
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

			/* As above, can't quite assert this here */
#ifdef NOT_USED
			Assert(join_clause_is_movable_into(rinfo,
											   outer_path->parent->relids,
											   real_outer_and_req));
#endif
			if (!join_clause_is_movable_into(rinfo,
											 outer_path->parent->relids,
											 outer_and_req))
				pclauses = lappend(pclauses, rinfo);
		}
	}

	/*
	 * Now, attach the identified moved-down clauses to the caller's
	 * restrict_clauses list.  By using list_concat in this order, we leave
	 * the original list structure of restrict_clauses undamaged.
	 */
	*restrict_clauses = list_concat(pclauses, *restrict_clauses);

	/* If we already have a PPI for this parameterization, just return it */
	if ((ppi = find_param_path_info(joinrel, required_outer)))
		return ppi;

	/* Estimate the number of rows returned by the parameterized join */
	rows = get_parameterized_joinrel_size(root, joinrel,
										  outer_path,
										  inner_path,
										  sjinfo,
										  *restrict_clauses);

	/*
	 * And now we can build the ParamPathInfo.  No point in saving the
	 * input-pair-dependent clause list, though.
	 *
	 * Note: in GEQO mode, we'll be called in a temporary memory context, but
	 * the joinrel structure is there too, so no problem.
	 */
	ppi = makeNode(ParamPathInfo);
	ppi->ppi_req_outer = required_outer;
	ppi->ppi_rows = rows;
	ppi->ppi_clauses = NIL;
	joinrel->ppilist = lappend(joinrel->ppilist, ppi);

	return ppi;
}

/*
 * get_appendrel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for an append relation.
 *
 * For an append relation, the rowcount estimate will just be the sum of
 * the estimates for its children.  However, we still need a ParamPathInfo
 * to flag the fact that the path requires parameters.  So this just creates
 * a suitable struct with zero ppi_rows (and no ppi_clauses either, since
 * the Append node isn't responsible for checking quals).
 */
ParamPathInfo *
get_appendrel_parampathinfo(RelOptInfo *appendrel, Relids required_outer)
{
	ParamPathInfo *ppi;

	/* If rel has LATERAL refs, every path for it should account for them */
	Assert(bms_is_subset(appendrel->lateral_relids, required_outer));

	/* Unparameterized paths have no ParamPathInfo */
	if (bms_is_empty(required_outer))
		return NULL;

	Assert(!bms_overlap(appendrel->relids, required_outer));

	/* If we already have a PPI for this parameterization, just return it */
	if ((ppi = find_param_path_info(appendrel, required_outer)))
		return ppi;

	/* Else build the ParamPathInfo */
	ppi = makeNode(ParamPathInfo);
	ppi->ppi_req_outer = required_outer;
	ppi->ppi_rows = 0;
	ppi->ppi_clauses = NIL;
	appendrel->ppilist = lappend(appendrel->ppilist, ppi);

	return ppi;
}

/*
 * Returns a ParamPathInfo for the parameterization given by required_outer, if
 * already available in the given rel. Returns NULL otherwise.
 */
ParamPathInfo *
find_param_path_info(RelOptInfo *rel, Relids required_outer)
{
	ListCell   *lc;

	foreach(lc, rel->ppilist)
	{
		ParamPathInfo *ppi = (ParamPathInfo *) lfirst(lc);

		if (bms_equal(ppi->ppi_req_outer, required_outer))
			return ppi;
	}

	return NULL;
}

/*
 * build_joinrel_partition_info
 *		If the two relations have same partitioning scheme, their join may be
 *		partitioned and will follow the same partitioning scheme as the joining
 *		relations. Set the partition scheme and partition key expressions in
 *		the join relation.
 */
static void
build_joinrel_partition_info(RelOptInfo *joinrel, RelOptInfo *outer_rel,
							 RelOptInfo *inner_rel, List *restrictlist,
							 JoinType jointype)
{
	int			partnatts;
	int			cnt;
	PartitionScheme part_scheme;

	/* Nothing to do if partitionwise join technique is disabled. */
	if (!enable_partitionwise_join)
	{
		Assert(!IS_PARTITIONED_REL(joinrel));
		return;
	}

	/*
	 * We can only consider this join as an input to further partitionwise
	 * joins if (a) the input relations are partitioned and have
	 * consider_partitionwise_join=true, (b) the partition schemes match, and
	 * (c) we can identify an equi-join between the partition keys.  Note that
	 * if it were possible for have_partkey_equi_join to return different
	 * answers for the same joinrel depending on which join ordering we try
	 * first, this logic would break.  That shouldn't happen, though, because
	 * of the way the query planner deduces implied equalities and reorders
	 * the joins.  Please see optimizer/README for details.
	 */
	if (!IS_PARTITIONED_REL(outer_rel) || !IS_PARTITIONED_REL(inner_rel) ||
		!outer_rel->consider_partitionwise_join ||
		!inner_rel->consider_partitionwise_join ||
		outer_rel->part_scheme != inner_rel->part_scheme ||
		!have_partkey_equi_join(joinrel, outer_rel, inner_rel,
								jointype, restrictlist))
	{
		Assert(!IS_PARTITIONED_REL(joinrel));
		return;
	}

	part_scheme = outer_rel->part_scheme;

	Assert(REL_HAS_ALL_PART_PROPS(outer_rel) &&
		   REL_HAS_ALL_PART_PROPS(inner_rel));

	/*
	 * For now, our partition matching algorithm can match partitions only
	 * when the partition bounds of the joining relations are exactly same.
	 * So, bail out otherwise.
	 */
	if (outer_rel->nparts != inner_rel->nparts ||
		!partition_bounds_equal(part_scheme->partnatts,
								part_scheme->parttyplen,
								part_scheme->parttypbyval,
								outer_rel->boundinfo, inner_rel->boundinfo))
	{
		Assert(!IS_PARTITIONED_REL(joinrel));
		return;
	}

	/*
	 * This function will be called only once for each joinrel, hence it
	 * should not have partition scheme, partition bounds, partition key
	 * expressions and array for storing child relations set.
	 */
	Assert(!joinrel->part_scheme && !joinrel->partexprs &&
		   !joinrel->nullable_partexprs && !joinrel->part_rels &&
		   !joinrel->boundinfo);

	/*
	 * Join relation is partitioned using the same partitioning scheme as the
	 * joining relations and has same bounds.
	 */
	joinrel->part_scheme = part_scheme;
	joinrel->boundinfo = outer_rel->boundinfo;
	partnatts = joinrel->part_scheme->partnatts;
	joinrel->partexprs = (List **) palloc0(sizeof(List *) * partnatts);
	joinrel->nullable_partexprs =
		(List **) palloc0(sizeof(List *) * partnatts);
	joinrel->nparts = outer_rel->nparts;
	joinrel->part_rels =
		(RelOptInfo **) palloc0(sizeof(RelOptInfo *) * joinrel->nparts);

	/*
	 * Set the consider_partitionwise_join flag.
	 */
	Assert(outer_rel->consider_partitionwise_join);
	Assert(inner_rel->consider_partitionwise_join);
	joinrel->consider_partitionwise_join = true;

	/*
	 * Construct partition keys for the join.
	 *
	 * An INNER join between two partitioned relations can be regarded as
	 * partitioned by either key expression.  For example, A INNER JOIN B ON
	 * A.a = B.b can be regarded as partitioned on A.a or on B.b; they are
	 * equivalent.
	 *
	 * For a SEMI or ANTI join, the result can only be regarded as being
	 * partitioned in the same manner as the outer side, since the inner
	 * columns are not retained.
	 *
	 * An OUTER join like (A LEFT JOIN B ON A.a = B.b) may produce rows with
	 * B.b NULL. These rows may not fit the partitioning conditions imposed on
	 * B.b. Hence, strictly speaking, the join is not partitioned by B.b and
	 * thus partition keys of an OUTER join should include partition key
	 * expressions from the OUTER side only.  However, because all
	 * commonly-used comparison operators are strict, the presence of nulls on
	 * the outer side doesn't cause any problem; they can't match anything at
	 * future join levels anyway.  Therefore, we track two sets of
	 * expressions: those that authentically partition the relation
	 * (partexprs) and those that partition the relation with the exception
	 * that extra nulls may be present (nullable_partexprs).  When the
	 * comparison operator is strict, the latter is just as good as the
	 * former.
	 */
	for (cnt = 0; cnt < partnatts; cnt++)
	{
		List	   *outer_expr;
		List	   *outer_null_expr;
		List	   *inner_expr;
		List	   *inner_null_expr;
		List	   *partexpr = NIL;
		List	   *nullable_partexpr = NIL;

		outer_expr = list_copy(outer_rel->partexprs[cnt]);
		outer_null_expr = list_copy(outer_rel->nullable_partexprs[cnt]);
		inner_expr = list_copy(inner_rel->partexprs[cnt]);
		inner_null_expr = list_copy(inner_rel->nullable_partexprs[cnt]);

		switch (jointype)
		{
			case JOIN_INNER:
				partexpr = list_concat(outer_expr, inner_expr);
				nullable_partexpr = list_concat(outer_null_expr,
												inner_null_expr);
				break;

			case JOIN_SEMI:
			case JOIN_ANTI:
				partexpr = outer_expr;
				nullable_partexpr = outer_null_expr;
				break;

			case JOIN_LEFT:
				partexpr = outer_expr;
				nullable_partexpr = list_concat(inner_expr,
												outer_null_expr);
				nullable_partexpr = list_concat(nullable_partexpr,
												inner_null_expr);
				break;

			case JOIN_FULL:
				nullable_partexpr = list_concat(outer_expr,
												inner_expr);
				nullable_partexpr = list_concat(nullable_partexpr,
												outer_null_expr);
				nullable_partexpr = list_concat(nullable_partexpr,
												inner_null_expr);
				break;

			default:
				elog(ERROR, "unrecognized join type: %d", (int) jointype);

		}

		joinrel->partexprs[cnt] = partexpr;
		joinrel->nullable_partexprs[cnt] = nullable_partexpr;
	}
}

/*
 * build_child_join_reltarget
 *	  Set up a child-join relation's reltarget from a parent-join relation.
 */
static void
build_child_join_reltarget(PlannerInfo *root,
						   RelOptInfo *parentrel,
						   RelOptInfo *childrel,
						   int nappinfos,
						   AppendRelInfo **appinfos)
{
	/* Build the targetlist */
	childrel->reltarget->exprs = (List *)
		adjust_appendrel_attrs(root,
							   (Node *) parentrel->reltarget->exprs,
							   nappinfos, appinfos);

	/* Set the cost and width fields */
	childrel->reltarget->cost.startup = parentrel->reltarget->cost.startup;
	childrel->reltarget->cost.per_tuple = parentrel->reltarget->cost.per_tuple;
	childrel->reltarget->width = parentrel->reltarget->width;
}

/*
 * Check if the relation can produce grouped paths and return the information
 * it'll need for it. The passed relation is the non-grouped one which has the
 * reltarget already constructed.
 */
RelAggInfo *
create_rel_agg_info(PlannerInfo *root, RelOptInfo *rel)
{
	List	   *gvis;
	List	   *aggregates = NIL;
	bool		found_other_rel_agg;
	ListCell   *lc;
	RelAggInfo *result;
	PathTarget *agg_input;
	PathTarget *target = NULL;
	List	   *grp_exprs_extra = NIL;
	List	   *group_clauses_final;
	int			i;

	/*
	 * The function shouldn't have been called if there's no opportunity for
	 * aggregation push-down.
	 */
	Assert(root->grouped_var_list != NIL);

	/*
	 * The current implementation of aggregation push-down cannot handle
	 * PlaceHolderVar (PHV).
	 *
	 * If we knew that the PHV should be evaluated in this target (and of
	 * course, if its expression matched some Aggref argument), we'd just let
	 * init_grouping_targets add that Aggref. On the other hand, if we knew
	 * that the PHV is evaluated below the current rel, we could ignore it
	 * because the referencing Aggref would take care of propagation of the
	 * value to upper joins.
	 *
	 * The problem is that the same PHV can be evaluated in the target of the
	 * current rel or in that of lower rel --- depending on the input paths.
	 * For example, consider rel->relids = {A, B, C} and if ph_eval_at = {B,
	 * C}. Path "A JOIN (B JOIN C)" implies that the PHV is evaluated by the
	 * "(B JOIN C)", while path "(A JOIN B) JOIN C" evaluates the PHV itself.
	 */
	foreach(lc, rel->reltarget->exprs)
	{
		Expr	   *expr = lfirst(lc);

		if (IsA(expr, PlaceHolderVar))
			return NULL;
	}

	if (IS_SIMPLE_REL(rel))
	{
		RangeTblEntry *rte = root->simple_rte_array[rel->relid];;

		/*
		 * rtekind != RTE_RELATION case is not supported yet.
		 */
		if (rte->rtekind != RTE_RELATION)
			return NULL;
	}

	/* Caller should only pass base relations or joins. */
	Assert(rel->reloptkind == RELOPT_BASEREL ||
		   rel->reloptkind == RELOPT_JOINREL);

	/*
	 * If any outer join can set the attribute value to NULL, the Agg plan
	 * would receive different input at the base rel level.
	 *
	 * XXX For RELOPT_JOINREL, do not return if all the joins that can set any
	 * entry of the grouped target (do we need to postpone this check until
	 * the grouped target is available, and init_grouping_targets take care?)
	 * of this rel to NULL are provably below rel. (It's ok if rel is one of
	 * these joins.)
	 */
	if (bms_overlap(rel->relids, root->nullable_baserels))
		return NULL;

	/*
	 * Use equivalence classes to generate additional grouping expressions for
	 * the current rel. Without these we might not be able to apply
	 * aggregation to the relation result set.
	 *
	 * It's important that create_grouping_expr_grouped_var_infos has
	 * processed the explicit grouping columns by now. If the grouping clause
	 * contains multiple expressions belonging to the same EC, the original
	 * (i.e. not derived) one should be preferred when we build grouping
	 * target for a relation. Otherwise we have a problem when trying to match
	 * target entries to grouping clauses during plan creation, see
	 * get_grouping_expression().
	 */
	gvis = list_copy(root->grouped_var_list);
	foreach(lc, root->grouped_var_list)
	{
		GroupedVarInfo *gvi = lfirst_node(GroupedVarInfo, lc);
		int			relid = -1;

		/* Only interested in grouping expressions. */
		if (IsA(gvi->gvexpr, Aggref))
			continue;

		while ((relid = bms_next_member(rel->relids, relid)) >= 0)
		{
			GroupedVarInfo *gvi_trans;

			gvi_trans = translate_expression_to_rel(root, gvi, relid);
			if (gvi_trans != NULL)
				gvis = lappend(gvis, gvi_trans);
		}
	}

	/*
	 * Check if some aggregates or grouping expressions can be evaluated in
	 * this relation's target, and collect all vars referenced by these
	 * aggregates / grouping expressions;
	 */
	found_other_rel_agg = false;
	foreach(lc, gvis)
	{
		GroupedVarInfo *gvi = lfirst_node(GroupedVarInfo, lc);

		/*
		 * The subset includes gv_eval_at uninitialized, which includes
		 * Aggref.aggstar.
		 */
		if (bms_is_subset(gvi->gv_eval_at, rel->relids))
		{
			/*
			 * init_grouping_targets will handle plain Var grouping
			 * expressions because it needs to look them up in
			 * grouped_var_list anyway.
			 */
			if (IsA(gvi->gvexpr, Var))
				continue;

			/*
			 * Currently, GroupedVarInfo only handles Vars and Aggrefs.
			 */
			Assert(IsA(gvi->gvexpr, Aggref));

			gvi->agg_partial = (Aggref *) copyObject(gvi->gvexpr);
			mark_partial_aggref(gvi->agg_partial, AGGSPLIT_INITIAL_SERIAL);

			/*
			 * Accept the aggregate.
			 */
			aggregates = lappend(aggregates, gvi);
		}
		else if (IsA(gvi->gvexpr, Aggref))
		{
			/*
			 * Remember that there is at least one aggregate expression that
			 * needs something else than this rel.
			 */
			found_other_rel_agg = true;

			/*
			 * This condition effectively terminates creation of the
			 * RelAggInfo, so there's no reason to check the next
			 * GroupedVarInfo.
			 */
			break;
		}
	}

	/*
	 * Grouping makes little sense w/o aggregate function and w/o grouping
	 * expressions.
	 */
	if (aggregates == NIL)
	{
		list_free(gvis);
		return NULL;
	}

	/*
	 * Give up if some other aggregate(s) need relations other than the
	 * current one.
	 *
	 * If the aggregate needs the current rel plus anything else, then the
	 * problem is that grouping of the current relation could make some input
	 * variables unavailable for the "higher aggregate", and it'd also
	 * decrease the number of input rows the "higher aggregate" receives.
	 *
	 * If the aggregate does not even need the current rel, then neither the
	 * current rel nor anything else should be grouped because we do not
	 * support join of two grouped relations.
	 */
	if (found_other_rel_agg)
	{
		list_free(gvis);
		return NULL;
	}

	/*
	 * Create target for grouped paths as well as one for the input paths of
	 * the aggregation paths.
	 */
	target = create_empty_pathtarget();
	agg_input = create_empty_pathtarget();

	/*
	 * Cannot suitable targets for the aggregation push-down be derived?
	 */
	if (!init_grouping_targets(root, rel, target, agg_input, gvis,
							   &grp_exprs_extra))
	{
		list_free(gvis);
		return NULL;
	}

	list_free(gvis);

	/*
	 * Aggregation push-down makes no sense w/o grouping expressions.
	 */
	if ((list_length(target->exprs) + list_length(grp_exprs_extra)) == 0)
		return NULL;

	group_clauses_final = root->parse->groupClause;

	/*
	 * If the aggregation target should have extra grouping expressions (in
	 * order to emit input vars for join conditions), add them now. This step
	 * includes assignment of tleSortGroupRef's which we can generate now.
	 */
	if (list_length(grp_exprs_extra) > 0)
	{
		Index		sortgroupref;

		/*
		 * We'll have to add some clauses, but query group clause must be
		 * preserved.
		 */
		group_clauses_final = list_copy(group_clauses_final);

		/*
		 * Always start at root->max_sortgroupref. The extra grouping
		 * expressions aren't used during the final aggregation, so the
		 * sortgroupref values don't need to be unique across the query. Thus
		 * we don't have to increase root->max_sortgroupref, which makes
		 * recognition of the extra grouping expressions pretty easy.
		 */
		sortgroupref = root->max_sortgroupref;

		/*
		 * Generate the SortGroupClause's and add the expressions to the
		 * target.
		 */
		foreach(lc, grp_exprs_extra)
		{
			Var		   *var = lfirst_node(Var, lc);
			SortGroupClause *cl = makeNode(SortGroupClause);

			/*
			 * Initialize the SortGroupClause.
			 *
			 * As the final aggregation will not use this grouping expression,
			 * we don't care whether sortop is < or >. The value of
			 * nulls_first should not matter for the same reason.
			 */
			cl->tleSortGroupRef = ++sortgroupref;
			get_sort_group_operators(var->vartype,
									 false, true, false,
									 &cl->sortop, &cl->eqop, NULL,
									 &cl->hashable);
			group_clauses_final = lappend(group_clauses_final, cl);
			add_column_to_pathtarget(target, (Expr *) var,
									 cl->tleSortGroupRef);

			/*
			 * The aggregation input target must emit this var too.
			 */
			add_column_to_pathtarget(agg_input, (Expr *) var,
									 cl->tleSortGroupRef);
		}
	}

	/*
	 * Add aggregates to the grouping target.
	 */
	foreach(lc, aggregates)
	{
		GroupedVarInfo *gvi;

		gvi = lfirst_node(GroupedVarInfo, lc);
		add_column_to_pathtarget(target, (Expr *) gvi->agg_partial,
								 gvi->sortgroupref);
	}

	/*
	 * Build a list of grouping expressions and a list of the corresponding
	 * SortGroupClauses.
	 */
	i = 0;
	result = makeNode(RelAggInfo);
	foreach(lc, target->exprs)
	{
		Index		sortgroupref = 0;
		SortGroupClause *cl;
		Expr	   *texpr;

		texpr = (Expr *) lfirst(lc);

		if (IsA(texpr, Aggref))
		{
			/*
			 * Once we see Aggref, no grouping expressions should follow.
			 */
			break;
		}

		/*
		 * Find the clause by sortgroupref.
		 */
		sortgroupref = target->sortgrouprefs[i++];

		/*
		 * Besides being an aggregate, the target expression should have no
		 * other reason then being a column of a relation functionally
		 * dependent on the GROUP BY clause. So it's not actually a grouping
		 * column.
		 */
		if (sortgroupref == 0)
			continue;

		/*
		 * group_clause_final contains the "local" clauses, so this search
		 * should succeed.
		 */
		cl = get_sortgroupref_clause(sortgroupref, group_clauses_final);

		result->group_clauses = list_append_unique(result->group_clauses,
												   cl);

		/*
		 * Add only unique clauses because of joins (both sides of a join can
		 * point at the same grouping clause). XXX Is it worth adding a bool
		 * argument indicating that we're dealing with join right now?
		 */
		result->group_exprs = list_append_unique(result->group_exprs,
												 texpr);
	}

	/*
	 * Since neither target nor agg_input is supposed to be identical to the
	 * source reltarget, compute the width and cost again.
	 *
	 * target does not yet contain aggregates, but these will be accounted by
	 * AggPath.
	 */
	set_pathtarget_cost_width(root, target);
	set_pathtarget_cost_width(root, agg_input);

	result->relids = bms_copy(rel->relids);
	result->target = target;
	result->agg_input = agg_input;

	/* Finally collect the aggregates. */
	while (lc != NULL)
	{
		Aggref	   *aggref = lfirst_node(Aggref, lc);

		/*
		 * Partial aggregation is what the grouped paths should do.
		 */
		result->agg_exprs = lappend(result->agg_exprs, aggref);
		lc = lnext(lc);
	}

	/* The "input_rows" field should be set by caller. */
	return result;
}

/*
 * Initialize target for grouped paths (target) as well as a target for paths
 * that generate input for aggregation (agg_input).
 *
 * group_exprs_extra_p receives a list of Var nodes for which we need to
 * construct SortGroupClause. Those vars will then be used as additional
 * grouping expressions, for the sake of join clauses.
 *
 * gvis a list of GroupedVarInfo's possibly useful for rel.
 *
 * Return true iff the targets could be initialized.
 */
static bool
init_grouping_targets(PlannerInfo *root, RelOptInfo *rel,
					  PathTarget *target, PathTarget *agg_input,
					  List *gvis, List **group_exprs_extra_p)
{
	ListCell   *lc;
	List	   *possibly_dependent = NIL;
	Var		   *tvar;

	foreach(lc, rel->reltarget->exprs)
	{
		Index		sortgroupref;

		/*
		 * Given that PlaceHolderVar currently prevents us from doing
		 * aggregation push-down, the source target cannot contain anything
		 * more complex than a Var.
		 */
		tvar = lfirst_node(Var, lc);

		sortgroupref = get_expression_sortgroupref((Expr *) tvar, gvis);
		if (sortgroupref > 0)
		{
			/*
			 * If the target expression can be used as the grouping key, we
			 * don't have to worry whether it can be emitted by the AggPath
			 * pushed down to relation / join.
			 */
			add_column_to_pathtarget(target, (Expr *) tvar, sortgroupref);

			/*
			 * As for agg_input, add the original expression but set
			 * sortgroupref in addition.
			 */
			add_column_to_pathtarget(agg_input, (Expr *) tvar, sortgroupref);
		}
		else
		{
			if (is_var_needed_by_join(root, tvar, rel))
			{
				/*
				 * The variable is needed for a join, however it's neither in
				 * the GROUP BY clause nor can it be derived from it using EC.
				 * (Otherwise it would have to be added to the targets above.)
				 * We need to construct special SortGroupClause for that
				 * variable.
				 *
				 * Note that its tleSortGroupRef needs to be unique within
				 * agg_input, so we need to postpone creation of the
				 * SortGroupClause's until we're done with the iteration of
				 * rel->reltarget->exprs. Also it makes sense for the caller
				 * to do some more check before it starts to create those
				 * SortGroupClause's.
				 */
				*group_exprs_extra_p = lappend(*group_exprs_extra_p, tvar);
			}
			else if (is_var_in_aggref_only(root, tvar))
			{
				/*
				 * Another reason we might need this variable is that some
				 * aggregate pushed down to this relation references it. In such a
				 * case, add that var to agg_input, but not to "target". However,
				 * if the aggregate is not the only reason for the var to be in
				 * the target, some more checks need to be performed below.
				 */
				add_new_column_to_pathtarget(agg_input, (Expr *) tvar);
			}
			else
			{
				/*
				 * The Var can be functionally dependent on another expression
				 * of the target, but we cannot check until the other
				 * expressions are in the target.
				 */
				possibly_dependent = lappend(possibly_dependent, tvar);
			}
		}
	}

	/*
	 * Now we can check whether the expression is functionally dependent on
	 * another one.
	 */
	foreach(lc, possibly_dependent)
	{
		List	   *deps = NIL;
		RangeTblEntry *rte;

		tvar = lfirst_node(Var, lc);
		rte = root->simple_rte_array[tvar->varno];

		/*
		 * Check if the Var can be in the grouping key even though it's not
		 * mentioned by the GROUP BY clause (and could not be derived using
		 * ECs).
		 */
		if (check_functional_grouping(rte->relid, tvar->varno,
									  tvar->varlevelsup,
									  target->exprs, &deps))
		{
			/*
			 * The var shouldn't be actually used for grouping key evaluation
			 * (instead, the one this depends on will be), so sortgroupref
			 * should not be important.
			 */
			add_new_column_to_pathtarget(target, (Expr *) tvar);
			add_new_column_to_pathtarget(agg_input, (Expr *) tvar);
		}
		else
		{
			/*
			 * As long as the query is semantically correct, arriving here
			 * means that the var is referenced by a generic grouping
			 * expression but not referenced by any join.
			 *
			 * If the aggregate push-down will support generic grouping
			 * expression sin the future, create_rel_agg_info() will have to
			 * add this variable to "agg_input" target and also add the whole
			 * generic expression to "target".
			 */
			return false;
		}
	}

	return true;
}

/*
 * Check whether given variable appears in Aggref(s) which we consider usable
 * at relation / join level, and only in the Aggref(s).
 */
static bool
is_var_in_aggref_only(PlannerInfo *root, Var *var)
{
	ListCell   *lc;
	bool		found = false;

	foreach(lc, root->grouped_var_list)
	{
		GroupedVarInfo *gvi = lfirst_node(GroupedVarInfo, lc);
		ListCell   *lc2;
		List	   *vars;

		if (!IsA(gvi->gvexpr, Aggref))
			continue;

		if (!bms_is_member(var->varno, gvi->gv_eval_at))
			continue;

		/*
		 * XXX Consider some sort of caching.
		 */
		vars = pull_var_clause((Node *) gvi->gvexpr, PVC_RECURSE_AGGREGATES);
		foreach(lc2, vars)
		{
			Var		   *v = lfirst_node(Var, lc2);

			if (equal(v, var))
			{
				found = true;
				break;
			}

		}
		list_free(vars);

		if (found)
			break;
	}

	/* No aggregate references the Var? */
	if (!found)
		return false;

	/* Does the Var appear in the target outside aggregates? */
	found = false;
	foreach(lc, root->processed_tlist)
	{
		TargetEntry *te = lfirst_node(TargetEntry, lc);

		if (IsA(te->expr, Aggref))
			continue;

		if (equal(te->expr, var))
			return false;

	}

	/* The Var is in aggregate(s) and only there. */
	return true;
}

/*
 * Check if given variable is needed by joins above the current rel?
 *
 * Consider pushing the aggregate avg(b.y) down to relation "b" for the
 * following query:
 *
 *    SELECT a.i, avg(b.y)
 *    FROM a JOIN b ON b.j = a.i
 *    GROUP BY a.i;
 *
 * If we aggregate the "b" relation alone, the column "b.j" needs to be used
 * as the grouping key because otherwise it cannot find its way to the input
 * of the join expression.
 */
static bool
is_var_needed_by_join(PlannerInfo *root, Var *var, RelOptInfo *rel)
{
	Relids		relids_no_top;
	int			ndx;
	RelOptInfo *baserel;

	/*
	 * The relids we're not interested in do include 0, which is the top-level
	 * targetlist. The only reason for relids to contain 0 should be that
	 * arg_var is referenced either by aggregate or by grouping expression,
	 * but right now we're interested in the *other* reasons. (As soon
	 * aggregation is pushed down, the aggregates in the query targetlist no
	 * longer need direct reference to arg_var anyway.)
	 */

	relids_no_top = bms_copy(rel->relids);
	bms_add_member(relids_no_top, 0);

	baserel = find_base_rel(root, var->varno);
	ndx = var->varattno - baserel->min_attr;
	if (bms_nonempty_difference(baserel->attr_needed[ndx],
								relids_no_top))
		return true;

	return false;
}
