/*-------------------------------------------------------------------------
 *
 * cdbconn.c
 *
 * SegmentDatabaseDescriptor methods
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "libpq/libpq-be.h"

extern int	pq_flush(void);
extern int	pq_putmessage(char msgtype, const char *s, size_t len);

#include "cdb/cdbconn.h"            /* me */
#include "cdb/cdbutil.h"            /* CdbComponentDatabaseInfo */
#include "cdb/cdbvars.h"

int		gp_segment_connect_timeout = 180;

static void MPPnoticeReceiver(void * arg, const PGresult * res)
{
	PQExpBufferData             msgbuf;
	PGMessageField *pfield;
	int elevel = INFO;
	char * sqlstate = "00000";
	char * severity = "WARNING";
	char * file = "";
	char * line = NULL;
	char * func = "";
	char  message[1024];
	char * detail = NULL;
	char * hint = NULL;
	char * context = NULL;
	
	SegmentDatabaseDescriptor    *segdbDesc = (SegmentDatabaseDescriptor    *) arg;
	if (!res)
		return;
		
	
	strcpy(message,"missing error text");
	
	for (pfield = res->errFields; pfield != NULL; pfield = pfield->next)
	{
		switch (pfield->code)
		{
			case PG_DIAG_SEVERITY:
				severity = pfield->contents;
				if (strcmp(pfield->contents,"WARNING")==0)
					elevel = WARNING;
				else if (strcmp(pfield->contents,"NOTICE")==0)
					elevel = NOTICE;
				else if (strcmp(pfield->contents,"DEBUG1")==0 ||
					     strcmp(pfield->contents,"DEBUG")==0)
					elevel = DEBUG1;
				else if (strcmp(pfield->contents,"DEBUG2")==0)
					elevel = DEBUG2;
				else if (strcmp(pfield->contents,"DEBUG3")==0)
					elevel = DEBUG3;
				else if (strcmp(pfield->contents,"DEBUG4")==0)
					elevel = DEBUG4;
				else if (strcmp(pfield->contents,"DEBUG5")==0)
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
					strcat(message,"  (");
					strcat(message, segdbDesc->whoami);
					strcat(message,")");
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
	
	if (elevel < client_min_messages &&  elevel  != INFO)
		return;

    /*
     * We use PQExpBufferData instead of StringInfoData
     * because the former uses malloc, the latter palloc.
     * We are in a thread, and we CANNOT use palloc since it's not
     * thread safe.  We cannot call elog or ereport either for the
     * same reason.
     */
    initPQExpBuffer(&msgbuf);
	

	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* New style with separate fields */

		appendPQExpBufferChar(&msgbuf, PG_DIAG_SEVERITY);
		appendBinaryPQExpBuffer(&msgbuf, severity, strlen(severity)+1);

		appendPQExpBufferChar(&msgbuf, PG_DIAG_SQLSTATE);
		appendBinaryPQExpBuffer(&msgbuf, sqlstate, strlen(sqlstate)+1);

		/* M field is required per protocol, so always send something */
		appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_PRIMARY);
        appendBinaryPQExpBuffer(&msgbuf, message , strlen(message) + 1);

		if (detail)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_DETAIL);
			appendBinaryPQExpBuffer(&msgbuf, detail, strlen(detail)+1);
		}

		if (hint)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_MESSAGE_HINT);
			appendBinaryPQExpBuffer(&msgbuf, hint, strlen(hint)+1);
		}

		if (context)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_CONTEXT);
			appendBinaryPQExpBuffer(&msgbuf, context, strlen(context)+1);
		}

		/*
		  if (edata->cursorpos > 0)
		  {
		  snprintf(tbuf, sizeof(tbuf), "%d", edata->cursorpos);
		  appendPQExpBufferChar(&msgbuf, PG_DIAG_STATEMENT_POSITION);
		  appendBinaryPQExpBuffer(&msgbuf, tbuf);
		  }

		  if (edata->internalpos > 0)
		  {
		  snprintf(tbuf, sizeof(tbuf), "%d", edata->internalpos);
		  appendPQExpBufferChar(&msgbuf, PG_DIAG_INTERNAL_POSITION);
		  appendBinaryPQExpBuffer(&msgbuf, tbuf);
		  }

		  if (edata->internalquery)
		  {
		  appendPQExpBufferChar(&msgbuf, PG_DIAG_INTERNAL_QUERY);
		  appendBinaryPQExpBuffer(&msgbuf, edata->internalquery);
		  }
		*/
		if (file)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_FILE);
			appendBinaryPQExpBuffer(&msgbuf, file, strlen(file)+1);
		}

		if (line)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_LINE);
			appendBinaryPQExpBuffer(&msgbuf, line, strlen(line)+1);
		}

		if (func)
		{
			appendPQExpBufferChar(&msgbuf, PG_DIAG_SOURCE_FUNCTION);
			appendBinaryPQExpBuffer(&msgbuf, func, strlen(func)+1);
		}
		
	}
	else
	{
			
		appendPQExpBuffer(&msgbuf, "%s:  ", severity);
	
		appendBinaryPQExpBuffer(&msgbuf, message, strlen(message));
	
		appendPQExpBufferChar(&msgbuf, '\n');
		appendPQExpBufferChar(&msgbuf, '\0');

	}

	appendPQExpBufferChar(&msgbuf, '\0');		/* terminator */

	pq_putmessage('N', msgbuf.data, msgbuf.len);
	
	termPQExpBuffer(&msgbuf);
	
	pq_flush();	
}


/* Initialize a QE connection descriptor in storage provided by the caller. */
void
cdbconn_initSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc,
                              struct CdbComponentDatabaseInfo  *cdbinfo)
{
    MemSet(segdbDesc, 0, sizeof(*segdbDesc));

    /* Segment db info */
    segdbDesc->segment_database_info = cdbinfo;
    segdbDesc->segindex = cdbinfo->segindex;
    //segdbDesc->dbname = MyProcPort->database_name ? strdup(MyProcPort->database_name) : NULL;
	//segdbDesc->username = MyProcPort->user_name ? strdup(MyProcPort->user_name) : NULL;

    /* Connection info */
    segdbDesc->conn = NULL;
    segdbDesc->motionListener = 0;
    segdbDesc->whoami = NULL;
    segdbDesc->myAgent = NULL;

    /* Connection error info */
    segdbDesc->errcode = 0;
    initPQExpBuffer(&segdbDesc->error_message);
}                               /* cdbconn_initSegmentDescriptor */


/* Free all memory owned by this segment descriptor. */
void
cdbconn_termSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc)
{
    /* Free the error message buffer. */
    segdbDesc->errcode = 0;
    termPQExpBuffer(&segdbDesc->error_message);

    /* Free connection info. */
    if (segdbDesc->whoami)
    {
        free(segdbDesc->whoami);
        segdbDesc->whoami = NULL;
    }
}                               /* cdbconn_termSegmentDescriptor */


/* Connect to a QE as a client via libpq. */
bool                            /* returns true if connected */
cdbconn_doConnect(SegmentDatabaseDescriptor *segdbDesc,
				  const char *gpqeid,
				  const char *options)
{
    CdbComponentDatabaseInfo   *q = segdbDesc->segment_database_info;
#define MAX_KEYWORDS 10
	const char *keywords[MAX_KEYWORDS];
	const char *values[MAX_KEYWORDS];
	int			nkeywords = 0;
	char		portstr[20];
	char		timeoutstr[20];

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
	 * On the master, we must use UNIX domain sockets for security -- as it can
	 * be authenticated. See MPP-15802.
	 */
	if (!(q->segindex == MASTER_CONTENT_ID &&
			GpIdentity.segindex == MASTER_CONTENT_ID))
	{
		/*
		 * First we pick the cached hostip if we have it.
		 *
		 * If we don't have a cached hostip, we use the host->address,
		 * if we don't have that we fallback to host->hostname.
		 */
		if (q->hostip != NULL)
		{
			keywords[nkeywords] = "hostaddr";
			values[nkeywords] = q->hostip;
			nkeywords++;
		}
		else if (q->address != NULL)
		{
			if (isdigit(q->address[0]))
			{
				keywords[nkeywords] = "hostaddr";
				values[nkeywords] = q->address;
				nkeywords++;
			}
			else
			{
				keywords[nkeywords] = "host";
				values[nkeywords] = q->address;
				nkeywords++;
			}
		}
	    else if (q->hostname == NULL)
		{
			keywords[nkeywords] = "host";
			values[nkeywords] = "";
			nkeywords++;
		}
	    else if (isdigit(q->hostname[0]))
		{
			keywords[nkeywords] = "hostaddr";
			values[nkeywords] = q->hostname;
			nkeywords++;
		}
	    else
		{
			keywords[nkeywords] = "host";
			values[nkeywords] = q->hostname;
			nkeywords++;
		}
	}

	snprintf(portstr, sizeof(portstr), "%u", q->port);
	keywords[nkeywords] = "port";
	values[nkeywords] = portstr;
	nkeywords++;

    if (MyProcPort->database_name)
	{
		keywords[nkeywords] = "dbname";
		values[nkeywords] = MyProcPort->database_name;
		nkeywords++;
	}

	keywords[nkeywords] = "user";
	values[nkeywords] = MyProcPort->user_name;
	nkeywords++;

	snprintf(timeoutstr, sizeof(timeoutstr), "%d", gp_segment_connect_timeout);
	keywords[nkeywords] = "connect_timeout";
	values[nkeywords] = timeoutstr;
	nkeywords++;

	keywords[nkeywords] = NULL;
	values[nkeywords] = NULL;

	Assert (nkeywords < MAX_KEYWORDS);

    /*
     * Call libpq to connect
     */
    segdbDesc->conn = PQconnectdbParams(keywords, values, false);

    /* Build whoami string to identify the QE for use in messages. */
    if(!cdbconn_setSliceIndex(segdbDesc, -1))
    {
        if (!segdbDesc->errcode)
            segdbDesc->errcode = ERRCODE_GP_INTERCONNECTION_ERROR;

        /* Don't use elog, it's not thread-safe */
        if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
            write_log("%s\n", segdbDesc->error_message.data);

        PQfinish(segdbDesc->conn);
        segdbDesc->conn = NULL;
    }

    /*
     * Check for connection failure.
     */
    if (PQstatus(segdbDesc->conn) == CONNECTION_BAD)
    {
        if (!segdbDesc->errcode)
            segdbDesc->errcode = ERRCODE_GP_INTERCONNECTION_ERROR;
        appendPQExpBuffer(&segdbDesc->error_message,
                          "Master unable to connect to %s with options %s\n",
                          segdbDesc->whoami,
                          PQerrorMessage(segdbDesc->conn));

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
        /* Command the QE to initialize its motion layer.
         * Wait for it to respond giving us the TCP port number
         * where it listens for connections from the gang below.
         */
        segdbDesc->motionListener = PQgetQEdetail(segdbDesc->conn, false);
        
        segdbDesc->backendPid = PQbackendPID(segdbDesc->conn);

        /* Don't use elog, it's not thread-safe */
        if (gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
            write_log("Connected to %s motionListener=%d\n",
						 segdbDesc->whoami,
						 segdbDesc->motionListener);
    }

    return segdbDesc->conn != NULL;
}                               /* cdbconn_doConnect */

/* Build text to identify this QE in error messages. */
bool
cdbconn_setSliceIndex(SegmentDatabaseDescriptor    *segdbDesc,
                      int                           sliceIndex)
{
    CdbComponentDatabaseInfo   *q = segdbDesc->segment_database_info;
    PQExpBuffer scratchbuf = &segdbDesc->error_message;
    int         scratchoff = scratchbuf->len;
    Assert(scratchbuf->len < 300000);

    /* Format the identity of the segment db. */
    if (q->segindex >= 0)
    {
        appendPQExpBuffer(scratchbuf, "seg%d", q->segindex);

        /* Format the slice index. */
        if (sliceIndex > 0)
            appendPQExpBuffer(scratchbuf, " slice%d", sliceIndex);
    }
    else
        appendPQExpBuffer(scratchbuf,
                          SEGMENT_IS_ACTIVE_PRIMARY(q) ? "entry db" : "mirror entry db");

    /* Format the connection info. */
    appendPQExpBuffer(scratchbuf, " %s:%d",
                      q->hostname, q->port);

    /* If connected, format the QE's process id. */
    if (segdbDesc->conn)
    {
        int pid = PQbackendPID(segdbDesc->conn);
        if (pid)
            appendPQExpBuffer(scratchbuf, " pid=%d", pid);
    }

    /* Store updated whoami text. */
    if (segdbDesc->whoami != NULL)
	    free(segdbDesc->whoami);

    segdbDesc->whoami = strdup(scratchbuf->data + scratchoff);
    if(!segdbDesc->whoami)
    {
	    appendPQExpBuffer(scratchbuf, " Error: Out of Memory");
	    return false;
    }

    /* Give back our scratch space at tail of error_message buffer. */
    truncatePQExpBuffer(scratchbuf, scratchoff);
    return true;
}                               /* cdbconn_setSliceIndex */
