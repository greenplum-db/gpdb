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
#include "funcapi.h"
#include "optimizer/prep.h"
#include "utils/builtins.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/tcopprot.h"

#define atooid(x)  ((Oid) strtoul((x), NULL, 10))

Datum gp_dump_query_oids(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_dump_query_oids);

/*
 * Find all child table relids including inheritances, interior and leaf
 * partitions of given root table oid, append them into string buffer.
 */
static void
appendChildrenRelids(StringInfoData *relbuf, Oid relid)
{
	List	   *relids;
	ListCell   *lc;

	relids = find_all_inheritors(relid, NoLock);
	if (list_length(relids) <= 1)
		return;

	relids = list_delete_first(relids);
	foreach(lc, relids)
	{
		appendStringInfo(relbuf, ",%u", lfirst_oid(lc));
	}
}

static void
traverseQueryOids
	(
	Query          *pquery,
	HTAB           *relhtab,
	StringInfoData *relbuf,
	HTAB           *funchtab,
	StringInfoData *funcbuf
	)
{
	bool	   found;
	const char *whitespace = " \t\n\r";
	char	   *query = nodeToString(pquery);
	char	   *token = strtok(query, whitespace);

	while (token)
	{
		if (pg_strcasecmp(token, ":relid") == 0)
		{
			token = strtok(NULL, whitespace);
			if (token)
			{
				Oid relid = atooid(token);
				hash_search(relhtab, (void *)&relid, HASH_ENTER, &found);
				if (!found)
				{
					if (relbuf->len != 0)
						appendStringInfo(relbuf, "%s", ",");
					appendStringInfo(relbuf, "%u", relid);
					appendChildrenRelids(relbuf, relid);
				}
			}
		}
		else if (pg_strcasecmp(token, ":funcid") == 0)
		{
			token = strtok(NULL, whitespace);
			if (token)
			{
				Oid funcid = atooid(token);
				hash_search(funchtab, (void *)&funcid, HASH_ENTER, &found);
				if (!found)
				{
					if (funcbuf->len != 0)
						appendStringInfo(funcbuf, "%s", ",");
					appendStringInfo(funcbuf, "%u", funcid);
				}
			}
		}

		token = strtok(NULL, whitespace);
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
	List	   *expanded_queryList = NIL;
	ListCell   *lc;
	HASHCTL		ctl;
	HTAB	   *relhtab;
	HTAB	   *funchtab;
	StringInfoData relbuf,
				funcbuf;
	StringInfoData str;
	ErrorContextCallback sqlerrcontext;

	memset(&ctl, 0, sizeof(HASHCTL));
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(Oid);
	ctl.hash = oid_hash;

	relhtab = hash_create("relid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);
	funchtab = hash_create("funcid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);

	/*
	 * Setup error traceback support for ereport().
	 */
	sqlerrcontext.callback = sql_query_parse_error_callback;
	sqlerrcontext.arg = sqlText;
	sqlerrcontext.previous = error_context_stack;
	error_context_stack = &sqlerrcontext;

	/*
	 * Traverse through the query list. For EXPLAIN statements, the query list
	 * contains an ExplainStmt with the raw parse tree of the actual query.
	 * Analyze the explained queries instead of the ExplainStmt itself.
	 */
	queryList = pg_parse_and_rewrite(sqlText, NULL, 0);
	foreach(lc, queryList)
	{
		Query	   *query = (Query *) lfirst(lc);

		if (query->commandType == CMD_UTILITY &&
			IsA(query->utilityStmt, ExplainStmt))
		{
			ExplainStmt *estmt = (ExplainStmt *) query->utilityStmt;
			List	   *l;

			l = pg_analyze_and_rewrite(estmt->query, sqlText, NULL, 0);

			expanded_queryList = list_concat(expanded_queryList, l);
		}
		else
			expanded_queryList = lappend(expanded_queryList, query);
	}

	error_context_stack = sqlerrcontext.previous;

	/* Then scan each Query and scrape any relation and function OIDs */
	initStringInfo(&relbuf);
	initStringInfo(&funcbuf);

	foreach(lc, expanded_queryList)
	{
		traverseQueryOids((Query *) lfirst(lc), relhtab, &relbuf, funchtab, &funcbuf);
	}

	/* Construct the final output */
	initStringInfo(&str);
	appendStringInfo(&str, "{\"relids\": \"%s\", \"funcids\": \"%s\"}", relbuf.data, funcbuf.data);

	PG_RETURN_TEXT_P(cstring_to_text(str.data));
}
