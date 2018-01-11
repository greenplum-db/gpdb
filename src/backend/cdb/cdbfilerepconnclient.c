/*-------------------------------------------------------------------------
 *
 * cdbfilerepconnclient.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepconnclient.c
 *
 *-------------------------------------------------------------------------
 */

/*
 *
 * Responsibilities of this module.
 *		*)
 *
 */
#include "postgres.h"

#include <signal.h>

#include "cdb/cdbvars.h"

#include "cdb/cdbfilerepconnclient.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfilerepservice.h"

static PGconn *filerep_conn = NULL;

/*
 *
 */
int
FileRepConnClient_EstablishConnection(
									  char *hostAddress,
									  int port,
									  bool reportError)
{
	int			status = STATUS_OK;
	char		portbuf[11];
	char		timeoutbuf[11];
	const char *keys[5];
	const char *vals[5];

/*	FileRepConnClient_CloseConnection();*/

	snprintf(portbuf, sizeof(portbuf), "%d", port);
	snprintf(timeoutbuf, sizeof(timeoutbuf), "%d", gp_segment_connect_timeout);

	keys[0] = "host";
	vals[0] = hostAddress;
	keys[1] = "port";
	vals[1] = portbuf;
	keys[2] = "dbname";
	vals[2] = "postgres";
	keys[3] = "connect_timeout";
	vals[3] = timeoutbuf;
	keys[4] = NULL;
	vals[4] = NULL;

	filerep_conn = PQconnectdbParams(keys, vals, false);

	if (PQstatus(filerep_conn) != CONNECTION_OK)
	{
		if (reportError || Debug_filerep_print)
			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("could not establish connection with server, host:'%s' port:'%d' err:'%s' : %m",
							hostAddress,
							port,
							PQerrorMessage(filerep_conn)),
					 errSendAlert(true),
					 FileRep_errcontext()));

		status = STATUS_ERROR;

		if (filerep_conn)
		{
			PQfinish(filerep_conn);
			filerep_conn = NULL;
		}
	}

	/* NOTE Handle error message see ftsprobe.c */

	return status;
}

/*
 *
 */
void
FileRepConnClient_CloseConnection(void)
{
	if (filerep_conn)
	{
		/* use non-blocking mode to avoid the long timeout in pqSendSome */
		filerep_conn->nonblocking = TRUE;
		PQfinish(filerep_conn);
		filerep_conn = NULL;
	}
	return;
}

/*
 *
 *
 * Control Message has msg_type='C'.
 * Control Message is consumed by Receiver thread on mirror side.
 *
 * Data Message has msg_type='M'.
 * Data Message is inserted in Shared memory and consumed by Consumer
 * thread on mirror side.
 */
bool
FileRepConnClient_SendMessage(
							  FileRepConsumerProcIndex_e messageType,
							  bool messageSynchronous,
							  char *message,
							  uint32 messageLength)
{
	char		msgType = 0;
	int			status = STATUS_OK;

#ifdef USE_ASSERT_CHECKING
	int			prevOutCount = filerep_conn->outCount;
#endif							/* // USE_ASSERT_CHECKING */

	switch (messageType)
	{
		case FileRepMessageTypeXLog:
			msgType = '1';
			break;
		case FileRepMessageTypeAO01:
			msgType = '2';
			break;
		case FileRepMessageTypeWriter:
			msgType = '3';
			break;
		case FileRepMessageTypeShutdown:
			msgType = 'S';
			break;
		default:
			return false;
	}

	/**
	 * Note that pqPutMsgStart and pqPutnchar both may grow the connection's internal buffer, and do not
	 *   flush data
	 */
	if (pqPutMsgStart(msgType, true, filerep_conn) < 0)
	{
		return false;
	}

	if (pqPutnchar(message, messageLength, filerep_conn) < 0)
	{
		return false;
	}

	/*
	 * Server side needs complete messages for mode-transitions so disable
	 * auto-flush since it flushes partial messages
	 */
	pqPutMsgEndNoAutoFlush(filerep_conn);

	/* assert that a flush did not occur */
	Assert(prevOutCount + messageLength + 5 == filerep_conn->outCount); /* the +5 is the amount
																		 * added by
																		 * pgPutMsgStart */

	/*
	 * note also that we could do a flush beforehand to avoid having
	 * pqPutMsgStart and pqPutnchar growing the buffer
	 */
	if (messageSynchronous || filerep_conn->outCount >= file_rep_min_data_before_flush)
	{
		int			result = 0;

		/* wait and timeout will be handled by pqWaitTimeout */
		while ((status = pqFlushNonBlocking(filerep_conn)) > 0)
		{
			/* retry on timeout */
			while (!(result = pqWaitTimeout(FALSE, TRUE, filerep_conn, time(NULL) + file_rep_socket_timeout)))
			{
				if (FileRepSubProcess_IsStateTransitionRequested())
				{
					elog(WARNING, "segment state transition requested while waiting to write data to socket");
					status = -1;
					break;
				}
			}

			if (result < 0)
			{
				ereport(WARNING,
						(errcode_for_socket_access(),
						 errmsg("could not write data to socket, failure detected : %m")));
				status = -1;
				break;
			}

			if (status == -1)
			{
				break;
			}
		}

		if (status < 0)
		{
			return false;
		}
		Assert(status == 0);
		return true;
	}

	return true;
}
