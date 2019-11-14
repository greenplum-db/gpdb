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

#include "commands/dbcommands.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "libpq/libpq.h"
#include "libpq/libpq-be.h"
#include "libpq/pqformat.h"

#include "cdb/cdbconn.h"		/* me */
#include "cdb/cdbutil.h"		/* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"
#include "cdb/cdbgang.h"


static uint32 cdbconn_get_motion_listener_port(PGconn *conn);
static void cdbconn_disconnect(SegmentDatabaseDescriptor *segdbDesc);

static void MPPnoticeReceiver(void *arg, const PGresult *res);

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

/* Initialize a QE connection descriptor in CdbComponentsContext */
SegmentDatabaseDescriptor *
cdbconn_createSegmentDescriptor(struct CdbComponentDatabaseInfo *cdbinfo, int identifier, bool isWriter)
{
	MemoryContext oldContext;
	SegmentDatabaseDescriptor *segdbDesc = NULL;

	Assert(CdbComponentsContext);
	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

	segdbDesc = (SegmentDatabaseDescriptor *)palloc0(sizeof(SegmentDatabaseDescriptor));

	/* Segment db info */
	segdbDesc->segment_database_info = cdbinfo;
	segdbDesc->segindex = cdbinfo->config->segindex;

	/* Connection info, set in function cdbconn_doConnect */
	segdbDesc->conn = NULL;
	segdbDesc->motionListener = 0;
	segdbDesc->backendPid = 0;

	/* whoami */
	segdbDesc->whoami = NULL;
	segdbDesc->identifier = identifier;
	segdbDesc->isWriter = isWriter;

	MemoryContextSwitchTo(oldContext);
	return segdbDesc;
}

/* Free memory of segment descriptor. */
void
cdbconn_termSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc)
{
	CdbComponentDatabases *cdbs;

	Assert(CdbComponentsContext);

	cdbs = segdbDesc->segment_database_info->cdbs;

	/* put qe identifier to free list for reuse */
	cdbs->freeCounterList = lappend_int(cdbs->freeCounterList, segdbDesc->identifier);

	cdbconn_disconnect(segdbDesc);

	if (segdbDesc->whoami != NULL)
	{
		pfree(segdbDesc->whoami);
		segdbDesc->whoami = NULL;
	}
}								/* cdbconn_termSegmentDescriptor */

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
	 * won't use domain socket in function connectDBStart. Also we set the
	 * connection type for entrydb connection so that QE could change Gp_role
	 * from DISPATCH to EXECUTE.
	 *
	 * For other QE connections, we set "hostaddr". "host" is not used.
	 */
	if (segdbDesc->segindex == MASTER_CONTENT_ID &&
		IS_QUERY_DISPATCHER())
	{
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = "";
		nkeywords++;
	}
	else
	{
		Assert(cdbinfo->config->hostip != NULL);
		keywords[nkeywords] = "hostaddr";
		values[nkeywords] = cdbinfo->config->hostip;
		nkeywords++;
	}

	keywords[nkeywords] = "host";
	values[nkeywords] = "";
	nkeywords++;

	snprintf(portstr, sizeof(portstr), "%u", cdbinfo->config->port);
	keywords[nkeywords] = "port";
	values[nkeywords] = portstr;
	nkeywords++;

	keywords[nkeywords] = "dbname";
	if(MyProcPort && MyProcPort->database_name)
	{
		values[nkeywords] = MyProcPort->database_name;
	}
	else
	{
		/*
		 * get database name from MyDatabaseId, which is initialized
		 * in InitPostgres()
		 */
		Assert(MyDatabaseId != InvalidOid);
		values[nkeywords] = get_database_name(MyDatabaseId);
	}
	nkeywords++;

	/*
	 * Set the client encoding to match database encoding in QD->QE
	 * connections.  All the strings dispatched from QD to be in the database
	 * encoding, and all strings sent back to the QD will also be in the
	 * database encoding.
	 *
	 * Most things don't pay attention to client_encoding in QE processes:
	 * query results are normally sent back via the interconnect, and the 'M'
	 * type QD->QE messages, used to dispatch queries, don't perform encoding
	 * conversion.  But some things, like error messages, and internal
	 * commands dispatched directly with CdbDispatchCommand, do care.
	 */
	keywords[nkeywords] = "client_encoding";
	values[nkeywords] = GetDatabaseEncodingName();
	nkeywords++;

	keywords[nkeywords] = "user";
	if (MyProcPort && MyProcPort->user_name)
	{
		values[nkeywords] = MyProcPort->user_name;
	}
	else
	{
		/*
		 * get user name from AuthenticatedUserId which is initialized
		 * in InitPostgres()
		 */
		values[nkeywords] = GetUserNameFromId(GetAuthenticatedUserId(), false);
	}
	nkeywords++;

	keywords[nkeywords] = GPCONN_TYPE;
	values[nkeywords] = GPCONN_TYPE_INTERNAL;
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
static void
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
	bool retval = true;

	/* PQstatus() is smart enough to handle NULL */
	while (NULL != (pRes = PQgetResult(segdbDesc->conn)))
	{
		stat = PQresultStatus(pRes);
		PQclear(pRes);

		elog(LOG, "(%s) Leftover result at freeGang time: %s %s", segdbDesc->whoami,
			 PQresStatus(stat),
			 PQerrorMessage(segdbDesc->conn));

		if (stat == PGRES_FATAL_ERROR || stat == PGRES_BAD_RESPONSE)
		{
			retval = true;
			break;
		}

		if (i++ > retryCount)
		{
			retval = false;
			break;
		}
	}

	/*
	 * Clear of all the notify messages as well.
	 */
	PGnotify   *notify = segdbDesc->conn->notifyHead;
	while (notify != NULL)
	{
		PGnotify   *prev = notify;
		notify = notify->next;
		PQfreemem(prev);
	}
	segdbDesc->conn->notifyHead = segdbDesc->conn->notifyTail = NULL;

	return retval;
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

/*
 * Build text to identify this QE in error messages.
 * Don't call this function in threads.
 */
void
cdbconn_setQEIdentifier(SegmentDatabaseDescriptor *segdbDesc,
				int sliceIndex)
{
	CdbComponentDatabaseInfo *cdbinfo = segdbDesc->segment_database_info;
	StringInfoData string;
	MemoryContext oldContext;

	Assert(CdbComponentsContext);
	oldContext = MemoryContextSwitchTo(CdbComponentsContext);

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
	appendStringInfo(&string, " %s:%d", cdbinfo->config->hostip, cdbinfo->config->port);

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


/*-------------------------------------------------------------------------
 * QE Notice receiver support
 *
 * When a QE process emits a NOTICE (or WARNING, INFO, etc.) message, it
 * needs to be delivered to the user. To do that, we install a libpq Notice
 * receiver callback to every QD->QE connection.
 *
 * The callback is very limited in what it can do, so it cannot directly
 * forward the Notice to the user->QD connection. Instead, it queues the
 * Notices as a list of QENotice structs. Later, when we are out of the
 * callback, forwardQENotices() sends the queued Notices to the client.
 *-------------------------------------------------------------------------
 */

typedef struct QENotice QENotice;
struct QENotice
{
	QENotice   *next;

	int			elevel;
	char		sqlstate[6];
	char		severity[10];
	char	   *file;
	char		line[10];
	char	   *func;
	char	   *message;
	char	   *detail;
	char	   *hint;
	char	   *context;

	char		buf[];
};

static QENotice *qeNotices_head = NULL;
static QENotice *qeNotices_tail = NULL;

/*
 * libpq Notice receiver callback.
 *
 * NB: This is a callback, so we are very limited in what we can do. In
 * particular, we must not call ereport() or elog(), which might longjmp()
 * out of the callback. Libpq might get confused by that. That also means
 * that we cannot call palloc()!
 *
 * A QENotice struct is created for each incoming Notice, and put in a
 * queue for later processing. The QENotices are allocatd with good old
 * malloc()!
 */
static void
MPPnoticeReceiver(void *arg, const PGresult *res)
{
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

	/*
	 * If MyProcPort is NULL, there is no client, so no need to generate notice.
	 * One example is that there is no client for a background worker.
	 */
	if (!res || MyProcPort == NULL) 
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

	/*
	 * If this message is filtered out by client_min_messages, we have nothing
	 * to do. (The QE shouldn't have sent it to us in the first place...)
	 */
	if (elevel >= client_min_messages || elevel == INFO)
	{
		QENotice   *notice;
		uint64		size;
		char	   *bufptr;
		int			file_len;
		int			func_len;
		int			message_len;
		int			detail_len;
		int			hint_len;
		int			context_len;

		/*
		 * We use malloc(), because we are in a libpq callback, and we CANNOT
		 * use palloc(). We allocate space for the QENotice and the strings in
		 * a single malloc() call.
		 */

		/*
		 * First, compute the required size of the allocation.
		 */

/* helper macro for computing the total allocation size */
#define SIZE_VARLEN_FIELD(fldname) \
		if (fldname != NULL) \
		{ \
			fldname##_len = strlen(fldname) + 1; \
			size += fldname##_len; \
		} \
		else \
			fldname##_len = 0

		size = offsetof(QENotice, buf);
		SIZE_VARLEN_FIELD(file);
		SIZE_VARLEN_FIELD(func);
		SIZE_VARLEN_FIELD(message);
		SIZE_VARLEN_FIELD(detail);
		SIZE_VARLEN_FIELD(hint);
		SIZE_VARLEN_FIELD(context);

		/*
		 * Perform the allocation.  Put a limit on the max size, as a sanity
		 * check.  (The libpq protocol itself limits the size the message can
		 * be, but better safe than sorry.)
		 *
		 * We can't ereport() if this fails, so we just drop the notice to
		 * the floor. Hope it wasn't important...
		 */
		if (size >= MaxAllocSize)
			return;

		notice = malloc(size);
		if (!notice)
			return;

		/*
		 * Allocation succeeded.  Now fill in the struct.
		 */
		bufptr = notice->buf;

#define COPY_VARLEN_FIELD(fldname) \
		if (fldname != NULL) \
		{ \
			notice->fldname = bufptr; \
			memcpy(bufptr, fldname, fldname##_len); \
			bufptr += fldname##_len; \
		} \
		else \
			notice->fldname = NULL

		notice->elevel = elevel;
		strlcpy(notice->sqlstate, sqlstate, sizeof(notice->sqlstate));
		strlcpy(notice->severity, severity, sizeof(notice->severity));
		COPY_VARLEN_FIELD(file);
		strlcpy(notice->line, line, sizeof(notice->line));
		COPY_VARLEN_FIELD(func);
		COPY_VARLEN_FIELD(message);
		COPY_VARLEN_FIELD(detail);
		COPY_VARLEN_FIELD(hint);
		COPY_VARLEN_FIELD(context);

		Assert(bufptr - (char *) notice == size);

		/* Link it to the queue */
		notice->next = NULL;
		if (qeNotices_tail)
		{
			qeNotices_tail->next = notice;
			qeNotices_tail = notice;
		}
		else
			qeNotices_tail = qeNotices_head = notice;
	}
}

/*
 * Send all Notices to the client, that we have accumulated from QEs since last
 * call.
 *
 * This should be called after every libpq call that might read from the QD->QE
 * connection, so that the notices are sent to the user in a timely fashion.
 */
void
forwardQENotices(void)
{
	bool hasNotices = false;

	while (qeNotices_head)
	{
		QENotice *notice;
		StringInfoData msgbuf;

		notice = qeNotices_head;
		hasNotices = true;

		/*
		 * Unlink it first, so that if something goes wrong in sending it to
		 * the client, we don't get stuck in a loop trying to send the same
		 * message again and again.
		 */
		qeNotices_head = notice->next;
		if (qeNotices_head == NULL)
			qeNotices_tail = NULL;

		/*
		 * Use PG_TRY() - PG_CATCH() to make sure we free the struct, no
		 * matter what.
		 */
		PG_TRY();
		{
			/* 'N' (Notice) is for nonfatal conditions, 'E' is for errors */
			pq_beginmessage(&msgbuf, 'N');

			if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
			{
				/* New style with separate fields */
				pq_sendbyte(&msgbuf, PG_DIAG_SEVERITY);
				pq_sendstring(&msgbuf, notice->severity);

				pq_sendbyte(&msgbuf, PG_DIAG_SQLSTATE);
				pq_sendstring(&msgbuf, notice->sqlstate);

				/* M field is required per protocol, so always send something */
				pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_PRIMARY);
				pq_sendstring(&msgbuf, notice->message);

				if (notice->detail)
				{
					pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_DETAIL);
					pq_sendstring(&msgbuf, notice->detail);
				}

				if (notice->hint)
				{
					pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_HINT);
					pq_sendstring(&msgbuf, notice->hint);
				}

				if (notice->context)
				{
					pq_sendbyte(&msgbuf, PG_DIAG_CONTEXT);
					pq_sendstring(&msgbuf, notice->context);
				}

				if (notice->file)
				{
					pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FILE);
					pq_sendstring(&msgbuf, notice->file);
				}

				if (notice->line[0])
				{
					pq_sendbyte(&msgbuf,PG_DIAG_SOURCE_LINE);
					pq_sendstring(&msgbuf, notice->line);
				}

				if (notice->func)
				{
					pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FUNCTION);
					pq_sendstring(&msgbuf, notice->func);
				}

				pq_sendbyte(&msgbuf, '\0');		/* terminator */
			}
			else
			{
				/* Old style --- gin up a backwards-compatible message */
				StringInfoData buf;

				initStringInfo(&buf);

				appendStringInfo(&buf, "%s:  ", notice->severity);

				if (notice->func)
					appendStringInfo(&buf, "%s: ", notice->func);

				if (notice->message)
					appendStringInfoString(&buf, notice->message);
				else
					appendStringInfoString(&buf, _("missing error text"));

				appendStringInfoChar(&buf, '\n');

				pq_sendstring(&msgbuf, buf.data);

				pfree(buf.data);
			}

			pq_endmessage(&msgbuf);
			free(notice);
		}
		PG_CATCH();
		{
			free(notice);
			PG_RE_THROW();
		}
		PG_END_TRY();
	}
	if (hasNotices)
		pq_flush();
}
