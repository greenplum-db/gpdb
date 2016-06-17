/*-------------------------------------------------------------------------
 *
 * parse_partition.h
 *	  handle partition clauses in parser
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/parse/parse_partition.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_PARTITION_H
#define PARSE_PARTITION_H

#include "parser/analyze.h"
#include "parser/parse_node.h"

extern void transformPartitionBy(ParseState *pstate, CreateStmtContext *cxt,
					 CreateStmt *stmt, Node *partitionBy, GpPolicy *policy);
extern void PartitionRangeItemIsValid(ParseState *pstate, PartitionRangeItem *pri);
extern Node *coerce_partition_value(Node *node, Oid typid, int32 typmod,
					   PartitionByType partype);

#endif   /* PARSE_PARTITION_H */
