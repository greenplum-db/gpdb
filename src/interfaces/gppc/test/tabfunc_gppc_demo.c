/*
 * Table Function demo using GPPC-1.1 API
 */
#include <string.h>
#include "gppc.h"

/*
 * The error handler.  We can call GppcReport with INFO when ERROR, since it's not
 * infinite recursion.  For test purpose, set 'x' to message when WARNING.
 */
static void
errorcallback(GppcReportInfo info, void *arg)
{
        GppcText message;
        GppcReportLevel         elevel = GppcGetReportLevel(info);
        const char                   *rptMessage = GppcGetReportMessage(info);
        size_t len = strlen(rptMessage);
        message = GppcAllocText(len);
        memcpy(GppcGetTextPointer(message), rptMessage, len);

        if (elevel == GPPC_WARNING && arg)
                memset(GppcGetTextPointer(arg), 'x', GppcGetTextLength(arg));
        else if (elevel == GPPC_ERROR && arg)
                GppcReport(GPPC_INFO, "inside callback: %s", GppcTextGetCString(arg));
        else if (elevel == GPPC_NOTICE && arg)
                GppcReport(GPPC_INFO, "inside callback message is: %s", rptMessage);
}

GPPC_FUNCTION_INFO(tablefunc_describe);
GppcDatum tablefunc_describe(GPPC_FUNCTION_ARGS);

GppcDatum
tablefunc_describe(GPPC_FUNCTION_ARGS)
{
        GppcText arg = GppcCStringGetText("Error callback in describe function");
        GppcTupleDesc   tdesc;
        GppcInt4                avalue;
        bool                    isnull, iserror;
        GppcTupleDesc   odesc;
        GppcReportCallbackState cbstate;

        /* For a test purpose to make sure it's working in the describe func */
        cbstate = GppcInstallReportCallback(errorcallback, arg);

        /* Fetch and validate input */
        if (GPPC_NARGS() != 1 || GPPC_ARGISNULL(0))
                GppcReport(GPPC_ERROR, "invalid invocation of describe");

        /* Now get the tuple descriptor for the ANYTABLE we received */
        tdesc = GPPC_TF_INPUT_DESC(0, &iserror);

        if (iserror)
                GppcReport(GPPC_ERROR, "cannot build tuple descriptor");

        avalue = GPPC_TF_GETARG_INT4(1, &isnull, &iserror);
        if (iserror)
                GppcReport(GPPC_ERROR, "function is mal-declared");
        if (isnull)
                GppcReport(GPPC_ERROR, "the second argument should not be NULL");

        if (avalue < 1 || avalue > GppcTupleDescNattrs(tdesc))
                GppcReport(GPPC_ERROR, "invalid column position %d", avalue);

        /* Print out the attlen -- just an excuse to use GppcTupleDescAttrLen() */
        GppcReport(GPPC_NOTICE, "attlen is %d", GppcTupleDescAttrLen(tdesc, avalue - 1));

        /* Build an output tuple a single column based on the column number above */
        odesc = GppcCreateTemplateTupleDesc(1);
        GppcTupleDescInitEntry(odesc, 1,
                                                   GppcTupleDescAttrName(tdesc, avalue - 1),
                                                   GppcTupleDescAttrType(tdesc, avalue - 1),
                                                   GppcTupleDescAttrTypmod(tdesc, avalue - 1));

        GppcUninstallReportCallback(cbstate);

        /* Finally return that tupdesc */
        GPPC_RETURN_TUPLEDESC(odesc);
}

GPPC_FUNCTION_INFO(tablefunc_project);
GppcDatum tablefunc_project(GPPC_FUNCTION_ARGS);

GppcDatum
tablefunc_project(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext     fctx;
        GppcAnyTable    scan;
        GppcTupleDesc   out_tupdesc, in_tupdesc;
        GppcHeapTuple   tuple;
        int                             position;
        GppcDatum               values[1];
        bool                    nulls[1];
        GppcDatum               result;

        /*
         * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
         * correctly.
         */
        if (GPPC_NARGS() != 2 || GPPC_ARGISNULL(0) || GPPC_ARGISNULL(1))
                GppcReport(GPPC_ERROR, "invalid invocation of project");
        scan = GPPC_GETARG_ANYTABLE(0);
        position = GPPC_GETARG_INT4(1);

        /* Basic set-returning function (SRF) protocol, setup the context */
        if (GPPC_SRF_IS_FIRSTCALL())
        {
                fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        out_tupdesc = GPPC_SRF_RESULT_DESC();
        in_tupdesc = GppcAnyTableGetTupleDesc(scan);
        tuple = GppcAnyTableGetNextTuple(scan);

        /* Based on what the describe callback should have setup */
        if (position < 1 || position > GppcTupleDescNattrs(in_tupdesc))
                GppcReport(GPPC_ERROR, "invalid column position(%d)", position);
        if (GppcTupleDescNattrs(out_tupdesc) != 1)
                GppcReport(GPPC_ERROR, "invalid column length in out_tupdesc");
        /* if (GppcTupleDescAttrType(out_tupdesc, 0) !=
                        GppcTupleDescAttrType(in_tupdesc, 0))
                GppcReport(GPPC_ERROR, "type mismatch");
        */
        /* check for end of scan */
        if (tuple == NULL)
                GPPC_SRF_RETURN_DONE(fctx);

        /* Construct the output tuple and convert to a datum */
        values[0] = GppcGetAttributeByNum(tuple, position, &nulls[0]);
        result = GppcBuildHeapTupleDatum(out_tupdesc, values, nulls);

        /* Return the next result */
        GPPC_SRF_RETURN_NEXT(fctx, result);
}

GPPC_FUNCTION_INFO(describe_spi);
GppcDatum describe_spi(GPPC_FUNCTION_ARGS);

GppcDatum describe_spi(GPPC_FUNCTION_ARGS)
{
        GppcTupleDesc   tdesc, odesc;
        GppcSPIResult   result;
        bool                    isnull, iserror;
        char               *query;
        char               *colname = NULL;
        GppcDatum               d_colname;

        tdesc = GPPC_TF_INPUT_DESC(0, &iserror);

        if (GppcSPIConnect() < 0)
                GppcReport(GPPC_ERROR, "unable to connect to SPI");

        /* Get query string */
        query = GppcTextGetCString(GPPC_TF_GETARG_TEXT(1, &isnull, &iserror));
        if (isnull || iserror)
                GppcReport(GPPC_ERROR, "invalid invocation of describe_spi");

        result = GppcSPIExec(query, 0);
        if (result->processed > 0)
                colname = GppcSPIGetValue(result, 1, true);
        if (colname == NULL)
                colname = "?column?";
        GppcSPIFinish();

        /* Build tuple desc */
        odesc = GppcCreateTemplateTupleDesc(1);
        GppcTupleDescInitEntry(odesc, 1,
                                                   colname, GppcOidText, -1);

        d_colname = GppcTextGetDatum(GppcCStringGetText(colname));
        /* Pass the query to project */
        GPPC_TF_SET_USERDATA(GppcDatumGetByteaCopy(d_colname));

        GPPC_RETURN_TUPLEDESC(odesc);
}

GPPC_FUNCTION_INFO(project_spi);
GppcDatum project_spi(GPPC_FUNCTION_ARGS);

GppcDatum project_spi(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext     fctx;
        GppcAnyTable            scan;
        GppcTupleDesc           odesc, idesc;
        GppcHeapTuple           tuple;
        char                            colname[255];
        GppcDatum                       values[1];
        bool                            isnull[1];
        GppcDatum                       result;

        scan = GPPC_GETARG_ANYTABLE(0);
        /* Get the user context from the describe function */
        strcpy(colname,
                   GppcTextGetCString(GppcDatumGetText(GppcByteaGetDatum(GPPC_TF_GET_USERDATA()))));

        if (GPPC_SRF_IS_FIRSTCALL())
        {
                fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        odesc = GPPC_SRF_RESULT_DESC();
        idesc = GppcAnyTableGetTupleDesc(scan);
        tuple = GppcAnyTableGetNextTuple(scan);

        if (tuple == NULL)
                GPPC_SRF_RETURN_DONE(fctx);

        values[0] = GppcGetAttributeByNum(tuple, 1, &isnull[0]);
        if (!isnull[0])
                values[0] = GppcTextGetDatum(GppcCStringGetText(
                                strcat(colname, GppcTextGetCString(GppcDatumGetText(values[0])))));

        result = GppcBuildHeapTupleDatum(odesc, values, isnull);
        GPPC_SRF_RETURN_NEXT(fctx, result);
}

typedef struct MySession
{
        GppcReportCallbackState cbstate;
        char                               *message;
} MySession;

static void
tfcallback(GppcReportInfo info, void *arg)
{
        GppcReportLevel         elevel = GppcGetReportLevel(info);

        if (elevel == GPPC_ERROR)
        {
                MySession                          *sess;

                sess = (MySession *) arg;
                GppcReport(GPPC_INFO, "message: %s", sess->message);
                GppcFree(sess);
        }
}

GPPC_FUNCTION_INFO(project_errorcallback);
GppcDatum project_errorcallback(GPPC_FUNCTION_ARGS);

GppcDatum project_errorcallback(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext     fctx;
        GppcAnyTable            scan;
        GppcTupleDesc           odesc, idesc;
        GppcHeapTuple           tuple;
        GppcDatum                  *values;
        bool                       *isnull;
        int                                     i, attnum;
        GppcDatum                       result;

        scan = GPPC_GETARG_ANYTABLE(0);

        if (GPPC_SRF_IS_FIRSTCALL())
        {
                MySession                                  *sess;

                fctx = GPPC_SRF_FIRSTCALL_INIT();
                sess = (MySession *) GppcSRFAlloc(fctx, sizeof(MySession));
                sess->cbstate = GppcInstallReportCallback(tfcallback, sess);
                sess->message = GppcSRFAlloc(fctx, 255);
                strcpy(sess->message, "Hello, world!");

                /* Save session in the SRF context */
                GppcSRFSave(fctx, sess);
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /*
         * Return the input tuple as is, but it seems
         * TableFunction doesn't accept tuples with <idesc>, so copy
         * everything to a new tuple.
         */
        odesc = GPPC_SRF_RESULT_DESC();
        idesc = GppcAnyTableGetTupleDesc(scan);
        tuple = GppcAnyTableGetNextTuple(scan);

        if (tuple == NULL)
        {
                /* End of the input scan */
                MySession                                  *sess;

                sess = (MySession *) GppcSRFRestore(fctx);
                GppcUninstallReportCallback(sess->cbstate);
                GppcFree(sess);
                GPPC_SRF_RETURN_DONE(fctx);
        }

        attnum = GppcTupleDescNattrs(idesc);
        values = GppcAlloc(sizeof(GppcDatum) * attnum);
        isnull = GppcAlloc(sizeof(bool) * attnum);
        for (i = 0; i < attnum; i++)
        {
                values[i] = GppcGetAttributeByNum(tuple, 1, &isnull[i]);
                if (i == 0 && isnull[0])
                        GppcReport(GPPC_ERROR, "first attribute is NULL");
        }

        result = GppcBuildHeapTupleDatum(odesc, values, isnull);
        GPPC_SRF_RETURN_NEXT(fctx, result);
}

GPPC_FUNCTION_INFO(mytransform);
GppcDatum mytransform(GPPC_FUNCTION_ARGS);

GppcDatum
mytransform(GPPC_FUNCTION_ARGS)
{
	GppcFuncCallContext	fctx;
	GppcAnyTable            scan;
	GppcHeapTuple           tuple;
	GppcTupleDesc           in_tupdesc;
	GppcTupleDesc           out_tupdesc;
	GppcDatum               tup_datum;
	bool			nulls[2];
	GppcDatum               out_values[2];
	bool			out_nulls[2];

	/* 
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (GPPC_NARGS() < 1 || GPPC_ARGISNULL(0))
		GppcReport(GPPC_ERROR, "invalid invocation of mytransform");
	scan = GPPC_GETARG_ANYTABLE(0);  /* Should be the first parameter */

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (GPPC_SRF_IS_FIRSTCALL())
	{
		fctx = GPPC_SRF_FIRSTCALL_INIT();
	}
	fctx = GPPC_SRF_PERCALL_SETUP();

	/* Get the next value from the input scan */
	in_tupdesc  = GppcAnyTableGetTupleDesc(scan);
	tuple       = GppcAnyTableGetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		GPPC_SRF_RETURN_DONE(fctx);

	/*
	 * We expect an input of two columns (int, text) for this stupid 
	 * table function, if that is not what we got then complain.
	 */
	if (GppcTupleDescNattrs(in_tupdesc) != 2		||
	    GppcTupleDescAttrType(in_tupdesc, 0) != GppcOidInt4 ||
	    GppcTupleDescAttrType(in_tupdesc, 1) != GppcOidText)
	{
		GppcReport(GPPC_ERROR, "invalid input tuple for function mytransform");
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
        
        /* Build an output tuple with 2 columns */
        out_tupdesc = GppcCreateTemplateTupleDesc(2);

        /* Initialize the 1st column using the info of 2nd column of the input */
        GppcTupleDescInitEntry(out_tupdesc, 1, 
                               GppcTupleDescAttrName(in_tupdesc, 1),
                               GppcTupleDescAttrType(in_tupdesc, 1),
                               GppcTupleDescAttrTypmod(in_tupdesc, 1));
        /* Initialize the 2nd column using the info of 1st column of the input */
        GppcTupleDescInitEntry(out_tupdesc, 2, 
                               GppcTupleDescAttrName(in_tupdesc, 0),
                               GppcTupleDescAttrType(in_tupdesc, 0),
                               GppcTupleDescAttrTypmod(in_tupdesc, 0));
	
	/* For output tuple we check two columns (text, int) */
	if (GppcTupleDescNattrs(out_tupdesc) != 2 		 ||
            GppcTupleDescAttrType(out_tupdesc, 0) != GppcOidText ||
            GppcTupleDescAttrType(out_tupdesc, 1) != GppcOidInt4)
	{
		GppcReport(GPPC_ERROR, "invalid output tuple for function mytransform");
	}
        
        /* 
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	out_values[0] = GppcGetAttributeByNum(tuple, 2, &nulls[0]);
	out_values[1] = GppcGetAttributeByNum(tuple, 1, &nulls[1]);
	out_nulls[0] = nulls[0];
	out_nulls[1] = nulls[1];

	/* 
	 * Final output must always be a GppcDatum, so convert the tuple as required
	 * by the API.
	 */
        /* GppcBuildHeapTupleDatum is the shortcut for heap_form_tuple + HeapTupleGetDatum */
	tup_datum = GppcBuildHeapTupleDatum(out_tupdesc, out_values, out_nulls);

	/* Extract values from input tuple, build output tuple */
	GPPC_SRF_RETURN_NEXT(fctx, tup_datum);
}

GPPC_FUNCTION_INFO(project_describe);
GppcDatum project_describe(GPPC_FUNCTION_ARGS);

/*
 * A more dynamic describe function that produces different results depending
 * on what sort of input it receives.
 */
GppcDatum
project_describe(GPPC_FUNCTION_ARGS)
{
        GppcTupleDesc                    tdesc;
        GppcTupleDesc                    odesc;
        GppcInt4                         avalue;
        bool                             isnull, iserror;

        /* Fetch and validate input, the decribe function only take one internal argument */
        if (GPPC_NARGS() != 1 || GPPC_ARGISNULL(0))
                GppcReport(GPPC_ERROR, "invalid invocation of describe function");

        /* Now get the tuple descriptor for the ANYTABLE we received */
        tdesc = GPPC_TF_INPUT_DESC(0, &iserror);
        if (iserror)
        	GppcReport(GPPC_ERROR, "cannot build tuple descriptor");

        /* 
         * The intent of this table function is that it returns the Nth column
         * from the input, which requires us to know what N is.  We get N from
         * the second parameter to the table function.
         *
         * Try to evaluate that argument to a constant value.
         */
        avalue = GPPC_TF_GETARG_INT4(1, &isnull, &iserror);
        if (isnull)
                GppcReport(GPPC_ERROR, "the 2nd argument must be an integer");
        if (iserror)
                GppcReport(GPPC_ERROR, "function is mal-declared");

        if (avalue < 1 || avalue > GppcTupleDescNattrs(tdesc))
                GppcReport(GPPC_ERROR, "invalid column position %d", avalue);

        /* Build an output tuple a single column based on the column number above */
        odesc = GppcCreateTemplateTupleDesc(1);
        GppcTupleDescInitEntry(odesc, 1,
                               GppcTupleDescAttrName(tdesc, avalue-1),
                               GppcTupleDescAttrType(tdesc, avalue-1),
                               GppcTupleDescAttrTypmod(tdesc, avalue-1));

        /* Finally return that tupdesc */
        GPPC_RETURN_TUPLEDESC(odesc);
}

GPPC_FUNCTION_INFO(project);
GppcDatum project(GPPC_FUNCTION_ARGS); 

GppcDatum
project(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext        	fctx;
        GppcAnyTable                	scan;
        GppcHeapTuple 			tuple;
        GppcTupleDesc                   in_tupdesc; 
        GppcTupleDesc                   out_tupdesc;
        GppcDatum                       tup_datum;
        GppcDatum                       values[1];
        bool                            nulls[1];
        int                             position;

        /*
         * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
         * correctly.
         */
        if (GPPC_NARGS() != 2 || GPPC_ARGISNULL(0) || GPPC_ARGISNULL(1))
                GppcReport(GPPC_ERROR, "invalid invocation of project");
        scan = GPPC_GETARG_ANYTABLE(0);  /* Should be the first parameter */
        position = GPPC_GETARG_INT4(1);

        /* Basic set-returning function (SRF) protocol, setup the context */
        if (GPPC_SRF_IS_FIRSTCALL())
        {
                fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        out_tupdesc = GPPC_SRF_RESULT_DESC();
        in_tupdesc  = GppcAnyTableGetTupleDesc(scan);
        tuple       = GppcAnyTableGetNextTuple(scan);

        /* Based on what the describe callback should have setup */
        if (GppcTupleDescNattrs(out_tupdesc) != 1) 
                GppcReport(GPPC_ERROR, "invalid column length in out_tupdesc");
        if (GppcTupleDescAttrType(out_tupdesc, 0) != GppcTupleDescAttrType(in_tupdesc, position - 1)) 
                GppcReport(GPPC_ERROR, "output tuple type mis-match");
        if (GppcTupleDescAttrTypmod(out_tupdesc, 0) != GppcTupleDescAttrTypmod(in_tupdesc, position - 1))
                GppcReport(GPPC_ERROR, "output tuple typmod mis-match");
        if (position <= 0 || position > GppcTupleDescNattrs(in_tupdesc))
        	GppcReport(GPPC_ERROR, "column position is out of range");

        /* check for end of scan */
        if (tuple == NULL)
                GPPC_SRF_RETURN_DONE(fctx);

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
        values[0] = GppcGetAttributeByNum(tuple, position, &nulls[0]);

        /* Construct the output tuple and convert to a datum */
        tup_datum = GppcBuildHeapTupleDatum(out_tupdesc, values, nulls);

        /* Return the next result */
        GPPC_SRF_RETURN_NEXT(fctx, tup_datum);
}

GPPC_FUNCTION_INFO(userdata_describe);
GppcDatum userdata_describe(GPPC_FUNCTION_ARGS);

GppcDatum
userdata_describe(GPPC_FUNCTION_ARGS)
{
        GppcTupleDesc        tupdesc;
        GppcDatum 	     userdata;
        //size_t           bytes;
        const char      *message = "copied data from describe function.";

        /* Fetch and validate input */
        if (GPPC_NARGS() != 1 || GPPC_ARGISNULL(0))
                GppcReport(GPPC_ERROR, "invalid invocation of userdata_describe");

        /* Build a result tuple descriptor */
        tupdesc = GppcCreateTemplateTupleDesc(1);
        GppcTupleDescInitEntry(tupdesc, 1, "message", GppcOidText, -1);

        /* Prepare user data */
        userdata = GppcTextGetDatum(GppcCStringGetText(message));

        /* Set to send */
        GPPC_TF_SET_USERDATA(GppcDatumGetBytea(userdata));

        GPPC_RETURN_TUPLEDESC(tupdesc);
}

GPPC_FUNCTION_INFO(userdata_project);
GppcDatum userdata_project(GPPC_FUNCTION_ARGS);

GppcDatum
userdata_project(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext  	 fctx;
        GppcAnyTable             scan;
        GppcHeapTuple            tuple;
        GppcTupleDesc            out_tupdesc;
        GppcDatum                tup_datum;
        GppcDatum                values[1];
        bool                     nulls[1];
        GppcDatum		 userdata;
        
        /* 
         * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
         * correctly.
         */
        if (GPPC_NARGS() != 1 || GPPC_ARGISNULL(0))
                GppcReport(GPPC_ERROR, "invalid invocation of userdata_project");
        scan = GPPC_GETARG_ANYTABLE(0);  /* Should be the first parameter */
        if (GPPC_SRF_IS_FIRSTCALL())
        {
                fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        out_tupdesc     = GPPC_SRF_RESULT_DESC();
        tuple           = GppcAnyTableGetNextTuple(scan);
        if (tuple == NULL)
                GPPC_SRF_RETURN_DONE(fctx);

        /* Receive message from describe function */
        userdata = GppcByteaGetDatum(GPPC_TF_GET_USERDATA());
        if (userdata)
        {
                values[0] = userdata;
                nulls[0] = false;
        }
        else
        {
                values[0] = (GppcDatum) 0;
                nulls[0] = true;
        }
        /* Construct the output tuple and convert to a datum */
        tup_datum = GppcBuildHeapTupleDatum(out_tupdesc, values, nulls);

        /* Return the next result */
        GPPC_SRF_RETURN_NEXT(fctx, tup_datum);
}

GPPC_FUNCTION_INFO(userdata_describe2);
GppcDatum userdata_describe2(GPPC_FUNCTION_ARGS);

GppcDatum
userdata_describe2(GPPC_FUNCTION_ARGS)
{
        GppcTupleDesc   tupdesc;
        GppcText        value;
        bool        isnull, iserror;
        GppcDatum   userdata;
        // const char      *message = "copied data from describe function.";

        /* Fetch and validate input */
        if (GPPC_NARGS() != 1 || GPPC_ARGISNULL(0))
                GppcReport(GPPC_ERROR, "invalid invocation of userdata_describe2");

        /* Build a result tuple descriptor */
        tupdesc = GppcCreateTemplateTupleDesc(2);
        GppcTupleDescInitEntry(tupdesc, 1, "position", GppcOidInt4, -1);
        GppcTupleDescInitEntry(tupdesc, 2, "message", GppcOidText, -1);

        value = GPPC_TF_GETARG_TEXT(1, &isnull, &iserror);
        if (isnull)
                GppcReport(GPPC_ERROR, "the 2nd argument must not be null");
        if (iserror)
                GppcReport(GPPC_ERROR, "function is mal-declared");

        userdata = GppcTextGetDatum(value);

        /* Set to send */
        GPPC_TF_SET_USERDATA(GppcDatumGetBytea(userdata));

        GPPC_RETURN_TUPLEDESC(tupdesc);
}

GPPC_FUNCTION_INFO(userdata_project2);
GppcDatum userdata_project2(GPPC_FUNCTION_ARGS);

GppcDatum
userdata_project2(GPPC_FUNCTION_ARGS)
{
        GppcFuncCallContext      fctx;
        GppcAnyTable             scan;
        GppcHeapTuple            tuple;
        GppcTupleDesc            in_tupdesc;
        GppcTupleDesc            out_tupdesc;
        GppcDatum                tup_datum;
        GppcDatum                values[2];
        bool                     nulls[2];
        GppcDatum                posvalue;
        bool                     posisnull;
        GppcDatum		 userdata;

        /*
         * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
         * correctly.
         */
        if (GPPC_NARGS() != 2 || GPPC_ARGISNULL(0) || GPPC_ARGISNULL(1))
                GppcReport(GPPC_ERROR, "invalid invocation of userdata_project2");
        scan = GPPC_GETARG_ANYTABLE(0);  /* Should be the first parameter */
        if (GPPC_SRF_IS_FIRSTCALL())
        {
                fctx = GPPC_SRF_FIRSTCALL_INIT();
        }
        fctx = GPPC_SRF_PERCALL_SETUP();

        /* Get the next value from the input scan */
        out_tupdesc = GPPC_SRF_RESULT_DESC();
        in_tupdesc  = GppcAnyTableGetTupleDesc(scan);
        tuple       = GppcAnyTableGetNextTuple(scan);
        if (GppcTupleDescAttrType(in_tupdesc, 0) != GppcOidInt4)
                GppcReport(GPPC_ERROR, "TABLE query must have int4 type in the first column");
        if (tuple == NULL)
                GPPC_SRF_RETURN_DONE(fctx);

        posvalue = GppcGetAttributeByNum(tuple, 1, &posisnull);
        values[0] = posvalue;
        nulls[0] = posisnull;
        /* Receive file data from describe function */
        userdata = GppcByteaGetDatum(GPPC_TF_GET_USERDATA());

        if (userdata && !posisnull)
        {
                values[1] = userdata;
                nulls[1] = false;
        }
        else
        {
                values[1] = (GppcDatum) 0;
                nulls[1] = true;
        }
        /* Construct the output tuple and convert to a datum */
        tup_datum = GppcBuildHeapTupleDatum(out_tupdesc, values, nulls);

        /* Return the next result */
        GPPC_SRF_RETURN_NEXT(fctx, tup_datum);
}
