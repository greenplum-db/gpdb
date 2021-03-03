/*-------------------------------------------------------------------------
 *
 * tablefuncapi.h
 *	  Declarations for Table Function API
 *
 * Portions Copyright (c) 2011, EMC corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/tablefuncapi.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TABLEFUNCAPI_H
#define TABLEFUNCAPI_H

#include "fmgr.h"
#include "access/tupdesc.h"
#include "access/htup.h"

typedef struct AnyTableData *AnyTable;

#define DatumGetAnyTable(d)   ((AnyTable) DatumGetPointer(d))
#define AnyTableGetDatum(x)   PointerGetDatum(x)

#define PG_GETARG_ANYTABLE(n) (DatumGetAnyTable(PG_GETARG_DATUM(n)))

#define TF_SET_USERDATA(userdata) tf_set_userdata_internal(fcinfo, userdata)
#define TF_GET_USERDATA() tf_get_userdata_internal(fcinfo)

extern TupleDesc AnyTable_GetTupleDesc(AnyTable);
extern HeapTuple AnyTable_GetNextTuple(AnyTable);

extern void tf_set_userdata_internal(FunctionCallInfo fcinfo, bytea *userdata);
extern bytea *tf_get_userdata_internal(FunctionCallInfo fcinfo);

#endif
