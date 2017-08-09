#include "postgres.h"
#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"

#include "access/heapam.h"
#include "storage/bufmgr.h"

PG_MODULE_MAGIC;

extern void flush_relation_buffers(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(flush_relation_buffers);

void
flush_relation_buffers(PG_FUNCTION_ARGS)
{
	Oid relid = PG_GETARG_OID(0);
	Relation r = heap_open(relid, AccessShareLock);
	FlushRelationBuffers(r);
	heap_close(r, AccessShareLock);
}

