/*-------------------------------------------------------------------------
 *
 * ftsprobehandler.c
 *	  Implementation of handling of FTS probe
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsprobehandler.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libpq/pqformat.h"
#include "libpq/libpq.h"
#include "postmaster/fts.h"
#include "utils/guc.h"
#include "replication/gp_replication.h"

static void
SendProbeResponse(ProbeResponse *response)
{
	StringInfoData buf;

	initStringInfo(&buf);

	BeginCommand(FTS_MSG_TYPE_PROBE, DestRemote);

	pq_beginmessage(&buf, 'T');
	pq_sendint(&buf, Natts_fts_probe_response, 2); /* 1 field */

	pq_sendstring(&buf, "is_mirror_up");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_probe_response_is_mirror_up, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */
	pq_endmessage(&buf);

	/* Send a DataRow message */
	pq_beginmessage(&buf, 'D');
	pq_sendint(&buf, Natts_fts_probe_response, 2);		/* # of columns */
	pq_sendint(&buf, 1, 4); /* col1 len */
	pq_sendint(&buf, response->IsMirrorUp, 1);
	pq_endmessage(&buf);

	EndCommand(FTS_MSG_TYPE_PROBE, DestRemote);
	pq_flush();
}

void
HandleFtsWalRepProbe()
{
	ProbeResponse response;

	response.IsMirrorUp = IsMirrorUp();
	SendProbeResponse(&response);
}
