/*-------------------------------------------------------------------------
 *
 * cdbrelsize.c
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbrelsize.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "catalog/catalog.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "libpq-fe.h"
#include "lib/stringinfo.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"

#include "cdb/cdbrelsize.h"

/*
 * Get the max size of the relation across segments
 */
int64
cdbRelMaxSegSize(Relation rel)
{
	int64		size = 0;
	int			i;
	CdbPgResults cdb_pgresults = {NULL, 0};
	StringInfoData buffer;

	/*
	 * Let's ask the QEs for the size of the relation
	 */
	initStringInfo(&buffer);

	/*
	 * Relation Oids are assumed to be in sync in all nodes.
	 */
	appendStringInfo(&buffer, "select pg_relation_size(%u)",
					 RelationGetRelid(rel));

	CdbDispatchCommand(buffer.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

	for (i = 0; i < cdb_pgresults.numResults; i++)
	{
		struct pg_result *pgresult = cdb_pgresults.pg_results[i];

		if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			elog(ERROR, "cdbRelMaxSegSize: resultStatus not tuples_Ok: %s %s",
				 PQresStatus(PQresultStatus(pgresult)), PQresultErrorMessage(pgresult));
		}
		else
		{
			Assert(PQntuples(pgresult) == 1);
			int64		tempsize = 0;

			(void) scanint8(PQgetvalue(pgresult, 0, 0), false, &tempsize);
			if (tempsize > size)
				size = tempsize;
		}
	}

	pfree(buffer.data);

	cdbdisp_clearCdbPgResults(&cdb_pgresults);

	return size;
}
