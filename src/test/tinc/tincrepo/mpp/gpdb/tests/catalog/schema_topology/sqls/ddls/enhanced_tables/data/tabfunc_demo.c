/*
 * Table Function demo
 */

#include "postgres.h"
#include "funcapi.h"
#include "tablefuncapi.h"
#include "executor/executor.h"
#include "parser/parse_expr.h"

#include "utils/timestamp.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* table function demo */
PG_FUNCTION_INFO_V1(mytransform);
PG_FUNCTION_INFO_V1(mytransform2);
PG_FUNCTION_INFO_V1(sessionize);
PG_FUNCTION_INFO_V1(project);
PG_FUNCTION_INFO_V1(project_describe);
extern Datum mytransform(PG_FUNCTION_ARGS);
extern Datum mytransform2(PG_FUNCTION_ARGS);
extern Datum sessionize(PG_FUNCTION_ARGS);
extern Datum project(PG_FUNCTION_ARGS);
extern Datum project_describe(PG_FUNCTION_ARGS);

Datum
mytransform(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	TupleDesc            out_tupdesc;
	Datum                tup_datum;
	Datum                values[2];
	bool				 nulls[2];
	Datum                out_values[2];
	bool				 out_nulls[2];

	/* 
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() < 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of mytransform");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/*
	 * We expect an input/output of two columns (int, text) for this stupid 
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts			   != 2				||
		in_tupdesc->attrs[0]->atttypid != INT4OID		||
		in_tupdesc->attrs[1]->atttypid != TEXTOID)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function mytransform"),
				 errhint("expected (integer, text) ")));
	}

	/* For output tuple we also check for possibility of dropped columns */
	if (out_tupdesc->natts				 != 2			||
		(out_tupdesc->attrs[0]->atttypid != TEXTOID		&& 
		 !out_tupdesc->attrs[0]->attisdropped)			||
		(out_tupdesc->attrs[1]->atttypid != INT4OID		&&
		 !out_tupdesc->attrs[1]->attisdropped))
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tuple for function mytransform"),
				 errhint("expected (text, integer) ")));
	}


	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do the whole tuple at once.
	 */
	heap_deform_tuple(tuple, in_tupdesc, values, nulls);

	/* 
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	out_values[0] = values[1];
	out_values[1] = values[0];
	out_nulls[0] = nulls[1];
	out_nulls[1] = nulls[0];
	tuple = heap_form_tuple(out_tupdesc, out_values, out_nulls);

	/* 
	 * Final output must always be a Datum, so convert the tuple as required
	 * by the API.
	 */
	tup_datum = HeapTupleGetDatum(tuple);

	/* Extract values from input tuple, build output tuple */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

Datum
mytransform2(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	TupleDesc            out_tupdesc;
	Datum                tup_datum;
	Datum                values[1];
	bool				 nulls[1];
	Datum                out_values[1];
	bool				 out_nulls[1];
	
	/* 
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() < 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of mytransform2");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
	
	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();
	
	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);
	
	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);
	
	/*
	 * We expect an input/output of one column (int) for this another stupid 
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts			   != 1				||
		in_tupdesc->attrs[0]->atttypid != INT4OID)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function mytransform2"),
				 errhint("expected (integer) ")));
	}
	
	/* For output tuple we also check for possibility of dropped columns */
	if (out_tupdesc->natts				 != 1			||
		(out_tupdesc->attrs[0]->atttypid != INT4OID		&&
		 !out_tupdesc->attrs[0]->attisdropped))
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tuple for function mytransform2"),
				 errhint("expected (integer) ")));
	}
	
	
	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do the whole tuple at once.
	 */
	heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	
	/* 
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	/*
        out_values[0] = values[0] + 1;
	out_values[0] = Int32GetDatum(values[0]) + 1;
	*/
	out_values[0] = values[0];
	out_nulls[0] = nulls[0];
	tuple = heap_form_tuple(out_tupdesc, out_values, out_nulls);
	
	/* 
	 * Final output must always be a Datum, so convert the tuple as required
	 * by the API.
	 */
	tup_datum = HeapTupleGetDatum(tuple);
	
	/* Extract values from input tuple, build output tuple */
	SRF_RETURN_NEXT(fctx, tup_datum);
}


typedef struct session_state {
	int			id;
	Timestamp	time;
	int			counter;
} session_state;


Datum
sessionize(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	TupleDesc            out_tupdesc;
	Datum                tup_datum;
	Datum                values[3];
	bool				 nulls[3];
	session_state       *state;
	int                  newId;
	Timestamp            newTime;
	Interval            *threshold;

	/* 
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() != 2 || PG_ARGISNULL(0) || PG_ARGISNULL(1))
		elog(ERROR, "invalid invocation of sessionize");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
	threshold = PG_GETARG_INTERVAL_P(1); 

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;

		fctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(fctx->multi_call_memory_ctx);
		
		state = (session_state*) palloc0(sizeof(session_state));
		fctx->user_fctx = (void*) state;
		state->id = -9999;  /* gross hack: stupid special value for demo */

		MemoryContextSwitchTo(oldcontext);
	}
	fctx = SRF_PERCALL_SETUP();
	state = (session_state*) fctx->user_fctx;

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/*
	 * We expect an input/output of two columns (int, text) for this stupid 
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts			   != 2				||
		in_tupdesc->attrs[0]->atttypid != INT4OID		||
		in_tupdesc->attrs[1]->atttypid != TIMESTAMPOID)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function sessionize"),
				 errhint("expected (integer, timestamp) ")));
	}

	/* For output tuple we also check for possibility of dropped columns */
	if (out_tupdesc->natts				 != 3			||
		(out_tupdesc->attrs[0]->atttypid != INT4OID		&& 
		 !out_tupdesc->attrs[0]->attisdropped)			||
		(out_tupdesc->attrs[1]->atttypid != TIMESTAMPOID &&
		 !out_tupdesc->attrs[1]->attisdropped)			||
		(out_tupdesc->attrs[2]->atttypid != INT4OID     &&
		 !out_tupdesc->attrs[2]->attisdropped))
	{
		ereport(ERROR, 
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tuple for function sessionize"),
				 errhint("expected (integer, timestamp, integer) ")));
	}


	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do the whole tuple at once.
	 */
	heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	newId = DatumGetInt32(values[0]);
	newTime = DatumGetTimestamp(values[1]);

	/* just skip null input */
	if (nulls[0] || nulls[1])
	{
		nulls[2] = true;
	}
	else
	{
		nulls[2] = false;

		/* handle state transition */
		if (newId == state->id)
		{
			Datum d;

			/* Calculate old timestamp + interval */
			d = DirectFunctionCall2(timestamp_pl_interval, 
									TimestampGetDatum(state->time),
									IntervalPGetDatum(threshold));
			
			/* if that is less than new interval then bump counter */
			d = DirectFunctionCall2(timestamp_lt, d, TimestampGetDatum(newTime));
		
			if (DatumGetBool(d))
				state->counter++;
			state->time = newTime;		
		}
		else
		{
			state->id	   = newId;
			state->time	   = newTime;
			state->counter = 1;
		}
	}

	/* 
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	values[2] = Int32GetDatum(state->counter);
	tuple = heap_form_tuple(out_tupdesc, values, nulls);

	/* 
	 * Final output must always be a Datum, so convert the tuple as required
	 * by the API.
	 */
	tup_datum = HeapTupleGetDatum(tuple);

	/* Extract values from input tuple, build output tuple */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

Datum
project(PG_FUNCTION_ARGS)
{
        FuncCallContext         *fctx;
        ReturnSetInfo           *rsi;
        AnyTable                         scan;
        HeapTuple                        tuple;
        TupleDesc                        in_tupdesc;
        TupleDesc                        out_tupdesc;
        Datum                            tup_datum;
        Datum                            values[1];
        bool                             nulls[1];
        int                                      position;

        /*
         * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
         * correctly.
         */
        if (PG_NARGS() != 2 || PG_ARGISNULL(0) || PG_ARGISNULL(1))
                elog(ERROR, "invalid invocation of project");
        scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
        position = PG_GETARG_INT32(1);

        /* Basic set-returning function (SRF) protocol, setup the context */
        if (SRF_IS_FIRSTCALL())
        {
                fctx = SRF_FIRSTCALL_INIT();
        }
        fctx = SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        rsi                     = (ReturnSetInfo *) fcinfo->resultinfo;
        out_tupdesc = rsi->expectedDesc;
        in_tupdesc  = AnyTable_GetTupleDesc(scan);
        tuple       = AnyTable_GetNextTuple(scan);

        /* Based on what the describe callback should have setup */
        Insist(position > 0 && position <= in_tupdesc->natts);
        Insist(out_tupdesc->natts == 1);
        Insist(out_tupdesc->attrs[0]->atttypid == in_tupdesc->attrs[position-1]->atttypid);
        
        /* check for end of scan */
        if (tuple == NULL)
                SRF_RETURN_DONE(fctx);

        /* -----
         * Extract fields from input tuple, there are several possibilities
         * depending on if we want to fetch the rows by name, by number, or extract
         * the full tuple contents.
         *
         *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
         *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
         *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
         *
         * In this case we have chosen to do extract by position 
         */
        values[0] = GetAttributeByNum(tuple->t_data, (AttrNumber) position, &nulls[0]);

        /* Construct the output tuple and convert to a datum */
        tuple = heap_form_tuple(out_tupdesc, values, nulls);
        tup_datum = HeapTupleGetDatum(tuple);

        /* Return the next result */
        SRF_RETURN_NEXT(fctx, tup_datum);
}


/*
 * A more dynamic describe function that produces different results depending
 * on what sort of input it receives.
 */
Datum
project_describe(PG_FUNCTION_ARGS)
{
        FuncExpr                        *fexpr;
        List                            *fargs;
        ListCell                        *lc;
        Oid                                     *argtypes;
        int                                      numargs;
        TableValueExpr          *texpr;
        Query                           *qexpr;
        TupleDesc                        tdesc;
        TupleDesc                        odesc;
        Expr                            *aexpr;
        int                                      avalue;
        Datum                            d;
        int                                      i;

        /* Fetch and validate input */
        if (PG_NARGS() != 1 || PG_ARGISNULL(0))
                elog(ERROR, "invalid invocation of dynamic_describe");  

        fexpr = (FuncExpr*) PG_GETARG_POINTER(0);
        Insist(IsA(fexpr, FuncExpr));   /* Assert we got what we expected */
        
        /*
         * We should know the type information of the arguments of our calling
         * function, but this demonstrates how we could determine that if we
         * didn't already know.
         */
        fargs = fexpr->args;
        numargs = list_length(fargs);
        argtypes = palloc(sizeof(Oid)*numargs);
        i = 0;
        foreach(lc, fargs)
        {
                Node *arg = lfirst(lc);
                argtypes[i++] = exprType(arg);
        }
        
        /* --------
         * Given that we believe we know that this function is tied to exactly
         * one implementation, lets verify that the above types are what we 
         * were expecting:
         *   - two arguments
         *   - first argument "anytable"
         *   - second argument "text"
         * --------
         */
        Insist(numargs == 2);
        Insist(argtypes[0] == ANYTABLEOID);
        Insist(argtypes[1] == INT4OID);

        /* Now get th tuple descriptor for the ANYTABLE we received */
        texpr = (TableValueExpr*) linitial(fargs);
        Insist(IsA(texpr, TableValueExpr));  /* double check that cast */
        
        qexpr = (Query*) texpr->subquery;
        Insist(IsA(qexpr, Query));

        tdesc = ExecCleanTypeFromTL(qexpr->targetList, false);
        
        /* 
         * The intent of this table function is that it returns the Nth column
         * from the input, which requires us to know what N is.  We get N from
         * the second parameter to the table function.
         *
         * Try to evaluate that argument to a constant value.
         */
        {
                bool                     isNull;
                ExprDoneCond     isDone;
                ExprState               *estate;

                aexpr  = (Expr*) lsecond(fargs);
                
                /*
                 * Might be able to relax this a little, the real goal is limiting to
                 * expressions we can safely evaluate without an executor context 
                 */
                if (!IsA(aexpr, Const))
                        elog(ERROR, "unable to resolve type for function"); /* improve this error */

                estate = ExecInitExpr(aexpr, NULL);
                d = ExecEvalExpr(estate, NULL, &isNull, &isDone);

                /* Single scalar values only please */
                if (isDone != ExprSingleResult)
                        elog(ERROR, "unable to resolve type for function"); /* improve this error */

                /* Non null values only */
                if (isNull)
                        elog(ERROR, "unable to resolve type for function"); /* improve this error */

                avalue = DatumGetInt32(d);
        }

        if (avalue < 1 || avalue > tdesc->natts)
                elog(ERROR, "invalid column position %d", avalue);

        /* Build an output tuple a single column based on the column number above */
        odesc = CreateTemplateTupleDesc(1, false);
        TupleDescInitEntry(odesc, 1, 
                                           NameStr(tdesc->attrs[avalue-1]->attname),
                                           tdesc->attrs[avalue-1]->atttypid,
                                           tdesc->attrs[avalue-1]->atttypmod,
                                           0);

        /* Finally return that tupdesc */
        PG_RETURN_POINTER(odesc);
}
