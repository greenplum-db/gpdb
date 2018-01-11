/*-------------------------------------------------------------------------
 *
 * analyze.h
 *		parse analysis for optimizable statements
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/analyze.h,v 1.40 2009/01/01 17:24:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef ANALYZE_H
#define ANALYZE_H

#include "parser/parse_node.h"


extern Query *parse_analyze(Node *parseTree, const char *sourceText,
			  Oid *paramTypes, int numParams);
extern Query *parse_analyze_varparams(Node *parseTree, const char *sourceText,
						Oid **paramTypes, int *numParams);
extern Query *parse_sub_analyze(Node *parseTree, ParseState *parentParseState);

extern List *analyzeCreateSchemaStmt(CreateSchemaStmt *stmt);

extern Query *transformStmt(ParseState *pstate, Node *parseTree);

extern bool analyze_requires_snapshot(Node *parseTree);

extern void CheckSelectLocking(Query *qry);
extern void applyLockingClause(Query *qry, Index rtindex,
				   bool forUpdate, bool noWait);

/* State shared by transformCreateStmt and its subroutines */
typedef struct
{
	const char *stmtType;		/* "CREATE TABLE" or "ALTER TABLE" */
	RangeVar   *relation;		/* relation to create */
	Relation	rel;			/* opened/locked rel, if ALTER */
	List	   *inhRelations;	/* relations to inherit from */
	bool		hasoids;		/* does relation have an OID column? */
	bool		isalter;		/* true if altering existing table */
	bool		iscreatepart;	/* true if create in service of creating a part */
	bool		issplitpart;
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

#define MaxPolicyAttributeNumber MaxHeapAttributeNumber

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
