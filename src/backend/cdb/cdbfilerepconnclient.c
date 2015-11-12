/*
 *  filerepconnclient.c
 *  
 *
 *  Copyright 2009-2010 Greenplum Inc. All rights reserved.
 *
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
#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "pqexpbuffer.h"
#include "cdb/cdbfilerepservice.h"

static PGconn	*conn = NULL;
/*
 *
 */
int 
FileRepConnClient_EstablishConnection(
	char	*hostAddress,
	int		port,
	bool	reportError)
{
	int			status = STATUS_OK;
	PQExpBuffer	buffer = NULL;
	
/*	FileRepConnClient_CloseConnection();*/
	
	buffer = createPQExpBuffer();

	if (PQExpBufferBroken(buffer))
	{
		destroyPQExpBuffer(buffer);
		/* XXX: allocation failed. Is this the right thing to do ? */
		return STATUS_ERROR;
	}

	appendPQExpBuffer(buffer, "host=%s ", hostAddress);	
	appendPQExpBuffer(buffer, "port=%d ", port);
	appendPQExpBuffer(buffer, "dbname=postgres ");
	appendPQExpBuffer(buffer, "connect_timeout=%d", gp_segment_connect_timeout);
	
	conn = PQconnectdb(buffer->data);

	if (PQstatus(conn) != CONNECTION_OK) {		
		if (reportError || Debug_filerep_print)
			ereport(WARNING, 
					(errcode_for_socket_access(),
					 errmsg("could not establish connection with server, host:'%s' port:'%d' err:'%s' : %m",
							hostAddress,
							port,
							PQerrorMessage(conn)),
					 errSendAlert(true),
					 FileRep_errcontext()));
		
		status = STATUS_ERROR;
		
		if (conn) {
			PQfinish(conn);
			conn = NULL;
		}
	}
	
	/* NOTE Handle error message see ftsprobe.c */
	
	if (buffer) {
		destroyPQExpBuffer(buffer);
		buffer = NULL;
	}
	return status;	
}

/*
 *
 */
void
FileRepConnClient_CloseConnection(void)
{
	if (conn) {
		/* use non-blocking mode to avoid the long timeout in pqSendSome */
		conn->nonblocking = TRUE;
		PQfinish(conn);
		conn = NULL;
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
	FileRepConsumerProcIndex_e	messageType, 
	bool	messageSynchronous,
	char*	message, 
	uint32	messageLength)
{
	char msgType = 0;
	int status = STATUS_OK;

#ifdef USE_ASSERT_CHECKING
	int prevOutCount = conn->outCount;
#endif // USE_ASSERT_CHECKING

	switch(messageType)
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
	if (pqPutMsgStart(msgType, true, conn) < 0 )
	{
		return false;
	}

	if ( pqPutnchar(message, messageLength, conn) < 0 )
	{
		return false;
	}

	/* Server side needs complete messages for mode-transitions so disable auto-flush since it flushes
	 *  partial messages
	 */
	pqPutMsgEndNoAutoFlush(conn);

	/* assert that a flush did not occur */
	Assert( prevOutCount + messageLength + 5 == conn->outCount ); /* the +5 is the amount added by pgPutMsgStart */

	/*
	 *                note also that we could do a flush beforehand to avoid
	 *                having pqPutMsgStart and pqPutnchar growing the buffer
	 */
	 if (messageSynchronous || conn->outCount >= file_rep_min_data_before_flush )
	{
		int result = 0;
		/* wait and timeout will be handled by pqWaitTimeout */
		while ((status = pqFlushNonBlocking(conn)) > 0)
		{
			/* retry on timeout */
			while (!(result = pqWaitTimeout(FALSE, TRUE, conn, time(NULL) + file_rep_socket_timeout)))
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
		Assert( status == 0 );
		return true;
	}

	return true;
}
