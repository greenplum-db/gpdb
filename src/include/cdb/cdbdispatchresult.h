/*-------------------------------------------------------------------------
 *
 * cdbdispatchresult.h
 * routines for dispatching commands from the dispatcher process
 * to the qExec processes.
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISPATCHRESULT_H
#define CDBDISPATCHRESULT_H

#include "cdb/cdbdisp.h"
#include "commands/tablecmds.h"
#include "utils/hsearch.h"

struct pg_result;                   /* PGresult ... #include "gp-libpq-fe.h" */
struct SegmentDatabaseDescriptor;   /* #include "cdb/cdbconn.h" */
struct StringInfoData;              /* #include "lib/stringinfo.h" */
struct PQExpBufferData;             /* #include "libpq-int.h" */

/*
 * CdbDispatchResult:
 * Struct for holding the result information
 * for a command dispatched by CdbCommandDispatch to
 * a single segdb.
 */
typedef struct CdbDispatchResult
{
    struct SegmentDatabaseDescriptor *segdbDesc;
                                        /* libpq connection to QE process;
                                         * reset to NULL after end of thread
                                         */

    struct CdbDispatchResults *meleeResults; /* owner of this CdbDispatchResult */
    int                 meleeIndex;     /* index of this entry within
                                         * results->resultArray
                                         */

    int                 errcode;        /* ERRCODE_xxx (sqlstate encoded as
                                         * an int) of first error, or 0.
                                         */
    int                 errindex;       /* index of first entry in resultbuf
                                         * that represents an error; or -1.
                                         * Pass to cdbconn_getResult().
                                         */
    int                 okindex;        /* index of last entry in resultbuf
                                         * with resultStatus == PGRES_TUPLES_OK
                                         * or PGRES_COMMAND_OK (command ended
                                         * successfully); or -1.
                                         * Pass to cdbconn_getResult().
                                         */
    struct PQExpBufferData *resultbuf;      /* -> array of ptr to PGresult */
    struct PQExpBufferData *error_message;  /* -> string of messages; or NULL */

    bool                hasDispatched;  /* true => PQsendCommand done */
    bool                stillRunning;   /* true => busy in dispatch thread */
    DispatchWaitMode    sentSignal;     /* type of signal sent */
    bool                wasCanceled;    /* true => got any of these errors:
                                         *  ERRCODE_GP_OPERATION_CANCELED
                                         *  ERRCODE_QUERY_CANCELED
                                         */

	bool						QEIsPrimary;
	bool						QEWriter_HaveInfo;
	DistributedTransactionId 	QEWriter_DistributedTransactionId;
	CommandId 					QEWriter_CommandId;
	bool 						QEWriter_Dirty;

	int					numrowsrejected; /* num rows rejected in SREH mode */
	
}   CdbDispatchResult;


/* Create a CdbDispatchResult object, appending it to the
 * resultArray of a given CdbDispatchResults object.
 */
CdbDispatchResult *
cdbdisp_makeResult(struct CdbDispatchResults           *meleeResults,
                   struct SegmentDatabaseDescriptor    *segdbDesc,
                   int                                  sliceIndex);

/* Destroy a CdbDispatchResult object. */
void
cdbdisp_termResult(CdbDispatchResult  *dispatchResult);

/* Reset a CdbDispatchResult object for possible reuse. */
void
cdbdisp_resetResult(CdbDispatchResult  *dispatchResult);

/* Take note of an error.
 * 'errcode' is the ERRCODE_xxx value for setting the client's SQLSTATE.
 * NB: This can be called from a dispatcher thread, so it must not use
 * palloc/pfree or elog/ereport because they are not thread safe.
 */
void
cdbdisp_seterrcode(int                  errcode,        /* ERRCODE_xxx or 0 */
                   int                  resultIndex,    /* -1 if no PGresult */
                   CdbDispatchResult   *dispatchResult);

/* Store a PGresult object ptr in the result buffer.
 * NB: Caller must not PQclear() the PGresult object.
 */
void
cdbdisp_appendResult(CdbDispatchResult *dispatchResult,
                     struct pg_result  *res);

/* Return the i'th PGresult object ptr (if i >= 0), or
 * the n+i'th one (if i < 0), or NULL (if i out of bounds).
 * NB: Caller must not PQclear() the PGresult object.
 */
struct pg_result *
cdbdisp_getPGresult(CdbDispatchResult *dispatchResult, int i);

/* Return the number of PGresult objects in the result buffer. */
int
cdbdisp_numPGresult(CdbDispatchResult  *dispatchResult);

/* Remove all of the PGresult ptrs from a CdbDispatchResult object
 * and place them into an array provided by the caller.  The caller
 * becomes responsible for PQclear()ing them.  Returns the number of
 * PGresult ptrs placed in the array.
 */
int
cdbdisp_snatchPGresults(CdbDispatchResult  *dispatchResult,
                        struct pg_result  **pgresultptrs,
                        int                 maxresults);

/* Display a CdbDispatchResult in the log for debugging.
 * Call only from main thread, during or after cdbdisp_checkDispatchResults.
 */
void
cdbdisp_debugDispatchResult(CdbDispatchResult  *dispatchResult,
                            int                 elevel_error,
                            int                 elevel_success);

/* Format a CdbDispatchResult into a StringInfo buffer provided by caller.
 * If verbose = true, reports all results; else reports at most one error.
 */
void
cdbdisp_dumpDispatchResult(CdbDispatchResult       *dispatchResult,
                           bool                     verbose,
                           struct StringInfoData   *buf);

/*--------------------------------------------------------------------*/

/*
 * CdbDispatchResults_SliceInfo:
 * An entry in a CdbDispatchResults object's slice map.
 * Used to find the CdbDispatchResult objects for a gang
 * of QEs given their slice index.
 */
typedef struct CdbDispatchResults_SliceInfo
{
    int                 resultBegin;
    int                 resultEnd;
} CdbDispatchResults_SliceInfo;

/*--------------------------------------------------------------------*/

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
    CdbDispatchResult  *resultArray;    /* -> array [0..resultCapacity-1] of
                                         *      struct CdbDispatchResult
                                         */
    int                 resultCount;    /* num of assigned slots (num of QEs)
                                         * 0 <= resultCount <= resultCapacity
                                         */
    int                 resultCapacity; /* size of resultArray (total #slots) */

    /*
     * Summary of results
     */
    volatile int        iFirstError;    /* index of the resultArray entry for
                                         * the QE that was first to report an
                                         * error; or -1 if no error.
                                         */
    volatile int        errcode;        /* ERRCODE_xxx (sqlstate encoded as
                                         * an int) of the first error, or 0.
                                         */
    /*
     * Dispatch options
     */
    bool                cancelOnError;  /* true => stop remaining QEs on err */

    /*
     * Map: sliceIndex => resultArray index
	 * -> array [0..sliceCapacity-1] of CdbDispatchResults_SliceInfo;
	 *      or NULL
     */
    CdbDispatchResults_SliceInfo   *sliceMap;

    int                 sliceCapacity;  /* num of slots in sliceMap */

	/* MPP-6253: during dispatch, it is important to check to see that
	 * the writer gang isn't already doing something -- this is an
	 * important, missing sanity check */
	struct Gang *writer_gang;
} CdbDispatchResults;


/* Allocate a CdbDispatchResults object in the current memory context. */
CdbDispatchResults *
cdbdisp_makeDispatchResults(int     resultCapacity,
                            int     sliceCapacity,
                            bool    cancelOnError);

/* Clean up and free a CdbDispatchResults object. */
void
cdbdisp_destroyDispatchResults(CdbDispatchResults  *results);

/* Format a CdbDispatchResults object.
 * Appends error messages to caller's StringInfo buffer.
 * Returns ERRCODE_xxx if some error was found, or 0 if no errors.
 * Before calling this function, you must call CdbCheckDispatchResult().
 */
int
cdbdisp_dumpDispatchResults(struct CdbDispatchResults  *gangResults,
                            struct StringInfoData      *buffer,
                            bool                        verbose);

/* Return sum of the cmdTuples values from CdbDispatchResult
 * entries that have a successful PGresult.  If sliceIndex >= 0,
 * uses only the entries belonging to the specified slice.
 */
int64
cdbdisp_sumCmdTuples(CdbDispatchResults *results, int sliceIndex);

HTAB *
cdbdisp_sumAoPartTupCount(PartitionNode *parts, CdbDispatchResults *results);
HTAB *process_aotupcounts(PartitionNode *parts, HTAB *ht, 
						  void *aotupcounts,
						  int naotupcounts);

/*
 * If several tuples were eliminated/rejected from the result because of
 * bad data formatting (this is currenly only possible in external tables
 * with single row error handling) - sum up the total rows rejected from
 * all QE's and notify the client.
 */
void
cdbdisp_sumRejectedRows(CdbDispatchResults *results);

/*
 * max of the lastOid values returned from the QEs
 */
Oid
cdbdisp_maxLastOid(CdbDispatchResults *results, int sliceIndex);

/* Return ptr to first resultArray entry for a given sliceIndex. */
CdbDispatchResult *
cdbdisp_resultBegin(CdbDispatchResults *results, int sliceIndex);

/* Return ptr to last+1 resultArray entry for a given sliceIndex. */
CdbDispatchResult *
cdbdisp_resultEnd(CdbDispatchResults *results, int sliceIndex);

/*--------------------------------------------------------------------*/

/* Convert compact error code (ERRCODE_xxx) to 5-char SQLSTATE string,
 * and put it into a 6-char buffer provided by caller.
 */
char *                          /* returns outbuf+5 */
cdbdisp_errcode_to_sqlstate(int errcode, char outbuf[6]);

/* Convert SQLSTATE string to compact error code (ERRCODE_xxx). */
int
cdbdisp_sqlstate_to_errcode(const char *sqlstate);

char *
cdbdisp_relayresults(CdbDispatchResults *pPrimaryResults);

/*--------------------------------------------------------------------*/

#endif   /* CDBDISPATCHRESULT_H */
