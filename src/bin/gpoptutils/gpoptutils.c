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
void traverseQueryOids(Query *pquery, HTAB *relhtab, StringInfoData *relbuf, HTAB *funchtab, StringInfoData *funcbuf);

Datum gp_dump_query_oids(PG_FUNCTION_ARGS);

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(gp_dump_query_oids);

static void traverseQueryOids
	(
	Query *pquery,
	HTAB *relhtab,
	StringInfoData *relbuf,
	HTAB *funchtab,
	StringInfoData *funcbuf
	)
{
	ListCell *plc;
	bool relFound, funcFound;
	foreach (plc, pquery->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(plc);

		switch (rte->rtekind)
		{
			case RTE_RELATION:
			{
				hash_search(relhtab, (void *)&rte->relid, HASH_ENTER, &relFound);
				if (!relFound)
				{
					if (0 != relbuf->len)
						appendStringInfo(relbuf, "%s", ",");
					appendStringInfo(relbuf, "%u", rte->relid);
				}
			}
				break;
			case RTE_FUNCTION:
			{
				FuncExpr *node = (FuncExpr *)rte->funcexpr; 
				hash_search(funchtab, (void *)&node->funcid, HASH_ENTER, &funcFound);
				if (!funcFound)
				{
					if (0 != funcbuf->len)
						appendStringInfo(funcbuf, "%s", ",");
					appendStringInfo(funcbuf, "%u", node->funcid);
				}
			}
				break;
			case RTE_SUBQUERY:
				traverseQueryOids(rte->subquery, relhtab, relbuf, funchtab, funcbuf);
				break;
			default:
				break;
		}
	}
	
	foreach (plc, pquery->targetList)
	{
		Expr *expr = ((TargetEntry *) lfirst(plc))->expr;
		if (expr->type == T_FuncExpr)
		{
			// expression node for a function call, i.e. select f();
			FuncExpr *node = (FuncExpr *)expr;
			hash_search(funchtab, (void *)&node->funcid, HASH_ENTER, &funcFound);
			if (!funcFound)
			{
				if (0 != funcbuf->len)
					appendStringInfo(funcbuf, "%s", ",");
				appendStringInfo(funcbuf, "%u", node->funcid);
			}
		}
		else if(expr->type == T_SubLink)
		{
			// subselect appearing in an expression
			SubLink *sublink = (SubLink *)expr;
			traverseQueryOids((Query *)sublink->subselect, relhtab, relbuf, funchtab, funcbuf);
		}
	}

	foreach(plc, pquery->cteList)
	{
		CommonTableExpr *cte = (CommonTableExpr *) lfirst(plc);
		traverseQueryOids((Query *)cte->ctequery, relhtab, relbuf, funchtab, funcbuf);
	}
}

/*
 * Function dumping dependent relation & function oids for a given SQL text
 */
Datum
gp_dump_query_oids(PG_FUNCTION_ARGS)
{
	char *sqlText = text_to_cstring(PG_GETARG_TEXT_P(0));
	List *queryList = pg_parse_and_rewrite(sqlText, NULL, 0);
	ListCell *plc;
	
	StringInfoData relbuf, funcbuf;
	initStringInfo(&relbuf);
	initStringInfo(&funcbuf);
	
	typedef struct OidHashEntry
	{
		Oid key;
		bool value;
	} OidHashEntry;
	HASHCTL ctl;
	ctl.keysize = sizeof(Oid);
	ctl.entrysize = sizeof(OidHashEntry);
	ctl.hash = oid_hash;
	HTAB *relhtab = hash_create("relid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);
	HTAB *funchtab = hash_create("funcid hash table", 100, &ctl, HASH_ELEM | HASH_FUNCTION);
	
	foreach(plc, queryList)
	{
		Query *query = (Query *) lfirst(plc);
		if (CMD_UTILITY == query->commandType && T_ExplainStmt == query->utilityStmt->type)
		{
			Query *queryExplain = ((ExplainStmt *)query->utilityStmt)->query;
			List *queryTree = QueryRewrite(queryExplain);
			Assert(1 == list_length(queryTree));
			query = (Query *) lfirst(list_head(queryTree));
		}
		traverseQueryOids(query, relhtab, &relbuf, funchtab, &funcbuf);
	}
	
	hash_destroy(relhtab);
	hash_destroy(funchtab);

	StringInfoData str;
	initStringInfo(&str);
	appendStringInfo(&str, "{\"relids\": \"%s\", \"funcids\": \"%s\"}", relbuf.data, funcbuf.data);

	text *result = cstring_to_text(str.data);
	pfree(relbuf.data);
	pfree(funcbuf.data);
	pfree(str.data);

	PG_RETURN_TEXT_P(result);
}
