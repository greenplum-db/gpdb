/*-------------------------------------------------------------------------
 *
 * cdbfilerepconnserver.c
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepconnserver.c
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

#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbfilerepconnserver.h"
#include "cdb/cdbfilerepservice.h"
#include "libpq/libpq.h"
#include "libpq/auth.h"
#include "libpq/pqformat.h"

static Port *port;
static void ConnFree(void);

static int	listenSocket[FILEREP_MAX_LISTEN];

/*
 *
 */
int
FileRepConnServer_StartListener(
								char *hostAddress,
								int portLocal)
{

	int			status = STATUS_OK;
	int			i;

	for (i = 0; i < FILEREP_MAX_LISTEN; i++)
	{
		listenSocket[i] = -1;
	}

	/* NOTE check if family AF_UNIX has to be considered as well */
	status = StreamServerPort(
							  AF_UNSPEC,
							  hostAddress,
							  (unsigned short) portLocal,
							  NULL,
							  listenSocket,
							  FILEREP_MAX_LISTEN);

	if (status != STATUS_OK)
	{
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("could not start listener, host:'%s' port:'%d': %m",
						hostAddress,
						portLocal),
				 errSendAlert(true),
				 FileRep_errcontext()));
	}

	return status;
}

/*
 *
 */
int
FileRepConnServer_CreateConnection()
{
	int			status = STATUS_OK;

	port = (Port *) calloc(1, sizeof(Port));
	if (port == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough memory to create connection"))));
		return status;
	}

	status = StreamConnection(listenSocket[0], port);
	if (status != STATUS_OK)
	{
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("could not accept connection: %m"),
				 FileRep_errcontext()));

		if (port->sock >= 0)
		{
			StreamClose(port->sock);
		}
		ConnFree();
	}
	else
	{
		/*
		 * MPP-14225: On NIC failure, filerep receiver process's recv() system
		 * call will take hours to timeout, depending on the TCP timeout. Add
		 * SO_RCVTIMEO timeout to filerep receiver process's socket to avoid
		 * this.
		 */
		struct timeval tv;

		tv.tv_sec = file_rep_socket_timeout;
		tv.tv_usec = 0;			/* Not initializing this can cause strange
								 * errors */

		if (setsockopt(port->sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval)) == -1)
			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("could not set receive timeout on socket")));

		/* set TCP keep-alive parameters for FileRep connection */
		(void) pq_setkeepalivesidle(gp_filerep_tcp_keepalives_idle, port);
		(void) pq_setkeepalivesinterval(gp_filerep_tcp_keepalives_interval, port);
		(void) pq_setkeepalivescount(gp_filerep_tcp_keepalives_count, port);

		MyProcPort = port;
	}

	return status;
}

/*
 *
 */
int
FileRepConnServer_Select(void)
{
	struct timeval timeout;
	fd_set		rfds;
	int			retval;

	timeout.tv_sec = 0;
	timeout.tv_usec = 100 * 1000L;

	FD_ZERO(&rfds);

	FD_SET(listenSocket[0], &rfds);

	retval = select(listenSocket[0] + 1, &rfds, NULL, NULL, &timeout);

	/*
	 * check and process any signals received The routine returns TRUE if the
	 * received signal requests process shutdown.
	 */

	if (retval)
	{
		if (!FD_ISSET(listenSocket[0], &rfds))
		{
			retval = -1;
		}
	}

	if (retval == -1)
	{
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("receive failure on connection: %m"),
				 FileRep_errcontext()));
	}

	return retval;
}

void
FileRepConnServer_CloseConnection(void)
{
	if (port != NULL)
	{
		secure_close(port);

		if (port->sock >= 0)
		{
			/* to close socket (file descriptor) */
			StreamClose(port->sock);
			port->sock = -1;
		}
		ConnFree();
	}
}

/*
 *
 */
void
ConnFree(void)
{
	if (port)
	{
		/* We copied the pointer in FileRepConnServer_CreateConnection() */
		if (port == MyProcPort)
			MyProcPort = NULL;
		free(port);
	}
	port = NULL;
}

/*
 * Receive Startup packet
 * Response Client Authentication
 */
int
FileRepConnServer_ReceiveStartupPacket(void)
{
	uint32		length;
	int			status = STATUS_OK;
	char	   *buf = NULL;

	pq_init();

	status = FileRepConnServer_ReceiveMessageLength(&length);
	if (status != STATUS_OK)
	{
		goto exit;
	}

	if (length < (uint32) sizeof(ProtocolVersion) ||
		length > MAX_STARTUP_PACKET_LENGTH)
	{

		status = STATUS_ERROR;
		ereport(WARNING,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("invalid length of startup packet"),
				 FileRep_errcontext()));
		goto exit;
	}

	buf = (char *) malloc(length + 1);
	if (buf == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("not enough memory to allocate buffer for startup packet"),
				 FileRep_errcontext()));
	}
	memset(buf, 0, length + 1);

	if (pq_getbytes(buf, length) == EOF)
	{

		status = STATUS_ERROR;
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("receive EOF on connection: %m"),
				 FileRep_errcontext()));
		goto exit;
	}

	port->proto = ntohl(*((ProtocolVersion *) buf));

	if (PG_PROTOCOL_MAJOR(port->proto) >= 3)
	{
		/* uint32	offset = sizeof(ProtocolVersion); */
		/*
		 * tell the client that it is authorized (no pg_hba.conf and password
		 * are required).
		 */
		StringInfoData buf;

		/* sends AUTH_REQ_OK back to client */
		FakeClientAuthentication(port);

		/* send to client that we are ready to receive data */
		/* similar to ReadyForQuery(DestRemoteExecute); */
		pq_beginmessage(&buf, 'Z');
		pq_sendbyte(&buf, 'I');
		pq_endmessage(&buf);

		pq_flush();
	}
	else
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("not supported version"),
				 FileRep_errcontext()));
	}

exit:
	if (buf)
	{
		free(buf);
		buf = NULL;
	}

	return status;
}

/**
 * Blocks until data is available, the socket is in error, or the function is interrupted.
 *
 * return false if the function was interrupted, true otherwise.
 */
bool
FileRepConnServer_AwaitMessageBegin()
{
	return pq_waitForDataUsingSelect();
}

/*
 *
 */
int
FileRepConnServer_ReceiveMessageType(
									 FileRepConsumerProcIndex_e *fileRepMessageType)
{

	char		messageType;

	int			status = STATUS_OK;

	messageType = pq_getbyte();


	switch (messageType)
	{

		case '1':
			*fileRepMessageType = FileRepMessageTypeXLog;
			break;

		case '2':
			*fileRepMessageType = FileRepMessageTypeAO01;
			break;

		case '3':
			*fileRepMessageType = FileRepMessageTypeWriter;
			break;

		case 'S':
			*fileRepMessageType = FileRepMessageTypeShutdown;
			break;

		case EOF:
			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("receive EOF on connection: %m")));
			status = STATUS_ERROR;
			break;
		case 'X':
			/* Close Message(sent by PQfinish()) */

			/*
			 * Client closed connection.  Client does not wait for response.
			 */
			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("receive close on connection: %m")));
			status = STATUS_ERROR;
			break;
		default:
			ereport(WARNING,
					(errcode_for_socket_access(),
					 errmsg("receive unexpected message type on connection: %m")));
			status = STATUS_ERROR;
			break;
	}

	return status;
}

/*
 *
 */
int
FileRepConnServer_ReceiveMessageLength(uint32 *len)

{
	int32		length;

	if (pq_getbytes((char *) &length, 4) == EOF)
	{
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("receive EOF on connection: %m")));

		return STATUS_ERROR;
	}

	length = ntohl(length);

	if (length < 4)
	{
		ereport(WARNING,
				(errmsg("receive unexpected message length on connection")));
		return STATUS_ERROR;
	}

	length -= 4;

	*len = length;

	return STATUS_OK;
}


/*
 *
 */
int
FileRepConnServer_ReceiveMessageData(
									 char *data,
									 uint32 length)
{

	if (pq_getbytes(data, length) == EOF)
	{
		ereport(WARNING,
				(errcode_for_socket_access(),
				 errmsg("receive EOF on connection: %m")));

		return STATUS_ERROR;
	}

	return STATUS_OK;
}
