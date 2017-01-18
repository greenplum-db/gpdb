/*-------------------------------------------------------------------------
 *
 * binary_upgradeall.c
 *		Functions to dump Oid dispatch commands from pg_dumpall
 *
 * Portions Copyright 2017 Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/binary_upgradeall.c
 *
 *-------------------------------------------------------------------------
 */


#include <unistd.h>

#include "binary_upgradeall.h"
#include "pqexpbuffer.h"

static char query_buffer[QUERY_ALLOC];

void
dumpFilespaceOid(FILE *OPF, PGconn *conn, char *fsname)
{
	PGresult   *catalog_res;
	int			ntups;
	Oid			fs_oid;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT oid FROM pg_filespace WHERE fsname = '%s'::text;",
			 fsname);

	catalog_res = PQexec(conn, query_buffer);
	if (PQresultStatus(catalog_res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "ERROR: query returned error: \"%s\"",
				PQresultErrorMessage(catalog_res));
		PQfinish(conn);
		exit(1);
	}

	ntups = PQntuples(catalog_res);
	if (ntups != 1)
	{
		fprintf(stderr, "ERROR: query returned %d rows instead of one: %s\n",
				ntups, query_buffer);
		PQfinish(conn);
		exit(1);
	}

	fs_oid = atooid(PQgetvalue(catalog_res, 0, PQfnumber(catalog_res, "oid")));

	fprintf(OPF,
			"SELECT binary_upgrade.preassign_filespace_oid("
			"'%u'::pg_filespace.oid, '%s'::text);\n",
			fs_oid, fsname);

	PQclear(catalog_res);
}

void
dumpTablespaceOid(FILE *OPF, PGconn *conn, char *tsname)
{
	PGresult   *catalog_res;
	int			ntups;
	Oid			ts_oid;

	snprintf(query_buffer, sizeof(query_buffer),
			 "SELECT oid FROM pg_tablespace WHERE spcname = '%s'::text;",
			 tsname);
	catalog_res = PQexec(conn, query_buffer);
	if (PQresultStatus(catalog_res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "ERROR: query returned error: \"%s\"",
				PQresultErrorMessage(catalog_res));
		PQfinish(conn);
		exit(1);
	}

	ntups = PQntuples(catalog_res);
	if (ntups != 1)
	{
		fprintf(stderr, "ERROR: query returned %d rows instead of one: %s\n",
				ntups, query_buffer);
		PQfinish(conn);
		exit(1);
	}

	ts_oid = atooid(PQgetvalue(catalog_res, 0, PQfnumber(catalog_res, "oid")));

	fprintf(OPF,
			"SELECT binary_upgrade.preassign_tablespace_oid("
			"'%u'::pg_tablespace.oid, '%s'::text);\n",
			ts_oid, tsname);

	PQclear(catalog_res);
}

void
dumpResqueueOid(FILE *OPF, PGconn *conn, int server_version, Oid rqoid, char *rsqname)
{
	PQExpBuffer catalog_query;
	PGresult   *catalog_res;
	int			ntups;
	int			i;

	if (server_version >= 80214)
	{
		catalog_query = createPQExpBuffer();
		appendPQExpBuffer(catalog_query, "SELECT oid, restypid "
										 "FROM pg_resqueuecapability "
										 "WHERE resqueueid = '%u'::pg_catalog.oid;",
										 rqoid);

		catalog_res = PQexec(conn, catalog_query->data);
		if (PQresultStatus(catalog_res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "ERROR: query returned error: \"%s\"",
					PQresultErrorMessage(catalog_res));
			PQfinish(conn);
			exit(1);
		}

		ntups = PQntuples(catalog_res);

		for (i = 0; i < ntups; i++)
		{
			Oid rqcapoid = atooid(PQgetvalue(catalog_res, i, 0));
			Oid restypid = atooid(PQgetvalue(catalog_res, i, 1));

			fprintf(OPF,
					"SELECT binary_upgrade.preassign_resqueuecb_oid("
					"'%u'::pg_catalog.oid, '%u'::pg_catalog.oid, '%u'::pg_catalog.oid);\n",
					rqcapoid, rqoid, restypid);
		}

		destroyPQExpBuffer(catalog_query);
		PQclear(catalog_res);
	}

	fprintf(OPF,
			"SELECT binary_upgrade.preassign_resqueue_oid("
			"'%u'::pg_catalog.oid, '%s'::text);\n",
			rqoid, rsqname);
}

void
dumpRoleOid(FILE *OPF, Oid roleoid, const char *rolename)
{
	fprintf(OPF,
			"SELECT binary_upgrade.preassign_authid_oid("
			"'%u'::pg_catalog.oid, '%s'::text);\n",
			roleoid, rolename);
}

void
dumpDatabaseOid(FILE *OPF, Oid dboid, const char *fdbname)
{
	fprintf(OPF,
			"SELECT binary_upgrade.preassign_database_oid("
			"'%u'::pg_catalog.oid, '%s'::text);\n",
			dboid, fdbname);
}
