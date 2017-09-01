/*-------------------------------------------------------------------------
 *
 * gppc_test.c
 *	  libgppc test program
 *
 * Portions Copyright (c) 2012, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/interfaces/gppc/test/gppc_test/gppc_test.c
 *
 *-------------------------------------------------------------------------
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "gppc.h"

GPPC_FUNCTION_INFO(oidcheckfunc);
GppcDatum oidcheckfunc(GPPC_FUNCTION_ARGS);

GppcDatum oidcheckfunc(GPPC_FUNCTION_ARGS)
{
	char	   *arg = GppcTextGetCString(GPPC_GETARG_TEXT(0));

	/*
	 * We use INT4 as the result type, because  RETURN_OID macros are not prepared.
	 */
	if (strcmp("bool", arg) == 0)
		GPPC_RETURN_INT4(GppcOidBool);
	else if (strcmp("char", arg) == 0)
		GPPC_RETURN_INT4(GppcOidChar);
	else if (strcmp("int2", arg) == 0)
		GPPC_RETURN_INT4(GppcOidInt2);
	else if (strcmp("int4", arg) == 0)
		GPPC_RETURN_INT4(GppcOidInt4);
	else if (strcmp("int8", arg) == 0)
		GPPC_RETURN_INT4(GppcOidInt8);
	else if (strcmp("float4", arg) == 0)
		GPPC_RETURN_INT4(GppcOidFloat4);
	else if (strcmp("float8", arg) == 0)
		GPPC_RETURN_INT4(GppcOidFloat8);
	else if (strcmp("text", arg) == 0)
		GPPC_RETURN_INT4(GppcOidText);
	else if (strcmp("varchar", arg) == 0)
		GPPC_RETURN_INT4(GppcOidVarChar);
	else if (strcmp("bpchar", arg) == 0)
		GPPC_RETURN_INT4(GppcOidBpChar);
	else if (strcmp("bytea", arg) == 0)
		GPPC_RETURN_INT4(GppcOidBytea);
	else if (strcmp("numeric", arg) == 0)
		GPPC_RETURN_INT4(GppcOidNumeric);
	else if (strcmp("date", arg) == 0)
		GPPC_RETURN_INT4(GppcOidDate);
	else if (strcmp("time", arg) == 0)
		GPPC_RETURN_INT4(GppcOidTime);
	else if (strcmp("timetz", arg) == 0)
		GPPC_RETURN_INT4(GppcOidTimeTz);
	else if (strcmp("timestamp", arg) == 0)
		GPPC_RETURN_INT4(GppcOidTimestamp);
	else if (strcmp("timestamptz", arg) == 0)
		GPPC_RETURN_INT4(GppcOidTimestampTz);

	GPPC_RETURN_NULL();
}

GPPC_FUNCTION_INFO(boolfunc);
GppcDatum boolfunc(GPPC_FUNCTION_ARGS);

GppcDatum boolfunc(GPPC_FUNCTION_ARGS)
{
	GppcBool	arg1 = GPPC_GETARG_BOOL(0);

	GPPC_RETURN_BOOL(arg1);
}

GPPC_FUNCTION_INFO(charfunc);
GppcDatum charfunc(GPPC_FUNCTION_ARGS);

GppcDatum charfunc(GPPC_FUNCTION_ARGS)
{
	GppcChar	arg1 = GPPC_GETARG_CHAR(0);

	GPPC_RETURN_CHAR(arg1);
}

GPPC_FUNCTION_INFO(int2mulfunc);
GppcDatum int2mulfunc(GPPC_FUNCTION_ARGS);

GppcDatum int2mulfunc(GPPC_FUNCTION_ARGS)
{
	GppcInt2	arg1 = GPPC_GETARG_INT2(0);
	GppcInt2	arg2 = GPPC_GETARG_INT2(1);

	GPPC_RETURN_INT2(arg1 * arg2);
}

GPPC_FUNCTION_INFO(int4func1);
GppcDatum int4func1(GPPC_FUNCTION_ARGS);

GppcDatum int4func1(GPPC_FUNCTION_ARGS)
{
	int			nargs = GPPC_NARGS();
	GppcInt4	arg1;

	if (nargs < 1)
		arg1 = 0;
	else
		arg1 = GPPC_GETARG_INT4(0);

	GPPC_RETURN_INT4(arg1);
}

GPPC_FUNCTION_INFO(int8plusfunc);
GppcDatum int8plusfunc(GPPC_FUNCTION_ARGS);

GppcDatum int8plusfunc(GPPC_FUNCTION_ARGS)
{
	GppcInt8	arg1 = GPPC_GETARG_INT8(0);
	GppcInt8	arg2 = GPPC_GETARG_INT8(1);

	GPPC_RETURN_INT8(arg1 + arg2);
}

GPPC_FUNCTION_INFO(float4func1);
GppcDatum float4func1(GPPC_FUNCTION_ARGS);

GppcDatum float4func1(GPPC_FUNCTION_ARGS)
{
	GppcFloat4	arg1 = GPPC_GETARG_FLOAT4(0);

	GPPC_RETURN_FLOAT4(arg1 + 1.5);
}

GPPC_FUNCTION_INFO(float8func1);
GppcDatum float8func1(GPPC_FUNCTION_ARGS);

GppcDatum float8func1(GPPC_FUNCTION_ARGS)
{
	GppcFloat8	arg1 = GPPC_GETARG_FLOAT8(0);

	GPPC_RETURN_FLOAT8(arg1 / 2.0);
}

GPPC_FUNCTION_INFO(textdoublefunc);
GppcDatum textdoublefunc(GPPC_FUNCTION_ARGS);

GppcDatum textdoublefunc(GPPC_FUNCTION_ARGS)
{
	GppcText	arg1 = GPPC_GETARG_TEXT(0);
	size_t		clen = GppcGetTextLength(arg1);
	GppcText	result;

	result = GppcAllocText(clen * 2);
	memcpy(GppcGetTextPointer(result), GppcGetTextPointer(arg1), clen);
	memcpy(GppcGetTextPointer(result) + clen, GppcGetTextPointer(arg1), clen);

	GPPC_RETURN_TEXT(result);
}

GPPC_FUNCTION_INFO(textgenfunc);
GppcDatum textgenfunc(GPPC_FUNCTION_ARGS);

GppcDatum textgenfunc(GPPC_FUNCTION_ARGS)
{
	GppcText	result = GppcCStringGetText("cstring result");

	GPPC_RETURN_TEXT(result);
}

GPPC_FUNCTION_INFO(textcopyfunc);
GppcDatum textcopyfunc(GPPC_FUNCTION_ARGS);

GppcDatum textcopyfunc(GPPC_FUNCTION_ARGS)
{
	GppcText	copy = GPPC_GETARG_TEXT_COPY(0);
	GppcText	arg = GPPC_GETARG_TEXT(0);
	GppcBool	needcopy = GPPC_GETARG_BOOL(1);

	*(GppcGetTextPointer(copy)) = '!';

	GPPC_RETURN_TEXT(needcopy ? copy : arg);
}

GPPC_FUNCTION_INFO(varchardoublefunc);
GppcDatum varchardoublefunc(GPPC_FUNCTION_ARGS);

GppcDatum varchardoublefunc(GPPC_FUNCTION_ARGS)
{
	GppcVarChar	arg1 = GPPC_GETARG_VARCHAR(0);
	size_t		clen = GppcGetVarCharLength(arg1);
	GppcVarChar	result;

	result = GppcAllocVarChar(clen * 2);
	memcpy(GppcGetVarCharPointer(result), GppcGetVarCharPointer(arg1), clen);
	memcpy(GppcGetVarCharPointer(result) + clen, GppcGetVarCharPointer(arg1), clen);

	GPPC_RETURN_VARCHAR(result);
}

GPPC_FUNCTION_INFO(varchargenfunc);
GppcDatum varchargenfunc(GPPC_FUNCTION_ARGS);

GppcDatum varchargenfunc(GPPC_FUNCTION_ARGS)
{
	GppcVarChar	result = GppcCStringGetVarChar("cstring result");

	GPPC_RETURN_VARCHAR(result);
}

GPPC_FUNCTION_INFO(varcharcopyfunc);
GppcDatum varcharcopyfunc(GPPC_FUNCTION_ARGS);

GppcDatum varcharcopyfunc(GPPC_FUNCTION_ARGS)
{
	GppcVarChar	copy = GPPC_GETARG_VARCHAR_COPY(0);
	GppcVarChar	arg = GPPC_GETARG_VARCHAR(0);
	GppcBool	needcopy = GPPC_GETARG_BOOL(1);

	*(GppcGetVarCharPointer(copy)) = '!';

	GPPC_RETURN_VARCHAR(needcopy ? copy : arg);
}

GPPC_FUNCTION_INFO(bpchardoublefunc);
GppcDatum bpchardoublefunc(GPPC_FUNCTION_ARGS);

GppcDatum bpchardoublefunc(GPPC_FUNCTION_ARGS)
{
	GppcBpChar	arg1 = GPPC_GETARG_BPCHAR(0);
	size_t		clen = GppcGetBpCharLength(arg1);
	GppcBpChar	result;

	result = GppcAllocBpChar(clen * 2);
	memcpy(GppcGetBpCharPointer(result), GppcGetBpCharPointer(arg1), clen);
	memcpy(GppcGetBpCharPointer(result) + clen, GppcGetBpCharPointer(arg1), clen);

	GPPC_RETURN_BPCHAR(result);
}

GPPC_FUNCTION_INFO(bpchargenfunc);
GppcDatum bpchargenfunc(GPPC_FUNCTION_ARGS);

GppcDatum bpchargenfunc(GPPC_FUNCTION_ARGS)
{
	GppcBpChar	result = GppcCStringGetBpChar("cstring result");

	GPPC_RETURN_BPCHAR(result);
}

GPPC_FUNCTION_INFO(bpcharcopyfunc);
GppcDatum bpcharcopyfunc(GPPC_FUNCTION_ARGS);

GppcDatum bpcharcopyfunc(GPPC_FUNCTION_ARGS)
{
	GppcBpChar	copy = GPPC_GETARG_BPCHAR_COPY(0);
	GppcBpChar	arg = GPPC_GETARG_BPCHAR(0);
	GppcBool	needcopy = GPPC_GETARG_BOOL(1);

	*(GppcGetBpCharPointer(copy)) = '!';

	GPPC_RETURN_BPCHAR(needcopy ? copy : arg);
}

GPPC_FUNCTION_INFO(errfunc1);
GppcDatum errfunc1(GPPC_FUNCTION_ARGS);

GppcDatum errfunc1(GPPC_FUNCTION_ARGS)
{
	GppcText	arg1 = GPPC_GETARG_TEXT(0);
	char	   *cstr;

	cstr = GppcTextGetCString(arg1);
	GppcReport(GPPC_NOTICE, "%s", cstr);

	GPPC_RETURN_NULL();
}

GPPC_FUNCTION_INFO(argisnullfunc);
GppcDatum argisnullfunc(GPPC_FUNCTION_ARGS);

GppcDatum argisnullfunc(GPPC_FUNCTION_ARGS)
{
	if (GPPC_ARGISNULL(0))
		GPPC_RETURN_BOOL(true);
	else
		GPPC_RETURN_BOOL(false);
}

GPPC_FUNCTION_INFO(byteafunc1);
GppcDatum byteafunc1(GPPC_FUNCTION_ARGS);

GppcDatum byteafunc1(GPPC_FUNCTION_ARGS)
{
	GppcBytea		bytea = GPPC_GETARG_BYTEA_COPY(0);
	char		   *mem = GppcGetByteaPointer(bytea);

	mem[0]++;

	GPPC_RETURN_BYTEA(bytea);
}

GPPC_FUNCTION_INFO(numericfunc1);
GppcDatum numericfunc1(GPPC_FUNCTION_ARGS);

GppcDatum numericfunc1(GPPC_FUNCTION_ARGS)
{
	GppcNumeric		numeric = GPPC_GETARG_NUMERIC(0);
	char		   *str;

	str = GppcNumericGetCString(numeric);
	if (strlen(str) > 0 && ('0' <= str[0] && str[0] <= '8'))
		str[0]++;

	GPPC_RETURN_NUMERIC(GppcCStringGetNumeric(str));
}

GPPC_FUNCTION_INFO(numericfunc2);
GppcDatum numericfunc2(GPPC_FUNCTION_ARGS);

GppcDatum numericfunc2(GPPC_FUNCTION_ARGS)
{
	GppcNumeric		numeric = GPPC_GETARG_NUMERIC(0);

	GPPC_RETURN_FLOAT8(GppcNumericGetFloat8(numeric));
}

GPPC_FUNCTION_INFO(numericfunc3);
GppcDatum numericfunc3(GPPC_FUNCTION_ARGS);

GppcDatum numericfunc3(GPPC_FUNCTION_ARGS)
{
	GppcFloat8		flt = GPPC_GETARG_FLOAT8(0);

	GPPC_RETURN_NUMERIC(GppcFloat8GetNumeric(flt));
}

GPPC_FUNCTION_INFO(numericdef1);
GppcDatum numericdef1(GPPC_FUNCTION_ARGS);

GppcDatum numericdef1(GPPC_FUNCTION_ARGS)
{
	GppcInt4		typmod = GPPC_GETARG_INT4(0);
	int16_t			precision, scale;
	char			buf[255];

	if (GppcGetNumericDef(typmod, &precision, &scale))
		snprintf(buf, 255, "NUMERIC(%d, %d)", precision, scale);
	else
		snprintf(buf, 255, "NUMERIC()");

	GPPC_RETURN_TEXT(GppcCStringGetText(buf));
}

GPPC_FUNCTION_INFO(datefunc1);
GppcDatum datefunc1(GPPC_FUNCTION_ARGS);

GppcDatum datefunc1(GPPC_FUNCTION_ARGS)
{
	GppcDate		arg = GPPC_GETARG_DATE(0);
	GppcTm			tm;

	GppcDateGetTm(arg, &tm);
	tm.tm_year += 1;
	tm.tm_mon += 1;
	tm.tm_mday += 1;

	GPPC_RETURN_DATE(GppcTmGetDate(&tm));
}

GPPC_FUNCTION_INFO(timefunc1);
GppcDatum timefunc1(GPPC_FUNCTION_ARGS);

GppcDatum timefunc1(GPPC_FUNCTION_ARGS)
{
	GppcTime		arg = GPPC_GETARG_TIME(0);
	GppcTm			tm;

	GppcTimeGetTm(arg, &tm);
	tm.tm_hour += 1;
	tm.tm_min += 1;
	tm.tm_sec += 1;

	GPPC_RETURN_TIME(GppcTmGetTime(&tm));
}

GPPC_FUNCTION_INFO(timetzfunc1);
GppcDatum timetzfunc1(GPPC_FUNCTION_ARGS);

GppcDatum timetzfunc1(GPPC_FUNCTION_ARGS)
{
	GppcTimeTz		arg = GPPC_GETARG_TIMETZ(0);
	GppcTm			tm;

	GppcTimeTzGetTm(arg, &tm);
	tm.tm_hour += 1;
	tm.tm_min += 1;
	tm.tm_sec += 1;

	GPPC_RETURN_TIMETZ(GppcTmGetTimeTz(&tm));
}

GPPC_FUNCTION_INFO(timestampfunc1);
GppcDatum timestampfunc1(GPPC_FUNCTION_ARGS);

GppcDatum timestampfunc1(GPPC_FUNCTION_ARGS)
{
	GppcTimestamp	arg = GPPC_GETARG_TIMESTAMP(0);
	GppcTm			tm;

	GppcTimestampGetTm(arg, &tm);
	tm.tm_year += 1;
	tm.tm_mon += 1;
	tm.tm_mday += 1;
	tm.tm_hour += 1;
	tm.tm_min += 1;
	tm.tm_sec += 1;

	GPPC_RETURN_TIMESTAMP(GppcTmGetTimestamp(&tm));
}

GPPC_FUNCTION_INFO(timestamptzfunc1);
GppcDatum timestamptzfunc1(GPPC_FUNCTION_ARGS);

GppcDatum timestamptzfunc1(GPPC_FUNCTION_ARGS)
{
	GppcTimestampTz	arg = GPPC_GETARG_TIMESTAMPTZ(0);
	GppcTm			tm;

	GppcTimestampTzGetTm(arg, &tm);
	tm.tm_year += 1;
	tm.tm_mon += 1;
	tm.tm_mday += 1;
	tm.tm_hour += 1;
	tm.tm_min += 1;
	tm.tm_sec += 1;

	GPPC_RETURN_TIMESTAMPTZ(GppcTmGetTimestampTz(&tm));
}

/*
 * SPI test 1: run query, get the attribute by number at the final row as string
 */
GPPC_FUNCTION_INFO(spifunc1);
GppcDatum spifunc1(GPPC_FUNCTION_ARGS);

GppcDatum spifunc1(GPPC_FUNCTION_ARGS)
{
	char		   *query = GppcTextGetCString(GPPC_GETARG_TEXT(0));
	GppcInt4		attno = GPPC_GETARG_INT4(1);
	GppcSPIResult	result;
	char		   *val = NULL;

	if (GppcSPIConnect() < 0)
		GppcReport(GPPC_ERROR, "connect error");

	result = GppcSPIExec(query, 0);
	while (result->current < result->processed)
	{
		val = GppcSPIGetValue(result, attno, true);
		result->current++;
	}

	GppcSPIFinish();

	if (val)
		GPPC_RETURN_TEXT(GppcCStringGetText(val));
	else
		GPPC_RETURN_NULL();
}

/*
 * SPI test 2: run query, get the attribute by name at the final row as string
 */
GPPC_FUNCTION_INFO(spifunc2);
GppcDatum spifunc2(GPPC_FUNCTION_ARGS);

GppcDatum spifunc2(GPPC_FUNCTION_ARGS)
{
	char		   *query = GppcTextGetCString(GPPC_GETARG_TEXT(0));
	char		   *attname = GppcTextGetCString(GPPC_GETARG_TEXT(1));
	GppcSPIResult	result;
	char		   *val = NULL;

	if (GppcSPIConnect() < 0)
		GppcReport(GPPC_ERROR, "connect error");

	result = GppcSPIExec(query, 0);
	while (result->current < result->processed)
	{
		val = GppcSPIGetValueByName(result, attname, true);
		result->current++;
	}

	GppcSPIFinish();

	if (val)
		GPPC_RETURN_TEXT(GppcCStringGetText(val));
	else
		GPPC_RETURN_NULL();
}

/*
 * SPI test 3: run query, get the attribute by number at the final row as datum
 */
GPPC_FUNCTION_INFO(spifunc3);
GppcDatum spifunc3(GPPC_FUNCTION_ARGS);

GppcDatum spifunc3(GPPC_FUNCTION_ARGS)
{
	char		   *query = GppcTextGetCString(GPPC_GETARG_TEXT(0));
	GppcInt4		attno = GPPC_GETARG_INT4(1);
	GppcSPIResult	result;
	GppcDatum		datum = (GppcDatum) 0;
	bool			isnull = true;

	if (GppcSPIConnect() < 0)
		GppcReport(GPPC_ERROR, "connect error");

	result = GppcSPIExec(query, 0);
	while (result->current < result->processed)
	{
		datum = GppcSPIGetDatum(result, attno, &isnull, true);
		result->current++;
	}

	GppcSPIFinish();

	if (!isnull)
		GPPC_RETURN_TEXT(GppcDatumGetText(datum));
	else
		GPPC_RETURN_NULL();
}

/*
 * SPI test 4: run query, get the attribute by name at the final row as datum
 */
GPPC_FUNCTION_INFO(spifunc4);
GppcDatum spifunc4(GPPC_FUNCTION_ARGS);

GppcDatum spifunc4(GPPC_FUNCTION_ARGS)
{
	char		   *query = GppcTextGetCString(GPPC_GETARG_TEXT(0));
	char		   *attname = GppcTextGetCString(GPPC_GETARG_TEXT(1));
	GppcSPIResult	result;
	GppcDatum		datum = (GppcDatum) 0;
	bool			isnull = true;

	if (GppcSPIConnect() < 0)
		GppcReport(GPPC_ERROR, "connect error");

	result = GppcSPIExec(query, 0);
	while (result->current < result->processed)
	{
		datum = GppcSPIGetDatumByName(result, attname, &isnull, true);
		result->current++;
	}

	GppcSPIFinish();

	if (!isnull)
		GPPC_RETURN_TEXT(GppcDatumGetText(datum));
	else
		GPPC_RETURN_NULL();
}

/*
 * The error handler.  We can call GppcReport with INFO when ERROR, since it's not
 * infinite recursion.  For test purpose, set 'x' to message when WARNING.
 */
static void
errorcallback(GppcReportInfo info, void *arg)
{
	GppcReportLevel		elevel = GppcGetReportLevel(info);

	if (elevel == GPPC_WARNING && arg)
		memset(GppcGetTextPointer(arg), 'x', GppcGetTextLength(arg));
	else if (elevel == GPPC_ERROR && arg)
		GppcReport(GPPC_INFO, "inside callback: %s", GppcTextGetCString(arg));
}

GPPC_FUNCTION_INFO(errorcallbackfunc1);
GppcDatum errorcallbackfunc1(GPPC_FUNCTION_ARGS);

GppcDatum
errorcallbackfunc1(GPPC_FUNCTION_ARGS)
{
	GppcText		arg = GPPC_GETARG_TEXT_COPY(0);
	char		   *carg = GppcTextGetCString(arg);
	GppcReportCallbackState	cbinfo;

	cbinfo = GppcInstallReportCallback(errorcallback, arg);

	if (strcmp(carg, "info") == 0)
		GppcReport(GPPC_INFO, "info emit");
	else if (strcmp(carg, "warning") == 0)
		GppcReport(GPPC_WARNING, "warning emit");
	else if (strcmp(carg, "error") == 0)
		GppcReport(GPPC_ERROR, "error emit");

	GppcUninstallReportCallback(cbinfo);

	GPPC_RETURN_TEXT(arg);
}

GPPC_FUNCTION_INFO(tablefunc_describe);
GppcDatum tablefunc_describe(GPPC_FUNCTION_ARGS);

GppcDatum
tablefunc_describe(GPPC_FUNCTION_ARGS)
{
	GppcTupleDesc	tdesc;
	GppcInt4		avalue;
	bool			isnull, iserror;
	GppcTupleDesc	odesc;
	GppcReportCallbackState	cbstate;

	/* For a test purpose to make sure it's working in the describe func */
	cbstate = GppcInstallReportCallback(errorcallback, NULL);

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
	GppcReport(GPPC_INFO, "attlen is %d", GppcTupleDescAttrLen(tdesc, avalue - 1));

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
	GppcFuncCallContext	fctx;
	GppcAnyTable	scan;
	GppcTupleDesc	out_tupdesc, in_tupdesc;
	GppcHeapTuple	tuple;
	int				position;
	GppcDatum		values[1];
	bool			nulls[1];
	GppcDatum		result;

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
	if (GppcTupleDescAttrType(out_tupdesc, 0) !=
			GppcTupleDescAttrType(in_tupdesc, 0))
		GppcReport(GPPC_ERROR, "type mismatch");

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
	GppcTupleDesc	tdesc, odesc;
	GppcSPIResult	result;
	bool			isnull, iserror;
	char		   *query;
	char		   *colname = NULL;
	GppcDatum		d_colname;

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
	GppcFuncCallContext	fctx;
	GppcAnyTable		scan;
	GppcTupleDesc		odesc, idesc;
	GppcHeapTuple		tuple;
	char				colname[255];
	GppcDatum			values[1];
	bool				isnull[1];
	GppcDatum			result;

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
	GppcReportCallbackState	cbstate;
	char				   *message;
} MySession;

static void
tfcallback(GppcReportInfo info, void *arg)
{
	GppcReportLevel		elevel = GppcGetReportLevel(info);

	if (elevel == GPPC_ERROR)
	{
		MySession			   *sess;

		sess = (MySession *) arg;
		GppcReport(GPPC_INFO, "message: %s", sess->message);
		GppcFree(sess);
	}
}

GPPC_FUNCTION_INFO(project_errorcallback);
GppcDatum project_errorcallback(GPPC_FUNCTION_ARGS);

GppcDatum project_errorcallback(GPPC_FUNCTION_ARGS)
{
	GppcFuncCallContext	fctx;
	GppcAnyTable		scan;
	GppcTupleDesc		odesc, idesc;
	GppcHeapTuple		tuple;
	GppcDatum		   *values;
	bool			   *isnull;
	int					i, attnum;
	GppcDatum			result;

	scan = GPPC_GETARG_ANYTABLE(0);

	if (GPPC_SRF_IS_FIRSTCALL())
	{
		MySession				   *sess;

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
		MySession				   *sess;

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

GPPC_FUNCTION_INFO(test_encoding_name);
GppcDatum test_encoding_name(GPPC_FUNCTION_ARGS);

GppcDatum
test_encoding_name(GPPC_FUNCTION_ARGS)
{
	GppcText res;
	const char *str = GppcDatabaseEncodingName(GPPC_UTF8);
	size_t len = strlen(str);

	res = GppcAllocText(len);
	memcpy(GppcGetTextPointer(res), str, len);
	
	GPPC_RETURN_TEXT(res);
}

static void
allocate_a_lot(GppcMemoryContext context)
{
	void   *ptr;
	int		i;
	GppcMemoryContext oldcontext;

	oldcontext = GppcMemoryContextSwitchTo(context);

	for (i = 0; i < 20; i++)
	{
		/*
		 * 900M bytes, completely depending on the default setting,
		 * but it's fairly reasonable, I believe...
		 */
		ptr = (int *) GppcAlloc(900 * 1000 * 1000);

		GppcMemoryContextReset(context);
	}

	GppcMemoryContextSwitchTo(oldcontext);
}

GPPC_FUNCTION_INFO(test_reset_current_memory);
GppcDatum test_reset_current_memory(GPPC_FUNCTION_ARGS);

GppcDatum
test_reset_current_memory(GPPC_FUNCTION_ARGS)
{
	allocate_a_lot(GppcGetCurrentMemoryContext());

	GPPC_RETURN_BOOL(true);
}

GPPC_FUNCTION_INFO(test_reset_child_memory);
GppcDatum test_reset_child_memory(GPPC_FUNCTION_ARGS);

GppcDatum
test_reset_child_memory(GPPC_FUNCTION_ARGS)
{
	GppcMemoryContext current, child;
	int	   *my_ptr;

	current = GppcGetCurrentMemoryContext();
	child = GppcMemoryContextCreate(current);

	my_ptr = (int *) GppcAlloc(10 * sizeof(int));
	my_ptr[0] = 42;

	allocate_a_lot(child);

	/* Should still be valid. */
	GPPC_RETURN_BOOL(my_ptr[0] == 42);
}

GPPC_FUNCTION_INFO(test_interrupt);
GppcDatum test_interrupt(GPPC_FUNCTION_ARGS);

GppcDatum
test_interrupt(GPPC_FUNCTION_ARGS)
{
	GppcCheckForInterrupts();

	GppcReport(GPPC_ERROR, "GppcCheckForInterrupts did not catch error");

	GPPC_RETURN_NULL();
}
