/*-------------------------------------------------------------------------
 *
 * cdboidsync.c
 *
 * Make sure we don't re-use oids already used on the segment databases
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

	Assert(Gp_role == GP_ROLE_DISPATCH);

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

		/*
		 * We take the *numerically* maximum OID among the primaries.
		 *
		 * It might be tempting to find the "logically highest" OID among the
		 * primaries because we do pair-wise OID logical comparison
		 * elsewhere. However, that "logically maximum" of N Oids is undefined
		 * for N > 2 primaries. This is because "logically precedes" is not a
		 * transitive relationship.
		 *
		 * For example, take into consideration this set of four Oids:
		 * {0, 1<<30, 1<<31, 3 * (1 << 30)}.
		 */
		if (tempoid > oid)
			oid = tempoid;
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);
	return oid;
}

Datum
pg_highest_oid(PG_FUNCTION_ARGS pg_attribute_unused())
{
	Oid			result;
	Oid			max_from_segdbs;

	result = ShmemVariableCache->nextOid;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		max_from_segdbs = get_max_oid_from_segDBs();

		/*
		 * Return the logically larger Oid between the numeric maximum of the
		 * primaries and the master's Oid counter. This is not 100% accurate
		 * because the primaries can be in a wide range of Oids... but this is
		 * good enough for the majority of production clusters.
		 */
		if (OidFollowsNextOid(max_from_segdbs))
			result = max_from_segdbs;
	}

	PG_RETURN_OID(result);
}

void
cdb_sync_oid_to_segments(void)
{
	if (Gp_role == GP_ROLE_DISPATCH && IsNormalProcessingMode())
	{
		Oid max_oid_from_primaries = get_max_oid_from_segDBs();

		AdvanceObjectId(max_oid_from_primaries + 1);
	}
}
