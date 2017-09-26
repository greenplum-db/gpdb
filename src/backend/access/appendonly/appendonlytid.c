/*-------------------------------------------------------------------------
 *
 * appendonlytid.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonlytid.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "access/appendonlytid.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "cdb/cdbappendonlystorage.h"

#include "access/heapam.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/acl.h"
#include "utils/builtins.h"

#define DatumGetAOTupleId(X)	 ((AOTupleId*) DatumGetPointer(X))
#define AOTupleIdGetDatum(X)	 PointerGetDatum(X)
#define PG_GETARG_AOTID(n) 		 DatumGetAOTupleId(PG_GETARG_DATUM(n))
#define PG_RETURN_AOTID(x) 		 return AOTupleIdGetDatum(x)

#define LDELIM			'('
#define RDELIM			')'
#define DELIM			','
#define NAOTIDARGS		2

/* ----------------------------------------------------------------
 *		gpaotidin
 * ----------------------------------------------------------------
 */
Datum
gpaotidin(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	char	   *p,
			   *coord[NAOTIDARGS];
	int			i;
	AOTupleId  *result;
	unsigned int segmentFileNum;
	int64		rowNum;
	char	   *badp;

	for (i = 0, p = str; *p && i < NAOTIDARGS && *p != RDELIM; p++)
		if (*p == DELIM || (*p == LDELIM && !i))
			coord[i++] = p + 1;

	if (i < NAOTIDARGS)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpaotid: \"%s\"",
						str)));

/*  UNDONE: Move */
#define AOTupleId_MaxSegmentFileNum            127

	errno = 0;
	segmentFileNum = strtoul(coord[0], &badp, 10);
	if (errno || *badp != DELIM ||
		segmentFileNum > AOTupleId_MaxSegmentFileNum)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpaotid: \"%s\"",
						str)));

	rowNum = strtoll(coord[1], &badp, 10);
	if (errno || *badp != RDELIM ||
		rowNum > AOTupleId_MaxRowNum || rowNum < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type gpaotid: \"%s\"",
						str)));

	result = (AOTupleId *) palloc(sizeof(AOTupleId));

	AOTupleIdInit_Init(result);
	AOTupleIdInit_segmentFileNum(result, segmentFileNum);
	AOTupleIdInit_rowNum(result, rowNum);

	PG_RETURN_AOTID(result);
}

/* ----------------------------------------------------------------
 *		gpaotidout
 * ----------------------------------------------------------------
 */
Datum
gpaotidout(PG_FUNCTION_ARGS)
{
	AOTupleId  *aoTupleId = PG_GETARG_AOTID(0);
	unsigned int segmentFileNum;
	int64		rowNum;
	char		buf[32];

	segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	rowNum = AOTupleIdGet_rowNum(aoTupleId);

	/* Perhaps someday we should output this as a record. */
	snprintf(buf, sizeof(buf), "(%u," INT64_FORMAT ")", segmentFileNum, rowNum);

	PG_RETURN_CSTRING(pstrdup(buf));
}

/*
 *		gpaotidrecv			- converts external binary format to tid
 */
Datum
gpaotidrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	AOTupleId  *result;
	unsigned int segmentFileNum;
	int64		rowNum;

	segmentFileNum = pq_getmsgint(buf, sizeof(segmentFileNum));
	/* UNDONE: pg_getmsgint doesn't handle 8 byte integers... */
	rowNum = pq_getmsgint(buf, sizeof(rowNum));

	result = (AOTupleId *) palloc(sizeof(ItemPointerData));

	AOTupleIdInit_Init(result);
	AOTupleIdInit_segmentFileNum(result, segmentFileNum);
	AOTupleIdInit_rowNum(result, rowNum);

	PG_RETURN_AOTID(result);
}

/*
 *		gpaotidsend			- converts tid to binary format
 */
Datum
gpaotidsend(PG_FUNCTION_ARGS)
{
	AOTupleId  *aoTupleId = PG_GETARG_AOTID(0);
	unsigned int segmentFileNum;
	int64		rowNum;
	StringInfoData buf;

	segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	rowNum = AOTupleIdGet_rowNum(aoTupleId);

	pq_begintypsend(&buf);
	pq_sendint(&buf, segmentFileNum, sizeof(segmentFileNum));
	/* UNDONE: pq_sendint doesn't handle 8 byte integers... */
	pq_sendint(&buf, rowNum, sizeof(rowNum));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

#define MAX_AO_TUPLE_ID_BUFFER 25
static char AOTupleIdBuffer[MAX_AO_TUPLE_ID_BUFFER];

char *
AOTupleIdToString(AOTupleId *aoTupleId)
{
	int			segmentFileNum = AOTupleIdGet_segmentFileNum(aoTupleId);
	int64		rowNum = AOTupleIdGet_rowNum(aoTupleId);
	int			snprintfResult;

	snprintfResult =
		snprintf(AOTupleIdBuffer, MAX_AO_TUPLE_ID_BUFFER, "(%d," INT64_FORMAT ")",
				 segmentFileNum, rowNum);

	Assert(snprintfResult >= 0);
	Assert(snprintfResult < MAX_AO_TUPLE_ID_BUFFER);

	return AOTupleIdBuffer;
}
