/*-------------------------------------------------------------------------
 *
 * explain.h
 *	  prototypes for explain.c
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/commands/explain.h,v 1.35 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXPLAIN_H
#define EXPLAIN_H

#include "executor/executor.h"

typedef enum ExplainFormat
{
		EXPLAIN_FORMAT_TEXT,
		EXPLAIN_FORMAT_XML,
		EXPLAIN_FORMAT_JSON
} ExplainFormat;

typedef struct ExplainState
{
	StringInfo str;				/* output buffer */
	/* options */
	bool		verbose;			/* print plan targetlists */
	bool		analyze;			/* print actual times */
	bool		costs;				/* print costs */
	bool		buffers;
	bool		timing;
	ExplainFormat format;	/*output format */
	/* other states */
	PlannedStmt *pstmt;	/* top of plan */
	List	   *rtable;				/* range table */
	int		indent;				/* current indentation level */
	List		*grouping_stack;	/* format-specific grouping state */

	bool    dxl;						/* explain in DXL format */
    /* CDB */
    struct CdbExplain_ShowStatCtx  *showstatctx;    /* EXPLAIN ANALYZE info */
    Slice          *currentSlice;   /* slice whose nodes we are visiting */
    ErrorData      *deferredError;  /* caught error to be re-thrown */
} ExplainState;


/* Hook for plugins to get control in ExplainOneQuery() */
typedef void (*ExplainOneQuery_hook_type) (Query *query,
													   ExplainState *es,
													   const char *queryString,
													   ParamListInfo params);
extern PGDLLIMPORT ExplainOneQuery_hook_type ExplainOneQuery_hook;

/* Hook for plugins to get control in explain_get_index_name() */
typedef const char *(*explain_get_index_name_hook_type) (Oid indexId);
extern PGDLLIMPORT explain_get_index_name_hook_type explain_get_index_name_hook;


extern void ExplainQuery(ExplainStmt *stmt, const char *queryString,
			 ParamListInfo params, DestReceiver *dest);

extern void ExplainInitState(ExplainState *es);

extern TupleDesc ExplainResultDesc(ExplainStmt *stmt);

extern void ExplainOneUtility(Node *utilityStmt, ExplainState *es,
				  const char *queryString, ParamListInfo params);

extern void ExplainOnePlan(PlannedStmt *plannedstmt, ExplainState *es,
				const char *queryString,  ParamListInfo params);

extern void ExplainPrintPlan(ExplainState *es, QueryDesc *queryDesc);

extern void ExplainSeparatePlans(ExplainState *es);

#endif   /* EXPLAIN_H */
