#include "postgres.h"
#include "funcapi.h"
#include "nodes/pg_list.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum invalidate_buffers(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(invalidate_buffers);
Datum
invalidate_buffers(PG_FUNCTION_ARGS)
{
	RelFileNode			rnode;

	rnode.spcNode = PG_GETARG_OID(0);
	rnode.dbNode  = PG_GETARG_OID(1);
	rnode.relNode = PG_GETARG_OID(2);

	DropRelFileNodeBuffers(rnode, MAIN_FORKNUM, false, 0);

	PG_RETURN_BOOL(true);
}

