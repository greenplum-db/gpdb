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
	Oid			oid = 0;
	Oid			tempoid;
	int			i;
	CdbPgResults cdb_pgresults = {NULL, 0};

	const char *cmd = "select pg_catalog.pg_highest_oid()";

	CdbDispatchCommand(cmd, DF_WITH_SNAPSHOT, &cdb_pgresults);

	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		PGresult   *res = cdb_pgresults.pg_results[i];
		int			ntuples;
		int			nfields;

		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			char	   *msg = pstrdup(PQresultErrorMessage(res));

			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("could not get highest OID from segment"),
					 errdetail("%s", msg)));
		}

		ntuples = PQntuples(res);
		nfields = PQnfields(res);
		if (ntuples != 1 || nfields != 1)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("unexpected result set received from pg_highest_oid()"),
					 errdetail("Result set had %d rows and %d columns",
							   ntuples, nfields)));
		}

		tempoid = DatumGetObjectId(
			DirectFunctionCall1(oidin,
								CStringGetDatum(PQgetvalue(res, 0, 0))));

		/* XXX: This doesn't do the right thing at OID wraparound. */
		if (tempoid > oid)
			oid = tempoid;
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
