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
	RelFileNodeBackend  rnodebackend;

	rnodebackend.node.spcNode = PG_GETARG_OID(0);
	rnodebackend.node.dbNode  = PG_GETARG_OID(1);
	rnodebackend.node.relNode = PG_GETARG_OID(2);

	rnodebackend.backend = InvalidBackendId; /* not temporary/local */

	DropRelFileNodeBuffers(rnodebackend, MAIN_FORKNUM, 0);

	PG_RETURN_BOOL(true);
}

