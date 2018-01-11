/*-------------------------------------------------------------------------
 *
 * cdb_table.c
 *
 * Structures and functions to read cdb tables using libpq.
 *
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/bin/pg_dump/cdb/cdb_table.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "pqexpbuffer.h"
#include "libpq-fe.h"
#include "cdb_dump_util.h"
#include "cdb_table.h"

/*-------------------------------------------------------------------------
 *
 * FreeCDBBaseTableArray( CDBBaseTableArray* pTableAr ) returns void
 *
 * This frees the pointers allocated in each of the CBBBaseTable structs
 * that are part of the pTableAr->pData array, and then frees the pData
 * pointer itself.	It does NOT free the pTableAr.
 *-------------------------------------------------------------------------
 */

void
FreeCDBBaseTableArray(CDBBaseTableArray *pTableAr)
{
	int			i;

	if (pTableAr == NULL)
		return;

	for (i = 0; i < pTableAr->count; i++)
	{
		CDBBaseTable *pBaseTable = &pTableAr->pData[i];

		if (pBaseTable->pszTableName != NULL)
			free(pBaseTable->pszTableName);
		if (pBaseTable->pszNspName != NULL)
			free(pBaseTable->pszNspName);
	}

	if (pTableAr->pData != NULL)
		free(pTableAr->pData);

	pTableAr->count = 0;
	pTableAr->pData = NULL;
}

/*-------------------------------------------------------------------------
 *
 * FreeCDBSegmentInstanceArray( CDBSegmentInstanceArray* pSegmentInstanceAr ) returns void
 *
 * This frees the pointers allocated in each of the CDBSegmentInstance structs
 * that are part of the pSegmentInstanceAr->pData array, and then frees the pData
 * pointer itself.	It does NOT free the pSegmentInstanceAr.
 *-------------------------------------------------------------------------
 */
void
FreeCDBSegmentInstanceArray(CDBSegmentInstanceArray *pSegmentInstanceAr)
{
	if (pSegmentInstanceAr == NULL)
		return;

	if (pSegmentInstanceAr->pData != NULL)
	{
		free(pSegmentInstanceAr->pData);
		pSegmentInstanceAr->pData = NULL;
	}

	pSegmentInstanceAr->count = 0;
}

/*-------------------------------------------------------------------------
 *
 * GetCDBBaseTableArray(PGconn* pConn, const char* pszDBName, CDBBaseTableArray* pTableAr )
 * returns bool
 *
 * This reads all rows from cdb_basetable, with an optional filter on partitioned.
 *
 * After using it, call FreeCDBBaseTableArray.
 *-------------------------------------------------------------------------
 */

bool
GetCDBBaseTableArray(PGconn *pConn, CDBBaseTableArray *pTableAr)
{
	bool		bRtn = false;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;
	int			ntups;
	int			i_tbloid;
	int			i_relname;
	int			i_nspname;
	int			i;

	pTableAr->count = 0;
	pTableAr->pData = NULL;

	pQry = createPQExpBuffer();
	appendPQExpBuffer(pQry, " SELECT"
					  "	p.localoid,"
					  "	c.relname,"
					  "	n.nspname"
					  "	FROM"
					  "	gp_distribution_policy p,"
					  "	pg_class c,"
					  "	pg_namespace n"
					  "	WHERE"
					  "	p.localoid = c.oid"
					  " 	AND c.relnamespace = n.oid"
					  "	AND NOT c.relkind = 'x'"
					  "	ORDER BY"
					  "	2");

	pRes = PQexec(pConn, pQry->data);

	if (pRes == NULL || PQresultStatus(pRes) != PGRES_TUPLES_OK)
	{
		mpp_err_msg("ERROR", "mpp_table", "query to obtain list of Greenplum managed tables failed : %s",
					PQerrorMessage(pConn));
		goto cleanup;
	}

	ntups = PQntuples(pRes);

	pTableAr->count = ntups;
	pTableAr->pData = (CDBBaseTable *) calloc(ntups, sizeof(CDBBaseTable));
	if (pTableAr->pData == NULL)
	{
		mpp_err_msg("ERROR", "mpp_table", "cannot allocate memory for query results in GetCDBBaseTableArray\n");
		goto cleanup;
	}

	/* get column numbers */
	i_tbloid = PQfnumber(pRes, "localoid");
	i_relname = PQfnumber(pRes, "relname");
	i_nspname = PQfnumber(pRes, "nspname");

	for (i = 0; i < ntups; i++)
	{
		CDBBaseTable *pBaseTable = &pTableAr->pData[i];

		pBaseTable->tbloid = atoi(PQgetvalue(pRes, i, i_tbloid));
		pBaseTable->pszTableName = strdup(PQgetvalue(pRes, i, i_relname));
		pBaseTable->pszNspName = strdup(PQgetvalue(pRes, i, i_nspname));
	}

	bRtn = true;

cleanup:
	if (pRes != NULL)
		PQclear(pRes);
	destroyPQExpBuffer(pQry);

	return bRtn;
}

/*-------------------------------------------------------------------------
 *
 * GetCDBSegmentInstanceArray(PGconn* pConn, const char* pszDBName, CDBSegmentInstanceArray* pSegmentInstanceAr )
 * returns bool
 *
 * This reads all rows from gp_segment_instance.
 *
 * After using it, call FreeCDBSegmentInstanceArray.
 *-------------------------------------------------------------------------
 */
bool
GetCDBSegmentInstanceArray(PGconn *pConn, const char *pszDBName, CDBSegmentInstanceArray *pSegmentInstanceAr)
{
	int			ntups;
	int			i_dbid;
	int			i_isValid;
	int			i_hostname;
	int			i_port;
	int			i_datadir;
	int			i;

	bool		bRtn = false;
	PQExpBuffer pQry = NULL;
	PGresult   *pRes = NULL;

	pSegmentInstanceAr->count = 0;
	pSegmentInstanceAr->pData = NULL;

	pQry = createPQExpBuffer();
	appendPQExpBuffer(pQry, " SELECT"
					  "	d.dbid,"
					  "   d.valid,"
					  "   h.hostname,"
					  "   l.port,"
					  "   l.datadir"
					  "	FROM"
					  " mpp.pgdatabase d,"
					  "   mpp.listener l,"
					  "   mpp.host h"
					  "   where d.listenerid = l.listenerid"
					  "   and l.hostId = h.hostid");

	pRes = PQexec(pConn, pQry->data);
	if (pRes == NULL || PQresultStatus(pRes) != PGRES_TUPLES_OK)
	{
		mpp_err_msg("ERROR", "mpp_table" "query to obtain list of Greenplum segment databases failed : %s",
					PQerrorMessage(pConn));
		goto cleanup;
	}

	ntups = PQntuples(pRes);
	if (ntups <= 0)
	{
		mpp_err_msg("ERROR", "mpp_table" "no Greenplum segment databases for database \"%s\"\n", pszDBName);
		goto cleanup;
	}

	pSegmentInstanceAr->count = ntups;
	pSegmentInstanceAr->pData = (CDBSegmentInstance *) malloc(ntups * sizeof(CDBSegmentInstance));
	if (pSegmentInstanceAr->pData == NULL)
	{
		mpp_err_msg("ERROR", "mpp_table", "cannot allocate memory for query results in GetCDBSegmentInstanceArray\n");
		goto cleanup;
	}

	i_dbid = PQfnumber(pRes, "dbid");
	i_isValid = PQfnumber(pRes, "valid");
	i_hostname = PQfnumber(pRes, "hostname");
	i_port = PQfnumber(pRes, "port");
	i_datadir = PQfnumber(pRes, "datadir");

	for (i = 0; i < ntups; i++)
	{
		CDBSegmentInstance *pSegmentInstance = &pSegmentInstanceAr->pData[i];

		pSegmentInstance->segid = atoi(PQgetvalue(pRes, i, i_dbid));
		pSegmentInstance->isValid = (strcmp(PQgetvalue(pRes, i, i_isValid), "t") == 0);
		pSegmentInstance->dbname = strdup(pszDBName);
		pSegmentInstance->dbhostname = PQgetvalue(pRes, i, i_hostname);
		pSegmentInstance->dbport = atoi(PQgetvalue(pRes, i, i_port));
		pSegmentInstance->dbdatadir = PQgetvalue(pRes, i, i_datadir);
	}

	bRtn = true;

cleanup:
	if (pRes != NULL)
		PQclear(pRes);
	destroyPQExpBuffer(pQry);

	return bRtn;
}
