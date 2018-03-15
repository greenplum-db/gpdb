/*---------------------------------------------------------------------
 *
 * gp_dump_oids.c
 *
 * Copyright (c) 2015-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/adt/gp_dump_oids.c
 *
 *---------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_proc.h"
#include "tcop/tcopprot.h"
#include "optimizer/planmain.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

Datum gp_dump_query_oids(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_dump_query_oids);

/*
 * Append a list of Oids to a StringInfo, separated by commas.
 */
static void
appendOids(StringInfo buf, List *oids)
{
	ListCell   *lc;
	bool		first = true;

	foreach(lc, oids)
	{
		if (!first)
			appendStringInfoChar(buf, ',');
		appendStringInfo(buf, "%u", lfirst_oid(lc));
		first = false;
	}
}

/*
 * Error context callback for handling errors in the SQL query passed as
 * argument to gp_dump_query_oids()
 */
static void
sql_query_parse_error_callback(void *arg)
{
	const char *query_text = (const char *) arg;
	int			syntaxerrposition = geterrposition();

	/*
	 * The error is not in the original query. Report the query that was
	 * passed as argument as an "internal query", and the position in it.
	 */
	errposition(0);
	internalerrposition(syntaxerrposition);
	internalerrquery(query_text);
}

/*
 * Function dumping dependent relation & function oids for a given SQL text
 */
Datum
gp_dump_query_oids(PG_FUNCTION_ARGS)
{
	char	   *sqlText = text_to_cstring(PG_GETARG_TEXT_P(0));
	List	   *queryList;
	ListCell   *lc;
	HASHCTL		ctl;
	HTAB	   *dedup_htab;
	StringInfoData str;
	ErrorContextCallback sqlerrcontext;
	Relation	procRelation;
	List	   *invalidItems = NIL;
	List	   *relationOids = NIL;
	List	   *deduped_relationOids = NIL;
	List	   *procOids = NIL;

	/*
	 * Setup error traceback support for ereport().
	 */
	sqlerrcontext.callback = sql_query_parse_error_callback;
	sqlerrcontext.arg = sqlText;
	sqlerrcontext.previous = error_context_stack;
	error_context_stack = &sqlerrcontext;

	/*
	 * Parse and analyze the query.
	 */
	queryList = pg_parse_and_rewrite(sqlText, NULL, 0);

	error_context_stack = sqlerrcontext.previous;

	/* Then scan each Query and scrape any relation and function OIDs */
	foreach(lc, queryList)
	{
		Query	   *q = lfirst(lc);
		List	   *q_relationOids = NIL;
		List	   *q_invalidItems = NIL;

		extract_query_dependencies((Node *) q, &q_relationOids, &q_invalidItems);

		relationOids = list_concat(relationOids, q_relationOids);
		invalidItems = list_concat(invalidItems, q_invalidItems);
	}

	/*
	 * Deduplicate the relation oids
	 */
	memset(&ctl, 0, sizeof(HASHCTL));
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(Oid);
	ctl.hash = oid_hash;
	dedup_htab = hash_create("relid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);

	deduped_relationOids = NIL;
	foreach(lc, relationOids)
	{
		Oid			relid = lfirst_oid(lc);
		bool		found;

		hash_search(dedup_htab, (void *) &relid, HASH_ENTER, &found);
		if (!found)
		{
			deduped_relationOids = lappend_oid(deduped_relationOids, relid);

			/*
			 * Also find all child table relids including inheritances,
			 * interior and leaf partitions of given root table oid
			 */
			List *child_relids = find_all_inheritors(relid, NoLock, NULL);

			if (child_relids)
			{
				child_relids = list_delete_first(child_relids);
				deduped_relationOids = list_concat(deduped_relationOids, child_relids);
			}
		}
	}
	relationOids = deduped_relationOids;

	/*
	 * Fetch the procedure OIDs based on the PlanInvalItems. Deduplicate them as
	 * we go.
	 */
	procRelation = heap_open(ProcedureRelationId, AccessShareLock);

	memset(&ctl, 0, sizeof(HASHCTL));
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(Oid);
	ctl.hash = oid_hash;
	dedup_htab = hash_create("funcid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);

	foreach (lc, invalidItems)
	{
		PlanInvalItem *inval_item = (PlanInvalItem *) lfirst(lc);

		if (inval_item->cacheId == PROCOID)
		{
			HeapTupleData htup;
			Buffer		buf = InvalidBuffer;

			htup.t_self = inval_item->tupleId;

			if (heap_fetch(procRelation,
						   SnapshotAny,
						   &htup,
						   &buf,
						   false,
						   NULL))
			{
				Oid			funcId = HeapTupleGetOid(&htup);
				bool		found;

				hash_search(dedup_htab, (void *) &funcId, HASH_ENTER, &found);
				if (!found)
					 procOids = lappend_oid(procOids, funcId);
			}

			if (BufferIsValid(buf))
				ReleaseBuffer(buf);
		}
	}

	heap_close(procRelation, AccessShareLock);

	/*
	 * Construct the final output
	 */
	initStringInfo(&str);

	appendStringInfo(&str, "{\"relids\": \"");
	appendOids(&str, relationOids);

	appendStringInfoString(&str, "\", \"funcids\": \"");
	appendOids(&str, procOids);
	appendStringInfo(&str, "\"}");

	PG_RETURN_TEXT_P(cstring_to_text(str.data));
}
