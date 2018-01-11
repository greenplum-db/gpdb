/*-------------------------------------------------------------------------
 *
 * cdb_seginst.c
 *
 * Structures and functions to read a join of cdb_seg, cdb_instance, and
 * gp_segment_instance tables using libpq
 *
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/bin/pg_dump/cdb/cdb_seginst.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "pqexpbuffer.h"
#include "libpq-fe.h"
#include <regex.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>
#include "cdb_dump_util.h"
#include "cdb_table.h"
#include "cdb_seginst.h"

/* FreeSegmentDatabaseArray: This function frees the memory allocated for ther pSegDBAr->pData array,
 * and the strings that were allocated for each element in the array
 * It does NOT free the pSegDBAr itself.
 */
void
FreeSegmentDatabaseArray(SegmentDatabaseArray *pSegDBAr)
{
	int			i;

	if (pSegDBAr == NULL)
		return;

	for (i = 0; i < pSegDBAr->count; i++)
	{
		SegmentDatabase *pSegDB = &pSegDBAr->pData[i];

		if (pSegDB->pszHost != NULL)
			free(pSegDB->pszHost);
	}

	if (pSegDBAr->pData != NULL)
		free(pSegDBAr->pData);

	pSegDBAr->count = 0;
	pSegDBAr->pData = NULL;
}

void
FreeRestorePairArray(RestorePairArray * restorePairAr)
{
	if (restorePairAr == NULL)
		return;

	if (restorePairAr->pData != NULL)
		free(restorePairAr->pData);

	restorePairAr->count = 0;
	restorePairAr->pData = NULL;
}

/* GetSegmentDatabaseArray: This function reads all active instance segment pairs
 * from the function gp_segment_instance_map().
 * It then uses the set specification fo filter out any instid, segid combinations
 * that don't match the set.  bExcludeHead = true is used for restoring, because
 * the head is restored first seperately from the rest of the databases.
 */
bool
GetDumpSegmentDatabaseArray(PGconn *pConn,
							int remote_version,
							SegmentDatabaseArray *pSegDBAr,
							ActorSet actors,
							char *dump_set_str,
							char *pszDBName,
							char *pszUserName,
							bool dataOnly,
							bool schemaOnly)
{
	bool		bRtn = true;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;
	int			ntups;
	int			count;
	int			i_dbid;
	int			i_content;
	int			i_host;
	int			i_port;
	int			i;
	int			j;
	int			x;
	int			dbidset_count = 0;
	int		   *dbidset = NULL;
	SegmentDatabase *pSegDB;

	pQry = createPQExpBuffer();

	appendPQExpBuffer(pQry, "SELECT"
					  " dbid,"
					  " content,"
					  " hostname,"
					  " port "
					  "FROM "
					  " gp_segment_configuration "
					  "WHERE role='p' "
					  "ORDER BY content DESC");

	pRes = PQexec(pConn, pQry->data);
	if (!pRes || PQresultStatus(pRes) != PGRES_TUPLES_OK)
	{
		mpp_err_msg("ERROR", "gp_dump", "query to obtain list of Greenplum segment databases failed: %s",
					PQerrorMessage(pConn));
		bRtn = false;
		goto cleanup;
	}

	ntups = PQntuples(pRes);
	if (ntups <= 0)
	{
		mpp_err_msg("ERROR", "gp_dump", "no Greenplum segment databases found on master segment schema");
		bRtn = false;
		goto cleanup;
	}

	count = ntups + 1;

	/*
	 * See how many set elements there are that were specified by HostIP or
	 * name.
	 */

	/*
	 * Allocate enough memory for all of them, even though some may be
	 * filtered out.
	 */
	pSegDBAr->count = 0;
	pSegDBAr->pData = (SegmentDatabase *) calloc(count, sizeof(SegmentDatabase));
	if (pSegDBAr->pData == NULL)
	{
		mpp_err_msg("ERROR", "gp_dump", "Unable to allocate memory for Greenplum segment/instances information\n");
		bRtn = false;
		goto cleanup;
	}

	/* get the column numbers */
	i_dbid = PQfnumber(pRes, "dbid");
	i_content = PQfnumber(pRes, "content");
	i_host = PQfnumber(pRes, "hostname");
	i_port = PQfnumber(pRes, "port");

	/*
	 * if an individual set of dbid's was requested to be dumped, parse it and
	 * check that it really exists and is a primary segment.
	 */
	if (actors == SET_INDIVIDUAL)
	{
		/* allocate dbidset. len is more than we need but it's a safe bet */
		dbidset = (int *) calloc(strlen(dump_set_str), sizeof(int));
		MemSet(dbidset, 0, strlen(dump_set_str) * sizeof(int));

		if (dbidset == NULL)
		{
			mpp_err_msg("ERROR", "gp_dump", "Unable to allocate memory for Greenplum dbid set information\n");
			bRtn = false;
			goto cleanup;
		}

		/* parse the user specified dbid list, return dbid count */
		dbidset_count = parseDbidSet(dbidset, dump_set_str);

		if (dbidset_count < 1)
		{
			bRtn = false;
			goto cleanup;
		}

		for (i = 0; i < dbidset_count; i++)
		{
			bool		match = false;

			for (j = 0; j < ntups; j++)
			{
				int			dbid = atoi(PQgetvalue(pRes, j, i_dbid));

				if (dbid == dbidset[i])
				{
					match = true;
					break;
				}
			}

			if (!match)
			{
				mpp_err_msg("ERROR", "gp_dump", "dbid %d is not a correct Greenplum segment databases "
						"entry, or is not a primary segment\n ", dbidset[i]);
				bRtn = false;
				goto cleanup;
			}
		}
	}

	mpp_err_msg("INFO", "gp_dump", "Preparing to dump the following segments:\n");

	/* Read through the results set (all primary segments) */
	x = 0;
	for (i = 0; i < ntups; i++)
	{
		int			dbid = atoi(PQgetvalue(pRes, i, i_dbid));
		int			content = atoi(PQgetvalue(pRes, i, i_content));
		bool		should_dump = false;

		/* in dataOnly we don't dump master information */
		if (dataOnly && content == -1 && actors != SET_INDIVIDUAL)
			continue;

		/* in schemaOnly we skip all but the master */
		if (schemaOnly && content != -1 && actors != SET_INDIVIDUAL)
			continue;

		if (actors == SET_INDIVIDUAL)
		{
			should_dump = false;

			for (j = 0; j < dbidset_count; j++)
			{
				if (dbid == dbidset[j])
					should_dump = true;
			}

			if (!should_dump)
				continue;
		}

		pSegDB = &pSegDBAr->pData[x++];
		pSegDB->dbid = dbid;
		pSegDB->content = content;
		pSegDB->role = (content == -1 ? ROLE_MASTER : ROLE_SEGDB);
		pSegDB->port = atoi(PQgetvalue(pRes, i, i_port));
		pSegDB->pszHost = strdup(PQgetvalue(pRes, i, i_host));
		pSegDB->pszDBName = pszDBName;
		pSegDB->pszDBUser = pszUserName;
		pSegDB->pszDBPswd = PQpass(pConn);

		if (pSegDB->role == ROLE_MASTER)
			mpp_err_msg("INFO", "gp_dump", "Master (dbid 1)\n");
		else
			mpp_err_msg("INFO", "gp_dump", "Segment %d (dbid %d)\n",
						pSegDB->content,
						pSegDB->dbid);
	}

	/* set the count to be the number that passed the set inclusion test */
	pSegDBAr->count = x;

cleanup:
	destroyPQExpBuffer(pQry);
	if (pRes != NULL)
		PQclear(pRes);

	return bRtn;
}

bool
GetRestoreSegmentDatabaseArray(PGconn *pConn,
							   RestorePairArray * restorePairAr,
							   BackupLoc backupLocation,
							   char *restore_set_str,
							   bool dataOnly)
{
	bool		bRtn = true;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;
	int			ntups;
	int			count;
	int			i_dbid;
	int			i_content;
	int			i_host;
	int			i_port;
	int			i;
	int			j;
	int			x;
	int			dbidset_count = 0;
	int		   *dbidset = NULL;
	SegmentDatabase *sourceSegDB;
	SegmentDatabase *targetSegDB;

	pQry = createPQExpBuffer();

	appendPQExpBuffer(pQry, "SELECT"
					  " dbid,"
					  " content,"
					  " hostname,"
					  " port "
					  "FROM "
					  " gp_segment_configuration "
					  "WHERE role='p' "
					  "ORDER BY content DESC");

	pRes = PQexec(pConn, pQry->data);
	if (!pRes || PQresultStatus(pRes) != PGRES_TUPLES_OK)
	{
		mpp_err_msg("ERROR", "gp_restore", "query to obtain list of Greenplum segment databases failed: %s",
					PQerrorMessage(pConn));
		bRtn = false;
		goto cleanup;
	}

	ntups = PQntuples(pRes);
	if (ntups <= 0)
	{
		mpp_err_msg("ERROR", "gp_restore", "no Greenplum segment databases found on master segment schema");
		bRtn = false;
		goto cleanup;
	}

	count = ntups + 1;

	/*
	 * Allocate enough memory for all of them, even though some may be
	 * filtered out.
	 */
	restorePairAr->count = 0;
	restorePairAr->pData = (RestorePair *) calloc(count, sizeof(RestorePair));
	if (restorePairAr->pData == NULL)
	{
		mpp_err_msg("ERROR", "gp_restore", "Unable to allocate memory for Greenplum segment database information\n");
		bRtn = false;
		goto cleanup;
	}

	/* get the column numbers */
	i_dbid = PQfnumber(pRes, "dbid");
	i_content = PQfnumber(pRes, "content");
	i_host = PQfnumber(pRes, "hostname");
	i_port = PQfnumber(pRes, "port");

	/*
	 * if the dump file is on individual databases, parse the list of dbid's
	 * where those files exist.
	 */
	if (backupLocation == FILE_ON_INDIVIDUAL)
	{
		/* allocate dbidset. len is more than we need but it's a safe bet */
		dbidset = (int *) calloc(strlen(restore_set_str), sizeof(int));
		MemSet(dbidset, 0, strlen(restore_set_str) * sizeof(int));

		if (dbidset == NULL)
		{
			mpp_err_msg("ERROR", "gp_restore", "Unable to allocate memory for Greenplum dbidset information\n");
			bRtn = false;
			goto cleanup;
		}

		/* parse the user specified dbid list, return dbid count */
		dbidset_count = parseDbidSet(dbidset, restore_set_str);

		if (dbidset_count < 1)
		{
			bRtn = false;
			goto cleanup;
		}

		for (i = 0; i < dbidset_count; i++)
		{
			bool		match = false;

			for (j = 0; j < ntups; j++)
			{
				int			dbid = atoi(PQgetvalue(pRes, j, i_dbid));

				if (dbid == dbidset[i])
				{
					match = true;
					break;
				}
			}

			if (!match)
			{
				mpp_err_msg("ERROR", "gp_restore", "dbid %d is not a primary Greenplum segment databases entry\n",
							dbidset[i]);
				bRtn = false;
				goto cleanup;
			}
		}
	}

	mpp_err_msg("INFO", "gp_restore", "Preparing to restore the following segments:\n");

	/* Read through the results set.  */
	x = 0;
	for (i = 0; i < ntups; i++)
	{
		int			dbid = atoi(PQgetvalue(pRes, i, i_dbid));
		int			contentid = atoi(PQgetvalue(pRes, i, i_content));
		bool		should_restore = false;

		/* if dataOnly don't restore the master (table definitions) */
		if (dataOnly && contentid == -1)
			continue;

		if (backupLocation == FILE_ON_INDIVIDUAL)
		{
			should_restore = false;

			for (j = 0; j < dbidset_count; j++)
			{
				if (dbid == dbidset[j])
					should_restore = true;
			}

			if (!should_restore)
				continue;
		}

		targetSegDB = &restorePairAr->pData[x].segdb_target;
		targetSegDB->dbid = dbid;
		targetSegDB->role = (contentid == -1 ? ROLE_MASTER : ROLE_SEGDB);
		targetSegDB->port = atoi(PQgetvalue(pRes, i, i_port));
		targetSegDB->pszHost = strdup(PQgetvalue(pRes, i, i_host));
		targetSegDB->pszDBName = PQdb(pConn);
		targetSegDB->pszDBUser = PQuser(pConn);
		targetSegDB->pszDBPswd = PQpass(pConn);
		targetSegDB->content = contentid;

		sourceSegDB = &restorePairAr->pData[x].segdb_source;
		sourceSegDB->dbid = targetSegDB->dbid;
		sourceSegDB->role = targetSegDB->role;
		sourceSegDB->port = targetSegDB->port;
		sourceSegDB->pszHost = targetSegDB->pszHost;
		sourceSegDB->pszDBName = targetSegDB->pszDBName;
		sourceSegDB->pszDBUser = targetSegDB->pszDBUser;
		if (targetSegDB->pszDBPswd)
			sourceSegDB->pszDBPswd = targetSegDB->pszDBPswd;

		if (targetSegDB->role == ROLE_MASTER)
			mpp_err_msg("INFO", "gp_restore", "Master (dbid 1)\n");
		else
			mpp_err_msg("INFO", "gp_restore", "Segment %d (dbid %d)\n",
						contentid,
						targetSegDB->dbid);

		x++;
	}

	/* set the count to be the number that passed the set inclusion test */
	restorePairAr->count = x;

cleanup:
	destroyPQExpBuffer(pQry);
	if (pRes != NULL)
		PQclear(pRes);

	return bRtn;
}
