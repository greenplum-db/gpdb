/*--------------------------------------------------------------------------
 *
 * gpexterrorhandle.c
 *	  Provides routines for external table's persistent error log
 *
 * Portions Copyright (c) 2020-Present Pivotal Software, Inc.
 *--------------------------------------------------------------------------
 */
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
 * gp_read_persistent_error_log
 *
 * Returns set of error log tuples.
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

