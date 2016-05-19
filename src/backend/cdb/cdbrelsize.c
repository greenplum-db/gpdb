/*-------------------------------------------------------------------------
 *
 * cdbrelsize.h
 *
 * Get the max size of the relation across the segDBs
 *
 * Copyright (c) 2006-2008, Greenplum inc
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "catalog/catalog.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "cdb/cdbdisp_query.h"
#include "gp-libpq-fe.h"
#include "lib/stringinfo.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"

#include "cdb/cdbrelsize.h"

#define relsize_cache_size 100

struct relsize_cache_entry 
{
	Oid	relOid;
	int64 size;
};

static struct relsize_cache_entry relsize_cache[relsize_cache_size] = { {0,0} };

static int last_cache_entry = -1;		/* -1 for cache not initialized yet */

void clear_relsize_cache(void)
{
	int i;
	for (i=0; i < relsize_cache_size; i++)
	{
		relsize_cache[i].relOid = InvalidOid;
		relsize_cache[i].size = 0;
	}
	last_cache_entry = -1;
}

int64 cdbRelSize(Relation rel)
{
	int64	size = 0;
	int		i;
	int 	resultCount = 0;
	struct pg_result **results = NULL;
	StringInfoData errbuf;
	StringInfoData buffer;

	char	*schemaName;
	char	*relName;	

	if (last_cache_entry  >= 0)
	{
		for (i=0; i < relsize_cache_size; i++)
		{
			if (relsize_cache[i].relOid == RelationGetRelid(rel))
				return relsize_cache[i].size;
		}
	}

	/*
	 * Let's ask the QEs for the size of the relation
	 */
	initStringInfo(&buffer);
	initStringInfo(&errbuf);

	schemaName = get_namespace_name(RelationGetNamespace(rel));
	if (schemaName == NULL)
		elog(ERROR, "cache lookup failed for namespace %d",
			 RelationGetNamespace(rel));
	relName = RelationGetRelationName(rel);

	/* 
	 * Safer to pass names than oids, just in case they get out of sync between QD and QE,
	 * which might happen with a toast table or index, I think (but maybe not)
	 */
	appendStringInfo(&buffer, "select pg_relation_size('%s.%s')",
					 quote_identifier(schemaName), quote_identifier(relName));

	/* 
	 * In the future, it would be better to send the command to only one QE for the optimizer's needs,
	 * but for ALTER TABLE, we need to be sure if the table has any rows at all.
	 */
	results = cdbdisp_dispatchRMCommand(buffer.data, true, &errbuf, &resultCount);

	if (errbuf.len > 0)
	{
		ereport(WARNING, (errmsg("cdbRelSize error (gathered %d results from cmd '%s')", resultCount, buffer.data),
						  errdetail("%s", errbuf.data)));
		pfree(errbuf.data);
		pfree(buffer.data);
		
		return -1;
	}
	else
	{
										
		for (i = 0; i < resultCount; i++)
		{
			if (PQresultStatus(results[i]) != PGRES_TUPLES_OK)
			{
				elog(ERROR,"cdbRelSize: resultStatus not tuples_Ok: %s   %s",PQresStatus(PQresultStatus(results[i])),PQresultErrorMessage(results[i]));
			}
			else
			{
				/*
				 * Due to funkyness in the current dispatch agent code, instead of 1 result 
				 * per QE with 1 row each, we can get back 1 result per dispatch agent, with
				 * one row per QE controlled by that agent.
				 */
				int j;
				for (j = 0; j < PQntuples(results[i]); j++)
				{
					int64 tempsize = 0;
					(void) scanint8(PQgetvalue(results[i], j, 0), false, &tempsize);
					if (tempsize > size)
					 	size = tempsize;
				}
			}
		}
	
		pfree(errbuf.data);
		pfree(buffer.data);

		for (i = 0; i < resultCount; i++)
			PQclear(results[i]);
	
		free(results);
	}

	if (size >= 0)	/* Cache the size even if it is zero, as table might be empty */
	{
		if (last_cache_entry < 0)
			last_cache_entry = 0;

		relsize_cache[last_cache_entry].relOid = RelationGetRelid(rel);
		relsize_cache[last_cache_entry].size = size;
		last_cache_entry = (last_cache_entry+1) % relsize_cache_size;
	}

	return size;
}
