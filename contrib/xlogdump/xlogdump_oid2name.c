/*
 * xlogdump_oid2name.c
 *
 * a collection of functions to get database object names from the oids
 * by looking up the system catalog.
 */
#include "xlogdump_oid2name.h"

#include "pqexpbuffer.h"
#include "postgres.h"

static PGconn		*conn = NULL; /* Connection for translating oids of global objects */

static PGresult		*_res = NULL; /* a result set variable for relname2attr_*() functions */

static char dbName[NAMEDATALEN];

static char *pghost = NULL;
static char *pgport = NULL;
static char *pguser = NULL;
static char *pgpass = NULL;

/*
 * Structure for the linked-list to hold oid-name lookup cache.
 */
struct oid2name_t {
	Oid oid;
	char *name;
	struct oid2name_t *next;
};

/*
 * Head element of the linked-list.
 */
struct oid2name_t *oid2name_table = NULL;

static char *cache_get(Oid);
static struct oid2name_t *cache_put(Oid, char *);

static bool oid2name_query(char *, size_t, const char *);
static bool oid2name_get_name(uint32, char *, size_t, const char *);

/*
 * cache_get()
 *
 * looks up the oid-name mapping cache table. If not found, returns NULL.
 */
static char *
cache_get(Oid oid)
{
	struct oid2name_t *curr = oid2name_table;

	while (curr!=NULL)
	{
	  //		printf("DEBUG: find %d %s\n", curr->oid, curr->name);

		if (curr->oid == oid)
			return curr->name;

		curr = curr->next;
	}

	return NULL;
}

/*
 * cache_put()
 *
 * puts a new entry to the tail of the oid-name mapping cache table.
 */
static struct oid2name_t *
cache_put(Oid oid, char *name)
{
	struct oid2name_t *curr = oid2name_table;

	if (!curr)
	{
		curr = (struct oid2name_t *)malloc( sizeof(struct oid2name_t) );
		memset(curr, 0, sizeof(struct oid2name_t));

		curr->oid = oid;
		curr->name = strdup(name);

		oid2name_table = curr;

		//		printf("DEBUG: put %d %s\n", curr->oid, curr->name);

		return curr;
	}

	while (curr->next!=NULL)
	{
		curr = curr->next;
	}

	curr->next = (struct oid2name_t *)malloc( sizeof(struct oid2name_t) );
	memset(curr->next, 0, sizeof(struct oid2name_t));
	curr->next->oid = oid;
	curr->next->name = strdup(name);

	//	printf("DEBUG: put %d %s\n", curr->next->oid, curr->next->name);

	return curr->next;
}

bool
oid2name_from_file(const char *file)
{
	FILE *fp;
	Oid oid;
	char name[NAMEDATALEN];

	if ( (fp = fopen(file, "r"))==NULL )
	{
		fprintf(stderr, "ERROR: %s not found.\n",  file);
		return false;
	}

	printf("NOTICE: Using '%s' as an oid2name cache file.\n", file);

	while ( fscanf(fp, "%d %s", &oid, name)>=2 )
	{
	  //		printf("oid=%d, name=%s\n", oid, name);
		cache_put(oid, name);
	}

	fclose(fp);

	return true;
}

bool
oid2name_to_file(const char *file)
{
	const char *oid2name_stmt[] = {
		"SELECT oid,spcname FROM pg_tablespace ORDER BY oid",
		"SELECT oid,datname FROM pg_database ORDER BY oid",
		"SELECT oid,relname FROM pg_class ORDER BY oid"
	};
	PGresult *res = NULL;
	int i, j;

	FILE *fp;

	if ( (fp = fopen(file, "w"))==NULL )
	{
		fprintf(stderr, "ERROR: Can't write %s.\n", file);
		return false;
	}

	for (i=0 ; i<3 ; i++)
	{
		res = PQexec(conn, oid2name_stmt[i]);

		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			return false;
		}

		for (j=0 ; j<PQntuples(res) ; j++)
			fprintf(fp, "%s %s\n", PQgetvalue(res, j, 0), PQgetvalue(res, j, 1));

		PQclear(res);
	}

	fclose(fp);
	return true;
}

/*
 * Open a database connection
 */
bool
DBConnect(const char *host, const char *port, char *database, const char *user)
{
	pghost = strdup(host);
	pgport = strdup(port);
	pguser = strdup(user);
	pgpass = NULL;

 retry_login:
	conn = PQsetdbLogin(pghost,
			    pgport,
			    NULL,
			    NULL,
			    database,
			    pguser,
			    pgpass);

	if (PQstatus(conn) == CONNECTION_BAD)
	{
		/* wait a password if required to login, then retry. */
		if (strcmp(PQerrorMessage(conn), PQnoPasswordSupplied) == 0 &&
		    !feof(stdin))
		{
			PQfinish(conn);
			conn = NULL;
			pgpass = simple_prompt("Password: ", 100, false);
			goto retry_login;
		}

		fprintf(stderr, "Connection to database failed: %s",
			PQerrorMessage(conn));

		PQfinish(conn);
		conn = NULL;

		return false;
	}

	return true;
}

static bool
oid2name_query(char *buf, size_t buflen, const char *query)
{
	PGresult *res = NULL;

	if (!conn)
		return false;

	res = PQexec(conn, query);
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
		PQclear(res);
		return false;
	}
	if (PQntuples(res) > 0)
	{
		strncpy(buf, PQgetvalue(res, 0, 0), buflen);
		PQclear(res);
		return true;
	}

	PQclear(res);
	return false;
}

static bool
oid2name_get_name(uint32 oid, char *buf, size_t buflen, const char *query)
{
	if (cache_get(oid))
	{
		snprintf(buf, buflen, "%s", cache_get(oid));
		return true;
	}

	if ( oid2name_query(buf, buflen, query) )
	{
		cache_put(oid, buf);
		return true;
	}

	snprintf(buf, buflen, "%u", oid);
	return false;
}

/*
 * Atempt to read the name of tablespace into lastSpcName
 * (if there's a database connection and the oid changed since lastSpcOid)
 */
char *
getSpaceName(uint32 spcid, char *buf, size_t buflen)
{
	char dbQry[1024];

	snprintf(dbQry, sizeof(dbQry), "SELECT spcname FROM pg_tablespace WHERE oid = %i", spcid);

	oid2name_get_name(spcid, buf, buflen, dbQry);

	return buf;
}


/*
 * Atempt to get the name of database (if there's a database connection)
 */
char *
getDbName(uint32 dbid, char *buf, size_t buflen)
{
	char dbQry[1024];

	snprintf(dbQry, sizeof(dbQry), "SELECT datname FROM pg_database WHERE oid = %i", dbid);

	if ( oid2name_get_name(dbid, buf, buflen, dbQry) )
	{
		/*
		 * Need to keep name of the database going to be connected
		 * in order to retreive object names (tables, indexes, ...)
		 * at the next step, particularly in getRelName().
		 */
		strncpy(dbName, buf, sizeof(dbName));
	}

	return buf;
}

/*
 * Atempt to get the name of relation and copy to relName 
 * (if there's a database connection and the reloid changed)
 * Copy a string with oid if not found
 */
char *
getRelName(uint32 relid, char *buf, size_t buflen)
{
	char dbQry[1024];

	/* Try the relfilenode and oid just in case the filenode has changed
	   If it has changed more than once we can't translate it's name */
	snprintf(dbQry, sizeof(dbQry), "SELECT relname, oid FROM pg_class WHERE relfilenode = %i OR oid = %i", relid, relid);

	if (cache_get(relid))
	{
		snprintf(buf, buflen, "%s", cache_get(relid));
		return buf;
	}

	/*
	 * If the xlog record has some information about rmgr operation on
	 * a different database, it needs to establish a new connection
	 * to the different database in order to retreive a object name
	 * from the system catalog.
	 */
	if (conn && strcmp(PQdb(conn), dbName) != 0)
	{
		/*
		 * Re-connect to the different database.
		 */
		if (conn)
			PQfinish(conn);

		conn = PQsetdbLogin(pghost, pgport, NULL, NULL,
				    dbName, pguser, pgpass);
	}

	oid2name_get_name(relid, buf, buflen, dbQry);

	return buf;
}

int
relname2attr_begin(const char *relname)
{
	char dbQry[1024];

	if ( _res!=NULL )
		PQclear(_res);

	snprintf(dbQry, sizeof(dbQry), "SELECT attname, atttypid, attlen, attbyval, attalign FROM pg_attribute a, pg_class c WHERE attnum > 0 AND attrelid = c.oid AND c.relname='%s' ORDER BY attnum", relname);
	_res = PQexec(conn, dbQry);
	if (PQresultStatus(_res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
		PQclear(_res);
		_res = NULL;
		return -1;
	}

	return PQntuples(_res);
}

int
relname2attr_fetch(int i, attrib_t *att)
{
	snprintf(att->attname, NAMEDATALEN, "%s", PQgetvalue(_res, i, 0));
	att->atttypid = atoi( PQgetvalue(_res, i, 1) );
	att->attlen   = atoi( PQgetvalue(_res, i, 2) );
	att->attbyval = *(PQgetvalue(_res, i, 3));
	att->attalign = *(PQgetvalue(_res, i, 4));

	return i;
}

void
relname2attr_end()
{
	PQclear(_res);
	_res = NULL;
}

bool
oid2name_enabled(void)
{
	return (conn!=NULL);
}

void
DBDisconnect(void)
{
	if (conn)
		PQfinish(conn);
}
