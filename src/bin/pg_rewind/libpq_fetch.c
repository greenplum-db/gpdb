/*-------------------------------------------------------------------------
 *
 * libpq_fetch.c
 *	  Functions for fetching files from a remote server.
 *
 * Copyright (c) 2013-2015, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

/* for ntohl/htonl */
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pg_rewind.h"
#include "datapagemap.h"
#include "fetch.h"
#include "file_ops.h"
#include "filemap.h"
#include "logging.h"

#include "libpq-fe.h"
#include "catalog/catalog.h"
#include "catalog/pg_type.h"

#include "pqexpbuffer.h"

static PGconn *conn = NULL;

/* Contents of recovery.conf to be generated */
static PQExpBuffer recoveryconfcontents = NULL;

/*
 * Files are fetched max CHUNKSIZE bytes at a time.
 *
 * (This only applies to files that are copied in whole, or for truncated
 * files where we copy the tail. Relation files, where we know the individual
 * blocks that need to be fetched, are fetched in BLCKSZ chunks.)
 */
#define CHUNKSIZE 1000000

static void receiveFileChunks(const char *sql);
static void execute_pagemap(datapagemap_t *pagemap, const char *path);
static char *run_simple_query(const char *sql);

void
libpqConnect(const char *connstr)
{
	char	   *str;
	PGresult   *res;

	conn = PQconnectdb(connstr);
	if (PQstatus(conn) == CONNECTION_BAD)
		pg_fatal("could not connect to server: %s",
				 PQerrorMessage(conn));

	pg_log(PG_PROGRESS, "connected to server\n");

	/* Secure connection by enforcing search_path */
	res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', '', false)");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		pg_fatal("could not clear search_path: %s",
				 PQresultErrorMessage(res));
	PQclear(res);

	/*
	 * Check that the server is not in hot standby mode. There is no
	 * fundamental reason that couldn't be made to work, but it doesn't
	 * currently because we use a temporary table. Better to check for it
	 * explicitly than error out, for a better error message.
	 */
	str = run_simple_query("SELECT pg_is_in_recovery()");
	if (strcmp(str, "f") != 0)
		pg_fatal("source server must not be in recovery mode\n");
	pg_free(str);

	/*
	 * Also check that full_page_writes is enabled.  We can get torn pages if
	 * a page is modified while we read it with pg_read_binary_file(), and we
	 * rely on full page images to fix them.
	 */
	str = run_simple_query("SHOW full_page_writes");
	if (strcmp(str, "on") != 0)
		pg_fatal("full_page_writes must be enabled in the source server\n");
	pg_free(str);

	/*
	 * Although we don't do any "real" updates, we do work with a temporary
	 * table. We don't care about synchronous commit for that. It doesn't
	 * otherwise matter much, but if the server is using synchronous
	 * replication, and replication isn't working for some reason, we don't
	 * want to get stuck, waiting for it to start working again.
	 */
	res = PQexec(conn, "SET synchronous_commit = off");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		pg_fatal("could not set up connection context: %s",
				 PQresultErrorMessage(res));
	PQclear(res);
}

/*
 * Runs a query that returns a single value.
 * The result should be pg_free'd after use.
 */
static char *
run_simple_query(const char *sql)
{
	PGresult   *res;
	char	   *result;

	res = PQexec(conn, sql);

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		pg_fatal("error running query (%s) in source server: %s",
				 sql, PQresultErrorMessage(res));

	/* sanity check the result set */
	if (PQnfields(res) != 1 || PQntuples(res) != 1 || PQgetisnull(res, 0, 0))
		pg_fatal("unexpected result set from query\n");

	result = pg_strdup(PQgetvalue(res, 0, 0));

	PQclear(res);

	return result;
}

/*
 * Calls pg_current_xlog_insert_location() function
 */
XLogRecPtr
libpqGetCurrentXlogInsertLocation(void)
{
	XLogRecPtr	result;
	uint32		hi;
	uint32		lo;
	char	   *val;

	val = run_simple_query("SELECT pg_current_xlog_insert_location()");

	if (sscanf(val, "%X/%X", &hi, &lo) != 2)
		pg_fatal("unrecognized result \"%s\" for current WAL insert location\n", val);

	result = ((uint64) hi) << 32 | lo;

	pg_free(val);

	return result;
}

/*
 * Get a list of all files in the data directory.
 */
void
libpqProcessFileList(void)
{
	PGresult   *res;
	const char *sql;
	int			i;

	/*
	 * Create a recursive directory listing of the whole data directory.
	 *
	 * The WITH RECURSIVE part does most of the work. The second part gets the
	 * targets of the symlinks in pg_tblspc directory.
	 *
	 * XXX: There is no backend function to get a symbolic link's target in
	 * general, so if the admin has put any custom symbolic links in the data
	 * directory, they won't be copied correctly.
	 */
	sql =
		"SET gp_recursive_cte TO ON;\n"
		"WITH RECURSIVE files (path, filename, size, isdir) AS (\n"
		"  SELECT '' AS path, filename, size, isdir FROM\n"
		"  (SELECT pg_ls_dir('.', true, false) AS filename) AS fn,\n"
		"        pg_stat_file(fn.filename, true) AS this\n"
		"  UNION ALL\n"
		"  SELECT parent.path || parent.filename || '/' AS path,\n"
		"         fn, this.size, this.isdir\n"
		"  FROM files AS parent,\n"
		"       pg_ls_dir(parent.path || parent.filename, true, false) AS fn,\n"
		"       pg_stat_file(parent.path || parent.filename || '/' || fn, true) AS this\n"
		"       WHERE parent.isdir = 't'\n"
		")\n"
		"SELECT path || filename, size, isdir,\n"
		"       pg_tablespace_location(pg_tablespace.oid) AS link_target\n"
		"FROM files\n"
		"LEFT OUTER JOIN pg_tablespace ON files.path = 'pg_tblspc/'\n"
		"                             AND oid::text = files.filename\n";
	res = PQexec(conn, sql);

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		pg_fatal("could not fetch file list: %s",
				 PQresultErrorMessage(res));

	/* sanity check the result set */
	if (PQnfields(res) != 4)
		pg_fatal("unexpected result set while fetching file list\n");

	/* Read result to local variables */
	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *path = PQgetvalue(res, i, 0);
		int64		filesize = atol(PQgetvalue(res, i, 1));
		bool		isdir = (strcmp(PQgetvalue(res, i, 2), "t") == 0);
		char	   *link_target = PQgetvalue(res, i, 3);
		file_type_t type;

		if (PQgetisnull(res, 0, 1))
		{
			/*
			 * The file was removed from the server while the query was
			 * running. Ignore it.
			 */
			continue;
		}

		if (link_target[0])
			type = FILE_TYPE_SYMLINK;
		else if (isdir)
			type = FILE_TYPE_DIRECTORY;
		else
			type = FILE_TYPE_REGULAR;

		process_source_file(path, type, filesize, link_target);
	}
	PQclear(res);
}

/*
 * Converts an int64 from network byte order to native format.
 */
static int64
pg_recvint64(int64 value)
{
	union
	{
		int64	i64;
		uint32	i32[2];
	} swap;
	int64	result;

	swap.i64 = value;

	result = (uint32) ntohl(swap.i32[0]);
	result <<= 32;
	result |= (uint32) ntohl(swap.i32[1]);

	return result;
}

/*----
 * Runs a query, which returns pieces of files from the remote source data
 * directory, and overwrites the corresponding parts of target files with
 * the received parts. The result set is expected to be of format:
 *
 * path		text	-- path in the data directory, e.g "base/1/123"
 * begin	int8	-- offset within the file
 * chunk	bytea	-- file content
 *----
 */
static void
receiveFileChunks(const char *sql)
{
	PGresult   *res;

	if (PQsendQueryParams(conn, sql, 0, NULL, NULL, NULL, NULL, 1) != 1)
		pg_fatal("could not send query: %s", PQerrorMessage(conn));

	pg_log(PG_DEBUG, "getting file chunks\n");

	if (PQsetSingleRowMode(conn) != 1)
		pg_fatal("could not set libpq connection to single row mode\n");

	while ((res = PQgetResult(conn)) != NULL)
	{
		char	   *filename;
		int			filenamelen;
		int64		chunkoff;
		char		chunkoff_str[32];
		int			chunksize;
		char	   *chunk;

		switch (PQresultStatus(res))
		{
			case PGRES_SINGLE_TUPLE:
				break;

			case PGRES_TUPLES_OK:
				PQclear(res);
				continue;		/* final zero-row result */

			default:
				pg_fatal("unexpected result while fetching remote files: %s",
						 PQresultErrorMessage(res));
		}

		/* sanity check the result set */
		if (PQnfields(res) != 3 || PQntuples(res) != 1)
			pg_fatal("unexpected result set size while fetching remote files\n");

		if (PQftype(res, 0) != TEXTOID ||
			PQftype(res, 1) != INT8OID ||
			PQftype(res, 2) != BYTEAOID)
		{
			pg_fatal("unexpected data types in result set while fetching remote files: %u %u %u\n",
					 PQftype(res, 0), PQftype(res, 1), PQftype(res, 2));
		}

		if (PQfformat(res, 0) != 1 &&
			PQfformat(res, 1) != 1 &&
			PQfformat(res, 2) != 1)
		{
			pg_fatal("unexpected result format while fetching remote files\n");
		}

		if (PQgetisnull(res, 0, 0) ||
			PQgetisnull(res, 0, 1))
		{
			pg_fatal("unexpected null values in result while fetching remote files\n");
		}

		if (PQgetlength(res, 0, 1) != sizeof(int64))
			pg_fatal("unexpected result length while fetching remote files\n");

		/* Read result set to local variables */
		memcpy(&chunkoff, PQgetvalue(res, 0, 1), sizeof(int64));
		chunkoff = pg_recvint64(chunkoff);
		chunksize = PQgetlength(res, 0, 2);

		filenamelen = PQgetlength(res, 0, 0);
		filename = pg_malloc(filenamelen + 1);
		memcpy(filename, PQgetvalue(res, 0, 0), filenamelen);
		filename[filenamelen] = '\0';

		chunk = PQgetvalue(res, 0, 2);

		/*
		 * If a file has been deleted on the source, remove it on the target
		 * as well.  Note that multiple unlink() calls may happen on the same
		 * file if multiple data chunks are associated with it, hence ignore
		 * unconditionally anything missing.  If this file is not a relation
		 * data file, then it has been already truncated when creating the
		 * file chunk list at the previous execution of the filemap.
		 */
		if (PQgetisnull(res, 0, 2))
		{
			pg_log(PG_DEBUG,
			  "received null value for chunk for file \"%s\", file has been deleted\n",
				   filename);
			remove_target_file(filename, true);
			pg_free(filename);
			PQclear(res);
			continue;
		}

		/*
		 * Separate step to keep platform-dependent format code out of
		 * translatable strings.
		 */
		snprintf(chunkoff_str, sizeof(chunkoff_str), INT64_FORMAT, chunkoff);
		pg_log(PG_DEBUG, "received chunk for file \"%s\", offset %s, size %d\n",
			   filename, chunkoff_str, chunksize);

		open_target_file(filename, false);

		write_target_range(chunk, chunkoff, chunksize);

		pg_free(filename);

		PQclear(res);
	}
}

/*
 * Receive a single file as a malloc'd buffer.
 */
char *
libpqGetFile(const char *filename, size_t *filesize)
{
	PGresult   *res;
	char	   *result;
	int			len;
	const char *paramValues[1];

	paramValues[0] = filename;
	res = PQexecParams(conn, "SELECT pg_read_binary_file($1)",
					   1, NULL, paramValues, NULL, NULL, 1);

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
		pg_fatal("could not fetch remote file \"%s\": %s",
				 filename, PQresultErrorMessage(res));

	/* sanity check the result set */
	if (PQntuples(res) != 1 || PQgetisnull(res, 0, 0))
		pg_fatal("unexpected result set while fetching remote file \"%s\"\n",
				 filename);

	/* Read result to local variables */
	len = PQgetlength(res, 0, 0);
	result = pg_malloc(len + 1);
	memcpy(result, PQgetvalue(res, 0, 0), len);
	result[len] = '\0';

	PQclear(res);

	pg_log(PG_DEBUG, "fetched file \"%s\", length %d\n", filename, len);

	if (filesize)
		*filesize = len;
	return result;
}

/*
 * Write a file range to a temporary table in the server.
 *
 * The range is sent to the server as a COPY formatted line, to be inserted
 * into the 'fetchchunks' temporary table. It is used in receiveFileChunks()
 * function to actually fetch the data.
 */
static void
fetch_file_range(const char *path, uint64 begin, uint64 end)
{
	char		linebuf[MAXPGPATH + 23];

	/* Split the range into CHUNKSIZE chunks */
	while (end - begin > 0)
	{
		unsigned int len;

		/* Fine as long as CHUNKSIZE is not bigger than UINT32_MAX */
		if (end - begin > CHUNKSIZE)
			len = CHUNKSIZE;
		else
			len = (unsigned int) (end - begin);

		snprintf(linebuf, sizeof(linebuf), "%s\t" UINT64_FORMAT "\t%u\n", path, begin, len);

		if (PQputCopyData(conn, linebuf, strlen(linebuf)) != 1)
			pg_fatal("could not send COPY data: %s",
					 PQerrorMessage(conn));

		begin += len;
	}
}

/*
 * Fetch all changed blocks from remote source data directory.
 */
void
libpq_executeFileMap(filemap_t *map)
{
	file_entry_t *entry;
	const char *sql;
	PGresult   *res;
	int			i;

	/*
	 * First create a temporary table, and load it with the blocks that we
	 * need to fetch.
	 */
	sql = "CREATE TEMPORARY TABLE fetchchunks(path text, begin int8, len int4);";
	res = PQexec(conn, sql);

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		pg_fatal("could not create temporary table: %s",
				 PQresultErrorMessage(res));
	PQclear(res);

	sql = "COPY fetchchunks FROM STDIN";
	res = PQexec(conn, sql);

	if (PQresultStatus(res) != PGRES_COPY_IN)
		pg_fatal("could not send file list: %s",
				 PQresultErrorMessage(res));
	PQclear(res);

	for (i = 0; i < map->narray; i++)
	{
		entry = map->array[i];

		/* If this is a relation file, copy the modified blocks */
		execute_pagemap(&entry->pagemap, entry->path);

		switch (entry->action)
		{
			case FILE_ACTION_NONE:
				/* nothing else to do */
				break;

			case FILE_ACTION_COPY:
				/* Truncate the old file out of the way, if any */
				open_target_file(entry->path, true);
				fetch_file_range(entry->path, 0, entry->newsize);
				break;

			case FILE_ACTION_TRUNCATE:
				truncate_target_file(entry->path, entry->newsize);
				break;

			case FILE_ACTION_COPY_TAIL:
				fetch_file_range(entry->path, entry->oldsize, entry->newsize);
				break;

			case FILE_ACTION_REMOVE:
				remove_target(entry);
				break;

			case FILE_ACTION_CREATE:
				create_target(entry);
				break;
		}
	}

	if (PQputCopyEnd(conn, NULL) != 1)
		pg_fatal("could not send end-of-COPY: %s",
				 PQerrorMessage(conn));

	while ((res = PQgetResult(conn)) != NULL)
	{
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
			pg_fatal("unexpected result while sending file list: %s",
					 PQresultErrorMessage(res));
		PQclear(res);
	}

	/*
	 * We've now copied the list of file ranges that we need to fetch to the
	 * temporary table. Now, actually fetch all of those ranges.
	 */
	sql =
		"SELECT path, begin, \n"
		"  pg_read_binary_file(path, begin, len, true) AS chunk\n"
		"FROM fetchchunks\n";

	receiveFileChunks(sql);
}

static void
execute_pagemap(datapagemap_t *pagemap, const char *path)
{
	datapagemap_iterator_t *iter;
	BlockNumber blkno;
	off_t		offset;

	iter = datapagemap_iterate(pagemap);
	while (datapagemap_next(iter, &blkno))
	{
		offset = blkno * BLCKSZ;

		fetch_file_range(path, offset, offset + BLCKSZ);
	}
	pg_free(iter);
}

/*
 * TODO: Most of the below code is copied directly from pg_basebackup.c. There
 * are only a couple subtle differences if you diff the two (e.g. removal of
 * recovery.done file, excluding "options" to avoid utility mode, etc.). We
 * should create a common library between the two to create the recovery.conf
 * file.
 */
static void
disconnect_and_exit(int code)
{
	if (conn != NULL)
		PQfinish(conn);

	exit(code);
}

/*
 * Escape a parameter value so that it can be used as part of a libpq
 * connection string, e.g. in:
 *
 * application_name=<value>
 *
 * The returned string is malloc'd. Return NULL on out-of-memory.
 */
static char *
escapeConnectionParameter(const char *src)
{
	bool		need_quotes = false;
	bool		need_escaping = false;
	const char *p;
	char	   *dstbuf;
	char	   *dst;

	/*
	 * First check if quoting is needed. Any quote (') or backslash (\)
	 * characters need to be escaped. Parameters are separated by whitespace,
	 * so any string containing whitespace characters need to be quoted. An
	 * empty string is represented by ''.
	 */
	if (strchr(src, '\'') != NULL || strchr(src, '\\') != NULL)
		need_escaping = true;

	for (p = src; *p; p++)
	{
		if (isspace((unsigned char) *p))
		{
			need_quotes = true;
			break;
		}
	}

	if (*src == '\0')
		return pg_strdup("''");

	if (!need_quotes && !need_escaping)
		return pg_strdup(src);	/* no quoting or escaping needed */

	/*
	 * Allocate a buffer large enough for the worst case that all the source
	 * characters need to be escaped, plus quotes.
	 */
	dstbuf = pg_malloc(strlen(src) * 2 + 2 + 1);

	dst = dstbuf;
	if (need_quotes)
		*(dst++) = '\'';
	for (; *src; src++)
	{
		if (*src == '\'' || *src == '\\')
			*(dst++) = '\\';
		*(dst++) = *src;
	}
	if (need_quotes)
		*(dst++) = '\'';
	*dst = '\0';

	return dstbuf;
}

/*
 * Escape a string so that it can be used as a value in a key-value pair
 * a configuration file.
 */
static char *
escape_quotes(const char *src)
{
	char	   *result = escape_single_quotes_ascii(src);

	if (!result)
	{
		fprintf(stderr, _("%s: out of memory\n"), progname);
		exit(1);
	}
	return result;
}

#define GP_WALRECEIVER_APPNAME "gp_walreceiver"

/*
 * Create a recovery.conf file in memory using a PQExpBuffer
 */
void
GenerateRecoveryConf(char *replication_slot)
{
	PQconninfoOption *connOptions;
	PQconninfoOption *option;
	PQExpBufferData conninfo_buf;
	char	   *escaped;

	recoveryconfcontents = createPQExpBuffer();
	if (!recoveryconfcontents)
	{
		fprintf(stderr, _("%s: out of memory\n"), progname);
		disconnect_and_exit(1);
	}

	connOptions = PQconninfo(conn);
	if (connOptions == NULL)
	{
		fprintf(stderr, _("%s: out of memory\n"), progname);
		disconnect_and_exit(1);
	}

	appendPQExpBufferStr(recoveryconfcontents, "standby_mode = 'on'\n");
	appendPQExpBufferStr(recoveryconfcontents, "recovery_target_timeline = 'latest'\n");

	initPQExpBuffer(&conninfo_buf);
	for (option = connOptions; option && option->keyword; option++)
	{
		/*
		 * Do not emit this setting if: - the setting is "replication",
		 * "dbname" or "fallback_application_name", since these would be
		 * overridden by the libpqwalreceiver module anyway. - not set or
		 * empty.
		 */
		if (strcmp(option->keyword, "replication") == 0 ||
			strcmp(option->keyword, "dbname") == 0 ||
			strcmp(option->keyword, "fallback_application_name") == 0 ||
			strcmp(option->keyword, "options") == 0 ||
			(option->val == NULL) ||
			(option->val[0] == '\0'))
			continue;

		/* Separate key-value pairs with spaces */
		if (conninfo_buf.len != 0)
			appendPQExpBufferStr(&conninfo_buf, " ");

		/*
		 * Write "keyword=value" pieces, the value string is escaped and/or
		 * quoted if necessary.
		 */
		escaped = escapeConnectionParameter(option->val);
		appendPQExpBuffer(&conninfo_buf, "%s=%s", option->keyword, escaped);
		free(escaped);
	}

	appendPQExpBuffer(&conninfo_buf, " application_name=%s", GP_WALRECEIVER_APPNAME);
	/*
	 * Escape the connection string, so that it can be put in the config file.
	 * Note that this is different from the escaping of individual connection
	 * options above!
	 */
	escaped = escape_quotes(conninfo_buf.data);
	appendPQExpBuffer(recoveryconfcontents, "primary_conninfo = '%s'\n", escaped);
	free(escaped);

	if (replication_slot)
	{
		escaped = escape_quotes(replication_slot);
		appendPQExpBuffer(recoveryconfcontents, "primary_slot_name = '%s'\n", replication_slot);
		free(escaped);
	}

	if (PQExpBufferBroken(recoveryconfcontents) ||
		PQExpBufferDataBroken(conninfo_buf))
	{
		fprintf(stderr, _("%s: out of memory\n"), progname);
		disconnect_and_exit(1);
	}

	termPQExpBuffer(&conninfo_buf);

	PQconninfoFree(connOptions);
}


/*
 * Write a recovery.conf file into the directory specified in basedir,
 * with the contents already collected in memory.
 */
void
WriteRecoveryConf(void)
{
	char		filename[MAXPGPATH];
	FILE	   *cf;

	/* Remove recovery.done file that was most likely copied from source instance */
	snprintf(filename, sizeof(filename), "%s/recovery.done", datadir_target);
	unlink(filename);

	snprintf(filename, sizeof(filename), "%s/recovery.conf", datadir_target);

	cf = fopen(filename, "w");
	if (cf == NULL)
	{
		fprintf(stderr, _("%s: could not create file \"%s\": %s\n"), progname, filename, strerror(errno));
		disconnect_and_exit(1);
	}

	if (fwrite(recoveryconfcontents->data, recoveryconfcontents->len, 1, cf) != 1)
	{
		fprintf(stderr,
				_("%s: could not write to file \"%s\": %s\n"),
				progname, filename, strerror(errno));
		disconnect_and_exit(1);
	}

	fclose(cf);
}
