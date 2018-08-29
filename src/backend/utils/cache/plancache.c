/*-------------------------------------------------------------------------
 *
 * plancache.c
 *	  Plan cache management.
 *
 * The plan cache manager has two principal responsibilities: deciding when
 * to use a generic plan versus a custom (parameter-value-specific) plan,
 * and tracking whether cached plans need to be invalidated because of schema
 * changes in the objects they depend on.
 *
 * The logic for choosing generic or custom plans is in choose_custom_plan,
 * which see for comments.
 *
 * Cache invalidation is driven off sinval events.	Any CachedPlanSource
 * that matches the event is marked invalid, as is its generic CachedPlan
 * if it has one.  When (and if) the next demand for a cached plan occurs,
 * parse analysis and rewrite is repeated to build a new valid query tree,
 * and then planning is performed as normal.
 *
 * Note that if the sinval was a result of user DDL actions, parse analysis
 * could throw an error, for example if a column referenced by the query is
 * no longer present.  The creator of a cached plan can specify whether it
 * is allowable for the query to change output tupdesc on replan (this
 * could happen with "SELECT *" for example) --- if so, it's up to the
 * caller to notice changes and cope with them.
 *
 * Currently, we track exactly the dependencies of plans on relations and
 * user-defined functions.	On relcache invalidation events or pg_proc
 * syscache invalidation events, we invalidate just those plans that depend
 * on the particular object being modified.  (Note: this scheme assumes
 * that any table modification that requires replanning will generate a
 * relcache inval event.)  We also watch for inval events on certain other
 * system catalogs, such as pg_namespace; but for them, our response is
 * just to invalidate all plans.  We expect updates on those catalogs to
 * be infrequent enough that more-detailed tracking is not worth the effort.
 *
 *
 * Portions Copyright (c) 1996-2012, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/plancache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/transam.h"
#include "catalog/namespace.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/planmain.h"
#include "optimizer/prep.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "storage/lmgr.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"


/*
 * This is the head of the backend's list of "saved" CachedPlanSources (i.e.,
 * those that are in long-lived storage and are examined for sinval events).
 * We thread the structs manually instead of using List cells so that we can
 * guarantee to save a CachedPlanSource without error.
 */
static CachedPlanSource *first_saved_plan = NULL;

static void ReleaseGenericPlan(CachedPlanSource *plansource);
static List *RevalidateCachedQuery(CachedPlanSource *plansource, IntoClause *intoClause);
static bool CheckCachedPlan(CachedPlanSource *plansource);
static CachedPlan *BuildCachedPlan(CachedPlanSource *plansource, List *qlist,
				ParamListInfo boundParams, IntoClause *intoClause);
static bool choose_custom_plan(CachedPlanSource *plansource,
				   ParamListInfo boundParams,
				   IntoClause *intoClause);
static double cached_plan_cost(CachedPlan *plan);
static void AcquireExecutorLocks(List *stmt_list, bool acquire);
static void AcquirePlannerLocks(List *stmt_list, bool acquire);
static void ScanQueryForLocks(Query *parsetree, bool acquire);
static bool ScanQueryWalker(Node *node, bool *acquire);
static bool plan_list_is_transient(List *stmt_list);
static bool plan_list_is_oneoff(List *stmt_list);
static TupleDesc PlanCacheComputeResultDesc(List *stmt_list);
static void PlanCacheRelCallback(Datum arg, Oid relid);
static void PlanCacheFuncCallback(Datum arg, int cacheid, uint32 hashvalue);
static void PlanCacheSysCallback(Datum arg, int cacheid, uint32 hashvalue);


/*
 * InitPlanCache: initialize module during InitPostgres.
 *
 * All we need to do is hook into inval.c's callback lists.
 */
void
InitPlanCache(void)
{
	CacheRegisterRelcacheCallback(PlanCacheRelCallback, (Datum) 0);
	CacheRegisterSyscacheCallback(PROCOID, PlanCacheFuncCallback, (Datum) 0);
	CacheRegisterSyscacheCallback(NAMESPACEOID, PlanCacheSysCallback, (Datum) 0);
	CacheRegisterSyscacheCallback(OPEROID, PlanCacheSysCallback, (Datum) 0);
	CacheRegisterSyscacheCallback(AMOPOPID, PlanCacheSysCallback, (Datum) 0);
}

/*
 * CreateCachedPlan: initially create a plan cache entry.
 *
 * Creation of a cached plan is divided into two steps, CreateCachedPlan and
 * CompleteCachedPlan.	CreateCachedPlan should be called after running the
 * query through raw_parser, but before doing parse analysis and rewrite;
 * CompleteCachedPlan is called after that.  The reason for this arrangement
 * is that it can save one round of copying of the raw parse tree, since
 * the parser will normally scribble on the raw parse tree.  Callers would
 * otherwise need to make an extra copy of the parse tree to ensure they
 * still had a clean copy to present at plan cache creation time.
 *
 * All arguments presented to CreateCachedPlan are copied into a memory
 * context created as a child of the call-time CurrentMemoryContext, which
 * should be a reasonably short-lived working context that will go away in
 * event of an error.  This ensures that the cached plan data structure will
 * likewise disappear if an error occurs before we have fully constructed it.
 * Once constructed, the cached plan can be made longer-lived, if needed,
 * by calling SaveCachedPlan.
 *
 * raw_parse_tree: output of raw_parser(), or NULL if empty query
 * query_string: original query text
 * commandTag: compile-time-constant tag for query, or NULL if empty query
 * sourceTag: GPDB specific.
 */
CachedPlanSource *
CreateCachedPlan(Node *raw_parse_tree,
				 const char *query_string,
				 const char *commandTag)
{
	CachedPlanSource *plansource;
	MemoryContext source_context;
	MemoryContext oldcxt;

	Assert(query_string != NULL);		/* required as of 8.4 */

	/*
	 * Make a dedicated memory context for the CachedPlanSource and its
	 * permanent subsidiary data.  It's probably not going to be large, but
	 * just in case, use the default maxsize parameter.  Initially it's a
	 * child of the caller's context (which we assume to be transient), so
	 * that it will be cleaned up on error.
	 */
	source_context = AllocSetContextCreate(CurrentMemoryContext,
										   "CachedPlanSource",
										   ALLOCSET_SMALL_MINSIZE,
										   ALLOCSET_SMALL_INITSIZE,
										   ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Create and fill the CachedPlanSource struct within the new context.
	 * Most fields are just left empty for the moment.
	 */
	oldcxt = MemoryContextSwitchTo(source_context);

	plansource = (CachedPlanSource *) palloc0(sizeof(CachedPlanSource));
	plansource->magic = CACHEDPLANSOURCE_MAGIC;
	plansource->raw_parse_tree = copyObject(raw_parse_tree);
	plansource->query_string = pstrdup(query_string);
	/* sourceTag is filled in CompleteCachedPlan(). */
	plansource->commandTag = commandTag;
	plansource->param_types = NULL;
	plansource->num_params = 0;
	plansource->parserSetup = NULL;
	plansource->parserSetupArg = NULL;
	plansource->cursor_options = 0;
	plansource->fixed_result = false;
	plansource->resultDesc = NULL;
	plansource->search_path = NULL;
	plansource->context = source_context;
	plansource->query_list = NIL;
	plansource->relationOids = NIL;
	plansource->invalItems = NIL;
	plansource->query_context = NULL;
	plansource->gplan = NULL;
	plansource->is_complete = false;
	plansource->is_saved = false;
	plansource->is_valid = false;
	plansource->generation = 0;
	plansource->next_saved = NULL;
	plansource->generic_cost = -1;
	plansource->total_custom_cost = 0;
	plansource->num_custom_plans = 0;

	MemoryContextSwitchTo(oldcxt);

	return plansource;
}

/*
 * CompleteCachedPlan: second step of creating a plan cache entry.
 *
 * Pass in the analyzed-and-rewritten form of the query, as well as the
 * required subsidiary data about parameters and such.	All passed values will
 * be copied into the CachedPlanSource's memory, except as specified below.
 * After this is called, GetCachedPlan can be called to obtain a plan, and
 * optionally the CachedPlanSource can be saved using SaveCachedPlan.
 *
 * If querytree_context is not NULL, the querytree_list must be stored in that
 * context (but the other parameters need not be).	The querytree_list is not
 * copied, rather the given context is kept as the initial query_context of
 * the CachedPlanSource.  (It should have been created as a child of the
 * caller's working memory context, but it will now be reparented to belong
 * to the CachedPlanSource.)  The querytree_context is normally the context in
 * which the caller did raw parsing and parse analysis.  This approach saves
 * one tree copying step compared to passing NULL, but leaves lots of extra
 * cruft in the query_context, namely whatever extraneous stuff parse analysis
 * created, as well as whatever went unused from the raw parse tree.  Using
 * this option is a space-for-time tradeoff that is appropriate if the
 * CachedPlanSource is not expected to survive long.
 *
 * plancache.c cannot know how to copy the data referenced by parserSetupArg,
 * and it would often be inappropriate to do so anyway.  When using that
 * option, it is caller's responsibility that the referenced data remains
 * valid for as long as the CachedPlanSource exists.
 *
 * plansource: structure returned by CreateCachedPlan
 * querytree_list: analyzed-and-rewritten form of query (list of Query nodes)
 * querytree_context: memory context containing querytree_list,
 *					  or NULL to copy querytree_list into a fresh context
 * param_types: array of fixed parameter type OIDs, or NULL if none
 * num_params: number of fixed parameters
 * parserSetup: alternate method for handling query parameters
 * parserSetupArg: data to pass to parserSetup
 * cursor_options: options bitmask to pass to planner
 * fixed_result: TRUE to disallow future changes in query's result tupdesc
 */
void
CompleteCachedPlan(CachedPlanSource *plansource,
				   List *querytree_list,
				   MemoryContext querytree_context,
				   NodeTag sourceTag,
				   Oid *param_types,
				   int num_params,
				   ParserSetupHook parserSetup,
				   void *parserSetupArg,
				   int cursor_options,
				   bool fixed_result)
{
	MemoryContext source_context = plansource->context;
	MemoryContext oldcxt = CurrentMemoryContext;

	/* Assert caller is doing things in a sane order */
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(!plansource->is_complete);

	/*
	 * If caller supplied a querytree_context, reparent it underneath the
	 * CachedPlanSource's context; otherwise, create a suitable context and
	 * copy the querytree_list into it.
	 */
	if (querytree_context != NULL)
	{
		MemoryContextSetParent(querytree_context, source_context);
		MemoryContextSwitchTo(querytree_context);
	}
	else
	{
		/* Again, it's a good bet the querytree_context can be small */
		querytree_context = AllocSetContextCreate(source_context,
												  "CachedPlanQuery",
												  ALLOCSET_SMALL_MINSIZE,
												  ALLOCSET_SMALL_INITSIZE,
												  ALLOCSET_DEFAULT_MAXSIZE);
		MemoryContextSwitchTo(querytree_context);
		querytree_list = (List *) copyObject(querytree_list);
	}

	plansource->query_context = querytree_context;
	plansource->query_list = querytree_list;

	/*
	 * Use the planner machinery to extract dependencies.  Data is saved in
	 * query_context.  (We assume that not a lot of extra cruft is created by
	 * this call.)
	 */
	extract_query_dependencies((Node *) querytree_list,
							   &plansource->relationOids,
							   &plansource->invalItems);

	/*
	 * Save the final parameter types (or other parameter specification data)
	 * into the source_context, as well as our other parameters.  Also save
	 * the result tuple descriptor.
	 */
	MemoryContextSwitchTo(source_context);

	if (num_params > 0)
	{
		plansource->param_types = (Oid *) palloc(num_params * sizeof(Oid));
		memcpy(plansource->param_types, param_types, num_params * sizeof(Oid));
	}
	else
		plansource->param_types = NULL;
	plansource->sourceTag = sourceTag;
	plansource->num_params = num_params;
	plansource->parserSetup = parserSetup;
	plansource->parserSetupArg = parserSetupArg;
	plansource->cursor_options = cursor_options;
	plansource->fixed_result = fixed_result;
	plansource->resultDesc = PlanCacheComputeResultDesc(querytree_list);

	MemoryContextSwitchTo(oldcxt);

	/*
	 * Fetch current search_path into dedicated context, but do any
	 * recalculation work required in caller's context.
	 */
	plansource->search_path = GetOverrideSearchPath(source_context);

	plansource->is_complete = true;
	plansource->is_valid = true;
}

/*
 * SaveCachedPlan: save a cached plan permanently
 *
 * This function moves the cached plan underneath CacheMemoryContext (making
 * it live for the life of the backend, unless explicitly dropped), and adds
 * it to the list of cached plans that are checked for invalidation when an
 * sinval event occurs.
 *
 * This is guaranteed not to throw error; callers typically depend on that
 * since this is called just before or just after adding a pointer to the
 * CachedPlanSource to some permanent data structure of their own.	Up until
 * this is done, a CachedPlanSource is just transient data that will go away
 * automatically on transaction abort.
 */
void
SaveCachedPlan(CachedPlanSource *plansource)
{
	/* Assert caller is doing things in a sane order */
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(plansource->is_complete);
	Assert(!plansource->is_saved);

	/*
	 * In typical use, this function would be called before generating any
	 * plans from the CachedPlanSource.  If there is a generic plan, moving it
	 * into CacheMemoryContext would be pretty risky since it's unclear
	 * whether the caller has taken suitable care with making references
	 * long-lived.	Best thing to do seems to be to discard the plan.
	 */
	ReleaseGenericPlan(plansource);

	/*
	 * Reparent the source memory context under CacheMemoryContext so that it
	 * will live indefinitely.	The query_context follows along since it's
	 * already a child of the other one.
	 */
	MemoryContextSetParent(plansource->context, CacheMemoryContext);

	/*
	 * Add the entry to the global list of cached plans.
	 */
	plansource->next_saved = first_saved_plan;
	first_saved_plan = plansource;

	plansource->is_saved = true;
}

/*
 * DropCachedPlan: destroy a cached plan.
 *
 * Actually this only destroys the CachedPlanSource: any referenced CachedPlan
 * is released, but not destroyed until its refcount goes to zero.	That
 * handles the situation where DropCachedPlan is called while the plan is
 * still in use.
 */
void
DropCachedPlan(CachedPlanSource *plansource)
{
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

	/* If it's been saved, remove it from the list */
	if (plansource->is_saved)
	{
		if (first_saved_plan == plansource)
			first_saved_plan = plansource->next_saved;
		else
		{
			CachedPlanSource *psrc;

			for (psrc = first_saved_plan; psrc; psrc = psrc->next_saved)
			{
				if (psrc->next_saved == plansource)
				{
					psrc->next_saved = plansource->next_saved;
					break;
				}
			}
		}
		plansource->is_saved = false;
	}

	/* Decrement generic CachePlan's refcount and drop if no longer needed */
	ReleaseGenericPlan(plansource);

	/*
	 * Remove the CachedPlanSource and all subsidiary data (including the
	 * query_context if any).
	 */
	MemoryContextDelete(plansource->context);
}

/*
 * ReleaseGenericPlan: release a CachedPlanSource's generic plan, if any.
 */
static void
ReleaseGenericPlan(CachedPlanSource *plansource)
{
	/* Be paranoid about the possibility that ReleaseCachedPlan fails */
	if (plansource->gplan)
	{
		CachedPlan *plan = plansource->gplan;

		Assert(plan->magic == CACHEDPLAN_MAGIC);
		plansource->gplan = NULL;
		ReleaseCachedPlan(plan, false);
	}
}

/*
 * RevalidateCachedQuery: ensure validity of analyzed-and-rewritten query tree.
 *
 * What we do here is re-acquire locks and redo parse analysis if necessary.
 * On return, the query_list is valid and we have sufficient locks to begin
 * planning.
 *
 * If any parse analysis activity is required, the caller's memory context is
 * used for that work.
 *
 * The result value is the transient analyzed-and-rewritten query tree if we
 * had to do re-analysis, and NIL otherwise.  (This is returned just to save
 * a tree copying step in a subsequent BuildCachedPlan call.)
 *
 * GPDB: See GetCachedPlan() for why intoClause is added here.
 */
static List *
RevalidateCachedQuery(CachedPlanSource *plansource, IntoClause *intoClause)
{
	bool		snapshot_set;
	Node	   *rawtree;
	List	   *tlist;			/* transient query-tree list */
	List	   *qlist;			/* permanent query-tree list */
	TupleDesc	resultDesc;
	MemoryContext querytree_context;
	MemoryContext oldcxt;

	/*
	 * If the query is currently valid, acquire locks on the referenced
	 * objects; then check again.  We need to do it this way to cover the race
	 * condition that an invalidation message arrives before we get the locks.
	 */
	if (plansource->is_valid && intoClause == NULL)
	{
		AcquirePlannerLocks(plansource->query_list, true);

		/*
		 * By now, if any invalidation has happened, the inval callback
		 * functions will have marked the query invalid.
		 */
		if (plansource->is_valid && intoClause == NULL)
		{
			/* Successfully revalidated and locked the query. */
			return NIL;
		}

		/* Ooops, the race case happened.  Release useless locks. */
		AcquirePlannerLocks(plansource->query_list, false);
	}

	/*
	 * Discard the no-longer-useful query tree.  (Note: we don't want to do
	 * this any earlier, else we'd not have been able to release locks
	 * correctly in the race condition case.)
	 */
	plansource->is_valid = false;
	plansource->query_list = NIL;
	plansource->relationOids = NIL;
	plansource->invalItems = NIL;

	/*
	 * Free the query_context.	We don't really expect MemoryContextDelete to
	 * fail, but just in case, make sure the CachedPlanSource is left in a
	 * reasonably sane state.  (The generic plan won't get unlinked yet, but
	 * that's acceptable.)
	 */
	if (plansource->query_context)
	{
		MemoryContext qcxt = plansource->query_context;

		plansource->query_context = NULL;
		MemoryContextDelete(qcxt);
	}

	/* Drop the generic plan reference if any */
	ReleaseGenericPlan(plansource);

	/*
	 * Now re-do parse analysis and rewrite.  This not incidentally acquires
	 * the locks we need to do planning safely.
	 */
	Assert(plansource->is_complete);

	/*
	 * Restore the search_path that was in use when the plan was made. See
	 * comments for PushOverrideSearchPath about limitations of this.
	 *
	 * (XXX is there anything else we really need to restore?)
	 */
	PushOverrideSearchPath(plansource->search_path);

	/*
	 * If a snapshot is already set (the normal case), we can just use that
	 * for parsing/planning.  But if it isn't, install one.  Note: no point in
	 * checking whether parse analysis requires a snapshot; utility commands
	 * don't have invalidatable plans, so we'd not get here for such a
	 * command.
	 */
	snapshot_set = false;
	if (!ActiveSnapshotSet())
	{
		PushActiveSnapshot(GetTransactionSnapshot());
		snapshot_set = true;
	}

	rawtree = copyObject(plansource->raw_parse_tree);

	/*
	 * Run parse analysis and rule rewriting.  The parser tends to scribble on
	 * its input, so we must copy the raw parse tree to prevent corruption of
	 * the cache.
	 */
	if (rawtree == NULL)
		tlist = NIL;
	else if (plansource->parserSetup != NULL)
		tlist = pg_analyze_and_rewrite_params(rawtree,
											  plansource->query_string,
											  plansource->parserSetup,
											  plansource->parserSetupArg);
	else
		tlist = pg_analyze_and_rewrite(rawtree,
									   plansource->query_string,
									   plansource->param_types,
									   plansource->num_params);

	/* GPDB: For CTAS query, set its isCTAS to be true */
	if (intoClause)
	{
		Assert(list_length(tlist) == 1);
		Query *query = (Query *) linitial(tlist);
		query->isCTAS = true;
	}

	/* Release snapshot if we got one */
	if (snapshot_set)
		PopActiveSnapshot();

	/* Now we can restore current search path */
	PopOverrideSearchPath();

	/*
	 * Check or update the result tupdesc.	XXX should we use a weaker
	 * condition than equalTupleDescs() here?
	 *
	 * We assume the parameter types didn't change from the first time, so no
	 * need to update that.
	 */
	resultDesc = PlanCacheComputeResultDesc(tlist);
	if (resultDesc == NULL && plansource->resultDesc == NULL)
	{
		/* OK, doesn't return tuples */
	}
	else if (intoClause != NULL)
	{
		/* OK */
	}
	else if (resultDesc == NULL || plansource->resultDesc == NULL ||
			 !equalTupleDescs(resultDesc, plansource->resultDesc, true))
	{
		/* can we give a better error message? */
		if (plansource->fixed_result)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cached plan must not change result type")));
		oldcxt = MemoryContextSwitchTo(plansource->context);
		if (resultDesc)
			resultDesc = CreateTupleDescCopy(resultDesc);
		if (plansource->resultDesc)
			FreeTupleDesc(plansource->resultDesc);
		plansource->resultDesc = resultDesc;
		MemoryContextSwitchTo(oldcxt);
	}

	/*
	 * Allocate new query_context and copy the completed querytree into it.
	 * It's transient until we complete the copying and dependency extraction.
	 */
	querytree_context = AllocSetContextCreate(CurrentMemoryContext,
											  "CachedPlanQuery",
											  ALLOCSET_SMALL_MINSIZE,
											  ALLOCSET_SMALL_INITSIZE,
											  ALLOCSET_DEFAULT_MAXSIZE);
	oldcxt = MemoryContextSwitchTo(querytree_context);

	qlist = (List *) copyObject(tlist);

	/*
	 * Use the planner machinery to extract dependencies.  Data is saved in
	 * query_context.  (We assume that not a lot of extra cruft is created by
	 * this call.)
	 */
	extract_query_dependencies((Node *) qlist,
							   &plansource->relationOids,
							   &plansource->invalItems);

	MemoryContextSwitchTo(oldcxt);

	/* Now reparent the finished query_context and save the links */
	MemoryContextSetParent(querytree_context, plansource->context);

	plansource->query_context = querytree_context;
	plansource->query_list = qlist;

	/*
	 * Note: we do not reset generic_cost or total_custom_cost, although we
	 * could choose to do so.  If the DDL or statistics change that prompted
	 * the invalidation meant a significant change in the cost estimates, it
	 * would be better to reset those variables and start fresh; but often it
	 * doesn't, and we're better retaining our hard-won knowledge about the
	 * relative costs.
	 */

	plansource->is_valid = true;

	/* Return transient copy of querytrees for possible use in planning */
	return tlist;
}

/*
 * CheckCachedPlan: see if the CachedPlanSource's generic plan is valid.
 *
 * Caller must have already called RevalidateCachedQuery to verify that the
 * querytree is up to date.
 *
 * On a "true" return, we have acquired the locks needed to run the plan.
 * (We must do this for the "true" result to be race-condition-free.)
 */
static bool
CheckCachedPlan(CachedPlanSource *plansource)
{
	CachedPlan *plan = plansource->gplan;

	/* Assert that caller checked the querytree */
	Assert(plansource->is_valid);

	/* If there's no generic plan, just say "false" */
	if (!plan)
		return false;

	Assert(plan->magic == CACHEDPLAN_MAGIC);

	/*
	 * If it appears valid, acquire locks and recheck; this is much the same
	 * logic as in RevalidateCachedQuery, but for a plan.
	 */
	if (plan->is_valid)
	{
		/*
		 * Plan must have positive refcount because it is referenced by
		 * plansource; so no need to fear it disappears under us here.
		 */
		Assert(plan->refcount > 0);

		AcquireExecutorLocks(plan->stmt_list, true);

		/*
		 * If plan was transient, check to see if TransactionXmin has
		 * advanced, and if so invalidate it.
		 */
		if (plan->is_valid &&
			TransactionIdIsValid(plan->saved_xmin) &&
			!TransactionIdEquals(plan->saved_xmin, TransactionXmin))
			plan->is_valid = false;

		/*
		 * By now, if any invalidation has happened, the inval callback
		 * functions will have marked the plan invalid.
		 */
		if (plan->is_valid)
		{
			/* Successfully revalidated and locked the query. */
			return true;
		}

		/* Ooops, the race case happened.  Release useless locks. */
		AcquireExecutorLocks(plan->stmt_list, false);
	}

	/*
	 * Plan has been invalidated, so unlink it from the parent and release it.
	 */
	ReleaseGenericPlan(plansource);

	return false;
}

/*
 * BuildCachedPlan: construct a new CachedPlan from a CachedPlanSource.
 *
 * qlist should be the result value from a previous RevalidateCachedQuery,
 * or it can be set to NIL if we need to re-copy the plansource's query_list.
 *
 * To build a generic, parameter-value-independent plan, pass NULL for
 * boundParams.  To build a custom plan, pass the actual parameter values via
 * boundParams.  For best effect, the PARAM_FLAG_CONST flag should be set on
 * each parameter value; otherwise the planner will treat the value as a
 * hint rather than a hard constant.
 *
 * Planning work is done in the caller's memory context.  The finished plan
 * is in a child memory context, which typically should get reparented.
 *
 * GPDB: See GetCachedPlan() for why intoClause is added here.
 */
static CachedPlan *
BuildCachedPlan(CachedPlanSource *plansource, List *qlist,
				ParamListInfo boundParams, IntoClause *intoClause)
{
	CachedPlan *plan;
	List	   *plist;
	bool		snapshot_set;
	bool		spi_pushed;
	MemoryContext plan_context;
	MemoryContext oldcxt;

	/*
	 * Normally the querytree should be valid already, but if it's not,
	 * rebuild it.
	 *
	 * NOTE: GetCachedPlan should have called RevalidateCachedQuery first, so
	 * we ought to be holding sufficient locks to prevent any invalidation.
	 * However, if we're building a custom plan after having built and
	 * rejected a generic plan, it's possible to reach here with is_valid
	 * false due to an invalidation while making the generic plan.	In theory
	 * the invalidation must be a false positive, perhaps a consequence of an
	 * sinval reset event or the CLOBBER_CACHE_ALWAYS debug code.  But for
	 * safety, let's treat it as real and redo the RevalidateCachedQuery call.
	 */
	if (!plansource->is_valid)
		qlist = RevalidateCachedQuery(plansource, intoClause);

	/*
	 * If we don't already have a copy of the querytree list that can be
	 * scribbled on by the planner, make one.
	 */
	if (qlist == NIL)
		qlist = (List *) copyObject(plansource->query_list);

	/*
	 * Restore the search_path that was in use when the plan was made. See
	 * comments for PushOverrideSearchPath about limitations of this.
	 *
	 * (XXX is there anything else we really need to restore?)
	 *
	 * Note: it's a bit annoying to do this and snapshot-setting twice in the
	 * case where we have to do both re-analysis and re-planning.  However,
	 * until there's some evidence that the cost is actually meaningful
	 * compared to parse analysis + planning, I'm not going to contort the
	 * code enough to avoid that.
	 */
	PushOverrideSearchPath(plansource->search_path);

	/*
	 * If a snapshot is already set (the normal case), we can just use that
	 * for planning.  But if it isn't, and we need one, install one.
	 */
	snapshot_set = false;
	if (!ActiveSnapshotSet() &&
		plansource->raw_parse_tree &&
		analyze_requires_snapshot(plansource->raw_parse_tree))
	{
		PushActiveSnapshot(GetTransactionSnapshot());
		snapshot_set = true;
	}

	/*
	 * The planner may try to call SPI-using functions, which causes a problem
	 * if we're already inside one.  Rather than expect all SPI-using code to
	 * do SPI_push whenever a replan could happen, it seems best to take care
	 * of the case here.
	 */
	spi_pushed = SPI_push_conditional();

	/*
	 * Generate the plan.
	 */
	plist = pg_plan_queries(qlist, plansource->cursor_options, boundParams);

	/* Clean up SPI state */
	SPI_pop_conditional(spi_pushed);

	/* Release snapshot if we got one */
	if (snapshot_set)
		PopActiveSnapshot();

	/* Now we can restore current search path */
	PopOverrideSearchPath();

	/*
	 * Make a dedicated memory context for the CachedPlan and its subsidiary
	 * data.  It's probably not going to be large, but just in case, use the
	 * default maxsize parameter.  It's transient for the moment.
	 */
	plan_context = AllocSetContextCreate(CurrentMemoryContext,
										 "CachedPlan",
										 ALLOCSET_SMALL_MINSIZE,
										 ALLOCSET_SMALL_INITSIZE,
										 ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Copy plan into the new context.
	 */
	oldcxt = MemoryContextSwitchTo(plan_context);

	plist = (List *) copyObject(plist);

	/*
	 * Create and fill the CachedPlan struct within the new context.
	 */
	plan = (CachedPlan *) palloc(sizeof(CachedPlan));
	plan->magic = CACHEDPLAN_MAGIC;
	plan->stmt_list = plist;

	/*
	 * In GPDB, the planner is more aggressive, and e.g. eagerly evaluates
	 * stable functions in the planner already. Such plans are marked as
	 * 'one-off', and mustn't be reused. Likewise, plans for CTAS are not
	 * reused, because the plan depends on the target data distribution.
	 */
	if (plan_list_is_oneoff(plist) || intoClause)
	{
		plan->saved_xmin = BootstrapTransactionId;
	}
	else if (plan_list_is_transient(plist))
	{
		Assert(TransactionIdIsNormal(TransactionXmin));
		plan->saved_xmin = TransactionXmin;
	}
	else
		plan->saved_xmin = InvalidTransactionId;
	plan->refcount = 0;
	plan->context = plan_context;
	plan->is_saved = false;
	plan->is_valid = true;

	/* assign generation number to new plan */
	plan->generation = ++(plansource->generation);

	MemoryContextSwitchTo(oldcxt);

	return plan;
}

/*
 * choose_custom_plan: choose whether to use custom or generic plan
 *
 * This defines the policy followed by GetCachedPlan.
 */
static bool
choose_custom_plan(CachedPlanSource *plansource, ParamListInfo boundParams, IntoClause *intoClause)
{
	double		avg_custom_cost;

	/* Force to replan for CTAS */
	if (intoClause != NULL)
		return true;

	/* Never any point in a custom plan if there's no parameters */
	if (boundParams == NULL)
		return false;

	/* See if caller wants to force the decision */
	if (plansource->cursor_options & CURSOR_OPT_GENERIC_PLAN)
		return false;
	if (plansource->cursor_options & CURSOR_OPT_CUSTOM_PLAN)
		return true;

	/* Generate custom plans until we have done at least 5 (arbitrary) */
	if (plansource->num_custom_plans < 5)
		return true;

	avg_custom_cost = plansource->total_custom_cost / plansource->num_custom_plans;

	/*
	 * Prefer generic plan if it's less than 10% more expensive than average
	 * custom plan.  This threshold is a bit arbitrary; it'd be better if we
	 * had some means of comparing planning time to the estimated runtime cost
	 * differential.
	 *
	 * Note that if generic_cost is -1 (indicating we've not yet determined
	 * the generic plan cost), we'll always prefer generic at this point.
	 */
	if (plansource->generic_cost < avg_custom_cost * 1.1)
		return false;

	return true;
}

/*
 * cached_plan_cost: calculate estimated cost of a plan
 */
static double
cached_plan_cost(CachedPlan *plan)
{
	double		result = 0;
	ListCell   *lc;

	foreach(lc, plan->stmt_list)
	{
		PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc);

		if (!IsA(plannedstmt, PlannedStmt))
			continue;			/* Ignore utility statements */

		result += plannedstmt->planTree->total_cost;
	}

	return result;
}

/*
 * GetCachedPlan: get a cached plan from a CachedPlanSource.
 *
 * This function hides the logic that decides whether to use a generic
 * plan or a custom plan for the given parameters: the caller does not know
 * which it will get.
 *
 * On return, the plan is valid and we have sufficient locks to begin
 * execution.
 *
 * On return, the refcount of the plan has been incremented; a later
 * ReleaseCachedPlan() call is expected.  The refcount has been reported
 * to the CurrentResourceOwner if useResOwner is true (note that that must
 * only be true if it's a "saved" CachedPlanSource).
 *
 * Note: if any replanning activity is required, the caller's memory context
 * is used for that work.
 *
 * In GPDB, this function has one extra parameters: intoClause.
 * If 'intoClause' is given, the plan is to be used as part of a
 * CREATE TABLE AS statement. That affects the distribution of the output rows:
 * we cannot reuse a generic plan that fetches all the output rows into master.
 * They should be distributed to the correct segments according to the
 * distribution policy of the target table, instead. A non-NULL intoClause
 * therefore also forces the plan to be re-planned on next call.
 */
CachedPlan *
GetCachedPlan(CachedPlanSource *plansource, ParamListInfo boundParams,
			  bool useResOwner, IntoClause *intoClause)
{
	CachedPlan *plan;
	List	   *qlist;
	bool		customplan;

	/* Assert caller is doing things in a sane order */
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(plansource->is_complete);
	/* This seems worth a real test, though */
	if (useResOwner && !plansource->is_saved)
		elog(ERROR, "cannot apply ResourceOwner to non-saved cached plan");

	/* Make sure the querytree list is valid and we have parse-time locks */
	qlist = RevalidateCachedQuery(plansource, intoClause);

	/* Decide whether to use a custom plan */
	customplan = choose_custom_plan(plansource, boundParams, intoClause);

	if (!customplan)
	{
		if (CheckCachedPlan(plansource))
		{
			/* We want a generic plan, and we already have a valid one */
			plan = plansource->gplan;
			Assert(plan->magic == CACHEDPLAN_MAGIC);
		}
		else
		{
			/* Build a new generic plan */
			plan = BuildCachedPlan(plansource, qlist, NULL, NULL);
			/* Just make real sure plansource->gplan is clear */
			ReleaseGenericPlan(plansource);
			/* Link the new generic plan into the plansource */
			plansource->gplan = plan;
			plan->refcount++;
			/* Immediately reparent into appropriate context */
			if (plansource->is_saved)
			{
				/* saved plans all live under CacheMemoryContext */
				MemoryContextSetParent(plan->context, CacheMemoryContext);
				plan->is_saved = true;
			}
			else
			{
				/* otherwise, it should be a sibling of the plansource */
				MemoryContextSetParent(plan->context,
								MemoryContextGetParent(plansource->context));
			}
			/* Update generic_cost whenever we make a new generic plan */
			plansource->generic_cost = cached_plan_cost(plan);

			/*
			 * If, based on the now-known value of generic_cost, we'd not have
			 * chosen to use a generic plan, then forget it and make a custom
			 * plan.  This is a bit of a wart but is necessary to avoid a
			 * glitch in behavior when the custom plans are consistently big
			 * winners; at some point we'll experiment with a generic plan and
			 * find it's a loser, but we don't want to actually execute that
			 * plan.
			 */
			customplan = choose_custom_plan(plansource, boundParams, intoClause);

			/*
			 * If we choose to plan again, we need to re-copy the query_list,
			 * since the planner probably scribbled on it.	We can force
			 * BuildCachedPlan to do that by passing NIL.
			 */
			qlist = NIL;
		}
	}

	if (customplan)
	{
		/* Build a custom plan */
		plan = BuildCachedPlan(plansource, qlist, boundParams, intoClause);
		/* Accumulate total costs of custom plans, but 'ware overflow */
		if (plansource->num_custom_plans < INT_MAX)
		{
			plansource->total_custom_cost += cached_plan_cost(plan);
			plansource->num_custom_plans++;
		}
	}

	/* Flag the plan as in use by caller */
	if (useResOwner)
		ResourceOwnerEnlargePlanCacheRefs(CurrentResourceOwner);
	plan->refcount++;
	if (useResOwner)
		ResourceOwnerRememberPlanCacheRef(CurrentResourceOwner, plan);

	/*
	 * Saved plans should be under CacheMemoryContext so they will not go away
	 * until their reference count goes to zero.  In the generic-plan cases we
	 * already took care of that, but for a custom plan, do it as soon as we
	 * have created a reference-counted link.
	 */
	if (customplan && plansource->is_saved)
	{
		MemoryContextSetParent(plan->context, CacheMemoryContext);
		plan->is_saved = true;
	}

	return plan;
}

/*
 * ReleaseCachedPlan: release active use of a cached plan.
 *
 * This decrements the reference count, and frees the plan if the count
 * has thereby gone to zero.  If useResOwner is true, it is assumed that
 * the reference count is managed by the CurrentResourceOwner.
 *
 * Note: useResOwner = false is used for releasing references that are in
 * persistent data structures, such as the parent CachedPlanSource or a
 * Portal.	Transient references should be protected by a resource owner.
 */
void
ReleaseCachedPlan(CachedPlan *plan, bool useResOwner)
{
	Assert(plan->magic == CACHEDPLAN_MAGIC);
	if (useResOwner)
	{
		Assert(plan->is_saved);
		ResourceOwnerForgetPlanCacheRef(CurrentResourceOwner, plan);
	}
	Assert(plan->refcount > 0);
	plan->refcount--;
	if (plan->refcount == 0)
		MemoryContextDelete(plan->context);
}

/*
 * CachedPlanSetParentContext: move a CachedPlanSource to a new memory context
 *
 * This can only be applied to unsaved plans; once saved, a plan always
 * lives underneath CacheMemoryContext.
 */
void
CachedPlanSetParentContext(CachedPlanSource *plansource,
						   MemoryContext newcontext)
{
	/* Assert caller is doing things in a sane order */
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(plansource->is_complete);

	/* This seems worth a real test, though */
	if (plansource->is_saved)
		elog(ERROR, "cannot move a saved cached plan to another context");

	/* OK, let the caller keep the plan where he wishes */
	MemoryContextSetParent(plansource->context, newcontext);

	/*
	 * The query_context needs no special handling, since it's a child of
	 * plansource->context.  But if there's a generic plan, it should be
	 * maintained as a sibling of plansource->context.
	 */
	if (plansource->gplan)
	{
		Assert(plansource->gplan->magic == CACHEDPLAN_MAGIC);
		MemoryContextSetParent(plansource->gplan->context, newcontext);
	}
}

/*
 * CopyCachedPlan: make a copy of a CachedPlanSource
 *
 * This is a convenience routine that does the equivalent of
 * CreateCachedPlan + CompleteCachedPlan, using the data stored in the
 * input CachedPlanSource.	The result is therefore "unsaved" (regardless
 * of the state of the source), and we don't copy any generic plan either.
 * The result will be currently valid, or not, the same as the source.
 */
CachedPlanSource *
CopyCachedPlan(CachedPlanSource *plansource)
{
	CachedPlanSource *newsource;
	MemoryContext source_context;
	MemoryContext querytree_context;
	MemoryContext oldcxt;

	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(plansource->is_complete);

	source_context = AllocSetContextCreate(CurrentMemoryContext,
										   "CachedPlanSource",
										   ALLOCSET_SMALL_MINSIZE,
										   ALLOCSET_SMALL_INITSIZE,
										   ALLOCSET_DEFAULT_MAXSIZE);

	oldcxt = MemoryContextSwitchTo(source_context);

	newsource = (CachedPlanSource *) palloc0(sizeof(CachedPlanSource));
	newsource->magic = CACHEDPLANSOURCE_MAGIC;
	newsource->raw_parse_tree = copyObject(plansource->raw_parse_tree);
	newsource->query_string = pstrdup(plansource->query_string);
	newsource->sourceTag = plansource->sourceTag;
	newsource->commandTag = plansource->commandTag;
	if (plansource->num_params > 0)
	{
		newsource->param_types = (Oid *)
			palloc(plansource->num_params * sizeof(Oid));
		memcpy(newsource->param_types, plansource->param_types,
			   plansource->num_params * sizeof(Oid));
	}
	else
		newsource->param_types = NULL;
	newsource->num_params = plansource->num_params;
	newsource->parserSetup = plansource->parserSetup;
	newsource->parserSetupArg = plansource->parserSetupArg;
	newsource->cursor_options = plansource->cursor_options;
	newsource->fixed_result = plansource->fixed_result;
	if (plansource->resultDesc)
		newsource->resultDesc = CreateTupleDescCopy(plansource->resultDesc);
	else
		newsource->resultDesc = NULL;
	newsource->search_path = CopyOverrideSearchPath(plansource->search_path);
	newsource->context = source_context;

	querytree_context = AllocSetContextCreate(source_context,
											  "CachedPlanQuery",
											  ALLOCSET_SMALL_MINSIZE,
											  ALLOCSET_SMALL_INITSIZE,
											  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(querytree_context);
	newsource->query_list = (List *) copyObject(plansource->query_list);
	newsource->relationOids = (List *) copyObject(plansource->relationOids);
	newsource->invalItems = (List *) copyObject(plansource->invalItems);
	newsource->query_context = querytree_context;

	newsource->gplan = NULL;

	newsource->is_complete = true;
	newsource->is_saved = false;
	newsource->is_valid = plansource->is_valid;
	newsource->generation = plansource->generation;
	newsource->next_saved = NULL;

	/* We may as well copy any acquired cost knowledge */
	newsource->generic_cost = plansource->generic_cost;
	newsource->total_custom_cost = plansource->total_custom_cost;
	newsource->num_custom_plans = plansource->num_custom_plans;

	MemoryContextSwitchTo(oldcxt);

	return newsource;
}

/*
 * CachedPlanIsValid: test whether the rewritten querytree within a
 * CachedPlanSource is currently valid (that is, not marked as being in need
 * of revalidation).
 *
 * This result is only trustworthy (ie, free from race conditions) if
 * the caller has acquired locks on all the relations used in the plan.
 */
bool
CachedPlanIsValid(CachedPlanSource *plansource)
{
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	return plansource->is_valid;
}

/*
 * CachedPlanGetTargetList: return tlist, if any, describing plan's output
 *
 * The result is guaranteed up-to-date.  However, it is local storage
 * within the cached plan, and may disappear next time the plan is updated.
 */
List *
CachedPlanGetTargetList(CachedPlanSource *plansource)
{
	Node	   *pstmt;

	/* Assert caller is doing things in a sane order */
	Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);
	Assert(plansource->is_complete);

	/*
	 * No work needed if statement doesn't return tuples (we assume this
	 * feature cannot be changed by an invalidation)
	 */
	if (plansource->resultDesc == NULL)
		return NIL;

	/* Make sure the querytree list is valid and we have parse-time locks */
	RevalidateCachedQuery(plansource, NULL);

	/* Get the primary statement and find out what it returns */
	pstmt = PortalListGetPrimaryStmt(plansource->query_list);

	return FetchStatementTargetList(pstmt);
}

/*
 * AcquireExecutorLocks: acquire locks needed for execution of a cached plan;
 * or release them if acquire is false.
 */
static void
AcquireExecutorLocks(List *stmt_list, bool acquire)
{
	ListCell   *lc1;

	foreach(lc1, stmt_list)
	{
		PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc1);
		int			rt_index;
		ListCell   *lc2;

		Assert(!IsA(plannedstmt, Query));
		if (!IsA(plannedstmt, PlannedStmt))
		{
			/*
			 * Ignore utility statements, except those (such as EXPLAIN) that
			 * contain a parsed-but-not-planned query.	Note: it's okay to use
			 * ScanQueryForLocks, even though the query hasn't been through
			 * rule rewriting, because rewriting doesn't change the query
			 * representation.
			 */
			Query	   *query = UtilityContainsQuery((Node *) plannedstmt);

			if (query)
				ScanQueryForLocks(query, acquire);
			continue;
		}

		rt_index = 0;
		foreach(lc2, plannedstmt->rtable)
		{
			RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc2);
			LOCKMODE	lockmode;
			PlanRowMark *rc;

			rt_index++;

			if (rte->rtekind != RTE_RELATION)
				continue;

			/*
			 * Acquire the appropriate type of lock on each relation OID. Note
			 * that we don't actually try to open the rel, and hence will not
			 * fail if it's been dropped entirely --- we'll just transiently
			 * acquire a non-conflicting lock.
			 */
			if (list_member_int(plannedstmt->resultRelations, rt_index))
			{
				/*
				 * RowExclusiveLock is acquired in PostgreSQL here.  Greenplum
				 * acquires ExclusiveLock to avoid distributed deadlock due to
				 * concurrent UPDATE/DELETE on the same table.  This is in
				 * parity with CdbTryOpenRelation().  Catalog tables are
				 * replicated across cluster and don't suffer from the
				 * deadlock.
				 * Since we have introduced Global Deadlock Detector, only for ao
				 * table should we upgrade the lock.
				 */
				if (rte->relid > FirstNormalObjectId &&
					(plannedstmt->commandType == CMD_UPDATE ||
					 plannedstmt->commandType == CMD_DELETE) &&
					CondUpgradeRelLock(rte->relid))
					lockmode = ExclusiveLock;
				else
					lockmode = RowExclusiveLock;
			}
			else if ((rc = get_plan_rowmark(plannedstmt->rowMarks, rt_index)) != NULL &&
					 RowMarkRequiresRowShareLock(rc->markType))
				lockmode = RowShareLock;
			else
				lockmode = AccessShareLock;

			if (acquire)
				LockRelationOid(rte->relid, lockmode);
			else
				UnlockRelationOid(rte->relid, lockmode);
		}
	}
}

/*
 * AcquirePlannerLocks: acquire locks needed for planning of a querytree list;
 * or release them if acquire is false.
 *
 * Note that we don't actually try to open the relations, and hence will not
 * fail if one has been dropped entirely --- we'll just transiently acquire
 * a non-conflicting lock.
 */
static void
AcquirePlannerLocks(List *stmt_list, bool acquire)
{
	ListCell   *lc;

	foreach(lc, stmt_list)
	{
		Query	   *query = (Query *) lfirst(lc);

		Assert(IsA(query, Query));

		if (query->commandType == CMD_UTILITY)
		{
			/* Ignore utility statements, unless they contain a Query */
			query = UtilityContainsQuery(query->utilityStmt);
			if (query)
				ScanQueryForLocks(query, acquire);
			continue;
		}

		ScanQueryForLocks(query, acquire);
	}
}

/*
 * ScanQueryForLocks: recursively scan one Query for AcquirePlannerLocks.
 */
static void
ScanQueryForLocks(Query *parsetree, bool acquire)
{
	ListCell   *lc;
	int			rt_index;

	/* Shouldn't get called on utility commands */
	Assert(parsetree->commandType != CMD_UTILITY);

	/*
	 * First, process RTEs of the current query level.
	 */
	rt_index = 0;
	foreach(lc, parsetree->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
		LOCKMODE	lockmode;

		rt_index++;
		switch (rte->rtekind)
		{
			case RTE_RELATION:
				/* Acquire or release the appropriate type of lock */
				if (rt_index == parsetree->resultRelation)
				{
					/*
					 * RowExclusiveLock is acquired in PostgreSQL here.
					 * Greenplum acquires ExclusiveLock to avoid distributed
					 * deadlock due to concurrent UPDATE/DELETE on the same
					 * table.  This is in parity with CdbTryOpenRelation().
					 * Catalog tables are replicated across cluster and don't
					 * suffer from the deadlock.
					 */
					if (rte->relid > FirstNormalObjectId &&
						(parsetree->commandType == CMD_UPDATE ||
						 parsetree->commandType == CMD_DELETE) &&
						CondUpgradeRelLock(rte->relid))
						lockmode = ExclusiveLock;
					else
						lockmode = RowExclusiveLock;
				}
				else if (get_parse_rowmark(parsetree, rt_index) != NULL)
					lockmode = RowShareLock;
				else
					lockmode = AccessShareLock;
				if (acquire)
					LockRelationOid(rte->relid, lockmode);
				else
					UnlockRelationOid(rte->relid, lockmode);
				break;

			case RTE_SUBQUERY:
				/* Recurse into subquery-in-FROM */
				ScanQueryForLocks(rte->subquery, acquire);
				break;

			default:
				/* ignore other types of RTEs */
				break;
		}
	}

	/* Recurse into subquery-in-WITH */
	foreach(lc, parsetree->cteList)
	{
		CommonTableExpr *cte = (CommonTableExpr *) lfirst(lc);

		ScanQueryForLocks((Query *) cte->ctequery, acquire);
	}

	/*
	 * Recurse into sublink subqueries, too.  But we already did the ones in
	 * the rtable and cteList.
	 */
	if (parsetree->hasSubLinks)
	{
		query_tree_walker(parsetree, ScanQueryWalker,
						  (void *) &acquire,
						  QTW_IGNORE_RC_SUBQUERIES);
	}
}

/*
 * Walker to find sublink subqueries for ScanQueryForLocks
 */
static bool
ScanQueryWalker(Node *node, bool *acquire)
{
	if (node == NULL)
		return false;
	if (IsA(node, SubLink))
	{
		SubLink    *sub = (SubLink *) node;

		/* Do what we came for */
		ScanQueryForLocks((Query *) sub->subselect, *acquire);
		/* Fall through to process lefthand args of SubLink */
	}

	/*
	 * Do NOT recurse into Query nodes, because ScanQueryForLocks already
	 * processed subselects of subselects for us.
	 */
	return expression_tree_walker(node, ScanQueryWalker,
								  (void *) acquire);
}

/*
 * plan_list_is_transient: check if any of the plans in the list are transient.
 */
static bool
plan_list_is_transient(List *stmt_list)
{
	ListCell   *lc;

	foreach(lc, stmt_list)
	{
		PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc);

		if (!IsA(plannedstmt, PlannedStmt))
			continue;			/* Ignore utility statements */

		if (plannedstmt->transientPlan)
			return true;
	}

	return false;
}

/*
 * plan_list_is_oneoff: check if any of the plans in the list are one-off plans
 */
static bool
plan_list_is_oneoff(List *stmt_list)
{
	ListCell   *lc;

	foreach(lc, stmt_list)
	{
		PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc);

		if (!IsA(plannedstmt, PlannedStmt))
			continue;			/* Ignore utility statements */

		if (plannedstmt->oneoffPlan)
			return true;
	}

	return false;
}

/*
 * PlanCacheComputeResultDesc: given a list of analyzed-and-rewritten Queries,
 * determine the result tupledesc it will produce.	Returns NULL if the
 * execution will not return tuples.
 *
 * Note: the result is created or copied into current memory context.
 */
static TupleDesc
PlanCacheComputeResultDesc(List *stmt_list)
{
	Query	   *query;

	switch (ChoosePortalStrategy(stmt_list))
	{
		case PORTAL_ONE_SELECT:
		case PORTAL_ONE_MOD_WITH:
			query = (Query *) linitial(stmt_list);
			Assert(IsA(query, Query));
			return ExecCleanTypeFromTL(query->targetList, false);

		case PORTAL_ONE_RETURNING:
			query = (Query *) PortalListGetPrimaryStmt(stmt_list);
			Assert(IsA(query, Query));
			Assert(query->returningList);
			return ExecCleanTypeFromTL(query->returningList, false);

		case PORTAL_UTIL_SELECT:
			query = (Query *) linitial(stmt_list);
			Assert(IsA(query, Query));
			Assert(query->utilityStmt);
			return UtilityTupleDescriptor(query->utilityStmt);

		case PORTAL_MULTI_QUERY:
			/* will not return tuples */
			break;
	}
	return NULL;
}

/*
 * PlanCacheRelCallback
 *		Relcache inval callback function
 *
 * Invalidate all plans mentioning the given rel, or all plans mentioning
 * any rel at all if relid == InvalidOid.
 */
static void
PlanCacheRelCallback(Datum arg, Oid relid)
{
	CachedPlanSource *plansource;

	for (plansource = first_saved_plan; plansource; plansource = plansource->next_saved)
	{
		Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

		/* No work if it's already invalidated */
		if (!plansource->is_valid)
			continue;

		/*
		 * Check the dependency list for the rewritten querytree.
		 */
		if ((relid == InvalidOid) ? plansource->relationOids != NIL :
			list_member_oid(plansource->relationOids, relid))
		{
			/* Invalidate the querytree and generic plan */
			plansource->is_valid = false;
			if (plansource->gplan)
				plansource->gplan->is_valid = false;
		}

		/*
		 * The generic plan, if any, could have more dependencies than the
		 * querytree does, so we have to check it too.
		 */
		if (plansource->gplan && plansource->gplan->is_valid)
		{
			ListCell   *lc;

			foreach(lc, plansource->gplan->stmt_list)
			{
				PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc);

				Assert(!IsA(plannedstmt, Query));
				if (!IsA(plannedstmt, PlannedStmt))
					continue;	/* Ignore utility statements */
				if ((relid == InvalidOid) ? plannedstmt->relationOids != NIL :
					list_member_oid(plannedstmt->relationOids, relid))
				{
					/* Invalidate the generic plan only */
					plansource->gplan->is_valid = false;
					break;		/* out of stmt_list scan */
				}
			}
		}
	}
}

/*
 * PlanCacheFuncCallback
 *		Syscache inval callback function for PROCOID cache
 *
 * Invalidate all plans mentioning the object with the specified hash value,
 * or all plans mentioning any member of this cache if hashvalue == 0.
 *
 * Note that the coding would support use for multiple caches, but right
 * now only user-defined functions are tracked this way.
 */
static void
PlanCacheFuncCallback(Datum arg, int cacheid, uint32 hashvalue)
{
	CachedPlanSource *plansource;

	for (plansource = first_saved_plan; plansource; plansource = plansource->next_saved)
	{
		ListCell   *lc;

		Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

		/* No work if it's already invalidated */
		if (!plansource->is_valid)
			continue;

		/*
		 * Check the dependency list for the rewritten querytree.
		 */
		foreach(lc, plansource->invalItems)
		{
			PlanInvalItem *item = (PlanInvalItem *) lfirst(lc);

			if (item->cacheId != cacheid)
				continue;
			if (hashvalue == 0 ||
				item->hashValue == hashvalue)
			{
				/* Invalidate the querytree and generic plan */
				plansource->is_valid = false;
				if (plansource->gplan)
					plansource->gplan->is_valid = false;
				break;
			}
		}

		/*
		 * The generic plan, if any, could have more dependencies than the
		 * querytree does, so we have to check it too.
		 */
		if (plansource->gplan && plansource->gplan->is_valid)
		{
			foreach(lc, plansource->gplan->stmt_list)
			{
				PlannedStmt *plannedstmt = (PlannedStmt *) lfirst(lc);
				ListCell   *lc3;

				Assert(!IsA(plannedstmt, Query));
				if (!IsA(plannedstmt, PlannedStmt))
					continue;	/* Ignore utility statements */
				foreach(lc3, plannedstmt->invalItems)
				{
					PlanInvalItem *item = (PlanInvalItem *) lfirst(lc3);

					if (item->cacheId != cacheid)
						continue;
					if (hashvalue == 0 ||
						item->hashValue == hashvalue)
					{
						/* Invalidate the generic plan only */
						plansource->gplan->is_valid = false;
						break;	/* out of invalItems scan */
					}
				}
				if (!plansource->gplan->is_valid)
					break;		/* out of stmt_list scan */
			}
		}
	}
}

/*
 * PlanCacheSysCallback
 *		Syscache inval callback function for other caches
 *
 * Just invalidate everything...
 */
static void
PlanCacheSysCallback(Datum arg, int cacheid, uint32 hashvalue)
{
	ResetPlanCache();
}

/*
 * ResetPlanCache: invalidate all cached plans.
 */
void
ResetPlanCache(void)
{
	CachedPlanSource *plansource;

	for (plansource = first_saved_plan; plansource; plansource = plansource->next_saved)
	{
		ListCell   *lc;

		Assert(plansource->magic == CACHEDPLANSOURCE_MAGIC);

		/* No work if it's already invalidated */
		if (!plansource->is_valid)
			continue;

		/*
		 * We *must not* mark transaction control statements as invalid,
		 * particularly not ROLLBACK, because they may need to be executed in
		 * aborted transactions when we can't revalidate them (cf bug #5269).
		 * In general there is no point in invalidating utility statements
		 * since they have no plans anyway.  So invalidate it only if it
		 * contains at least one non-utility statement, or contains a utility
		 * statement that contains a pre-analyzed query (which could have
		 * dependencies.)
		 */
		foreach(lc, plansource->query_list)
		{
			Query	   *query = (Query *) lfirst(lc);

			Assert(IsA(query, Query));
			if (query->commandType != CMD_UTILITY ||
				UtilityContainsQuery(query->utilityStmt))
			{
				/* non-utility statement, so invalidate */
				plansource->is_valid = false;
				if (plansource->gplan)
					plansource->gplan->is_valid = false;
				/* no need to look further */
				break;
			}
		}
	}
}
