/*
 * Copyright (c) 2015 Pivotal Inc. All Rights Reserved
 *
 * ---------------------------------------------------------------------
 *
 * The dynamically linked library created from this source can be reference by
 * creating a function in psql that references it. For example,
 *
 * CREATE FUNCTION gp_dump_query_oids(text)
 *	RETURNS text
 *	AS '$libdir/gpoptutils', 'gp_dump_query_oids'
 *	LANGUAGE C STRICT;
 */

#include "postgres.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "gpopt/utils/nodeutils.h"
#include "rewrite/rewriteHandler.h"
#include "c.h"

extern
List *pg_parse_and_rewrite(const char *query_string, Oid *paramTypes, int iNumParams);

extern
List *QueryRewrite(Query *parsetree);

static
Query *parseSQL(char *szSqlText);

static
void traverseQueryRTEs(Query *pquery, HTAB *phtab, StringInfoData *buf);

Datum gp_dump_query_oids(PG_FUNCTION_ARGS);

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(gp_dump_query_oids);

/*
 * Parse a query given as SQL text.
 */
static Query *parseSQL(char *sqlText)
{
	Assert(sqlText);

	List *queryTree = pg_parse_and_rewrite(sqlText, NULL, 0);

	if (1 != list_length(queryTree))
	{
		elog(ERROR, "Cannot parse query. "
				"Please make sure the input contains a single valid query. \n%s", sqlText);
	}

	Query *query = (Query *) lfirst(list_head(queryTree));

	return query;
}

static void traverseQueryRTEs
	(
	Query *pquery,
	HTAB *phtab,
	StringInfoData *buf
	)
{
	ListCell *plc;
	bool found;
	foreach (plc, pquery->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(plc);

		switch (rte->rtekind)
		{
			case RTE_RELATION:
			{
				hash_search(phtab, (void *)&rte->relid, HASH_ENTER, &found);
				if (!found)
				{
					if (0 != buf->len)
						appendStringInfo(buf, "%s", ", ");
					appendStringInfo(buf, "%u", rte->relid);
				}
			}
				break;
			case RTE_SUBQUERY:
				traverseQueryRTEs(rte->subquery, phtab, buf);
				break;
			default:
				break;
		}
	}
}

/*
 * Function dumping dependent relation oids for a given SQL text
 */
Datum
gp_dump_query_oids(PG_FUNCTION_ARGS)
{
	char *szSqlText = text_to_cstring(PG_GETARG_TEXT_P(0));

	Query *pquery = parseSQL(szSqlText);
	if (CMD_UTILITY == pquery->commandType && T_ExplainStmt == pquery->utilityStmt->type)
	{
		Query *pqueryExplain = ((ExplainStmt *)pquery->utilityStmt)->query;
		List *plQueryTree = QueryRewrite(pqueryExplain);
		Assert(1 == list_length(plQueryTree));
		pquery = (Query *) lfirst(list_head(plQueryTree));
	}

	typedef struct OidHashEntry
	{
		Oid key;
		bool value;
	} OidHashEntry;
	HASHCTL ctl;
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(OidHashEntry);
	ctl.hash = oid_hash;

	StringInfoData buf;
	initStringInfo(&buf);

	HTAB *phtab = hash_create("relid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);
	traverseQueryRTEs(pquery, phtab, &buf);
	hash_destroy(phtab);

	StringInfoData str;
	initStringInfo(&str);
	appendStringInfo(&str, "{\"relids\": [%s]}", buf.data);

	text *result = cstring_to_text(str.data);
	pfree(buf.data);
	pfree(str.data);

	PG_RETURN_TEXT_P(result);
}
