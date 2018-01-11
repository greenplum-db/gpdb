//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		gpdbdefs.h
//
//	@doc:
//		C Linkage for GPDB functions used by GP optimizer
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDBDefs_H
#define GPDBDefs_H

extern "C" {

#include "postgres.h"
#include <string.h>
#include "nodes/nodes.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.h"
#include "nodes/print.h"
#include "nodes/pg_list.h"
#include "executor/execdesc.h"
#include "executor/nodeMotion.h"
#include "parser/parsetree.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/datum.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "optimizer/walkers.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parse_clause.h"
#include "parser/parse_oper.h"

#include "catalog/namespace.h"
#include "catalog/pg_exttable.h"
#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbmutate.h"
#include "commands/defrem.h"
#include "utils/typcache.h"
#include "utils/numeric.h"
#include "optimizer/tlist.h"
#include "optimizer/planmain.h"
#include "nodes/makefuncs.h"
#include "catalog/pg_operator.h"
#include "lib/stringinfo.h"
#include "utils/elog.h"
#include "utils/rel.h"
#include "utils/uri.h"
#include "access/relscan.h"
#include "access/heapam.h"
#include "catalog/pg_proc.h"
#include "tcop/dest.h"
#include "commands/trigger.h"
#include "parser/parse_coerce.h"
#include "utils/selfuncs.h"
#include "utils/faultinjector.h"
#include "funcapi.h"

extern
Query *preprocess_query_optimizer(Query *pquery, ParamListInfo boundParams);

extern
List *pg_parse_and_rewrite(const char *query_string, Oid *paramTypes, int iNumParams);

extern
PlannedStmt *pg_plan_query(Query *pqueryTree, ParamListInfo boundParams);

extern
char * get_rel_name(Oid relid);

extern
Relation RelationIdGetRelation(Oid relationId);

extern
void RelationClose(Relation relation);

extern
Oid get_atttype(Oid relid, AttrNumber attnum);

extern
RegProcedure get_opcode(Oid opid);

extern
void ExecutorStart(QueryDesc *pqueryDesc, int iEFlags);

extern
void ExecutorRun(QueryDesc *pqueryDesc, ScanDirection direction, long lCount);

extern
void ExecutorEnd(QueryDesc *pqueryDesc);

extern
void ExecCheckRTPerms(List *rangeTable);

extern
void ExecCheckRTEPerms(RangeTblEntry *rte);

} // end extern C


#endif // GPDBDefs_H

// EOF
