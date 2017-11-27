/*
 * dbsize.c
 *		object size functions
 *
 * Copyright (c) 2002-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/adt/dbsize.c,v 1.23 2009/01/01 17:23:49 momjian Exp $
 *
 */

#include "postgres.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <glob.h>

#include "lib/stringinfo.h"

#include "access/heapam.h"
#include "access/appendonlywriter.h"
#include "access/aocssegfiles.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_appendonly_fn.h"
#include "catalog/pg_tablespace.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/int8.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/syscache.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbpersistenttablespace.h"


static int64
get_size_from_segDBs(const char * cmd)
{
	int			spiresult;
	bool		succeeded = false;
	int64		result = 0;
	volatile bool connected = false;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	PG_TRY();
	{
		HeapTuple	tup;
		TupleDesc	tupdesc;
		bool		isnull;
		Datum		size;

		do
		{
			/* Establish an SPI session as a client of myself. */
			if (SPI_connect() != SPI_OK_CONNECT)
				break;

			connected = true;

			/* Do the query. */
			spiresult = SPI_execute(cmd, false, 0);

			/* Did the query succeed? */
			if (spiresult != SPI_OK_SELECT)
				break;

			if (SPI_processed < 1)
				break;

			tup = SPI_tuptable->vals[0];
			tupdesc = SPI_tuptable->tupdesc;

			size = heap_getattr(SPI_tuptable->vals[0], 1, SPI_tuptable->tupdesc, &isnull);
			if (isnull)
				break;

			result = DatumGetInt64(size);

			succeeded = true;
		}
		while (0);

		/* End recursive session. */
		connected = false;
		SPI_finish();

		if (!succeeded)
			elog(ERROR, "Unable to get sizes from segments");
	}
	/* Clean up in case of error. */
	PG_CATCH();
	{
		/* End recursive session. */
		if (connected)
			SPI_finish();

		/* Carry on with error handling. */
		PG_RE_THROW();
	}
	PG_END_TRY();

	return result;
}

/* Return physical size of directory contents, or 0 if dir doesn't exist */
int64
db_dir_size(const char *path)
{
	int64		dirsize = 0;
	struct dirent *direntry;
	DIR		   *dirdesc;
	char		filename[MAXPGPATH];

	dirdesc = AllocateDir(path);

	if (!dirdesc)
		return 0;

	while ((direntry = ReadDir(dirdesc, path)) != NULL)
	{
		struct stat fst;

		CHECK_FOR_INTERRUPTS();

		if (strcmp(direntry->d_name, ".") == 0 ||
			strcmp(direntry->d_name, "..") == 0)
			continue;

		snprintf(filename, MAXPGPATH, "%s/%s", path, direntry->d_name);

		if (stat(filename, &fst) < 0)
		{
			if (errno == ENOENT)
				continue;
			else
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m", filename)));
		}
		dirsize += fst.st_size;
	}

	FreeDir(dirdesc);
	return dirsize;
}

/*
 * calculate size of database in all tablespaces
 */
static int64
calculate_database_size(Oid dbOid)
{
	int64		totalsize;
	char		pathname[MAXPGPATH];
	Relation    rel;
	HeapScanDesc scandesc;
	HeapTuple   tuple;
	AclResult	aclresult;

	/* User must have connect privilege for target database */
	aclresult = pg_database_aclcheck(dbOid, GetUserId(), ACL_CONNECT);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_DATABASE,
					   get_database_name(dbOid));

	/* Scan through all tablespaces */
	rel = heap_open(TableSpaceRelationId, AccessShareLock);
	scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);
	tuple = heap_getnext(scandesc, ForwardScanDirection);
	totalsize = 0;
	while (HeapTupleIsValid(tuple))
	{
		char *priFilespace, *mirFilespace;
		Oid   tsOid;
		
		tsOid = HeapTupleGetOid(tuple);
		
		/* Don't include shared relations */
		if (tsOid != GLOBALTABLESPACE_OID)
		{			
			/* Find the filespace path for this tablespace */
			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
				tsOid, &priFilespace, &mirFilespace);

			/* Build the path for this database in this tablespace */
			FormDatabasePath(pathname, priFilespace, tsOid, dbOid);
			
			totalsize += db_dir_size(pathname);
		}

		tuple = heap_getnext(scandesc, ForwardScanDirection);
	}
	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);

	/* Complain if we found no trace of the DB at all */
	if (totalsize == 0)
		ereport(ERROR,
				(ERRCODE_UNDEFINED_DATABASE,
				 errmsg("database with OID %u does not exist", dbOid)));

	return totalsize;
}

Datum
pg_database_size_oid(PG_FUNCTION_ARGS)
{
	int64		size = 0;
	Oid			dbOid = PG_GETARG_OID(0);
	size = calculate_database_size(dbOid);
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		
		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_database_size(%u))::int8 from gp_dist_random('gp_id');", dbOid);

		size += get_size_from_segDBs(buffer.data);
	}

	PG_RETURN_INT64(size);
}

Datum
pg_database_size_name(PG_FUNCTION_ARGS)
{
	int64		size = 0;
	Name		dbName = PG_GETARG_NAME(0);
	Oid			dbOid = get_database_oid(NameStr(*dbName), false);
						
	size = calculate_database_size(dbOid);
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		
		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_database_size('%s'))::int8 from gp_dist_random('gp_id');", NameStr(*dbName));

		size += get_size_from_segDBs(buffer.data);
	}

	PG_RETURN_INT64(size);
}


/*
 * calculate total size of tablespace
 */
static int64
calculate_tablespace_size(Oid tblspcOid)
{
	char		tblspcPath[MAXPGPATH];
	char		pathname[MAXPGPATH];
	int64		totalsize = 0;
	DIR		   *dirdesc;
	struct dirent *direntry;
	AclResult	aclresult;

	/*
	 * User must have CREATE privilege for target tablespace, either
	 * explicitly granted or implicitly because it is default for current
	 * database.
	 */
	if (tblspcOid != MyDatabaseTableSpace)
	{
		aclresult = pg_tablespace_aclcheck(tblspcOid, GetUserId(), ACL_CREATE);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE,
						   get_tablespace_name(tblspcOid));
	}

	if (tblspcOid == DEFAULTTABLESPACE_OID)
		snprintf(tblspcPath, MAXPGPATH, "base");
	else if (tblspcOid == GLOBALTABLESPACE_OID)
		snprintf(tblspcPath, MAXPGPATH, "global");
	else
		snprintf(tblspcPath, MAXPGPATH, "pg_tblspc/%u", tblspcOid);

	dirdesc = AllocateDir(tblspcPath);

	if (!dirdesc)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open tablespace directory \"%s\": %m",
						tblspcPath)));

	while ((direntry = ReadDir(dirdesc, tblspcPath)) != NULL)
	{
		struct stat fst;

		CHECK_FOR_INTERRUPTS();

		if (strcmp(direntry->d_name, ".") == 0 ||
			strcmp(direntry->d_name, "..") == 0)
			continue;

		snprintf(pathname, MAXPGPATH, "%s/%s", tblspcPath, direntry->d_name);

		if (stat(pathname, &fst) < 0)
		{
			if (errno == ENOENT)
				continue;
			else
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m", pathname)));
		}

		if (S_ISDIR(fst.st_mode))
			totalsize += db_dir_size(pathname);

		totalsize += fst.st_size;
	}

	FreeDir(dirdesc);

	return totalsize;
}

Datum
pg_tablespace_size_oid(PG_FUNCTION_ARGS)
{
	int64		size = 0;
	Oid			tblspcOid = PG_GETARG_OID(0);

	size = calculate_tablespace_size(tblspcOid);
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		
		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_tablespace_size(%u))::int8 from gp_dist_random('gp_id');", tblspcOid);

		size += get_size_from_segDBs(buffer.data);
	}

	PG_RETURN_INT64(size);
}

Datum
pg_tablespace_size_name(PG_FUNCTION_ARGS)
{
	int64		size = 0;
	Name		tblspcName = PG_GETARG_NAME(0);
	Oid			tblspcOid = get_tablespace_oid(NameStr(*tblspcName), false);

	size = calculate_tablespace_size(tblspcOid);
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		
		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_tablespace_size('%s'))::int8 from gp_dist_random('gp_id');", NameStr(*tblspcName));

		size += get_size_from_segDBs(buffer.data);
	}

	PG_RETURN_INT64(size);
}

/*
 * calculate size of a relation
 *
 * Iterator over all files belong to the relation and do stat.
 * The obviously better way is to use glob.  For whatever reason,
 * glob is extremely slow if there are lots of relations in the
 * database.  So we handle all cases, instead. 
 */
static int64
calculate_relation_size(Relation rel, ForkNumber forknum)
{
	int64		totalsize = 0;
	char	   *relationpath;
	char		pathname[MAXPGPATH];
	unsigned int segcount = 0;

	relationpath = relpath(rel->rd_node, forknum);

if (RelationIsHeap(rel))
{
	/* Ordinary relation, including heap and index.
	 * They take form of relationpath, or relationpath.%d
	 * There will be no holes, therefore, we can stop we
	 * we reach the first non-exist file.
	 */
	for (segcount = 0;; segcount++)
	{
		struct stat fst;

		CHECK_FOR_INTERRUPTS();

		if (segcount == 0)
			snprintf(pathname, MAXPGPATH, "%s",
					 relationpath);
		else
			snprintf(pathname, MAXPGPATH, "%s.%u",
					 relationpath, segcount);

		if (stat(pathname, &fst) < 0)
		{
			if (errno == ENOENT)
				break;
			else
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file %s: %m", pathname)));
		}
		totalsize += fst.st_size;
	}
}
else if (RelationIsAoRows(rel))
	totalsize = GetAOTotalBytes(rel, SnapshotNow);
else if (RelationIsAoCols(rel))
	totalsize = GetAOCSTotalBytes(rel, SnapshotNow, true);

    /* RELSTORAGE_VIRTUAL has no space usage */
    return totalsize;
}

Datum
pg_relation_size(PG_FUNCTION_ARGS)
{
	Oid			relOid = PG_GETARG_OID(0);
	text	   *forkName = PG_GETARG_TEXT_P(1);
	Relation	rel;
	int64		size = 0;

	/**
	 * This function is peculiar in that it does its own dispatching.
	 * It does not work on entry db since we do not support dispatching
	 * from entry-db currently.
	 */
	if (Gp_role == GP_ROLE_EXECUTE && Gp_segment == -1)
		elog(ERROR, "This query is not currently supported by GPDB.");

	rel = try_relation_open(relOid, AccessShareLock, false);

	/*
	 * While we scan pg_class with an MVCC snapshot,
 	 * someone else might drop the table. It's better to return NULL for
	 * already-dropped tables than throw an error and abort the whole query.
	 */
	if (!RelationIsValid(rel))
  		PG_RETURN_NULL();

	if (relOid == 0 || rel->rd_node.relNode == 0)
		size = 0;
	else
		size = calculate_relation_size(rel,
									   forkname_to_number(text_to_cstring(forkName)));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		char *schemaName;
		char *relName;

		schemaName = get_namespace_name(get_rel_namespace(relOid));
		if (schemaName == NULL)
			elog(ERROR, "Cannot find schema for oid %d", relOid);
		relName = get_rel_name(relOid);
		if (relName == NULL)
			elog(ERROR, "Cannot find relation for oid %d", relOid);

		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_relation_size('%s.%s'))::int8 from gp_dist_random('gp_id');", quote_identifier(schemaName), quote_identifier(relName));

		size += get_size_from_segDBs(buffer.data);
	}

	relation_close(rel, AccessShareLock);

	PG_RETURN_INT64(size);
}

/*
 *	Compute the on-disk size of files for the relation according to the
 *	stat function, including heap data, index data, toast data, aoseg data,
 *  aoblkdir data, and aovisimap data.
 */
static int64
calculate_total_relation_size(Oid Relid)
{
	Relation	heapRel;
	Oid			toastOid;
	int64		size;
	ListCell   *cell;
	ForkNumber	forkNum;

	heapRel = try_relation_open(Relid, AccessShareLock, false);

	if (!RelationIsValid(heapRel))
		return 0;

	toastOid = heapRel->rd_rel->reltoastrelid;

	/* Get the heap size */
	if (Relid == 0 || heapRel->rd_node.relNode == 0)
		size = 0;
	else
	{
		size = 0;
		for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
			size += calculate_relation_size(heapRel, forkNum);
	}

	/* Include any dependent indexes */
	if (heapRel->rd_rel->relhasindex)
	{
		List	   *index_oids = RelationGetIndexList(heapRel);

		foreach(cell, index_oids)
		{
			Oid			idxOid = lfirst_oid(cell);
			Relation	iRel;

			iRel = try_relation_open(idxOid, AccessShareLock, false);

			if (RelationIsValid(iRel))
			{
				for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
					size += calculate_relation_size(iRel, forkNum);

				relation_close(iRel, AccessShareLock);
			}
		}

		list_free(index_oids);
	}

	/* Recursively include toast table (and index) size */
	if (OidIsValid(toastOid))
		size += calculate_total_relation_size(toastOid);

	if (RelationIsAoRows(heapRel) || RelationIsAoCols(heapRel))
	{
		Assert(OidIsValid(heapRel->rd_appendonly->segrelid));
		size += calculate_total_relation_size(heapRel->rd_appendonly->segrelid);

        /* block directory may not exist, post upgrade or new table that never has indexes */
   		if (OidIsValid(heapRel->rd_appendonly->blkdirrelid))
        {
     		size += calculate_total_relation_size(heapRel->rd_appendonly->blkdirrelid);
        }
		if (OidIsValid(heapRel->rd_appendonly->visimaprelid))
		{
			size += calculate_total_relation_size(heapRel->rd_appendonly->visimaprelid);
		}
	}

	relation_close(heapRel, AccessShareLock);

	return size;
}

Datum
pg_total_relation_size(PG_FUNCTION_ARGS)
{
	int64		size = 0;
	Oid			relOid = PG_GETARG_OID(0);

	/*
	 * While we scan pg_class with an MVCC snapshot,
	 * someone else might drop the table. It's better to return NULL for
	 * already-dropped tables than throw an error and abort the whole query.
	 */
	if (!OidIsValid(get_rel_name(relOid)))
		PG_RETURN_NULL();

	size = calculate_total_relation_size(relOid);
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		StringInfoData buffer;
		char *schemaName;
		char *relName;

		schemaName = get_namespace_name(get_rel_namespace(relOid));
		if (schemaName == NULL)
		{
			elog(ERROR, "Cannot find schema for oid %d", relOid);
		}

		relName = get_rel_name(relOid);
		if (relName == NULL)
		{
			elog(ERROR, "Cannot find relation for oid %d", relOid);
		}

		initStringInfo(&buffer);

		appendStringInfo(&buffer, "select sum(pg_total_relation_size('%s.%s'))::int8 from gp_dist_random('gp_id');", quote_identifier(schemaName), quote_identifier(relName));

		size += get_size_from_segDBs(buffer.data);
	}

	PG_RETURN_INT64(size);
}

/*
 * formatting with size units
 */
Datum
pg_size_pretty(PG_FUNCTION_ARGS)
{
	int64		size = PG_GETARG_INT64(0);
	char		buf[64];
	int64		limit = 10 * 1024;
	int64		limit2 = limit * 2 - 1;

	if (size < limit)
		snprintf(buf, sizeof(buf), INT64_FORMAT " bytes", size);
	else
	{
		size >>= 9;				/* keep one extra bit for rounding */
		if (size < limit2)
			snprintf(buf, sizeof(buf), INT64_FORMAT " kB",
					 (size + 1) / 2);
		else
		{
			size >>= 10;
			if (size < limit2)
				snprintf(buf, sizeof(buf), INT64_FORMAT " MB",
						 (size + 1) / 2);
			else
			{
				size >>= 10;
				if (size < limit2)
					snprintf(buf, sizeof(buf), INT64_FORMAT " GB",
							 (size + 1) / 2);
				else
				{
					size >>= 10;
					snprintf(buf, sizeof(buf), INT64_FORMAT " TB",
							 (size + 1) / 2);
				}
			}
		}
	}

	PG_RETURN_TEXT_P(cstring_to_text(buf));
}
