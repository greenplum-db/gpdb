/*-------------------------------------------------------------------------
 *
 * xlogloc.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/transam/xlogloc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "access/xlog.h"

#include "access/heapam.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/acl.h"
#include "utils/builtins.h"

#define DatumGetXLogLoc(X)	     ((XLogRecPtr*) DatumGetPointer(X))
#define XLogLocGetDatum(X)	     PointerGetDatum(X)
#define PG_GETARG_XLOGLOC(n)     DatumGetXLogLoc(PG_GETARG_DATUM(n))
#define PG_RETURN_XLOGLOC(x)     return XLogLocGetDatum(x)

#define LDELIM			'('
#define RDELIM			')'
#define DELIM			'/'
#define NXLOGLOCARGS	2

static int gpxlogloc_cmp_internal(XLogRecPtr *arg1, XLogRecPtr *arg2);


/* ----------------------------------------------------------------
 *		gpxloglocin
 * ----------------------------------------------------------------
 */
Datum
gpxloglocin(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	char	   *p,
			   *coord[NXLOGLOCARGS];
	int			i;
	XLogRecPtr	*result;
	uint32		result_xlogid;
	uint32		result_xrecoff;
	char	   *badp;

	for (i = 0, p = str; *p && i < NXLOGLOCARGS && *p != RDELIM; p++)
		if (*p == DELIM || (*p == LDELIM && !i))
			coord[i++] = p + 1;

	if (i < NXLOGLOCARGS)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpxlogloc: \"%s\"",
						str)));

	errno = 0;
	result_xlogid = strtoul(coord[0], &badp, 16);	// Hexadecimal.
	if (errno || *badp != DELIM)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpxlogloc: \"%s\"",
						str)));

	result_xrecoff = strtoul(coord[1], &badp, 16);	// Hexadecimal.
	if (errno || *badp != RDELIM)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpxlogloc: \"%s\"",
						str)));

	result = (XLogRecPtr*) palloc(sizeof(XLogRecPtr));

	result->xlogid = result_xlogid;
	result->xrecoff = result_xrecoff;

	PG_RETURN_XLOGLOC(result);
}

/* ----------------------------------------------------------------
 *		gpxloglocout
 * ----------------------------------------------------------------
 */
Datum
gpxloglocout(PG_FUNCTION_ARGS)
{
	XLogRecPtr *xlogLoc = PG_GETARG_XLOGLOC(0);
	char		buf[32];

	snprintf(buf, sizeof(buf), "(%X/%X)", xlogLoc->xlogid, xlogLoc->xrecoff);

	PG_RETURN_CSTRING(pstrdup(buf));
}

/*
 *		gpxloglocrecv			- converts external binary format to xlog location
 */
Datum
gpxloglocrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	XLogRecPtr	*result;
	uint32		result_xlogid;
	uint32		result_xrecoff;

	result_xlogid = pq_getmsgint(buf, sizeof(result_xlogid));
	result_xrecoff = pq_getmsgint(buf, sizeof(result_xrecoff));

	result = (XLogRecPtr*) palloc(sizeof(XLogRecPtr));

	result->xlogid = result_xlogid;
	result->xrecoff = result_xrecoff;

	PG_RETURN_XLOGLOC(result);
}

/*
 *		gpxloglocsend			- converts xlog location to binary format
 */
Datum
gpxloglocsend(PG_FUNCTION_ARGS)
{
	XLogRecPtr *xlogLoc = PG_GETARG_XLOGLOC(0);
	uint32		send_xlogid;
	uint32		send_xrecoff;
	StringInfoData buf;

	send_xlogid = xlogLoc->xlogid;
	send_xrecoff = xlogLoc->xrecoff;

	pq_begintypsend(&buf);
	pq_sendint(&buf, send_xlogid, sizeof(send_xlogid));
	pq_sendint(&buf, send_xrecoff, sizeof(send_xrecoff));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}


Datum
gpxlogloclarger(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);
	XLogRecPtr *result;
	
	if (gpxlogloc_cmp_internal(arg1, arg2) > 0)
		result = arg1;
	else
		result = arg2;
	
	PG_RETURN_XLOGLOC(result);		
}

Datum
gpxloglocsmaller(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);
	XLogRecPtr *result;
	
	if (gpxlogloc_cmp_internal(arg1, arg2) < 0)
		result = arg1;
	else
		result = arg2;
	
	PG_RETURN_XLOGLOC(result);		
}

Datum
gpxlogloceq(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) == 0);
}

Datum
gpxloglocne(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) != 0);
}

Datum
gpxlogloclt(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) < 0);
}

Datum
gpxloglocle(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) <= 0);
}

Datum
gpxloglocgt(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) > 0);
}

Datum
gpxloglocge(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_BOOL(gpxlogloc_cmp_internal(arg1, arg2) >= 0);
}

Datum
btgpxlogloccmp(PG_FUNCTION_ARGS)
{
	XLogRecPtr *arg1 = PG_GETARG_XLOGLOC(0);
	XLogRecPtr *arg2 = PG_GETARG_XLOGLOC(1);

	PG_RETURN_INT32(gpxlogloc_cmp_internal(arg1, arg2));
}

static int
gpxlogloc_cmp_internal(XLogRecPtr *arg1, XLogRecPtr *arg2)
{
	if(arg1->xlogid > arg2->xlogid)
		return 1;
	else if(arg1->xlogid < arg2->xlogid)
		return -1;
	else if(arg1->xrecoff > arg2->xrecoff)
		return 1;
	else if (arg1->xrecoff < arg2->xrecoff)
		return -1;
	else 
		return 0;
}

