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

#include "nodes/nodes.h"
#include "nodes/plannodes.h"
#include "nodes/pg_list.h"
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
#include "catalog/pg_proc.h"
#include "tcop/dest.h"
#include "commands/trigger.h"
#include "parser/parse_coerce.h"
#include "utils/selfuncs.h"
#include "utils/faultinjector.h"
#include "funcapi.h"

} // end extern C


#endif // GPDBDefs_H

// EOF
