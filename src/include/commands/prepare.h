/*-------------------------------------------------------------------------
 *
 * prepare.h
 *	  PREPARE, EXECUTE and DEALLOCATE commands, and prepared-stmt storage
 *
 *
 * Copyright (c) 2002-2008, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/include/commands/prepare.h,v 1.23 2007/01/05 22:19:53 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PREPARE_H
#define PREPARE_H

#include "executor/execdesc.h"
#include "utils/timestamp.h"
#include "utils/resscheduler.h"

struct TupOutputState;                  /* #include "executor/executor.h" */

/*
 * The data structure representing a prepared statement
 *
 * A prepared statement might be fully planned, or only parsed-and-rewritten.
 * If fully planned, stmt_list contains PlannedStmts and/or utility statements;
 * if not, it contains Query nodes.
 *
 * Note: all subsidiary storage lives in the context denoted by the context
 * field.  However, the string referenced by commandTag is not subsidiary
 * storage; it is assumed to be a compile-time-constant string.  As with
 * portals, commandTag shall be NULL if and only if the original query string
 * (before rewriting) was an empty string.
 */
typedef struct
{
	/* dynahash.c requires key to be first field */
	char		stmt_name[NAMEDATALEN];
	char	   *query_string;	/* text of query, or NULL */
	NodeTag		sourceTag;		/* GPDB: Original statement NodeTag */
	const char *commandTag;		/* command tag (a constant!), or NULL */
	List	   *query_list;		/* list of queries, rewritten, in case of replan */
	List	   *argtype_list;	/* list of parameter type OIDs */
	TimestampTz prepare_time;	/* the time when the stmt was prepared */
	bool		take_lock;  /* Take a Resource Queue Slot lock */
	bool		lock_isacquired;  /* If Resource Queue Slot lock is already taken */
	bool		from_sql;		/* stmt prepared via SQL, not FE/BE protocol? */
	ResPortalIncrement *incData;  /* For case when Resource Queue Slot lock has to be taken */
	MemoryContext context;		/* context containing this query */
} PreparedStatement;


/* Utility statements PREPARE, EXECUTE, DEALLOCATE, EXPLAIN EXECUTE */
extern void PrepareQuery(PrepareStmt *stmt, const char *queryString);
extern void ExecuteQuery(ExecuteStmt *stmt, const char *queryString, 
             ParamListInfo params,
			 DestReceiver *dest, char *completionTag);
extern void DeallocateQuery(DeallocateStmt *stmt);
extern void ExplainExecuteQuery(ExecuteStmt *execstmt, ExplainStmt *stmt,
					const char * queryString,
					ParamListInfo params,
					struct TupOutputState *tstate);

/* Low-level access to stored prepared statements */
extern void StorePreparedStatement(const char *stmt_name,
					   const char *query_string,
					   NodeTag	   sourceTag, /* GPDB */
					   const char *commandTag,
					   List *query_list,
					   List *argtype_list,
					   bool takeLock,
					   ResPortalIncrement *incData,
					   bool from_sql);
extern PreparedStatement *FetchPreparedStatement(const char *stmt_name,
					   bool throwError);
extern void DropPreparedStatement(const char *stmt_name, bool showError);
extern List *FetchPreparedStatementParams(const char *stmt_name);
extern TupleDesc FetchPreparedStatementResultDesc(PreparedStatement *stmt);
extern bool PreparedStatementReturnsTuples(PreparedStatement *stmt);
extern List *FetchPreparedStatementTargetList(PreparedStatement *stmt);

#endif   /* PREPARE_H */
