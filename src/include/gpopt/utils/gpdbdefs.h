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

#include "access/heapam.h"
#include "access/relscan.h"
#include "catalog/namespace.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbutil.h"
#include "cdb/partitionselection.h"
#include "commands/defrem.h"
#include "commands/trigger.h"
#include "executor/execdesc.h"
#include "executor/nodeMotion.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "nodes/execnodes.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "nodes/plannodes.h"
#include "nodes/print.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "optimizer/walkers.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "tcop/dest.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/elog.h"
#include "utils/faultinjector.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "utils/uri.h"


extern List *pg_parse_and_rewrite(const char *query_string, Oid *paramTypes,
								  int iNumParams);

extern PlannedStmt *pg_plan_query(Query *pqueryTree, ParamListInfo boundParams);

extern char *get_rel_name(Oid relid);

extern Relation RelationIdGetRelation(Oid relationId);

extern void RelationClose(Relation relation);

extern Oid get_atttype(Oid relid, AttrNumber attnum);

extern RegProcedure get_opcode(Oid opid);

extern void ExecutorStart(QueryDesc *pqueryDesc, int iEFlags);

extern void ExecutorRun(QueryDesc *pqueryDesc, ScanDirection direction,
						long lCount);

extern void ExecutorEnd(QueryDesc *pqueryDesc);

}  // end extern C


#endif	// GPDBDefs_H

// EOF
