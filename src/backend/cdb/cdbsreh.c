/*--------------------------------------------------------------------------
 *
 * cdbsreh.c
 *	  Provides routines for single row error handling for COPY and external
 *	  tables.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsreh.c
 *
 *--------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "libpq-fe.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/gp_policy.h"
#include "catalog/namespace.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "funcapi.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"
#include "utils/syscache.h"

static void ErrorLogFileName(Oid dbid, Oid relid, bool persistent, char *fname /* out */);
static void PreprocessByteaData(char *src);
static void ErrorLogWrite(CdbSreh *cdbsreh);
static Datum ReadValidErrorLogDatum(FILE *fp, TupleDesc tupledesc, const char* fname);
static bool RetrievePersistentErrorLogFromRangeVar(RangeVar *relrv, AclMode mode, char *fname /*out*/);
static bool ErrorLogPrefixDelete(Oid databaseId, Oid namespaceId, bool persistent);
static bool TruncateErrorLog(text *relname, bool persistent);

#define ErrorLogDir "errlog"
#define PersistentErrorLogDir "errlogpersistent"

#define ErrorLogNormalFileName(fname, dbId, relId) \
	snprintf(fname, MAXPGPATH, "%s/%u_%u", ErrorLogDir, dbId, relId)

#define ErrorLogPersistentFileName(fname, dbId, namespaceId, relName) \
	snprintf(fname, MAXPGPATH, "%s/%u_%u_%s", PersistentErrorLogDir, dbId, namespaceId, relName)

/*
 * Function context for gp_read_error_log
 */
typedef struct ReadErrorLogContext
{
	FILE	   *fp;				/* file pointer to the error log */
	char		filename[MAXPGPATH];	/* filename of fp */
} ReadErrorLogContext;

typedef enum RejectLimitCode
{
	REJECT_NONE = 0,
	REJECT_FIRST_BAD_LIMIT,
	REJECT_LIMIT_REACHED
} RejectLimitCode;

int			gp_initial_bad_row_limit = 1000;


/*
 * ErrorLogFileName - get error log file path.
 *
 * If the current error log is set to be persistent.
 * The log file will be different from normal one.
 */
static void
ErrorLogFileName(Oid dbid, Oid relid, bool persistent, char *fname /* out */)
{
	Assert(fname);
	Assert(OidIsValid(relid) && OidIsValid(dbid));

	if (persistent)
	{
		char	   *relname = get_rel_name(relid);
		Oid namespace = get_rel_namespace(relid);

		if (OidIsValid(namespace))
			ErrorLogPersistentFileName(fname, dbid, namespace, relname);
		else
			elog(ERROR, "relid %u does not exist for db %u", relid, dbid);
	}
	else
		ErrorLogNormalFileName(fname, dbid, relid);
}
/*
 * makeCdbSreh
 *
 * Allocate and initialize a Single Row Error Handling state object.
 * Pass in the only known parameters (both we get from the SQL stmt),
 * the other variables are set later on, when they are known.
 */
CdbSreh *
makeCdbSreh(int rejectlimit, bool is_limit_in_rows,
			char *filename, char *relname,
			char logerrors)
{
	CdbSreh    *h;

	h = palloc(sizeof(CdbSreh));

	h->errmsg = NULL;
	h->rawdata = NULL;
	h->linenumber = 0;
	h->processed = 0;
	h->relname = relname;
	h->rejectlimit = rejectlimit;
	h->is_limit_in_rows = is_limit_in_rows;
	h->rejectcount = 0;
	h->is_server_enc = false;
	h->logerrors = logerrors;

	snprintf(h->filename, sizeof(h->filename),
			 "%s", filename ? filename : "<stdin>");

	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype input routines, and should be faster than retail pfree's
	 * anyway.
	 */
	h->badrowcontext = AllocSetContextCreate(CurrentMemoryContext,
											 "SrehMemCtxt",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);

	return h;
}

void
destroyCdbSreh(CdbSreh *cdbsreh)
{

	/* delete the bad row context */
	MemoryContextDelete(cdbsreh->badrowcontext);

	pfree(cdbsreh);
}

/*
 * HandleSingleRowError
 *
 * The single row error handler. Gets called when a data error happened
 * in SREH mode. Reponsible for making a decision of what to do at that time.
 *
 * Some of the main actions are:
 *  - Keep track of reject limit. if reached make sure notify caller.
 *  - If error logging used, call the error logger to log this error.
 *  - If QD COPY send the bad row to the QE COPY to deal with.
 *
 */
void
HandleSingleRowError(CdbSreh *cdbsreh)
{

	/* increment total number of errors for this segment */
	cdbsreh->rejectcount++;

	/*
	 * If not specified table or file, do nothing.  Otherwise, record the
	 * error: QD - send the bad data row to a random QE (via roundrobin). QE -
	 * log the error in the error log file.
	 */
	if (IS_LOG_TO_FILE(cdbsreh->logerrors))
	{
		if (Gp_role == GP_ROLE_DISPATCH)
			elog(ERROR, "cannot not log suppressed input error in dispatcher");

		ErrorLogWrite(cdbsreh);
	}
}

/*
 * Returns the fixed schema for error log tuple.
 */
static TupleDesc
GetErrorTupleDesc(void)
{
	static TupleDesc tupdesc = NULL;

	/*
	 * Create the tuple descriptor on first call, and reuse on subsequent
	 * calls. It should never be scribbled on.
	 */
	if (tupdesc == NULL)
	{
		TupleDesc	tmp;
		MemoryContext oldcontext = MemoryContextSwitchTo(CacheMemoryContext);

		tmp = CreateTemplateTupleDesc(NUM_ERRORTABLE_ATTR, false);
		TupleDescInitEntry(tmp, 1, "cmdtime", TIMESTAMPTZOID, -1, 0);
		TupleDescInitEntry(tmp, 2, "relname", TEXTOID, -1, 0);
		TupleDescInitEntry(tmp, 3, "filename", TEXTOID, -1, 0);
		TupleDescInitEntry(tmp, 4, "linenum", INT4OID, -1, 0);
		TupleDescInitEntry(tmp, 5, "bytenum", INT4OID, -1, 0);
		TupleDescInitEntry(tmp, 6, "errmsg", TEXTOID, -1, 0);
		TupleDescInitEntry(tmp, 7, "rawdata", TEXTOID, -1, 0);
		TupleDescInitEntry(tmp, 8, "rawbytes", BYTEAOID, -1, 0);

		MemoryContextSwitchTo(oldcontext);

		tupdesc = tmp;
	}

	return tupdesc;
}

static HeapTuple
FormErrorTuple(CdbSreh *cdbsreh)
{
	bool		nulls[NUM_ERRORTABLE_ATTR];
	Datum		values[NUM_ERRORTABLE_ATTR];
	MemoryContext oldcontext;

	oldcontext = MemoryContextSwitchTo(cdbsreh->badrowcontext);

	/* Initialize all values for row to NULL */
	MemSet(values, 0, NUM_ERRORTABLE_ATTR * sizeof(Datum));
	MemSet(nulls, true, NUM_ERRORTABLE_ATTR * sizeof(bool));

	/* command start time */
	values[errtable_cmdtime - 1] = TimestampTzGetDatum(GetCurrentStatementStartTimestamp());
	nulls[errtable_cmdtime - 1] = false;

	/* line number */
	if (cdbsreh->linenumber > 0)
	{
		values[errtable_linenum - 1] = Int64GetDatum(cdbsreh->linenumber);
		nulls[errtable_linenum - 1] = false;
	}

	if (cdbsreh->is_server_enc)
	{
		/* raw data */
		values[errtable_rawdata - 1] = CStringGetTextDatum(cdbsreh->rawdata);
		nulls[errtable_rawdata - 1] = false;
	}
	else
	{
		/* raw bytes */
		PreprocessByteaData(cdbsreh->rawdata);
		values[errtable_rawbytes - 1] = DirectFunctionCall1(byteain, CStringGetDatum(cdbsreh->rawdata));
		nulls[errtable_rawbytes - 1] = false;
	}

	/* file name */
	values[errtable_filename - 1] = CStringGetTextDatum(cdbsreh->filename);
	nulls[errtable_filename - 1] = false;

	/* relation name */
	values[errtable_relname - 1] = CStringGetTextDatum(cdbsreh->relname);
	nulls[errtable_relname - 1] = false;

	/* error message */
	values[errtable_errmsg - 1] = CStringGetTextDatum(cdbsreh->errmsg);
	nulls[errtable_errmsg - 1] = false;


	MemoryContextSwitchTo(oldcontext);

	/*
	 * And now we can form the input tuple.
	 */
	return heap_form_tuple(GetErrorTupleDesc(), values, nulls);
}

/*
 * ReportSrehResults
 *
 * When necessary emit a NOTICE that describes the end result of the
 * SREH operations. Information includes the total number of rejected
 * rows, and whether rows were ignored or logged into an error log file.
 */
void
ReportSrehResults(CdbSreh *cdbsreh, uint64 total_rejected)
{
	if (total_rejected > 0)
	{
		ereport(NOTICE,
				(errmsg("found " INT64_FORMAT " data formatting errors (" INT64_FORMAT " or more input rows), rejected related input data",
						total_rejected, total_rejected)));
	}
}

/*
 * SendNumRows
 *
 * Using this function the QE sends back to the client QD the number
 * of rows that were rejected and completed in this last data load
 */
void
SendNumRows(int64 numrejected, int64 numcompleted)
{
	StringInfoData buf;

	if (Gp_role != GP_ROLE_EXECUTE)
		elog(FATAL, "SendNumRows: called outside of execute context.");

	pq_beginmessage(&buf, 'j'); /* 'j' is the msg code for rejected records */
	pq_sendint64(&buf, numrejected);
	if (numcompleted > 0)		/* optional send completed num for COPY FROM
								 * ON SEGMENT */
		pq_sendint64(&buf, numcompleted);
	pq_endmessage(&buf);
}

/* Identify the reject limit type */
static RejectLimitCode
GetRejectLimitCode(CdbSreh *cdbsreh)
{
	RejectLimitCode code = REJECT_NONE;

	/* special case: check for the case that we are rejecting every single row */
	if (ExceedSegmentRejectHardLimit(cdbsreh))
		return REJECT_FIRST_BAD_LIMIT;

	/* now check if actual reject limit is reached */
	if (cdbsreh->is_limit_in_rows)
	{
		/* limit is in ROWS */
		if (cdbsreh->rejectcount >= cdbsreh->rejectlimit)
			code = REJECT_LIMIT_REACHED;
	}
	else
	{
		/* limit is in PERCENT */

		/* calculate the percent only if threshold is satisfied */
		if (cdbsreh->processed > gp_reject_percent_threshold)
		{
			if ((cdbsreh->rejectcount * 100) / cdbsreh->processed >= cdbsreh->rejectlimit)
				code = REJECT_LIMIT_REACHED;
		}
	}

	return code;
}

/*
 * Reports error if we reached segment reject limit.
 */
void
ErrorIfRejectLimitReached(CdbSreh *cdbsreh)
{
	RejectLimitCode code;

	code = GetRejectLimitCode(cdbsreh);

	if (code == REJECT_NONE)
		return;

	switch (code)
	{
		case REJECT_FIRST_BAD_LIMIT:
			/* the special "first X rows are bad" case */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_GP_REJECT_LIMIT_REACHED),
					 errmsg("all %d first rows in this segment were rejected",
							gp_initial_bad_row_limit),
					 errdetail("Aborting operation regardless of REJECT LIMIT value, last error was: %s",
							   cdbsreh->errmsg)));
			break;
		case REJECT_LIMIT_REACHED:
			/* the normal case */
			ereport(ERROR,
					(errcode(ERRCODE_T_R_GP_REJECT_LIMIT_REACHED),
					 errmsg("segment reject limit reached, aborting operation"),
					 errdetail("Last error was: %s", cdbsreh->errmsg)));
			break;
		default:
			elog(ERROR, "unknown reject code %d", code);
	}
}

/*
 * Return true if the first bad rows exceed the hard limit.  We assume the
 * input is not well configured or similar case.  Stop the work regardless of
 * SEGMENT REJECT LIMIT value before exhausting disk space.  Setting the GUC
 * value to 0 indicates no hard limit.
 */
bool
ExceedSegmentRejectHardLimit(CdbSreh *cdbsreh)
{
	/* Keep going if the hard limit is "unlimited" */
	if (gp_initial_bad_row_limit == 0)
		return false;

	/* Stop if all the first bad rows reach to the hard limit. */
	if (cdbsreh->processed == gp_initial_bad_row_limit &&
		cdbsreh->rejectcount >= gp_initial_bad_row_limit)
		return true;

	return false;
}

/*
 * IsRejectLimitReached
 *
 * Returns true if seg reject limit reached, false otherwise.
 */
bool
IsRejectLimitReached(CdbSreh *cdbsreh)
{
	return GetRejectLimitCode(cdbsreh) != REJECT_NONE;
}

/*
 * This function is called when we are preparing to insert a bad row that
 * includes an encoding error into the bytea field of the error log file
 * (rawbytes). In rare occasions this bad row may also have an invalid bytea
 * sequence - a backslash not followed by a valid octal sequence - in which
 * case inserting into the error log file will fail. In here we make a pass to
 * detect if there's a risk of failing. If there isn't we just return. If there
 * is we remove the backslash and replace it with a x20 char. Yes, we are
 * actually modifying the user data, but this is a much better opion than
 * failing the entire load. It's also a bad row - a row that will require user
 * intervention anyway in order to reload.
 *
 * reference: MPP-2107
 *
 * NOTE: code is copied from esc_dec_len() in encode.c and slightly modified.
 */
static
void
PreprocessByteaData(char *src)
{
	const char *end = src + strlen(src);

	while (src < end)
	{
		if (src[0] != '\\')
			src++;
		else if (src + 3 < end &&
				 (src[1] >= '0' && src[1] <= '3') &&
				 (src[2] >= '0' && src[2] <= '7') &&
				 (src[3] >= '0' && src[3] <= '7'))
		{
			/*
			 * backslash + valid octal
			 */
			src += 4;
		}
		else if (src + 1 < end &&
				 (src[1] == '\\'))
		{
			/*
			 * two backslashes = backslash
			 */
			src += 2;
		}
		else
		{
			/*
			 * one backslash, not followed by ### valid octal. remove the
			 * backslash and put a x20 in its place.
			 */
			src[0] = ' ';
			src++;
		}
	}

}

/*
 * IsRejectLimitValid
 *
 * verify that the reject limit specified by the user is within the
 * allowed values for ROWS or PERCENT.
 */
void
VerifyRejectLimit(char rejectlimittype, int rejectlimit)
{
	if (rejectlimittype == 'r')
	{
		/* ROWS */
		if (rejectlimit < 2)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("segment reject limit in ROWS must be 2 or larger (got %d)",
							rejectlimit)));
	}
	else
	{
		/* PERCENT */
		Assert(rejectlimittype == 'p');
		if (rejectlimit < 1 || rejectlimit > 100)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("segment reject limit in PERCENT must be between 1 and 100 (got %d)",
							rejectlimit)));
	}

}

/*
 * Write into the error log file.  This opens the file every time,
 * so that we can keep it simple to deal with concurrent write.
 */
static void
ErrorLogWrite(CdbSreh *cdbsreh)
{
	HeapTuple	tuple;
	char		filename[MAXPGPATH];
	FILE	   *fp;
	pg_crc32c	crc;
	int			ret;

	Assert(OidIsValid(cdbsreh->relid));
	ErrorLogFileName(MyDatabaseId, cdbsreh->relid,
					 IS_LOG_ERRORS_PERSISTENTLY(cdbsreh->logerrors),
					 filename/* out */);
	tuple = FormErrorTuple(cdbsreh);

	INIT_CRC32C(crc);
	COMP_CRC32C(crc, tuple->t_data, tuple->t_len);
	FIN_CRC32C(crc);

	LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
	fp = AllocateFile(filename, "a");

	if (!fp && (errno == EMFILE || errno == ENFILE))
		ereport(ERROR,
				(errmsg("could not open \"%s\", too many open files: %m",
						filename)));

	if (!fp && errno == ENOENT)
	{
		char	   *errordir = ErrorLogDir;
		if (IS_LOG_ERRORS_PERSISTENTLY(cdbsreh->logerrors))
			errordir = PersistentErrorLogDir;
		ret = mkdir(errordir, S_IRWXU);
		if (ret == 0)
			fp = AllocateFile(filename, "a");
		else
			ereport(ERROR,
					(errmsg("could not create directory for errorlog \"%s\": %m",
							errordir)));
	}
	if (!fp)
		ereport(ERROR, (errmsg("could not open \"%s\": %m", filename)));

	/*
	 * format: 0-4: length 5-8: crc 9-n: tuple data
	 */
	if (fwrite(&tuple->t_len, 1, sizeof(tuple->t_len), fp) != sizeof(tuple->t_len))
		elog(ERROR, "could not write tuple length: %m");
	if (fwrite(&crc, 1, sizeof(pg_crc32c), fp) != sizeof(pg_crc32c))
		elog(ERROR, "could not write checksum: %m");
	if (fwrite(tuple->t_data, 1, tuple->t_len, fp) != tuple->t_len)
		elog(ERROR, "could not write tuple data: %m");

	FreeFile(fp);
	LWLockRelease(ErrorLogLock);

	heap_freetuple(tuple);
}

/*
 * Read one tuple and checksum value from the error log file pointed by fp.
 * This returns NULL whenever we see unexpected read or EOF.
 */
static HeapTuple
ErrorLogRead(FILE *fp, pg_crc32c *crc)
{
	uint32		t_len;
	HeapTuple	tuple = NULL;

	LWLockAcquire(ErrorLogLock, LW_SHARED);

	do
	{
		if (fread(&t_len, 1, sizeof(uint32), fp) != sizeof(uint32))
			break;

		/*
		 * The tuple is "in-memory" format of HeapTuple.  Allocate the whole
		 * chunk consecutively.
		 */
		tuple = palloc(HEAPTUPLESIZE + t_len);
		tuple->t_len = t_len;
		ItemPointerSetInvalid(&tuple->t_self);
		tuple->t_data = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);

		if (fread(crc, 1, sizeof(pg_crc32c), fp) != sizeof(pg_crc32c))
		{
			tuple = NULL;
			break;
		}

		if (fread(tuple->t_data, 1, tuple->t_len, fp) != tuple->t_len)
		{
			pfree(tuple);
			tuple = NULL;
			break;
		}
	} while (0);

	LWLockRelease(ErrorLogLock);

	return tuple;
}

/*
 * Utility to convert PGresult cell in text to Datum
 */
static Datum
ResultToDatum(PGresult *result, int row, AttrNumber attnum, PGFunction func, bool *isnull)
{
	if (PQgetisnull(result, row, attnum))
	{
		*isnull = true;
		return (Datum) 0;
	}
	else
	{
		*isnull = false;
		return DirectFunctionCall3(func,
								   CStringGetDatum(PQgetvalue(result, row, attnum)),
								   ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
	}
}

/*
 * Utility to read error log and execute CRC check.
 */
static Datum
ReadValidErrorLogDatum(FILE *fp, TupleDesc tupledesc, const char* fname)
{
	HeapTuple		tuple;
	Datum			result;
	pg_crc32c		crc,
					written_crc;

	tuple = ErrorLogRead(fp, &written_crc);

	/*
	 * CRC check.
	 */
	if (HeapTupleIsValid(tuple))
	{
		INIT_CRC32C(crc);
		COMP_CRC32C(crc, tuple->t_data, tuple->t_len);
		FIN_CRC32C(crc);

		if (!EQ_CRC32C(crc, written_crc))
		{
			elog(LOG, "incorrect checksum in error log %s", fname);
			tuple = NULL;
		}
	}

	/*
	 * If we found a valid tuple, return it.  Otherwise, fall through in
	 * the DONE routine.
	 */
	if (HeapTupleIsValid(tuple))
	{
		/*
		 * We need to set typmod for the executor to understand its type
		 * we just blessed.
		 */
		HeapTupleHeaderSetTypMod(tuple->t_data,
								 tupledesc->tdtypmod);

		result = HeapTupleGetDatum(tuple);
		return result;
	}
	return PointerGetDatum(NULL);
}

/*
 * gp_read_error_log
 *
 * Returns set of error log tuples.
 */
Datum
gp_read_error_log(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	ReadErrorLogContext *context;
	Datum		result;

	/*
	 * This function is marked as EXECUTE ON ALL SEGMENTS, so we should not
	 * get here in the dispatcher.
	 */
	Assert(Gp_role != GP_ROLE_DISPATCH);

	/*
	 * First call setup
	 */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		text	   *relname;
		RangeVar   *relrv;
		Oid			relid;
		AclResult	aclresult;

		funcctx = SRF_FIRSTCALL_INIT();

		relname = PG_GETARG_TEXT_P(0);
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		context = palloc0(sizeof(ReadErrorLogContext));
		funcctx->user_fctx = (void *) context;

		funcctx->tuple_desc = BlessTupleDesc(GetErrorTupleDesc());

		/*
		 * Open the error log file.
		 */

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
		relid = RangeVarGetRelid(relrv, NoLock, true);

		/* If the relation has gone, silently return no tuples. */
		if (OidIsValid(relid))
		{
			/* Requires SELECT priv to read error log. */
			aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_SELECT);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_CLASS, relrv->relname);

			ErrorLogNormalFileName(context->filename, MyDatabaseId, relid);
			context->fp = AllocateFile(context->filename, "r");
		}

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (ReadErrorLogContext *) funcctx->user_fctx;

	/*
	 * Read error log, probably on segments.  We don't check Gp_role, however,
	 * in case master also wants to read the file.
	 */
	if (context->fp)
	{
		result = ReadValidErrorLogDatum(
			context->fp, funcctx->tuple_desc, context->filename);
		if (DatumGetPointer(result) != NULL)
			SRF_RETURN_NEXT(funcctx, result);
	}

	/*
	 * Close the file, if we have opened it.
	 */
	if (context->fp != NULL)
	{
		FreeFile(context->fp);
		context->fp = NULL;
	}

	SRF_RETURN_DONE(funcctx);
}

/*
 * Utility to retrieve the error log persistent file from RangeVar.
 */
static bool
RetrievePersistentErrorLogFromRangeVar(RangeVar *relrv, AclMode mode, char *fname /*out*/)
{
	AclResult	aclresult;
	bool		findfile = false;
	Oid			namespaceId;
	char	   *schemaname;
	Oid			relid = InvalidOid;

	relid = RangeVarGetRelid(relrv, NoLock, true);

	if (OidIsValid(relid))
	{
		/* Requires priv on relation to operate error log. */
		aclresult = pg_class_aclcheck(relid, GetUserId(), mode);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_CLASS, relrv->relname);

		ErrorLogFileName(MyDatabaseId, relid, true, fname /* out */);
		return true;
	}

	if (relrv->schemaname)
	{
		/* use exact schema given */
		namespaceId = LookupExplicitNamespace(relrv->schemaname, true);
		if (OidIsValid(namespaceId))
		{
			schemaname = relrv->schemaname;
			findfile = true;
			ErrorLogPersistentFileName(fname, MyDatabaseId, namespaceId, relrv->relname);
		}
	}
	else
	{
		ListCell			   *cell;
		OverrideSearchPath	   *searchPath;
		char					filename[MAXPGPATH];

		searchPath = GetOverrideSearchPath(CurrentMemoryContext);
		foreach(cell, searchPath->schemas)
		{
			namespaceId = lfirst_oid(cell);
			ErrorLogPersistentFileName(filename, MyDatabaseId, namespaceId, relrv->relname);
			if (0 == access(filename, R_OK))
			{
				HeapTuple			tuple;
				Form_pg_namespace	pg_namespace_tuple;

				tuple = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(namespaceId));
				if (!HeapTupleIsValid(tuple))
					elog(ERROR, "cache lookup failed for namespace %u", namespaceId);

				pg_namespace_tuple = (Form_pg_namespace) GETSTRUCT(tuple);
				schemaname = pstrdup(NameStr(pg_namespace_tuple->nspname));
				ReleaseSysCache(tuple);
				StrNCpy(fname, filename, MAXPGPATH);
				findfile = true;
				break;
			}
		}
	}
	if (findfile)
	{
		/* Requires priv on namespace to operate error log. */
		aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), mode);
		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_NAMESPACE, schemaname);
		return true;
	}
	return false;
}

/*
 * gp_read_persistent_error_log
 *
 * Returns set of persistent error log tuples.
 */
Datum
gp_read_persistent_error_log(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	ReadErrorLogContext *context;
	Datum		result;

	/*
	 * This function is marked as EXECUTE ON ALL SEGMENTS, so we should not
	 * get here in the dispatcher.
	 */
	Assert(Gp_role != GP_ROLE_DISPATCH);

	/*
	 * First call setup
	 */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		text	   *relname;
		RangeVar   *relrv;
		bool		finderrorlog;

		funcctx = SRF_FIRSTCALL_INIT();

		relname = PG_GETARG_TEXT_P(0);
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		context = palloc0(sizeof(ReadErrorLogContext));
		funcctx->user_fctx = (void *) context;

		funcctx->tuple_desc = BlessTupleDesc(GetErrorTupleDesc());

		/*
		 * Open the error log file.
		 */

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));

		/* Requires SELECT priv to read error log. */
		finderrorlog = RetrievePersistentErrorLogFromRangeVar(
			relrv, ACL_SELECT, context->filename /* out */);

		if (finderrorlog)
		{
			context->fp = AllocateFile(context->filename, "r");
		}

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (ReadErrorLogContext *) funcctx->user_fctx;

	/*
	 * Read error log, probably on segments.  We don't check Gp_role, however,
	 * in case master also wants to read the file.
	 */
	if (context->fp)
	{
		result = ReadValidErrorLogDatum(
			context->fp, funcctx->tuple_desc, context->filename);
		if (DatumGetPointer(result) != NULL)
			SRF_RETURN_NEXT(funcctx, result);
	}

	/*
	 * Close the file, if we have opened it.
	 */
	if (context->fp != NULL)
	{
		FreeFile(context->fp);
		context->fp = NULL;
	}

	SRF_RETURN_DONE(funcctx);
}

static bool
ErrorLogPrefixDelete(Oid databaseId, Oid namespaceId, bool persistent)
{
	char		filename[MAXPGPATH];
	DIR		   *dir;
	struct dirent *de;
	char	   *dirpath;
	char		prefix[MAXPGPATH];
	int			len;

	if (persistent)
		dirpath = PersistentErrorLogDir;
	else
		dirpath = ErrorLogDir;

	if (OidIsValid(databaseId))
	{
		if (OidIsValid(namespaceId))
			snprintf(prefix, sizeof(prefix), "%u_%u_", databaseId, namespaceId);
		else
			snprintf(prefix, sizeof(prefix), "%u_", databaseId);
	}

	dir = AllocateDir(dirpath);

	/*
	 * If we cannot open the directory, most likely it does not exist. Do
	 * nothing.
	 */
	if (dir == NULL)
		return false;

	while ((de = ReadDir(dir, dirpath)) != NULL)
	{
		if (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0 ||
			strcmp(de->d_name, "/") == 0)
			continue;

		/*
		 * If database id is not given, delete all files.
		 * Or
		 * Filter by the prefix for persistent error log.
		 */
		if (!OidIsValid(databaseId) ||
			(strncmp(de->d_name, prefix, strlen(prefix)) == 0 && persistent))
		{
			len = snprintf(filename, MAXPGPATH, "%s/%s", dirpath, de->d_name);
			if (len >= (MAXPGPATH - 1))
			{
				ereport(WARNING,
						(errcode(ERRCODE_INTERNAL_ERROR),
							(errmsg("log filename truncation on \"%s\", unable to delete error log",
									de->d_name))));
				continue;
			}
			LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
			unlink(filename);
			LWLockRelease(ErrorLogLock);
			continue;
		}

		/*
		 * Filter by the database id prefix.
		 */
		if (strncmp(de->d_name, prefix, strlen(prefix)) == 0)
		{
			int			res;
			Oid			dummyDbId,
						dummyOid;

			res = sscanf(de->d_name, "%u_%u", &dummyDbId, &dummyOid);
			Assert(dummyDbId == databaseId);

			/*
			 * Recursively delete the file.
			 */
			if (res == 2)
			{
				ErrorLogDelete(databaseId, dummyOid);
			}
		}
	}

	FreeDir(dir);
	return true;
}


/*
 * PersistentErrorLogDelete - Delete the persistent error log.
 *
 * Only the superuser has the permission to delete.
 */
bool
PersistentErrorLogDelete(Oid databaseId, Oid namespaceId, const char *fname)
{
	bool		result = true;
	if (fname == NULL)
		return ErrorLogPrefixDelete(databaseId, namespaceId, true);

	LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
	if (unlink(fname) < 0)
		result = false;
	LWLockRelease(ErrorLogLock);
	return result;
}

/*
 * Delete the error log of the relation and return true if any.
 * If relationId is InvalidOid, scan the directory to look for
 * all the files prefixed with the databaseId, and delete them.
 */
bool
ErrorLogDelete(Oid databaseId, Oid relationId)
{
	char		filename[MAXPGPATH];
	bool		result = true;

	if (!OidIsValid(relationId))
		return ErrorLogPrefixDelete(databaseId, InvalidOid, false);

	LWLockAcquire(ErrorLogLock, LW_EXCLUSIVE);
	ErrorLogNormalFileName(filename, databaseId, relationId);
	if (unlink(filename) < 0)
		result = false;
	LWLockRelease(ErrorLogLock);

	return result;
}

static bool
TruncateErrorLog(text *relname, bool persistent)
{
	char	   *relname_str;
	RangeVar   *relrv;
	Oid			relid;
	bool		allResults = true;

	relname_str = text_to_cstring(relname);
	if (strcmp(relname_str, "*.*") == 0)
	{
		/*
		 * Only superuser is allowed to delete log files across database.
		 */
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 (errmsg("must be superuser to delete all error log files"))));

		ErrorLogPrefixDelete(InvalidOid, InvalidOid, persistent);
	}
	else if (strcmp(relname_str, "*") == 0)
	{
		/*
		 * Database owner can delete error log files.
		 */
		if (!pg_database_ownercheck(MyDatabaseId, GetUserId()))
			aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_DATABASE,
						   get_database_name(MyDatabaseId));

		ErrorLogPrefixDelete(MyDatabaseId, InvalidOid, persistent);
	}
	else
	{
		AclResult	aclresult;

		relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
		if (persistent)
		{
			char filename[MAXPGPATH];
			bool finderrorlog;

			/* Allow the table owner to truncate error log.
			 * But if the table is already dropped, allow the
			 * schema owner to truncate error log. */
			finderrorlog = RetrievePersistentErrorLogFromRangeVar(
				relrv, ACL_TRUNCATE, filename /* out */);

			/* We don't care if this fails or not. */
			if (finderrorlog)
				PersistentErrorLogDelete(MyDatabaseId, InvalidOid, filename);
		}
		else
		{
			relid = RangeVarGetRelid(relrv, NoLock, true);

			/* Return false if the relation does not exist. */
			if (!OidIsValid(relid))
				PG_RETURN_BOOL(false);

			/*
			* Allow only the table owner to truncate error log.
			*/
			aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_TRUNCATE);
			if (aclresult != ACLCHECK_OK)
				aclcheck_error(aclresult, ACL_KIND_CLASS, relrv->relname);

			/* We don't care if this fails or not. */
			ErrorLogDelete(MyDatabaseId, relid);
		}
	}

	/*
	 * Dispatch the work to segments.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		int			i = 0;
		char	   *sql;
		CdbPgResults cdb_pgresults = {NULL, 0};

		if (persistent)
			sql = psprintf("SELECT pg_catalog.gp_truncate_persistent_error_log(%s)",
					   quote_literal_cstr(text_to_cstring(relname)));
		else
			sql = psprintf("SELECT pg_catalog.gp_truncate_error_log(%s)",
						quote_literal_cstr(text_to_cstring(relname)));

		CdbDispatchCommand(sql, DF_WITH_SNAPSHOT, &cdb_pgresults);

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			Datum		value;
			bool		isnull;
			struct pg_result *pgresult = cdb_pgresults.pg_results[i];

			if (PQresultStatus(pgresult) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				ereport(ERROR,
						(errmsg("unexpected result from segment: %d",
								PQresultStatus(pgresult))));
			}
			value = ResultToDatum(pgresult, 0, 0, boolin, &isnull);
			allResults &= (!isnull && DatumGetBool(value));
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);
		pfree(sql);
	}

	/* Return true iif all segments return true. */
	return allResults;
}

/*
 * Delete error log of the specified relation.  This returns true from master
 * iif all segments and master find the relation.
 */
Datum
gp_truncate_error_log(PG_FUNCTION_ARGS)
{
	text	   *relname;

	relname = PG_GETARG_TEXT_P(0);

	PG_RETURN_BOOL(TruncateErrorLog(relname, false));
}

/*
 * Delete persistent error log of the specified relation.
 * This returns true from master iif all segments and
 * master find the relation.
 */
Datum
gp_truncate_persistent_error_log(PG_FUNCTION_ARGS)
{
	text	   *relname;

	relname = PG_GETARG_TEXT_P(0);

	PG_RETURN_BOOL(TruncateErrorLog(relname, true));
}
