/*-------------------------------------------------------------------------
 *
 * gppc.c
 *	  libgppc wrapper main
 *
 * Portions Copyright (c) 2012, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/interfaces/gppc/gppc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "tablefuncapi.h"

#include "executor/spi.h"
#include "mb/pg_wchar.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/memutils.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"

/*
 * GPPC_C_BUILD avoids definitions conflicting with actual backend definitions.
 */
#define GPPC_C_BUILD
#include "gppc/gppc.h"

#define ENSURE_NODE(val, nodetag) do{ \
	if (val != NULL && !IsA(val, nodetag)) \
		elog(ERROR, "unexpected value %d", (val == NULL ? 0 : nodeTag(val))); \
} while(0)

/*
 * libgppc.so has PG_MODULE_MAGIC, user libraries don't. Linking
 * to libgppc, user libraries can be loaded by the backend.
 */
PG_MODULE_MAGIC;

#define GPPC_MAP_FCINFO(info) ((FunctionCallInfo) info)

#define CHECK_TUPLEDESC_ATTNO(tupdesc, attno) do{ \
	if ((attno) < 0) \
		elog(ERROR, "requested invalid attno(%d)", (attno)); \
	if ((tupdesc) == NULL) \
		elog(ERROR, "tuple desc is null"); \
	if (((TupleDesc) (tupdesc))->natts <= (attno)) \
		elog(ERROR, "requested attno(%d) over tuple desc length(%d)", \
				(attno), ((TupleDesc) (tupdesc))->natts); \
}while(0)

typedef struct GppcReportCallbackStateData
{
	ErrorContextCallback		errcontext_cb;
	void	   (*func)(GppcReportInfo, void *);
	void	   *arg;
} GppcReportCallbackStateData;

/*
 * Type oids.  Since we don't want to expose PG headers to the third-party and
 * hard-coding in the header is not a good way to maintain, we assign them here.
 */
const GppcOid GppcOidInvalid = InvalidOid;
const GppcOid GppcOidBool = BOOLOID;
const GppcOid GppcOidChar = CHAROID;
const GppcOid GppcOidInt2 = INT2OID;
const GppcOid GppcOidInt4 = INT4OID;
const GppcOid GppcOidInt8 = INT8OID;
const GppcOid GppcOidFloat4 = FLOAT4OID;
const GppcOid GppcOidFloat8 = FLOAT8OID;
const GppcOid GppcOidText = TEXTOID;
const GppcOid GppcOidVarChar = VARCHAROID;
const GppcOid GppcOidBpChar = BPCHAROID;
const GppcOid GppcOidBytea = BYTEAOID;
const GppcOid GppcOidNumeric = NUMERICOID;
const GppcOid GppcOidDate = DATEOID;
const GppcOid GppcOidTime = TIMEOID;
const GppcOid GppcOidTimeTz = TIMETZOID;
const GppcOid GppcOidTimestamp = TIMESTAMPOID;
const GppcOid GppcOidTimestampTz = TIMESTAMPTZOID;
const GppcOid GppcOidAnyTable = ANYTABLEOID;

/*
 * V1 call convention helper function.  This is just to avoid leaking
 * the Pg_finfo_record struct.
 */
const void *
GppcFinfoV1(void)
{
	static const Pg_finfo_record my_finfo = { 1 };
	return &my_finfo;
}

/*
 * PG_NARGS
 */
int
GppcNargs(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return fcinfo->nargs;
}

/*
 * PG_RETURN_NULL
 */
GppcDatum
GppcReturnNull(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	fcinfo->isnull = true;
	return (GppcDatum ) 0;
}

/*
 * PG_ARGISNULL
 */
bool
GppcArgIsNull(GppcFcinfo info, int n)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return PG_ARGISNULL(n);
}

/*
 * PG_GETARG_DATUM
 */
GppcDatum
GppcGetArgDatum(GppcFcinfo info, int n)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return PG_GETARG_DATUM(n);
}

/*
 * BoolGetDatum
 */
GppcDatum
GppcBoolGetDatum(GppcBool x)
{
	return BoolGetDatum(x);
}

/*
 * CharGetDatum
 */
GppcDatum
GppcCharGetDatum(GppcChar x)
{
	return CharGetDatum(x);
}

/*
 * Int16GetDatum
 */
GppcDatum
GppcInt2GetDatum(GppcInt2 x)
{
	return Int16GetDatum(x);
}

/*
 * Int32GetDatum
 */
GppcDatum
GppcInt4GetDatum(GppcInt4 x)
{
	return Int32GetDatum(x);
}

/*
 * Int64GetDatum
 */
GppcDatum
GppcInt8GetDatum(GppcInt8 x)
{
	return Int64GetDatum(x);
}

/*
 * Float4GetDatum
 */
GppcDatum
GppcFloat4GetDatum(GppcFloat4 x)
{
	return Float4GetDatum(x);
}

/*
 * Float8GetDatum
 */
GppcDatum
GppcFloat8GetDatum(GppcFloat8 x)
{
	return Float8GetDatum(x);
}

/*
 * TextGetDatum
 */
GppcDatum
GppcTextGetDatum(GppcText x)
{
	return PointerGetDatum(x);
}

/*
 * VarCharGetDatum
 */
GppcDatum
GppcVarCharGetDatum(GppcVarChar x)
{
	return PointerGetDatum(x);
}

/*
 * BpCharGetDatum
 */
GppcDatum
GppcBpCharGetDatum(GppcBpChar x)
{
	return PointerGetDatum(x);
}

/*
 * ByteaGetDatum
 */
GppcDatum
GppcByteaGetDatum(GppcBytea x)
{
	return PointerGetDatum(x);
}

/*
 * NumericGetDatum
 */
GppcDatum
GppcNumericGetDatum(GppcNumeric x)
{
	return NumericGetDatum(x);
}

/*
 * DateGetDatum
 */
GppcDatum
GppcDateGetDatum(GppcDate x)
{
	return DateADTGetDatum(x);
}

/*
 * TimeGetDatum
 */
GppcDatum
GppcTimeGetDatum(GppcTime x)
{
	return TimeADTGetDatum(x);
}

/*
 * TimeTzGetDatum
 */
GppcDatum
GppcTimeTzGetDatum(GppcTimeTz x)
{
	return TimeTzADTPGetDatum(x);
}

/*
 * TimestampGetDatum
 */
GppcDatum
GppcTimestampGetDatum(GppcTimestamp x)
{
	return TimestampGetDatum(x);
}

/*
 * TimestampTzGetDatum
 */
GppcDatum
GppcTimestampTzGetDatum(GppcTimestampTz x)
{
	return TimestampTzGetDatum(x);
}

/*
 * AnyTableGetDatum
 */
GppcDatum
GppcAnyTableGetDatum(GppcAnyTable x)
{
	return PointerGetDatum(x);
}

/*
 * TupleDescGetDatum
 */
GppcDatum
GppcTupleDescGetDatum(GppcTupleDesc x)
{
	return PointerGetDatum(x);
}

/*
 * HeapTupleGetDatum
 */
GppcDatum
GppcHeapTupleGetDatum(GppcHeapTuple x)
{
	return PointerGetDatum(x);
}

/*
 * DatumGetBool
 */
GppcBool
GppcDatumGetBool(GppcDatum x)
{
	return DatumGetBool(x);
}

/*
 * DatumGetChar
 */
GppcChar
GppcDatumGetChar(GppcDatum x)
{
	return DatumGetChar(x);
}

/*
 * DatumGetInt16
 */
GppcInt2
GppcDatumGetInt2(GppcDatum x)
{
	return DatumGetInt16(x);
}

/*
 * DatumGetInt32
 */
GppcInt4
GppcDatumGetInt4(GppcDatum x)
{
	return DatumGetInt32(x);
}

/*
 * DatumGetInt64
 */
GppcInt8
GppcDatumGetInt8(GppcDatum x)
{
	return DatumGetInt64(x);
}

/*
 * DatumGetFloat4
 */
GppcFloat4
GppcDatumGetFloat4(GppcDatum x)
{
	return DatumGetFloat4(x);
}

/*
 * DatumGetFloat8
 */
GppcFloat8
GppcDatumGetFloat8(GppcDatum x)
{
	return DatumGetFloat8(x);
}

/*
 * DatumGetText
 */
GppcText
GppcDatumGetText(GppcDatum x)
{
	return (GppcText) DatumGetTextP(x);
}

/*
 * DatumGetTextCopy
 */
GppcText
GppcDatumGetTextCopy(GppcDatum x)
{
	return (GppcText) DatumGetTextPCopy(x);
}

/*
 * DatumGetVarChar
 */
GppcVarChar
GppcDatumGetVarChar(GppcDatum x)
{
	return (GppcVarChar) DatumGetVarCharP(x);
}

/*
 * DatumGetVarCharCopy
 */
GppcVarChar
GppcDatumGetVarCharCopy(GppcDatum x)
{
	return (GppcVarChar) DatumGetVarCharPCopy(x);
}

/*
 * DatumGetBpChar
 */
GppcBpChar
GppcDatumGetBpChar(GppcDatum x)
{
	return (GppcBpChar) DatumGetBpCharP(x);
}

/*
 * DatumGetBpCharCopy
 */
GppcBpChar
GppcDatumGetBpCharCopy(GppcDatum x)
{
	return (GppcBpChar) DatumGetBpCharPCopy(x);
}

/*
 * DatumGetBytea
 */
GppcBytea
GppcDatumGetBytea(GppcDatum x)
{
	return (GppcBytea) DatumGetByteaP(x);
}

/*
 * DatumGetByteaCopy
 */
GppcBytea
GppcDatumGetByteaCopy(GppcDatum x)
{
	return (GppcBytea) DatumGetByteaPCopy(x);
}

/*
 * DatumGetNumeric
 */
GppcNumeric
GppcDatumGetNumeric(GppcDatum x)
{
	return (GppcNumeric) DatumGetNumeric(x);
}

/*
 * DatumGetDate
 */
GppcDate
GppcDatumGetDate(GppcDatum x)
{
	return (GppcDate) DatumGetDateADT(x);
}

/*
 * DatumGetTime
 */
GppcTime
GppcDatumGetTime(GppcDatum x)
{
	return (GppcTime) DatumGetTimeADT(x);
}

/*
 * DatumGetTimeTz
 */
GppcTimeTz
GppcDatumGetTimeTz(GppcDatum x)
{
	return (GppcTimeTz) DatumGetTimeTzADTP(x);
}

/*
 * DatumGetTimestamp
 */
GppcTimestamp
GppcDatumGetTimestamp(GppcDatum x)
{
	return (GppcTimestamp) DatumGetTimestamp(x);
}

/*
 * DatumGetTimestampTz
 */
GppcTimestampTz
GppcDatumGetTimestampTz(GppcDatum x)
{
	return (GppcTimestampTz) DatumGetTimestampTz(x);
}

/*
 * DatumGetAnyTable
 */
GppcAnyTable
GppcDatumGetAnyTable(GppcDatum x)
{
	return (GppcAnyTable) DatumGetPointer(x);
}

/*
 * DatumGetTupleDesc
 */
GppcTupleDesc
GppcDatumGetTupleDesc(GppcDatum x)
{
	return (GppcTupleDesc) DatumGetPointer(x);
}

/*
 * DatumGetHeapTuple
 */
GppcHeapTuple
GppcDatumGetHeapTuple(GppcDatum x)
{
	return (GppcHeapTuple) DatumGetPointer(x);
}

/*
 *
 */
size_t
GppcGetTextLength(GppcText t)
{
	return VARSIZE(t) - VARHDRSZ;
}

/*
 * VARDATA(text)
 */
char *
GppcGetTextPointer(GppcText t)
{
	return VARDATA(t);
}

/*
 * TextDatumGetCString
 */
char *
GppcTextGetCString(GppcText t)
{
	return TextDatumGetCString(PointerGetDatum(t));
}

/*
 * CStringGetTextDatum
 */
GppcText
GppcCStringGetText(const char *s)
{
	return (GppcText) cstring_to_text(s);
}

/*
 *
 */
size_t
GppcGetVarCharLength(GppcVarChar t)
{
	return VARSIZE(t) - VARHDRSZ;
}

/*
 * VARDATA(VarChar)
 */
char *
GppcGetVarCharPointer(GppcVarChar t)
{
	return VARDATA(t);
}

/*
 * VarCharDatumGetCString
 */
char *
GppcVarCharGetCString(GppcVarChar t)
{
	return TextDatumGetCString(PointerGetDatum(t));
}

/*
 * CStringGetVarCharDatum
 */
GppcVarChar
GppcCStringGetVarChar(const char *s)
{
	return (GppcVarChar) cstring_to_text(s);
}

/*
 *
 */
size_t
GppcGetBpCharLength(GppcBpChar t)
{
	return VARSIZE(t) - VARHDRSZ;
}

/*
 * VARDATA(BpChar)
 */
char *
GppcGetBpCharPointer(GppcBpChar t)
{
	return VARDATA(t);
}

/*
 * BpCharDatumGetCString
 */
char *
GppcBpCharGetCString(GppcBpChar t)
{
	return TextDatumGetCString(PointerGetDatum(t));
}

/*
 * CStringGetBpCharDatum
 */
GppcBpChar
GppcCStringGetBpChar(const char *s)
{
	return (GppcBpChar) cstring_to_text(s);
}

/*
 *
 */
size_t
GppcGetByteaLength(GppcBytea x)
{
	return VARSIZE(x) - VARHDRSZ;
}

/*
 * VARDATA(x)
 */
char *
GppcGetByteaPointer(GppcBytea x)
{
	return VARDATA(x);
}

/*
 * numeric_out
 */
char *
GppcNumericGetCString(GppcNumeric n)
{
	return DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum((Numeric *) n)));
}

/*
 * numeric_in
 */
GppcNumeric
GppcCStringGetNumeric(const char *s)
{
	return (GppcNumeric) DatumGetPointer(
		DirectFunctionCall3(numeric_in, CStringGetDatum(s), NUMERICOID, -1));
}

bool
GppcGetNumericDef(int32_t typmod, int16_t *precision, int16_t *scale)
{
	*precision = 0;
	*scale = 0;
	if (typmod < (int32_t) (VARHDRSZ))
		return false;

	typmod -= VARHDRSZ;
	*precision = (typmod >> 16) & 0xffff;
	*scale = typmod & 0xffff;

	return true;
}

GppcFloat8
GppcNumericGetFloat8(GppcNumeric n)
{
	return DatumGetFloat8(
			DirectFunctionCall1(numeric_float8, NumericGetDatum((Numeric *) n)));
}

GppcNumeric
GppcFloat8GetNumeric(GppcFloat8 f)
{
	return (GppcNumeric) DatumGetPointer(
			DirectFunctionCall1(float8_numeric, Float8GetDatum(f)));
}

static void
timestamp2gppctm(Timestamp ts, GppcTm *tm)
{
	struct pg_tm	pgtm;
	fsec_t			fsec;

	if (timestamp2tm((Timestamp) ts, NULL, &pgtm, &fsec, NULL, NULL) != 0)
		elog(ERROR, "failed to convert timestamp");

	memcpy(tm, &pgtm, Min(sizeof(GppcTm), sizeof(struct pg_tm)));
	tm->tm_fsec = fsec;
}

static Timestamp
gppctm2timestamp(GppcTm *tm)
{
	struct pg_tm	pgtm;
	Timestamp		ts;
	fsec_t			fsec;

	memcpy(&pgtm, tm, Min(sizeof(GppcTm), sizeof(struct pg_tm)));
	fsec = tm->tm_fsec;

	if (tm2timestamp(&pgtm, fsec, NULL, &ts) != 0)
		elog(ERROR, "failed to convert to timestamp");

	return ts;
}


void
GppcDateGetTm(GppcDate x, GppcTm *tm)
{
	Timestamp		ts;

	ts = DatumGetTimestamp(DirectFunctionCall1(date_timestamp, DateADTGetDatum(x)));
	timestamp2gppctm(ts, tm);
}

GppcDate
GppcTmGetDate(GppcTm *tm)
{
	Timestamp		ts;

	ts = gppctm2timestamp(tm);

	return DatumGetDateADT(DirectFunctionCall1(timestamp_date, TimestampGetDatum(ts)));
}

void
GppcTimeGetTm(GppcTime x, GppcTm *tm)
{
	Timestamp		ts;

	ts = DatumGetTimestamp(DirectFunctionCall2(datetime_timestamp,
			DateADTGetDatum(POSTGRES_EPOCH_JDATE), TimeADTGetDatum((TimeADT) x)));

	timestamp2gppctm(ts, tm);
}

GppcTime
GppcTmGetTime(GppcTm *tm)
{
	Timestamp		ts;

	ts = gppctm2timestamp(tm);

	return DatumGetTimeADT(DirectFunctionCall1(timestamp_time, TimestampGetDatum(ts)));
}

void
GppcTimeTzGetTm(GppcTimeTz x, GppcTm *tm)
{
	TimeADT			t;
	Timestamp		ts;

	/*
	 * Drop time zone, as we adjust it below.  We do this because we don't have
	 * timestamptz2tm and always convert via timestamp.
	 */
	t = DatumGetTimeADT(
			DirectFunctionCall1(timetz_time, TimeTzADTPGetDatum((TimeTzADT *) x)));
	ts = DatumGetTimestamp(DirectFunctionCall2(datetime_timestamp,
			DateADTGetDatum(POSTGRES_EPOCH_JDATE), TimeADTGetDatum(t)));

	timestamp2gppctm(ts, tm);
	/* adjust time zone. gmtoff is in sec */
	tm->tm_isdst = 0;
	tm->tm_gmtoff = ((TimeTzADT *) x)->zone;
}

GppcTimeTz
GppcTmGetTimeTz(GppcTm *tm)
{
	Timestamp		ts;
	TimeADT			t;
	TimeTzADT	   *result;

	ts = gppctm2timestamp(tm);

	t = DatumGetTimeADT(DirectFunctionCall1(timestamp_time, TimestampGetDatum(ts)));
	result = DatumGetTimeTzADTP(DirectFunctionCall1(time_timetz, TimeADTGetDatum(t)));
	/* adjust time zone. gmtoff is in sec */
	result->zone = tm->tm_gmtoff;
	return (GppcTimeTz) result;
}

void
GppcTimestampGetTm(GppcTimestamp x, GppcTm *tm)
{
	timestamp2gppctm(x, tm);
}

GppcTimestamp
GppcTmGetTimestamp(GppcTm *tm)
{
	return gppctm2timestamp(tm);
}

void
GppcTimestampTzGetTm(GppcTimestampTz x, GppcTm *tm)
{
	timestamp2gppctm(x, tm);
}

GppcTimestampTz
GppcTmGetTimestampTz(GppcTm *tm)
{
	return gppctm2timestamp(tm);
}

/*
 * palloc
 */
void *
GppcAlloc(size_t bytes)
{
	return palloc(bytes);
}

/*
 * palloc0
 */
void *
GppcAlloc0(size_t bytes)
{
	return palloc0(bytes);
}

/*
 * repalloc
 */
void *
GppcRealloc(void *ptr, size_t bytes)
{
	return repalloc(ptr, bytes);
}

/*
 * pfree
 */
void
GppcFree(void *ptr)
{
	pfree(ptr);
}

/*
 * clen is byte length of the content
 */
GppcText
GppcAllocText(size_t clen)
{
	text	   *t;
	size_t		bytes;

	bytes = VARHDRSZ + clen;
	t = (text *) palloc(bytes);
	SET_VARSIZE(t, bytes);
	return (GppcText) t;
}

/*
 * clen is byte length of the content
 */
GppcVarChar
GppcAllocVarChar(size_t clen)
{
	text	   *t;
	size_t		bytes;

	bytes = VARHDRSZ + clen;
	t = (text *) palloc(bytes);
	SET_VARSIZE(t, bytes);
	return (GppcVarChar) t;
}

/*
 * clen is byte length of the content
 */
GppcBpChar
GppcAllocBpChar(size_t clen)
{
	text	   *t;
	size_t		bytes;

	bytes = VARHDRSZ + clen;
	t = (text *) palloc(bytes);
	SET_VARSIZE(t, bytes);
	return (GppcBpChar) t;
}

/*
 * blen is byte length of the content
 */
GppcBytea
GppcAllocBytea(size_t blen)
{
	bytea	   *b;
	size_t		bytes;

	bytes = VARHDRSZ + blen;
	b = (bytea *) palloc(bytes);
	SET_VARSIZE(b, bytes);
	return (GppcBytea) b;
}

/*
 * CurrentMemoryContext
 */
GppcMemoryContext
GppcGetCurrentMemoryContext(void)
{
	return (GppcMemoryContext) CurrentMemoryContext;
}

/*
 * AllocSetContextCreate
 */
GppcMemoryContext
GppcMemoryContextCreate(GppcMemoryContext parent)
{
	return (GppcMemoryContext)
		AllocSetContextCreate(parent,
							  "GppcUserContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);
}

/*
 * MemoryContextSwitchTo
 */
GppcMemoryContext
GppcMemoryContextSwitchTo(GppcMemoryContext context)
{
	return (GppcMemoryContext)
		MemoryContextSwitchTo((MemoryContext) context);
}

/*
 * MemoryContextReset
 */
void
GppcMemoryContextReset(GppcMemoryContext context)
{
	MemoryContextReset((MemoryContext) context);
}

/*
 * SRF_IS_FIRSTCALL
 */
bool
GppcSRFIsFirstCall(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return fcinfo->flinfo->fn_extra == NULL;
}

/*
 * SRF_FIRSTCALL_INIT
 */
GppcFuncCallContext
GppcSRFFirstCallInit(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return (GppcFuncCallContext) init_MultiFuncCall(fcinfo);
}

/*
 * SRF_PERCALL_SETUP
 */
GppcFuncCallContext
GppcSRFPerCallSetup(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);

	return (GppcFuncCallContext) per_MultiFuncCall(fcinfo);
}

/*
 * SRF_RETURN_NEXT
 */
GppcDatum
GppcSRFReturnNext(GppcFcinfo info, GppcFuncCallContext funcctx, GppcDatum result)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);
	ReturnSetInfo	   *rsi;

	((FuncCallContext *) funcctx)->call_cntr++;
	rsi = (ReturnSetInfo *) fcinfo->resultinfo;
	rsi->isDone = ExprMultipleResult;
	PG_RETURN_DATUM(result);
}

/*
 * SRF_RETURN_DONE
 */
GppcDatum
GppcSRFReturnDone(GppcFcinfo info, GppcFuncCallContext funcctx)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);
	ReturnSetInfo	   *rsi;

	end_MultiFuncCall(fcinfo, ((FuncCallContext *) funcctx));
	rsi = (ReturnSetInfo *) fcinfo->resultinfo;
	rsi->isDone = ExprEndResult;
	PG_RETURN_NULL();
}

/*
 * Returns TupleDesc for the exptected output of the function
 */
GppcTupleDesc
GppcSRFResultDesc(GppcFcinfo info)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);
	ReturnSetInfo	   *rsi;

	rsi = (ReturnSetInfo *) fcinfo->resultinfo;

	return (GppcTupleDesc) rsi->expectedDesc;
}

void *
GppcSRFAlloc(GppcFuncCallContext fctx, size_t bytes)
{
	FuncCallContext	   *ctx = (FuncCallContext *) fctx;

	return MemoryContextAlloc(ctx->multi_call_memory_ctx, bytes);
}

void *
GppcSRFAlloc0(GppcFuncCallContext fctx, size_t bytes)
{
	FuncCallContext	   *ctx = (FuncCallContext *) fctx;

	return MemoryContextAllocZero(ctx->multi_call_memory_ctx, bytes);
}

void
GppcSRFSave(GppcFuncCallContext fctx, void *ptr)
{
	FuncCallContext	   *ctx = (FuncCallContext *) fctx;

	ctx->user_fctx = ptr;
}

void *
GppcSRFRestore(GppcFuncCallContext fctx)
{
	FuncCallContext	   *ctx = (FuncCallContext *) fctx;

	return ctx->user_fctx;
}

int
GppcSPIConnect(void)
{
	int		rescode;

	rescode = SPI_connect();
	return rescode;
}

int
GppcSPIFinish(void)
{
	return SPI_finish();
}

GppcSPIResult
GppcSPIExec(const char *src, long tcount)
{
	int				rescode;
	GppcSPIResult	result;

	rescode = SPI_exec(src, tcount);
	result = (GppcSPIResult) palloc0(sizeof(GppcSPIResultData));
	if (rescode >= 0)
	{
		result->tuptable = (struct GppcSPITupleTableData *) SPI_tuptable;
		result->processed = SPI_processed;
	}
	result->rescode = rescode;

	return result;
}

char *
GppcSPIGetValue(GppcSPIResult result, int fnumber, bool makecopy)
{
	SPITupleTable  *tuptable = (SPITupleTable *) result->tuptable;
	char		   *value;

	value = SPI_getvalue(tuptable->vals[result->current], tuptable->tupdesc, fnumber);
	/*
	 * If the copy is demanded, move value to the upper context.
	 */
	if (makecopy && value)
	{
		char	   *tmp;
		size_t		len;

		len = strlen(value);
		tmp = SPI_palloc(len + 1);
		strcpy(tmp, value);
		value = tmp;
	}
	return value;
}

GppcDatum
GppcSPIGetDatum(GppcSPIResult result, int fnumber, bool *isnull, bool makecopy)
{
	SPITupleTable  *tuptable = (SPITupleTable *) result->tuptable;
	HeapTuple		tuple;

	/*
	 * It might be better if we could copy only the datum, but SPI interface
	 * doesn't have such function and copying datumCopy code here is too ugly.
	 */
	if (makecopy)
		tuple = SPI_copytuple(tuptable->vals[result->current]);
	else
		tuple = tuptable->vals[result->current];
	return SPI_getbinval(tuple, tuptable->tupdesc, fnumber, isnull);
}

char *
GppcSPIGetValueByName(GppcSPIResult result, const char *fname, bool makecopy)
{
	SPITupleTable  *tuptable = (SPITupleTable *) result->tuptable;
	int				fnumber = SPI_fnumber(tuptable->tupdesc, fname);

	return GppcSPIGetValue(result, fnumber, makecopy);
}

GppcDatum
GppcSPIGetDatumByName(GppcSPIResult result, const char *fname, bool *isnull, bool makecopy)
{
	SPITupleTable  *tuptable = (SPITupleTable *) result->tuptable;
	int				fnumber = SPI_fnumber(tuptable->tupdesc, fname);

	return GppcSPIGetDatum(result, fnumber, isnull, makecopy);
}

/*
 * TF_SET_USERDATA
 */
void
GppcTFSetUserData(void *GppcFcinfo, GppcBytea userdata)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(GppcFcinfo);

	TF_SET_USERDATA((bytea *) userdata);
}

/*
 * TF_GET_USERDATA
 */
GppcBytea
GppcTFGetUserData(void *GppcFcinfo)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(GppcFcinfo);

	return (GppcBytea) TF_GET_USERDATA();
}

/*
 * AnyTable_GetTupleDesc
 */
GppcTupleDesc
GppcAnyTableGetTupleDesc(GppcAnyTable t)
{
	return (GppcTupleDesc) AnyTable_GetTupleDesc((AnyTable) t);
}

/*
 * AnyTable_GetNextTuple
 */
GppcHeapTuple
GppcAnyTableGetNextTuple(GppcAnyTable t)
{
	return (GppcHeapTuple) AnyTable_GetNextTuple((AnyTable) t);
}

/*
 * Common code for the describe function.
 */
static FuncExpr *
TFGetFuncExpr(GppcFcinfo info, int argno, Oid typid)
{
	FunctionCallInfo	fcinfo = GPPC_MAP_FCINFO(info);
	FuncExpr		   *fexpr;

	/* Describe function argument is only 1 */
	if (PG_NARGS() < 1)
	{
		elog(DEBUG1, "invalid describe function or TFGetFuncExpr call");
		return NULL;
	}

	fexpr = (FuncExpr *) PG_GETARG_POINTER(0);
	if (!fexpr || !IsA(fexpr, FuncExpr))
	{
		elog(DEBUG1, "expected FuncExpr, but something else is found");
		return NULL;
	}

	if (list_length(fexpr->args) <= argno)
	{
		elog(DEBUG1, "argno is out of range");
		return NULL;
	}

	if (OidIsValid(typid) &&
		exprType(list_nth(fexpr->args, argno)) != typid)
	{
		elog(DEBUG1, "expected type = %u, but the expression returns different", typid);
		return NULL;
	}

	return fexpr;
}

/*
 * Evaluates function argument to constant Datum.
 * iserror is set if function is unexpectedly structured.
 * This function is designed for describe function.
 */
GppcDatum
GppcTFGetArgDatum(GppcFcinfo info, GppcOid typid, int argno, bool *isnull, bool *iserror)
{
	FuncExpr	   *fexpr;

	if (iserror)
		*iserror = false;

	fexpr = TFGetFuncExpr(info, argno, (Oid) typid);

	if (!fexpr)
	{
		if (iserror)
			*iserror = true;
		if (isnull)
			*isnull = true;
		return (GppcDatum) 0;
	}

	return ExecEvalFunctionArgToConst(fexpr, argno, isnull);
}

/*
 * Create TupleDesc for the table function input TABLE expression.
 * iserror is set if function is unexpectedly structured.
 * This function is designed for describe function.
 */
GppcTupleDesc
GppcTFInputDesc(GppcFcinfo info, int argno, bool *iserror)
{
	FuncExpr		   *fexpr;
	TableValueExpr	   *texpr;
	Query			   *qexpr;

	if (iserror)
		*iserror = false;
	fexpr = TFGetFuncExpr(info, argno, ANYTABLEOID);
	if (!fexpr)
	{
		if (iserror)
			*iserror = true;
		return NULL;
	}

	texpr = (TableValueExpr *) list_nth(fexpr->args, argno);
	if (!texpr || !IsA(texpr, TableValueExpr))
	{
		if (iserror)
			*iserror = true;
		return NULL;
	}

	qexpr = (Query *) texpr->subquery;
	if (!qexpr || !IsA(qexpr, Query))
	{
		if (iserror)
			*iserror = true;
		return NULL;
	}

	return (GppcTupleDesc) ExecCleanTypeFromTL(qexpr->targetList);
}

/*
 * elog
 */
void
GppcReport(GppcReportLevel elevel, const char *fmt, ...)
{
	va_list		ap;
	char		buf[1024];

	/* elog doesn't accept va_list. There could be a better way...  */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	elog(elevel, "%s", (const char *) buf);
}

/*
 * This gets in between user hook and error process and extracts error state from
 * the stack to pass to the hook function.
 */
static void
ReportCallbackInvoker(void *arg)
{
	GppcReportCallbackState	cbstate = (GppcReportCallbackState) arg;
	ErrorData			   *edata;
	MemoryContext			oldcontext;

	/*
	 * CopyErrorData() needs to be in other context than ErrorContext.
	 */
	oldcontext = MemoryContextSwitchTo(CurTransactionContext);
	edata = CopyErrorData();
	cbstate->func((GppcReportInfo) edata, cbstate->arg);
	pfree(edata);
	MemoryContextSwitchTo(oldcontext);
}

/*
 * Add the error context callback to the stack.  This searches the
 * same callback with the same argument by comparing the callback
 * and arg address, and if found return the stack pointer without
 * installing new hook.  The function signature of the hook is
 * slightly different from PG's hook function, for we want to know
 * the error state.
 */
GppcReportCallbackState
GppcInstallReportCallback(void (*func)(GppcReportInfo, void *), void *arg)
{
	GppcReportCallbackState		cbstate;
	ErrorContextCallback	   *errcontext_cb;

	errcontext_cb = error_context_stack;
	while (errcontext_cb)
	{
		/* If found, just return it. */
		if (errcontext_cb->callback == ReportCallbackInvoker &&
			((GppcReportCallbackState) errcontext_cb->arg)->func == func &&
			((GppcReportCallbackState) errcontext_cb->arg)->arg == arg)
			return (GppcReportCallbackState) errcontext_cb;
		errcontext_cb = errcontext_cb->previous;
	}
	/*
	 * Allocate it in ErrorContext as the caller may expect it to live longer.
	 */
	cbstate = (GppcReportCallbackState)
		MemoryContextAlloc(ErrorContext, sizeof(GppcReportCallbackStateData));
	errcontext_cb = (ErrorContextCallback *) cbstate;
	cbstate->func = func;
	cbstate->arg = arg;

	errcontext_cb->callback = ReportCallbackInvoker;
	errcontext_cb->arg = cbstate;
	errcontext_cb->previous = error_context_stack;
	error_context_stack = errcontext_cb;

	return cbstate;
}

/*
 * Uninstall the error context callback from the stack.  The callback
 * may be not on the top of stack, but we search it by the given pointer
 * and fix the linked list in the middle of stack if found.  In case
 * the hook is not found, this function does not do anything and silently
 * return.
 */
void
GppcUninstallReportCallback(GppcReportCallbackState cbstate)
{
	ErrorContextCallback	   *errcontext_cb, *next;

	errcontext_cb = error_context_stack;
	next = NULL;
	while (errcontext_cb)
	{
		if (errcontext_cb == (ErrorContextCallback *) cbstate)
			break;
		next = errcontext_cb;
		errcontext_cb = errcontext_cb->previous;
	}
	if (errcontext_cb)
	{
		if (next)
			next->previous = errcontext_cb->previous;
		else
			error_context_stack = errcontext_cb->previous;

		pfree(errcontext_cb);
	}
}

GppcReportLevel
GppcGetReportLevel(GppcReportInfo info)
{
	return ((ErrorData *) info)->elevel;
}

const char *
GppcGetReportMessage(GppcReportInfo info)
{
	return ((ErrorData *) info)->message;
}

void
GppcCheckForInterrupts(void)
{
	CHECK_FOR_INTERRUPTS();
}

/*
 * CreateTemplateTupleDesc
 * We omit unuseful last parameter.
 */
GppcTupleDesc
GppcCreateTemplateTupleDesc(int natts)
{
	return (GppcTupleDesc) CreateTemplateTupleDesc(natts);
}

/*
 * TupleDescInitEntry
 * We omit unuseful last parameter.
 * attno starts from 1
 */
void
GppcTupleDescInitEntry(GppcTupleDesc desc,
					   uint16_t attno,
					   const char *attname,
					   GppcOid typid,
					   int32_t typmod)
{
	TupleDescInitEntry((TupleDesc) desc, attno, attname, typid, typmod, 0);
}

/*
 * heap_form_tuple
 */
GppcHeapTuple
GppcHeapFormTuple(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls)
{
	return (GppcHeapTuple) heap_form_tuple((TupleDesc) tupdesc, (Datum *) values, nulls);
}

/*
 * Shortcut for heap_form_tuple + HeapTupleGetDatum
 */
GppcDatum
GppcBuildHeapTupleDatum(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls)
{
	HeapTuple		tuple;

	tuple = heap_form_tuple((TupleDesc) tupdesc, (Datum *) values, nulls);
	return (GppcDatum) HeapTupleGetDatum(tuple);
}

/*
 * Accessor to HeapTuple
 */
GppcDatum
GppcGetAttributeByName(GppcHeapTuple tuple, const char *attname, bool *isnull)
{
	return (GppcDatum) GetAttributeByName(((HeapTuple) tuple)->t_data, attname, isnull);
}

/* attno starts from 1 */
GppcDatum
GppcGetAttributeByNum(GppcHeapTuple tuple, int16_t attno, bool *isnull)
{
	return (GppcDatum) GetAttributeByNum(((HeapTuple) tuple)->t_data, attno, isnull);
}

/*
 * Accessor to TupleDesc
 */
int
GppcTupleDescNattrs(GppcTupleDesc tupdesc)
{
	return ((TupleDesc) tupdesc)->natts;
}

/* attno starts from 0 */
const char *
GppcTupleDescAttrName(GppcTupleDesc tupdesc, int16_t attno)
{
	CHECK_TUPLEDESC_ATTNO(tupdesc, attno);
	return NameStr(TupleDescAttr((TupleDesc) tupdesc, attno)->attname);
}

/* attno starts from 0 */
GppcOid
GppcTupleDescAttrType(GppcTupleDesc tupdesc, int16_t attno)
{
	CHECK_TUPLEDESC_ATTNO(tupdesc, attno);
	return (GppcOid) (TupleDescAttr((TupleDesc) tupdesc, attno)->atttypid);
}

/* attno starts from 0 */
int32_t
GppcTupleDescAttrTypmod(GppcTupleDesc tupdesc, int16_t attno)
{
	CHECK_TUPLEDESC_ATTNO(tupdesc, attno);
	return (int32_t) (TupleDescAttr((TupleDesc) tupdesc, attno)->atttypmod);
}

/* attno starts from 0 */
int16_t
GppcTupleDescAttrLen(GppcTupleDesc tupdesc, int16_t attno)
{
	CHECK_TUPLEDESC_ATTNO(tupdesc, attno);
	return (int16_t) (TupleDescAttr((TupleDesc) tupdesc, attno)->attlen);
}

/*
 * GetDatabaseEncoding
 * Returns the value from GetDatabaseEncoding for now, but if we renumber pg_enc,
 * we'll need to map it GppcEncoding to keep compatibility.
 */
GppcEncoding
GppcGetDatabaseEncoding(void)
{
	return GetDatabaseEncoding();
}

/*
 * pg_database_encoding_max_length
 */
int
GppcDatabaseEncodingMaxLength(void)
{
	return pg_database_encoding_max_length();
}

/*
 * Translates a GppcEncoding value to a human readable string.
 */
const char *
GppcDatabaseEncodingName(GppcEncoding enc)
{
	return pg_encoding_to_char(enc);
}
