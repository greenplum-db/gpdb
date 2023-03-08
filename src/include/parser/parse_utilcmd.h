/*-------------------------------------------------------------------------
 *
 * parse_utilcmd.h
 *		parse analysis for utility commands
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/parser/parse_utilcmd.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_UTILCMD_H
#define PARSE_UTILCMD_H

#include "nodes/parsenodes.h"
#include "parser/analyze.h"
#include "parser/parse_node.h"


extern List *transformCreateStmt(CreateStmt *stmt, const char *queryString);
extern List *transformCreateExternalStmt(CreateExternalStmt *stmt, const char *queryString);
extern List *transformAlterTableStmt(Oid relid, AlterTableStmt *stmt,
									 const char *queryString);
extern IndexStmt *transformIndexStmt(Oid relid, IndexStmt *stmt,
									 const char *queryString);
extern void transformRuleStmt(RuleStmt *stmt, const char *queryString,
							  List **actions, Node **whereClause);
extern List *transformCreateSchemaStmt(CreateSchemaStmt *stmt);
extern PartitionBoundSpec *transformPartitionBound(ParseState *pstate, Relation parent,
												   PartitionBoundSpec *spec);
extern List *expandTableLikeClause(RangeVar *heapRel,
								   TableLikeClause *table_like_clause);
extern IndexStmt *generateClonedIndexStmt(RangeVar *heapRel,
										  Relation source_idx,
										  const AttrNumber *attmap, int attmap_length,
										  Oid *constraintOid);

extern GpPolicy *getPolicyForDistributedBy(DistributedBy *distributedBy, TupleDesc tupdesc);


/* prototypes for functions in parse_partition.h */
extern Const *transformPartitionBoundValue(ParseState *pstate, Node *val,
										   const char *colName, Oid colType, int32 colTypmod,
										   Oid partCollation);
extern List *generatePartitions(Oid parentrelid, GpPartitionDefinition *gpPartSpec,
								PartitionSpec *subPartSpec,
								const char *queryString, List *parentoptions,
								const char *parentaccessmethod,
								List *parentattenc, bool addpartition);
GpPartitionDefinition *
transformGpPartitionDefinition(Oid parentrelid, const char *queryString,
							   GpPartitionDefinition *gpPartDef_orig);
extern void convert_exclusive_start_inclusive_end(Const *constval, Oid part_col_typid,
												  int32 part_col_typmod, bool is_exclusive_start);

typedef struct partname_comp
{
	const char *tablename;
	int level;
	int partnum;
} partname_comp;

extern CreateStmt *makePartitionCreateStmt(Relation parentrel, char *partname,
										   PartitionBoundSpec *boundspec,
										   PartitionSpec *subPart,
										   GpPartDefElem *elem,
										   partname_comp *partnamecomp);
extern char *ChoosePartitionName(const char *parentname, int level,
								 Oid naemspaceId, const char *partname,
								 int partnum);

#endif							/* PARSE_UTILCMD_H */
