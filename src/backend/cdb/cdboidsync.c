/*-------------------------------------------------------------------------
 *
 * cdboidsync.c
 *
 * Make sure we don't re-use oids already used on the segment databases
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdboidsync.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "miscadmin.h"
#include "utils/builtins.h"

#include "libpq-fe.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdboidsync.h"

static Oid
get_max_oid_from_segDBs(void)
{

	Oid oid = 0;
	Oid			tempoid = 0;
	int			i;
	CdbPgResults cdb_pgresults = {NULL, 0};

	const char *cmd = "select pg_highest_oid()";

	CdbDispatchCommand(cmd, DF_WITH_SNAPSHOT, &cdb_pgresults);

	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			elog(ERROR, "dboid: resultStatus not tuples_Ok");
		}
		else
		{
			Assert(PQntuples(cdb_pgresults.pg_results[i]) == 1);
			tempoid = atol(PQgetvalue(cdb_pgresults.pg_results[i], 0, 0));

			if (tempoid > oid)
				oid = tempoid;
		}
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);
	return oid;
}

Datum
pg_highest_oid(PG_FUNCTION_ARGS __attribute__((unused)))
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
		Oid			max_oid = get_max_oid_from_segDBs();

		/* Move our oid counter ahead of QEs */
		while (GetNewObjectId() <= max_oid);

		/* Burn a few extra just for safety */
		for (int i = 0; i < 10; i++)
			GetNewObjectId();
	}
}
