/*-------------------------------------------------------------------------
 *
 * analyze.h
 *		parse analysis for optimizable statements
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/analyze.h,v 1.34.2.1 2008/12/13 02:00:53 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/parse_node.h"


/* fwd declarations */
struct GpPolicy;

extern List *parse_analyze(Node *parseTree, const char *sourceText,
			  Oid *paramTypes, int numParams);
extern List *parse_analyze_varparams(Node *parseTree, const char *sourceText,
						Oid **paramTypes, int *numParams);
extern List *parse_sub_analyze(Node *parseTree, ParseState *parentParseState);
extern bool analyze_requires_snapshot(Node *parseTree);
extern List *analyzeCreateSchemaStmt(CreateSchemaStmt *stmt);
extern void CheckSelectLocking(Query *qry);
extern void applyLockingClause(Query *qry, Index rtindex,
				   bool forUpdate, bool noWait);

/* State shared by transformCreateStmt and its subroutines */
typedef struct
{
	const char *stmtType;		/* "CREATE TABLE" or "ALTER TABLE" */
	RangeVar   *relation;		/* relation to create */
	List	   *inhRelations;	/* relations to inherit from */
	bool		hasoids;		/* does relation have an OID column? */
	bool		isalter;		/* true if altering existing table */
	bool		isaddpart;		/* true if create in service of adding a part */
	List	   *columns;		/* ColumnDef items */
	List	   *ckconstraints;	/* CHECK constraints */
	List	   *fkconstraints;	/* FOREIGN KEY constraints */
	List	   *ixconstraints;	/* index-creating constraints */
	List	   *inh_indexes;	/* cloned indexes from INCLUDING INDEXES */
	List	   *blist;			/* "before list" of things to do before
								 * creating the table */
	List	   *alist;			/* "after list" of things to do after creating
								 * the table */
	List	   *dlist;			/* "deferred list" of utility statements to 
								 * transfer to the list CreateStmt->deferredStmts
								 * for later parse_analyze and dispatch */
	IndexStmt  *pkey;			/* PRIMARY KEY index, if any */

	MemoryContext tempCtx;
} CreateStmtContext;

Query *transformCreateStmt(ParseState *pstate, CreateStmt *stmt,
						   List **extras_before, List **extras_after);

int validate_partition_spec(ParseState 			*pstate,
							CreateStmtContext 	*cxt, 
							CreateStmt 			*stmt, 
							PartitionBy 		*partitionBy, 	
							char	   			*at_depth,
							int					 partNumber);

extern bool is_aocs(List *opts);

List *transformStorageEncodingClause(List *options);
List *TypeNameGetStorageDirective(TypeName *typname);
extern List * form_default_storage_directive(List *enc);

#endif   /* ANALYZE_H */
