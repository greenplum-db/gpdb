/*-------------------------------------------------------------------------
 *
 * cdboidsync.c
 *
 * Make sure we don't re-use oids already used on the segment databases
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/transam.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_tablespace.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/relcache.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "miscadmin.h"
#include "storage/proc.h"
#include "utils/builtins.h"

#include "gp-libpq-fe.h"
#include "lib/stringinfo.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "cdb/cdboidsync.h"

static Oid
get_max_oid_from_segDBs(void)
{

	Oid	oid = 0;
	Oid tempoid = 0;
	int		i;
	int 	resultCount = 0;
	struct pg_result **results = NULL;
	StringInfoData buffer;
	StringInfoData errbuf;
		
	initStringInfo(&buffer);
	
	appendStringInfo(&buffer, "select pg_highest_oid()");
	
	initStringInfo(&errbuf);

	results = cdbdisp_dispatchRMCommand(buffer.data, true, &errbuf, &resultCount);

	if (errbuf.len > 0)
		ereport(ERROR, (errmsg("pg_highest_oid error (gathered %d results from cmd '%s')", resultCount, buffer.data),
						errdetail("%s", errbuf.data)));
										
	for (i = 0; i < resultCount; i++)
	{
		if (PQresultStatus(results[i]) != PGRES_TUPLES_OK)
		{
			elog(ERROR,"dboid: resultStatus not tuples_Ok");
		}
		else
		{
			Assert(PQntuples(results[i]) == 1);
			tempoid = atol(PQgetvalue(results[i], 0, 0));

			if (tempoid > oid)
				oid = tempoid;
		}
	}

	pfree(errbuf.data);

	for (i = 0; i < resultCount; i++)
		PQclear(results[i]);

	free(results);
	
	return oid;
}

Datum
pg_highest_oid(PG_FUNCTION_ARGS __attribute__((unused)) )
{
	Oid			result;
	Oid			max_from_segdbs;

	result = ShmemVariableCache->nextOid;
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		
		max_from_segdbs = get_max_oid_from_segDBs();
		
		if (max_from_segdbs > result)
			result = max_from_segdbs;
		
	}


	PG_RETURN_OID(result);

}

void
cdb_sync_oid_to_segments(void)
{
	if (Gp_role == GP_ROLE_DISPATCH && IsNormalProcessingMode())
	{
		int 	i;
		Oid		max_oid = get_max_oid_from_segDBs();
		
		/* Move our oid counter ahead of QEs */
		while(GetNewObjectId() <= max_oid);
		
		/* Burn a few extra just for safety */
		for (i=0;i<10;i++)
			GetNewObjectId();
	}
	
}

