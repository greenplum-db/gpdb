/*-------------------------------------------------------------------------
 *
 * postgres_fdw.h
 *		  Foreign-data wrapper for remote PostgreSQL servers
 *
 * Portions Copyright (c) 2012-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  contrib/postgres_fdw/postgres_fdw.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef POSTGRES_FDW_H
#define POSTGRES_FDW_H

#include "foreign/foreign.h"
#include "lib/stringinfo.h"
#include "nodes/relation.h"
#include "utils/rel.h"

/* postgres_fdw is compiled as a backend, it needs the server's
 * header files such as executor/tuptable.h. It also needs libpq
 * to connect to a remote postgres database, so it's statically
 * linked to libpq.a which is compiled as a frontend using
 * -DFRONTEND.
 *
 * But the struct PQconninfoOption's length is different between
 * backend and frontend, there is no "connofs" field in frontend.
 * When postgres_fdw calls the function "PQconndefaults" implemented
 * in libpq.a and uses the returned PQconninfoOption variable, it crashs,
 * because the PQconninfoOption variable returned by libpq.a doesn't contain
 * the "connofs" value, but the postgres_fdw thinks it has, so it crashes.
 *
 * We define FRONTEND here to include frontend libpq header files.
 */
#ifdef LIBPQ_FE_H
#error "postgres_fdw.h" must be included before "libpq-fe.h"
#endif /* LIBPQ_FE_H */

#ifndef FRONTEND
#define FRONTEND
#include "libpq-fe.h"
#undef FRONTEND
#else
#include "libpq-fe.h"
#endif /* FRONTEND */

/* in postgres_fdw.c */
extern int	set_transmission_modes(void);
extern void reset_transmission_modes(int nestlevel);

/* in connection.c */
extern PGconn *GetConnection(ForeignServer *server, UserMapping *user,
			  bool will_prep_stmt);
extern void ReleaseConnection(PGconn *conn);
extern unsigned int GetCursorNumber(PGconn *conn);
extern unsigned int GetPrepStmtNumber(PGconn *conn);
extern PGresult *pgfdw_get_result(PGconn *conn, const char *query);
extern PGresult *pgfdw_exec_query(PGconn *conn, const char *query);
extern void pgfdw_report_error(int elevel, PGresult *res, PGconn *conn,
				   bool clear, const char *sql);

/* in option.c */
extern int ExtractConnectionOptions(List *defelems,
						 const char **keywords,
						 const char **values);

/* in deparse.c */
extern void classifyConditions(PlannerInfo *root,
				   RelOptInfo *baserel,
				   List *input_conds,
				   List **remote_conds,
				   List **local_conds);
extern bool is_foreign_expr(PlannerInfo *root,
				RelOptInfo *baserel,
				Expr *expr);
extern void deparseSelectSql(StringInfo buf,
				 PlannerInfo *root,
				 RelOptInfo *baserel,
				 Bitmapset *attrs_used,
				 List **retrieved_attrs);
extern void appendWhereClause(StringInfo buf,
				  PlannerInfo *root,
				  RelOptInfo *baserel,
				  List *exprs,
				  bool is_first,
				  List **params);
extern void deparseInsertSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *targetAttrs, List *returningList,
				 List **retrieved_attrs);
extern void deparseUpdateSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *targetAttrs, List *returningList,
				 List **retrieved_attrs);
extern void deparseDeleteSql(StringInfo buf, PlannerInfo *root,
				 Index rtindex, Relation rel,
				 List *returningList,
				 List **retrieved_attrs);
extern void deparseAnalyzeSizeSql(StringInfo buf, Relation rel);
extern void deparseAnalyzeSql(StringInfo buf, Relation rel,
				  List **retrieved_attrs);

#endif   /* POSTGRES_FDW_H */
