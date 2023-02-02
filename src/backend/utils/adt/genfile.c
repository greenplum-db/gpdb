/*-------------------------------------------------------------------------
 *
 * genfile.c
 *		Functions for direct access to files
 *
 *
 * Copyright (c) 2004-2019, PostgreSQL Global Development Group
 *
 * Author: Andreas Pflug <pgadmin@pse-consulting.de>
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/genfile.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "access/htup_details.h"
#include "access/xlog_internal.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace_d.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
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


/*
 * Convert a "text" filename argument to C string, and check it's allowable.
 *
 * Filename may be absolute or relative to the DataDir, but we only allow
 * absolute paths that match DataDir or Log_directory.
 *
 * This does a privilege check against the 'pg_read_server_files' role, so
 * this function is really only appropriate for callers who are only checking
 * 'read' access.  Do not use this function if you are looking for a check
 * for 'write' or 'program' access without updating it to access the type
 * of check as an argument and checking the appropriate role membership.
 */
static char *
convert_and_check_filename(text *arg)
{
	char	   *filename;

	filename = text_to_cstring(arg);
	canonicalize_path(filename);	/* filename can change length here */

	/*
	 * Members of the 'pg_read_server_files' role are allowed to access any
	 * files on the server as the PG user, so no need to do any further checks
	 * here.
	 */
	if (is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_SERVER_FILES))
		return filename;

	/* User isn't a member of the default role, so check if it's allowable */
	if (is_absolute_path(filename))
	{
		/* Disallow '/a/b/data/..' */
		if (path_contains_parent_reference(filename))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 (errmsg("reference to parent directory (\"..\") not allowed"))));

		/*
		 * Allow absolute paths if within DataDir or Log_directory, even
		 * though Log_directory might be outside DataDir.
		 */
		if (!path_is_prefix_of_path(DataDir, filename) &&
			(!is_absolute_path(Log_directory) ||
			 !path_is_prefix_of_path(Log_directory, filename)))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 (errmsg("absolute path not allowed"))));
	}
	else if (!path_is_relative_and_below_cwd(filename))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("path must be in or below the current directory"))));

	return filename;
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
static bytea *
read_binary_file(const char *filename, int64 seek_offset, int64 bytes_to_read,
				 bool missing_ok)
{
	bytea	   *buf;
	size_t		nbytes = 0;
	FILE	   *file;

	/* clamp request size to what we can actually deliver */
	if (bytes_to_read > (int64) (MaxAllocSize - VARHDRSZ))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length too large")));

	if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL)
	{
		if (missing_ok && errno == ENOENT)
			return NULL;
		else
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\" for reading: %m",
							filename)));
	}

	if (fseeko(file, (off_t) seek_offset,
			   (seek_offset >= 0) ? SEEK_SET : SEEK_END) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file \"%s\": %m", filename)));

	if (bytes_to_read >= 0)
	{
		/* If passed explicit read size just do it */
		buf = (bytea *) palloc((Size) bytes_to_read + VARHDRSZ);

		nbytes = fread(VARDATA(buf), 1, (size_t) bytes_to_read, file);
	}
	else
	{
		/* Negative read size, read rest of file */
		StringInfoData sbuf;

		initStringInfo(&sbuf);
		/* Leave room in the buffer for the varlena length word */
		sbuf.len += VARHDRSZ;
		Assert(sbuf.len < sbuf.maxlen);

		while (!(feof(file) || ferror(file)))
		{
			size_t		rbytes;

			/* Minimum amount to read at a time */
#define MIN_READ_SIZE 4096

			/*
			 * If not at end of file, and sbuf.len is equal to
			 * MaxAllocSize - 1, then either the file is too large, or
			 * there is nothing left to read. Attempt to read one more
			 * byte to see if the end of file has been reached. If not,
			 * the file is too large; we'd rather give the error message
			 * for that ourselves.
			 */
			if (sbuf.len == MaxAllocSize - 1)
			{
				char	rbuf[1];

				if (fread(rbuf, 1, 1, file) != 0 || !feof(file))
					ereport(ERROR,
							(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
							 errmsg("file length too large")));
				else
					break;
			}

			/* OK, ensure that we can read at least MIN_READ_SIZE */
			enlargeStringInfo(&sbuf, MIN_READ_SIZE);

			/*
			 * stringinfo.c likes to allocate in powers of 2, so it's likely
			 * that much more space is available than we asked for.  Use all
			 * of it, rather than making more fread calls than necessary.
			 */
			rbytes = fread(sbuf.data + sbuf.len, 1,
						   (size_t) (sbuf.maxlen - sbuf.len - 1), file);
			sbuf.len += rbytes;
			nbytes += rbytes;
		}

		/* Now we can commandeer the stringinfo's buffer as the result */
		buf = (bytea *) sbuf.data;
	}

	if (ferror(file))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read file \"%s\": %m", filename)));

	SET_VARSIZE(buf, nbytes + VARHDRSZ);

	FreeFile(file);

	return buf;
}

/*
 * Similar to read_binary_file, but we verify that the contents are valid
 * in the database encoding.
 */
static text *
read_text_file(const char *filename, int64 seek_offset, int64 bytes_to_read,
			   bool missing_ok)
{
	bytea	   *buf;

	buf = read_binary_file(filename, seek_offset, bytes_to_read, missing_ok);

	if (buf != NULL)
	{
		/* Make sure the input is valid */
		pg_verifymbstr(VARDATA(buf), VARSIZE(buf) - VARHDRSZ, false);

		/* OK, we can cast it to text safely */
		return (text *) buf;
	}
	else
		return NULL;
}

/*
 * Read a section of a file, returning it as text
 *
 * This function is kept to support adminpack 1.0.
 */
Datum
pg_read_file(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_PP(0);
	int64		seek_offset = 0;
	int64		bytes_to_read = -1;
	bool		missing_ok = false;
	char	   *filename;
	text	   *result;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files with adminpack 1.0"),
		/* translator: %s is a SQL function name */
				  errhint("Consider using %s, which is part of core, instead.",
						  "pg_read_file()"))));

	/* handle optional arguments */
	if (PG_NARGS() >= 3)
	{
		seek_offset = PG_GETARG_INT64(1);
		bytes_to_read = PG_GETARG_INT64(2);

		if (bytes_to_read < 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("requested length cannot be negative")));
	}
	if (PG_NARGS() >= 4)
		missing_ok = PG_GETARG_BOOL(3);

	filename = convert_and_check_filename(filename_t);

	result = read_text_file(filename, seek_offset, bytes_to_read, missing_ok);
	if (result)
		PG_RETURN_TEXT_P(result);
	else
		PG_RETURN_NULL();
}

/*
 * Read a section of a file, returning it as text
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_read_file_v2(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_PP(0);
	int64		seek_offset = 0;
	int64		bytes_to_read = -1;
	bool		missing_ok = false;
	char	   *filename;
	text	   *result;

	/* handle optional arguments */
	if (PG_NARGS() >= 3)
	{
		seek_offset = PG_GETARG_INT64(1);
		bytes_to_read = PG_GETARG_INT64(2);

		if (bytes_to_read < 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("requested length cannot be negative")));
	}
	if (PG_NARGS() >= 4)
		missing_ok = PG_GETARG_BOOL(3);

	filename = convert_and_check_filename(filename_t);

	result = read_text_file(filename, seek_offset, bytes_to_read, missing_ok);
	if (result)
		PG_RETURN_TEXT_P(result);
	else
		PG_RETURN_NULL();
}

/*
 * Read a section of a file, returning it as bytea
 */
Datum
pg_read_binary_file(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_PP(0);
	int64		seek_offset = 0;
	int64		bytes_to_read = -1;
	bool		missing_ok = false;
	char	   *filename;
	bytea	   *result;

	/* handle optional arguments */
	if (PG_NARGS() >= 3)
	{
		seek_offset = PG_GETARG_INT64(1);
		bytes_to_read = PG_GETARG_INT64(2);

		if (bytes_to_read < 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("requested length cannot be negative")));
	}
	if (PG_NARGS() >= 4)
		missing_ok = PG_GETARG_BOOL(3);

	filename = convert_and_check_filename(filename_t);

	result = read_binary_file(filename, seek_offset,
							  bytes_to_read, missing_ok);
	if (result)
		PG_RETURN_BYTEA_P(result);
	else
		PG_RETURN_NULL();
}


/*
 * Wrapper functions for the 1 and 3 argument variants of pg_read_file_v2()
 * and pg_binary_read_file().
 *
 * These are necessary to pass the sanity check in opr_sanity, which checks
 * that all built-in functions that share the implementing C function take
 * the same number of arguments.
 */
Datum
pg_read_file_off_len(PG_FUNCTION_ARGS)
{
	return pg_read_file_v2(fcinfo);
}

Datum
pg_read_file_all(PG_FUNCTION_ARGS)
{
	return pg_read_file_v2(fcinfo);
}

Datum
pg_read_binary_file_off_len(PG_FUNCTION_ARGS)
{
	return pg_read_binary_file(fcinfo);
}

Datum
pg_read_binary_file_all(PG_FUNCTION_ARGS)
{
	return pg_read_binary_file(fcinfo);
}

/*
 * stat a file
 */
Datum
pg_stat_file(PG_FUNCTION_ARGS)
{
	text	   *filename_t = PG_GETARG_TEXT_PP(0);
	char	   *filename;
	struct stat fst;
	Datum		values[6];
	bool		isnull[6];
	HeapTuple	tuple;
	TupleDesc	tupdesc;
	bool		missing_ok = false;

	/* check the optional argument */
	if (PG_NARGS() == 2)
		missing_ok = PG_GETARG_BOOL(1);

	filename = convert_and_check_filename(filename_t);

	if (stat(filename, &fst) < 0)
	{
		if (missing_ok && errno == ENOENT)
			PG_RETURN_NULL();
		else
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not stat file \"%s\": %m", filename)));
	}

	/*
	 * This record type had better match the output parameters declared for me
	 * in pg_proc.h.
	 */
	tupdesc = CreateTemplateTupleDesc(6);
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
 * stat a file (1 argument version)
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments
 */
Datum
pg_stat_file_1arg(PG_FUNCTION_ARGS)
{
	return pg_stat_file(fcinfo);
}

/*
 * List a directory (returns the filenames only)
 */
Datum
pg_ls_dir(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	char	   *location;
	bool		missing_ok = false;
	bool		include_dot_dirs = false;
	bool		randomAccess;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	DIR		   *dirdesc;
	struct dirent *de;
	MemoryContext oldcontext;

	location = convert_and_check_filename(PG_GETARG_TEXT_PP(0));

	/* check the optional arguments */
	if (PG_NARGS() == 3)
	{
		if (!PG_ARGISNULL(1))
			missing_ok = PG_GETARG_BOOL(1);
		if (!PG_ARGISNULL(2))
			include_dot_dirs = PG_GETARG_BOOL(2);
	}

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("materialize mode required, but it is not allowed in this context")));

	/* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
	oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

	tupdesc = CreateTemplateTupleDesc(1);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "pg_ls_dir", TEXTOID, -1, 0);

	randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
	tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	dirdesc = AllocateDir(location);
	if (!dirdesc)
	{
		/* Return empty tuplestore if appropriate */
		if (missing_ok && errno == ENOENT)
			return (Datum) 0;
		/* Otherwise, we can let ReadDir() throw the error */
	}

	while ((de = ReadDir(dirdesc, location)) != NULL)
	{
		Datum		values[1];
		bool		nulls[1];

		if (!include_dot_dirs &&
			(strcmp(de->d_name, ".") == 0 ||
			 strcmp(de->d_name, "..") == 0))
			continue;

		values[0] = CStringGetTextDatum(de->d_name);
		nulls[0] = false;

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	FreeDir(dirdesc);
	return (Datum) 0;
}

/*
 * List a directory (1 argument version)
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments.
 */
Datum
pg_ls_dir_1arg(PG_FUNCTION_ARGS)
{
	return pg_ls_dir(fcinfo);
}

/* ------------------------------------
 * pg_file_write_internal - Workhorse for pg_file_write functions.
 *
 * This handles the actual work for pg_file_write.
 */
static int64
pg_file_write_internal(text *file, text *data, bool replace)
{
	FILE       *f;
	char       *filename;
	int64           count = 0;

	filename = convert_and_check_filename(file);

	if (!replace)
	{
		struct stat fst;

		if (stat(filename, &fst) >= 0)
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_FILE),
					 errmsg("file \"%s\" exists", filename)));

		f = AllocateFile(filename, "wb");
	}
	else
		f = AllocateFile(filename, "ab");

	if (!f)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for writing: %m",
					 filename)));

	count = fwrite(VARDATA_ANY(data), 1, VARSIZE_ANY_EXHDR(data), f);
	if (count != VARSIZE_ANY_EXHDR(data) || FreeFile(f))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write file \"%s\": %m", filename)));

	return (count);
}

/* ------------------------------------
 * pg_file_write - old version
 *
 * keep the superuser check
 */
Datum
pg_file_write(PG_FUNCTION_ARGS)
{
	text	   *file = PG_GETARG_TEXT_PP(0);
	text	   *data = PG_GETARG_TEXT_PP(1);
	bool		replace = PG_GETARG_BOOL(2);
	int64		count = 0;

	requireSuperuser();

	count = pg_file_write_internal(file, data, replace);

	PG_RETURN_INT64(count);
}

/* ------------------------------------
 * pg_file_write_v1_1 - Version 1.1
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_file_write_v1_1(PG_FUNCTION_ARGS)
{
	text	   *file = PG_GETARG_TEXT_PP(0);
	text	   *data = PG_GETARG_TEXT_PP(1);
	bool		replace = PG_GETARG_BOOL(2);
	int64		count = 0;

	count = pg_file_write_internal(file, data, replace);

	PG_RETURN_INT64(count);
}


/* ------------------------------------
 * pg_file_rename_internal - Workhorse for pg_file_rename functions.
 *
 * This handles the actual work for pg_file_rename.
 */
static bool
pg_file_rename_internal(text *file1, text *file2, text *file3)
{
	char	   *fn1,
			   *fn2,
			   *fn3;
	int			rc;

	fn1 = convert_and_check_filename(file1);
	fn2 = convert_and_check_filename(file2);

	if (file3 == NULL)
		fn3 = NULL;
	else
		fn3 = convert_and_check_filename(file3);

	if (access(fn1, W_OK) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("file \"%s\" is not accessible: %m", fn1)));

		return false;
	}

	if (fn3 && access(fn2, W_OK) < 0)
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("file \"%s\" is not accessible: %m", fn2)));

		return false;
	}

	rc = access(fn3 ? fn3 : fn2, W_OK);
	if (rc >= 0 || errno != ENOENT)
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_FILE),
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
						(errcode(ERRCODE_UNDEFINED_FILE),
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

	return true;
}

/* ------------------------------------
 * pg_file_rename - old version
 *
 * keep the superuser check
 */
Datum
pg_file_rename(PG_FUNCTION_ARGS)
{
	text	   *file1;
	text	   *file2;
	text	   *file3;
	bool		result;

	requireSuperuser();

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();

	file1 = PG_GETARG_TEXT_PP(0);
	file2 = PG_GETARG_TEXT_PP(1);

	if (PG_ARGISNULL(2))
		file3 = NULL;
	else
		file3 = PG_GETARG_TEXT_PP(2);

	result = pg_file_rename_internal(file1, file2, file3);

	PG_RETURN_BOOL(result);
}

/* ------------------------------------
 * pg_file_rename_v1_1 - Version 1.1
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_file_rename_v1_1(PG_FUNCTION_ARGS)
{
	text	   *file1;
	text	   *file2;
	text	   *file3;
	bool		result;

	if (PG_ARGISNULL(0) || PG_ARGISNULL(1))
		PG_RETURN_NULL();

	file1 = PG_GETARG_TEXT_PP(0);
	file2 = PG_GETARG_TEXT_PP(1);

	if (PG_ARGISNULL(2))
		file3 = NULL;
	else
		file3 = PG_GETARG_TEXT_PP(2);

	result = pg_file_rename_internal(file1, file2, file3);

	PG_RETURN_BOOL(result);
}

/* ------------------------------------
 * pg_file_unlink - old version
 *
 * keep the superuser check
 */
Datum
pg_file_unlink(PG_FUNCTION_ARGS)
{
	char	   *filename;

	requireSuperuser();

	filename = convert_and_check_filename(PG_GETARG_TEXT_PP(0));

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

/* ------------------------------------
 * pg_file_unlink_v1_1 - Version 1.1
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_file_unlink_v1_1(PG_FUNCTION_ARGS)
{
	char	   *filename;

	filename = convert_and_check_filename(PG_GETARG_TEXT_PP(0));

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

static Datum
pg_logdir_ls_internal(FunctionCallInfo fcinfo)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	bool		randomAccess;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	AttInMetadata *attinmeta;
	DIR		   *dirdesc;
	struct dirent *de;
	MemoryContext oldcontext;
	bool prefix_is_gpdb = true;

	if (strcmp(Log_filename, "gpdb-%Y-%m-%d_%H%M%S.csv") != 0 &&
		strcmp(Log_filename, "gpdb-%Y-%m-%d_%H%M%S.log") != 0 &&
		strcmp(Log_filename, "postgresql-%Y-%m-%d_%H%M%S.log") != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("the log_filename parameter must equal 'gpdb-%%Y-%%m-%%d_%%H%%M%%S.csv'")));

	if (strncmp(Log_filename, "gpdb", 4) != 0)
		prefix_is_gpdb = false;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("materialize mode required, but it is not allowed in this context")));

	/* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
	oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

	tupdesc = CreateTemplateTupleDesc(2);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "starttime",
					   TIMESTAMPOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "filename",
					   TEXTOID, -1, 0);

	randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
	tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	attinmeta = TupleDescGetAttInMetadata(tupdesc);

	dirdesc = AllocateDir(Log_directory);
	while ((de = ReadDir(dirdesc, Log_directory)) != NULL)
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
			/*
			 * Default format: gpdb-YYYY-MM-DD_HHMMSS.log or gpdb-YYYY-MM-DD_HHMMSS.csv
			 */
			if (strlen(de->d_name) != 26
				|| strncmp(de->d_name, "gpdb-", 5) != 0
				|| de->d_name[15] != '_'
				|| (strcmp(de->d_name + 22, ".log") != 0 && strcmp(de->d_name + 22, ".csv") != 0))
				continue;

			/* extract timestamp portion of filename */
			snprintf(timestampbuf, sizeof(timestampbuf), "%s", de->d_name + 5);
			timestampbuf[17] = '\0';
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
		values[1] = psprintf("%s/%s", Log_directory, de->d_name);

		tuple = BuildTupleFromCStrings(attinmeta, values);

		tuplestore_puttuple(tupstore, tuple);
	}

	FreeDir(dirdesc);
	return (Datum) 0;
}

/* ------------------------------------
 * pg_logdir_ls - Old version
 *
 * keep the superuser check
 */
Datum
pg_logdir_ls(PG_FUNCTION_ARGS)
{
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("only superuser can list the log directory"))));

	return (pg_logdir_ls_internal(fcinfo));
}

/* ------------------------------------
 * pg_logdir_ls_v1_1 - Version 1.1
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_logdir_ls_v1_1(PG_FUNCTION_ARGS)
{
	return (pg_logdir_ls_internal(fcinfo));
}

/*
 * Generic function to return a directory listing of files.
 *
 * If the directory isn't there, silently return an empty set if missing_ok.
 * Other unreadable-directory cases throw an error.
 */
static Datum
pg_ls_dir_files(FunctionCallInfo fcinfo, const char *dir, bool missing_ok)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	bool		randomAccess;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	DIR		   *dirdesc;
	struct dirent *de;
	MemoryContext oldcontext;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("materialize mode required, but it is not allowed in this context")));

	/* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
	oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
	tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	/*
	 * Now walk the directory.  Note that we must do this within a single SRF
	 * call, not leave the directory open across multiple calls, since we
	 * can't count on the SRF being run to completion.
	 */
	dirdesc = AllocateDir(dir);
	if (!dirdesc)
	{
		/* Return empty tuplestore if appropriate */
		if (missing_ok && errno == ENOENT)
			return (Datum) 0;
		/* Otherwise, we can let ReadDir() throw the error */
	}

	while ((de = ReadDir(dirdesc, dir)) != NULL)
	{
		Datum		values[3];
		bool		nulls[3];
		char		path[MAXPGPATH * 2];
		struct stat attrib;

		/* Skip hidden files */
		if (de->d_name[0] == '.')
			continue;

		/* Get the file info */
		snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
		if (stat(path, &attrib) < 0)
		{
			/* Ignore concurrently-deleted files, else complain */
			if (errno == ENOENT)
				continue;
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not stat file \"%s\": %m", path)));
		}

		/* Ignore anything but regular files */
		if (!S_ISREG(attrib.st_mode))
			continue;

		values[0] = CStringGetTextDatum(de->d_name);
		values[1] = Int64GetDatum((int64) attrib.st_size);
		values[2] = TimestampTzGetDatum(time_t_to_timestamptz(attrib.st_mtime));
		memset(nulls, 0, sizeof(nulls));

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	FreeDir(dirdesc);
	return (Datum) 0;
}

/* Function to return the list of files in the log directory */
Datum
pg_ls_logdir(PG_FUNCTION_ARGS)
{
	return pg_ls_dir_files(fcinfo, Log_directory, false);
}

/* Function to return the list of files in the WAL directory */
Datum
pg_ls_waldir(PG_FUNCTION_ARGS)
{
	return pg_ls_dir_files(fcinfo, XLOGDIR, false);
}

/*
 * Generic function to return the list of files in pgsql_tmp
 */
static Datum
pg_ls_tmpdir(FunctionCallInfo fcinfo, Oid tblspc)
{
	char		path[MAXPGPATH];

	if (!SearchSysCacheExists1(TABLESPACEOID, ObjectIdGetDatum(tblspc)))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("tablespace with OID %u does not exist",
						tblspc)));

	TempTablespacePath(path, tblspc);
	return pg_ls_dir_files(fcinfo, path, true);
}

/*
 * Function to return the list of temporary files in the pg_default tablespace's
 * pgsql_tmp directory
 */
Datum
pg_ls_tmpdir_noargs(PG_FUNCTION_ARGS)
{
	return pg_ls_tmpdir(fcinfo, DEFAULTTABLESPACE_OID);
}

/*
 * Function to return the list of temporary files in the specified tablespace's
 * pgsql_tmp directory
 */
Datum
pg_ls_tmpdir_1arg(PG_FUNCTION_ARGS)
{
	return pg_ls_tmpdir(fcinfo, PG_GETARG_OID(0));
}

/*
 * Function to return the list of files in the WAL archive status directory.
 */
Datum
pg_ls_archive_statusdir(PG_FUNCTION_ARGS)
{
	return pg_ls_dir_files(fcinfo, XLOGDIR "/archive_status", true);
}
