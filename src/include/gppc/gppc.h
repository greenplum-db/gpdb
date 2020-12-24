/* ----------------------------------------------------------------------- *//**
 *
 * @file gppc.h
 *
 * @brief Greenplum Partner Connector main header
 *
 * This header contains prototypes for libgppc -- the Greenplum Database Partner
 * Connector.
 *
 * The most common use of the GPPC is to write user defined functions which do
 * not need to be recompiled from one Greenplum Database release to the next.
 *
 * Those user defined functions usually have the following form:
 *
 * \code
 *   GPPC_FUNCTION_INFO(funcname);
 *   
 *   GppcDatum
 *   funcname(GPPC_FUNCTION_ARGS)
 *   {
 *     GppcXXX arg1 = GPPC_GETARG_XXXX(0);
 *     ....
 *     GPPC_RETURN_XXX(result);
 *   }
 * \endcode
 *
 * GPPC 1.0 is compatible with Greenplum Database 4.2.0.0 and above.
 * GPPC 1.1 is compatible with Greenplum Database 4.2.2.0 and above.
 * GPPC 1.2 is compatible with Greenplum Database 4.2.2.0 and above.
 *
 * Portions Copyright (c) 2012, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/gppc/gppc.h
 *
 *//* ----------------------------------------------------------------------- */
#ifndef GPPC_H
#define GPPC_H

#include <sys/types.h>
#include <stdint.h>

#ifndef GPPC_C_BUILD

#ifdef PG_VERSION_NUM
#error "PG_VERSION_NUM detected. Please not include GPDB internal headers."
#endif

#ifndef __cplusplus /* C++ has built-in bool */

/**
 * \brief Generic boolean type if it's not defined.
 */
typedef char		bool;
#ifndef true
/**
 * \brief Generic boolean true if it's not defined.
 */
#define true ((bool) 1)
#endif
#ifndef false
/**
 * \brief Generic boolean false if it's not defined.
 */
#define false ((bool) 0)
#endif

#endif  /* __cplusplus */

#endif  /* GPPC_C_BUILD */

/**
 * \brief Creates the necessary glue to make the named function SQL invokable.
 * \param funcname the name of exposed function.
 *
 * Every SQL invokable function must be declared via this macro.
 */
#define GPPC_FUNCTION_INFO(funcname) \
extern const void * pg_finfo_##funcname (void); \
const void * \
pg_finfo_##funcname (void) \
{ \
	return GppcFinfoV1(); \
} \
extern int no_such_variable

/**
 * \ingroup TimestampOps
 * \brief Temporal data type information extended from C standard tm structure.
 * The fields of this structure are equivalent to the ones of tm, though there are
 * a few modifications.  \a tm_year is an exact number of year, not 1900-based.
 * \a tm_mon starts from 1, not 0.  \a tm_isdst is 0 when \a tm_gmtoff is valid
 * as offset seconds from UTC.  \a tm_zone may contain time zone string.
 * \a tm_fsec is the fraction second.
 *
 *\sa GppcDateGetTm(), GppcTimeGetTm(), GppcTimeTzGetTm(), GppcTimestampGetTm(),
 *    GppcTimestampTzGetTm(), GppcTmGetDate(), GppcTmGetTime(), GppcTmGetTimeTz(),
 *    GppcTmGetTimestamp(), GppcTmGetTimestampTz()
 */
typedef struct GppcTm
{
	int			tm_sec; /**< second */
	int			tm_min; /**< minute */
	int			tm_hour; /**< hour */
	int			tm_mday; /**< day of month */
	int			tm_mon; /**< month */
	int			tm_year; /**< year */
	int			tm_wday; /**< day of week */
	int			tm_yday; /**< day of year */
	int			tm_isdst; /**< 0 if gmtoff is valid */
	int			tm_gmtoff; /**< GMT offset in seconds */
	const char *tm_zone; /**< time zone string */
	int			tm_fsec; /**< fraction second */
} GppcTm;

/**
 * \ingroup SPI
 * \brief Represents an SPI result data.  \a processed has the number of
 * processed rows by SPI mechanism and \a current holds the counter which
 * is used as the tuple pointer in GppcSPIGetDatumByNum() etc. You can set
 * it from user code as long as you keep 0 <= \a current < \a processed.
 * \a rescode is the SPI result code which indicates the result of last
 * operation issued by SPI.
 */
typedef struct GppcSPIResultData
{
	struct GppcSPITupleTableData   *tuptable; /**< opaque pointer to the internal data */
	uint32_t						processed; /**< number of affected rows by the operation */
	uint32_t						current; /**< position of tuples now looking at */
	int								rescode; /**< result code of last operation */
} GppcSPIResultData;

/**
 * \ingroup SPI
 * \brief pointer type of ::GppcSPIResultData
 * \sa ::GppcSPIResultData.
 */
typedef GppcSPIResultData *GppcSPIResult;

/**
 * \ingroup Reports
 * \brief Wraps error report information that can be accessed in a report
 * callback function.
 */
typedef struct GppcReportInfoData *GppcReportInfo;

/**
 * \brief SQL callable function's argument.
 *
 * Every function must have the same signature with this macro
 * as its only argument.
 */
#define GPPC_FUNCTION_ARGS GppcFcinfo fcinfo

/****************************************************************************
 * GPPC types
 ****************************************************************************/
/**
 * \brief A type which contains function call information.
 *
 * The contents of this data type are opaque to users of GPPC.
 */
typedef struct GppcFcinfoData		   *GppcFcinfo;

/**
 * \brief Generic type which can represent any data type.
 */
typedef int64_t							GppcDatum;

/**
 * \brief One byte boolean type which maps to SQL bool.
 */
typedef char							GppcBool;

/**
 * \brief One byte character type which maps to SQL "char".
 */
typedef char							GppcChar;

/**
 * \brief Two byte signed integer type which maps to SQL int2/smallint.
 */
typedef int16_t							GppcInt2;

/**
 * \brief Four byte signed integer type which maps to SQL int4/integer.
 */
typedef int32_t							GppcInt4;

/**
 * \brief Eight byte signed integer type which maps to SQL int8/bigint.
 */
typedef int64_t							GppcInt8;

/**
 * \brief Four byte floating point type which maps to SQL float4/real.
 */
typedef float							GppcFloat4;

/**
 * \brief Eight byte floating point type which maps to SQL
 * float8/double precision.
 */
typedef double							GppcFloat8;

/**
 * \brief Generic string type which maps to SQL text.
 */
typedef struct GppcTextData			   *GppcText;

/**
 * \brief Variable length string which maps to SQL varchar.
 */
typedef struct GppcVarCharData		   *GppcVarChar;

/**
 * \brief Blank padded string which maps to SQL char.
 */
typedef struct GppcBpCharData		   *GppcBpChar;

/**
 * \brief Byte array type which maps to SQL bytea.
 */
typedef struct GppcByteaData		   *GppcBytea;

/**
 * \brief Decimal type which maps to SQL numeric.
 */
typedef struct GppcNumericData		   *GppcNumeric;

/**
 * \brief Date type which maps to SQL date.
 */
typedef int32_t							GppcDate;

/**
 * \brief Time without time zone which maps to SQL time.
 */
typedef int64_t							GppcTime;

/**
 * \brief Time with time zone which maps to SQL timetz.
 */
typedef struct TimeTzData			   *GppcTimeTz;

/**
 * \brief Timestamp without time zone which maps to SQL timestamp.
 */
typedef int64_t							GppcTimestamp;

/**
 * \brief Timestamp with time zone which maps to SQL timestamptz.
 */
typedef int64_t							GppcTimestampTz;

/**
 * \brief Subquery representation which maps to SQL anytable.
 */
typedef struct GppcAnyTableData		   *GppcAnyTable;

/**
 * \brief Unsigned integer which maps to an object identifier (OID) type.
 */
typedef uint32_t						GppcOid;

/**
 * \brief Abstract type for a row descriptor.
 */
typedef struct GppcTupleDescData	   *GppcTupleDesc;

/**
 * \brief set returning function context.
 */
typedef struct GppcFuncCallContextData *GppcFuncCallContext;

/**
 * \brief Abstract type for a row.
 */
typedef struct GppcHeapTupleData	   *GppcHeapTuple;

/**
 * \brief Report callback state.
 * \sa GppcInstallReportCallback(), GppcUninstallReportCallback()
 */
typedef struct GppcReportCallbackStateData   *GppcReportCallbackState;

/****************************************************************************
 * GPPC type oid
 ****************************************************************************/
/**
 * \brief This points none of valid SQL type.
 */
extern const GppcOid GppcOidInvalid;

/**
 * \brief Oid for SQL bool.
 */
extern const GppcOid GppcOidBool;

/**
 * \brief Oid for SQL "char".
 */
extern const GppcOid GppcOidChar;

/**
 * \brief Oid for SQL int2/smallint.
 */
extern const GppcOid GppcOidInt2;

/**
 * \brief Oid for SQL int4/integer.
 */
extern const GppcOid GppcOidInt4;

/**
 * \brief Oid for SQL int8/bigint.
 */
extern const GppcOid GppcOidInt8;

/**
 * \brief Oid for SQL float4/real.
 */
extern const GppcOid GppcOidFloat4;

/**
 * \brief Oid for SQL float8/double precision.
 */
extern const GppcOid GppcOidFloat8;

/**
 * \brief Oid for SQL text.
 */
extern const GppcOid GppcOidText;

/**
 * \brief Oid for SQL varchar.
 */
extern const GppcOid GppcOidVarChar;

/**
 * \brief Oid for SQL char.
 */
extern const GppcOid GppcOidBpChar;

/**
 * \brief Oid for SQL bytea.
 */
extern const GppcOid GppcOidBytea;

/**
 * \brief Oid for SQL numeric.
 */
extern const GppcOid GppcOidNumeric;

/**
 * \brief Oid for SQL date.
 */
extern const GppcOid GppcOidDate;

/**
 * \brief Oid for SQL time (time without time zone).
 */
extern const GppcOid GppcOidTime;

/**
 * \brief Oid for SQL timetz (time with time zone).
 */
extern const GppcOid GppcOidTimeTz;

/**
 * \brief Oid for SQL timestamp (timestamp without time zone).
 */
extern const GppcOid GppcOidTimestamp;

/**
 * \brief Oid for SQL timestamptz (timestamp with time zone).
 */
extern const GppcOid GppcOidTimestampTz;

/**
 * \brief Oid for SQL anytable.
 */
extern const GppcOid GppcOidAnyTable;

/**
 ****************************************************************************
 * \defgroup FunctionControls Basic function control operations
 * @{
 ****************************************************************************
 */
/**
 * \brief Returns number of argument supplied to an SQL invoked function.
 */
#define GPPC_NARGS() (GppcNargs(fcinfo))

/**
 * \brief Terminates an SQL invoked function returning NULL value.
 */
#define GPPC_RETURN_NULL() return GppcReturnNull(fcinfo)

/**
 * \brief Returns true if a given argument to an SQL invoked function was NULL.
 * \param n the argument position starting from 0.
 */
#define GPPC_ARGISNULL(n) (GppcArgIsNull(fcinfo, n))

/****************************************************************************
 * GETARG macros
 ****************************************************************************/
/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcDatum
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_DATUM(n)	(GppcGetArgDatum(fcinfo, n))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcBool
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_BOOL(n)		(GppcDatumGetBool(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcChar
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_CHAR(n)		(GppcDatumGetChar(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcInt2
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_INT2(n)		(GppcDatumGetInt2(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcInt4
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_INT4(n)		(GppcDatumGetInt4(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcInt8
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_INT8(n)		(GppcDatumGetInt8(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcFloat4
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_FLOAT4(n)	(GppcDatumGetFloat4(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcFloat8
 * \param n the argument position starting from 0.
 */
#define GPPC_GETARG_FLOAT8(n)	(GppcDatumGetFloat8(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcText
 * \param n the argument position starting from 0.
 *
 * The returned ::GppcText may have a direct pointer to database content,
 * so do not modify the content.
 */
#define GPPC_GETARG_TEXT(n)		(GppcDatumGetText(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcText
 * \param n the argument position starting from 0.
 *
 * In contrast to GPPC_GETARG_TEXT(), this returns a copy of the argument,
 * so the function can modify its content.
 */
#define GPPC_GETARG_TEXT_COPY(n)	(GppcDatumGetTextCopy(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcVarChar
 * \param n the argument position starting from 0.
 *
 * The returned ::GppcVarChar may have a direct pointer to database content,
 * so do not modify the content.
 */
#define GPPC_GETARG_VARCHAR(n)	(GppcDatumGetVarChar(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcVarChar
 * \param n the argument position starting from 0.
 *
 * In contrast to GPPC_GETARG_VARCHAR(), this returns a copy of the argument,
 * so the function can modify its content.
 */
#define GPPC_GETARG_VARCHAR_COPY(n)	(GppcDatumGetVarCharCopy(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcBpChar
 * \param n the argument position starting from 0.
 *
 * The returned ::GppcBpChar may have a direct pointer to database content,
 * so do not modify the content.
 */
#define GPPC_GETARG_BPCHAR(n)	(GppcDatumGetBpChar(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcBpChar
 * \param n the argument position starting from 0.
 *
 * In contrast to GPPC_GETARG_BPCHAR(), this returns a copy of the argument,
 * so the function can modify its content.
 */
#define GPPC_GETARG_BPCHAR_COPY(n)	(GppcDatumGetBpCharCopy(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcBytea
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_BYTEA(n)	(GppcDatumGetBytea(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcBytea
 * \param n argument position starting from 0.
 *
 * In contrast to GPPC_GETARG_BYTEA(), this returns a copy of the argument,
 * so the function can modify its content.
 */
#define GPPC_GETARG_BYTEA_COPY(n)	(GppcDatumGetByteaCopy(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcNumeric
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_NUMERIC(n)	(GppcDatumGetNumeric(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcDate
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_DATE(n)		(GppcDatumGetDate(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcTime
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_TIME(n)		(GppcDatumGetTime(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcTimeTz
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_TIMETZ(n)	(GppcDatumGetTimeTz(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcTimestamp
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_TIMESTAMP(n)	(GppcDatumGetTimestamp(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcTimestampTz
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_TIMESTAMPTZ(n)	(GppcDatumGetTimestampTz(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an SQL invoked function as ::GppcAnyTable
 * \param n argument position starting from 0.
 */
#define GPPC_GETARG_ANYTABLE(n)		(GppcDatumGetAnyTable(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an internal pointer of tuple descriptor.
 */
#define GPPC_GETARG_TUPLEDESC(n)	(GppcDatumGetTupleDesc(GppcGetArgDatum(fcinfo, n)))

/**
 * \brief Retrieves an argument to an internal pointer of row content.
 */
#define GPPC_GETARG_HEAPTUPLE(n)	(GppcDatumGetHeapTuple(GppcGetArgDatum(fcinfo, n)))

/****************************************************************************
 * RETURN macros
 ****************************************************************************/
/**
 * \brief Terminates an SQL invoked function returning value as any SQL type.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_DATUM(x)	return (x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL bool.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_BOOL(x)		return GppcBoolGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL "char".
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_CHAR(x)		return GppcCharGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL int2.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_INT2(x)		return GppcInt2GetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL int4.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_INT4(x)		return GppcInt4GetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL int8.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_INT8(x)		return GppcInt8GetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL float4.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_FLOAT4(x)	return GppcFloat4GetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL float8.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_FLOAT8(x)	return GppcFloat8GetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL text.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TEXT(x)		return GppcTextGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL varchar.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_VARCHAR(x)	return GppcVarCharGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL char.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_BPCHAR(x)	return GppcBpCharGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL bytea.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_BYTEA(x)	return GppcByteaGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL numeric.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_NUMERIC(x)	return GppcNumericGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL date.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_DATE(x)	return GppcDateGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL time.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TIME(x)	return GppcTimeGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL timetz.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TIMETZ(x)	return GppcTimeTzGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL timestamp.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TIMESTAMP(x)	return GppcTimestampGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL timestamptz.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TIMESTAMPTZ(x)	return GppcTimestampTzGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as SQL anytable.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_ANYTABLE(x)		return GppcAnyTableGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as internal tuple descriptor.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_TUPLEDESC(x)	return GppcTupleDescGetDatum(x)

/**
 * \brief Terminates an SQL invoked function returning value as internal row content.
 * \param x ::GppcDatum to be returned.
 */
#define GPPC_RETURN_HEAPTUPLE(x)	return GppcHeapTupleGetDatum(x)

/**
 * \sa GPPC_FUNCTION_INFO()
 */
const void *GppcFinfoV1(void);

/**
 * \sa GPPC_NARGS()
 */
int GppcNargs(GppcFcinfo info);

/**
 * \sa GPPC_RETURN_NULL()
 */
GppcDatum GppcReturnNull(GppcFcinfo info);

/**
 * \sa GPPC_ARGISNULL()
 */
bool GppcArgIsNull(GppcFcinfo info, int n);

/**
 * \sa GPPC_GETARG_INT4(), etc.
 */
GppcDatum GppcGetArgDatum(GppcFcinfo info, int n);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup DatumConv Conversion from/to GppcDatum
 * @{
 ****************************************************************************
 */
/****************************************************************************
 * Conversion to GppcDatum
 ****************************************************************************/
/**
 * \brief Converts ::GppcBool to ::GppcDatum
 * \param x ::GppcBool to be converted to ::GppcDatum
 */
GppcDatum		GppcBoolGetDatum(GppcBool x);

/**
 * \brief Converts ::GppcChar to ::GppcDatum
 * \param x ::GppcChar to be converted to ::GppcDatum
 */
GppcDatum		GppcCharGetDatum(GppcChar x);

/**
 * \brief Converts ::GppcInt2 to ::GppcDatum
 * \param x ::GppcInt2 to be converted to ::GppcDatum
 */
GppcDatum		GppcInt2GetDatum(GppcInt2 x);

/**
 * \brief Converts ::GppcInt4 to ::GppcDatum
 * \param x ::GppcInt4 to be converted to ::GppcDatum
 */
GppcDatum		GppcInt4GetDatum(GppcInt4 x);

/**
 * \brief Converts ::GppcInt8 to ::GppcDatum
 * \param x ::GppcInt8 to be converted to ::GppcDatum
 */
GppcDatum		GppcInt8GetDatum(GppcInt8 x);

/**
 * \brief Converts ::GppcFloat4 to ::GppcDatum
 * \param x ::GppcFloat4 to be converted to ::GppcDatum
 */
GppcDatum		GppcFloat4GetDatum(GppcFloat4 x);

/**
 * \brief Converts ::GppcFloat8 to ::GppcDatum
 * \param x ::GppcFloat8 to be converted to ::GppcDatum
 */
GppcDatum		GppcFloat8GetDatum(GppcFloat8 x);

/**
 * \brief Converts ::GppcText to ::GppcDatum
 * \param x ::GppcText to be converted to ::GppcDatum
 */
GppcDatum		GppcTextGetDatum(GppcText x);

/**
 * \brief Converts ::GppcVarChar to ::GppcDatum
 * \param x ::GppcVarChar to be converted to ::GppcDatum
 */
GppcDatum		GppcVarCharGetDatum(GppcVarChar x);

/**
 * \brief Converts ::GppcBpChar to ::GppcDatum
 * \param x ::GppcBpChar to be converted to ::GppcDatum
 */
GppcDatum		GppcBpCharGetDatum(GppcBpChar x);

/**
 * \brief Converts ::GppcBytea to ::GppcDatum
 * \param x ::GppcBytea to be converted to ::GppcDatum
 */
GppcDatum		GppcByteaGetDatum(GppcBytea x);

/**
 * \brief Converts ::GppcNumeric to ::GppcDatum
 * \param x ::GppcNumeric to be converted to ::GppcDatum
 */
GppcDatum		GppcNumericGetDatum(GppcNumeric x);

/**
 * \brief Converts ::GppcDate to ::GppcDatum
 * \param x ::GppcDate to be converted to ::GppcDatum
 */
GppcDatum		GppcDateGetDatum(GppcDate x);

/**
 * \brief Converts ::GppcTime to ::GppcDatum
 * \param x ::GppcTime to be converted to ::GppcDatum
 */
GppcDatum		GppcTimeGetDatum(GppcTime x);

/**
 * \brief Converts ::GppcTimeTz to ::GppcDatum
 * \param x ::GppcTimeTz to be converted to ::GppcDatum
 */
GppcDatum		GppcTimeTzGetDatum(GppcTimeTz x);

/**
 * \brief Converts ::GppcTimestamp to ::GppcDatum
 * \param x ::GppcTimestamp to be converted to ::GppcDatum
 */
GppcDatum		GppcTimestampGetDatum(GppcTimestamp x);

/**
 * \brief Converts ::GppcTimestampTz to ::GppcDatum
 * \param x ::GppcTimestampTz to be converted to ::GppcDatum
 */
GppcDatum		GppcTimestampTzGetDatum(GppcTimestampTz x);

/**
 * \brief Converts ::GppcAnyTable to ::GppcDatum
 * \param x ::GppcAnyTable to be converted to ::GppcDatum
 */
GppcDatum		GppcAnyTableGetDatum(GppcAnyTable x);

/**
 * \brief Converts ::GppcTupleDesc to ::GppcDatum
 * \param x ::GppcTupleDesc to be converted to ::GppcDatum
 */
GppcDatum		GppcTupleDescGetDatum(GppcTupleDesc x);

/**
 * \brief Converts ::GppcHeapTuple to ::GppcDatum
 * \param x ::GppcHeapTuple to be converted to ::GppcDatum
 */
GppcDatum		GppcHeapTupleGetDatum(GppcHeapTuple x);

/****************************************************************************
 * Conversion from GppcDatum
 ****************************************************************************/
/**
 * \brief Converts ::GppcDatum to ::GppcBool
 * \param x ::GppcDatum to be converted to ::GppcBool
 */
GppcBool		GppcDatumGetBool(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcChar
 * \param x ::GppcDatum to be converted to ::GppcChar
 */
GppcChar		GppcDatumGetChar(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcInt2
 * \param x ::GppcDatum to be converted to ::GppcInt2
 */
GppcInt2		GppcDatumGetInt2(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcInt4
 * \param x ::GppcDatum to be converted to ::GppcInt4
 */
GppcInt4		GppcDatumGetInt4(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcInt8
 * \param x ::GppcDatum to be converted to ::GppcInt8
 */
GppcInt8		GppcDatumGetInt8(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcFloat4
 * \param x ::GppcDatum to be converted to ::GppcFloat4
 */
GppcFloat4		GppcDatumGetFloat4(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcFloat8
 * \param x ::GppcDatum to be converted to ::GppcFloat8
 */
GppcFloat8		GppcDatumGetFloat8(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcText
 * \param x ::GppcDatum to be converted to ::GppcText
 */
GppcText		GppcDatumGetText(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcText
 * \param x ::GppcDatum to be converted and copied to ::GppcText
 */
GppcText		GppcDatumGetTextCopy(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcVarChar
 * \param x ::GppcDatum to be converted to ::GppcVarChar
 */
GppcVarChar		GppcDatumGetVarChar(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcVarChar
 * \param x ::GppcDatum to be converted and copied to ::GppcVarChar
 */
GppcVarChar		GppcDatumGetVarCharCopy(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcBpChar
 * \param x ::GppcDatum to be converted to ::GppcBpChar
 */
GppcBpChar		GppcDatumGetBpChar(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcBpChar
 * \param x ::GppcDatum to be converted and copied to ::GppcBpChar
 */
GppcBpChar		GppcDatumGetBpCharCopy(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcBytea
 * \param x ::GppcDatum to be converted to ::GppcBytea
 */
GppcBytea		GppcDatumGetBytea(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcBytea
 * \param x ::GppcDatum to be converted and copied to ::GppcBytea
 */
GppcBytea		GppcDatumGetByteaCopy(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcNumeric
 * \param x ::GppcDatum to be converted to ::GppcNumeric
 */
GppcNumeric		GppcDatumGetNumeric(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcDate
 * \param x ::GppcDatum to be converted to ::GppcDate
 */
GppcDate		GppcDatumGetDate(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcTime
 * \param x ::GppcDatum to be converted to ::GppcTime
 */
GppcTime		GppcDatumGetTime(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcTimeTz
 * \param x ::GppcDatum to be converted to ::GppcTimeTz
 */
GppcTimeTz		GppcDatumGetTimeTz(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcTimestamp
 * \param x ::GppcDatum to be converted to ::GppcTimestamp
 */
GppcTimestamp	GppcDatumGetTimestamp(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcTimestampTz
 * \param x ::GppcDatum to be converted to ::GppcTimestampTz
 */
GppcTimestampTz	GppcDatumGetTimestampTz(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcAnyTable
 * \param x ::GppcDatum to be converted to ::GppcAnyTable
 */
GppcAnyTable	GppcDatumGetAnyTable(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcTupleDesc
 * \param x ::GppcDatum to be converted to ::GppcTupleDesc
 */
GppcTupleDesc	GppcDatumGetTupleDesc(GppcDatum x);

/**
 * \brief Converts ::GppcDatum to ::GppcHeapTuple
 * \param x ::GppcDatum to be converted to ::GppcHeapTuple
 */
GppcHeapTuple	GppcDatumGetHeapTuple(GppcDatum x);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup TextOps Text operations
 * @{
 ****************************************************************************
 */
/**
 * \brief Returns byte size of this character string.
 * \param t ::GppcText holding character string.
 */
size_t GppcGetTextLength(GppcText t);

/**
 * \brief Returns the pointer to the head of character string.
 * \param t ::GppcText holding character string.
 *
 * Note the string is not null-terminated.  If you want a
 * null-terminated string, use GppcTextGetCString().  Also,
 * the memory pointed by this pointer may be the actual database
 * content.  Do not modify the memory content.
 */
char *GppcGetTextPointer(GppcText t);

/**
 * \brief Builds a null-terminated character string from ::GppcText.
 * \param t the original GppcText to build string from.
 *
 * The returned string is always copied locally and is therefore
 * mutable.
 */
char *GppcTextGetCString(GppcText t);

/**
 * \brief Builds ::GppcText from a null-terminated character string.
 * \param s character string to build ::GppcText from.
 *
 * This copies content from s and builds ::GppcText which can be returned
 * to the database.
 */
GppcText GppcCStringGetText(const char *s);

/**
 * \brief Returns byte size of this character string.
 * \param t ::GppcVarChar holding character string.
 */
size_t GppcGetVarCharLength(GppcVarChar t);

/**
 * \brief Returns the pointer to the head of character string.
 * \param t ::GppcVarChar holding character string.
 *
 * Note the string is not null-terminated.  If you want a
 * null-terminated string, use GppcVarCharGetCString().  Also,
 * the memory pointed by this pointer may be the actual database
 * content.  Do not modify the memory content.
 */
char *GppcGetVarCharPointer(GppcVarChar t);

/**
 * \brief Builds a null-terminated character string from ::GppcVarChar.
 * \param t the original GppcVarChar to build string from.
 *
 * The returned string is always copied locally hence it's mutable.
 */
char *GppcVarCharGetCString(GppcVarChar t);

/**
 * \brief Builds ::GppcVarChar from a null-terminated character string.
 * \param s character string to build ::GppcVarChar from.
 *
 * This copies content from s and builds ::GppcVarChar which can be returned
 * to the database.
 */
GppcVarChar GppcCStringGetVarChar(const char *s);

/**
 * \brief Returns byte size of this character string.
 * \param t ::GppcBpChar holding character string.
 */
size_t GppcGetBpCharLength(GppcBpChar t);

/**
 * \brief Returns the pointer to the head of character string.
 * \param t ::GppcBpChar holding character string.
 *
 * Note the string is not null-terminated.  If you want a
 * null-terminated string, use GppcBpCharGetCString().  Also,
 * the memory pointed by this pointer may be the actual database
 * content.  Do not modify the memory content.
 */
char *GppcGetBpCharPointer(GppcBpChar t);

/**
 * \brief Builds a null-terminated character string from ::GppcBpChar.
 * \param t the original GppcBpChar to build string from.
 *
 * The returned string is always copied locally hence it's mutable.
 */
char *GppcBpCharGetCString(GppcBpChar t);

/**
 * \brief Builds ::GppcBpChar from a null-terminated character string.
 * \param s character string to build ::GppcBpChar from.
 *
 * This copies content from s and builds ::GppcBpChar which can be returned
 * to the database.
 */
GppcBpChar GppcCStringGetBpChar(const char *s);

/**
 * \brief Returns byte size of this byte array.
 * \param x ::GppcBytea holding byte array.
 */
size_t GppcGetByteaLength(GppcBytea x);

/**
 * \brief Returns the pointer to the head of byte array.
 * \param x ::GppcBytea holding byte array.
 *
 * The memory pointed by this pointer may be the actual database
 * content.  Do not modify the memory content.
 */
char *GppcGetByteaPointer(GppcBytea x);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup NumericOps Numeric operations
 * @{
 ****************************************************************************
 */
/**
 * \brief Returns a null-terminated string that represents this deciaml value.
 * \param n ::GppcNumeric to be printed.
 */
char *GppcNumericGetCString(GppcNumeric n);

/**
 * \brief Returns ::GppcNumeric that the input represents.
 * \param s null-terminated string to be converted to ::GppcNumeric.
 *
 * This is the counter-operation of GppcNumericGetCString().
 */
GppcNumeric GppcCStringGetNumeric(const char *s);

/**
 * \brief Given 32 bit integer, interprets it into 16 bit integer precision and
 * scale.
 * \param typmod the type modifier of the numeric column.
 * \param precision the pointer to the 16 bit integer to set the precision value.
 * \param scale the pointer to the 16 bit integer to set the scale value.
 *
 * The precision is the total count of significant digits in the whole
 * number, and the scale is the count of decimal digits in the fraction part.
 * Returns true if the type modifier contains the precision and scale values,
 * and false if the type modifier doesn't.
 */
bool GppcGetNumericDef(int32_t typmod, int16_t *precision, int16_t *scale);

/**
 * \brief Returns a ::GppcFloat8 value down-cast from a ::GppcNumeric.
 * \param n a ::GppcNumeric value to down-cast to a ::GppcFloat8 value.
 */
GppcFloat8 GppcNumericGetFloat8(GppcNumeric n);

/**
 * \brief Returns a ::GppcNumeric value cast from a ::GppcFloat8.
 * \param f a ::GppcFloat8 value to cast to a ::GppcNumeric value.
 */
GppcNumeric GppcFloat8GetNumeric(GppcFloat8 f);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup TimestampOps Timestamp operations
 * @{
 ****************************************************************************
 */
/**
 * \brief Extracts information from ::GppcDate to ::GppcTm
 * \param x input ::GppcDate.
 * \param tm output ::GppcTm.
 *
 * This function sets only date related fields of ::GppcTm.
 */
void GppcDateGetTm(GppcDate x, GppcTm *tm);

/**
 * \brief Generates ::GppcDate from ::GppcTm.
 * \param tm source to generate ::GppcDate.
 */
GppcDate GppcTmGetDate(GppcTm *tm);

/**
 * \brief Extracts information from ::GppcTime to ::GppcTm
 * \param x input ::GppcTime.
 * \param tm output ::GppcTm.
 *
 * This function sets only time related fields of ::GppcTm.
 */
void GppcTimeGetTm(GppcTime x, GppcTm *tm);

/**
 * \brief Generates ::GppcTime from ::GppcTm.
 * \param tm source to generate ::GppcTime.
 */
GppcTime GppcTmGetTime(GppcTm *tm);

/**
 * \brief Extracts information from ::GppcTimeTz to ::GppcTm
 * \param x input ::GppcTimeTz.
 * \param tm output ::GppcTm.
 *
 * This function sets only time related fields of ::GppcTm.
 */
void GppcTimeTzGetTm(GppcTimeTz x, GppcTm *tm);

/**
 * \brief Generates ::GppcTimeTz from ::GppcTm.
 * \param tm source to generate ::GppcTimeTz.
 */
GppcTimeTz GppcTmGetTimeTz(GppcTm *tm);

/**
 * \brief Extracts information from ::GppcTimestamp to ::GppcTm
 * \param x input ::GppcTimestamp.
 * \param tm output ::GppcTm.
 */
void GppcTimestampGetTm(GppcTimestamp x, GppcTm *tm);

/**
 * \brief Generates ::GppcTimestamp from ::GppcTm.
 * \param tm source to generate ::GppcTimestamp.
 */
GppcTimestamp GppcTmGetTimestamp(GppcTm *tm);

/**
 * \brief Extracts information from ::GppcTimestampTz to ::GppcTm
 * \param x input ::GppcTimestampTz.
 * \param tm output ::GppcTm.
 */
void GppcTimestampTzGetTm(GppcTimestampTz x, GppcTm *tm);

/**
 * \brief Generates ::GppcTimestampTz from ::GppcTm.
 * \param tm source to generate ::GppcTimestampTz.
 */
GppcTimestampTz GppcTmGetTimestampTz(GppcTm *tm);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup Memory Memory operations
 *   Allocating, freeing memory chunk with the database memory pool, which
 *   is called <em> memory context </em>.  A memory context is pooled in the
 *   database backend and survive in certain life time.  After its life time
 *   is finished, the backend will delete all contents in the context.
 *   Typically a SQL-invoked function is in <em> per tuple context </em>,
 *   which is created and deleted everytime the backend processes a row.
 *   This means a SQL-invoked function should not assume the memory content
 *   allocated in the current memory context is alive across multiple function
 *   calls.
 * @{
 ****************************************************************************
 */
/**
 * \brief Memory context handle
 */
typedef void *GppcMemoryContext;

/**
 * \brief Allocates memory chunk in the current memory context.
 * \param bytes byte size to be allocated.
 *
 * The allocated pointer can be freed by GppcFree() if it's not needed anymore,
 * but the database will free it after this context finishes.  The memory
 * content is uninitialized.  If the database cannot allocate memory,
 * the function will raise an error.  Thus, the caller doesn't need to check
 * NULL.
 */
void *GppcAlloc(size_t bytes);

/**
 * \brief Allocates a memory chunk in the current memory context and
 * initializes it.
 * \param bytes byte size to be allocated.
 *
 * The only difference from GppcAlloc() is the returned memory is initialized
 * to zero.
 */
void *GppcAlloc0(size_t bytes);

/**
 * \brief Resizes the pre-allocated memory.
 * \param ptr pointer to the pre-allocated memory.
 * \param bytes byte size to be allocated.
 *
 * The returned pointer may be the same as input parameter, if the database
 * finds enough space after the pre-allocated memory.  Otherwise it allocates
 * another area and copy from the input.  If the database cannot allocate memory
 * this raises error.  Thus, the caller doesn't need to check NULL.
 */
void *GppcRealloc(void *ptr, size_t bytes);

/**
 * \brief Frees memory allocated by GppcAlloc(), GppcAlloc0() or GppcRealloc().
 * \param ptr pointer to pre-allocated memory.
 *
 * The memory allocated in the current memory context may or may not be freed by
 * this function.  The database will free it later anyway.
 */
void GppcFree(void *ptr);

/**
 * \brief Creates a new ::GppcText instance.
 * \param clen character string length in bytes the instance will have.
 *
 * The returned ::GppcText is allocated in the current memory context.
 */
GppcText GppcAllocText(size_t clen);

/**
 * \brief Creates a new ::GppcVarChar instance.
 * \param clen character string length in bytes the instance will have.
 *
 * The returned ::GppcVarChar is allocated in the current memory context.
 */
GppcVarChar GppcAllocVarChar(size_t clen);

/**
 * \brief Creates a new ::GppcBpChar instance.
 * \param clen character string length in bytes the instance will have.
 *
 * The returned ::GppcBpChar is allocated in the current memory context.
 */
GppcBpChar GppcAllocBpChar(size_t clen);

/**
 * \brief Creates a new ::GppcBytea instance.
 * \param blen character string length in bytes the instance will have.
 *
 * The returned ::GppcBytea is allocated in the current memory context.
 */
GppcBytea GppcAllocBytea(size_t blen);

/**
 * \brief Returns the current memory context.
 *
 * \since 1.2
 */
GppcMemoryContext GppcGetCurrentMemoryContext(void);

/**
 * \brief Creats a new ::GppcMemoryContext.
 * \param parent the parent context to create a new context in.
 *
 * This returns a newly-created memory context that the caller can
 * switch to by ::GppcMemoryContextSwitchTo.  The context will be created
 * within the \a parent context.
 *
 * \since 1.2
 */
GppcMemoryContext GppcMemoryContextCreate(GppcMemoryContext parent);

/**
 * \brief Switches the current memory context to \a context.
 * \param context memory context to be a new current context
 *
 * After switching the context, this returns the old memory context.
 * Make sure the old context is restored after the caller finishes
 * the work within this context.
 *
 * \since 1.2
 */
GppcMemoryContext GppcMemoryContextSwitchTo(GppcMemoryContext context);

/**
 * \brief Resets memory allocated within the \a context.
 * \param context the context to be reset
 *
 * The pointers allocated by ::GppcAlloc family are freed by this call.
 *
 * \since 1.2
 */
void GppcMemoryContextReset(GppcMemoryContext context);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup SRF Set Returning Function
 *   These provide capability to return set of rows from a SQL invoked
 *   function, including table functions.
 * @{
 ****************************************************************************
 */
/**
 * \brief Returns true if this SRF call is the first.
 *
 * The function should initialize the context by GPPC_SRF_FIRSTCALL_INIT()
 * if this is the first call.
 */
#define GPPC_SRF_IS_FIRSTCALL() GppcSRFIsFirstCall(fcinfo)

/**
 * \brief Initializes SRF context.
 */
#define GPPC_SRF_FIRSTCALL_INIT() GppcSRFFirstCallInit(fcinfo)

/**
 * \brief Returns stored SRF context.
 *
 * The SRF needs to restore the context every time it gets called.
 */
#define GPPC_SRF_PERCALL_SETUP() GppcSRFPerCallSetup(fcinfo)

/**
 * \brief Returns the value from this SRF, continuing to be called.
 * \param funcctx SRF context that is returned by GPPC_SRF_PERCALL_SETUP().
 * \param result ::GppcDatum to be returned from this function.
 */
#define GPPC_SRF_RETURN_NEXT(funcctx, result) \
	return GppcSRFReturnNext(fcinfo, funcctx, result)

/**
 * \brief Returns nothing from this SRF, terminating to be called.
 * \param funcctx SRF context that is returned by GPPC_SRF_PERCALL_SETUP().
 */
#define GPPC_SRF_RETURN_DONE(funcctx) return GppcSRFReturnDone(fcinfo, funcctx)

/**
 * \brief Returns ::GppcTupleDesc that describes the expected row of this SRF.
 *
 * Note that in case you set up the describe function in table functions, the
 * tuple descriptor should equal to the descriptor returned by the describe function.
 */
#define GPPC_SRF_RESULT_DESC() (GppcSRFResultDesc(fcinfo))

/**
 * \sa GPPC_SRF_IS_FIRSTCALL()
 */
bool GppcSRFIsFirstCall(GppcFcinfo info);

/**
 * \sa GPPC_SRF_FIRSTCALL_INIT()
 */
GppcFuncCallContext GppcSRFFirstCallInit(GppcFcinfo info);

/**
 * \sa GPPC_SRF_PERCALL_SETUP()
 */
GppcFuncCallContext GppcSRFPerCallSetup(GppcFcinfo info);

/**
 * \sa GPPC_SRF_RETURN_NEXT()
 */
GppcDatum GppcSRFReturnNext(GppcFcinfo info, GppcFuncCallContext funcctx, GppcDatum result);

/**
 * \sa GPPC_SRF_RETURN_DONE()
 */
GppcDatum GppcSRFReturnDone(GppcFcinfo info, GppcFuncCallContext funcctx);

/**
 * \sa GPPC_SRF_RESULT_DESC()
 */
GppcTupleDesc GppcSRFResultDesc(GppcFcinfo info);

/**
 * \brief Allocates memory in SRF context.
 * \param fctx SRF function context.
 * \param bytes byte size to be allocated.
 *
 * Memory allocated in the SRF context survive across multiple function calls
 * of SRF.  Use this instead of GppcAlloc() if an SRF function needs data
 * to live longer than a single function call.
 */
void *GppcSRFAlloc(GppcFuncCallContext fctx, size_t bytes);

/**
 * \brief Allocates memory in SRF context and initializes it.
 * \param fctx SRF function context.
 * \param bytes byte size to be allocated.
 *
 * The only difference from GppcSRFAlloc() is the returned memory is initialized
 * to zero.
 */
void *GppcSRFAlloc0(GppcFuncCallContext fctx, size_t bytes);

/**
 * \brief Saves user state in SRF context.
 * \param fctx SRF function context.
 * \param ptr arbitrary pointer to be saved.
 */
void GppcSRFSave(GppcFuncCallContext fctx, void *ptr);

/**
 * \brief Restores user state in SRF context.
 * \param fctx SRF function context.
 */
void *GppcSRFRestore(GppcFuncCallContext fctx);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup SPI Server Programming Interface
 *   This interface is used to issue a SQL query to the database
 *   from a SQL invoked function.
 * @{
 ****************************************************************************
 */
/**
 * \brief Connects to SPI.
 *
 * If successful, this returns non-negative value.  Note that the database
 * memory context is changed by this call, and everything allocated after
 * it will be deallocated by GppcSPIFinish(), unless explicitly copied to
 * the upper context.  To do explicit copy, pass true \a makecopy parameters
 * where possible, such like GppcSPIGetValue() and GppcSPIGetDatum().
 *
 * \sa GppcSPIFinish()
 */
int GppcSPIConnect(void);

/**
 * \brief Disconnects from SPI.
 *
 * If successful, this returns non-negative value.  Disconnection from SPI clears
 * all the memory allocated in between GppcSPIConnect() and GppcSPIFinish().
 *
 * \sa GppcSPIConnect()
 */
int GppcSPIFinish(void);

/**
 * \brief Executes SQL statement.
 * \param src SQL statement to be issued.
 * \param tcount maximum number of rows to be returned, or 0 to return all the rows.
 */
GppcSPIResult GppcSPIExec(const char *src, long tcount);

/**
 * \brief Retrieves an SQL result attribute as a character string.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fnumber attribute number to extract, starting from 1.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 */
char *GppcSPIGetValue(GppcSPIResult result, int fnumber, bool makecopy);

/**
 * \brief Retrieves an SQL result attribute as a ::GppcDatum.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fnumber attribute number to extract, starting from 1.
 * \param isnull to be set true if the returned value is SQL NULL.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 */
GppcDatum GppcSPIGetDatum(GppcSPIResult result, int fnumber, bool *isnull, bool makecopy);

/**
 * \brief Retrieves an SQL result attribute as a character string.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fname attribute name to extract.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 *
 * Note that GppcSPIGetValue() is faster than this function.
 */
char *GppcSPIGetValueByName(GppcSPIResult result, const char *fname, bool makecopy);

/**
 * \brief Retrieves an SQL result attribute as a ::GppcDatum.
 * \param result ::GppcSPIResult holding the SQL result.
 * \param fname attribute name to extract.
 * \param isnull to be set true if the returned value is SQL NULL.
 * \param makecopy true if the caller wants to keep the result out of SPI memory context.
 *
 * Note that GppcSPIGetDatum() is faster than this function.
 */
GppcDatum GppcSPIGetDatumByName(GppcSPIResult result, const char *fname, bool *isnull, bool makecopy);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup TableFunctions Table Functions
 *   These are used to interact with table functions (including describe
 *   functions.)
 * @{
 */
/**
 * \brief Returns the tuple descriptor of the input subquery in the describe function.
 * \param n position of TABLE argument to the table function, starting from 0.
 * \param iserror bool pointer to be set true if called in wrong context.
 */
#define GPPC_TF_INPUT_DESC(n, iserror) GppcTFInputDesc(fcinfo, n, iserror);

/**
 * \brief Returns argument as ::GppcBool in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_BOOL(n, isnull, iserror) \
	GppcDatumGetBool(GppcTFGetArgDatum(fcinfo, GppcOidBool, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcChar in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_CHAR(n, isnull, iserror) \
	GppcDatumGetChar(GppcTFGetArgDatum(fcinfo, GppcOidChar, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcInt2 in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_INT2(n, isnull, iserror) \
	GppcDatumGetInt2(GppcTFGetArgDatum(fcinfo, GppcOidInt2, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcInt4 in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_INT4(n, isnull, iserror) \
	GppcDatumGetInt4(GppcTFGetArgDatum(fcinfo, GppcOidInt4, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcInt8 in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_INT8(n, isnull, iserror) \
	GppcDatumGetInt8(GppcTFGetArgDatum(fcinfo, GppcOidInt8, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcFloat4 in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_FLOAT4(n, isnull, iserror) \
	GppcDatumGetFloat4(GppcTFGetArgDatum(fcinfo, GppcOidFloat4, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcFloat4 in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_FLOAT8(n, isnull, iserror) \
	GppcDatumGetFloat8(GppcTFGetArgDatum(fcinfo, GppcOidFloat8, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcText in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_TEXT(n, isnull, iserror) \
	GppcDatumGetText(GppcTFGetArgDatum(fcinfo, GppcOidText, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcVarChar in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_VARCHAR(n, isnull, iserror) \
	GppcDatumGetVarChar(GppcTFGetArgDatum(fcinfo, GppcOidVarChar, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcBpChar in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_BPCHAR(n, isnull, iserror) \
	GppcDatumGetBpChar(GppcTFGetArgDatum(fcinfo, GppcOidBpChar, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcBytea in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_BYTEA(n, isnull, iserror) \
	GppcDatumGetBytea(GppcTFGetArgDatum(fcinfo, GppcOidBytea, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcNumeric in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 1 \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_NUMERIC(n, isnull, iserror) \
	GppcDatumGetNumeric(GppcTFGetArgDatum(fcinfo, GppcOidNumeric, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcDate in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_DATE(n, isnull, iserror) \
	GppcDatumGetDate(GppcTFGetArgDatum(fcinfo, GppcOidDate, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcTime in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_TIME(n, isnull, iserror) \
	GppcDatumGetTime(GppcTFGetArgDatum(fcinfo, GppcOidTime, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcTimeTz in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_TIMETZ(n, isnull, iserror) \
	GppcDatumGetTimeTz(GppcTFGetArgDatum(fcinfo, GppcOidTimeTz, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcTimestamp in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_TIMESTAMP(n, isnull, iserror) \
	GppcDatumGetTimestamp(GppcTFGetArgDatum(fcinfo, GppcOidTimestamp, n, isnull, iserror))

/**
 * \brief Returns argument as ::GppcTimestampTz in the describe function.
 * \param n position of argument, starting from 0.
 * \param isnull bool pointer to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * \sa GppcTFGetArgDatum()
 */
#define GPPC_TF_GETARG_TIMESTAMPTZ(n, isnull, iserror) \
	GppcDatumGetTimestampTz(GppcTFGetArgDatum(fcinfo, GppcOidTimestampTz, n, isnull, iserror))

/**
 * \brief Passes arbitrary user data to be dispatched, in the describe function.
 * \param d arbitrary user data as ::GppcBytea
 */
#define GPPC_TF_SET_USERDATA(d) (GppcTFSetUserData(fcinfo, d))

/**
 * \brief Gets the user data passed by the describe function.
 */
#define GPPC_TF_GET_USERDATA() (GppcTFGetUserData(fcinfo))

/**
 * \sa GPPC_TF_SET_USERDATA()
 */
void GppcTFSetUserData(void *GppcFcinfo, GppcBytea userdata);

/**
 * \sa GPPC_TF_GET_USERDATA()
 */
GppcBytea GppcTFGetUserData(void *GppcFcinfo);

/**
 * \brief Returns the tuple descriptor from AnyTable argument.
 * \param t the TABLE query argument passed into the table function.
 *
 * This returns the same tuple descriptor as GPPC_TF_INPUT_DESC() does in the
 * describe function, but this should be called in the project function.
 */
GppcTupleDesc GppcAnyTableGetTupleDesc(GppcAnyTable t);

/**
 * \brief Fetches the next row content from TABLE query.
 * \param t the TABLE query argument passed into the table function.
 *
 * Returns NULL if there is no more row.
 */
GppcHeapTuple GppcAnyTableGetNextTuple(GppcAnyTable t);

/**
 * \brief Returns argument as ::GppcDatum in the describe function.
 * \param info function call info.
 * \param typid the expected type id.
 * \param argno position of argument, starting from 0.
 * \param isnull to be set true if the argument is null.
 * \param iserror bool pointer to be set true if called in wrong context.
 *
 * The argument \a typid can be GppcOidInvalid, in case the caller doesn't care
 * about type check.  iserror will be set true if the expected type does
 * not match the actual incoming expression (which means the function
 * declaration may be wrong, or the argument position may be wrong,) or if
 * the argument position is out of range, or just this gets called by other
 * functions than the describe function.  If this kind of mismatch happens,
 * this function emits the reason message as DEBUG1 log.  Consult the
 * database log file by setting message level to DEBUG1 if you need to
 * investigate the reason.
 */
GppcDatum GppcTFGetArgDatum(GppcFcinfo info, GppcOid typid, int argno,
								bool *isnull, bool *iserror);

/**
 * \sa GPPC_TF_INPUT_DESC()
 */
GppcTupleDesc GppcTFInputDesc(GppcFcinfo info, int argno, bool *iserror);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup Reports Reports
 *   This provides error reporting to the client and logging.
 * @{
 ****************************************************************************
 */
/**
 * \brief Report level used in GppcReport().
 * \sa Greenplum Administrator Guide
 */
typedef enum GppcReportLevel
{
	GPPC_DEBUG1				= 10,
	GPPC_DEBUG2				= 11,
	GPPC_DEBUG3				= 12,
	GPPC_DEBUG4				= 13,
	GPPC_DEBUG				= 14,
	GPPC_LOG				= 15,
	GPPC_INFO				= 17,
	GPPC_NOTICE				= 18,
	GPPC_WARNING			= 19,
	GPPC_ERROR				= 20,
} GppcReportLevel;

#ifndef PG_PRINTF_ATTRIBUTE
#ifdef WIN32
#define PG_PRINTF_ATTRIBUTE gnu_printf
#else
#define PG_PRINTF_ATTRIBUTE printf
#endif
#endif

/**
 * \brief Emits a formatted message and may raise error.
 * \param elevel reporting level.
 * \param fmt format text.
 *
 * This function takes variadic arguments just as printf to format a string
 * and emit it to the frontend and/or log file, depending on the reporting
 * level and session configuration parameters such as client_min_messages.
 */
void GppcReport(GppcReportLevel elevel, const char *fmt, ...)
__attribute__((format(PG_PRINTF_ATTRIBUTE, 2, 3)));

/**
 * \brief Installs report callback function which is called on any report.
 * \param func callback function
 * \param arg arbitrary data pointer which will be passed to the callback
 *
 * The callback signature is
 * \code
 *   void callback_function(GppcReportInfo info, void *arg);
 * \endcode
 * The first argument is the report infomation and the second argument is
 * arbitrary pointer that you pass to GppcInstallReportCallback().  This
 * callback will be called in any report event including INFO, NOTICE, etc.
 * In case ERROR occurs, the callback will be flushed automatically,
 * but otherwise it is the caller's responsibility to uninstall the
 * callback by calling GppcUninstallReportCallback().
 *
 * \sa GppcUninstallReportCallback(), GppcGetReportLevel(), GppcGetReportMessage()
 */
GppcReportCallbackState GppcInstallReportCallback(
		void (*func)(GppcReportInfo, void *), void *arg);

/**
 * \brief Uninstalls report callback installed by GppcInstallReportCallback().
 * \sa GppcInstallReportCallback()
 */
void GppcUninstallReportCallback(GppcReportCallbackState cbstate);

/**
 * \brief Retrieves report level from the report information.
 * \param info report information given to report callbacks
 */
GppcReportLevel
GppcGetReportLevel(GppcReportInfo info);

/**
 * \brief Retrieves report message from the report information.
 * \param info report information given to report callbacks
 */
const char *
GppcGetReportMessage(GppcReportInfo info);

/**
 * \brief Errors out if an interrupt is pending.
 *
 * \since 1.2
 */
void GppcCheckForInterrupts(void);

/**
 * @}
 */

/**
 ****************************************************************************
 * \defgroup Misc Miscellaneous features
 * @{
 ****************************************************************************
 */
/**
 * \brief Creates an empty tuple descriptor.
 * \param natts number of attributes that is contained in this descriptor.
 *
 * \sa GppcTupleDescInitEntry()
 * The caller will need to fill each attribute by calling GppcTupleDescInitEntry().
 */
GppcTupleDesc GppcCreateTemplateTupleDesc(int natts);

/**
 * \brief Fills an attribute in the tuple descriptor.
 * \param desc tuple descriptor to be filled in.
 * \param attno position to be filled, starting from 1.
 * \param attname attribute name.
 * \param typid attribute type oid.
 * \param typmod attribute type modifier.  Set -1 if unknown.
 */
void GppcTupleDescInitEntry(GppcTupleDesc desc, uint16_t attno,
			const char *attname, GppcOid typid, int32_t typmod);

/**
 * \brief Forms a tuple with an array of datums.
 * \param tupdesc descriptor of tuple to be formed.
 * \param values an array of datums.
 * \param nulls an array of bool that indicates SQL NULLs.
 *
 * The number of elements should match the one in tupdesc.  Use
 * GppcBuildHeapTupleDatum() to make it ::GppcDatum, which is convenient
 * to be returned from functions.
 */
GppcHeapTuple GppcHeapFormTuple(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls);

/**
 * \brief Forms a tuple with an array of datums and makes it ::GppcDatum
 * \param tupdesc descriptor of tuple to be formed.
 * \param values an array of datums.
 * \param nulls an array of bool that indicates SQL NULLs.
 *
 * \sa GppcHeapFormTuple()
 */
GppcDatum GppcBuildHeapTupleDatum(GppcTupleDesc tupdesc, GppcDatum *values, bool *nulls);

/**
 * \brief Fetches an attribute from tuple.
 * \param tuple the tuple containing the requested attribute.
 * \param attname attribute name.
 * \param isnull bool pointer to be set true if the attribute is SQL NULL.
 *
 * \sa GppcGetAttributeByNum.
 */
GppcDatum GppcGetAttributeByName(GppcHeapTuple tuple, const char *attname, bool *isnull);

/**
 * \brief Fetches an attribute from tuple.
 * \param tuple the tuple containing the requested attribute.
 * \param attno attribute position, starting from 1.
 * \param isnull bool pointer to be set true if the attribute is SQL NULL.
 *
 * \sa GppcGetAttributeByName.
 */
GppcDatum GppcGetAttributeByNum(GppcHeapTuple tuple, int16_t attno, bool *isnull);

/**
 * \brief Returns the number of attributes of this tuple descriptor.
 * \param tupdesc the tuple descriptor.
 */
int GppcTupleDescNattrs(GppcTupleDesc tupdesc);

/**
 * \brief Returns the attribute name in the descriptor.
 * \param tupdesc the tuple descriptor.
 * \param attno attribute position, starting from 0.
 */
const char *GppcTupleDescAttrName(GppcTupleDesc tupdesc, int16_t attno);

/**
 * \brief Returns the attribute type oid in the descriptor.
 * \param tupdesc the tuple descriptor.
 * \param attno attribute position, starting from 0.
 */
GppcOid GppcTupleDescAttrType(GppcTupleDesc tupdesc, int16_t attno);

/**
 * \brief Returns the attribute type length in the descriptor.
 * \param tupdesc the tuple descriptor.
 * \param attno attribute position, starting from 0.
 *
 * \sa GppcTupleDescAttrTypmod()
 *
 * Returns the byte length of the data type from the tuple descriptor, -1 if
 * the type is variable length, or -2 if the type is a null-terminated C string.
 */
int16_t GppcTupleDescAttrLen(GppcTupleDesc tupdesc, int16_t attno);

/**
 * \brief Returns the attribute type modifier oid in the descriptor.
 * \param tupdesc the tuple descriptor.
 * \param attno attribute position, starting from 0.
 */
int32_t GppcTupleDescAttrTypmod(GppcTupleDesc tupdesc, int16_t attno);

/**
 * \brief Represents character encoding.
 */
typedef enum GppcEncoding
{
	GPPC_SQL_ASCII = 0,				/* SQL/ASCII */
	GPPC_EUC_JP = 1,				/* EUC for Japanese */
	GPPC_EUC_CN = 2,				/* EUC for Chinese */
	GPPC_EUC_KR = 3,				/* EUC for Korean */
	GPPC_EUC_TW = 4,				/* EUC for Taiwan */
	GPPC_EUC_JIS_2004 = 5,			/* EUC-JIS-2004 */
	GPPC_UTF8 = 6,					/* Unicode UTF8 */
	GPPC_MULE_INTERNAL = 7,			/* Mule internal code */
	GPPC_LATIN1 = 8,				/* ISO-8859-1 Latin 1 */
	GPPC_LATIN2 = 9,				/* ISO-8859-2 Latin 2 */
	GPPC_LATIN3 = 10,				/* ISO-8859-3 Latin 3 */
	GPPC_LATIN4 = 11,				/* ISO-8859-4 Latin 4 */
	GPPC_LATIN5 = 12,				/* ISO-8859-9 Latin 5 */
	GPPC_LATIN6 = 13,				/* ISO-8859-10 Latin6 */
	GPPC_LATIN7 = 14,				/* ISO-8859-13 Latin7 */
	GPPC_LATIN8 = 15,				/* ISO-8859-14 Latin8 */
	GPPC_LATIN9 = 16,				/* ISO-8859-15 Latin9 */
	GPPC_LATIN10 = 17,				/* ISO-8859-16 Latin10 */
	GPPC_WIN1256 = 18,				/* windows-1256 */
	GPPC_WIN1258 = 19,				/* Windows-1258 */
	GPPC_WIN866 = 20,				/* (MS-DOS CP866) */
	GPPC_WIN874 = 21,				/* windows-874 */
	GPPC_KOI8R = 22,				/* KOI8-R */
	GPPC_WIN1251 = 23,				/* windows-1251 */
	GPPC_WIN1252 = 24,				/* windows-1252 */
	GPPC_ISO_8859_5 = 25,			/* ISO-8859-5 */
	GPPC_ISO_8859_6 = 26,			/* ISO-8859-6 */
	GPPC_ISO_8859_7 = 27,			/* ISO-8859-7 */
	GPPC_ISO_8859_8 = 28,			/* ISO-8859-8 */
	GPPC_WIN1250 = 29,				/* windows-1250 */
	GPPC_WIN1253 = 30,				/* windows-1253 */
	GPPC_WIN1254 = 31,				/* windows-1254 */
	GPPC_WIN1255 = 32,				/* windows-1255 */
	GPPC_WIN1257 = 33,				/* windows-1257 */
	GPPC_KOI8U = 34					/* KOI8-U */
} GppcEncoding;

/**
 * \brief Returns the database encoding.
 */
GppcEncoding GppcGetDatabaseEncoding(void);

/**
 * \brief Returns maximum byte size of database encoding in a character.
 */
int GppcDatabaseEncodingMaxLength(void);

/**
 * \brief Translates a ::GppcEncoding value to a human readable encoding name.
 *
 * If enc is not a valid GppcEncoding value, NULL is returned.
 */
const char *GppcDatabaseEncodingName(GppcEncoding enc);

/**
 * @}
 */
#endif   /* GPPC_H */
