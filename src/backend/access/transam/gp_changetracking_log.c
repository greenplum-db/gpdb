/*-------------------------------------------------------------------------
 *
 * gp_changetracking_log.c
 *		Set-returning function to view gp_distributed_log table.
 *
 * IDENTIFICATION
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "cdb/cdbresynchronizechangetracking.h"
#include "cdb/cdbvars.h"                /* Gp_segment */

Datum		gp_changetracking_log(PG_FUNCTION_ARGS);

#define NUM_COLUMNS_IN_FN_OUTPUT (9)

typedef struct CTContext
{
	File	file;			/* changetracking File */
	char	pageBuffer[CHANGETRACKING_BLCKSZ];	/* current page */
	char*	readRecordBuf; 	/* ReadRecord result area */
	char*	readHeaderBuf; 	/* Header result area */
	int		recs_read;
	int		recs_in_page;
	int		block_number;
	int		logRecOff;		/* offset of next record in page */
	int		logPageOff;		/* offset of current page in file */

    HeapTuple heapTuple;
    Datum     tupleValuesToCopy[NUM_COLUMNS_IN_FN_OUTPUT];
    int tupleDataLen;       /* used for asserting that tuple building is correct */
} CTContext;


/* Read another page, if possible */
static bool
readChangeTrackingPage(CTContext *context)
{
	size_t nread = FileRead(context->file, context->pageBuffer, CHANGETRACKING_BLCKSZ);

	if (nread == CHANGETRACKING_BLCKSZ)
	{
		context->logPageOff += CHANGETRACKING_BLCKSZ;
		return true;
	}
	
	if (nread != 0)
		elog(ERROR, "Unexpected partial page read of %d bytes", (int) nread);

	return false;
}

/*
 * Attempt to read a ChangeTracking record into readRecordBuf.
 */
static bool
ReadRecord(CTContext* context)
{
	ChangeTrackingRecord 		*record;
	ChangeTrackingPageHeader	*header;
	char	   *buffer;
	uint32		header_len = sizeof(ChangeTrackingPageHeader);
	uint32		record_len = sizeof(ChangeTrackingRecord);

	if (context->logRecOff <= 0 || context->recs_read == context->recs_in_page)
	{
		/* Need to advance to new page */
		if (!readChangeTrackingPage(context))
			return false;
		
		context->logRecOff = 0;
		context->recs_read = 0;
		
		/* read page header */
		memcpy(context->readHeaderBuf, context->pageBuffer, header_len);
		header = (ChangeTrackingPageHeader *)context->readHeaderBuf;
		
		if(header->blockversion != 1)
			elog(ERROR, "gp_changetracking_log currently supports storage version 1 only");
		
		context->recs_in_page = header->numrecords;		
		context->logRecOff += sizeof(ChangeTrackingPageHeader);
	}
	
	record = (ChangeTrackingRecord *) (context->pageBuffer + context->logRecOff);
	buffer = context->readRecordBuf;
	
	memcpy(buffer, record, record_len);
	record = (ChangeTrackingRecord *) buffer;
	context->logRecOff += record_len;
	context->recs_read++;
	
	return true;
}

/*
 * produce a view of the changtracking file which includes
 * all the buffer pool information needed for resynchronization.
 */
Datum
gp_changetracking_log(PG_FUNCTION_ARGS)
{
	
	FuncCallContext *funcctx;
	CTContext *context;

	bool isFirstCall = SRF_IS_FIRSTCALL();

	if (isFirstCall)
	{
		TupleDesc		tupdesc;
		MemoryContext 	oldcontext;
		int		   		ftype = PG_GETARG_INT32(0);
		
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		/* this had better match gp_distributed_xacts view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(NUM_COLUMNS_IN_FN_OUTPUT, false);
		
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segment_id", INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "dbid", INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "space", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "db", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "rel", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "xlogloc", XLOGLOCOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "blocknum", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "persistent_tid", TIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "persistent_sn", INT8OID, -1, 0); /* "fake" INT8, like in gp_persistent.c/h */
		
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		context = (CTContext *) palloc(sizeof(CTContext));
	

		if (ftype != CTF_LOG_FULL && 
			ftype != CTF_LOG_COMPACT && 
			ftype != CTF_LOG_TRANSIENT)
			elog(ERROR, "invalid log file descriptor specified (%d). "
						"valid values are %d (full), %d (compact), or %d (transient). ", 
						ftype, CTF_LOG_FULL, CTF_LOG_COMPACT, CTF_LOG_TRANSIENT);

		/* make sure log file exists. then open it */
		if(!ChangeTracking_DoesFileExist(ftype))
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("gp_changetracking_log couldn't open %s file on segment %d dbid %d", 
							 ChangeTracking_FtypeToString(ftype), Gp_segment, GpIdentity.dbid)));
				
		context->file = ChangeTracking_OpenFile(ftype);
		FileSeek(context->file, 0, SEEK_SET); 
		Assert(context->file >= 0);
		
		context->block_number = 0;
		context->logPageOff = -CHANGETRACKING_BLCKSZ; /* so 1st increment will give 0 */
		context->logRecOff = 0;
		context->recs_in_page = 0;
		context->recs_read = 0;
		context->readHeaderBuf = (char *) palloc(sizeof(ChangeTrackingPageHeader));
		context->readRecordBuf = (char *) palloc(sizeof(ChangeTrackingRecord));

        /* form the sample tuple and tuple data that will be updated on each function call */
        MemSet(context->tupleValuesToCopy, 0, sizeof(context->tupleValuesToCopy));
        context->tupleValuesToCopy[0] = Int16GetDatum((int16)GpIdentity.segindex);
        context->tupleValuesToCopy[1] = Int16GetDatum((int16)GpIdentity.dbid);
        context->heapTuple = NULL; /* will be initialized lazily */

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (CTContext *) funcctx->user_fctx;

	AssertEquivalent(isFirstCall, context->heapTuple == NULL);

	/*
	 * Get the next record. No more records? we're done.
	 */
	if (ReadRecord(context))
	{
		ChangeTrackingRecord *record = (ChangeTrackingRecord *) context->readRecordBuf;
		
		/*
		 * Form tuple with appropriate data.
		 */

        /* array entries 0 and 1 have been filled in with the gpidentity values already */
		context->tupleValuesToCopy[2] = ObjectIdGetDatum(record->relFileNode.spcNode);
		context->tupleValuesToCopy[3] = ObjectIdGetDatum(record->relFileNode.dbNode);
		context->tupleValuesToCopy[4] = ObjectIdGetDatum(record->relFileNode.relNode);
		context->tupleValuesToCopy[5] = PointerGetDatum(&record->xlogLocation);
		context->tupleValuesToCopy[6] = UInt32GetDatum(record->bufferPoolBlockNum);
		context->tupleValuesToCopy[7] = PointerGetDatum(&record->persistentTid);
		context->tupleValuesToCopy[8] = Int64GetDatum(record->persistentSerialNum);

        /*
         * we reuse the same heap tuple for subsequent calls.  This only works because all of our fields
         *   are of the same length
         */
        if ( context->heapTuple == NULL )
        {
            /* first time through, create a heap tuple */
            bool nulls[NUM_COLUMNS_IN_FN_OUTPUT];
            MemSet(nulls, false, sizeof(nulls));

    		MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

            context->heapTuple = heap_form_tuple(funcctx->tuple_desc, context->tupleValuesToCopy, nulls);
            context->tupleDataLen = heap_compute_data_size(funcctx->tuple_desc, context->tupleValuesToCopy, nulls);
		    MemoryContextSwitchTo(oldcontext);
        }
        else
        {
            /**
             * the calling code will copy our tuple data, so reuse our template one
             */
            HeapTupleHeader td = context->heapTuple->t_data;
			int data_len = heap_compute_data_size(funcctx->tuple_desc, context->tupleValuesToCopy, NULL);
#ifdef USE_ASSERT_CHECKING
            int len =
#endif
                heap_fill_tuple(funcctx->tuple_desc,
                            context->tupleValuesToCopy,
                            NULL /* isnull */,
                            (char *) td + td->t_hoff,
							data_len,
                            &td->t_infomask,
                            NULL /* bits -- NULL is okay because we have no nulls */);

            Assert(len == context->tupleDataLen);
        }

		Datum result = HeapTupleGetDatum(context->heapTuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	pfree(context->readHeaderBuf);
	pfree(context->readRecordBuf);
	FileClose(context->file);
	
	funcctx->user_fctx = NULL;
	
	SRF_RETURN_DONE(funcctx);
}

