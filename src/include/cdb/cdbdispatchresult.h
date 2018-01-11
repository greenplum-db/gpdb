/*-------------------------------------------------------------------------
 *
 * cdbdispatchresult.h
 * routines for processing dispatch results.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdispatchresult.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISPATCHRESULT_H
#define CDBDISPATCHRESULT_H

#include "cdb/cdbdisp.h"
#include "commands/tablecmds.h"
#include "utils/hsearch.h"

struct pg_result;                   /* PGresult ... #include "libpq-fe.h" */
struct SegmentDatabaseDescriptor;   /* #include "cdb/cdbconn.h" */
struct StringInfoData;              /* #include "lib/stringinfo.h" */
struct PQExpBufferData;             /* #include "libpq-int.h" */

typedef struct CdbPgResults
{
	struct pg_result **pg_results;
	int numResults;
}CdbPgResults;

/*
 * CdbDispatchResults_SliceInfo:
 * An entry in a CdbDispatchResults object's slice map.
 * Used to find the CdbDispatchResult objects for a gang
 * of QEs given their slice index.
 */
typedef struct CdbDispatchResults_SliceInfo
{
    int resultBegin;
    int resultEnd;
} CdbDispatchResults_SliceInfo;

/*
 * CdbDispatchResult:
 * Struct for holding the result information
 * for a command dispatched by CdbCommandDispatch to
 * a single segdb.
 */
typedef struct CdbDispatchResult
{
	/*
	 * libpq connection to QE process;
	 * * reset to NULL after end of thread
	 */
	struct SegmentDatabaseDescriptor *segdbDesc;

	/* owner of this CdbDispatchResult */
	struct CdbDispatchResults *meleeResults;

	/*
	 * index of this entry within
	 * results->resultArray
	 */
	int meleeIndex;

	/*
	 * ERRCODE_xxx (sqlstate encoded as
	 * an int) of first error, or 0.
	 */
	int errcode;

	/*
	 * index of first entry in resultbuf
	 * that represents an error; or -1.
	 * Pass to cdbconn_getResult().
	 */
	int errindex;

	/*
	 * index of last entry in resultbuf
	 * with resultStatus == PGRES_TUPLES_OK
	 * or PGRES_COMMAND_OK (command ended
	 * successfully); or -1.
	 * Pass to cdbconn_getResult().
	 */
	int okindex;        

	/*
	 * array of ptr to PGresult
	 */
	struct PQExpBufferData *resultbuf;

	/* string of messages; or NULL */
	struct PQExpBufferData *error_message;

	/* true => PQsendCommand done */
	bool hasDispatched;

	/* true => busy in dispatch thread */
	bool stillRunning;

	/* type of signal sent */
	DispatchWaitMode sentSignal;

	/*
	 * true => got any of these errors:
	 * ERRCODE_GP_OPERATION_CANCELED
	 * ERRCODE_QUERY_CANCELED
	 */
	bool wasCanceled;

	/* num rows rejected in SREH mode */
	int	numrowsrejected;

	/* num rows completed in COPY FROM ON SEGMENT */
	int	numrowscompleted;
} CdbDispatchResult;

/*
 * CdbDispatchResults:
 * A collection of CdbDispatchResult objects to hold and summarize
 * the results of dispatching a command or plan to one or more Gangs.
 */
typedef struct CdbDispatchResults
{
	/*
	 * Array of CdbDispatchResult objects, one per QE
	 */
	CdbDispatchResult  *resultArray;
	
	/*
	 * num of assigned slots (num of QEs)
	 * 0 <= resultCount <= resultCapacity
	 */
	int resultCount;
	
	/* size of resultArray (total #slots) */
	int resultCapacity;
	
	/*
	 * index of the resultArray entry for
	 * the QE that was first to report an
	 * error; or -1 if no error.
	 */
	volatile int iFirstError;
	
	/* 
	 * ERRCODE_xxx (sqlstate encoded as
	 * an int) of the first error, or 0.
	 */
	volatile int errcode;
	
	/* true => stop remaining QEs on err */
	bool cancelOnError;  
	
	/*
	 * Map: sliceIndex => resultArray index
	 */
	CdbDispatchResults_SliceInfo *sliceMap;
	
	/* num of slots in sliceMap */
	int sliceCapacity;
	
	/*during dispatch, it is important to check to see that
	 * the writer gang isn't already doing something -- this is an
	 * important, missing sanity check */
	struct Gang *writer_gang;
} CdbDispatchResults;


/*
 * Create a CdbDispatchResult object, appending it to the
 * resultArray of a given CdbDispatchResults object.
 */
CdbDispatchResult *
cdbdisp_makeResult(struct CdbDispatchResults *meleeResults,
                   struct SegmentDatabaseDescriptor *segdbDesc,
                   int sliceIndex);

/*
 * Destroy a CdbDispatchResult object.
 */
void
cdbdisp_termResult(CdbDispatchResult *dispatchResult);

/*
 * Reset a CdbDispatchResult object for possible reuse.
 */
void
cdbdisp_resetResult(CdbDispatchResult *dispatchResult);

/*
 * Take note of an error.
 * 'errcode' is the ERRCODE_xxx value for setting the client's SQLSTATE.
 * NB: This can be called from a dispatcher thread, so it must not use
 * palloc/pfree or elog/ereport because they are not thread safe.
 */
void
cdbdisp_seterrcode(int errcode, /* ERRCODE_xxx or 0 */
                   int resultIndex, /* -1 if no PGresult */
                   CdbDispatchResult *dispatchResult);

/*
 * Format a message, printf-style, and append to the error_message buffer.
 * Also write it to stderr if logging is enabled for messages of the
 * given severity level 'elevel' (for example, DEBUG1; or 0 to suppress).
 * 'errcode' is the ERRCODE_xxx value for setting the client's SQLSTATE.
 * NB: This can be called from a dispatcher thread, so it must not use
 * palloc/pfree or elog/ereport because they are not thread safe.
 */
void
cdbdisp_appendMessage(CdbDispatchResult *dispatchResult,
                      int errcode,
                      const char *fmt,
                      ...)
/* This extension allows gcc to check the format string */
__attribute__((format(printf, 3, 4)));

/*
 * Format a message, printf-style, and append to the error_message buffer.
 * Also write it to stderr if logging is enabled for messages of the
 * given severity level 'elevel' (for example, DEBUG1; or 0 to suppress).
 * 'errcode' is the ERRCODE_xxx value for setting the client's SQLSTATE.
 * NB: This can be called from a dispatcher thread, so it must not use
 * palloc/pfree or elog/ereport because they are not thread safe.
 */
void
cdbdisp_appendMessageNonThread(CdbDispatchResult *dispatchResult,
							   int errcode,
							   const char *fmt,
							   ...)
/* This extension allows gcc to check the format string */
__attribute__((format(printf, 3, 4)));


/*
 * Store a PGresult object ptr in the result buffer.
 * NB: Caller must not PQclear() the PGresult object.
 */
void
cdbdisp_appendResult(CdbDispatchResult *dispatchResult,
                     struct pg_result *res);

/*
 * Return the i'th PGresult object ptr (if i >= 0), or
 * the n+i'th one (if i < 0), or NULL (if i out of bounds).
 * NB: Caller must not PQclear() the PGresult object.
 */
struct pg_result *
cdbdisp_getPGresult(CdbDispatchResult *dispatchResult, int i);

/*
 * Return the number of PGresult objects in the result buffer.
 */
int
cdbdisp_numPGresult(CdbDispatchResult *dispatchResult);

/*
 * Display a CdbDispatchResult in the log for debugging.
 * Call only from main thread, during or after cdbdisp_checkDispatchResults.
 */
void
cdbdisp_debugDispatchResult(CdbDispatchResult  *dispatchResult);

/*
 * Format a CdbDispatchResult into a StringInfo buffer provided by caller.
 * It reports at most one error.
 */
void
cdbdisp_dumpDispatchResult(CdbDispatchResult *dispatchResult,
                           struct StringInfoData *buf);

/*
 * Format a CdbDispatchResults object.
 * Appends error messages to caller's StringInfo buffer.
 * Returns ERRCODE_xxx if some error was found, or 0 if no errors.
 * Before calling this function, you must call CdbCheckDispatchResult().
 */
int
cdbdisp_dumpDispatchResults(struct CdbDispatchResults *gangResults,
                            struct StringInfoData *buffer);

/*
 * Return sum of the cmdTuples values from CdbDispatchResult
 * entries that have a successful PGresult.  If sliceIndex >= 0,
 * uses only the entries belonging to the specified slice.
 */
int64
cdbdisp_sumCmdTuples(CdbDispatchResults *results, int sliceIndex);

/*
 * If several tuples were eliminated/rejected from the result because of
 * bad data formatting (this is currenly only possible in external tables
 * with single row error handling) - sum up the total rows rejected from
 * all QE's and notify the client.
 */
void
cdbdisp_sumRejectedRows(CdbDispatchResults *results);

HTAB *
cdbdisp_sumAoPartTupCount(PartitionNode *parts, CdbDispatchResults *results);

/*
 * max of the lastOid values returned from the QEs
 */
Oid
cdbdisp_maxLastOid(CdbDispatchResults *results, int sliceIndex);

/*
 * Return ptr to first resultArray entry for a given sliceIndex.
 */
CdbDispatchResult *
cdbdisp_resultBegin(CdbDispatchResults *results, int sliceIndex);

/*
 * Return ptr to last+1 resultArray entry for a given sliceIndex.
 */
CdbDispatchResult *
cdbdisp_resultEnd(CdbDispatchResults *results, int sliceIndex);

void
cdbdisp_returnResults(CdbDispatchResults *primaryResults, CdbPgResults *cdb_pgresults);

/*
 * used in the interconnect on the dispatcher to avoid error-cleanup deadlocks.
 */
bool
cdbdisp_checkResultsErrcode(struct CdbDispatchResults *meeleResults);

/*
 * cdbdisp_makeDispatchResults:
 * Allocates a CdbDispatchResults object in the current memory context.
 * Will be freed in function cdbdisp_destroyDispatcherState by deleting the
 * memory context.
 */
CdbDispatchResults *
cdbdisp_makeDispatchResults(int sliceCapacity,
							bool cancelOnError);

void
cdbdisp_clearCdbPgResults(CdbPgResults* cdb_pgresults);

extern struct HTAB *
PQprocessAoTupCounts(struct PartitionNode *parts, struct HTAB *ht,
					 void *aotupcounts, int naotupcounts);

#endif   /* CDBDISPATCHRESULT_H */
