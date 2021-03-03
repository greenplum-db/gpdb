/*-------------------------------------------------------------------------
 *
 * cdbdispatchresult.c
 *	  Functions for handling dispatch results
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbdispatchresult.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq-fe.h"		/* prerequisite for libpq-int.h */
#include "libpq-int.h"		/* PQExpBufferData */

#include "utils/guc.h"			/* log_min_messages */

#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbvars.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbdispatchresult.h"

static int cdbdisp_snatchPGresults(CdbDispatchResult *dispatchResult,
						struct pg_result **pgresultptrs, int maxresults);

static void
noTrailingNewlinePQ(PQExpBuffer buf)
{
	while (buf->len > 0 && buf->data[buf->len - 1] <= ' ' && buf->data[buf->len - 1] > '\0')
		buf->data[--buf->len] = '\0';
}

static void
oneTrailingNewlinePQ(PQExpBuffer buf)
{
	noTrailingNewlinePQ(buf);
	if (buf->len > 0)
		appendPQExpBufferChar(buf, '\n');
}

/*
 * Create a CdbDispatchResult object, appending it to the
 * resultArray of a given CdbDispatchResults object.
 */
CdbDispatchResult *
cdbdisp_makeResult(struct CdbDispatchResults *meleeResults,
				   struct SegmentDatabaseDescriptor *segdbDesc, int sliceIndex)
{
	CdbDispatchResult *dispatchResult;
	int			meleeIndex;

	Assert(meleeResults &&
		   meleeResults->resultArray &&
		   meleeResults->resultCount < meleeResults->resultCapacity);

	/*
	 * Allocate a slot for the new CdbDispatchResult object.
	 */
	meleeIndex = meleeResults->resultCount++;
	dispatchResult = &meleeResults->resultArray[meleeIndex];

	/*
	 * Initialize CdbDispatchResult.
	 */
	dispatchResult->meleeResults = meleeResults;
	dispatchResult->meleeIndex = meleeIndex;
	dispatchResult->segdbDesc = segdbDesc;
	dispatchResult->resultbuf = createPQExpBuffer();
	dispatchResult->error_message = createPQExpBuffer();
	dispatchResult->numrowsrejected = 0;
	dispatchResult->numrowscompleted = 0;

	if (PQExpBufferBroken(dispatchResult->resultbuf) ||
		PQExpBufferBroken(dispatchResult->error_message))
	{
		destroyPQExpBuffer(dispatchResult->resultbuf);
		dispatchResult->resultbuf = NULL;

		destroyPQExpBuffer(dispatchResult->error_message);
		dispatchResult->error_message = NULL;

		/*
		 * caller is responsible for cleanup -- can't elog(ERROR, ...) from
		 * here.
		 */
		return NULL;
	}

	/*
	 * Reset summary indicators.
	 */
	cdbdisp_resetResult(dispatchResult);

	/*
	 * Update slice map entry.
	 */
	if (sliceIndex >= 0 && sliceIndex < meleeResults->sliceCapacity)
	{
		CdbDispatchResults_SliceInfo *si = &meleeResults->sliceMap[sliceIndex];

		if (si->resultBegin == si->resultEnd)
		{
			si->resultBegin = meleeIndex;
			si->resultEnd = meleeIndex + 1;
		}
		else
		{
			if (si->resultBegin > meleeIndex)
				si->resultBegin = meleeIndex;
			if (si->resultEnd <= meleeIndex)
				si->resultEnd = meleeIndex + 1;
		}
	}

	return dispatchResult;
}

/*
 * Destroy a CdbDispatchResult object.
 */
void
cdbdisp_termResult(CdbDispatchResult *dispatchResult)
{
	PQExpBuffer trash;

	dispatchResult->segdbDesc = NULL;

	/*
	 * Free the PGresult objects.
	 */
	cdbdisp_resetResult(dispatchResult);

	/*
	 * Free the error message buffer and result buffer.
	 */
	trash = dispatchResult->resultbuf;
	dispatchResult->resultbuf = NULL;
	destroyPQExpBuffer(trash);

	trash = dispatchResult->error_message;
	dispatchResult->error_message = NULL;
	destroyPQExpBuffer(trash);
}

/*
 * Reset a CdbDispatchResult object for possible reuse.
 */
void
cdbdisp_resetResult(CdbDispatchResult *dispatchResult)
{
	PQExpBuffer buf = dispatchResult->resultbuf;
	PGresult  **begp = (PGresult **) buf->data;
	PGresult  **endp = (PGresult **) (buf->data + buf->len);
	PGresult  **p;

	/*
	 * Free the PGresult objects.
	 */
	for (p = begp; p < endp; ++p)
	{
		Assert(*p != NULL);
		PQclear(*p);
	}

	/*
	 * Reset summary indicators.
	 */
	dispatchResult->errcode = 0;
	dispatchResult->okindex = -1;

	/*
	 * Reset progress indicators.
	 */
	dispatchResult->hasDispatched = false;
	dispatchResult->stillRunning = false;
	dispatchResult->sentSignal = DISPATCH_WAIT_NONE;
	dispatchResult->wasCanceled = false;

	/*
	 * Empty (but don't free) the error message buffer and result buffer.
	 */
	resetPQExpBuffer(dispatchResult->resultbuf);
	resetPQExpBuffer(dispatchResult->error_message);
}

/*
 * Take note of an error.
 * 'errcode' is the ERRCODE_xxx value for setting the client's SQLSTATE.
 */
void
cdbdisp_seterrcode(int errcode, /* ERRCODE_xxx or 0 */
				   int resultIndex, /* -1 if no PGresult */
				   CdbDispatchResult *dispatchResult)
{
	CdbDispatchResults *meleeResults = dispatchResult->meleeResults;

	/*
	 * We must ensure a nonzero errcode.
	 */
	if (!errcode)
		errcode = ERRCODE_INTERNAL_ERROR;

	/*
	 * Was the command canceled?
	 */
	if (errcode == ERRCODE_GP_OPERATION_CANCELED ||
		errcode == ERRCODE_QUERY_CANCELED)
		dispatchResult->wasCanceled = true;

	/*
	 * If this is the first error from this QE, save the error code and the
	 * index of the PGresult buffer entry. We assume the caller has not yet
	 * added the item to the PGresult buffer.
	 */
	if (!dispatchResult->errcode)
	{
		dispatchResult->errcode = errcode;
	}

	if (!meleeResults)
		return;

	/*
	 * Remember which QE reported an error first among the gangs, but keep
	 * quiet about cancellation done at our request.
	 *
	 * Interconnection errors are given lower precedence because often they
	 * are secondary to an earlier and more interesting error.
	 */
	if (errcode == ERRCODE_GP_OPERATION_CANCELED &&
		dispatchResult->sentSignal == DISPATCH_WAIT_CANCEL)
	{
		/* nop */
	}
	else if (meleeResults->errcode == 0 ||
			 (meleeResults->errcode == ERRCODE_GP_INTERCONNECTION_ERROR &&
			  errcode != ERRCODE_GP_INTERCONNECTION_ERROR))
	{
		meleeResults->errcode = errcode;
		meleeResults->iFirstError = dispatchResult->meleeIndex;
	}
}


/*
 * NonThread version of cdbdisp_appendMessage.
 *
 * It's safe to use palloc/pfree or elog/ereport.
 */
void
cdbdisp_appendMessageNonThread(CdbDispatchResult *dispatchResult,
							   int elevel, const char *fmt,...)
{
	va_list		args;
	int			msgoff;

	/*
	 * Remember first error.
	 */
	cdbdisp_seterrcode(ERRCODE_GP_INTERCONNECTION_ERROR, -1, dispatchResult);

	/*
	 * Allocate buffer if first message. Insert newline between previous
	 * message and new one.
	 */
	Assert(dispatchResult->error_message != NULL);
	oneTrailingNewlinePQ(dispatchResult->error_message);

	msgoff = dispatchResult->error_message->len;

	/*
	 * Format the message and append it to the buffer.
	 */
	va_start(args, fmt);
	appendPQExpBufferVA(dispatchResult->error_message, fmt, args);
	va_end(args);

	/*
	 * Display the message on stderr for debugging, if requested. This helps
	 * to clarify the actual timing of threaded events.
	 */
	if (elevel >= log_min_messages)
	{
		oneTrailingNewlinePQ(dispatchResult->error_message);
		elog(LOG, "%s", dispatchResult->error_message->data + msgoff);
	}

	/*
	 * In case the caller wants to hand the buffer to ereport(), follow the
	 * ereport() convention of not ending with a newline.
	 */
	noTrailingNewlinePQ(dispatchResult->error_message);
}

/*
 * Store a PGresult object ptr in the result buffer.
 * NB: Caller must not PQclear() the PGresult object.
 */
void
cdbdisp_appendResult(CdbDispatchResult *dispatchResult, struct pg_result *res)
{
	Assert(dispatchResult && res);

	/*
	 * Attach the QE identification string to the PGresult
	 */
	if (dispatchResult->segdbDesc && dispatchResult->segdbDesc->whoami)
		pqSaveMessageField(res, PG_DIAG_GP_PROCESS_TAG, dispatchResult->segdbDesc->whoami);

	appendBinaryPQExpBuffer(dispatchResult->resultbuf, (char *) &res, sizeof(res));
}

/*
 * Return the i'th PGresult object ptr (if i >= 0), or
 * the n+i'th one (if i < 0), or NULL (if i out of bounds).
 * NB: Caller must not PQclear() the PGresult object.
 */
struct pg_result *
cdbdisp_getPGresult(CdbDispatchResult *dispatchResult, int i)
{
	if (dispatchResult)
	{
		PQExpBuffer buf = dispatchResult->resultbuf;
		PGresult  **begp = (PGresult **) buf->data;
		PGresult  **endp = (PGresult **) (buf->data + buf->len);
		PGresult  **p = (i >= 0) ? &begp[i] : &endp[i];

		if (p >= begp && p < endp)
		{
			Assert(*p != NULL);
			return *p;
		}
	}
	return NULL;
}

/*
 * Return the number of PGresult objects in the result buffer.
 */
int
cdbdisp_numPGresult(CdbDispatchResult *dispatchResult)
{
	return dispatchResult ? dispatchResult->resultbuf->len / sizeof(PGresult *) : 0;
}

/*
 * Display a CdbDispatchResult in the log for debugging.
 * Call only from main thread, during or after cdbdisp_checkDispatchResults.
 */
void
cdbdisp_debugDispatchResult(CdbDispatchResult *dispatchResult)
{
	int			ires;
	int			nres;

	Assert(dispatchResult != NULL);

	/*
	 * PGresult messages
	 */
	nres = cdbdisp_numPGresult(dispatchResult);
	for (ires = 0; ires < nres; ++ires)
	{
		PGresult   *pgresult = cdbdisp_getPGresult(dispatchResult, ires);
		ExecStatusType resultStatus = PQresultStatus(pgresult);
		char	   *whoami = PQresultErrorField(pgresult, PG_DIAG_GP_PROCESS_TAG);

		if (!whoami)
			whoami = "no process id";

		if (resultStatus == PGRES_COMMAND_OK ||
			resultStatus == PGRES_TUPLES_OK ||
			resultStatus == PGRES_COPY_IN ||
			resultStatus == PGRES_COPY_OUT ||
			resultStatus == PGRES_EMPTY_QUERY)
		{
			char	   *cmdStatus = PQcmdStatus(pgresult);

			elog(LOG, "DispatchResult from %s: ok %s (%s)",
				 dispatchResult->segdbDesc->whoami,
				 cmdStatus ? cmdStatus : "(no cmdStatus)", whoami);
		}
		else
		{
			char	   *sqlstate = PQresultErrorField(pgresult, PG_DIAG_SQLSTATE);
			char	   *pri = PQresultErrorField(pgresult, PG_DIAG_MESSAGE_PRIMARY);
			char	   *dtl = PQresultErrorField(pgresult, PG_DIAG_MESSAGE_DETAIL);
			char	   *sourceFile = PQresultErrorField(pgresult, PG_DIAG_SOURCE_FILE);
			char	   *sourceLine = PQresultErrorField(pgresult, PG_DIAG_SOURCE_LINE);
			int			lenpri = (pri == NULL) ? 0 : strlen(pri);

			if (!sqlstate)
				sqlstate = "no SQLSTATE";

			while (lenpri > 0 && pri[lenpri - 1] <= ' ' && pri[lenpri - 1] > '\0')
				lenpri--;

			ereport(LOG,
					(errmsg("DispatchResult from %s: error (%s) %s %.*s (%s)",
							dispatchResult->segdbDesc->whoami,
							sqlstate,
							PQresStatus(resultStatus),
							lenpri,
							pri ? pri : "", whoami),
					 errdetail("(%s:%s) %s",
							   sourceFile ? sourceFile : "unknown file",
							   sourceLine ? sourceLine : "unknown line",
							   dtl ? dtl : "")));
		}
	}

	/*
	 * Error found on our side of the libpq interface?
	 */
	if (dispatchResult->error_message &&
		dispatchResult->error_message->len > 0)
	{
		char		esqlstate[6];

		errcode_to_sqlstate(dispatchResult->errcode, esqlstate);
		elog(LOG, "DispatchResult from %s: connect error (%s) %s",
			 dispatchResult->segdbDesc->whoami,
			 esqlstate, dispatchResult->error_message->data);
	}

	/*
	 * Should have either an error code or an ok result.
	 */
	if (dispatchResult->errcode == 0 && dispatchResult->okindex < 0)
	{
		elog(LOG, "DispatchResult from %s: No ending status.",
			 dispatchResult->segdbDesc->whoami);
	}
}

/*
 * Construct an ErrorData from the dispatch results.
 */
ErrorData *
cdbdisp_dumpDispatchResult(CdbDispatchResult *dispatchResult)
{
	int			ires;
	int			nres;
	ErrorData  *errdata;

	if (!dispatchResult)
		return NULL;

	/*
	 * Format PGresult messages
	 */
	nres = cdbdisp_numPGresult(dispatchResult);
	for (ires = 0; ires < nres; ++ires)
	{
		PGresult   *pgresult = cdbdisp_getPGresult(dispatchResult, ires);

		errdata = cdbdisp_get_PQerror(pgresult);

		if (errdata)
			return errdata;
	}

	/*
	 * Error found on our side of the libpq interface?
	 */
	if (dispatchResult->error_message &&
		dispatchResult->error_message->len > 0)
	{
		if (errstart(ERROR, TEXTDOMAIN))
		{
			errcode(ERRCODE_GP_INTERCONNECTION_ERROR);
			errmsg("%s", dispatchResult->error_message->data);
			errdata = errfinish_and_return(__FILE__, __LINE__, PG_FUNCNAME_MACRO);
		}
		else
			pg_unreachable();

		return errdata;
	}

	return NULL;
}

/*
 * The returned error object is allocated in TopTransactionContext.
 * 
 * Caution: do not use the returned object across transaction boundary.
 * Current usages of this API are such that the returned object is either
 * logged using elog() or rethrown, both within a transaction context, at the
 * time of finishing a dispatched command.  The caution applies to future uses
 * of this function.
 */
ErrorData *
cdbdisp_get_PQerror(PGresult *pgresult)
{
	MemoryContext oldcontext;
	ExecStatusType resultStatus = PQresultStatus(pgresult);

	/*
	 * QE success
	 */
	if (resultStatus == PGRES_COMMAND_OK ||
		resultStatus == PGRES_TUPLES_OK ||
		resultStatus == PGRES_COPY_IN ||
		resultStatus == PGRES_COPY_OUT ||
		resultStatus == PGRES_EMPTY_QUERY)
	{
		return NULL;
	}

	/*
	 * QE error or libpq error
	 */

	/* These will be overwritten below with the values from QE, if the QE sent them. */
	char	   *filename = __FILE__;
	int			lineno = __LINE__;
	const char *funcname = PG_FUNCNAME_MACRO;
	int			qe_errcode = ERRCODE_GP_INTERCONNECTION_ERROR;

	char	   *whoami;
	char	   *fld;

	/*
	 * errstart need a const filename and funcname, make sure they
	 * are at least const in this transaction.
	 */
	oldcontext = MemoryContextSwitchTo(TopTransactionContext);
	fld = PQresultErrorField(pgresult, PG_DIAG_SOURCE_FILE);
	if (fld)
		filename = pstrdup(fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_SOURCE_LINE);
	if (fld)
		lineno = atoi(fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_SOURCE_FUNCTION);
	if (fld)
		funcname = pstrdup(fld);
	MemoryContextSwitchTo(oldcontext);

	/*
	 * We should only get errors with ERROR level or above, if the
	 * command failed. And if a QE disconnected with FATAL, or PANICed,
	 * we don't want to do the same in the QD. So, always an ERROR.
	 */
	if (!errstart(ERROR, TEXTDOMAIN))
		pg_unreachable(); /* unexpected path. */

	fld = PQresultErrorField(pgresult, PG_DIAG_SQLSTATE);
	if (fld)
		qe_errcode = sqlstate_to_errcode(fld);
	errcode(qe_errcode);

	whoami = PQresultErrorField(pgresult, PG_DIAG_GP_PROCESS_TAG);
	fld = PQresultErrorField(pgresult, PG_DIAG_MESSAGE_PRIMARY);
	if (!fld)
		fld = "no primary message received";

	if (whoami)
		errmsg("%s  (%s)", fld, whoami);
	else
		errmsg("%s", fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_MESSAGE_DETAIL);
	if (fld)
		errdetail("%s", fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_MESSAGE_HINT);
	if (fld)
		errhint("%s", fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_CONTEXT);
	if (fld)
		errcontext("%s", fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_SCHEMA_NAME);
	if (fld)
		err_generic_string(PG_DIAG_SCHEMA_NAME, fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_TABLE_NAME);
	if (fld)
		err_generic_string(PG_DIAG_TABLE_NAME, fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_COLUMN_NAME);
	if (fld)
		err_generic_string(PG_DIAG_COLUMN_NAME, fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_DATATYPE_NAME);
	if (fld)
		err_generic_string(PG_DIAG_DATATYPE_NAME, fld);

	fld = PQresultErrorField(pgresult, PG_DIAG_CONSTRAINT_NAME);
	if (fld)
		err_generic_string(PG_DIAG_CONSTRAINT_NAME, fld);

	Assert(TopTransactionContext);

	oldcontext = MemoryContextSwitchTo(TopTransactionContext);
	ErrorData *edata = errfinish_and_return(filename, lineno, funcname);
	MemoryContextSwitchTo(oldcontext);
	return edata;
}

/*
 * Format a CdbDispatchResults object.
 * Returns an ErrorData object in *qeError if some error was found, or NIL if no errors.
 * Before calling this function, you must call CdbCheckDispatchResult().
 */
void
cdbdisp_dumpDispatchResults(struct CdbDispatchResults *meleeResults,
							ErrorData **qeError)
{
	CdbDispatchResult *dispatchResult;

	/*
	 * Quick exit if no error (not counting ERRCODE_GP_OPERATION_CANCELED).
	 */
	if (!meleeResults || !meleeResults->errcode)
	{
		*qeError = NULL;
		return;
	}

	/*
	 * Find the CdbDispatchResult of the first QE that got an error.
	 */
	Assert(meleeResults->iFirstError >= 0 &&
		   meleeResults->iFirstError < meleeResults->resultCount);

	dispatchResult = &meleeResults->resultArray[meleeResults->iFirstError];

	Assert(dispatchResult->meleeResults == meleeResults &&
		   dispatchResult->errcode != 0);

	/*
	 * Format one QE's result.
	 */
	*qeError = cdbdisp_dumpDispatchResult(dispatchResult);
}

/*
 * Return sum of the cmdTuples values from CdbDispatchResult
 * entries that have a successful PGresult. If sliceIndex >= 0,
 * uses only the results belonging to the specified slice.
 */
int64
cdbdisp_sumCmdTuples(CdbDispatchResults *results, int sliceIndex)
{
	CdbDispatchResult *dispatchResult;
	CdbDispatchResult *resultEnd = cdbdisp_resultEnd(results, sliceIndex);
	PGresult   *pgresult;
	int64		sum = 0;

	for (dispatchResult = cdbdisp_resultBegin(results, sliceIndex);
		 dispatchResult < resultEnd; ++dispatchResult)
	{
		pgresult = cdbdisp_getPGresult(dispatchResult, dispatchResult->okindex);
		if (pgresult && !dispatchResult->errcode)
		{
			char	   *cmdTuples = PQcmdTuples(pgresult);

			if (cmdTuples)
				sum += atoll(cmdTuples);
		}
	}
	return sum;
}

/*
 * If several tuples were eliminated/rejected from the result because of
 * bad data formatting (this is currenly only possible in external tables
 * with single row error handling) - sum up the total rows rejected from
 * all QE's and notify the client.
 */
void
cdbdisp_sumRejectedRows(CdbDispatchResults *results)
{
	CdbDispatchResult *dispatchResult;
	CdbDispatchResult *resultEnd = cdbdisp_resultEnd(results, -1);
	PGresult   *pgresult;
	uint64		totalRejected = 0;

	for (dispatchResult = cdbdisp_resultBegin(results, -1);
		 dispatchResult < resultEnd; ++dispatchResult)
	{
		pgresult = cdbdisp_getPGresult(dispatchResult, dispatchResult->okindex);
		if (pgresult && !dispatchResult->errcode)
		{
			/*
			 * add num rows rejected from this QE to the total
			 */
			totalRejected += dispatchResult->numrowsrejected;
		}
	}

	if (totalRejected > 0)
		ReportSrehResults(NULL, totalRejected);

}

/*
 * Return ptr to first resultArray entry for a given sliceIndex.
 */
CdbDispatchResult *
cdbdisp_resultBegin(CdbDispatchResults *results, int sliceIndex)
{
	CdbDispatchResults_SliceInfo *si;

	if (!results)
		return NULL;

	if (sliceIndex < 0)
		return &results->resultArray[0];

	Assert(sliceIndex < results->sliceCapacity);

	si = &results->sliceMap[sliceIndex];

	Assert(si->resultBegin >= 0 &&
		   si->resultBegin <= si->resultEnd &&
		   si->resultEnd <= results->resultCount);

	return &results->resultArray[si->resultBegin];
}

/*
 * Return ptr to last+1 resultArray entry for a given sliceIndex.
 */
CdbDispatchResult *
cdbdisp_resultEnd(CdbDispatchResults *results, int sliceIndex)
{
	CdbDispatchResults_SliceInfo *si;

	if (!results)
		return NULL;

	if (sliceIndex < 0)
		return &results->resultArray[results->resultCount];

	si = &results->sliceMap[sliceIndex];

	return &results->resultArray[si->resultEnd];
}

void
cdbdisp_returnResults(CdbDispatchResults *primaryResults, CdbPgResults *cdb_pgresults)
{
	CdbDispatchResult *dispatchResult;
	int			nslots;
	int			nresults = 0;
	int			i;

	if (!primaryResults || !cdb_pgresults)
		return;

	/*
	 * Allocate result set ptr array. The caller must PQclear() each PGresult
	 * and free() the array.
	 */
	nslots = 0;

	for (i = 0; i < primaryResults->resultCount; ++i)
		nslots += cdbdisp_numPGresult(&primaryResults->resultArray[i]);

	cdb_pgresults->pg_results = (struct pg_result **) palloc0(nslots * sizeof(struct pg_result *));

	/*
	 * Collect results from primary gang.
	 */
	for (i = 0; i < primaryResults->resultCount; ++i)
	{
		dispatchResult = &primaryResults->resultArray[i];

		/*
		 * Take ownership of this QE's PGresult object(s).
		 */
		nresults += cdbdisp_snatchPGresults(dispatchResult,
											cdb_pgresults->pg_results + nresults,
											nslots - nresults);
	}

	Assert(nresults == nslots);

	/* tell the caller how many sets we're returning. */
	cdb_pgresults->numResults = nresults;
}

/*
 * used in the interconnect on the dispatcher to avoid error-cleanup deadlocks.
 */
bool
cdbdisp_checkResultsErrcode(struct CdbDispatchResults *meleeResults)
{
	if (meleeResults == NULL)
	{
		return false;
	}

	if (meleeResults->errcode)
	{
		return true;
	}

	return false;
}

/*
 * cdbdisp_makeDispatchResults:
 * Allocates a CdbDispatchResults object in the current memory context.
 * Will be freed in function cdbdisp_destroyDispatcherState by deleting the
 * memory context.
 */
void
cdbdisp_makeDispatchResults(CdbDispatcherState *ds,
							int sliceCapacity,
							bool cancelOnError)
{
	CdbDispatchResults *results;
	MemoryContext oldContext;
	int	resultCapacity;
	int nbytes;

	Assert(DispatcherContext);
	oldContext = MemoryContextSwitchTo(DispatcherContext);

	resultCapacity = ds->largestGangSize * sliceCapacity;
	nbytes = resultCapacity * sizeof(results->resultArray[0]);

	results = palloc0(sizeof(*results));
	results->resultArray = palloc0(nbytes);
	results->resultCapacity = resultCapacity;
	results->resultCount = 0;
	results->iFirstError = -1;
	results->errcode = 0;
	results->cancelOnError = cancelOnError;

	results->sliceMap = NULL;
	results->sliceCapacity = sliceCapacity;
	if (sliceCapacity > 0)
	{
		nbytes = sliceCapacity * sizeof(results->sliceMap[0]);
		results->sliceMap = palloc0(nbytes);
	}

	MemoryContextSwitchTo(oldContext);

	ds->primaryResults = results;
}

void
cdbdisp_clearCdbPgResults(CdbPgResults *cdb_pgresults)
{
	int			i = 0;

	if (!cdb_pgresults)
		return;

	for (i = 0; i < cdb_pgresults->numResults; i++)
		PQclear(cdb_pgresults->pg_results[i]);

	if (cdb_pgresults->pg_results)
	{
		pfree(cdb_pgresults->pg_results);
		cdb_pgresults->pg_results = NULL;
	}

	cdb_pgresults->numResults = 0;
}

/*
 * Remove all of the PGresult ptrs from a CdbDispatchResult object
 * and place them into an array provided by the caller. The caller
 * becomes responsible for PQclear()ing them. Returns the number of
 * PGresult ptrs placed in the array.
 */
static int
cdbdisp_snatchPGresults(CdbDispatchResult *dispatchResult,
						struct pg_result **pgresultptrs, int maxresults)
{
	PQExpBuffer buf = dispatchResult->resultbuf;
	PGresult  **begp = (PGresult **) buf->data;
	PGresult  **endp = (PGresult **) (buf->data + buf->len);
	PGresult  **p;
	int			nresults = 0;

	/*
	 * Snatch the PGresult objects.
	 */
	for (p = begp; p < endp; ++p)
	{
		Assert(*p != NULL);
		Assert(nresults < maxresults);
		pgresultptrs[nresults++] = *p;
		*p = NULL;
	}

	/*
	 * Empty our PGresult array.
	 */
	resetPQExpBuffer(buf);
	dispatchResult->okindex = -1;

	return nresults;
}
