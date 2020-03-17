#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "libpq-fe.h"
#include "catalog/namespace.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "utils/bytea.h"

typedef struct ReadPersistentErrorLogContext
{
	FILE	   *fp;					/* file pointer to the error log */
	char		filename[MAXPGPATH];/* filename of fp */
	int			numTuples;			/* number of total tuples when dispatch */
	PGresult  **segResults;			/* dispatch results */
	int			numSegResults;		/* number of segResults */
	int			currentResult;		/* current index in segResults to read */
	int			currentRow;			/* current row in current result */
} ReadPersistentErrorLogContext;

extern TupleDesc GetErrorTupleDesc(void);
extern Datum ReadValidErrorLogDatum(FILE *fp, TupleDesc tupledesc, const char* fname);
extern bool RetrievePersistentErrorLogFromRangeVar(RangeVar *relrv, AclMode mode, char *fname /*out*/);
extern bool TruncateErrorLog(text *relname, bool persistent);

/* Do the module magic dance */
PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(gp_read_persistent_error_log);
PG_FUNCTION_INFO_V1(gp_truncate_persistent_error_log);

Datum gp_read_persistent_error_log(PG_FUNCTION_ARGS);
Datum gp_truncate_persistent_error_log(PG_FUNCTION_ARGS);

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
 * gp_read_persistent_error_log
 *
 * Returns set of error log tuples.
 */
Datum
gp_read_persistent_error_log(PG_FUNCTION_ARGS)
{
	FuncCallContext	   *funcctx;
	ReadPersistentErrorLogContext *context;
	HeapTuple			tuple;
	Datum				result;

	/*
	 * First call setup
	 */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;
		text	   *relname;

		funcctx = SRF_FIRSTCALL_INIT();

		relname = PG_GETARG_TEXT_P(0);
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		context = palloc0(sizeof(ReadPersistentErrorLogContext));
		funcctx->user_fctx = (void *) context;

		funcctx->tuple_desc = BlessTupleDesc(GetErrorTupleDesc());

		/*
		 * Though this function is usually executed on segment, we dispatch
		 * the execution if it happens to be on QD, and combine the results
		 * into one set.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			struct CdbPgResults cdb_pgresults = {NULL, 0};
			StringInfoData sql;

			int		i;

			initStringInfo(&sql);
			/*
			 * construct SQL
			 */
			appendStringInfo(&sql,
					"SELECT * FROM gp_read_persistent_error_log(%s) ",
							 quote_literal_internal(text_to_cstring(relname)));

			CdbDispatchCommand(sql.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

			for (i = 0; i < cdb_pgresults.numResults; i++)
			{
				if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
				{
					cdbdisp_clearCdbPgResults(&cdb_pgresults);
					elog(ERROR, "unexpected result from segment: %d",
								PQresultStatus(cdb_pgresults.pg_results[i]));
				}
				context->numTuples += PQntuples(cdb_pgresults.pg_results[i]);
			}

			pfree(sql.data);

			context->segResults = cdb_pgresults.pg_results;
			context->numSegResults = cdb_pgresults.numResults;
		}
		else
		{
			/*
			 * In QE, read the error log.
			 */
			RangeVar	   *relrv;
			bool			finderrorlog;

			relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));

			/* Requires SELECT priv to read error log. */
			finderrorlog = RetrievePersistentErrorLogFromRangeVar(
				relrv, ACL_SELECT, context->filename /* out */);

			if (finderrorlog)
			{
				context->fp = AllocateFile(context->filename, "r");
			}
		}

		MemoryContextSwitchTo(oldcontext);

		if (Gp_role != GP_ROLE_DISPATCH && !context->fp)
		{
			pfree(context);
			SRF_RETURN_DONE(funcctx);
		}
	}

	funcctx = SRF_PERCALL_SETUP();
	context = (ReadPersistentErrorLogContext *) funcctx->user_fctx;

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
	 * If we got results from dispatch, return all the tuples.
	 */
	while (context->currentResult < context->numSegResults)
	{
		Datum		values[NUM_ERRORTABLE_ATTR];
		bool		isnull[NUM_ERRORTABLE_ATTR];
		PGresult   *segres = context->segResults[context->currentResult];
		int			row = context->currentRow;

		if (row >= PQntuples(segres))
		{
			context->currentRow = 0;
			context->currentResult++;
			continue;
		}
		context->currentRow++;

		MemSet(isnull, false, sizeof(isnull));

		values[0] = ResultToDatum(segres, row, 0, timestamptz_in, &isnull[0]);
		values[1] = ResultToDatum(segres, row, 1, textin, &isnull[1]);
		values[2] = ResultToDatum(segres, row, 2, textin, &isnull[2]);
		values[3] = ResultToDatum(segres, row, 3, int4in, &isnull[3]);
		values[4] = ResultToDatum(segres, row, 4, int4in, &isnull[4]);
		values[5] = ResultToDatum(segres, row, 5, textin, &isnull[5]);
		values[6] = ResultToDatum(segres, row, 6, textin, &isnull[6]);
		values[7] = ResultToDatum(segres, row, 7, byteain, &isnull[7]);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, isnull);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}

	if (context->segResults != NULL)
	{
		int		i;

		for (i = 0; i < context->numSegResults; i++)
			PQclear(context->segResults[i]);

		/* XXX: better to copy to palloc'ed area */
		free(context->segResults);
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