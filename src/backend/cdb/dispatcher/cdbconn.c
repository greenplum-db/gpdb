/*-------------------------------------------------------------------------
 *
 * cdbconn.c
 *
 * SegmentDatabaseDescriptor methods
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/dispatcher/cdbconn.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "libpq/libpq-be.h"

extern int	pq_flush(void);
extern int	pq_putmessage(char msgtype, const char *s, size_t len);

static uint32 cdbconn_get_motion_listener_port(PGconn *conn);

#include "cdb/cdbconn.h"		/* me */
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"

int			gp_segment_connect_timeout = 180;

static const char *
transStatusToString(PGTransactionStatusType status)
{
	const char *ret = "";

	switch (status)
	{
		case PQTRANS_IDLE:
			ret = "idle";
			break;
		case PQTRANS_ACTIVE:
			ret = "active";
			break;
		case PQTRANS_INTRANS:
			ret = "idle, within transaction";
			break;
		case PQTRANS_INERROR:
			ret = "idle, within failed transaction";
			break;
		case PQTRANS_UNKNOWN:
			ret = "unknown transaction status";
			break;
		default:
			Assert(false);
	}
	return ret;
}

static void
MPPnoticeReceiver(void *arg, const PGresult *res)
{
	PQExpBufferData msgbuf;
	PGMessageField *pfield;
	int			elevel = INFO;
	char	   *sqlstate = "00000";
	char	   *severity = "WARNING";
	char	   *file = "";
	char	   *line = NULL;
	char	   *func = "";
	char		message[1024];
	char	   *detail = NULL;
	char	   *hint = NULL;
	char	   *context = NULL;

	SegmentDatabaseDescriptor *segdbDesc = (SegmentDatabaseDescriptor *) arg;

	if (!res)
		return;

	strcpy(message, "missing error text");

	for (pfield = res->errFields; pfield != NULL; pfield = pfield->next)
	{
		switch (pfield->code)
		{
			case PG_DIAG_SEVERITY:
				severity = pfield->contents;
				if (strcmp(pfield->contents, "WARNING") == 0)
					elevel = WARNING;
				else if (strcmp(pfield->contents, "NOTICE") == 0)
					elevel = NOTICE;
				else if (strcmp(pfield->contents, "DEBUG1") == 0 ||
						 strcmp(pfield->contents, "DEBUG") == 0)
					elevel = DEBUG1;
				else if (strcmp(pfield->contents, "DEBUG2") == 0)
					elevel = DEBUG2;
				else if (strcmp(pfield->contents, "DEBUG3") == 0)
					elevel = DEBUG3;
				else if (strcmp(pfield->contents, "DEBUG4") == 0)
					elevel = DEBUG4;
				else if (strcmp(pfield->contents, "DEBUG5") == 0)
					elevel = DEBUG5;
				else
					elevel = INFO;
				break;
			case PG_DIAG_SQLSTATE:
				sqlstate = pfield->contents;
				break;
			case PG_DIAG_MESSAGE_PRIMARY:
				strncpy(message, pfield->contents, 800);
				message[800] = '\0';
				if (segdbDesc && segdbDesc->whoami && strlen(segdbDesc->whoami) < 200)
				{
					strcat(message, "  (");
					strcat(message, segdbDesc->whoami);
					strcat(message, ")");
				}
				break;
			case PG_DIAG_MESSAGE_DETAIL:
				detail = pfield->contents;
				break;
			case PG_DIAG_MESSAGE_HINT:
				hint = pfield->contents;
				break;
			case PG_DIAG_STATEMENT_POSITION:
			case PG_DIAG_INTERNAL_POSITION:
			case PG_DIAG_INTERNAL_QUERY:
				break;
			case PG_DIAG_CONTEXT:
				context = pfield->contents;
				break;
			case PG_DIAG_SOURCE_FILE:
				file = pfield->contents;
				break;
			case PG_DIAG_SOURCE_LINE:
				line = pfield->contents;
				break;
			case PG_DIAG_SOURCE_FUNCTION:
				func = pfield->contents;
				break;
			case PG_DIAG_GP_PROCESS_TAG:
				break;
			default:
				break;

		}
	}

	if (elevel < client_min_messages && elevel != INFO)
		return;

	/*
	 * We use PQExpBufferData instead of StringInfoData because the former
	 * uses malloc, the latter palloc. We are in a thread, and we CANNOT use
	 * palloc since it's not thread safe.  We cannot call elog or ereport
	 * either for the same reason.
	 */
	initPQExpBuffer(&msgbuf);


	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* New style with separate fields */

		appendPQExpBufferChar(&msgbuf, PG_DIAG_SEVERITY);
		appendBinaryPQExpBuffer(&msgbuf, severity, strlen(severity) + 1);

		appendPQExpBufferChar(&msgbuf, PG_DIAG_SQLSTATE);
		appendBinaryPQExpBuffer(&msgbuf, sqlstate, strlen(sqlstate) + 1);

		/* M field is required per protocol, so always send something */
		appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_PRIMARY);
		appendBinaryPQExpBuffer(&msgbuf, message, strlen(message) + 1);

		if (detail)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_DETAIL);
			appendBinaryPQExpBuffer(&msgbuf, detail, strlen(detail) + 1);
		}

		if (hint)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_HINT);
			appendBinaryPQExpBuffer(&msgbuf, hint, strlen(hint) + 1);
		}

		if (context)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_CONTEXT);
			appendBinaryPQExpBuffer(&msgbuf, context, strlen(context) + 1);
		}

		if (file)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_FILE);
			appendBinaryPQExpBuffer(&msgbuf, file, strlen(file) + 1);
		}

		if (line)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_LINE);
			appendBinaryPQExpBuffer(&msgbuf, line, strlen(line) + 1);
		}

		if (func)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_FUNCTION);
			appendBinaryPQExpBuffer(&msgbuf, func, strlen(func) + 1);
		}

	}
	else
	{

		appendPQExpBuffer(&msgbuf, "%s:  ", severity);

		appendBinaryPQExpBuffer(&msgbuf, message, strlen(message));

		appendPQExpBufferChar(&msgbuf, '\n');
		appendPQExpBufferChar(&msgbuf, '\0');

	}

	appendPQExpBufferChar(&msgbuf, '\0');	/* terminator */

	pq_putmessage('N', msgbuf.data, msgbuf.len);

	termPQExpBuffer(&msgbuf);

	pq_flush();
}

/* Initialize a QE connection descriptor in storage provided by the caller. */
void
cdbconn_initSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc,
							  struct CdbComponentDatabaseInfo *cdbinfo)
{
	MemSet(segdbDesc, 0, sizeof(*segdbDesc));

	/* Segment db info */
	segdbDesc->segment_database_info = cdbinfo;
	segdbDesc->segindex = cdbinfo->segindex;

	/* Connection info, set in function cdbconn_doConnect */
	segdbDesc->conn = NULL;
	segdbDesc->motionListener = 0;
	segdbDesc->backendPid = 0;

	/* whoami */
	segdbDesc->whoami = NULL;

	/* Connection error info */
	segdbDesc->errcode = 0;
	initPQExpBuffer(&segdbDesc->error_message);
}

/* Free memory of segment descriptor. */
void
cdbconn_termSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc)
{
	/* Free the error message buffer. */
	segdbDesc->errcode = 0;
	termPQExpBuffer(&segdbDesc->error_message);

	if (segdbDesc->whoami != NULL)
	{
		pfree(segdbDesc->whoami);
		segdbDesc->whoami = NULL;
	}
}								/* cdbconn_termSegmentDescriptor */

/*
 * Connect to a QE as a client via libpq.
 * returns true if connected.
 */
void
cdbconn_doConnect(SegmentDatabaseDescriptor *segdbDesc,
				  const char *gpqeid,
				  const char *options)
{
#define MAX_KEYWORDS 10
#define MAX_INT_STRING_LEN 20
	CdbComponentDatabaseInfo *cdbinfo = segdbDesc->segment_database_info;
	const char *keywords[MAX_KEYWORDS];
	const char *values[MAX_KEYWORDS];
	char		portstr[MAX_INT_STRING_LEN];
	char		timeoutstr[MAX_INT_STRING_LEN];
	int			nkeywords = 0;

	keywords[nkeywords] = "gpqeid";
	values[nkeywords] = gpqeid;
	nkeywords++;

	/*
	 * Build the connection string
	 */
	if (options)
	{
		keywords[nkeywords] = "options";
		values[nkeywords] = options;
		nkeywords++;
	}

	/*
	 * For entry DB connection, we make sure both "hostaddr" and "host" are
	 * empty string. Or else, it will fall back to environment variables and
	 * won't use domain socket in function connectDBStart.
	 *
	 * For other QE connections, we set "hostaddr". "host" is not used.
	 */
	if (segdbDesc->segindex == MASTER_CONTENT_ID &&
		GpIdentity.segindex == MASTER_CONTENT_ID)
	{
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = "";
		nkeywords++;
	}
	else
	{
		Assert(cdbinfo->hostip != NULL);
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = cdbinfo->hostip;
		nkeywords++;
	}

	keywords[nkeywords] = "host";
	values[nkeywords] = "";
	nkeywords++;

	snprintf(portstr, sizeof(portstr), "%u", cdbinfo->port);
	keywords[nkeywords] = "port";
	values[nkeywords] = portstr;
	nkeywords++;

	if (MyProcPort->database_name)
	{
		keywords[nkeywords] = "dbname";
		values[nkeywords] = MyProcPort->database_name;
		nkeywords++;
	}

	Assert(MyProcPort->user_name);
	keywords[nkeywords] = "user";
	values[nkeywords] = MyProcPort->user_name;
	nkeywords++;

	snprintf(timeoutstr, sizeof(timeoutstr), "%d", gp_segment_connect_timeout);
	keywords[nkeywords] = "connect_timeout";
	values[nkeywords] = timeoutstr;
	nkeywords++;

	keywords[nkeywords] = NULL;
	values[nkeywords] = NULL;

	Assert(nkeywords < MAX_KEYWORDS);

	/*
	 * Call libpq to connect
	 */
	segdbDesc->conn = PQconnectdbParams(keywords, values, false);

	/*
	 * Check for connection failure.
	 */
	if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
	{
		if (!segdbDesc->errcode)
			segdbDesc->errcode = ERRCODE_GP_INTERCONNECTION_ERROR;

		appendPQExpBuffer(&segdbDesc->error_message, "%s", PQerrorMessage(segdbDesc->conn));

		/* Don't use elog, it's not thread-safe */
		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			write_log("%s\n", segdbDesc->error_message.data);

		PQfinish(segdbDesc->conn);
		segdbDesc->conn = NULL;
	}

	/*
	 * Successfully connected.
	 */
	else
	{
		PQsetNoticeReceiver(segdbDesc->conn, &MPPnoticeReceiver, segdbDesc);

		/*
		 * Command the QE to initialize its motion layer. Wait for it to
		 * respond giving us the TCP port number where it listens for
		 * connections from the gang below.
		 */
		segdbDesc->motionListener = cdbconn_get_motion_listener_port(segdbDesc->conn);
		segdbDesc->backendPid = PQbackendPID(segdbDesc->conn);
		if (segdbDesc->motionListener == 0)
		{
			segdbDesc->errcode = ERRCODE_INTERNAL_ERROR;
			appendPQExpBuffer(&segdbDesc->error_message,
							  "Internal error: No motion listener port");

			if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
				write_log("%s\n", segdbDesc->error_message.data);

			PQfinish(segdbDesc->conn);
			segdbDesc->conn = NULL;
		}
		else
		{
			if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
				write_log("Connected to %s motionListener=%u/%u with options: %s\n",
						  segdbDesc->whoami,
						  (segdbDesc->motionListener & 0x0ffff),
						  ((segdbDesc->motionListener >> 16) & 0x0ffff),
						  options);
		}
	}
}

/*
 * Establish socket connection via libpq.
 * Caller should call PQconnectPoll to finish it up.
 */
void
cdbconn_doConnectStart(SegmentDatabaseDescriptor *segdbDesc,
					   const char *gpqeid,
					   const char *options)
{
#define MAX_KEYWORDS 10
#define MAX_INT_STRING_LEN 20
	CdbComponentDatabaseInfo *cdbinfo = segdbDesc->segment_database_info;
	const char *keywords[MAX_KEYWORDS];
	const char *values[MAX_KEYWORDS];
	char		portstr[MAX_INT_STRING_LEN];
	int			nkeywords = 0;

	keywords[nkeywords] = "gpqeid";
	values[nkeywords] = gpqeid;
	nkeywords++;

	/*
	 * Build the connection string
	 */
	if (options)
	{
		keywords[nkeywords] = "options";
		values[nkeywords] = options;
		nkeywords++;
	}

	/*
	 * For entry DB connection, we make sure both "hostaddr" and "host" are
	 * empty string. Or else, it will fall back to environment variables and
	 * won't use domain socket in function connectDBStart.
	 *
	 * For other QE connections, we set "hostaddr". "host" is not used.
	 */
	if (segdbDesc->segindex == MASTER_CONTENT_ID &&
		GpIdentity.segindex == MASTER_CONTENT_ID)
	{
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = "";
		nkeywords++;
	}
	else
	{
		Assert(cdbinfo->hostip != NULL);
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = cdbinfo->hostip;
		nkeywords++;
	}

	keywords[nkeywords] = "host";
	values[nkeywords] = "";
	nkeywords++;

	snprintf(portstr, sizeof(portstr), "%u", cdbinfo->port);
	keywords[nkeywords] = "port";
	values[nkeywords] = portstr;
	nkeywords++;

	if (MyProcPort->database_name)
	{
		keywords[nkeywords] = "dbname";
		values[nkeywords] = MyProcPort->database_name;
		nkeywords++;
	}

	Assert(MyProcPort->user_name);
	keywords[nkeywords] = "user";
	values[nkeywords] = MyProcPort->user_name;
	nkeywords++;

	keywords[nkeywords] = NULL;
	values[nkeywords] = NULL;

	Assert(nkeywords < MAX_KEYWORDS);

	segdbDesc->conn = PQconnectStartParams(keywords, values, false);
	return;
}

void
cdbconn_doConnectComplete(SegmentDatabaseDescriptor *segdbDesc)
{
	PQsetNoticeReceiver(segdbDesc->conn, &MPPnoticeReceiver, segdbDesc);

	/*
	 * Command the QE to initialize its motion layer. Wait for it to respond
	 * giving us the TCP port number where it listens for connections from the
	 * gang below.
	 */
	segdbDesc->motionListener = cdbconn_get_motion_listener_port(segdbDesc->conn);
	segdbDesc->backendPid = PQbackendPID(segdbDesc->conn);

	if (segdbDesc->motionListener != 0 &&
		gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "Connected to %s motionListenerPorts=%u/%u with options %s",
			 segdbDesc->whoami,
			 (segdbDesc->motionListener & 0x0ffff),
			 ((segdbDesc->motionListener >> 16) & 0x0ffff),
			 PQoptions(segdbDesc->conn));
	}
}

/* Disconnect from QE */
void
cdbconn_disconnect(SegmentDatabaseDescriptor *segdbDesc)
{
	if (PQstatus(segdbDesc->conn) != CONNECTION_BAD)
	{
		PGTransactionStatusType status = PQtransactionStatus(segdbDesc->conn);

		if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "Finishing connection with %s; %s", segdbDesc->whoami, transStatusToString(status));

		if (status == PQTRANS_ACTIVE)
		{
			char		errbuf[256];
			bool		sent;

			memset(errbuf, 0, sizeof(errbuf));

			if (Debug_cancel_print || gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
				elog(LOG, "Calling PQcancel for %s", segdbDesc->whoami);

			sent = cdbconn_signalQE(segdbDesc, errbuf, true);
			if (!sent)
				elog(LOG, "Unable to cancel: %s", strlen(errbuf) == 0 ? "cannot allocate PGCancel" : errbuf);
		}

		PQfinish(segdbDesc->conn);
		segdbDesc->conn = NULL;
	}
}

/*
 * Read result from connection and discard it.
 *
 * Retry at most N times.
 *
 * Return false if there'er still leftovers.
 */
bool
cdbconn_discardResults(SegmentDatabaseDescriptor *segdbDesc,
					   int retryCount)
{
	PGresult   *pRes = NULL;
	ExecStatusType stat;
	int			i = 0;

	/* PQstatus() is smart enough to handle NULL */
	while (NULL != (pRes = PQgetResult(segdbDesc->conn)))
	{
		stat = PQresultStatus(pRes);
		PQclear(pRes);

		elog(LOG, "(%s) Leftover result at freeGang time: %s %s", segdbDesc->whoami,
			 PQresStatus(stat),
			 PQerrorMessage(segdbDesc->conn));

		if (stat == PGRES_FATAL_ERROR || stat == PGRES_BAD_RESPONSE)
			return true;

		if (i++ > retryCount)
			return false;
	}

	return true;
}

/* Return if it's a bad connection */
bool
cdbconn_isBadConnection(SegmentDatabaseDescriptor *segdbDesc)
{
	return (segdbDesc->conn == NULL || PQsocket(segdbDesc->conn) < 0 ||
			PQstatus(segdbDesc->conn) == CONNECTION_BAD);
}

/* Return if it's a connection OK */
bool
cdbconn_isConnectionOk(SegmentDatabaseDescriptor *segdbDesc)
{
	return (PQstatus(segdbDesc->conn) == CONNECTION_OK);
}

/* Reset error message buffer */
void
cdbconn_resetQEErrorMessage(SegmentDatabaseDescriptor *segdbDesc)
{
	segdbDesc->errcode = 0;
	resetPQExpBuffer(&segdbDesc->error_message);
}

/*
 * Build text to identify this QE in error messages.
 * Don't call this function in threads.
 */
void
setQEIdentifier(SegmentDatabaseDescriptor *segdbDesc,
				int sliceIndex, MemoryContext mcxt)
{
	CdbComponentDatabaseInfo *cdbinfo = segdbDesc->segment_database_info;
	MemoryContext oldContext = MemoryContextSwitchTo(mcxt);
	StringInfoData string;

	initStringInfo(&string);

	/* Format the identity of the segment db. */
	if (segdbDesc->segindex >= 0)
	{
		appendStringInfo(&string, "seg%d", segdbDesc->segindex);

		/* Format the slice index. */
		if (sliceIndex > 0)
			appendStringInfo(&string, " slice%d", sliceIndex);
	}
	else
		appendStringInfo(&string, SEGMENT_IS_ACTIVE_PRIMARY(cdbinfo) ? "entry db" : "mirror entry db");

	/* Format the connection info. */
	appendStringInfo(&string, " %s:%d", cdbinfo->hostip, cdbinfo->port);

	/* If connected, format the QE's process id. */
	if (segdbDesc->backendPid != 0)
		appendStringInfo(&string, " pid=%d", segdbDesc->backendPid);

	if (segdbDesc->whoami != NULL)
		pfree(segdbDesc->whoami);
	segdbDesc->whoami = string.data;
	MemoryContextSwitchTo(oldContext);
}

/*
 * Send cancel/finish signal to still-running QE through libpq.
 *
 * errbuf is used to return error message(recommended size is 256 bytes).
 *
 * Returns true if we successfully sent a signal
 * (not necessarily received by the target process).
 */
bool
cdbconn_signalQE(SegmentDatabaseDescriptor *segdbDesc,
				 char *errbuf,
				 bool isCancel)
{
	bool		ret;

	PGcancel   *cn = PQgetCancel(segdbDesc->conn);

	if (cn == NULL)
		return false;

	if (isCancel)
		ret = PQcancel(cn, errbuf, 256);
	else
		ret = PQrequestFinish(cn, errbuf, 256);

	PQfreeCancel(cn);
	return ret;
}


/* GPDB function to retrieve QE-backend details (motion listener) */
static uint32
cdbconn_get_motion_listener_port(PGconn *conn)
{
	const char *val;
	char	   *endptr;
	uint32		result;

	val = PQparameterStatus(conn, "qe_listener_port");
	if (!val)
		return 0;

	errno = 0;
	result = strtoul(val, &endptr, 10);
	if (endptr == val || *endptr != '\0' || errno == ERANGE)
		return 0;

	return result;
}
