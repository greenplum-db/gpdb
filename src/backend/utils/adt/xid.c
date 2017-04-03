/*-------------------------------------------------------------------------
 *
 * xid.c
 *	  POSTGRES transaction identifier and command identifier datatypes.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/adt/xid.c,v 1.12 2009/01/01 17:23:50 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/transam.h"
#include "access/xact.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"

#define PG_GETARG_TRANSACTIONID(n)	DatumGetTransactionId(PG_GETARG_DATUM(n))
#define PG_RETURN_TRANSACTIONID(x)	return TransactionIdGetDatum(x)

#define PG_GETARG_COMMANDID(n)		DatumGetCommandId(PG_GETARG_DATUM(n))
#define PG_RETURN_COMMANDID(x)		return CommandIdGetDatum(x)


Datum
xidin(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);

	PG_RETURN_TRANSACTIONID((TransactionId) strtoul(str, NULL, 0));
}

Datum
xidout(PG_FUNCTION_ARGS)
{
	TransactionId transactionId = PG_GETARG_TRANSACTIONID(0);

	/* maximum 32 bit unsigned integer representation takes 10 chars */
	char	   *str = palloc(11);

	snprintf(str, 11, "%lu", (unsigned long) transactionId);

	PG_RETURN_CSTRING(str);
}

/*
 *		xidrecv			- converts external binary format to xid
 */
Datum
xidrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);

	PG_RETURN_TRANSACTIONID((TransactionId) pq_getmsgint(buf, sizeof(TransactionId)));
}

/*
 *		xidsend			- converts xid to binary format
 */
Datum
xidsend(PG_FUNCTION_ARGS)
{
	TransactionId arg1 = PG_GETARG_TRANSACTIONID(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendint(&buf, arg1, sizeof(arg1));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 *		xideq			- are two xids equal?
 */
Datum
xideq(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(TransactionIdEquals(xid1, xid2));
}

Datum
xidne(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(xid1 != xid2);
}

Datum
xidlt(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(xid1 < xid2);
}

Datum
xidgt(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(xid1 > xid2);
}

Datum
xidle(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(xid1 <= xid2);
}

Datum
xidge(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	PG_RETURN_BOOL(xid1 >= xid2);
}

Datum
btxidcmp(PG_FUNCTION_ARGS)
{
	TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
	TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

	if (xid1 == xid2)
		PG_RETURN_INT32(0);
	else if (xid1 > xid2)
		PG_RETURN_INT32(1);
	else
		PG_RETURN_INT32(-1);
}


/*
 * xid_age - compute age of an XID (relative to latest transaction id
 * allocated in system)
 *
 * ReadNewTransactionId() is used here instead of GetTopTransactionId(), as
 * this function may be called on QE Reader and with laxy XID try to allocate
 * XID as QE Reader which is not allowed.
 */
Datum
xid_age(PG_FUNCTION_ARGS)
{
	TransactionId xid = PG_GETARG_TRANSACTIONID(0);
	TransactionId now = ReadNewTransactionId();

	/* Permanent XIDs are always infinitely old */
	if (!TransactionIdIsNormal(xid))
		PG_RETURN_INT32(INT_MAX);

	PG_RETURN_INT32((int32) (now - xid));
}


/*****************************************************************************
 *	 COMMAND IDENTIFIER ROUTINES											 *
 *****************************************************************************/

/*
 *		cidin	- converts CommandId to internal representation.
 */
Datum
cidin(PG_FUNCTION_ARGS)
{
	char	   *s = PG_GETARG_CSTRING(0);
	CommandId	c;

	c = atoi(s);

	PG_RETURN_COMMANDID(c);
}

/*
 *		cidout	- converts a cid to external representation.
 */
Datum
cidout(PG_FUNCTION_ARGS)
{
	CommandId	c = PG_GETARG_COMMANDID(0);
	char	   *result = (char *) palloc(16);

	snprintf(result, 16, "%u", (unsigned int) c);
	PG_RETURN_CSTRING(result);
}

/*
 *		cidrecv			- converts external binary format to cid
 */
Datum
cidrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);

	PG_RETURN_COMMANDID((CommandId) pq_getmsgint(buf, sizeof(CommandId)));
}

/*
 *		cidsend			- converts cid to binary format
 */
Datum
cidsend(PG_FUNCTION_ARGS)
{
	CommandId	arg1 = PG_GETARG_COMMANDID(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendint(&buf, arg1, sizeof(arg1));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
cideq(PG_FUNCTION_ARGS)
{
	CommandId	arg1 = PG_GETARG_COMMANDID(0);
	CommandId	arg2 = PG_GETARG_COMMANDID(1);

	PG_RETURN_BOOL(arg1 == arg2);
}
