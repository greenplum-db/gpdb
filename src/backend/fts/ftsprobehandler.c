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
#include "replication/gp_replication.h"

static void
SendProbeResponse(ProbeResponse *response)
{
	StringInfoData buf;

	initStringInfo(&buf);

	pq_beginmessage(&buf, '\0');

	pq_sendbytes(&buf, (char *) response, sizeof(ProbeResponse));

	pq_endmessage(&buf);
	pq_flush();
}

void
HandleFtsWalRepProbe()
{
	ProbeResponse response;

	response.IsMirrorUp = IsMirrorUp();
	SendProbeResponse(&response);
}
