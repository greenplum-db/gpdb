/*-------------------------------------------------------------------------
 *
 * appendonlytid.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/access/appendonly/appendonlytid.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/appendonlytid.h"

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
