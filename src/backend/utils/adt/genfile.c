/*-------------------------------------------------------------------------
 *
 * genfile.c
 *		Functions for direct access to files
 *
 *
 * Copyright (c) 2004-2009, PostgreSQL Global Development Group
 *
 * Author: Andreas Pflug <pgadmin@pse-consulting.de>
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/adt/genfile.c,v 1.21 2009/01/01 17:23:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/datetime.h"


#ifdef WIN32

#ifdef rename
#undef rename
#endif

#ifdef unlink
#undef unlink
#endif
#endif

typedef struct
{
	char	   *location;
	DIR		   *dirdesc;
} directory_fctx;


/*
 * Convert a "text" filename argument to C string, and check it's allowable.
 *
 * Filename may be absolute or relative to the DataDir, but we only allow
 * absolute paths that match DataDir or Log_directory.
 */
static char *
convert_and_check_filename(text *arg)
{
	char	   *filename;

	filename = text_to_cstring(arg);
	canonicalize_path(filename);	/* filename can change length here */

	/* Disallow ".." in the path */
	if (path_contains_parent_reference(filename))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
			(errmsg("reference to parent directory (\"..\") not allowed"))));

	if (is_absolute_path(filename))
	{
		/* Allow absolute references within DataDir */
		if (path_is_prefix_of_path(DataDir, filename))
			return filename;
		/* The log directory might be outside our datadir, but allow it */
		if (is_absolute_path(Log_directory) &&
			path_is_prefix_of_path(Log_directory, filename))
			return filename;

		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("absolute path not allowed"))));
		return NULL;			/* keep compiler quiet */
	}
	else
	{
		return filename;
	}
}

/*
 * check for superuser, bark if not.
 */
static void
requireSuperuser(void)
{
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
			  (errmsg("only superuser may access generic file functions"))));
}

/*
 * Read a section of a file, returning it as bytea
 *
 * Caller is responsible for all permissions checking.
 *
 * We read the whole of the file when bytes_to_read is negative.
 */
bytea *
read_binary_file(const char *filename, int64 seek_offset, int64 bytes_to_read)
{
	bytea	   *buf;
	size_t		nbytes;
	FILE	   *file;

	if (bytes_to_read < 0)
	{
		if (seek_offset < 0)
			bytes_to_read = -seek_offset;
		else
		{
			struct stat fst;

			if (stat(filename, &fst) < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m", filename)));

			bytes_to_read = fst.st_size - seek_offset;
		}
	}

	/* not sure why anyone thought that int64 length was a good idea */
	if (bytes_to_read > (MaxAllocSize - VARHDRSZ))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length too large")));

	if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for reading: %m",
						filename)));

	if (fseeko(file, (off_t) seek_offset,
			   (seek_offset >= 0) ? SEEK_SET : SEEK_END) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file \"%s\": %m", filename)));

	buf = (bytea *) palloc((Size) bytes_to_read + VARHDRSZ);

	nbytes = fread(VARDATA(buf), 1, (size_t) bytes_to_read, file);

	if (ferror(file))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read file \"%s\": %m", filename)));

	SET_VARSIZE(buf, nbytes + VARHDRSZ);

	FreeFile(file);

	return buf;
}

/*
 * Read a section of a file, returning it as text
 */
Datum
pg_read_file(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_P(0);
	int64		seek_offset = PG_GETARG_INT64(1);
	int64		bytes_to_read = PG_GETARG_INT64(2);
	char	   *buf;
	size_t		nbytes;
	FILE	   *file;
	char	   *filename;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files"))));

	filename = convert_and_check_filename(filename_t);

	if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for reading: %m",
						filename)));

	if (fseeko(file, (off_t) seek_offset,
			   (seek_offset >= 0) ? SEEK_SET : SEEK_END) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file \"%s\": %m", filename)));

	if (bytes_to_read < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length cannot be negative")));

	/* not sure why anyone thought that int64 length was a good idea */
	if (bytes_to_read > (MaxAllocSize - VARHDRSZ))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length too large")));

	buf = palloc((Size) bytes_to_read + VARHDRSZ);

	nbytes = fread(VARDATA(buf), 1, (size_t) bytes_to_read, file);

	if (ferror(file))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read file \"%s\": %m", filename)));

	SET_VARSIZE(buf, nbytes + VARHDRSZ);

	FreeFile(file);
	pfree(filename);

	PG_RETURN_TEXT_P(buf);
}

/*
 * stat a file
 */
Datum
pg_stat_file(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_P(0);
	char	   *filename;
	struct stat fst;
	Datum		values[6];
	bool		isnull[6];
	HeapTuple	tuple;
	TupleDesc	tupdesc;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to get file information"))));

	filename = convert_and_check_filename(filename_t);

	if (stat(filename, &fst) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m", filename)));

	/*
	 * This record type had better match the output parameters declared for me
	 * in pg_proc.h.
	 */
	tupdesc = CreateTemplateTupleDesc(6, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1,
					   "size", INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2,
					   "access", TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3,
					   "modification", TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4,
					   "change", TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5,
					   "creation", TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 6,
					   "isdir", BOOLOID, -1, 0);
	BlessTupleDesc(tupdesc);

	memset(isnull, false, sizeof(isnull));

	values[0] = Int64GetDatum((int64) fst.st_size);
	values[1] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_atime));
	values[2] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_mtime));
	/* Unix has file status change time, while Win32 has creation time */
#if !defined(WIN32) && !defined(__CYGWIN__)
	values[3] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_ctime));
	isnull[4] = true;
#else
	isnull[3] = true;
	values[4] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_ctime));
#endif
	values[5] = BoolGetDatum(S_ISDIR(fst.st_mode));

	tuple = heap_form_tuple(tupdesc, values, isnull);

	pfree(filename);

	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*
 * List a directory (returns the filenames only)
 */
Datum
pg_ls_dir(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	struct dirent *de;
	directory_fctx *fctx;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to get directory listings"))));

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		fctx = palloc(sizeof(directory_fctx));
		fctx->location = convert_and_check_filename(PG_GETARG_TEXT_P(0));

		fctx->dirdesc = AllocateDir(fctx->location);

		if (!fctx->dirdesc)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open directory \"%s\": %m",
							fctx->location)));

		funcctx->user_fctx = fctx;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	fctx = (directory_fctx *) funcctx->user_fctx;

	while ((de = ReadDir(fctx->dirdesc, fctx->location)) != NULL)
	{
		if (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0)
			continue;

		SRF_RETURN_NEXT(funcctx, CStringGetTextDatum(de->d_name));
	}

	FreeDir(fctx->dirdesc);

	SRF_RETURN_DONE(funcctx);
}

/* ------------------------------------
 * generic file handling functions
 */

Datum
pg_file_write(PG_FUNCTION_ARGS)
{
	FILE	   *f;
	char	   *filename;
	text	   *data;
	int64		count = 0;

	requireSuperuser();

	filename = convert_and_check_filename(PG_GETARG_TEXT_P(0));
	data = PG_GETARG_TEXT_P(1);

	if (!PG_GETARG_BOOL(2))
	{
		struct stat fst;

		if (stat(filename, &fst) >= 0)
			ereport(ERROR,
					(ERRCODE_DUPLICATE_FILE,
					 errmsg("file \"%s\" exists", filename)));

		f = fopen(filename, "wb");
	}
	else
		f = fopen(filename, "ab");

	if (!f)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for writing: %m",
						filename)));

	if (VARSIZE(data) != 0)
	{
		count = fwrite(VARDATA(data), 1, VARSIZE(data) - VARHDRSZ, f);

		if (count != VARSIZE(data) - VARHDRSZ)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write file \"%s\": %m", filename)));
	}
	fclose(f);

	PG_RETURN_INT64(count);
}


Datum
pg_file_rename(PG_FUNCTION_ARGS)
{
	char	   *fn1,
			   *fn2,
			   *fn3;
	int			rc;

	requireSuperuser();

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();

	fn1 = convert_and_check_filename(PG_GETARG_TEXT_P(0));
	fn2 = convert_and_check_filename(PG_GETARG_TEXT_P(1));
	if (PG_ARGISNULL(2))
		fn3 = 0;
	else
		fn3 = convert_and_check_filename(PG_GETARG_TEXT_P(2));

	if (access(fn1, W_OK) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("file \"%s\" is not accessible: %m", fn1)));

		PG_RETURN_BOOL(false);
	}

	if (fn3 && access(fn2, W_OK) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("file \"%s\" is not accessible: %m", fn2)));

		PG_RETURN_BOOL(false);
	}

	rc = access(fn3 ? fn3 : fn2, 2);
	if (rc >= 0 || errno != ENOENT)
	{
		ereport(ERROR,
				(ERRCODE_DUPLICATE_FILE,
				 errmsg("cannot rename to target file \"%s\"",
						fn3 ? fn3 : fn2)));
	}

	if (fn3)
	{
		if (rename(fn2, fn3) != 0)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not rename \"%s\" to \"%s\": %m",
							fn2, fn3)));
		}
		if (rename(fn1, fn2) != 0)
		{
			ereport(WARNING,
					(errcode_for_file_access(),
					 errmsg("could not rename \"%s\" to \"%s\": %m",
							fn1, fn2)));

			if (rename(fn3, fn2) != 0)
			{
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not rename \"%s\" back to \"%s\": %m",
								fn3, fn2)));
			}
			else
			{
				ereport(ERROR,
						(ERRCODE_UNDEFINED_FILE,
						 errmsg("renaming \"%s\" to \"%s\" was reverted",
								fn2, fn3)));
			}
		}
	}
	else if (rename(fn1, fn2) != 0)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not rename \"%s\" to \"%s\": %m", fn1, fn2)));
	}

	PG_RETURN_BOOL(true);
}


Datum
pg_file_unlink(PG_FUNCTION_ARGS)
{
	char	   *filename;

	requireSuperuser();

	filename = convert_and_check_filename(PG_GETARG_TEXT_P(0));

	if (access(filename, W_OK) < 0)
	{
		if (errno == ENOENT)
			PG_RETURN_BOOL(false);
		else
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("file \"%s\" is not accessible: %m", filename)));
	}

	if (unlink(filename) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("could not unlink file \"%s\": %m", filename)));

		PG_RETURN_BOOL(false);
	}
	PG_RETURN_BOOL(true);
}


Datum
pg_logdir_ls(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	struct dirent *de;
	directory_fctx *fctx;
    bool prefix_is_gpdb = true;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("only superuser can list the log directory"))));

	if (strcmp(Log_filename, "gpdb-%Y-%m-%d_%H%M%S.csv") != 0 &&
        strcmp(Log_filename, "gpdb-%Y-%m-%d_%H%M%S.log") != 0 &&
        strcmp(Log_filename, "postgresql-%Y-%m-%d_%H%M%S.log") != 0 )
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("the log_filename parameter must equal 'gpdb-%%Y-%%m-%%d_%%H%%M%%S.csv'"))));

	if (strncmp(Log_filename, "gpdb", 4) != 0)
		prefix_is_gpdb = false;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		fctx = palloc(sizeof(directory_fctx));

		tupdesc = CreateTemplateTupleDesc(2, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "starttime",
						   TIMESTAMPOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "filename",
						   TEXTOID, -1, 0);

		funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

		fctx->location = pstrdup(Log_directory);
		fctx->dirdesc = AllocateDir(fctx->location);

		if (!fctx->dirdesc)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read directory \"%s\": %m",
							fctx->location)));

		funcctx->user_fctx = fctx;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	fctx = (directory_fctx *) funcctx->user_fctx;

	while ((de = ReadDir(fctx->dirdesc, fctx->location)) != NULL)
	{
		char	   *values[2];
		HeapTuple	tuple;
		char		timestampbuf[32];
		char	   *field[MAXDATEFIELDS];
		char		lowstr[MAXDATELEN + 1];
		int			dtype;
		int			nf,
					ftype[MAXDATEFIELDS];
		fsec_t		fsec;
		int			tz = 0;
		struct pg_tm date;

        if (prefix_is_gpdb)
        {
            int end = 17;
            /*
		     * Default format: gpdb-YYYY-MM-DD_HHMMSS.log or gpdb-YYYY-MM-DD_HHMMSS.csv
		     */
		    if (strlen(de->d_name) != 26
			    || strncmp(de->d_name, "gpdb-", 5) != 0
			    || de->d_name[15] != '_'
			    || (strcmp(de->d_name + 22, ".log") != 0 && strcmp(de->d_name + 22, ".csv") != 0))
            {
			    
                /* 
                 * Not our normal format.  Maybe old format without TIME fields?
                 */
             
                if (strlen(de->d_name) != 26
			    || strncmp(de->d_name, "gpdb-", 5) != 0
			    || de->d_name[15] != '_'
			    || (strcmp(de->d_name + 22, ".log") != 0 && strcmp(de->d_name + 22, ".csv") != 0))
                    continue;

                end = 10;

            }
		    /* extract timestamp portion of filename */
		    snprintf(timestampbuf, sizeof(timestampbuf), "%s", de->d_name + 5);
		    timestampbuf[end] = '\0';
        }
        else
        {
		    /*
		     * Default format: postgresql-YYYY-MM-DD_HHMMSS.log
		     */
		    if (strlen(de->d_name) != 32
			    || strncmp(de->d_name, "postgresql-", 11) != 0
			    || de->d_name[21] != '_'
			    || strcmp(de->d_name + 28, ".log") != 0)
			    continue;

		    /* extract timestamp portion of filename */
			snprintf(timestampbuf, sizeof(timestampbuf), "%s", de->d_name + 11);
		    timestampbuf[17] = '\0';
        }

		/* parse and decode expected timestamp to verify it's OK format */
		if (ParseDateTime(timestampbuf, lowstr, MAXDATELEN, field, ftype, MAXDATEFIELDS, &nf))
			continue;

		if (DecodeDateTime(field, ftype, nf, &dtype, &date, &fsec, &tz))
			continue;

		/* Seems the timestamp is OK; prepare and return tuple */

		values[0] = timestampbuf;
		values[1] = palloc(strlen(fctx->location) + strlen(de->d_name) + 2);
		sprintf(values[1], "%s/%s", fctx->location, de->d_name);

		tuple = BuildTupleFromCStrings(funcctx->attinmeta, values);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}

	FreeDir(fctx->dirdesc);
	SRF_RETURN_DONE(funcctx);
}

Datum
pg_file_length(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_P(0);
	char	   *filename;
	struct stat fst;

	requireSuperuser();

	filename = convert_and_check_filename(filename_t);

	if (stat(filename, &fst) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m", filename)));

	PG_RETURN_INT64((int64) fst.st_size);
}
