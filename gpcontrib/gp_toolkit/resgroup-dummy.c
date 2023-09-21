#include "postgres.h"

#include "funcapi.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_resgroup_get_iostats);

Datum
pg_resgroup_get_iostats(PG_FUNCTION_ARGS)
{
	elog(WARNING, "resource group is not supported on this system");
	PG_RETURN_NULL();
}
