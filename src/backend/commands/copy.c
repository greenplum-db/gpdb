/*-------------------------------------------------------------------------
 *
 * copy.c
 *		Implements the COPY utility command
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/copy.c,v 1.312 2009/06/11 14:48:55 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "access/fileam.h"
#include "access/heapam.h"
#include "access/appendonlywriter.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbpartition.h"
#include "commands/copy.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "commands/queue.h"
#include "executor/executor.h"
#include "executor/execDML.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/planner.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbcopy.h"
#include "cdb/cdbsreh.h"
#include "postmaster/autostats.h"
#include "utils/resscheduler.h"

extern int popen_with_stderr(int *rwepipe, const char *exe, bool forwrite);
extern int pclose_with_stderr(int pid, int *rwepipe, StringInfo sinfo);
extern char *make_command(const char *cmd, extvar_t *ev);

/* DestReceiver for COPY (SELECT) TO */
typedef struct
{
	DestReceiver pub;			/* publicly-known function pointers */
	CopyState	cstate;			/* CopyStateData for the command */
} DR_copy;

static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";


/* non-export function prototypes */
static void DoCopyTo(CopyState cstate);
extern void CopyToDispatch(CopyState cstate);
static void CopyTo(CopyState cstate);
extern void CopyFromDispatch(CopyState cstate);
static void CopyFrom(CopyState cstate);
static void CopyFromProcessDataFileHeader(CopyState cstate, CdbCopy *cdbCopy, bool *pfile_has_oids);
static char *CopyReadOidAttr(CopyState cstate, bool *isnull);
static void CopyAttributeOutText(CopyState cstate, char *string);
static void CopyAttributeOutCSV(CopyState cstate, char *string,
								bool use_quote, bool single_attr);
static bool DetectLineEnd(CopyState cstate, size_t bytesread  __attribute__((unused)));
static void CopyReadAttributesTextNoDelim(CopyState cstate, bool *nulls,
										  int num_phys_attrs, int attnum);
static Datum CopyReadBinaryAttribute(CopyState cstate,
									 int column_no, FmgrInfo *flinfo,
									 Oid typioparam, int32 typmod,
									 bool *isnull, bool skip_parsing);

/* Low-level communications functions */
static void SendCopyBegin(CopyState cstate);
static void ReceiveCopyBegin(CopyState cstate);
static void SendCopyEnd(CopyState cstate);
static void CopySendData(CopyState cstate, const void *databuf, int datasize);
static void CopySendString(CopyState cstate, const char *str);
static void CopySendChar(CopyState cstate, char c);
static int	CopyGetData(CopyState cstate, void *databuf, int datasize);

static void CopySendInt16(CopyState cstate, int16 val);
static void CopySendInt32(CopyState cstate, int32 val);
static bool CopyGetInt16(CopyState cstate, int16 *val);
static bool CopyGetInt32(CopyState cstate, int32 *val);
static bool CopyGetInt64(CopyState cstate, int64 *val);


/* byte scaning utils */
static char *scanTextLine(CopyState cstate, const char *s, char c, size_t len);
static char *scanCSVLine(CopyState cstate, const char *s, char c1, char c2, char c3, size_t len);

static void CopyExtractRowMetaData(CopyState cstate);
static void preProcessDataLine(CopyState cstate);
static void concatenateEol(CopyState cstate);
static char *escape_quotes(const char *src);
static void attr_get_key(CopyState cstate, CdbCopy *cdbCopy, int original_lineno_for_qe,
						 unsigned int target_seg, AttrNumber p_nattrs, AttrNumber *attrs,
						 Form_pg_attribute *attr_descs, int *attr_offsets, bool *attr_nulls,
						 FmgrInfo *in_functions, Oid *typioparams, Datum *values);
static void copy_in_error_callback(void *arg);
static void CopyInitPartitioningState(EState *estate);
static void CopyInitDataParser(CopyState cstate);
static bool CopyCheckIsLastLine(CopyState cstate);
static char *extract_line_buf(CopyState cstate);
uint64
DoCopyInternal(const CopyStmt *stmt, const char *queryString, CopyState cstate);

static GpDistributionData *
InitDistributionData(CopyState cstate, Form_pg_attribute *attr,
                     AttrNumber num_phys_attrs,
                     EState *estate, bool multi_dist_policy);
static void
FreeDistributionData(GpDistributionData *distData);
static void
InitPartitionData(PartitionData *partitionData, EState *estate, Form_pg_attribute *attr,
                  AttrNumber num_phys_attrs, MemoryContext ctxt);
static void
FreePartitionData(PartitionData *partitionData);
static GpDistributionData *
GetDistributionPolicyForPartition(CopyState cstate, EState *estate,
                                  PartitionData *partitionData, HTAB *hashmap,
                                  Oid *p_attr_types,
                                  GetAttrContext *getAttrContext,
                                  MemoryContext ctxt);
static unsigned int
GetTargetSeg(GpDistributionData *distData, Datum *baseValues, bool *baseNulls);
static ProgramPipes *open_program_pipes(char *command, bool forwrite);
static void close_program_pipes(CopyState cstate, bool ifThrow);

/* ==========================================================================
 * The following macros aid in major refactoring of data processing code (in
 * CopyFrom(+Dispatch)). We use macros because in some cases the code must be in
 * line in order to work (for example elog_dismiss() in PG_CATCH) while in
 * other cases we'd like to inline the code for performance reasons.
 *
 * NOTE that an almost identical set of macros exists in fileam.c. If you make
 * changes here you may want to consider taking a look there as well.
 * ==========================================================================
 */

#define RESET_LINEBUF \
cstate->line_buf.len = 0; \
cstate->line_buf.data[0] = '\0'; \
cstate->line_buf.cursor = 0;

#define RESET_ATTRBUF \
cstate->attribute_buf.len = 0; \
cstate->attribute_buf.data[0] = '\0'; \
cstate->attribute_buf.cursor = 0;

#define RESET_LINEBUF_WITH_LINENO \
line_buf_with_lineno.len = 0; \
line_buf_with_lineno.data[0] = '\0'; \
line_buf_with_lineno.cursor = 0;

/*
 * A data error happened. This code block will always be inside a PG_CATCH()
 * block right when a higher stack level produced an error. We handle the error
 * by checking which error mode is set (SREH or all-or-nothing) and do the right
 * thing accordingly. Note that we MUST have this code in a macro (as opposed
 * to a function) as elog_dismiss() has to be inlined with PG_CATCH in order to
 * access local error state variables.
 *
 * changing me? take a look at FILEAM_HANDLE_ERROR in fileam.c as well.
 */
#define COPY_HANDLE_ERROR \
if (cstate->errMode == ALL_OR_NOTHING) \
{ \
	/* re-throw error and abort */ \
	if (Gp_role == GP_ROLE_DISPATCH) \
		cdbCopyEnd(cdbCopy); \
	PG_RE_THROW(); \
} \
else \
{ \
	/* SREH - release error state and handle error */ \
\
	ErrorData	*edata; \
	MemoryContext oldcontext;\
	bool	rawdata_is_a_copy = false; \
	cur_row_rejected = true; \
\
	/* SREH must only handle data errors. all other errors must not be caught */\
	if (ERRCODE_TO_CATEGORY(elog_geterrcode()) != ERRCODE_DATA_EXCEPTION)\
	{\
		/* re-throw error and abort */ \
		if (Gp_role == GP_ROLE_DISPATCH) \
			cdbCopyEnd(cdbCopy); \
		PG_RE_THROW(); \
	}\
\
	/* save a copy of the error info */ \
	oldcontext = MemoryContextSwitchTo(cstate->cdbsreh->badrowcontext);\
	edata = CopyErrorData();\
	MemoryContextSwitchTo(oldcontext);\
\
	if (!elog_dismiss(DEBUG5)) \
		PG_RE_THROW(); /* <-- hope to never get here! */ \
\
	if (Gp_role == GP_ROLE_DISPATCH || cstate->on_segment)\
	{\
		Insist(cstate->err_loc_type == ROWNUM_ORIGINAL);\
		cstate->cdbsreh->rawdata = (char *) palloc(strlen(cstate->line_buf.data) * \
												   sizeof(char) + 1 + 24); \
\
		rawdata_is_a_copy = true; \
		sprintf(cstate->cdbsreh->rawdata, "%d%c%d%c%s", \
			    original_lineno_for_qe, \
				COPY_METADATA_DELIM, \
				cstate->line_buf_converted, \
				COPY_METADATA_DELIM, \
				cstate->line_buf.data);	\
	}\
	else\
	{\
		if (Gp_role == GP_ROLE_EXECUTE)\
		{\
			/* if line has embedded rownum, update the cursor to the pos right after */ \
			Insist(cstate->err_loc_type == ROWNUM_EMBEDDED);\
			cstate->line_buf.cursor = 0;\
			if(!cstate->md_error) \
				CopyExtractRowMetaData(cstate); \
		}\
\
		cstate->cdbsreh->rawdata = cstate->line_buf.data + cstate->line_buf.cursor; \
	}\
\
	cstate->cdbsreh->is_server_enc = cstate->line_buf_converted; \
	cstate->cdbsreh->linenumber = cstate->cur_lineno; \
	cstate->cdbsreh->processed = ++cstate->processed; \
	cstate->cdbsreh->consec_csv_err = cstate->num_consec_csv_err; \
\
	/* set the error message. Use original msg and add column name if available */ \
	if (cstate->cur_attname)\
	{\
		cstate->cdbsreh->errmsg = (char *) palloc((strlen(edata->message) + \
												  strlen(cstate->cur_attname) + \
												  10 + 1) * sizeof(char)); \
		sprintf(cstate->cdbsreh->errmsg, "%s, column %s", \
				edata->message, \
				cstate->cur_attname); \
	}\
	else\
	{\
		cstate->cdbsreh->errmsg = pstrdup(edata->message); \
	}\
\
	/* after all the prep work let cdbsreh do the real work */ \
	HandleSingleRowError(cstate->cdbsreh); \
\
	/* cleanup any extra memory copies we made */\
	if (rawdata_is_a_copy) \
		pfree(cstate->cdbsreh->rawdata); \
	if (!IsRejectLimitReached(cstate->cdbsreh)) \
		pfree(cstate->cdbsreh->errmsg); \
\
	MemoryContextReset(cstate->cdbsreh->badrowcontext);\
\
}

/*
 * if in SREH mode and data error occured it was already handled in
 * COPY_HANDLE_ERROR. Therefore, skip to the next row before attempting
 * to do any further processing on this one. There's a QE and QD versions
 * since the QE doesn't have a linebuf_with_lineno stringInfo.
 */
#define QD_GOTO_NEXT_ROW \
RESET_LINEBUF_WITH_LINENO; \
RESET_LINEBUF; \
cur_row_rejected = false; /* reset for next run */ \
continue; /* move on to the next data line */

#define QE_GOTO_NEXT_ROW \
RESET_LINEBUF; \
cur_row_rejected = false; /* reset for next run */ \
cstate->cur_attname = NULL;\
continue; /* move on to the next data line */

/*
 * Send copy start/stop messages for frontend copies.  These have changed
 * in past protocol redesigns.
 */
static void
SendCopyBegin(CopyState cstate)
{
	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* new way */
		StringInfoData buf;
		int			natts = list_length(cstate->attnumlist);
		int16		format = (cstate->binary ? 1 : 0);
		int			i;

		pq_beginmessage(&buf, 'H');
		pq_sendbyte(&buf, format);		/* overall format */
		pq_sendint(&buf, natts, 2);
		for (i = 0; i < natts; i++)
			pq_sendint(&buf, format, 2);		/* per-column formats */
		pq_endmessage(&buf);
		cstate->copy_dest = COPY_NEW_FE;
	}
	else if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 2)
	{
		/* old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('H');
		/* grottiness needed for old COPY OUT protocol */
		pq_startcopyout();
		cstate->copy_dest = COPY_OLD_FE;
	}
	else
	{
		/* very old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('B');
		/* grottiness needed for old COPY OUT protocol */
		pq_startcopyout();
		cstate->copy_dest = COPY_OLD_FE;
	}
}

static void
ReceiveCopyBegin(CopyState cstate)
{
	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* new way */
		StringInfoData buf;
		int			natts = list_length(cstate->attnumlist);
		int16		format = (cstate->binary ? 1 : 0);
		int			i;

		pq_beginmessage(&buf, 'G');
		pq_sendbyte(&buf, format);		/* overall format */
		pq_sendint(&buf, natts, 2);
		for (i = 0; i < natts; i++)
			pq_sendint(&buf, format, 2);		/* per-column formats */
		pq_endmessage(&buf);
		cstate->copy_dest = COPY_NEW_FE;
		cstate->fe_msgbuf = makeStringInfo();
	}
	else if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 2)
	{
		/* old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('G');
		cstate->copy_dest = COPY_OLD_FE;
	}
	else
	{
		/* very old way */
		if (cstate->binary)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("COPY BINARY is not supported to stdout or from stdin")));
		pq_putemptymessage('D');
		cstate->copy_dest = COPY_OLD_FE;
	}
	/* We *must* flush here to ensure FE knows it can send. */
	pq_flush();
}

static void
SendCopyEnd(CopyState cstate)
{
	if (cstate->copy_dest == COPY_NEW_FE)
	{
		/* Shouldn't have any unsent data */
		Assert(cstate->fe_msgbuf->len == 0);
		/* Send Copy Done message */
		pq_putemptymessage('c');
	}
	else
	{
		CopySendData(cstate, "\\.", 2);
		/* Need to flush out the trailer (this also appends a newline) */
		CopySendEndOfRow(cstate);
		pq_endcopyout(false);
	}
}

/*----------
 * CopySendData sends output data to the destination (file or frontend)
 * CopySendString does the same for null-terminated strings
 * CopySendChar does the same for single characters
 * CopySendEndOfRow does the appropriate thing at end of each data row
 *	(data is not actually flushed except by CopySendEndOfRow)
 *
 * NB: no data conversion is applied by these functions
 *----------
 */
static void
CopySendData(CopyState cstate, const void *databuf, int datasize)
{
	if (!cstate->is_copy_in) /* copy out */
	{
		appendBinaryStringInfo(cstate->fe_msgbuf, (char *) databuf, datasize);
	}
	else /* hack for: copy in */
	{
		/* we call copySendData in copy-in to handle results
		 * of default functions that we wish to send from the
		 * dispatcher to the executor primary and mirror segments.
		 * we do so by concatenating the results to line buffer.
		 */
		appendBinaryStringInfo(&cstate->line_buf, (char *) databuf, datasize);
	}
}

static void
CopySendString(CopyState cstate, const char *str)
{
	CopySendData(cstate, (void *) str, strlen(str));
}

static void
CopySendChar(CopyState cstate, char c)
{
	CopySendData(cstate, &c, 1);
}

/* AXG: Note that this will both add a newline AND flush the data.
 * For the dispatcher COPY TO we don't want to use this method since
 * our newlines already exist. We use another new method similar to
 * this one to flush the data
 */
void
CopySendEndOfRow(CopyState cstate)
{
	StringInfo	fe_msgbuf = cstate->fe_msgbuf;

	switch (cstate->copy_dest)
	{
		case COPY_FILE:
			if (!cstate->binary)
			{
				/* Default line termination depends on platform */
#ifndef WIN32
				CopySendChar(cstate, '\n');
#else
				CopySendString(cstate, "\r\n");
#endif
			}

			(void) fwrite(fe_msgbuf->data, fe_msgbuf->len,
						  1, cstate->copy_file);
			if (ferror(cstate->copy_file))
			{
				if (cstate->is_program)
				{
					if (errno == EPIPE)
					{
						/*
						 * The pipe will be closed automatically on error at
						 * the end of transaction, but we might get a better
						 * error message from the subprocess' exit code than
						 * just "Broken Pipe"
						 */
						close_program_pipes(cstate, true);

						/*
						 * If close_program_pipes() didn't throw an error,
						 * the program terminated normally, but closed the
						 * pipe first. Restore errno, and throw an error.
						 */
						errno = EPIPE;
					}
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not write to COPY program: %m")));
				}
				else
					ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write to COPY file: %m")));
			}

			break;
		case COPY_OLD_FE:
			/* The FE/BE protocol uses \n as newline for all platforms */
			if (!cstate->binary)
				CopySendChar(cstate, '\n');

			if (pq_putbytes(fe_msgbuf->data, fe_msgbuf->len))
			{
				/* no hope of recovering connection sync, so FATAL */
				ereport(FATAL,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("connection lost during COPY to stdout")));
			}
			break;
		case COPY_NEW_FE:
			/* The FE/BE protocol uses \n as newline for all platforms */
			if (!cstate->binary)
				CopySendChar(cstate, '\n');

			/* Dump the accumulated row as one CopyData message */
			(void) pq_putmessage('d', fe_msgbuf->data, fe_msgbuf->len);
			break;
		case COPY_EXTERNAL_SOURCE:
			/* we don't actually do the write here, we let the caller do it */
#ifndef WIN32
			CopySendChar(cstate, '\n');
#else
			CopySendString(cstate, "\r\n");
#endif
			return; /* don't want to reset msgbuf quite yet */
	}

	resetStringInfo(fe_msgbuf);
}

/*
 * AXG: This one is equivalent to CopySendEndOfRow() besides that
 * it doesn't send end of row - it just flushed the data. We need
 * this method for the dispatcher COPY TO since it already has data
 * with newlines (from the executors).
 */
static void
CopyToDispatchFlush(CopyState cstate)
{
	StringInfo	fe_msgbuf = cstate->fe_msgbuf;

	switch (cstate->copy_dest)
	{
		case COPY_FILE:

			(void) fwrite(fe_msgbuf->data, fe_msgbuf->len,
						  1, cstate->copy_file);
			if (ferror(cstate->copy_file))
			{
				if (cstate->is_program)
				{
					if (errno == EPIPE)
					{
						/*
						 * The pipe will be closed automatically on error at
						 * the end of transaction, but we might get a better
						 * error message from the subprocess' exit code than
						 * just "Broken Pipe"
						 */
						close_program_pipes(cstate, true);

						/*
						 * If close_program_pipes() didn't throw an error,
						 * the program terminated normally, but closed the
						 * pipe first. Restore errno, and throw an error.
						 */
						errno = EPIPE;
					}
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not write to COPY program: %m")));
				}
				else
					ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write to COPY file: %m")));
			}
			break;
		case COPY_OLD_FE:

			if (pq_putbytes(fe_msgbuf->data, fe_msgbuf->len))
			{
				/* no hope of recovering connection sync, so FATAL */
				ereport(FATAL,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("connection lost during COPY to stdout")));
			}
			break;
		case COPY_NEW_FE:

			/* Dump the accumulated row as one CopyData message */
			(void) pq_putmessage('d', fe_msgbuf->data, fe_msgbuf->len);
			break;
		case COPY_EXTERNAL_SOURCE:
			Insist(false); /* internal error */
			break;

	}

	resetStringInfo(fe_msgbuf);
}

/*
 * CopyGetData reads data from the source (file or frontend)
 * CopyGetChar does the same for single characters
 *
 * CopyGetEof checks if EOF was detected by previous Get operation.
 *
 * Note: when copying from the frontend, we expect a proper EOF mark per
 * protocol; if the frontend simply drops the connection, we raise error.
 * It seems unwise to allow the COPY IN to complete normally in that case.
 *
 * NB: no data conversion is applied by these functions
 *
 * Returns: the number of bytes that were successfully read
 * into the data buffer.
 */
static int
CopyGetData(CopyState cstate, void *databuf, int datasize)
{
	size_t		bytesread = 0;

	switch (cstate->copy_dest)
	{
		case COPY_FILE:
			bytesread = fread(databuf, 1, datasize, cstate->copy_file);
			if (feof(cstate->copy_file))
				cstate->fe_eof = true;
			if (ferror(cstate->copy_file))
			{
				if (cstate->is_program)
				{
					int olderrno = errno;

					close_program_pipes(cstate, true);

					/*
					 * If close_program_pipes() didn't throw an error,
					 * the program terminated normally, but closed the
					 * pipe first. Restore errno, and throw an error.
					 */
					errno = olderrno;

					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not read from COPY program: %m")));
				}
				else
					ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read from COPY file: %m")));
			}
			break;
		case COPY_OLD_FE:
			if (pq_getbytes((char *) databuf, datasize))
			{
				/* Only a \. terminator is legal EOF in old protocol */
				ereport(ERROR,
						(errcode(ERRCODE_CONNECTION_FAILURE),
						 errmsg("unexpected EOF on client connection")));
			}
			bytesread += datasize;		/* update the count of bytes that were
										 * read so far */
			break;
		case COPY_NEW_FE:
			while (datasize > 0 && !cstate->fe_eof)
			{
				int			avail;

				while (cstate->fe_msgbuf->cursor >= cstate->fe_msgbuf->len)
				{
					/* Try to receive another message */
					int			mtype;

			readmessage:
					mtype = pq_getbyte();
					if (mtype == EOF)
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
							 errmsg("unexpected EOF on client connection")));
					if (pq_getmessage(cstate->fe_msgbuf, 0))
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
							 errmsg("unexpected EOF on client connection")));
					switch (mtype)
					{
						case 'd':		/* CopyData */
							break;
						case 'c':		/* CopyDone */
							/* COPY IN correctly terminated by frontend */
							cstate->fe_eof = true;
							return bytesread;
						case 'f':		/* CopyFail */
							ereport(ERROR,
									(errcode(ERRCODE_QUERY_CANCELED),
									 errmsg("COPY from stdin failed: %s",
									   pq_getmsgstring(cstate->fe_msgbuf))));
							break;
						case 'H':		/* Flush */
						case 'S':		/* Sync */

							/*
							 * Ignore Flush/Sync for the convenience of client
							 * libraries (such as libpq) that may send those
							 * without noticing that the command they just
							 * sent was COPY.
							 */
							goto readmessage;
						default:
							ereport(ERROR,
									(errcode(ERRCODE_PROTOCOL_VIOLATION),
									 errmsg("unexpected message type 0x%02X during COPY from stdin",
											mtype)));
							break;
					}
				}
				avail = cstate->fe_msgbuf->len - cstate->fe_msgbuf->cursor;
				if (avail > datasize)
					avail = datasize;
				pq_copymsgbytes(cstate->fe_msgbuf, databuf, avail);
				databuf = (void *) ((char *) databuf + avail);
				bytesread += avail;		/* update the count of bytes that were
										 * read so far */
				datasize -= avail;
			}
			break;
		case COPY_EXTERNAL_SOURCE:
			Insist(false); /* RET read their own data with external_senddata() */
			break;

	}

	return bytesread;
}

/*
 * These functions do apply some data conversion
 */

/*
 * CopySendInt32 sends an int32 in network byte order
 */
static void
CopySendInt32(CopyState cstate, int32 val)
{
	uint32		buf;

	buf = htonl((uint32) val);
	CopySendData(cstate, &buf, sizeof(buf));
}

/*
 * CopyGetInt32 reads an int32 that appears in network byte order
 *
 * Returns true if OK, false if EOF
 */
static bool
CopyGetInt32(CopyState cstate, int32 *val)
{
	uint32		buf;

	if (CopyGetData(cstate, &buf, sizeof(buf)) != sizeof(buf))
	{
		*val = 0;				/* suppress compiler warning */
		return false;
	}
	*val = (int32) ntohl(buf);
	return true;
}

/*
 * CopyGetInt64 reads an int64 that appears in network byte order
 *
 * Returns true if OK, false if EOF
 */
static bool
CopyGetInt64(CopyState cstate, int64 *val)
{
	uint64		buf;

	if (CopyGetData(cstate, &buf, sizeof(buf)) != sizeof(buf))
	{
		*val = 0;				/* suppress compiler warning */
		return false;
	}
	*val = (int64) ntohll(buf);
	return true;
}

/*
 * CopySendInt16 sends an int16 in network byte order
 */
static void
CopySendInt16(CopyState cstate, int16 val)
{
	uint16		buf;

	buf = htons((uint16) val);
	CopySendData(cstate, &buf, sizeof(buf));
}

/*
 * CopyGetInt16 reads an int16 that appears in network byte order
 */
static bool
CopyGetInt16(CopyState cstate, int16 *val)
{
	uint16		buf;

	if (CopyGetData(cstate, &buf, sizeof(buf)) != sizeof(buf))
	{
		*val = 0;				/* suppress compiler warning */
		return false;
	}
	*val = (int16) ntohs(buf);
	return true;
}


/*
 * ValidateControlChars
 *
 * These routine is common for COPY and external tables. It validates the
 * control characters (delimiter, quote, etc..) and enforces the given rules.
 *
 * bool copy
 *  - pass true if you're COPY
 *  - pass false if you're an exttab
 *
 * bool load
 *  - pass true for inbound data (COPY FROM, SELECT FROM exttab)
 *  - pass false for outbound data (COPY TO, INSERT INTO exttab)
 */
void ValidateControlChars(bool copy, bool load, bool csv_mode, char *delim,
						char *null_print, char *quote, char *escape,
						List *force_quote, List *force_notnull,
						bool header_line, bool fill_missing, char *newline,
						int num_columns)
{
	bool	delim_off = (pg_strcasecmp(delim, "off") == 0);

	/*
	 * DELIMITER
	 *
	 * Only single-byte delimiter strings are supported. In addition, if the
	 * server encoding is a multibyte character encoding we only allow the
	 * delimiter to be an ASCII character (like postgresql. For more info
	 * on this see discussion and comments in MPP-3756).
	 */
	if (pg_database_encoding_max_length() == 1)
	{
		/* single byte encoding such as ascii, latinx and other */
		if (strlen(delim) != 1 && !delim_off)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("delimiter must be a single one-byte character, or \'off\'")));
	}
	else
	{
		/* multi byte encoding such as utf8 */
		if ((strlen(delim) != 1 || IS_HIGHBIT_SET(delim[0])) && !delim_off )
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("delimiter must be a single one-byte character, or \'off\'")));
	}

	if (strchr(delim, '\r') != NULL ||
		strchr(delim, '\n') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("delimiter cannot be newline or carriage return")));

	if (strchr(null_print, '\r') != NULL ||
		strchr(null_print, '\n') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("null representation cannot use newline or carriage return")));

	if (!csv_mode && strchr(delim, '\\') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("delimiter cannot be backslash")));

	if (strchr(null_print, delim[0]) != NULL && !delim_off)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		errmsg("delimiter must not appear in the NULL specification")));

	/*
	 * Disallow unsafe delimiter characters in non-CSV mode.  We can't allow
	 * backslash because it would be ambiguous.  We can't allow the other
	 * cases because data characters matching the delimiter must be
	 * backslashed, and certain backslash combinations are interpreted
	 * non-literally by COPY IN.  Disallowing all lower case ASCII letters is
	 * more than strictly necessary, but seems best for consistency and
	 * future-proofing.  Likewise we disallow all digits though only octal
	 * digits are actually dangerous.
	 */
	if (!csv_mode && !delim_off &&
		strchr("\\.abcdefghijklmnopqrstuvwxyz0123456789", delim[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("delimiter cannot be \"%s\"", delim)));

	if (delim_off)
	{

		/*
		 * We don't support delimiter 'off' for COPY because the QD COPY
		 * sometimes internally adds columns to the data that it sends to
		 * the QE COPY modules, and it uses the delimiter for it. There
		 * are ways to work around this but for now it's not important and
		 * we simply don't support it.
		 */
		if (copy)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Using no delimiter is only supported for external tables")));

		if (num_columns != 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Using no delimiter is only possible for a single column table")));

	}

	/*
	 * HEADER
	 */
	if(header_line)
	{
		if(!copy && Gp_role == GP_ROLE_DISPATCH)
		{
			/* (exttab) */
			if(load)
			{
				/* RET */
				ereport(NOTICE,
						(errmsg("HEADER means that each one of the data files "
								"has a header row.")));				
			}
			else
			{
				/* WET */
				ereport(ERROR,
						(errcode(ERRCODE_GP_FEATURE_NOT_YET),
						errmsg("HEADER is not yet supported for writable external tables")));				
			}
		}
	}

	/*
	 * QUOTE
	 */
	if (!csv_mode && quote != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("quote available only in CSV mode")));

	if (csv_mode && strlen(quote) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("quote must be a single character")));

	if (csv_mode && strchr(null_print, quote[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("CSV quote character must not appear in the NULL specification")));

	if (csv_mode && delim[0] == quote[0])
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("delimiter and quote must be different")));

	/*
	 * ESCAPE
	 */
	if (csv_mode && strlen(escape) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("escape in CSV format must be a single character")));

	if (!csv_mode &&
		(strchr(escape, '\r') != NULL ||
		strchr(escape, '\n') != NULL))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("escape representation in text format cannot use newline or carriage return")));

	if (!csv_mode && strlen(escape) != 1)
	{
		if (pg_strcasecmp(escape, "off"))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("escape must be a single character, or [OFF/off] to disable escapes")));
	}

	/*
	 * FORCE QUOTE
	 */
	if (!csv_mode && force_quote != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("force quote available only in CSV mode")));
	if (force_quote != NIL && load)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("force quote only available for data unloading, not loading")));

	/*
	 * FORCE NOT NULL
	 */
	if (!csv_mode && force_notnull != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("force not null available only in CSV mode")));
	if (force_notnull != NIL && !load)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("force not null only available for data loading, not unloading")));

	if (fill_missing && !load)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("fill missing fields only available for data loading, not unloading")));

	/*
	 * NEWLINE
	 */
	if (newline)
	{
		if (!load)
		{
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_YET),
					errmsg("newline currently available for data loading only, not unloading")));
		}
		else
		{
			if(pg_strcasecmp(newline, "lf") != 0 &&
			   pg_strcasecmp(newline, "cr") != 0 &&
			   pg_strcasecmp(newline, "crlf") != 0)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("invalid value for NEWLINE (%s)", newline),
						errhint("valid options are: 'LF', 'CRLF', 'CR'")));
		}
	}
}


/*
 *	 DoCopy executes the SQL COPY statement
 *
 * Either unload or reload contents of table <relation>, depending on <from>.
 * (<from> = TRUE means we are inserting into the table.) In the "TO" case
 * we also support copying the output of an arbitrary SELECT query.
 *
 * If <pipe> is false, transfer is between the table and the file named
 * <filename>.	Otherwise, transfer is between the table and our regular
 * input/output stream. The latter could be either stdin/stdout or a
 * socket, depending on whether we're running under Postmaster control.
 *
 * Iff <binary>, unload or reload in the binary format, as opposed to the
 * more wasteful but more robust and portable text format.
 *
 * Iff <oids>, unload or reload the format that includes OID information.
 * On input, we accept OIDs whether or not the table has an OID column,
 * but silently drop them if it does not.  On output, we report an error
 * if the user asks for OIDs in a table that has none (not providing an
 * OID column might seem friendlier, but could seriously confuse programs).
 *
 * If in the text format, delimit columns with delimiter <delim> and print
 * NULL values as <null_print>.
 *
 * When loading in the text format from an input stream (as opposed to
 * a file), recognize a "\." on a line by itself as EOF. Also recognize
 * a stream EOF.  When unloading in the text format to an output stream,
 * write a "." on a line by itself at the end of the data.
 *
 * Do not allow a Postgres user without superuser privilege to read from
 * or write to a file.
 *
 * Do not allow the copy if user doesn't have proper permission to access
 * the table or the specifically requested columns.
 */
uint64
DoCopyInternal(const CopyStmt *stmt, const char *queryString, CopyState cstate)
{
	bool		is_from = stmt->is_from;
	bool		pipe = (stmt->filename == NULL || Gp_role == GP_ROLE_EXECUTE);
	List	   *attnamelist = stmt->attlist;
	List	   *force_quote = NIL;
	List	   *force_notnull = NIL;
	AclMode		required_access = (is_from ? ACL_INSERT : ACL_SELECT);
	AclMode		relPerms;
	AclMode		remainingPerms;
	ListCell   *option;
	TupleDesc	tupDesc;
	int			num_phys_attrs;
	uint64		processed;
	bool		qe_copy_from = (is_from && (Gp_role == GP_ROLE_EXECUTE));
	/* save relationOid for auto-stats */
	Oid			relationOid = InvalidOid;

	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "binary") == 0)
		{
			if (cstate->binary)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->binary = intVal(defel->arg);
		}
		else if (strcmp(defel->defname, "oids") == 0)
		{
			if (cstate->oids)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->oids = intVal(defel->arg);
		}
		else if (strcmp(defel->defname, "delimiter") == 0)
		{
			if (cstate->delim)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->delim = strVal(defel->arg);
		}
		else if (strcmp(defel->defname, "null") == 0)
		{
			if (cstate->null_print)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->null_print = strVal(defel->arg);

			/*
			 * MPP-2010: unfortunately serialization function doesn't
			 * distinguish between 0x0 and empty string. Therefore we
			 * must assume that if NULL AS was indicated and has no value
			 * the actual value is an empty string.
			 */
			if(!cstate->null_print)
				cstate->null_print = "";
		}
		else if (strcmp(defel->defname, "csv") == 0)
		{
			if (cstate->csv_mode)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->csv_mode = intVal(defel->arg);
		}
		else if (strcmp(defel->defname, "header") == 0)
		{
			if (cstate->header_line)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->header_line = intVal(defel->arg);
		}
		else if (strcmp(defel->defname, "quote") == 0)
		{
			if (cstate->quote)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->quote = strVal(defel->arg);
		}
		else if (strcmp(defel->defname, "escape") == 0)
		{
			if (cstate->escape)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->escape = strVal(defel->arg);
		}
		else if (strcmp(defel->defname, "force_quote") == 0)
		{
			if (force_quote)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			force_quote = (List *) defel->arg;
		}
		else if (strcmp(defel->defname, "force_notnull") == 0)
		{
			if (force_notnull)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			force_notnull = (List *) defel->arg;
		}
		else if (strcmp(defel->defname, "fill_missing_fields") == 0)
		{
			if (cstate->fill_missing)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->fill_missing = intVal(defel->arg);
		}
		else if (strcmp(defel->defname, "newline") == 0)
		{
			if (cstate->eol_str)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->eol_str = strVal(defel->arg);
		}
		else if (strcmp(defel->defname, "on_segment") == 0)
		{
			if (cstate->on_segment)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->on_segment = TRUE;
		}
		else
			elog(ERROR, "option \"%s\" not recognized",
				 defel->defname);
	}

	/* Set defaults */

	/* Check for incompatible options */
	if (cstate->binary && cstate->delim)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot specify DELIMITER in BINARY mode")));

	if (stmt->is_program && stmt->filename == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("STDIN/STDOUT not allowed with PROGRAM")));

	if (cstate->on_segment && stmt->filename == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("STDIN and STDOUT are not supported by 'COPY ON SEGMENT'")));

	/*
	 * In PostgreSQL, HEADER is not allowed in text mode either, but in GPDB,
	 * only forbid it with BINARY.
	 */
	if (cstate->binary && cstate->header_line)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot specify HEADER in BINARY mode")));

	if (cstate->binary && cstate->csv_mode)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot specify CSV in BINARY mode")));

	if (cstate->binary && cstate->null_print)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cannot specify NULL in BINARY mode")));

	cstate->err_loc_type = ROWNUM_ORIGINAL;
	cstate->eol_type = EOL_UNKNOWN;
	cstate->escape_off = false;

	if (!cstate->delim)
		cstate->delim = cstate->csv_mode ? "," : "\t";

	if (!cstate->null_print)
		cstate->null_print = cstate->csv_mode ? "" : "\\N";

	if (cstate->csv_mode)
	{
		if (!cstate->quote)
			cstate->quote = "\"";
		if (!cstate->escape)
			cstate->escape = cstate->quote;
	}

	if (!cstate->csv_mode && !cstate->escape)
		cstate->escape = "\\";			/* default escape for text mode */

	/*
	 * Error handling setup
	 */
	if(stmt->sreh)
	{
		/* Single row error handling requested */
		SingleRowErrorDesc *sreh;
		bool		log_to_file = false;

		sreh = (SingleRowErrorDesc *)stmt->sreh;

		if (!is_from)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY single row error handling only available using COPY FROM")));

		if (sreh->into_file)
		{
			cstate->errMode = SREH_LOG;
			log_to_file = true;
		}
		else
		{
			cstate->errMode = SREH_IGNORE;
		}
		cstate->cdbsreh = makeCdbSreh(sreh->rejectlimit,
									  sreh->is_limit_in_rows,
									  stmt->filename,
									  stmt->relation->relname,
									  log_to_file);
	}
	else
	{
		/* No single row error handling requested. Use "all or nothing" */
		cstate->cdbsreh = NULL; /* default - no SREH */
		cstate->errMode = ALL_OR_NOTHING; /* default */
	}

	cstate->skip_ext_partition = stmt->skip_ext_partition;

	/* We must be a QE if we received the partitioning config */
	if (stmt->partitions)
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);
		cstate->partitions = stmt->partitions;
	}

	/*
	 * Validate our control characters and their combination
	 */
	ValidateControlChars(true,
						 is_from,
						 cstate->csv_mode,
						 cstate->delim,
						 cstate->null_print,
						 cstate->quote,
						 cstate->escape,
						 force_quote,
						 force_notnull,
						 cstate->header_line,
						 cstate->fill_missing,
						 cstate->eol_str,
						 0 /* pass correct value when COPY supports no delim */);

	if (!pg_strcasecmp(cstate->escape, "off"))
		cstate->escape_off = true;

	/* set end of line type if NEWLINE keyword was specified */
	if (cstate->eol_str)
		CopyEolStrToType(cstate);

	/* Disallow COPY to/from file or program except to superusers. */
	if (!pipe && !superuser())
	{
		if (stmt->is_program)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to COPY to or from an external program"),
					 errhint("Anyone can COPY to stdout or from stdin. "
							 "psql's \\copy command also works for anyone.")));
		else
			ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to COPY to or from a file"),
				 errhint("Anyone can COPY to stdout or from stdin. "
						 "psql's \\copy command also works for anyone.")));
	}

	cstate->copy_dest = COPY_FILE;		/* default */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		if (cstate->on_segment) /* Save data to a local file */
		{
			StringInfoData filepath;
			initStringInfo(&filepath);
			appendStringInfoString(&filepath, stmt->filename);

			replaceStringInfoString(&filepath, "<SEG_DATA_DIR>", DataDir);

			if (strstr(stmt->filename, "<SEGID>") == NULL)
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("<SEGID> is required for file name")));

			char segid_buf[8];
			snprintf(segid_buf, 8, "%d", GpIdentity.segindex);
			replaceStringInfoString(&filepath, "<SEGID>", segid_buf);

			cstate->filename = filepath.data;
			/* Rename filename if error log needed */
			if (NULL != cstate->cdbsreh)
			{
				snprintf(cstate->cdbsreh->filename,
						 sizeof(cstate->cdbsreh->filename), "%s",
						 filepath.data);
			}

			pipe = false;
		}
		else
		{
			cstate->filename = NULL; /* QE COPY always uses STDIN */
		}
	}
	else
	{
		cstate->filename = stmt->filename; /* Not on_segment, QD saves file to local */
	}
	cstate->copy_file = NULL;
	cstate->fe_msgbuf = NULL;
	cstate->fe_eof = false;
	cstate->missing_bytes = 0;
	cstate->is_program = stmt->is_program;
	
	if(!is_from)
	{
		if (pipe)
		{
			if (whereToSendOutput == DestRemote)
				cstate->fe_copy = true;
			else
				cstate->copy_file = stdout;
		}
		else
		{
			if (cstate->is_program)
			{
				if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
				{
					cstate->program_pipes = open_program_pipes("cat > /dev/null", true);
					cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_W);
				}
				else
				{
					cstate->program_pipes = open_program_pipes(cstate->filename, true);
					cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_W);
				}

				if (cstate->copy_file == NULL)
					ereport(ERROR,
							(errmsg("could not execute command \"%s\": %m",
									cstate->filename)));
			}
			else
			{
				mode_t		oumask; /* Pre-existing umask value */
				struct stat st;
				char *filename = cstate->filename;

				if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
					filename = "/dev/null";

				/*
				* Prevent write to relative path ... too easy to shoot oneself in the
				* foot by overwriting a database file ...
				*/
				if (!is_absolute_path(filename))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_NAME),
							errmsg("relative path not allowed for COPY to file")));

				oumask = umask((mode_t) 022);
				cstate->copy_file = AllocateFile(filename, PG_BINARY_W);
				umask(oumask);

				if (cstate->copy_file == NULL)
					ereport(ERROR,
							(errcode_for_file_access(),
							errmsg("could not open file \"%s\" for writing: %m", filename)));

				// Increase buffer size to improve performance  (cmcdevitt)
				setvbuf(cstate->copy_file, NULL, _IOFBF, 393216); // 384 Kbytes

				fstat(fileno(cstate->copy_file), &st);
				if (S_ISDIR(st.st_mode))
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							errmsg("\"%s\" is a directory", filename)));
			}
		}

	}

	elog(DEBUG1,"DoCopy starting");
	if (stmt->relation)
	{
		Assert(!stmt->query);
		cstate->queryDesc = NULL;

		/* Open and lock the relation, using the appropriate lock type. */
		cstate->rel = heap_openrv(stmt->relation,
							 (is_from ? RowExclusiveLock : AccessShareLock));

		/* save relation oid for auto-stats call later */
		relationOid = RelationGetRelid(cstate->rel);

		tupDesc = RelationGetDescr(cstate->rel);

		/* Check relation permissions. */
		relPerms = pg_class_aclmask(RelationGetRelid(cstate->rel), GetUserId(),
									required_access, ACLMASK_ALL);
		remainingPerms = required_access & ~relPerms;
		if (remainingPerms != 0)
		{
			/* We don't have table permissions, check per-column permissions */
			List	   *attnums;
			ListCell   *cur;

			attnums = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);
			foreach(cur, attnums)
			{
				int			attnum = lfirst_int(cur);

				if (pg_attribute_aclcheck(RelationGetRelid(cstate->rel),
										  attnum,
										  GetUserId(),
										  remainingPerms) != ACLCHECK_OK)
					aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
								   RelationGetRelationName(cstate->rel));
			}
		}

		/* check read-only transaction */
		if (XactReadOnly && is_from && !cstate->rel->rd_islocaltemp)
			ereport(ERROR,
					(errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION),
					 errmsg("transaction is read-only")));

		/* Don't allow COPY w/ OIDs to or from a table without them */
		if (cstate->oids && !cstate->rel->rd_rel->relhasoids)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("table \"%s\" does not have OIDs",
							RelationGetRelationName(cstate->rel))));

		/* Update error log info */
		if (cstate->cdbsreh)
			cstate->cdbsreh->relid = RelationGetRelid(cstate->rel);
	}
	else
	{
		List	   *rewritten;
		Query	   *query;
		PlannedStmt *plan;
		DestReceiver *dest;

		Assert(!is_from);
		cstate->rel = NULL;

		/* Don't allow COPY w/ OIDs from a select */
		if (cstate->oids)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY (SELECT) WITH OIDS is not supported")));

		/*
		 * Run parse analysis and rewrite.	Note this also acquires sufficient
		 * locks on the source table(s).
		 *
		 * Because the parser and planner tend to scribble on their input, we
		 * make a preliminary copy of the source querytree.  This prevents
		 * problems in the case that the COPY is in a portal or plpgsql
		 * function and is executed repeatedly.  (See also the same hack in
		 * DECLARE CURSOR and PREPARE.)  XXX FIXME someday.
		 */
		rewritten = pg_analyze_and_rewrite((Node *) copyObject(stmt->query),
										   queryString, NULL, 0);

		/* We don't expect more or less than one result query */
		if (list_length(rewritten) != 1)
			elog(ERROR, "unexpected rewrite result");

		query = (Query *) linitial(rewritten);
		Assert(query->commandType == CMD_SELECT);
		Assert(query->utilityStmt == NULL);

		/* Query mustn't use INTO, either */
		if (query->intoClause)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY (SELECT INTO) is not supported")));

		/* plan the query */
		plan = planner(query, 0, NULL);

		/*
		 * Use a snapshot with an updated command ID to ensure this query sees
		 * results of any previously executed queries.
		 */
		PushUpdatedSnapshot(GetActiveSnapshot());

		/* Create dest receiver for COPY OUT */
		dest = CreateDestReceiver(DestCopyOut);
		((DR_copy *) dest)->cstate = cstate;

		/* Create a QueryDesc requesting no output */
		cstate->queryDesc = CreateQueryDesc(plan, queryString,
											GetActiveSnapshot(),
											InvalidSnapshot,
											dest, NULL, false);

		if (gp_enable_gpperfmon && Gp_role == GP_ROLE_DISPATCH)
		{
			Assert(queryString);
			gpmon_qlog_query_submit(cstate->queryDesc->gpmon_pkt);
			gpmon_qlog_query_text(cstate->queryDesc->gpmon_pkt,
					queryString,
					application_name,
					GetResqueueName(GetResQueueId()),
					GetResqueuePriority(GetResQueueId()));
		}

		/*
		 * Call ExecutorStart to prepare the plan for execution.
		 *
		 * ExecutorStart computes a result tupdesc for us
		 */
		ExecutorStart(cstate->queryDesc, 0);

		tupDesc = cstate->queryDesc->tupDesc;
	}

	cstate->attnamelist = attnamelist;
	/* Generate or convert list of attributes to process */
	cstate->attnumlist = CopyGetAttnums(tupDesc, cstate->rel, attnamelist);

	num_phys_attrs = tupDesc->natts;

	/* Convert FORCE QUOTE name list to per-column flags, check validity */
	cstate->force_quote_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (force_quote)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_quote);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				   errmsg("FORCE QUOTE column \"%s\" not referenced by COPY",
						  NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_quote_flags[attnum - 1] = true;
		}
	}

	/* Convert FORCE NOT NULL name list to per-column flags, check validity */
	cstate->force_notnull_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (force_notnull)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, force_notnull);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				errmsg("FORCE NOT NULL column \"%s\" not referenced by COPY",
					   NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_notnull_flags[attnum - 1] = true;
		}

		/* keep the raw version too, we will need it later */
		cstate->force_notnull = force_notnull;
	}

	/* Set up variables to avoid per-attribute overhead. */
	initStringInfo(&cstate->attribute_buf);
	initStringInfo(&cstate->line_buf);
	cstate->processed = 0;

	/*
	 * Set up encoding conversion info.  Even if the client and server
	 * encodings are the same, we must apply pg_client_to_server() to
	 * validate data in multibyte encodings. However, transcoding must
	 * be skipped for COPY FROM in executor mode since data already arrived
	 * in server encoding (was validated and trancoded by dispatcher mode
	 * COPY). For this same reason encoding_embeds_ascii can never be true
	 * for COPY FROM in executor mode.
	 */
	cstate->client_encoding = pg_get_client_encoding();
	cstate->need_transcoding =
		((cstate->client_encoding != GetDatabaseEncoding() ||
		  pg_database_encoding_max_length() > 1) && !qe_copy_from);

	cstate->encoding_embeds_ascii = (qe_copy_from ? false : PG_ENCODING_IS_CLIENT_ONLY(cstate->client_encoding));
	cstate->line_buf_converted = (Gp_role == GP_ROLE_EXECUTE ? true : false);
	setEncodingConversionProc(cstate, pg_get_client_encoding(), !is_from);

	/*
	 * some greenplum db specific vars
	 */
	cstate->is_copy_in = (is_from ? true : false);
	if (is_from)
	{
		cstate->error_on_executor = false;
		initStringInfo(&(cstate->executor_err_context));
	}

	if (is_from)				/* copy from file to database */
	{
		bool		pipe = (cstate->filename == NULL || Gp_role == GP_ROLE_EXECUTE);
		bool		shouldDispatch = (Gp_role == GP_ROLE_DISPATCH &&
									  cstate->rel->rd_cdbpolicy != NULL);
		char		relkind;

		Assert(cstate->rel);

		relkind = cstate->rel->rd_rel->relkind;

		if (relkind != RELKIND_RELATION)
		{
			if (relkind == RELKIND_VIEW)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy to view \"%s\"",
								RelationGetRelationName(cstate->rel))));
			else if (relkind == RELKIND_SEQUENCE)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy to sequence \"%s\"",
								RelationGetRelationName(cstate->rel))));
			else
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy to non-table relation \"%s\"",
								RelationGetRelationName(cstate->rel))));
		}

		if(stmt->sreh && Gp_role != GP_ROLE_EXECUTE && !cstate->rel->rd_cdbpolicy)
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY single row error handling only available for distributed user tables")));

		if (pipe)
		{
			if (whereToSendOutput == DestRemote)
				ReceiveCopyBegin(cstate);
			else
				cstate->copy_file = stdin;
		}
		else
		{
			if (cstate->is_program)
			{
				if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
				{
					cstate->program_pipes = open_program_pipes("cat /dev/null", false);
					cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_R);
				}
				else
				{
					cstate->program_pipes = open_program_pipes(cstate->filename, false);
					cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_R);
				}

				if (cstate->copy_file == NULL)
					ereport(ERROR,
							(errmsg("could not execute command \"%s\": %m",
									cstate->filename)));
			}
			else
			{
				struct stat st;
				char *filename = cstate->filename;

				/* Use dummy file on master for COPY FROM ON SEGMENT */
				if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
					filename = "/dev/null";

				cstate->copy_file = AllocateFile(filename, PG_BINARY_R);

				if (cstate->copy_file == NULL)
					ereport(ERROR,
							(errcode_for_file_access(),
							errmsg("could not open file \"%s\" for reading: %m",
									filename)));

				// Increase buffer size to improve performance  (cmcdevitt)
				setvbuf(cstate->copy_file, NULL, _IOFBF, 393216); // 384 Kbytes

				fstat(fileno(cstate->copy_file), &st);
				if (S_ISDIR(st.st_mode))
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							errmsg("\"%s\" is a directory", filename)));
			}
		}


		/*
		 * Append Only Tables.
		 *
		 * If QD, build a list of all the relations (relids) that may get data
		 * inserted into them as a part of this operation. This includes
		 * the relation specified in the COPY command, plus any partitions
		 * that it may have. Then, call assignPerRelSegno to assign a segfile
		 * number to insert into each of the Append Only relations that exists
		 * in this global list. We generate the list now and save it in cstate.
		 *
		 * If QE - get the QD generated list from CopyStmt and each relation can
		 * find it's assigned segno by looking at it (during CopyFrom).
		 *
		 * Utility mode always builds a one single mapping.
		 */
		if(shouldDispatch)
		{
			Oid relid = RelationGetRelid(cstate->rel);
			List *all_relids = NIL;

			all_relids = lappend_oid(all_relids, relid);

			if (rel_is_partitioned(relid))
			{
				if (cstate->on_segment && gp_enable_segment_copy_checking && !partition_policies_equal(cstate->rel->rd_cdbpolicy, RelationBuildPartitionDesc(cstate->rel, false)))
				{
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("COPY FROM ON SEGMENT doesn't support checking distribution key restriction when the distribution policy of the partition table is different from the main table"),
							 errhint("\"SET gp_enable_segment_copy_checking=off\" can be used to disable distribution key checking.")));
					return cstate->processed;
				}
				PartitionNode *pn = RelationBuildPartitionDesc(cstate->rel, false);
				all_relids = list_concat(all_relids, all_partition_relids(pn));
			}

			cstate->ao_segnos = assignPerRelSegno(all_relids);
		}
		else
		{
			if (stmt->ao_segnos)
			{
				/* We must be a QE if we received the aosegnos config */
				Assert(Gp_role == GP_ROLE_EXECUTE);
				cstate->ao_segnos = stmt->ao_segnos;
			}
			else
			{
				/*
				 * utility mode (or dispatch mode for no policy table).
				 * create a one entry map for our one and only relation
				 */
				if(RelationIsAoRows(cstate->rel) || RelationIsAoCols(cstate->rel))
				{
					SegfileMapNode *n = makeNode(SegfileMapNode);
					n->relid = RelationGetRelid(cstate->rel);
					n->segno = SetSegnoForWrite(cstate->rel, InvalidFileSegNumber);
					cstate->ao_segnos = lappend(cstate->ao_segnos, n);
				}
			}
		}

		/*
		 * Set up is done. Get to work!
		 */
		if (shouldDispatch)
		{
			/* data needs to get dispatched to segment databases */
			CopyFromDispatch(cstate);
		}
		else
		{
			/* data needs to get inserted locally */
			if (cstate->on_segment)
			{
				MemoryContext oldcxt;
				oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
				cstate->rel->rd_cdbpolicy = palloc(sizeof(GpPolicy) + sizeof(AttrNumber) * stmt->nattrs);
				cstate->rel->rd_cdbpolicy->nattrs = stmt->nattrs;
				cstate->rel->rd_cdbpolicy->ptype = stmt->ptype;
				memcpy(cstate->rel->rd_cdbpolicy->attrs, stmt->distribution_attrs, sizeof(AttrNumber) * stmt->nattrs);
				MemoryContextSwitchTo(oldcxt);
			}
			CopyFrom(cstate);
		}

		if (!pipe)
		{
			if (cstate->is_program)
			{
				close_program_pipes(cstate, true);
			}
			else if (FreeFile(cstate->copy_file))
			{
					ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not close file \"%s\": %m",
								cstate->filename)));
			}
		}
	}
	else
		DoCopyTo(cstate);		/* copy from database to file */

	/*
	 * Close the relation or query.  If reading, we can release the
	 * AccessShareLock we got; if writing, we should hold the lock until end
	 * of transaction to ensure that updates will be committed before lock is
	 * released.
	 */
	if (cstate->rel)
		heap_close(cstate->rel, (is_from ? NoLock : AccessShareLock));
	if (cstate->queryDesc)
	{
		/* Close down the query and free resources. */
		ExecutorEnd(cstate->queryDesc);
		FreeQueryDesc(cstate->queryDesc);
		cstate->queryDesc = NULL;
		PopActiveSnapshot();
	}

	/* Clean up single row error handling related memory */
	if (cstate->cdbsreh)
		destroyCdbSreh(cstate->cdbsreh);

	/* Clean up storage (probably not really necessary) */
	processed = cstate->processed;

    /* MPP-4407. Logging number of tuples copied */
	if (Gp_role == GP_ROLE_DISPATCH
			&& is_from
			&& relationOid != InvalidOid
			&& GetCommandLogLevel((Node *) stmt) <= log_statement)
	{
		elog(DEBUG1, "type_of_statement = %s dboid = %d tableoid = %d num_tuples_modified = %u",
				autostats_cmdtype_to_string(AUTOSTATS_CMDTYPE_COPY),
				MyDatabaseId,
				relationOid,
				(unsigned int) processed);
	}

    /* Issue automatic ANALYZE if conditions are satisfied (MPP-4082). */
	if (Gp_role == GP_ROLE_DISPATCH && is_from)
		auto_stats(AUTOSTATS_CMDTYPE_COPY, relationOid, processed, false /* inFunction */);

	if (cstate->force_quote_flags)
		pfree(cstate->force_quote_flags);
	if (cstate->force_notnull_flags)
		pfree(cstate->force_notnull_flags);

	pfree(cstate->attribute_buf.data);
	pfree(cstate->line_buf.data);

	return processed;
}

uint64
DoCopy(const CopyStmt *stmt, const char *queryString)
{
	uint64 result = -1;
	/* Allocate workspace and zero all fields */
	CopyState cstate = (CopyStateData *) palloc0(sizeof(CopyStateData));
	PG_TRY();
	{
		result = DoCopyInternal(stmt, queryString, cstate);
	}
	PG_CATCH();
	{
		if (!(!cstate->on_segment && Gp_role == GP_ROLE_EXECUTE)) {
			if (cstate->is_program)
			{
				close_program_pipes(cstate, false);
			}
		}

		if (cstate->queryDesc)
		{
			/* should shutdown the mpp stuff such as interconnect and dispatch thread */
			mppExecutorCleanup(cstate->queryDesc);
		}
		PG_RE_THROW();
	}
	PG_END_TRY();
	pfree(cstate);
	return result;
}

/*
 * This intermediate routine exists mainly to localize the effects of setjmp
 * so we don't need to plaster a lot of variables with "volatile".
 */
static void
DoCopyTo(CopyState cstate)
{
	bool		pipe = (cstate->filename == NULL);

	if (cstate->rel)
	{
		if (cstate->rel->rd_rel->relkind != RELKIND_RELATION)
		{
			if (cstate->rel->rd_rel->relkind == RELKIND_VIEW)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy from view \"%s\"",
								RelationGetRelationName(cstate->rel)),
						 errhint("Try the COPY (SELECT ...) TO variant.")));
			else if (cstate->rel->rd_rel->relkind == RELKIND_SEQUENCE)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy from sequence \"%s\"",
								RelationGetRelationName(cstate->rel))));
			else
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy from non-table relation \"%s\"",
								RelationGetRelationName(cstate->rel))));
		}
		else if (RelationIsExternal(cstate->rel))
		{
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot copy from external relation \"%s\"",
								RelationGetRelationName(cstate->rel)),
						 errhint("Try the COPY (SELECT ...) TO variant.")));
		}
		else if (rel_has_external_partition(cstate->rel->rd_id))
		{
			if (!cstate->skip_ext_partition)
			{
				ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from relation \"%s\" which has external partition(s)",
							RelationGetRelationName(cstate->rel)),
					 errhint("Try the COPY (SELECT ...) TO variant.")));
			}
			else
			{
				ereport(NOTICE,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("COPY ignores external partition(s)")));
			}
		}
	}else
	{
		/* Report error because COPY ON SEGMENT don't know the data location of the result of SELECT query.*/
		if(cstate->on_segment)
		{
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						errmsg("'COPY (SELECT ...) TO' doesn't support 'ON SEGMENT'.")));
		}
	}

	PG_TRY();
	{
		if (cstate->fe_copy)
			SendCopyBegin(cstate);
		else if	(Gp_role == GP_ROLE_EXECUTE && cstate->on_segment)
		{
			SendCopyBegin(cstate);
			/*
			 * For COPY ON SEGMENT command, segment writes to file
			 * instead of front end. Switch to COPY_FILE
			 */
			cstate->copy_dest = COPY_FILE;
		}

		/*
		 * We want to dispatch COPY TO commands only in the case that
		 * we are the dispatcher and we are copying from a user relation
		 * (a relation where data is distributed in the segment databases).
		 * Otherwize, if we are not the dispatcher *or* if we are
		 * doing COPY (SELECT) we just go straight to work, without
		 * dispatching COPY commands to executors.
		 */
		if (Gp_role == GP_ROLE_DISPATCH && cstate->rel && cstate->rel->rd_cdbpolicy)
			CopyToDispatch(cstate);
		else
			CopyTo(cstate);

		if (cstate->fe_copy)
			SendCopyEnd(cstate);
		else if (Gp_role == GP_ROLE_EXECUTE && cstate->on_segment)
		{
			/*
			 * For COPY ON SEGMENT command, switch back to front end
			 * before sending copy end which is "\."
			 */
			cstate->copy_dest = COPY_NEW_FE;
			SendCopyEnd(cstate);
		}
	}
	PG_CATCH();
	{
		/*
		 * Make sure we turn off old-style COPY OUT mode upon error. It is
		 * okay to do this in all cases, since it does nothing if the mode is
		 * not on.
		 */
		if (Gp_role == GP_ROLE_EXECUTE && cstate->on_segment)
			cstate->copy_dest = COPY_NEW_FE;

		pq_endcopyout(true);
		PG_RE_THROW();
	}
	PG_END_TRY();

	if (!pipe)
	{
		if (cstate->is_program)
		{
			close_program_pipes(cstate, true);
		}
		else if (FreeFile(cstate->copy_file))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not close file \"%s\": %m",
							cstate->filename)));
		}
	}
}

/*
 * CopyToCreateDispatchCommand
 *
 * Create the COPY command that will get dispatched to the QE's.
 */
static void CopyToCreateDispatchCommand(CopyState cstate,
										StringInfo cdbcopy_cmd,
										AttrNumber	num_phys_attrs,
										Form_pg_attribute *attr)

{
	ListCell   *cur;
	bool		is_first_col = true;
	int			i;

	/* append schema and tablename */
	appendStringInfo(cdbcopy_cmd, "COPY %s.%s",
					 quote_identifier(get_namespace_name(RelationGetNamespace(cstate->rel))),
					 quote_identifier(RelationGetRelationName(cstate->rel)));
	/*
	 * append column list. NOTE: if not specified originally, attnumlist will
	 * include all non-dropped columns of the table by default
	 */
	if(num_phys_attrs > 0) /* don't append anything for zero column table */
	{
		foreach(cur, cstate->attnumlist)
		{
			int			attnum = lfirst_int(cur);
			int			m = attnum - 1;

			/* We don't add dropped attributes */
			if (attr[m]->attisdropped)
				continue;

			/* append column string. quote it if needed */
			appendStringInfo(cdbcopy_cmd, (is_first_col ? "(%s" : ",%s"),
							 quote_identifier(NameStr(attr[m]->attname)));

			is_first_col = false;
		}

		if (!is_first_col)
			appendStringInfo(cdbcopy_cmd, ")");
	}

	if (cstate->is_program)
		appendStringInfo(cdbcopy_cmd, " TO PROGRAM");
	else
		appendStringInfo(cdbcopy_cmd, " TO");

	if (cstate->on_segment)
	{
		appendStringInfo(cdbcopy_cmd, " '%s' WITH ON SEGMENT", cstate->filename);
	}
	else if (cstate->is_program)
	{
			appendStringInfo(cdbcopy_cmd, " '%s' WITH", cstate->filename);
	}
	else
	{
			appendStringInfo(cdbcopy_cmd, " STDOUT WITH");
	}

	if (cstate->oids)
		appendStringInfo(cdbcopy_cmd, " OIDS");

	if (cstate->binary)
		appendStringInfo(cdbcopy_cmd, " BINARY");
	else
	{
		appendStringInfo(cdbcopy_cmd, " DELIMITER AS E'%s'", escape_quotes(cstate->delim));
		appendStringInfo(cdbcopy_cmd, " NULL AS E'%s'", escape_quotes(cstate->null_print));

		/* if default escape in text format ("\") leave expression out */
		if (!cstate->csv_mode && strcmp(cstate->escape, "\\") != 0)
			appendStringInfo(cdbcopy_cmd, " ESCAPE AS E'%s'", escape_quotes(cstate->escape));

		if (cstate->csv_mode)
		{
			appendStringInfo(cdbcopy_cmd, " CSV");

			/*
			 * If on_segment, QE needs to write their own CSV header. If not,
			 * only QD needs to, QE doesn't send CSV header to QD
			 */
			if (cstate->on_segment && cstate->header_line)
				appendStringInfo(cdbcopy_cmd, " HEADER");

			appendStringInfo(cdbcopy_cmd, " QUOTE AS E'%s'", escape_quotes(cstate->quote));
			appendStringInfo(cdbcopy_cmd, " ESCAPE AS E'%s'", escape_quotes(cstate->escape));

			/* Create list of FORCE QUOTE columns */
			is_first_col = true;
			for (i = 0; i < num_phys_attrs; i++)
			{
				if (cstate->force_quote_flags[i])
				{
					if (is_first_col)
						appendStringInfoString(cdbcopy_cmd, "FORCE QUOTE ");
					else
						appendStringInfoString(cdbcopy_cmd, ", ");
					is_first_col = false;

					appendStringInfoString(cdbcopy_cmd,
										   quote_identifier(NameStr(attr[i]->attname)));
				}
			}

			/* do NOT include HEADER. Header row is created by dispatcher COPY */
		}
	}
}


/*
 * Copy from relation TO file. Starts a COPY TO command on each of
 * the executors and gathers all the results and writes it out.
 */
 void
CopyToDispatch(CopyState cstate)
{
	TupleDesc	tupDesc;
	int			num_phys_attrs;
	int			attr_count;
	Form_pg_attribute *attr;
	CdbCopy    *cdbCopy;
	StringInfoData cdbcopy_err;
	StringInfoData cdbcopy_cmd;

	tupDesc = cstate->rel->rd_att;
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(cstate->attnumlist);

	/* We use fe_msgbuf as a per-row buffer regardless of copy_dest */
	cstate->fe_msgbuf = makeStringInfo();

	/*
	 * prepare to get COPY data from segDBs:
	 * 1 - re-construct the orignial COPY command sent from the client.
	 * 2 - execute a BEGIN DTM transaction.
	 * 3 - send the COPY command to all segment databases.
	 */

	cdbCopy = makeCdbCopy(false);

	cdbCopy->partitions = RelationBuildPartitionDesc(cstate->rel, false);
	cdbCopy->skip_ext_partition = cstate->skip_ext_partition;

	/* XXX: lock all partitions */

	/* allocate memory for error and copy strings */
	initStringInfo(&cdbcopy_err);
	initStringInfo(&cdbcopy_cmd);

	/* create the command to send to QE's and store it in cdbcopy_cmd */
	CopyToCreateDispatchCommand(cstate,
								&cdbcopy_cmd,
								num_phys_attrs,
								attr);

	/*
	 * Start a COPY command in every db of every segment in Greenplum Database.
	 *
	 * From this point in the code we need to be extra careful
	 * about error handling. ereport() must not be called until
	 * the COPY command sessions are closed on the executors.
	 * Calling ereport() will leave the executors hanging in
	 * COPY state.
	 */
	elog(DEBUG5, "COPY command sent to segdbs: %s", cdbcopy_cmd.data);

	PG_TRY();
	{
		cdbCopyStart(cdbCopy, cdbcopy_cmd.data, NULL);

		if (cstate->binary)
		{
			/* Generate header for a binary copy */
			int32		tmp;

			/* Signature */
			CopySendData(cstate, (char *) BinarySignature, 11);
			/* Flags field */
			tmp = 0;
			if (cstate->oids)
				tmp |= (1 << 16);
			CopySendInt32(cstate, tmp);
			/* No header extension */
			tmp = 0;
			CopySendInt32(cstate, tmp);
		}

		/* if a header has been requested send the line */
		if (cstate->header_line)
		{
			ListCell   *cur;
			bool		hdr_delim = false;

			/*
			 * For non-binary copy, we need to convert null_print to client
			 * encoding, because it will be sent directly with CopySendString.
			 *
			 * MPP: in here we only care about this if we need to print the
			 * header. We rely on the segdb server copy out to do the conversion
			 * before sending the data rows out. We don't need to repeat it here
			 */
			if (cstate->need_transcoding)
				cstate->null_print = (char *)
					pg_server_to_custom(cstate->null_print,
										strlen(cstate->null_print),
										cstate->client_encoding,
										cstate->enc_conversion_proc);

			foreach(cur, cstate->attnumlist)
			{
				int			attnum = lfirst_int(cur);
				char	   *colname;

				if (hdr_delim)
					CopySendChar(cstate, cstate->delim[0]);
				hdr_delim = true;

				colname = NameStr(attr[attnum - 1]->attname);

				CopyAttributeOutCSV(cstate, colname, false,
									list_length(cstate->attnumlist) == 1);
			}

			/* add a newline and flush the data */
			CopySendEndOfRow(cstate);
		}

		/*
		 * This is the main work-loop. In here we keep collecting data from the
		 * COPY commands on the segdbs, until no more data is available. We
		 * keep writing data out a chunk at a time.
		 */
		while(true)
		{

			bool done;
			bool copy_cancel = (QueryCancelPending ? true : false);

			/* get a chunk of data rows from the QE's */
			done = cdbCopyGetData(cdbCopy, copy_cancel, &cstate->processed);

			/* send the chunk of data rows to destination (file or stdout) */
			if(cdbCopy->copy_out_buf.len > 0) /* conditional is important! */
			{
				/*
				 * in the dispatcher we receive chunks of whole rows with row endings.
				 * We don't want to use CopySendEndOfRow() b/c it adds row endings and
				 * also b/c it's intended for a single row at a time. Therefore we need
				 * to fill in the out buffer and just flush it instead.
				 */
				CopySendData(cstate, (void *) cdbCopy->copy_out_buf.data, cdbCopy->copy_out_buf.len);
				CopyToDispatchFlush(cstate);
			}

			if(done)
			{
				if(cdbCopy->remote_data_err || cdbCopy->io_errors)
					appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);

				break;
			}
		}
	}
    /* catch error from CopyStart, CopySendEndOfRow or CopyToDispatchFlush */
	PG_CATCH();
	{
		appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);

		/* TODO: end COPY in all the segdbs in progress */
		cdbCopyEnd(cdbCopy);

		ereport(LOG,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("%s", cdbcopy_err.data)));
		PG_RE_THROW();
	}
	PG_END_TRY();

	if (cstate->binary)
	{
		/* Generate trailer for a binary copy */
		CopySendInt16(cstate, -1);
		/* Need to flush out the trailer */
		CopySendEndOfRow(cstate);
	}

	/* we can throw the error now if QueryCancelPending was set previously */
	CHECK_FOR_INTERRUPTS();

	/*
	 * report all accumulated errors back to the client.
	 */
	if (cdbCopy->remote_data_err)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("%s", cdbcopy_err.data)));
	if (cdbCopy->io_errors)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("%s", cdbcopy_err.data)));

	pfree(cdbcopy_cmd.data);
	pfree(cdbcopy_err.data);
	pfree(cdbCopy);
}


/*
 * Copy from relation or query TO file.
 */
static void
CopyTo(CopyState cstate)
{
	TupleDesc	tupDesc;
	int			num_phys_attrs;
	Form_pg_attribute *attr;
	ListCell   *cur;
	List *target_rels = NIL;
	ListCell *lc;

	if (cstate->rel)
	{
		if (cstate->partitions)
		{
			ListCell *lc;
			List *relids = all_partition_relids(cstate->partitions);

			foreach(lc, relids)
			{
				Oid relid = lfirst_oid(lc);
				Relation rel = heap_open(relid, AccessShareLock);

				target_rels = lappend(target_rels, rel);
			}
		}
		else
			target_rels = lappend(target_rels, cstate->rel);

		tupDesc = RelationGetDescr(cstate->rel);
	}
	else
		tupDesc = cstate->queryDesc->tupDesc;

	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	cstate->null_print_client = cstate->null_print;		/* default */

	/* We use fe_msgbuf as a per-row buffer regardless of copy_dest */
	cstate->fe_msgbuf = makeStringInfo();

	/* Get info about the columns we need to process. */
	cstate->out_functions = (FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));
	foreach(cur, cstate->attnumlist)
	{
		int			attnum = lfirst_int(cur);
		Oid			out_func_oid;
		bool		isvarlena;

		if (cstate->binary)
			getTypeBinaryOutputInfo(attr[attnum - 1]->atttypid,
									&out_func_oid,
									&isvarlena);
		else
			getTypeOutputInfo(attr[attnum - 1]->atttypid,
							  &out_func_oid,
							  &isvarlena);
		fmgr_info(out_func_oid, &cstate->out_functions[attnum - 1]);
	}

	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype output routines, and should be faster than retail pfree's
	 * anyway.	(We don't need a whole econtext as CopyFrom does.)
	 */
	cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "COPY TO",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * we need to convert null_print to client
	 * encoding, because it will be sent directly with CopySendString.
	 */
	if (cstate->need_transcoding)
		cstate->null_print_client = pg_server_to_custom(cstate->null_print,
														cstate->null_print_len,
														cstate->client_encoding,
														cstate->enc_conversion_proc);

	if (cstate->binary)
	{
		/* binary header should not be sent in execute mode. */
		if (Gp_role != GP_ROLE_EXECUTE || cstate->on_segment)
		{
			/* Generate header for a binary copy */
			int32		tmp;

			/* Signature */
			CopySendData(cstate, (char *) BinarySignature, 11);
			/* Flags field */
			tmp = 0;
			if (cstate->oids)
				tmp |= (1 << 16);
			CopySendInt32(cstate, tmp);
			/* No header extension */
			tmp = 0;
			CopySendInt32(cstate, tmp);
		}
	}
	else
	{
		/* if a header has been requested send the line */
		if (cstate->header_line)
		{
			/* header should not be printed in execute mode. */
			if (Gp_role != GP_ROLE_EXECUTE || cstate->on_segment)
			{
				bool		hdr_delim = false;

				foreach(cur, cstate->attnumlist)
				{
					int			attnum = lfirst_int(cur);
					char	   *colname;

					if (hdr_delim)
						CopySendChar(cstate, cstate->delim[0]);
					hdr_delim = true;

					colname = NameStr(attr[attnum - 1]->attname);

					CopyAttributeOutCSV(cstate, colname, false,
										list_length(cstate->attnumlist) == 1);
				}
				CopySendEndOfRow(cstate);
			}
		}
	}

	if (cstate->rel)
	{
		foreach(lc, target_rels)
		{
			Relation rel = lfirst(lc);
			Datum	   *values;
			bool	   *nulls;
			HeapScanDesc scandesc = NULL;			/* used if heap table */
			AppendOnlyScanDesc aoscandesc = NULL;	/* append only table */

			tupDesc = RelationGetDescr(rel);
			attr = tupDesc->attrs;
			num_phys_attrs = tupDesc->natts;

			/*
			 * We need to update attnumlist because different partition
			 * entries might have dropped tables.
			 */
			cstate->attnumlist =
				CopyGetAttnums(tupDesc, rel, cstate->attnamelist);

			pfree(cstate->out_functions);
			cstate->out_functions =
				(FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));

			/* Get info about the columns we need to process. */
			foreach(cur, cstate->attnumlist)
			{
				int			attnum = lfirst_int(cur);
				Oid			out_func_oid;
				bool		isvarlena;

				if (cstate->binary)
					getTypeBinaryOutputInfo(attr[attnum - 1]->atttypid,
											&out_func_oid,
											&isvarlena);
				else
					getTypeOutputInfo(attr[attnum - 1]->atttypid,
									  &out_func_oid,
									  &isvarlena);

				fmgr_info(out_func_oid, &cstate->out_functions[attnum - 1]);
			}

			values = (Datum *) palloc(num_phys_attrs * sizeof(Datum));
			nulls = (bool *) palloc(num_phys_attrs * sizeof(bool));

			if (RelationIsHeap(rel))
			{
				HeapTuple	tuple;

				scandesc = heap_beginscan(rel, GetActiveSnapshot(), 0, NULL);
				while ((tuple = heap_getnext(scandesc, ForwardScanDirection)) != NULL)
				{
					CHECK_FOR_INTERRUPTS();

					/* Deconstruct the tuple ... faster than repeated heap_getattr */
					heap_deform_tuple(tuple, tupDesc, values, nulls);

					/* Format and send the data */
					CopyOneRowTo(cstate, HeapTupleGetOid(tuple), values, nulls);
				}

				heap_endscan(scandesc);
			}
			else if (RelationIsAoRows(rel))
			{
				MemTuple		tuple;
				TupleTableSlot	*slot = MakeSingleTupleTableSlot(tupDesc);
				MemTupleBinding *mt_bind = create_memtuple_binding(tupDesc);

				aoscandesc = appendonly_beginscan(rel, GetActiveSnapshot(),
												  GetActiveSnapshot(), 0, NULL);

				while ((tuple = appendonly_getnext(aoscandesc, ForwardScanDirection, slot)) != NULL)
				{
					CHECK_FOR_INTERRUPTS();

					/* Extract all the values of the  tuple */
					slot_getallattrs(slot);
					values = slot_get_values(slot);
					nulls = slot_get_isnull(slot);

					/* Format and send the data */
					CopyOneRowTo(cstate, MemTupleGetOid(tuple, mt_bind), values, nulls);
				}

				ExecDropSingleTupleTableSlot(slot);

				appendonly_endscan(aoscandesc);
			}
			else if (RelationIsAoCols(rel))
			{
				AOCSScanDesc scan = NULL;
				TupleTableSlot *slot = MakeSingleTupleTableSlot(tupDesc);
				bool *proj = NULL;

				int nvp = tupDesc->natts;
				int i;

				if (tupDesc->tdhasoid)
				{
				    elog(ERROR, "OIDS=TRUE is not allowed on tables that use column-oriented storage. Use OIDS=FALSE");
				}

				proj = palloc(sizeof(bool) * nvp);
				for(i = 0; i < nvp; ++i)
				    proj[i] = true;

				scan = aocs_beginscan(rel, GetActiveSnapshot(),
									  GetActiveSnapshot(),
									  NULL /* relationTupleDesc */, proj);
				for(;;)
				{
				    CHECK_FOR_INTERRUPTS();

				    aocs_getnext(scan, ForwardScanDirection, slot);
				    if (TupIsNull(slot))
				        break;

				    slot_getallattrs(slot);
				    values = slot_get_values(slot);
				    nulls = slot_get_isnull(slot);

					CopyOneRowTo(cstate, InvalidOid, values, nulls);
				}

				ExecDropSingleTupleTableSlot(slot);
				aocs_endscan(scan);

				pfree(proj);
			}
			else if(RelationIsExternal(rel))
			{
				/* should never get here */
				if (!cstate->skip_ext_partition)
				{
				    elog(ERROR, "internal error");
				}
			}
			else
			{
				/* should never get here */
				Assert(false);
			}

			/* partition table, so close */
			if (cstate->partitions)
				heap_close(rel, NoLock);
		}
	}
	else
	{
		Assert(Gp_role != GP_ROLE_EXECUTE);

		/* run the plan --- the dest receiver will send tuples */
		ExecutorRun(cstate->queryDesc, ForwardScanDirection, 0L);
	}

	/* binary trailer should not be sent in execute mode. */
	if (cstate->binary)
	{
		if (Gp_role != GP_ROLE_EXECUTE || (Gp_role == GP_ROLE_EXECUTE && cstate->on_segment))
		{
			/* Generate trailer for a binary copy */
			CopySendInt16(cstate, -1);

			/* Need to flush out the trailer */
			CopySendEndOfRow(cstate);
		}
	}

	if (Gp_role == GP_ROLE_EXECUTE && cstate->on_segment)
		SendNumRows(0, cstate->processed);

	MemoryContextDelete(cstate->rowcontext);
}

void
CopyOneCustomRowTo(CopyState cstate, bytea *value)
{
	appendBinaryStringInfo(cstate->fe_msgbuf,
						   VARDATA_ANY((void *) value),
						   VARSIZE_ANY_EXHDR((void *) value));
}

/*
 * Emit one row during CopyTo().
 */
void
CopyOneRowTo(CopyState cstate, Oid tupleOid, Datum *values, bool *nulls)
{
	bool		need_delim = false;
	FmgrInfo   *out_functions = cstate->out_functions;
	MemoryContext oldcontext;
	ListCell   *cur;
	char	   *string;

	MemoryContextReset(cstate->rowcontext);
	oldcontext = MemoryContextSwitchTo(cstate->rowcontext);

	if (cstate->binary)
	{
		/* Binary per-tuple header */
		CopySendInt16(cstate, list_length(cstate->attnumlist));
		/* Send OID if wanted --- note attnumlist doesn't include it */
		if (cstate->oids)
		{
			/* Hack --- assume Oid is same size as int32 */
			CopySendInt32(cstate, sizeof(int32));
			CopySendInt32(cstate, tupleOid);
		}
	}
	else
	{
		/* Text format has no per-tuple header, but send OID if wanted */
		/* Assume digits don't need any quoting or encoding conversion */
		if (cstate->oids)
		{
			string = DatumGetCString(DirectFunctionCall1(oidout,
												ObjectIdGetDatum(tupleOid)));
			CopySendString(cstate, string);
			need_delim = true;
		}
	}

	foreach(cur, cstate->attnumlist)
	{
		int			attnum = lfirst_int(cur);
		Datum		value = values[attnum - 1];
		bool		isnull = nulls[attnum - 1];

		if (!cstate->binary)
		{
			if (need_delim)
				CopySendChar(cstate, cstate->delim[0]);
			need_delim = true;
		}

		if (isnull)
		{
			if (!cstate->binary)
				CopySendString(cstate, cstate->null_print_client);
			else
				CopySendInt32(cstate, -1);
		}
		else
		{
			if (!cstate->binary)
			{
				char		quotec = cstate->quote ? cstate->quote[0] : '\0';

				/* int2out or int4out ? */
				if (out_functions[attnum -1].fn_oid == 39 ||  /* int2out or int4out */
					out_functions[attnum -1].fn_oid == 43 )
				{
					char tmp[33];
					/*
					 * The standard postgres way is to call the output function, but that involves one or more pallocs,
					 * and a call to sprintf, followed by a conversion to client charset.
					 * Do a fast conversion to string instead.
					 */

					if (out_functions[attnum -1].fn_oid ==  39)
						pg_itoa(DatumGetInt16(value),tmp);
					else
						pg_ltoa(DatumGetInt32(value),tmp);

					/*
					 * Integers don't need quoting, or transcoding to client char
					 * set. We still quote them if FORCE QUOTE was used, though.
					 */
					if (cstate->force_quote_flags[attnum - 1])
						CopySendChar(cstate, quotec);
					CopySendData(cstate, tmp, strlen(tmp));
					if (cstate->force_quote_flags[attnum - 1])
						CopySendChar(cstate, quotec);
				}
				else if (out_functions[attnum -1].fn_oid == 1702)   /* numeric_out */
				{
					string = OutputFunctionCall(&out_functions[attnum - 1],
												value);
					/*
					 * Numerics don't need quoting, or transcoding to client char
					 * set. We still quote them if FORCE QUOTE was used, though.
					 */
					if (cstate->force_quote_flags[attnum - 1])
						CopySendChar(cstate, quotec);
					CopySendData(cstate, string, strlen(string));
					if (cstate->force_quote_flags[attnum - 1])
						CopySendChar(cstate, quotec);
				}
				else
				{
					string = OutputFunctionCall(&out_functions[attnum - 1],
												value);
					if (cstate->csv_mode)
						CopyAttributeOutCSV(cstate, string,
											cstate->force_quote_flags[attnum - 1],
											list_length(cstate->attnumlist) == 1);
					else
						CopyAttributeOutText(cstate, string);
				}
			}
			else
			{
				bytea	   *outputbytes;

				outputbytes = SendFunctionCall(&out_functions[attnum - 1],
											   value);
				CopySendInt32(cstate, VARSIZE(outputbytes) - VARHDRSZ);
				CopySendData(cstate, VARDATA(outputbytes),
							 VARSIZE(outputbytes) - VARHDRSZ);
			}
		}
	}

	/*
	 * Finish off the row: write it to the destination, and update the count.
	 * However, if we're in the context of a writable external table, we let
	 * the caller do it - send the data to its local external source (see
	 * external_insert() ).
	 */
	if(cstate->copy_dest != COPY_EXTERNAL_SOURCE)
	{
		CopySendEndOfRow(cstate);
		cstate->processed++;
	}

	MemoryContextSwitchTo(oldcontext);
}

static void CopyFromProcessDataFileHeader(CopyState cstate, CdbCopy *cdbCopy, bool *pfile_has_oids)
{
	if (!cstate->binary)
	{
		*pfile_has_oids = cstate->oids;	/* must rely on user to tell us... */
	}
	else
	{
		/* Read and verify binary header */
		char		readSig[11];
		int32		tmp_flags, tmp_extension;
		int32		tmp;

		/* Signature */
		if (CopyGetData(cstate, readSig, 11) != 11 ||
			memcmp(readSig, BinarySignature, 11) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("COPY file signature not recognized")));
		/* Flags field */
		if (!CopyGetInt32(cstate, &tmp_flags))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("invalid COPY file header (missing flags)")));
		*pfile_has_oids = (tmp_flags & (1 << 16)) != 0;
		tmp = tmp_flags & ~(1 << 16);
		if ((tmp >> 16) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				errmsg("unrecognized critical flags in COPY file header")));
		/* Header extension length */
		if (!CopyGetInt32(cstate, &tmp_extension) ||
			tmp_extension < 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("invalid COPY file header (missing length)")));
		/* Skip extension header, if present */
		while (tmp_extension-- > 0)
		{
			if (CopyGetData(cstate, readSig, 1) != 1)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						errmsg("invalid COPY file header (wrong length)")));
		}

		/* Send binary header to all segments except:
			* dummy file on master for COPY FROM ON SEGMENT
			*/
		if(Gp_role == GP_ROLE_DISPATCH && !cstate->on_segment)
		{
			uint32 buf;
			cdbCopySendDataToAll(cdbCopy, (char *) BinarySignature, 11);
			buf = htonl((uint32) tmp_flags);
			cdbCopySendDataToAll(cdbCopy, (char *) &buf, 4);
			buf = htonl((uint32) 0);
			cdbCopySendDataToAll(cdbCopy, (char *) &buf, 4);
		}
	}

	if (*pfile_has_oids && cstate->binary)
	{
		FmgrInfo	oid_in_function;
		Oid			oid_typioparam;
		Oid			in_func_oid;

		getTypeBinaryInputInfo(OIDOID,
							&in_func_oid, &oid_typioparam);
		fmgr_info(in_func_oid, &oid_in_function);
	}
}

/*
 * CopyFromCreateDispatchCommand
 *
 * The COPY command that needs to get dispatched to the QE's isn't necessarily
 * the same command that arrived from the parser to the QD. For example, we
 * always change filename to STDIN, we may pre-evaluate constant values or
 * functions on the QD and send them to the QE with an extended column list.
 */
static int CopyFromCreateDispatchCommand(CopyState cstate,
										  StringInfo cdbcopy_cmd,
										  GpPolicy  *policy,
										  AttrNumber	num_phys_attrs,
										  AttrNumber	num_defaults,
										  AttrNumber	p_nattrs,
										  AttrNumber	h_attnum,
										  int *defmap,
										  ExprState **defexprs,
										  Form_pg_attribute *attr)
{
	ListCell   *cur;
	bool		is_first_col;
	int			i,
				p_index = 0;
	AttrNumber	extra_attr_count = 0; /* count extra attributes we add in the dispatcher COPY
										 usually non constant defaults we pre-evaluate in here */

	Assert(Gp_role == GP_ROLE_DISPATCH);

	/* append schema and tablename */
	appendStringInfo(cdbcopy_cmd, "COPY %s.%s",
					 quote_identifier(get_namespace_name(RelationGetNamespace(cstate->rel))),
					 quote_identifier(RelationGetRelationName(cstate->rel)));
	/*
	 * append column list. NOTE: if not specified originally, attnumlist will
	 * include all non-dropped columns of the table by default
	 */
	if(num_phys_attrs > 0) /* don't append anything for zero column table */
	{
		is_first_col = true;
		foreach(cur, cstate->attnumlist)
		{
			int			attnum = lfirst_int(cur);
			int			m = attnum - 1;

			/* We don't add dropped attributes */
			if (attr[m]->attisdropped)
				continue;

			/* append column string. quote it if needed */
			appendStringInfo(cdbcopy_cmd, (is_first_col ? "(%s" : ",%s"),
							 quote_identifier(NameStr(attr[m]->attname)));

			is_first_col = false;
		}

		/*
		 * In order to maintain consistency between the primary and mirror segment data, we
		 * want to evaluate all table columns that are not participating in this COPY command
		 * and have a non-constant default values on the dispatcher. If we let them evaluate
		 * on the primary and mirror executors separately - they will get different values.
		 * Also, if the distribution column is not participating and it has any default value,
		 * we have to evaluate it on the dispatcher only too, so that it wouldn't hash as a null
		 * and inserted as a default value on the segment databases.
		 *
		 * Therefore, we include these columns in the column list for the executor COPY.
		 * The default values will be evaluated on the dispatcher COPY and the results for
		 * the added columns will be appended to each data row that is shipped to the segments.
		 */
		extra_attr_count = 0;

		for (i = 0; i < num_defaults; i++)
		{
			bool add_to_list = false;

			/* check 1: is this default for a distribution column? */
			for (p_index = 0; p_index < p_nattrs; p_index++)
			{
				h_attnum = policy->attrs[p_index];

				if(h_attnum - 1 == defmap[i])
					add_to_list = true;
			}

			/* check 2: is this a non constant default? */
			if(defexprs[i]->expr->type != T_Const)
				add_to_list = true;

			if(add_to_list)
			{
				/* We don't add dropped attributes */
				/* XXXX: this check seems unnecessary given how CopyFromDispatch constructs defmap */
				if (attr[defmap[i]]->attisdropped)
					continue;

				/* append column string. quote it if needed */
				appendStringInfo(cdbcopy_cmd, (is_first_col ? "(%s" : ",%s"),
								 quote_identifier(NameStr(attr[defmap[i]]->attname)));

				extra_attr_count++;
				is_first_col = false;
			}
		}

		if (!is_first_col)
			appendStringInfo(cdbcopy_cmd, ")");
	}

	/*
	 * NOTE: we used to always pass STDIN here to the QEs. But since we want
	 * the QEs to know the original file name for recording it in an error log file
	 * (if they use one) we actually pass the filename here, and in the QE COPY
	 * we get it, save it, and then always revert back to actually using STDIN.
	 * (if we originally use STDIN we just pass it along and record that in the
	 * error log file).
	 */
	if(cstate->filename)
	{
		if (cstate->is_program)
			appendStringInfo(cdbcopy_cmd, " FROM PROGRAM %s WITH", quote_literal_internal(cstate->filename));
		else
			appendStringInfo(cdbcopy_cmd, " FROM %s WITH", quote_literal_internal(cstate->filename));
	}
	else
		appendStringInfo(cdbcopy_cmd, " FROM STDIN WITH");

	if (cstate->on_segment)
		appendStringInfo(cdbcopy_cmd, " ON SEGMENT");

	if (cstate->oids)
		appendStringInfo(cdbcopy_cmd, " OIDS");

	if (cstate->binary)
	{
		appendStringInfo(cdbcopy_cmd, " BINARY");
	}
	else
	{
		appendStringInfo(cdbcopy_cmd, " DELIMITER AS E'%s'", escape_quotes(cstate->delim));
		appendStringInfo(cdbcopy_cmd, " NULL AS E'%s'", escape_quotes(cstate->null_print));

		/* if default escape in text format ("\") leave expression out */
		if (!cstate->csv_mode && strcmp(cstate->escape, "\\") != 0)
			appendStringInfo(cdbcopy_cmd, " ESCAPE AS E'%s'", escape_quotes(cstate->escape));

		/* if EOL is already defined it means that NEWLINE was declared. pass it along */
		if (cstate->eol_type != EOL_UNKNOWN)
		{
			Assert(cstate->eol_str);
			appendStringInfo(cdbcopy_cmd, " NEWLINE AS '%s'", escape_quotes(cstate->eol_str));
		}

		if (cstate->csv_mode)
		{
			appendStringInfo(cdbcopy_cmd, " CSV");

			/*
			 * If on_segment, QE needs to write its own CSV header. If not,
			 * only QD needs to, QE doesn't send CSV header to QD
			 */
			if (cstate->on_segment && cstate->header_line)
				appendStringInfo(cdbcopy_cmd, " HEADER");

			appendStringInfo(cdbcopy_cmd, " QUOTE AS E'%s'", escape_quotes(cstate->quote));
			appendStringInfo(cdbcopy_cmd, " ESCAPE AS E'%s'", escape_quotes(cstate->escape));

			if(cstate->force_notnull)
			{
				ListCell   *l;

				is_first_col = true;
				appendStringInfo(cdbcopy_cmd, " FORCE NOT NULL");

				foreach(l, cstate->force_notnull)
				{
					const char	   *col_name = strVal(lfirst(l));

					appendStringInfo(cdbcopy_cmd, (is_first_col ? " %s" : ",%s"),
									 quote_identifier(col_name));
					is_first_col = false;
				}
			}
			/* do NOT include HEADER. Header row is "swallowed" by dispatcher COPY */
		}
	}

	if (cstate->fill_missing)
		appendStringInfo(cdbcopy_cmd, " FILL MISSING FIELDS");

	/* add single row error handling clauses if necessary */
	if (cstate->errMode != ALL_OR_NOTHING)
	{
		if (cstate->errMode == SREH_LOG)
		{
			appendStringInfoString(cdbcopy_cmd, " LOG ERRORS");
		}

		appendStringInfo(cdbcopy_cmd, " SEGMENT REJECT LIMIT %d %s",
						 cstate->cdbsreh->rejectlimit, (cstate->cdbsreh->is_limit_in_rows ? "ROWS" : "PERCENT"));
	}

	return extra_attr_count;
}

/*
 * Copy FROM file to relation.
 */
void
CopyFromDispatch(CopyState cstate)
{
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs,
				attr_count,
				num_defaults;
	FmgrInfo   *in_functions;
	FmgrInfo	oid_in_function;
	FmgrInfo   *out_functions; /* for handling defaults in Greenplum Database */
	Oid		   *typioparams;
	Oid			oid_typioparam = 0;
	int			attnum;
	int			i;
	int			p_index;
	Oid			in_func_oid;
	Oid			out_func_oid;
	Datum	   *values;
	bool	   *nulls;
	int		   *attr_offsets;
	int			total_rejected_from_qes = 0;
	int			total_completed_from_qes = 0;
	bool		isnull;
	bool	   *isvarlena;
	ResultRelInfo *resultRelInfo;
	EState	   *estate = CreateExecutorState(); /* for ExecConstraints() */
	bool		file_has_oids = false;
	int		   *defmap;
	ExprState **defexprs;		/* array of default att expressions */
	ExprContext *econtext;		/* used for ExecEvalExpr for default atts */
	MemoryContext oldcontext = CurrentMemoryContext;
	ErrorContextCallback errcontext;
	bool		no_more_data = false;
	bool		cur_row_rejected = false;
	CdbCopy    *cdbCopy;

	GpDistributionData *distData = NULL;		/*distribution policy for root table */
	GpDistributionData *part_distData = palloc(sizeof(GpDistributionData));		/* distribution policy for part table */
	GetAttrContext *getAttrContext = palloc(sizeof(GetAttrContext));		/* get attr values context */
	/* init partition data*/
	PartitionData *partitionData = palloc(sizeof(PartitionData));
	partitionData->part_values = NULL;
	partitionData->part_attr_types = NULL;
	partitionData->part_typio = NULL;
	partitionData->part_infuncs = NULL;
	partitionData->part_attnum = NULL;
	partitionData->part_attnums = 0;

	/*
	 * This stringInfo will contain 2 types of error messages:
	 *
	 * 1) Data errors refer to errors that are a result of inappropriate
	 *	  input data or constraint violations. All data error messages
	 *	  from the segment databases will be added to this variable and
	 *	  reported back to the client at the end of the copy command
	 *	  execution on the dispatcher.
	 * 2) Any command execution error that occurs during this COPY session.
	 *	  Such errors will usually be failure to send data over the network,
	 *	  a COPY command that was rejected by the segment databases or any I/O
	 *	  error.
	 */
	StringInfoData cdbcopy_err;

	/*
	 * a reconstructed and modified COPY command that is dispatched to segments.
	 */
	StringInfoData cdbcopy_cmd;

	/*
	 * Variables for cdbpolicy
	 */
	GpPolicy  *policy; /* the partitioning policy for this table */
	AttrNumber	p_nattrs; /* num of attributes in the distribution policy */
	Oid       *p_attr_types;	/* types for each policy attribute */

	/*
	 * Variables for original row number tracking
	 */
	StringInfoData line_buf_with_lineno;
	int			original_lineno_for_qe;

	/*
	 * Variables for cdbhash
	 */

	/*
	 * In the case of partitioned tables with children that have different
	 * distribution policies, we maintain a hash table of CdbHashs and
	 * GpPolicies for each child table. We lazily add them to the hash --
	 * when a partition is returned which we haven't seen before, we makeCdbHash
	 * and copy the policy over.
	 */

	CdbHash *cdbHash = NULL;
	AttrNumber	h_attnum;		/* hash key attribute number */
	unsigned int target_seg = 0;	/* result segment of cdbhash */

	tupDesc = RelationGetDescr(cstate->rel);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(cstate->attnumlist);
	num_defaults = 0;
	h_attnum = 0;

	/*
	 * Init original row number tracking vars
	 */
	initStringInfo(&line_buf_with_lineno);
	original_lineno_for_qe = 1;

	/*
	 * We need a ResultRelInfo so we can use the regular executor's
	 * index-entry-making machinery.  (There used to be a huge amount of
	 * code here that basically duplicated execUtils.c ...)
	 */
	resultRelInfo = makeNode(ResultRelInfo);
	resultRelInfo->ri_RangeTableIndex = 1;		/* dummy */
	resultRelInfo->ri_RelationDesc = cstate->rel;
	resultRelInfo->ri_TrigDesc = CopyTriggerDesc(cstate->rel->trigdesc);
	if (resultRelInfo->ri_TrigDesc)
		resultRelInfo->ri_TrigFunctions = (FmgrInfo *)
            palloc0(resultRelInfo->ri_TrigDesc->numtriggers * sizeof(FmgrInfo));
    resultRelInfo->ri_TrigInstrument = NULL;
    ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);

	ExecOpenIndices(resultRelInfo);

	estate->es_result_relations = resultRelInfo;
	estate->es_num_result_relations = 1;
	estate->es_result_relation_info = resultRelInfo;

	econtext = GetPerTupleExprContext(estate);

	/*
	 * Pick up the required catalog information for each attribute in the
	 * relation, including the input function, the element type (to pass
	 * to the input function), and info about defaults and constraints.
	 */
	in_functions = (FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));
	out_functions = (FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));
	typioparams = (Oid *) palloc(num_phys_attrs * sizeof(Oid));
	defmap = (int *) palloc(num_phys_attrs * sizeof(int));
	defexprs = (ExprState **) palloc(num_phys_attrs * sizeof(ExprState *));
	isvarlena = (bool *) palloc(num_phys_attrs * sizeof(bool));


	for (attnum = 1; attnum <= num_phys_attrs; attnum++)
	{
		/* We don't need info for dropped attributes */
		if (attr[attnum - 1]->attisdropped)
			continue;

		/* Fetch the input function and typioparam info */
		if (cstate->binary)
			getTypeBinaryInputInfo(attr[attnum - 1]->atttypid,
								   &in_func_oid, &typioparams[attnum - 1]);
		else
			getTypeInputInfo(attr[attnum - 1]->atttypid,
							 &in_func_oid, &typioparams[attnum - 1]);
		fmgr_info(in_func_oid, &in_functions[attnum - 1]);

		/*
		 * Fetch the output function and typioparam info. We need it
		 * for handling default functions on the dispatcher COPY, if
		 * there are any.
		 */
		if (cstate->binary)
			getTypeBinaryOutputInfo(attr[attnum - 1]->atttypid,
									&out_func_oid,
									&isvarlena[attnum - 1]);
		else
			getTypeOutputInfo(attr[attnum - 1]->atttypid,
							  &out_func_oid,
							  &isvarlena[attnum - 1]);
		fmgr_info(out_func_oid, &out_functions[attnum - 1]);

		/* TODO: is force quote array necessary for default conversion */

		/* Get default info if needed */
		if (!list_member_int(cstate->attnumlist, attnum))
		{
			/* attribute is NOT to be copied from input */
			/* use default value if one exists */
			Node	   *defexpr = build_column_default(cstate->rel, attnum);

			if (defexpr != NULL)
			{
				defexprs[num_defaults] = ExecPrepareExpr((Expr *) defexpr,
														 estate);
				defmap[num_defaults] = attnum - 1;
				num_defaults++;
			}
		}
	}

	/*
	 * prepare to COPY data into segDBs:
	 * - set table partitioning information
	 * - set append only table relevant info for dispatch.
	 * - get the distribution policy for this table.
	 * - build a COPY command to dispatch to segdbs.
	 * - dispatch the modified COPY command to all segment databases.
	 * - prepare cdbhash for hashing on row values.
	 */
	cdbCopy = makeCdbCopy(true);

	estate->es_result_partitions = cdbCopy->partitions =
		RelationBuildPartitionDesc(cstate->rel, false);

	CopyInitPartitioningState(estate);


	if (list_length(cstate->ao_segnos) > 0)
		cdbCopy->ao_segnos = cstate->ao_segnos;

	/* add cdbCopy reference to cdbSreh (if needed) */
	if (cstate->errMode != ALL_OR_NOTHING)
		cstate->cdbsreh->cdbcopy = cdbCopy;

	/* get data for distribution */
	bool multi_dist_policy = estate->es_result_partitions
	        && !partition_policies_equal(cstate->rel->rd_cdbpolicy,
	                                     estate->es_result_partitions);
	distData = InitDistributionData(cstate, attr, num_phys_attrs,
	                                estate, multi_dist_policy);
	policy = distData->policy;
	cdbHash = distData->cdbHash;
	p_attr_types = distData->p_attr_types;
	p_nattrs = distData->p_nattrs;
	/* allocate memory for error and copy strings */
	initStringInfo(&cdbcopy_err);
	initStringInfo(&cdbcopy_cmd);

	/* store the COPY command string in cdbcopy_cmd */
	int extra_attr_count = CopyFromCreateDispatchCommand(cstate,
								  &cdbcopy_cmd,
								  policy,
								  num_phys_attrs,
								  num_defaults,
								  p_nattrs,
								  h_attnum,
								  defmap,
								  defexprs,
								  attr);

	/* init partition routing data structure */
	if (estate->es_result_partitions)
	{
		InitPartitionData(partitionData, estate, attr, num_phys_attrs, oldcontext);
	}
	/*
	 * Dispatch the COPY command.
	 *
	 * From this point in the code we need to be extra careful about error
	 * handling. ereport() must not be called until the COPY command sessions
	 * are closed on the executors. Calling ereport() will leave the executors
	 * hanging in COPY state.
	 *
	 * For errors detected by the dispatcher, we save the error message in
	 * cdbcopy_err StringInfo, move on to closing all COPY sessions on the
	 * executors and only then raise an error. We need to make sure to TRY/CATCH
	 * all other errors that may be raised from elsewhere in the backend. All
	 * error during COPY on the executors will be detected only when we end the
	 * COPY session there, so we are fine there.
	 */
	elog(DEBUG5, "COPY command sent to segdbs: %s", cdbcopy_cmd.data);
	PG_TRY();
	{
		cdbCopyStart(cdbCopy, cdbcopy_cmd.data, cstate->rel->rd_cdbpolicy);
	}
	PG_CATCH();
	{
		/* get error message from CopyStart */
		appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);

		/* end COPY in all the segdbs in progress */
		cdbCopyEnd(cdbCopy);

		/* get error message from CopyEnd */
		appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);

		ereport(LOG,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("%s", cdbcopy_err.data)));
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* Prepare to catch AFTER triggers. */
	//AfterTriggerBeginQuery();

	/*
	 * Check BEFORE STATEMENT insertion triggers. It's debateable whether we
	 * should do this for COPY, since it's not really an "INSERT" statement as
	 * such. However, executing these triggers maintains consistency with the
	 * EACH ROW triggers that we already fire on COPY.
	 */
	//ExecBSInsertTriggers(estate, resultRelInfo);

	/* Skip header processing if dummy file on master for COPY FROM ON SEGMENT */
	if (!cstate->on_segment || Gp_role != GP_ROLE_DISPATCH)
	{
		CopyFromProcessDataFileHeader(cstate, cdbCopy, &file_has_oids);
	}

	values = (Datum *) palloc(num_phys_attrs * sizeof(Datum));
	nulls = (bool *) palloc(num_phys_attrs * sizeof(bool));
	attr_offsets = (int *) palloc(num_phys_attrs * sizeof(int));

	/* Set up callback to identify error line number */
	errcontext.callback = copy_in_error_callback;
	errcontext.arg = (void *) cstate;
	errcontext.previous = error_context_stack;
	error_context_stack = &errcontext;
	cstate->err_loc_type = ROWNUM_ORIGINAL;

	CopyInitDataParser(cstate);

	do
	{
		size_t		bytesread = 0;

		if (!cstate->binary)
		{
			/* read a chunk of data into the buffer */
			PG_TRY();
			{
				bytesread = CopyGetData(cstate, cstate->raw_buf, RAW_BUF_SIZE);
			}
			PG_CATCH();
			{
				/*
				 * If we are here, we got some kind of communication error
				 * with the client or a bad protocol message. clean up and
				 * re-throw error. Note that we don't handle this error in
				 * any special way in SREH mode as it's not a data error.
				 */
				cdbCopyEnd(cdbCopy);
				PG_RE_THROW();
			}
			PG_END_TRY();

			cstate->raw_buf_done = false;

			/* set buffer pointers to beginning of the buffer */
			cstate->begloc = cstate->raw_buf;
			cstate->raw_buf_index = 0;
		}

		/*
		 * continue if some bytes were read or if we didn't reach EOF. if we
		 * both reached EOF _and_ no bytes were read, quit the loop we are
		 * done
		 */
		if (bytesread > 0 || !cstate->fe_eof)
		{
			/* on first time around just throw the header line away */
			if (cstate->header_line)
			{
				PG_TRY();
				{
					cstate->line_done = cstate->csv_mode ?
						CopyReadLineCSV(cstate, bytesread) :
						CopyReadLineText(cstate, bytesread);
				}
				PG_CATCH();
				{
					/*
					 * TODO: use COPY_HANDLE_ERROR here, but make sure to
					 * ignore this error per the "note:" below.
					 */

					/*
					 * got here? encoding conversion error occured on the
					 * header line (first row).
					 */
					if(cstate->errMode == ALL_OR_NOTHING)
					{
						/* re-throw error and abort */
						cdbCopyEnd(cdbCopy);
						PG_RE_THROW();
					}
					else
					{
						/* SREH - release error state */
						if(!elog_dismiss(DEBUG5))
							PG_RE_THROW(); /* hope to never get here! */

						/*
						 * note: we don't bother doing anything special here.
						 * we are never interested in logging a header line
						 * error. just continue the workflow.
						 */
					}
				}
				PG_END_TRY();

				cstate->cur_lineno++;
				RESET_LINEBUF;

				cstate->header_line = false;
			}

			while (!cstate->raw_buf_done)
			{
				part_distData->cdbHash = NULL;
				part_distData->policy = NULL;
				Oid loaded_oid = InvalidOid;
				if (QueryCancelPending)
				{
					/* quit processing loop */
					no_more_data = true;
					break;
				}

				/* Reset the per-tuple exprcontext */
				ResetPerTupleExprContext(estate);

				/* Switch into its memory context */
				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

				/* Initialize all values for row to NULL */
				MemSet(values, 0, num_phys_attrs * sizeof(Datum));
				MemSet(nulls, true, num_phys_attrs * sizeof(bool));
				MemSet(attr_offsets, 0, num_phys_attrs * sizeof(int));

				/* Get the line number of the first line of this data row */
				original_lineno_for_qe = cstate->cur_lineno + 1;

				if (!cstate->binary)
				{
				PG_TRY();
				{
					/* Actually read the line into memory here */
					cstate->line_done = cstate->csv_mode ?
						CopyReadLineCSV(cstate, bytesread) :
						CopyReadLineText(cstate, bytesread);
				}
				PG_CATCH();
				{
					/* got here? encoding conversion/check error occurred */
					COPY_HANDLE_ERROR;
				}
				PG_END_TRY();

				if(cur_row_rejected)
				{
					ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
					QD_GOTO_NEXT_ROW;
				}


				if(!cstate->line_done)
				{
					/*
					 * if eof reached, and no data in line_buf,
					 * we don't need to do att parsing
					 */
					if (cstate->fe_eof && cstate->line_buf.len == 0)
					{
						break;
					}
					/*
					 * We did not finish reading a complete data line.
					 *
					 * If eof is not yet reached, we skip att parsing
					 * and read more data. But if eof _was_ reached it means
					 * that the original last data line is defective and
					 * we want to catch that error later on.
					 */
					if (!cstate->fe_eof || cstate->end_marker)
						break;
				}

				if (file_has_oids)
				{
					char	   *oid_string;

					/* can't be in CSV mode here */
					oid_string = CopyReadOidAttr(cstate, &isnull);

					if (isnull)
					{
						/* got here? null in OID column error */

						if(cstate->errMode == ALL_OR_NOTHING)
						{
							/* report error and abort */
							cdbCopyEnd(cdbCopy);

							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("null OID in COPY data.")));
						}
						else
						{
							/* SREH */
							cstate->cdbsreh->rejectcount++;
							cur_row_rejected = true;
						}

					}
					else
					{
						PG_TRY();
						{
							cstate->cur_attname = "oid";
							loaded_oid = DatumGetObjectId(DirectFunctionCall1(oidin,
										   CStringGetDatum(oid_string)));
						}
						PG_CATCH();
						{
							/* got here? oid column conversion failed */
							COPY_HANDLE_ERROR;
						}
						PG_END_TRY();

						if (loaded_oid == InvalidOid)
						{
							if(cstate->errMode == ALL_OR_NOTHING)
							{
								/* report error and abort */
								cdbCopyEnd(cdbCopy);

								ereport(ERROR,
										(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
										 errmsg("invalid OID in COPY data.")));
							}
							else
							{
								/* SREH */
								cstate->cdbsreh->rejectcount++;
								cur_row_rejected = true;
							}
						}

						cstate->cur_attname = NULL;
					}

					if(cur_row_rejected)
					{
						ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
						QD_GOTO_NEXT_ROW;
					}
				}
				}
				else
				{
					/*
					 * Binary mode, not doing anything here;
					 * Deferring "line" segmenting and parsing to next code block.
					 */
				}

				PG_TRY();
				{
					/*
					 * parse and convert the data line attributes.
					 */
					if (!cstate->binary)
					{
					if (cstate->csv_mode)
						CopyReadAttributesCSV(cstate, nulls, attr_offsets, num_phys_attrs, attr);
					else
						CopyReadAttributesText(cstate, nulls, attr_offsets, num_phys_attrs, attr);

						/* Parse only partition attributes */
					attr_get_key(cstate, cdbCopy,
								 original_lineno_for_qe,
								 target_seg,
								 p_nattrs, policy->attrs,
								 attr, attr_offsets, nulls,
							   	 in_functions, typioparams,
								 values);
					}
					else
					{
						/* binary */
						int16		fld_count;
						int32		fld_size;
						char buffer[20];
						ListCell   *cur;

						resetStringInfo(&cstate->line_buf);

						if (!CopyGetInt16(cstate, &fld_count) ||
							fld_count == -1)
						{
							no_more_data = true;
							break;
						}

						cstate->cur_lineno++;

						/*
						 * copy to line_buf
						*/
						uint16 fld_count_be = htons((uint16) fld_count + extra_attr_count);
						appendBinaryStringInfo(&cstate->line_buf, &fld_count_be, 2);

						if (fld_count != attr_count)
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("QE: line %s: row field count is %d, expected %d",
									 		linenumber_atoi(buffer, cstate->cur_lineno),
											(int) fld_count, attr_count)));

						if (file_has_oids)
						{
							cstate->cur_attname = "oid";
							loaded_oid =
								DatumGetObjectId(CopyReadBinaryAttribute(cstate,
																		 0,
																		 &oid_in_function,
																		 oid_typioparam,
																		 -1,
																		 &isnull,
																		 false));
							fld_size = isnull ? -1 : cstate->attribute_buf.len;
							uint32 fld_size_be = htonl((uint32) fld_size);
							appendBinaryStringInfo(&cstate->line_buf,
												   &fld_size_be,
												   4);
							if (!isnull)
								appendBinaryStringInfo(&cstate->line_buf,
													   cstate->attribute_buf.data,
													   cstate->attribute_buf.len);
							if (isnull || loaded_oid == InvalidOid)
								ereport(ERROR,
										(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
										 errmsg("invalid OID in COPY data")));
							cstate->cur_attname = NULL;
						}

						i = 0;
						AttrNumber p_index;
						foreach(cur, cstate->attnumlist)
						{
							int			attnum = lfirst_int(cur);
							int			m = attnum - 1;

							cstate->cur_attname = NameStr(attr[m]->attname);
							i++;

							bool skip_parsing = true;
							/* using same logic as the two invocations of attr_get_key */
							for (p_index = 0; p_index < p_nattrs; p_index++)
							{
								if (attnum == policy->attrs[p_index])
								{
									skip_parsing = false;
									break;
								}
							}
							if (skip_parsing && partitionData->part_attnums > 0) {
								for (p_index = 0; p_index < p_nattrs; p_index++) {
									if (attnum == partitionData->part_attnum[p_index])
									{
										skip_parsing = false;
										break;
									}
								}
							}
							values[m] = CopyReadBinaryAttribute(cstate,
																i,
																&in_functions[m],
																typioparams[m],
																attr[m]->atttypmod,
																&isnull,
																skip_parsing);
							fld_size = isnull ? -1 : cstate->attribute_buf.len;
							uint32 fld_size_be = htonl((uint32) fld_size);
							appendBinaryStringInfo(&cstate->line_buf,
												   &fld_size_be,
												   4);
							if (!isnull)
								appendBinaryStringInfo(&cstate->line_buf,
													   cstate->attribute_buf.data,
													   cstate->attribute_buf.len);
							nulls[m] = isnull;
							cstate->cur_attname = NULL;
						}
					}

					/*
					 * Now compute defaults for only:
					 * 1 - the distribution column,
					 * 2 - any other column with a non-constant default expression
					 * (such as a function) that is, of course, if these columns
					 * not provided by the input data.
					 * Anything not processed here or above will remain NULL.
					 *
					 * These are fields in addition to those specified in the original COPY command.
					 * They are computed by QD here and fed to the QEs.
					 * See same logic and comments in CopyFromCreateDispatchCommand
					 */
					for (i = 0; i < num_defaults; i++)
					{
						bool compute_default = false;

						/* check 1: is this default for a distribution column? */
						for (p_index = 0; p_index < p_nattrs; p_index++)
						{
							h_attnum = policy->attrs[p_index];

							if(h_attnum - 1 == defmap[i])
								compute_default = true;
						}

						/* check 2: is this a default function? (non-constant default) */
						if(defexprs[i]->expr->type != T_Const)
							compute_default = true;

						if(compute_default)
						{
							values[defmap[i]] = ExecEvalExpr(defexprs[i], econtext,
															 &isnull, NULL);

							/* Extend line_buf for the QDs */
							if (!cstate->binary)
							{
								char *string;
							/*
							 * prepare to concatinate next value:
							 * remove eol characters from end of line buf
							 */
							truncateEol(&cstate->line_buf, cstate->eol_type);

							if (isnull)
							{
								appendStringInfo(&cstate->line_buf, "%c%s", cstate->delim[0], cstate->null_print);
							}
							else
							{
								nulls[defmap[i]] = false;

								appendStringInfo(&cstate->line_buf, "%c", cstate->delim[0]); /* write the delimiter */

								string = DatumGetCString(FunctionCall3(&out_functions[defmap[i]],
																	   values[defmap[i]],
																	   ObjectIdGetDatum(typioparams[defmap[i]]),
																	   Int32GetDatum(attr[defmap[i]]->atttypmod)));
								if (cstate->csv_mode)
								{
									CopyAttributeOutCSV(cstate, string,
														false, /*force_quote[attnum - 1],*/
														list_length(cstate->attnumlist) == 1);
								}
								else
									CopyAttributeOutText(cstate, string);
							}

							/* re-add the eol characters */
							concatenateEol(cstate);
						}
							else
							{
								/* binary format */
								if (isnull) {
									uint32 fld_size_be = htonl((uint32) -1);
									appendBinaryStringInfo(&cstate->line_buf,
														   &fld_size_be,
														   4);
								} else {
									bytea	   *outputbytes;
									outputbytes = SendFunctionCall(&out_functions[defmap[i]],
																   FunctionCall3(&out_functions[defmap[i]],
																		   		 values[defmap[i]],
																		   		 ObjectIdGetDatum(typioparams[defmap[i]]),
																		   		 Int32GetDatum(attr[defmap[i]]->atttypmod)));
									int32 fld_size = VARSIZE(outputbytes) - VARHDRSZ;
									uint32 fld_size_be = htonl((uint32) fld_size);
									appendBinaryStringInfo(&cstate->line_buf,
														   &fld_size_be,
														   4);
									appendBinaryStringInfo(&cstate->line_buf,
														   VARDATA(outputbytes),
														   fld_size);
								}
							}
						}

					}
					/* lock partition */
					if (estate->es_result_partitions)
					{
						getAttrContext->tupDesc = tupDesc;
						getAttrContext->attr = attr;
						getAttrContext->num_phys_attrs = num_phys_attrs;
						getAttrContext->attr_offsets = attr_offsets;
						getAttrContext->nulls = nulls;
						getAttrContext->values = values;
						getAttrContext->cdbCopy = cdbCopy;
						getAttrContext->original_lineno_for_qe =
							original_lineno_for_qe;
						part_distData = GetDistributionPolicyForPartition(
							        cstate, estate, partitionData,
							        distData->hashmap, distData->p_attr_types,
							        getAttrContext, oldcontext);
					}

					if (!part_distData->cdbHash)
					{
						part_distData->policy = distData->policy;
						part_distData->cdbHash = distData->cdbHash;
						part_distData->p_attr_types = distData->p_attr_types;
						part_distData->hashmap = distData->hashmap;
						part_distData->p_nattrs =distData->p_nattrs;
					}
					/*
					 * policy should be PARTITIONED (normal tables) or
					 * ENTRY
					 */
					if (!part_distData->policy)
					{
						elog(FATAL, "Bad or undefined policy. (%p)", part_distData->policy);
					}
				}
				PG_CATCH();
				{
					COPY_HANDLE_ERROR;
				}
				PG_END_TRY();

				if (no_more_data)
					break;

				if(cur_row_rejected)
				{
					ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
					QD_GOTO_NEXT_ROW;
				}

				/*
				 * At this point in the code, values[x] is final for this
				 * data row -- either the input data, a null or a default
				 * value is in there, and constraints applied.
				 *
				 * Perform a cdbhash on this data row. Perform a hash operation
				 * on each attribute that is included in CDB policy (partitioning
				 * key columns). Send COPY data line to the target segment
				 * database executors. Data row will not be inserted locally.
				 */
				target_seg  = GetTargetSeg(part_distData, values, nulls);
				/*
				 * Send data row to all databases for this segment.
				 * Also send the original row number with the data.
				 */
				if (!cstate->binary)
				{
					/*
					 * Text/CSV: modify the data to look like:
				 *    "<lineno>^<linebuf_converted>^<data>"
				 */
				appendStringInfo(&line_buf_with_lineno, "%d%c%d%c%s",
								 original_lineno_for_qe,
								 COPY_METADATA_DELIM,
								 cstate->line_buf_converted, \
								 COPY_METADATA_DELIM, \
								 cstate->line_buf.data);
				}
				else
				{
					/*
					 * Binary: modify the data to look like:
					 *    "<lineno:int64><data:bytes>"
					 */
					uint64 lineno = htonll((uint64) original_lineno_for_qe);
					appendBinaryStringInfo(&line_buf_with_lineno,
										   &lineno,
										   sizeof(lineno));
					appendBinaryStringInfo(&line_buf_with_lineno,
										   cstate->line_buf.data,
										   cstate->line_buf.len);
				}
				
				/* send modified data */
				if (!cstate->on_segment) {
					cdbCopySendData(cdbCopy,
									target_seg,
									line_buf_with_lineno.data,
									line_buf_with_lineno.len);
					RESET_LINEBUF_WITH_LINENO;
				}

				cstate->processed++;
				if (estate->es_result_partitions)
					resultRelInfo->ri_aoprocessed++;

				if (cdbCopy->io_errors)
				{
					appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);
					no_more_data = true;
					break;
				}

				RESET_LINEBUF;

			}					/* end while(!raw_buf_done) */
		}						/* end if (bytesread > 0 || !cstate->fe_eof) */
		else
			/* no bytes read, end of data */
		{
			no_more_data = true;
		}
	} while (!no_more_data);

	/*
	 * Done reading input data and sending it off to the segment
	 * databases Now we would like to end the copy command on
	 * all segment databases across the cluster.
	 */
	total_rejected_from_qes = cdbCopyEndAndFetchRejectNum(cdbCopy, &total_completed_from_qes);

	/*
	 * If we quit the processing loop earlier due to a
	 * cancel query signal, we now throw an error.
	 * (Safe to do only after cdbCopyEnd).
	 */
	CHECK_FOR_INTERRUPTS();


	if (cdbCopy->remote_data_err || cdbCopy->io_errors)
		appendBinaryStringInfo(&cdbcopy_err, cdbCopy->err_msg.data, cdbCopy->err_msg.len);

	if (cdbCopy->remote_data_err)
	{
		cstate->error_on_executor = true;
		if(cdbCopy->err_context.len > 0)
			appendBinaryStringInfo(&cstate->executor_err_context, cdbCopy->err_context.data, cdbCopy->err_context.len);
	}

	/*
	 * report all accumulated errors back to the client. We get here if an error
	 * happened in all-or-nothing error handling mode or if reject limit was
	 * reached in single-row error handling mode.
	 */
	if (cdbCopy->remote_data_err)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("%s", cdbcopy_err.data)));
	if (cdbCopy->io_errors)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				 errmsg("%s", cdbcopy_err.data)));

	/*
	 * switch back away from COPY error context callback. don't want line
	 * error information anymore
	 */
	error_context_stack = errcontext.previous;

	/*
	 * If we got here it means that either all the data was loaded or some rows
	 * were rejected in SREH mode. In other words - nothing caused an abort.
	 * We now want to report the actual number of rows loaded and rejected.
	 * If any rows were rejected from the QE COPY processes subtract this number
	 * from the number of rows that were successfully processed on the QD COPY
	 * so that we can report the correct number.
	 */
	if(cstate->cdbsreh)
	{
		int total_rejected = 0;
		int total_rejected_from_qd = cstate->cdbsreh->rejectcount;

		/*
		 * If error log has been requested, then we send the row to the segment
		 * so that it can be written in the error log file. The segment process
		 * counts it again as a rejected row. So we ignore the reject count
		 * from the master and only consider the reject count from segments.
		 */
		if (cstate->cdbsreh->log_to_file)
			total_rejected_from_qd = 0;

		total_rejected = total_rejected_from_qd + total_rejected_from_qes;
		cstate->processed -= total_rejected;

		/* emit a NOTICE with number of rejected rows */
		ReportSrehResults(cstate->cdbsreh, total_rejected);
	}

	cstate->processed += total_completed_from_qes;

	/*
	 * Done, clean up
	 */
	MemoryContextSwitchTo(oldcontext);

	/* Execute AFTER STATEMENT insertion triggers */
	//ExecASInsertTriggers(estate, resultRelInfo);

	/* Handle queued AFTER triggers */
	//AfterTriggerEndQuery(estate);

	resultRelInfo = estate->es_result_relations;
	for (i = estate->es_num_result_relations; i > 0; i--)
	{
		/* update AO tuple counts */
		char relstorage = RelinfoGetStorage(resultRelInfo);
		if (relstorage_is_ao(relstorage))
		{
			if (cdbCopy->aotupcounts)
			{
				HTAB *ht = cdbCopy->aotupcounts;
				struct {
					Oid relid;
					int64 tupcount;
				} *ao;
				bool found;
				Oid relid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

				ao = hash_search(ht, &relid, HASH_FIND, &found);
				if (found)
				{
   	 				/* find out which segnos the result rels in the QE's used */
    			    ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);

    				UpdateMasterAosegTotals(resultRelInfo->ri_RelationDesc,
											resultRelInfo->ri_aosegno,
											ao->tupcount, 1);
				}
			}
			else
			{
				ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);
				UpdateMasterAosegTotals(resultRelInfo->ri_RelationDesc,
										resultRelInfo->ri_aosegno,
										cstate->processed, 1);
			}
		}

		/* Close indices and then the relation itself */
		ExecCloseIndices(resultRelInfo);
		heap_close(resultRelInfo->ri_RelationDesc, NoLock);
		resultRelInfo++;
	}

	/*
	 * free all resources besides ones that are needed for error reporting
	 */
	pfree(values);
	pfree(nulls);
	pfree(attr_offsets);
	pfree(in_functions);
	pfree(out_functions);
	pfree(isvarlena);
	pfree(typioparams);
	pfree(defmap);
	pfree(defexprs);
	pfree(cdbcopy_cmd.data);
	pfree(cdbcopy_err.data);
	pfree(line_buf_with_lineno.data);
	pfree(cdbCopy);
	pfree(part_distData);
	pfree(getAttrContext);
	FreePartitionData(partitionData);
	FreeDistributionData(distData);

	/*
	 * Don't worry about the partition table hash map, that will be
	 * freed when our current memory context is freed. And that will be
	 * quite soon.
	 */
	
	cstate->rel = NULL; /* closed above */
	FreeExecutorState(estate);
}

/*
 * Copy FROM file to relation.
 */
static void
CopyFrom(CopyState cstate)
{
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs,
				attr_count,
				num_defaults;
	FmgrInfo   *in_functions;
	FmgrInfo	oid_in_function;
	Oid		   *typioparams;
	Oid			oid_typioparam = 0;
	int			attnum;
	int			i;
	Oid			in_func_oid;
	Datum		*partValues = NULL;
	bool		*partNulls = NULL;
	bool		isnull;
	ResultRelInfo *resultRelInfo;
	EState	   *estate = CreateExecutorState(); /* for ExecConstraints() */
	TupleTableSlot *baseSlot;
	TupleTableSlot *slot;
	bool		file_has_oids = false;
	int		   *defmap;
	ExprState **defexprs;		/* array of default att expressions */
	ExprContext *econtext;		/* used for ExecEvalExpr for default atts */
	MemoryContext oldcontext = CurrentMemoryContext;
	ErrorContextCallback errcontext;
	CommandId	mycid = GetCurrentCommandId(true);
	int			hi_options = 0; /* start with default heap_insert options */
	BulkInsertState bistate;
	int		   *attr_offsets;
	bool		no_more_data = false;
	ListCell   *cur;
	bool		cur_row_rejected = false;
	int			original_lineno_for_qe = 0; /* keep compiler happy (var referenced by macro) */
	CdbCopy    *cdbCopy = NULL; /* never used... for compiling COPY_HANDLE_ERROR */
	tupDesc = RelationGetDescr(cstate->rel);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(cstate->attnumlist);
	num_defaults = 0;
	bool		is_segment_data_processed = (cstate->on_segment && Gp_role == GP_ROLE_EXECUTE) ? false : true;
	bool is_check_distkey = (cstate->on_segment && Gp_role == GP_ROLE_EXECUTE && gp_enable_segment_copy_checking) ? true : false;
	GpDistributionData	*distData = NULL; /* distribution data used to compute target seg */
	unsigned int	target_seg = 0; /* result segment of cdbhash */

	/*----------
	 * Check to see if we can avoid writing WAL
	 *
	 * If archive logging/streaming is not enabled *and* either
	 *	- table was created in same transaction as this COPY
	 *	- data is being written to relfilenode created in this transaction
	 * then we can skip writing WAL.  It's safe because if the transaction
	 * doesn't commit, we'll discard the table (or the new relfilenode file).
	 * If it does commit, we'll have done the heap_sync at the bottom of this
	 * routine first.
	 *
	 * As mentioned in comments in utils/rel.h, the in-same-transaction test
	 * is not completely reliable, since in rare cases rd_createSubid or
	 * rd_newRelfilenodeSubid can be cleared before the end of the transaction.
	 * However this is OK since at worst we will fail to make the optimization.
	 *
	 * Also, if the target file is new-in-transaction, we assume that checking
	 * FSM for free space is a waste of time, even if we must use WAL because
	 * of archiving.  This could possibly be wrong, but it's unlikely.
	 *
	 * The comments for heap_insert and RelationGetBufferForTuple specify that
	 * skipping WAL logging is only safe if we ensure that our tuples do not
	 * go into pages containing tuples from any other transactions --- but this
	 * must be the case if we have a new table or new relfilenode, so we need
	 * no additional work to enforce that.
	 *----------
	 */
	if (cstate->rel->rd_createSubid != InvalidSubTransactionId ||
		cstate->rel->rd_newRelfilenodeSubid != InvalidSubTransactionId)
	{
		hi_options |= HEAP_INSERT_SKIP_FSM;
		if (!XLogIsNeeded())
			hi_options |= HEAP_INSERT_SKIP_WAL;
	}

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * We need a ResultRelInfo so we can use the regular executor's
	 * index-entry-making machinery.  (There used to be a huge amount of code
	 * here that basically duplicated execUtils.c ...)
	 */
	resultRelInfo = makeNode(ResultRelInfo);
	resultRelInfo->ri_RangeTableIndex = 1;		/* dummy */
	resultRelInfo->ri_RelationDesc = cstate->rel;
	resultRelInfo->ri_TrigDesc = CopyTriggerDesc(cstate->rel->trigdesc);
	if (resultRelInfo->ri_TrigDesc)
		resultRelInfo->ri_TrigFunctions = (FmgrInfo *)
			palloc0(resultRelInfo->ri_TrigDesc->numtriggers * sizeof(FmgrInfo));
	resultRelInfo->ri_TrigInstrument = NULL;
	ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);

	ExecOpenIndices(resultRelInfo);

	estate->es_result_relations = resultRelInfo;
	estate->es_num_result_relations = 1;
	estate->es_result_relation_info = resultRelInfo;
	estate->es_result_partitions = cstate->partitions;

	CopyInitPartitioningState(estate);

	/* Set up a tuple slot too */
	baseSlot = MakeSingleTupleTableSlot(tupDesc);

	econtext = GetPerTupleExprContext(estate);

	/*
	 * Pick up the required catalog information for each attribute in the
	 * relation, including the input function, the element type (to pass to
	 * the input function), and info about defaults and constraints.
	 */
	in_functions = (FmgrInfo *) palloc(num_phys_attrs * sizeof(FmgrInfo));
	typioparams = (Oid *) palloc(num_phys_attrs * sizeof(Oid));
	defmap = (int *) palloc(num_phys_attrs * sizeof(int));
	defexprs = (ExprState **) palloc(num_phys_attrs * sizeof(ExprState *));

	for (attnum = 1; attnum <= num_phys_attrs; attnum++)
	{
		/* We don't need info for dropped attributes */
		if (attr[attnum - 1]->attisdropped)
			continue;

		/* Fetch the input function and typioparam info */
		if (cstate->binary)
			getTypeBinaryInputInfo(attr[attnum - 1]->atttypid,
								   &in_func_oid, &typioparams[attnum - 1]);
		else
			getTypeInputInfo(attr[attnum - 1]->atttypid,
							 &in_func_oid, &typioparams[attnum - 1]);
		fmgr_info(in_func_oid, &in_functions[attnum - 1]);

		/* Get default info if needed */
		if (!list_member_int(cstate->attnumlist, attnum))
		{
			/* attribute is NOT to be copied from input */
			/* use default value if one exists */
			Node	   *defexpr = build_column_default(cstate->rel, attnum);

			if (defexpr != NULL)
			{
				defexprs[num_defaults] = ExecPrepareExpr((Expr *) defexpr,
														 estate);
				defmap[num_defaults] = attnum - 1;
				num_defaults++;
			}
		}
	}

	/* prepare distribuion data for computing target seg*/
	if (is_check_distkey)
	{
		distData = InitDistributionData(cstate, attr, num_phys_attrs, estate, false);
	}

	/* Prepare to catch AFTER triggers. */
	AfterTriggerBeginQuery();

	/*
	 * Check BEFORE STATEMENT insertion triggers. It's debateable whether we
	 * should do this for COPY, since it's not really an "INSERT" statement as
	 * such. However, executing these triggers maintains consistency with the
	 * EACH ROW triggers that we already fire on COPY.
	 */
	ExecBSInsertTriggers(estate, resultRelInfo);

	/* 
	 * Skip header processing if dummy file get from master for COPY FROM ON
	 * SEGMENT
	 */
	if (!cstate->on_segment || Gp_role != GP_ROLE_EXECUTE)
	{
		CopyFromProcessDataFileHeader(cstate, cdbCopy, &file_has_oids);
	}

	attr_offsets = (int *) palloc(num_phys_attrs * sizeof(int));

	partValues = (Datum *) palloc(attr_count * sizeof(Datum));
	partNulls = (bool *) palloc(attr_count * sizeof(bool));

	bistate = GetBulkInsertState();

	/* Set up callback to identify error line number */
	errcontext.callback = copy_in_error_callback;
	errcontext.arg = (void *) cstate;
	errcontext.previous = error_context_stack;
	error_context_stack = &errcontext;

	if (Gp_role == GP_ROLE_EXECUTE && (cstate->on_segment == false))
		cstate->err_loc_type = ROWNUM_EMBEDDED; /* get original row num from QD COPY */
	else
		cstate->err_loc_type = ROWNUM_ORIGINAL; /* we can count rows by ourselves */

	CopyInitDataParser(cstate);

PROCESS_SEGMENT_DATA:
	do
	{
		size_t		bytesread = 0;

		if (!cstate->binary)
		{
			PG_TRY();
			{
				/* read a chunk of data into the buffer */
				bytesread = CopyGetData(cstate, cstate->raw_buf, RAW_BUF_SIZE);
			}
			PG_CATCH();
			{
				/*
				 * If we are here, we got some kind of communication error
				 * with the client or a bad protocol message. clean up and
				 * re-throw error. Note that we don't handle this error in
				 * any special way in SREH mode as it's not a data error.
				 */
				COPY_HANDLE_ERROR;
			}
			PG_END_TRY();

			cstate->raw_buf_done = false;

			/* set buffer pointers to beginning of the buffer */
			cstate->begloc = cstate->raw_buf;
			cstate->raw_buf_index = 0;
		}

		/*
		 * continue if some bytes were read or if we didn't reach EOF. if we
		 * both reached EOF _and_ no bytes were read, quit the loop we are
		 * done
		 */
		if (bytesread > 0 || !cstate->fe_eof)
		{
			/* handle HEADER, but only if COPY FROM ON SEGMENT */
			if (cstate->header_line && cstate->on_segment)
			{
				/* on first time around just throw the header line away */
				PG_TRY();
				{
					cstate->line_done = cstate->csv_mode ?
						CopyReadLineCSV(cstate, bytesread) :
						CopyReadLineText(cstate, bytesread);
				}
				PG_CATCH();
				{
					/*
					 * got here? encoding conversion error occured on the
					 * header line (first row).
					 */
					if (cstate->errMode == ALL_OR_NOTHING)
					{
						/* re-throw error and abort */
						COPY_HANDLE_ERROR;
					}
					else
					{
						/* SREH - release error state */
						if (!elog_dismiss(DEBUG5))
							PG_RE_THROW(); /* hope to never get here! */

						/*
						 * note: we don't bother doing anything special here.
						 * we are never interested in logging a header line
						 * error. just continue the workflow.
						 */
					}
				}
				PG_END_TRY();

				cstate->cur_lineno++;
				RESET_LINEBUF;

				cstate->header_line = false;
			}

			while (!cstate->raw_buf_done)
			{
				bool		skip_tuple;
				Oid			loaded_oid = InvalidOid;
				char		relstorage;
				Datum	   *baseValues;
				bool	   *baseNulls;

				CHECK_FOR_INTERRUPTS();

				/* Reset the per-tuple exprcontext */
				ResetPerTupleExprContext(estate);

				/* Switch into its memory context */
				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

				/* Initialize all values for row to NULL */
				ExecClearTuple(baseSlot);
				baseValues = slot_get_values(baseSlot);
				baseNulls = slot_get_isnull(baseSlot);

				MemSet(baseValues, 0, num_phys_attrs * sizeof(Datum));
				MemSet(baseNulls, true, num_phys_attrs * sizeof(bool));
				/* reset attribute pointers */
				MemSet(attr_offsets, 0, num_phys_attrs * sizeof(int));

				if (!cstate->binary)
				{
				PG_TRY();
				{
					/* Actually read the line into memory here */
					cstate->line_done = cstate->csv_mode ?
					CopyReadLineCSV(cstate, bytesread) :
					CopyReadLineText(cstate, bytesread);
				}
				PG_CATCH();
				{
					/* got here? encoding conversion/check error occurred */
					COPY_HANDLE_ERROR;
				}
				PG_END_TRY();

				if(cur_row_rejected)
				{
					ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
					QE_GOTO_NEXT_ROW;
				}

				if(!cstate->line_done)
				{
					/*
					 * if eof reached, and no data in line_buf,
					 * we don't need to do att parsing.
					 */
					if (cstate->fe_eof && cstate->line_buf.len == 0)
					{
						break;
					}
					/*
					 * We did not finish reading a complete date line
					 *
					 * If eof is not yet reached, we skip att parsing
					 * and read more data. But if eof _was_ reached it means
					 * that the original last data line is defective and
					 * we want to catch that error later on.
					 */
					if (!cstate->fe_eof || cstate->end_marker)
						break;
				}

				if (file_has_oids)
				{
					char	   *oid_string;

					/* can't be in CSV mode here */
					oid_string = CopyReadOidAttr(cstate, &isnull);

					if (isnull)
					{
						/* got here? null in OID column error */

						if(cstate->errMode == ALL_OR_NOTHING)
						{
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("null OID in COPY data.")));
						}
						else
						{
							/* SREH */
							cstate->cdbsreh->rejectcount++;
							cur_row_rejected = true;
						}

					}
					else
					{
						PG_TRY();
						{
							cstate->cur_attname = "oid";
							loaded_oid = DatumGetObjectId(DirectFunctionCall1(oidin,
																			  CStringGetDatum(oid_string)));
						}
						PG_CATCH();
						{
							/* got here? oid column conversion failed */
							COPY_HANDLE_ERROR;
						}
						PG_END_TRY();

						if (loaded_oid == InvalidOid)
						{
							if(cstate->errMode == ALL_OR_NOTHING)
								ereport(ERROR,
										(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
										 errmsg("invalid OID in COPY data.")));
							else /* SREH */
							{
								cstate->cdbsreh->rejectcount++;
								cur_row_rejected = true;
							}
						}
						cstate->cur_attname = NULL;
					}

					if(cur_row_rejected)
					{
						ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
						QE_GOTO_NEXT_ROW;
					}

				}

				PG_TRY();
				{
					if (cstate->csv_mode)
						CopyReadAttributesCSV(cstate, baseNulls, attr_offsets, num_phys_attrs, attr);
					else
						CopyReadAttributesText(cstate, baseNulls, attr_offsets, num_phys_attrs, attr);

					/*
					 * Loop to read the user attributes on the line.
					 */
					foreach(cur, cstate->attnumlist)
					{
						int			attnum = lfirst_int(cur);
						int			m = attnum - 1;
						char	   *string;

						string = cstate->attribute_buf.data + attr_offsets[m];

						if (baseNulls[m])
							isnull = true;
						else
							isnull = false;

						if (cstate->csv_mode && isnull && cstate->force_notnull_flags[m])
						{
							string = cstate->null_print;		/* set to NULL string */
							isnull = false;
						}

						cstate->cur_attname = NameStr(attr[m]->attname);

						baseValues[m] = InputFunctionCall(&in_functions[m],
													  isnull ? NULL : string,
													  typioparams[m],
													  attr[m]->atttypmod);
						baseNulls[m] = isnull;
						cstate->cur_attname = NULL;
					}
				}
				PG_CATCH();
				{
					COPY_HANDLE_ERROR; /* SREH */
				}
				PG_END_TRY();

				if (cur_row_rejected)
				{
					ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
					QE_GOTO_NEXT_ROW;
				}
				}
				else
				{
					/* binary */
					if (cstate->err_loc_type == ROWNUM_EMBEDDED)
					{
						/**
						* Incoming data format:
						*     <original_line_num:uint64><data for this row:bytes>
						* We consume "original_line_num" before parsing the data.
						* See also CopyExtractRowMetaData(cstate) for text/csv formats.
						*/
						int64 line_num;
						if (!CopyGetInt64(cstate, &line_num))
						{
							no_more_data = true;
							break;
						}
						cstate->cur_lineno = line_num;
					}

					int16		fld_count;
					ListCell   *cur;
					char buffer[20];

					if (!CopyGetInt16(cstate, &fld_count) ||
						fld_count == -1)
					{
						no_more_data = true;
						break;
					}

					if (fld_count != attr_count)
						ereport(ERROR,
								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								 errmsg("QD: line %s: row field count is %d, expected %d",
								 		linenumber_atoi(buffer, cstate->cur_lineno),
										(int) fld_count, attr_count)));

					if (file_has_oids)
					{
						cstate->cur_attname = "oid";
						loaded_oid =
							DatumGetObjectId(CopyReadBinaryAttribute(cstate,
																	 0,
																	 &oid_in_function,
																	 oid_typioparam,
																	 -1,
																	 &isnull,
																	 false));
						if (isnull || loaded_oid == InvalidOid)
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("invalid OID in COPY data")));
						cstate->cur_attname = NULL;
					}

					i = 0;
					foreach(cur, cstate->attnumlist)
					{
						int			attnum = lfirst_int(cur);
						int			m = attnum - 1;

						cstate->cur_attname = NameStr(attr[m]->attname);
						i++;
						baseValues[m] = CopyReadBinaryAttribute(cstate,
															i,
															&in_functions[m],
															typioparams[m],
															attr[m]->atttypmod,
															&isnull,
															false);
						baseNulls[m] = isnull;
						cstate->cur_attname = NULL;
					}
				}

					/*
					 * Now compute and insert any defaults available for the columns
					 * not provided by the input data.	Anything not processed here or
					 * above will remain NULL.
					 */
					for (i = 0; i < num_defaults; i++)
					{
						baseValues[defmap[i]] = ExecEvalExpr(defexprs[i], econtext,
														 &isnull, NULL);

						if (!isnull)
							baseNulls[defmap[i]] = false;
					}

				/*
				 * We might create a ResultRelInfo which needs to persist
				 * the per tuple context.
				 */
				PG_TRY();
				{
					MemoryContextSwitchTo(estate->es_query_cxt);
					if (estate->es_result_partitions)
					{
						resultRelInfo = values_get_partition(baseValues, baseNulls,
															 tupDesc, estate);
						estate->es_result_relation_info = resultRelInfo;
						FreeBulkInsertState(bistate);
						bistate = GetBulkInsertState();
					}
				}
				PG_CATCH();
				{
					COPY_HANDLE_ERROR;
				}
				PG_END_TRY();

				if (cur_row_rejected)
				{
					MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
					ErrorIfRejectLimitReached(cstate->cdbsreh, cdbCopy);
					QE_GOTO_NEXT_ROW;
				}

				relstorage = RelinfoGetStorage(resultRelInfo);
				if (relstorage == RELSTORAGE_AOROWS &&
					resultRelInfo->ri_aoInsertDesc == NULL)
				{
					ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);
					resultRelInfo->ri_aoInsertDesc =
						appendonly_insert_init(resultRelInfo->ri_RelationDesc,
											   resultRelInfo->ri_aosegno, false);
				}
				else if (relstorage == RELSTORAGE_AOCOLS &&
						 resultRelInfo->ri_aocsInsertDesc == NULL)
				{
					ResultRelInfoSetSegno(resultRelInfo, cstate->ao_segnos);
                    resultRelInfo->ri_aocsInsertDesc =
                        aocs_insert_init(resultRelInfo->ri_RelationDesc,
                        				 resultRelInfo->ri_aosegno, false);
				}
				else if (relstorage == RELSTORAGE_EXTERNAL &&
						 resultRelInfo->ri_extInsertDesc == NULL)
				{
					resultRelInfo->ri_extInsertDesc =
						external_insert_init(resultRelInfo->ri_RelationDesc);
				}

				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

				ExecStoreVirtualTuple(baseSlot);

				/*
				 * And now we can form the input tuple.
				 *
				 * The resulting tuple is stored in 'slot'
				 */
				if (resultRelInfo->ri_partSlot != NULL)
				{
					AttrMap *map = resultRelInfo->ri_partInsertMap;
					Assert(map != NULL);

					slot = resultRelInfo->ri_partSlot;
					ExecClearTuple(slot);
					partValues = slot_get_values(resultRelInfo->ri_partSlot);
					partNulls = slot_get_isnull(resultRelInfo->ri_partSlot);
					MemSet(partValues, 0, attr_count * sizeof(Datum));
					MemSet(partNulls, true, attr_count * sizeof(bool));

					reconstructTupleValues(map, baseValues, baseNulls, (int) num_phys_attrs,
										   partValues, partNulls, (int) attr_count);
					ExecStoreVirtualTuple(slot);
				}
				else
				{
					slot = baseSlot;
				}

				if (is_check_distkey && distData->p_nattrs > 0)
				{
					target_seg = GetTargetSeg(distData, slot_get_values(slot), slot_get_isnull(slot));

					PG_TRY();
					{
						/* check distribution key if COPY FROM ON SEGMENT */
						if (GpIdentity.segindex != target_seg)
							ereport(ERROR,
									(errcode(ERRCODE_INTEGRITY_CONSTRAINT_VIOLATION),
									 errmsg("value of distribution key doesn't belong to segment with ID %d, it belongs to segment with ID %d",
											GpIdentity.segindex, target_seg)));
					}
					PG_CATCH();
					{
						COPY_HANDLE_ERROR;
					}
					PG_END_TRY();
				}

				/*
				 * Triggers and stuff need to be invoked in query context.
				 */
				MemoryContextSwitchTo(estate->es_query_cxt);

				/* Partitions don't support triggers yet */
				Assert(!(estate->es_result_partitions &&
						 resultRelInfo->ri_TrigDesc));

				skip_tuple = false;

				/* BEFORE ROW INSERT Triggers */
				if (resultRelInfo->ri_TrigDesc &&
					resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_INSERT] > 0)
				{
					HeapTuple	newtuple;
					HeapTuple	tuple;

					tuple = ExecFetchSlotHeapTuple(slot);

					Assert(resultRelInfo->ri_TrigFunctions != NULL);
					newtuple = ExecBRInsertTriggers(estate, resultRelInfo, tuple);

					if (newtuple == NULL)		/* "do nothing" */
						skip_tuple = true;
					else if (newtuple != tuple) /* modified by Trigger(s) */
					{
						ExecStoreHeapTuple(newtuple, slot, InvalidBuffer, true);
					}
				}

				if (!skip_tuple)
				{
					char relstorage = RelinfoGetStorage(resultRelInfo);
					ItemPointerData insertedTid;

					/*
					 * Check the constraints of the tuple
					 */
					if (resultRelInfo->ri_RelationDesc->rd_att->constr)
						ExecConstraints(resultRelInfo, slot, estate);

					/*
					 * OK, store the tuple and create index entries for it
					 */
					if (relstorage == RELSTORAGE_AOROWS)
					{
						MemTuple	mtuple;

						mtuple = ExecFetchSlotMemTuple(slot, false);

						/* inserting into an append only relation */
						appendonly_insert(resultRelInfo->ri_aoInsertDesc, mtuple, InvalidOid, (AOTupleId *) &insertedTid);
					}
					else if (relstorage == RELSTORAGE_AOCOLS)
					{
                        aocs_insert(resultRelInfo->ri_aocsInsertDesc, slot);
						insertedTid = *slot_get_ctid(slot);
					}
					else if (relstorage == RELSTORAGE_EXTERNAL)
					{
						HeapTuple tuple;

						tuple = ExecFetchSlotHeapTuple(slot);
						external_insert(resultRelInfo->ri_extInsertDesc, tuple);
						ItemPointerSetInvalid(&insertedTid);
					}
					else
					{
						HeapTuple tuple;

						tuple = ExecFetchSlotHeapTuple(slot);
						heap_insert(resultRelInfo->ri_RelationDesc, tuple, mycid, hi_options, bistate,
									GetCurrentTransactionId());
						insertedTid = tuple->t_self;
					}

					if (resultRelInfo->ri_NumIndices > 0)
						ExecInsertIndexTuples(slot, &insertedTid, estate, false);

					/* AFTER ROW INSERT Triggers */
					if (resultRelInfo->ri_TrigDesc &&
						resultRelInfo->ri_TrigDesc->n_after_row[TRIGGER_EVENT_INSERT] > 0)
					{
						HeapTuple tuple;

						tuple = ExecFetchSlotHeapTuple(slot);
						ExecARInsertTriggers(estate, resultRelInfo, tuple);
					}

					/*
					 * We count only tuples not suppressed by a BEFORE INSERT trigger;
					 * this is the same definition used by execMain.c for counting
					 * tuples inserted by an INSERT command.
					 *
					 * MPP: incrementing this counter here only matters for utility
					 * mode. in dispatch mode only the dispatcher COPY collects row
					 * count, so this counter is meaningless.
					 */
					cstate->processed++;
					if (relstorage_is_ao(relstorage))
						resultRelInfo->ri_aoprocessed++;
				}

				RESET_LINEBUF;
			}					/* end while(!raw_buf_done) */
		}						/* end if (bytesread > 0 || !cstate->fe_eof) */
		else
			/* no bytes read, end of data */
		{
			no_more_data = true;
		}
	} while (!no_more_data);

	/*
	 * After processed data from QD, which is empty and just for workflow, now
	 * to process the data on segment, only one shot if cstate->on_segment &&
	 * Gp_role == GP_ROLE_DISPATCH
	 */
	if (!is_segment_data_processed)
	{
		if (cstate->is_program)
		{
			cstate->program_pipes = open_program_pipes(cstate->filename, false);
			cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_R);

			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errmsg("could not execute command \"%s\": %m",
								cstate->filename)));
		}
		else
		{
			struct stat st;
			char *filename = cstate->filename;
			cstate->copy_file = AllocateFile(filename, PG_BINARY_R);

			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
						errmsg("could not open file \"%s\" for reading: %m",
								filename)));

			// Increase buffer size to improve performance  (cmcdevitt)
			setvbuf(cstate->copy_file, NULL, _IOFBF, 393216); // 384 Kbytes

			fstat(fileno(cstate->copy_file), &st);
			if (S_ISDIR(st.st_mode))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						errmsg("\"%s\" is a directory", filename)));
		}

		cstate->copy_dest = COPY_FILE;

		is_segment_data_processed = true;

		CopyFromProcessDataFileHeader(cstate, cdbCopy, &file_has_oids);
		CopyInitDataParser(cstate);
		no_more_data = false;

		goto PROCESS_SEGMENT_DATA;
	}

	elog(DEBUG1, "Segment %u, Copied %lu rows.", GpIdentity.segindex, cstate->processed);

	/* Done, clean up */
	if (cstate->on_segment && cstate->is_program)
	{
		close_program_pipes(cstate, true);
	}
	else if (cstate->on_segment && FreeFile(cstate->copy_file))
	{
			ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m",
						cstate->filename)));
	}

	error_context_stack = errcontext.previous;

	FreeBulkInsertState(bistate);

	MemoryContextSwitchTo(estate->es_query_cxt);

	/* Execute AFTER STATEMENT insertion triggers */
	ExecASInsertTriggers(estate, resultRelInfo);

	/* Handle queued AFTER triggers */
	AfterTriggerEndQuery(estate);

	/*
	 * If SREH and in executor mode send the number of rejected
	 * rows to the client (QD COPY).
	 * If COPY ... FROM ... ON SEGMENT, then need to send the number of completed
	 */
	if ((cstate->errMode != ALL_OR_NOTHING && Gp_role == GP_ROLE_EXECUTE)
		|| cstate->on_segment)
		SendNumRows((cstate->errMode != ALL_OR_NOTHING) ? cstate->cdbsreh->rejectcount : 0,
				cstate->on_segment ? cstate->processed : 0);

	if (estate->es_result_partitions && Gp_role == GP_ROLE_EXECUTE)
		SendAOTupCounts(estate);

	/* NB: do not pfree baseValues/baseNulls and partValues/partNulls here, since
	 * there may be duplicate free in ExecDropSingleTupleTableSlot; if not, they
	 * would be freed by FreeExecutorState anyhow */

	ExecDropSingleTupleTableSlot(baseSlot);

	/*
	 * If we skipped writing WAL, then we need to sync the heap (but not
	 * indexes since those use WAL anyway)
	 */
	if (hi_options & HEAP_INSERT_SKIP_WAL)
		heap_sync(cstate->rel);

	/*
	 * Finalize appends and close relations we opened.
	 */
	resultRelInfo = estate->es_result_relations;
	for (i = estate->es_num_result_relations; i > 0; i--)
	{
			if (resultRelInfo->ri_aoInsertDesc)
					appendonly_insert_finish(resultRelInfo->ri_aoInsertDesc);

			if (resultRelInfo->ri_aocsInsertDesc)
					aocs_insert_finish(resultRelInfo->ri_aocsInsertDesc);

			if (resultRelInfo->ri_extInsertDesc)
					external_insert_finish(resultRelInfo->ri_extInsertDesc);
			
			/* Close indices and then the relation itself */
			ExecCloseIndices(resultRelInfo);
			heap_close(resultRelInfo->ri_RelationDesc, NoLock);
			resultRelInfo++;
	}
	
	cstate->rel = NULL; /* closed above */

	MemoryContextSwitchTo(oldcontext);

	/* free distribution data after switching oldcontext */
	FreeDistributionData(distData);

	FreeExecutorState(estate);
}

/*
 * Finds the next TEXT line that is in the input buffer and loads
 * it into line_buf. Returns an indication if the line that was read
 * is complete (if an unescaped line-end was encountered). If we
 * reached the end of buffer before the whole line was written into the
 * line buffer then returns false.
 */
bool
CopyReadLineText(CopyState cstate, size_t bytesread)
{
	int			linesize;
	char		escapec = '\0';

	/* mark that encoding conversion hasn't occurred yet */
	cstate->line_buf_converted = false;

	/*
	 * set the escape char for text format ('\\' by default).
	 */
	escapec = cstate->escape[0];

	if (cstate->raw_buf_index >= bytesread)
	{
		cstate->raw_buf_done = true;
		cstate->line_done = CopyCheckIsLastLine(cstate);
		return false;
	}

	/*
	 * Detect end of line type if not already detected.
	 */
	if (cstate->eol_type == EOL_UNKNOWN)
	{
		cstate->quote = NULL;

		if (!DetectLineEnd(cstate, bytesread))
		{
			/* load entire input buffer into line buf, and quit */
			appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf, bytesread);
			cstate->raw_buf_done = true;
			cstate->line_done = CopyCheckIsLastLine(cstate);

			if (cstate->line_done)
				preProcessDataLine(cstate);

			return cstate->line_done;
		}
	}

	/*
	 * Special case: eol is CRNL, last byte of previous buffer was an
	 * unescaped CR and 1st byte of current buffer is NL. We check for
	 * that here.
	 */
	if (cstate->eol_type == EOL_CRLF)
	{
		/* if we started scanning from the 1st byte of the buffer */
		if (cstate->begloc == cstate->raw_buf)
		{
			/* and had a CR in last byte of prev buf */
			if (cstate->cr_in_prevbuf)
			{
				/*
				 * if this 1st byte in buffer is 2nd byte of line end sequence
				 * (linefeed)
				 */
				if (*(cstate->begloc) == cstate->eol_ch[1])
				{
					/*
					* load that one linefeed byte and indicate we are done
					* with the data line
					*/
					appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, 1);
					cstate->raw_buf_index++;
					cstate->begloc++;
					cstate->cr_in_prevbuf = false;
					preProcessDataLine(cstate);

					if (cstate->raw_buf_index >= bytesread)
					{
						cstate->raw_buf_done = true;
					}
					return true;
				}
			}

			cstate->cr_in_prevbuf = false;
		}
	}

	/*
	 * (we need a loop so that if eol_ch is found, but prev ch is backslash,
	 * we can search for the next eol_ch)
	 */
	while (true)
	{
		/* reached end of buffer */
		if ((cstate->endloc = scanTextLine(cstate, cstate->begloc, cstate->eol_ch[0], bytesread - cstate->raw_buf_index)) == NULL)
		{
			linesize = bytesread - (cstate->begloc - cstate->raw_buf);
			appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, linesize);

			if (cstate->eol_type == EOL_CRLF && cstate->line_buf.len > 1)
			{
				char	   *last_ch = cstate->line_buf.data + cstate->line_buf.len - 1; /* before terminating \0 */

				if (*last_ch == '\r')
					cstate->cr_in_prevbuf = true;
			}

			cstate->line_done = CopyCheckIsLastLine(cstate);
			cstate->raw_buf_done = true;

			break;
		}
		else
			/* found the 1st eol ch in raw_buf. */
		{
			bool		eol_found = true;

			/*
			 * Load that piece of data (potentially a data line) into the line buffer,
			 * and update the pointers for the next scan.
			 */
			linesize = cstate->endloc - cstate->begloc + 1;
			appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, linesize);
			cstate->raw_buf_index += linesize;
			cstate->begloc = cstate->endloc + 1;

			if (cstate->eol_type == EOL_CRLF)
			{
				/* check if there is a '\n' after the '\r' */
				if (cstate->raw_buf_index < bytesread && *(cstate->endloc + 1) == '\n')
				{
					/* this is a line end */
					appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, 1);		/* load that '\n' */
					cstate->raw_buf_index++;
					cstate->begloc++;
				}
				/* no data left, may in next buf*/
				else if (cstate->raw_buf_index >= bytesread)
				{
					cstate->cr_in_prevbuf = true;
					eol_found = false;
				}
				else
				{
					/* just a CR, not a line end */
					eol_found = false;
				}
			}

			/*
			 * in some cases, this end of line char happens to be the
			 * last character in the buffer. we need to catch that.
			 */
			if (cstate->raw_buf_index >= bytesread)
				cstate->raw_buf_done = true;

			/*
			 * if eol was found, and it isn't escaped, line is done
			 */
			if (eol_found)
			{
				cstate->line_done = true;
				break;
			}
			else
			{
				/* stay in the loop and process some more data. */
				cstate->line_done = false;

				/* no data left, retuen false */
				if (cstate->raw_buf_done)
				{
					return false;
				}

				if (eol_found)
					cstate->cur_lineno++;		/* increase line index for error
												 * reporting */
			}

		}						/* end of found eol_ch */
	}

	/* Done reading a complete line. Do pre processing of the raw input data */
	if (cstate->line_done)
		preProcessDataLine(cstate);

	/*
	 * check if this line is an end marker -- "\."
	 */
	cstate->end_marker = false;

	switch (cstate->eol_type)
	{
		case EOL_LF:
			if (!strcmp(cstate->line_buf.data, "\\.\n"))
				cstate->end_marker = true;
			break;
		case EOL_CR:
			if (!strcmp(cstate->line_buf.data, "\\.\r"))
				cstate->end_marker = true;
			break;
		case EOL_CRLF:
			if (!strcmp(cstate->line_buf.data, "\\.\r\n"))
				cstate->end_marker = true;
			break;
		case EOL_UNKNOWN:
			break;
	}

	if (cstate->end_marker)
	{
		/*
		 * Reached end marker. In protocol version 3 we
		 * should ignore anything after \. up to protocol
		 * end of copy data.
		 */
		if (cstate->copy_dest == COPY_NEW_FE)
		{
			while (!cstate->fe_eof)
			{
				CopyGetData(cstate, cstate->raw_buf, RAW_BUF_SIZE);	/* eat data */
			}
		}

		cstate->fe_eof = true;
		/* we don't want to process a \. as data line, want to quit. */
		cstate->line_done = false;
		cstate->raw_buf_done = true;
	}

	return cstate->line_done;
}

/*
 * Finds the next CSV line that is in the input buffer and loads
 * it into line_buf. Returns an indication if the line that was read
 * is complete (if an unescaped line-end was encountered). If we
 * reached the end of buffer before the whole line was written into the
 * line buffer then returns false.
 */
bool
CopyReadLineCSV(CopyState cstate, size_t bytesread)
{
	int			linesize;
	char		quotec = '\0',
				escapec = '\0';
	bool		csv_is_invalid = false;

	/* mark that encoding conversion hasn't occurred yet */
	cstate->line_buf_converted = false;

	escapec = cstate->escape[0];
	quotec = cstate->quote[0];

	/* ignore special escape processing if it's the same as quotec */
	if (quotec == escapec)
		escapec = '\0';

	if (cstate->raw_buf_index >= bytesread)
	{
		cstate->raw_buf_done = true;
		cstate->line_done = CopyCheckIsLastLine(cstate);
		return false;
	}

	/*
	 * Detect end of line type if not already detected.
	 */
	if (cstate->eol_type == EOL_UNKNOWN)
	{
		if (!DetectLineEnd(cstate, bytesread))
		{
			/* EOL not found. load entire input buffer into line buf, and return */
			appendBinaryStringInfo(&cstate->line_buf, cstate->raw_buf, bytesread);
			cstate->line_done = CopyCheckIsLastLine(cstate);;
			cstate->raw_buf_done = true;

			if (cstate->line_done)
				preProcessDataLine(cstate);

			return cstate->line_done;
		}
	}

	/*
	 * Special case: eol is CRNL, last byte of previous buffer was an
	 * unescaped CR and 1st byte of current buffer is NL. We check for
	 * that here.
	 */
	if (cstate->eol_type == EOL_CRLF)
	{
		/* if we started scanning from the 1st byte of the buffer */
		if (cstate->begloc == cstate->raw_buf)
		{
			/* and had a CR in last byte of prev buf */
			if (cstate->cr_in_prevbuf)
			{
				/*
				 * if this 1st byte in buffer is 2nd byte of line end sequence
				 * (linefeed)
				 */
				if (*(cstate->begloc) == cstate->eol_ch[1])
				{
					/*
					 * load that one linefeed byte and indicate we are done
					 * with the data line
					 */
					appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, 1);
					cstate->raw_buf_index++;
					cstate->begloc++;
					cstate->line_done = true;
					preProcessDataLine(cstate);
					cstate->cr_in_prevbuf = false;

					if (cstate->raw_buf_index >= bytesread)
					{
						cstate->raw_buf_done = true;
					}
					return true;
				}
			}

			cstate->cr_in_prevbuf = false;
		}
	}

	/*
	 * (we need a loop so that if eol_ch is found, but we are in quotes,
	 * we can search for the next eol_ch)
	 */
	while (true)
	{
		/* reached end of buffer */
		if ((cstate->endloc = scanCSVLine(cstate, cstate->begloc, cstate->eol_ch[0], escapec, quotec, bytesread - cstate->raw_buf_index)) == NULL)
		{
			linesize = bytesread - (cstate->begloc - cstate->raw_buf);
			appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, linesize);

			if (cstate->line_buf.len > 1)
			{
				char	   *last_ch = cstate->line_buf.data + cstate->line_buf.len - 1; /* before terminating \0 */

				if (*last_ch == '\r')
				{
					if (cstate->eol_type == EOL_CRLF)
						cstate->cr_in_prevbuf = true;
				}
			}

			cstate->line_done = CopyCheckIsLastLine(cstate);
			cstate->raw_buf_done = true;
			break;
		}
		else
			/* found 1st eol char in raw_buf. */
		{
			bool		eol_found = true;

			/*
			 * Load that piece of data (potentially a data line) into the line buffer,
			 * and update the pointers for the next scan.
			 */
			linesize = cstate->endloc - cstate->begloc + 1;
			appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, linesize);
			cstate->raw_buf_index += linesize;
			cstate->begloc = cstate->endloc + 1;

			/* end of line only if not in quotes */
			if (cstate->in_quote)
			{
				/* buf done, but still in quote */
				if (cstate->raw_buf_index >= bytesread)
					cstate->raw_buf_done = true;

				cstate->line_done = false;

				/* update file line for error message */

				/*
				 * TODO: for dos line end we need to do check before
				 * incrementing!
				 */
				cstate->cur_lineno++;

				/*
				 * If we are still in quotes and linebuf len is extremely large
				 * then this file has bad csv and we have to stop the rolling
				 * snowball from getting bigger.
				 */
				if(cstate->line_buf.len >= gp_max_csv_line_length)
				{
					csv_is_invalid = true;
					cstate->in_quote = false;
					cstate->line_done = true;
					cstate->num_consec_csv_err++;
					break;
				}

				if (cstate->raw_buf_done)
					break;
			}
			else
			{
				/* if dos eol, check for '\n' after the '\r' */
				if (cstate->eol_type == EOL_CRLF)
				{
					if (cstate->raw_buf_index < bytesread && *(cstate->endloc + 1) == '\n')
					{
						/* this is a line end */
						appendBinaryStringInfo(&cstate->line_buf, cstate->begloc, 1);	/* load that '\n' */
						cstate->raw_buf_index++;
						cstate->begloc++;
					}
					else if (cstate->raw_buf_index >= bytesread)
					{
						cstate->cr_in_prevbuf = true;
						eol_found = false;
					}
					else
					{
						/* just a CR, not a line end */
						eol_found = false;
					}
				}

				/*
				 * in some cases, this end of line char happens to be the
				 * last character in the buffer. we need to catch that.
				 */
				if (cstate->raw_buf_index >= bytesread)
					cstate->raw_buf_done = true;

				/*
				 * if eol was found line is done
				 */
				if (eol_found)
				{
					cstate->line_done = true;
					break;
				}
				else
				{
					cstate->line_done = false;
					/* no data left, return false */
					if (cstate->raw_buf_done)
					{
						return false;
					}
				}
			}
		}						/* end of found eol_ch */
	}


	/* Done reading a complete line. Do pre processing of the raw input data */
	if (cstate->line_done)
		preProcessDataLine(cstate);

	/*
	 * We have a corrupted csv format case. It is already converted to server
	 * encoding, *which is necessary*. Ok, we can report an error now.
	 */
	if(csv_is_invalid)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("data line too long. likely due to invalid csv data")));
	else
		cstate->num_consec_csv_err = 0; /* reset consecutive count */

	/*
	 * check if this line is an end marker -- "\."
	 */
	cstate->end_marker = false;

	switch (cstate->eol_type)
	{
		case EOL_LF:
			if (!strcmp(cstate->line_buf.data, "\\.\n"))
				cstate->end_marker = true;
			break;
		case EOL_CR:
			if (!strcmp(cstate->line_buf.data, "\\.\r"))
				cstate->end_marker = true;
			break;
		case EOL_CRLF:
			if (!strcmp(cstate->line_buf.data, "\\.\r\n"))
				cstate->end_marker = true;
			break;
		case EOL_UNKNOWN:
			break;
	}

	if (cstate->end_marker)
	{
		/*
		 * Reached end marker. In protocol version 3 we
		 * should ignore anything after \. up to protocol
		 * end of copy data.
		 */
		if (cstate->copy_dest == COPY_NEW_FE)
		{
			while (!cstate->fe_eof)
			{
				CopyGetData(cstate, cstate->raw_buf, RAW_BUF_SIZE);	/* eat data */
			}
		}

		cstate->fe_eof = true;
		/* we don't want to process a \. as data line, want to quit. */
		cstate->line_done = false;
		cstate->raw_buf_done = true;
	}

	return cstate->line_done;
}

/*
 * Detected the eol type by looking at the first data row.
 * Possible eol types are NL, CR, or CRNL. If eol type was
 * detected, it is set and a boolean true is returned to
 * indicated detection was successful. If the first data row
 * is longer than the input buffer, we return false and will
 * try again in the next buffer.
 */
static bool
DetectLineEnd(CopyState cstate, size_t bytesread  __attribute__((unused)))
{
	int			index = 0;
	int			lineno = 0;
	char		c;
	char		quotec = '\0',
				escapec = '\0';
	bool		csv = false;
	
	/*
	 * CSV special case. See MPP-7819.
	 * 
	 * this functions may change the in_quote value while processing.
	 * this is ok as we need to keep state in case we don't find EOL
	 * in this buffer and need to be called again to continue searching.
	 * BUT if EOL *was* found we must reset to the state we had since 
	 * we are about to reprocess this buffer again in CopyReadLineCSV
	 * from the same starting point as we are in right now. 
	 */
	bool save_inquote = cstate->in_quote;
	bool save_lastwas = cstate->last_was_esc;

	/* if user specified NEWLINE we should never be here */
	Assert(!cstate->eol_str);

	if (cstate->quote)					/* CSV format */
	{
		csv = true;
		quotec = cstate->quote[0];
		escapec = cstate->escape[0];
		/* ignore special escape processing if it's the same as quotec */
		if (quotec == escapec)
			escapec = '\0';
	}

	while (index < RAW_BUF_SIZE)
	{
		c = cstate->raw_buf[index];

		if (csv)
		{
			if (cstate->in_quote && c == escapec)
				cstate->last_was_esc = !cstate->last_was_esc;
			if (c == quotec && !cstate->last_was_esc)
				cstate->in_quote = !cstate->in_quote;
			if (c != escapec)
				cstate->last_was_esc = false;
		}

		if (c == '\n')
		{
			lineno++;
			
			if (!csv || (csv && !cstate->in_quote))
			{
				cstate->eol_type = EOL_LF;
				cstate->eol_ch[0] = '\n';
				cstate->eol_ch[1] = '\0';

				cstate->in_quote = save_inquote; /* see comment at declaration */
				cstate->last_was_esc = save_lastwas;
				return true;
			}
			else if(csv && cstate->in_quote && cstate->line_buf.len + index >= gp_max_csv_line_length)
			{	
				/* we do a "line too long" CSV check for the first row as well (MPP-7869) */
				cstate->in_quote = false;
				cstate->line_done = true;
				cstate->num_consec_csv_err++;
				cstate->cur_lineno += lineno;
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								errmsg("data line too long. likely due to invalid csv data")));
			}

		}
		if (c == '\r')
		{
			lineno++;
			
			if (!csv || (csv && !cstate->in_quote))
			{
				if (cstate->raw_buf[index + 1] == '\n')		/* always safe */
				{
					cstate->eol_type = EOL_CRLF;
					cstate->eol_ch[0] = '\r';
					cstate->eol_ch[1] = '\n';
				}
				else
				{
					cstate->eol_type = EOL_CR;
					cstate->eol_ch[0] = '\r';
					cstate->eol_ch[1] = '\0';
				}

				cstate->in_quote = save_inquote; /* see comment at declaration */
				cstate->last_was_esc = save_lastwas;
				return true;
			}
		}

		index++;
	}

	/* since we're yet to find the EOL this buffer will never be 
	 * re-processed so add the number of rows we found so we don't lose it */
	cstate->cur_lineno += lineno;

	return false;
}

/*
 *	Return decimal value for a hexadecimal digit
 */
static int
GetDecimalFromHex(char hex)
{
	if (isdigit((unsigned char) hex))
		return hex - '0';
	else
		return tolower((unsigned char) hex) - 'a' + 10;
}

/*
 * Read all TEXT attributes. Attributes are parsed from line_buf and
 * inserted (all at once) to attribute_buf, while saving pointers to
 * each attribute's starting position.
 *
 * When this routine finishes execution both the nulls array and
 * the attr_offsets array are updated. The attr_offsets will include
 * the offset from the beginning of the attribute array of which
 * each attribute begins. If a specific attribute is not used for this
 * COPY command (ommitted from the column list), a value of 0 will be assigned.
 * For example: for table foo(a,b,c,d,e) and COPY foo(a,b,e)
 * attr_offsets may look something like this after this routine
 * returns: [0,20,0,0,55]. That means that column "a" value starts
 * at byte offset 0, "b" in 20 and "e" in 55, in attribute_buf.
 *
 * In the attribute buffer (attribute_buf) each attribute
 * is terminated with a '\0', and therefore by using the attr_offsets
 * array we could point to a beginning of an attribute and have it
 * behave as a C string, much like previously done in COPY.
 *
 * Another aspect to improving performance is reducing the frequency
 * of data load into buffers. The original COPY read attribute code
 * loaded a character at a time. In here we try to load a chunk of data
 * at a time. Usually a chunk will include a full data row
 * (unless we have an escaped delim). That effectively reduces the number of
 * loads by a factor of number of bytes per row. This improves performance
 * greatly, unfortunately it add more complexity to the code.
 *
 * Global participants in parsing logic:
 *
 * line_buf.cursor -- an offset from beginning of the line buffer
 * that indicates where we are about to begin the next scan. Note that
 * if we have WITH OIDS or if we ran CopyExtractRowMetaData this cursor is
 * already shifted and is not in the beginning of line buf anymore.
 *
 * attribute_buf.cursor -- an offset from the beginning of the
 * attribute buffer that indicates where the current attribute begins.
 */

void
CopyReadAttributesText(CopyState cstate, bool * __restrict nulls,
					   int * __restrict attr_offsets, int num_phys_attrs, Form_pg_attribute * __restrict attr)
{
	char		delimc = cstate->delim[0];		/* delimiter character */
	char		escapec = cstate->escape[0];	/* escape character    */
	char	   *scan_start;		/* pointer to line buffer for scan start. */
	char	   *scan_end;		/* pointer to line buffer where char was found */
	char	   *stop;
	char	   *scanner;
	int			attr_pre_len = 0;/* attr raw len, before processing escapes */
	int			attr_post_len = 0;/* current attr len after escaping */
	int			m;				/* attribute index being parsed */
	int			bytes_remaining;/* num bytes remaining to be scanned in line
								 * buf */
	int			chunk_start;	/* offset to beginning of line chunk to load */
	int			chunk_len = 0;	/* length of chunk of data to load to attr buf */
	int			oct_val;		/* byte value for octal escapes */
	int			hex_val;
	int			attnum = 0;		/* attribute number being parsed */
	int			attribute = 1;
	bool		saw_non_ascii = false;
	ListCell   *cur;			/* cursor to attribute list used for this COPY */

	/* init variables for attribute scan */
	RESET_ATTRBUF;

	/* cursor is now > 0 if we copy WITH OIDS */
	scan_start = cstate->line_buf.data + cstate->line_buf.cursor;
	chunk_start = cstate->line_buf.cursor;

	cur = list_head(cstate->attnumlist);

	/* check for zero column table case */
	if(num_phys_attrs > 0)
	{
		attnum = lfirst_int(cur);
		m = attnum - 1;
	}

	if (cstate->escape_off)
		escapec = delimc;		/* look only for delimiters, escapes are
								 * disabled */

	/* have a single column only and no delim specified? take the fast track */
	if (cstate->delimiter_off)
    {
		CopyReadAttributesTextNoDelim(cstate, nulls, num_phys_attrs,
											 attnum);
        return;
    }

	/*
	 * Scan through the line buffer to read all attributes data
	 */
	while (cstate->line_buf.cursor < cstate->line_buf.len)
	{
		bytes_remaining = cstate->line_buf.len - cstate->line_buf.cursor;
		stop = scan_start + bytes_remaining;
		/*
		 * We can eliminate one test (for length) in the loop by replacing the
		 * last byte with the delimiter.  We need to remember what it was so we
		 * can replace it later.
		 */
		char  endchar = *(stop-1);
		*(stop-1) = delimc;

		/* Find the next of: delimiter, or escape, or end of buffer */
		for (scanner = scan_start; *scanner != delimc && *scanner != escapec; scanner++)
			;
		if (scanner == (stop-1) && endchar != delimc)
		{
			if (endchar != escapec)
				scanner++;
		}
		*(stop-1) = endchar;

		scan_end = (*scanner != '\0' ? (char *) scanner : NULL);

		if (scan_end == NULL)
		{
			/* GOT TO END OF LINE BUFFER */

			if (cur == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("extra data after last expected column")));

			attnum = lfirst_int(cur);
			m = attnum - 1;

			/* don't count eol char(s) in attr and chunk len calculation */
			if (cstate->eol_type == EOL_CRLF)
			{
				attr_pre_len += bytes_remaining - 2;
				chunk_len = cstate->line_buf.len - chunk_start - 2;
			}
			else
			{
				attr_pre_len += bytes_remaining - 1;
				chunk_len = cstate->line_buf.len - chunk_start - 1;
			}

			/* check if this is a NULL value or data value (assumed NULL) */
			if (attr_pre_len == cstate->null_print_len
				&&
				strncmp(cstate->line_buf.data + cstate->line_buf.len
					- attr_pre_len - (cstate->eol_type == EOL_CRLF ? 2 : 1),
					cstate->null_print, attr_pre_len) == 0)
				nulls[m] = true;
			else
				nulls[m] = false;

			attr_offsets[m] = cstate->attribute_buf.cursor;


			/* load the last chunk, the whole buffer in most cases */
			appendBinaryStringInfo(&cstate->attribute_buf, cstate->line_buf.data + chunk_start, chunk_len);

			cstate->line_buf.cursor += attr_pre_len + 2;		/* skip eol char and
														 * '\0' to exit loop */

			/*
			 * line is done, but do we have more attributes to process?
			 *
			 * normally, remaining attributes that have no data means ERROR,
			 * however, with FILL MISSING FIELDS remaining attributes become
			 * NULL. since attrs are null by default we leave unchanged and
			 * avoid throwing an error, with the exception of empty data lines
			 * for multiple attributes, which we intentionally don't support.
			 */
			if (lnext(cur) != NULL)
			{
				if (!cstate->fill_missing)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("missing data for column \"%s\"",
									 NameStr(attr[lfirst_int(lnext(cur)) - 1]->attname))));

				else if (attribute == 1 && attr_pre_len == 0)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("missing data for column \"%s\", found empty data line",
									 NameStr(attr[lfirst_int(lnext(cur)) - 1]->attname))));
			}
		}
		else
			/* FOUND A DELIMITER OR ESCAPE */
		{
			if (cur == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("extra data after last expected column")));

			if (*scan_end == delimc)	/* found a delimiter */
			{
				attnum = lfirst_int(cur);
				m = attnum - 1;

				/* (we don't include the delimiter ch in length) */
				attr_pre_len += scan_end - scan_start;
				attr_post_len += scan_end - scan_start;

				/* check if this is a null print or data (assumed NULL) */
				if (attr_pre_len == cstate->null_print_len &&
					strncmp(scan_end - attr_pre_len, cstate->null_print, attr_pre_len) == 0)
					nulls[m] = true;
				else
					nulls[m] = false;

				/* set the pointer to next attribute position */
				attr_offsets[m] = cstate->attribute_buf.cursor;

				/*
				 * update buffer cursors to our current location, +1 to skip
				 * the delimc
				 */
				cstate->line_buf.cursor = scan_end - cstate->line_buf.data + 1;
				cstate->attribute_buf.cursor += attr_post_len + 1;

				/* prepare scan for next attr */
				scan_start = cstate->line_buf.data + cstate->line_buf.cursor;
				cur = lnext(cur);
				attr_pre_len = 0;
				attr_post_len = 0;

				/*
				 * for the dispatcher - stop parsing once we have
				 * all the hash field values. We don't need the rest.
				 */
				if (Gp_role == GP_ROLE_DISPATCH)
				{
					if (attribute == cstate->last_hash_field)
					{
						/*
						 * load the chunk from chunk_start to end of current
						 * attribute, not including delimiter
						 */
						chunk_len = cstate->line_buf.cursor - chunk_start - 1;
						appendBinaryStringInfo(&cstate->attribute_buf, cstate->line_buf.data + chunk_start, chunk_len);
						break;
					}
				}

				attribute++;
			}
			else
				/* found an escape character */
			{
				char		nextc = *(scan_end + 1);
				char		newc;
				int			skip = 2;

				chunk_len = (scan_end - cstate->line_buf.data) - chunk_start + 1;

				/* load a chunk of data */
				appendBinaryStringInfo(&cstate->attribute_buf, cstate->line_buf.data + chunk_start, chunk_len);

				switch (nextc)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						/* handle \013 */
						oct_val = OCTVALUE(nextc);
						nextc = *(scan_end + 2);

						/*
						 * (no need for out bad access check since line if
						 * buffered)
						 */
						if (ISOCTAL(nextc))
						{
							skip++;
							oct_val = (oct_val << 3) + OCTVALUE(nextc);
							nextc = *(scan_end + 3);
							if (ISOCTAL(nextc))
							{
								skip++;
								oct_val = (oct_val << 3) + OCTVALUE(nextc);
							}
						}
						newc = oct_val & 0377;	/* the escaped byte value */
						if (newc == '\0' || IS_HIGHBIT_SET(newc))
							saw_non_ascii = true;
						break;
					case 'x':
						/* Handle \x3F */
						hex_val = 0; /* init */
						nextc = *(scan_end + 2); /* get char after 'x' */

						if (isxdigit((unsigned char)nextc))
						{
							skip++;
							hex_val = GetDecimalFromHex(nextc);
							nextc = *(scan_end + 3); /* get second char */

							if (isxdigit((unsigned char)nextc))
							{
								skip++;
								hex_val = (hex_val << 4) + GetDecimalFromHex(nextc);
							}
							newc = hex_val & 0xff;
							if (newc == '\0' || IS_HIGHBIT_SET(newc))
								saw_non_ascii = true;
						}
						else
						{
							newc = 'x';
						}
						break;

					case 'b':
						newc = '\b';
						break;
					case 'f':
						newc = '\f';
						break;
					case 'n':
						newc = '\n';
						break;
					case 'r':
						newc = '\r';
						break;
					case 't':
						newc = '\t';
						break;
					case 'v':
						newc = '\v';
						break;
					default:
						if (nextc == delimc)
							newc = delimc;
						else if (nextc == escapec)
							newc = escapec;
						else
						{
							/* no escape sequence found. it's a lone escape */
							
							bool next_is_eol = ((nextc == '\n' && cstate->eol_type == EOL_LF) ||
											    (nextc == '\r' && (cstate->eol_type == EOL_CR || 
																   cstate->eol_type == EOL_CRLF)));
							
							if(!next_is_eol)
							{
								/* take next char literally */
								newc = nextc;
							}
							else
							{
								/* there isn't a next char (end of data in line). we keep the 
								 * backslash as a literal character. We don't skip over the EOL,
								 * since we don't support escaping it anymore (unlike PG).
								 */
								newc = escapec;
								skip--;
							}
						}

						break;
				}

				/* update to current length, add escape and escaped chars  */
				attr_pre_len += scan_end - scan_start + 2;
				/* update to current length, escaped char */
				attr_post_len += scan_end - scan_start + 1;

				/*
				 * Need to get rid of the escape character. This is done by
				 * loading the chunk up to including the escape character
				 * into the attribute buffer. Then overwriting the escape char
				 * with the escaped sequence or char, and continuing to scan
				 * from *after* the char than is after the escape in line_buf.
				 */
				*(cstate->attribute_buf.data + cstate->attribute_buf.len - 1) = newc;
				cstate->line_buf.cursor = scan_end - cstate->line_buf.data + skip;
				scan_start = scan_end + skip;
				chunk_start = cstate->line_buf.cursor;
				chunk_len = 0;
			}

		}						/* end delimiter/backslash */

	}							/* end line buffer scan. */

	/*
	 * Replace all delimiters with NULL for string termination.
	 * NOTE: only delimiters (NOT necessarily all delimc) are replaced.
	 * Example (delimc = '|'):
	 * - Before:  f  1	|  f  \|  2  |	f  3
	 * - After :  f  1 \0  f   |  2 \0	f  3
	 */
	for (attribute = 0; attribute < num_phys_attrs; attribute++)
	{
		if (attr_offsets[attribute] != 0)
			*(cstate->attribute_buf.data + attr_offsets[attribute] - 1) = '\0';
	}

	/* 
	 * MPP-6816 
	 * If any attribute has a de-escaped octal or hex sequence with a
	 * high bit set, we check that the changed attribute text is still
	 * valid WRT encoding. We run the check on all attributes since 
	 * such octal sequences are so rare in client data that it wouldn't
	 * affect performance at all anyway.
	 */
	if (saw_non_ascii)
	{
		for (attribute = 0; attribute < num_phys_attrs; attribute++)
		{
			char *fld = cstate->attribute_buf.data + attr_offsets[attribute];
			pg_verifymbstr(fld, strlen(fld), false);
		}
	}
}

/*
 * Read all the attributes of the data line in CSV mode,
 * performing de-escaping as needed. Escaping does not follow the normal
 * PostgreSQL text mode, but instead "standard" (i.e. common) CSV usage.
 *
 * Quoted fields can span lines, in which case the line end is embedded
 * in the returned string.
 *
 * null_print is the null marker string.  Note that this is compared to
 * the pre-de-escaped input string (thus if it is quoted it is not a NULL).
 *----------
 */
void
CopyReadAttributesCSV(CopyState cstate, bool *nulls, int *attr_offsets,
					  int num_phys_attrs, Form_pg_attribute *attr)
{
	char		delimc = cstate->delim[0];
	char		quotec = cstate->quote[0];
	char		escapec = cstate->escape[0];
	int			attribute = 1;
	ListCell   *cur;			/* cursor to attribute list used for this COPY */
	int 		attr_cursor;

	/* init variables for attribute scan */
	RESET_ATTRBUF;

	cur = list_head(cstate->attnumlist);
	attr_cursor = cstate->attribute_buf.cursor;

	int line_len = cstate->line_buf.len;
	if (cstate->eol_type == EOL_CRLF)
		line_len--;

	/* if zero column table and data is trying to get in */
	if(num_phys_attrs == 0)
	{
		if (line_len > 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));
		return;
	}

	if (cstate->delimiter_off)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("delimiter 'OFF' is not supported by copy command")));

	for (;;)
	{
		bool		found_delim = false;
		bool		saw_quote = false;
		int			input_len = 0;
		int			attnum;			/* attribute number being parsed */
		int			fieldno = 0;			/* attribute index being parsed */
		int			start_cursor = cstate->line_buf.cursor;
		int			end_cursor;

		/*
		 * Scan data for field,
		 *
		 * The loop starts in "not quote" mode and then toggles between that
		 * and "in quote" mode. The loop exits normally if it is in "not
		 * quote" mode and a delimiter or line end is seen.
		 */
		for (;;)
		{
			char		c;

			/* Not in quote */
			for (;;)
			{
				end_cursor = cstate->line_buf.cursor;
				if (cstate->line_buf.cursor >= line_len - 1)
					goto endfield;
				c = cstate->line_buf.data[cstate->line_buf.cursor++];
				/* unquoted field delimiter */
				if (c == delimc)
				{
					found_delim = true;
					goto endfield;
				}
				/* start of quoted field (or part of field) */
				if (c == quotec)
				{
					saw_quote = true;
					break;
				}
				/* Add c to output string */
				appendStringInfoCharMacro(&cstate->attribute_buf, c);
				cstate->attribute_buf.cursor++;
			}

			/* In quote */
			for (;;)
			{
				end_cursor = cstate->line_buf.cursor;
				if (cstate->line_buf.cursor >= line_len - 1)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unterminated CSV quoted field")));

				c = cstate->line_buf.data[cstate->line_buf.cursor++];

				/* escape within a quoted field */
				if (c == escapec)
				{
					/*
					 * peek at the next char if available, and escape it if it
					 * is an escape char or a quote char
					 */
					if (cstate->line_buf.cursor < line_len - 1)
					{
						char 		nextc = cstate->line_buf.data[cstate->line_buf.cursor];

						if (nextc == escapec || nextc == quotec)
						{
							appendStringInfoCharMacro(&cstate->attribute_buf, nextc);
							cstate->attribute_buf.cursor++;
							cstate->line_buf.cursor++;
							continue;
						}
					}
				}

				/*
				 * end of quoted field. Must do this test after testing for
				 * escape in case quote char and escape char are the same
				 * (which is the common case).
				 */
				if (c == quotec)
					break;

				/* Add c to output string */
				appendStringInfoCharMacro(&cstate->attribute_buf, c);
				cstate->attribute_buf.cursor++;
			}
		}
endfield:
		if (cur == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));

		/* Check whether raw input matched null marker */
		input_len = end_cursor - start_cursor;

		/* finished processing attributes in line */
		if (!found_delim)
		{
			/*
			 * line is done, but do we have more attributes to process?
			 *
			 * normally, remaining attributes that have no data means ERROR,
			 * however, with FILL MISSING FIELDS remaining attributes become
			 * NULL. since attrs are null by default we leave unchanged and
			 * avoid throwing an error, with the exception of empty data lines
			 * for multiple attributes, which we intentionally don't support.
			 */
			if (lnext(cur) != NULL)
			{
				if (!cstate->fill_missing)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("missing data for column \"%s\"",
									NameStr(attr[lfirst_int(lnext(cur)) - 1]->attname))));

				else if (attribute == 1 && input_len == 0)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("missing data for column \"%s\", found empty data line",
									NameStr(attr[lfirst_int(lnext(cur)) - 1]->attname))));
			}
		}

		/* check whether raw input matched null marker */
		attnum = lfirst_int(cur);
		fieldno = attnum - 1;
		if (!saw_quote && input_len == cstate->null_print_len &&
			strncmp(&cstate->line_buf.data[start_cursor], cstate->null_print, input_len) == 0)
			nulls[fieldno] = true;
		else
			nulls[fieldno] = false;
		attr_offsets[fieldno] = attr_cursor;

		/* Terminate attribute value in output area */
		appendStringInfoCharMacro(&cstate->attribute_buf, '\0');
		cstate->attribute_buf.cursor++;
		attr_cursor = cstate->attribute_buf.cursor;

		/* setup next attribute scan */
		cur = lnext(cur);

		/*
		 * for the dispatcher - stop parsing once we have
		 * all the hash field values. We don't need the rest.
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			if (attribute == cstate->last_hash_field)
				break;
		}

		attribute++;

		/* Done if we hit EOL instead of a delim */
		if (!found_delim)
			break;
	}

	if (cstate->eol_type == EOL_CRLF)
		cstate->line_buf.cursor++;
}

/*
 * Read a single attribute line when delimiter is 'off'. This is a fast track -
 * we copy the entire line buf into the attribute buf, check for null value,
 * and we're done.
 *
 * Note that no equivalent function exists for CSV, as in CSV we still may
 * need to parse quotes etc. so the functionality of delimiter_off is inlined
 * inside of CopyReadAttributesCSV
 */
static void
CopyReadAttributesTextNoDelim(CopyState cstate, bool *nulls, int num_phys_attrs,
							  int attnum)
{
	int 	len = 0;

	Assert(num_phys_attrs == 1);

	/* don't count eol char(s) in attr len calculation */
	len = cstate->line_buf.len - 1;

	if (cstate->eol_type == EOL_CRLF)
		len--;

	/* check if this is a NULL value or data value (assumed NULL) */
	if (len == cstate->null_print_len &&
		strncmp(cstate->line_buf.data, cstate->null_print, len) == 0)
		nulls[attnum - 1] = true;
	else
		nulls[attnum - 1] = false;

	appendBinaryStringInfo(&cstate->attribute_buf, cstate->line_buf.data, len);
}

/*
 * Read the first attribute. This is mainly used to maintain support
 * for an OID column. All the rest of the columns will be read at once with
 * CopyReadAttributesText.
 */
static char *
CopyReadOidAttr(CopyState cstate, bool *isnull)
{
	char		delimc = cstate->delim[0];
	char	   *start_loc = cstate->line_buf.data + cstate->line_buf.cursor;
	char	   *end_loc;
	int			attr_len = 0;
	int			bytes_remaining;

	/* reset attribute buf to empty */
	RESET_ATTRBUF;

	/* # of bytes that were not yet processed in this line */
	bytes_remaining = cstate->line_buf.len - cstate->line_buf.cursor;

	/* got to end of line */
	if ((end_loc = scanTextLine(cstate, start_loc, delimc, bytes_remaining)) == NULL)
	{
		attr_len = bytes_remaining - 1; /* don't count '\n' in len calculation */
		appendBinaryStringInfo(&cstate->attribute_buf, start_loc, attr_len);
		cstate->line_buf.cursor += attr_len + 2;		/* skip '\n' and '\0' */
	}
	else
		/* found a delimiter */
	{
		/*
		 * (we don't care if delim was preceded with a backslash, because it's
		 * an invalid OID anyway)
		 */

		attr_len = end_loc - start_loc; /* we don't include the delimiter ch */

		appendBinaryStringInfo(&cstate->attribute_buf, start_loc, attr_len);
		cstate->line_buf.cursor += attr_len + 1;
	}


	/* check whether raw input matched null marker */
	if (attr_len == cstate->null_print_len && strncmp(start_loc, cstate->null_print, attr_len) == 0)
		*isnull = true;
	else
		*isnull = false;

	return cstate->attribute_buf.data;
}

/*
 * Read a binary attribute.
 * skip_parsing is a hack for CopyFromDispatch (so we don't parse unneeded fields)
 */
static Datum
CopyReadBinaryAttribute(CopyState cstate,
						int column_no, FmgrInfo *flinfo,
						Oid typioparam, int32 typmod,
						bool *isnull, bool skip_parsing)
{
	int32		fld_size;
	Datum		result = 0;

	if (!CopyGetInt32(cstate, &fld_size))
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));
	if (fld_size == -1)
	{
		*isnull = true;
		return ReceiveFunctionCall(flinfo, NULL, typioparam, typmod);
	}
	if (fld_size < 0)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("invalid field size")));

	/* reset attribute_buf to empty, and load raw data in it */
	resetStringInfo(&cstate->attribute_buf);

	enlargeStringInfo(&cstate->attribute_buf, fld_size);
	if (CopyGetData(cstate, cstate->attribute_buf.data,
					fld_size) != fld_size)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));

	cstate->attribute_buf.len = fld_size;
	cstate->attribute_buf.data[fld_size] = '\0';

	if (!skip_parsing)
	{
		/* Call the column type's binary input converter */
		result = ReceiveFunctionCall(flinfo, &cstate->attribute_buf,
									 typioparam, typmod);

		/* Trouble if it didn't eat the whole buffer */
		if (cstate->attribute_buf.cursor != cstate->attribute_buf.len)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
					 errmsg("incorrect binary data format")));
	}

	*isnull = false;
	return result;
}

/*
 * Send text representation of one attribute, with conversion and escaping
 */
#define DUMPSOFAR() \
	do { \
		if (ptr > start) \
			CopySendData(cstate, start, ptr - start); \
	} while (0)

/*
 * Send text representation of one attribute, with conversion and escaping
 */
static void
CopyAttributeOutText(CopyState cstate, char *string)
{
	char	   *ptr;
	char	   *start;
	char		c;
	char		delimc = cstate->delim[0];
	char		escapec = cstate->escape[0];

	if (cstate->need_transcoding)
		ptr = pg_server_to_custom(string, 
								  strlen(string), 
								  cstate->client_encoding, 
								  cstate->enc_conversion_proc);
	else
		ptr = string;


	if (cstate->escape_off)
	{
		CopySendData(cstate, ptr, strlen(ptr));
		return;
	}

	/*
	 * We have to grovel through the string searching for control characters
	 * and instances of the delimiter character.  In most cases, though, these
	 * are infrequent.	To avoid overhead from calling CopySendData once per
	 * character, we dump out all characters between escaped characters in a
	 * single call.  The loop invariant is that the data from "start" to "ptr"
	 * can be sent literally, but hasn't yet been.
	 *
	 * We can skip pg_encoding_mblen() overhead when encoding is safe, because
	 * in valid backend encodings, extra bytes of a multibyte character never
	 * look like ASCII.  This loop is sufficiently performance-critical that
	 * it's worth making two copies of it to get the IS_HIGHBIT_SET() test out
	 * of the normal safe-encoding path.
	 */
	if (cstate->encoding_embeds_ascii)
	{
		start = ptr;
		while ((c = *ptr) != '\0')
		{
			if ((unsigned char) c < (unsigned char) 0x20)
			{
				/*
				 * \r and \n must be escaped, the others are traditional. We
				 * prefer to dump these using the C-like notation, rather than
				 * a backslash and the literal character, because it makes the
				 * dump file a bit more proof against Microsoftish data
				 * mangling.
				 */
				switch (c)
				{
					case '\b':
						c = 'b';
						break;
					case '\f':
						c = 'f';
						break;
					case '\n':
						c = 'n';
						break;
					case '\r':
						c = 'r';
						break;
					case '\t':
						c = 't';
						break;
					case '\v':
						c = 'v';
						break;
					default:
						/* If it's the delimiter, must backslash it */
						if (c == delimc)
							break;
						/* All ASCII control chars are length 1 */
						ptr++;
						continue;		/* fall to end of loop */
				}
				/* if we get here, we need to convert the control char */
				DUMPSOFAR();
				CopySendChar(cstate, escapec);
				CopySendChar(cstate, c);
				start = ++ptr;	/* do not include char in next run */
			}
			else if (c == escapec || c == delimc)
			{
				DUMPSOFAR();
				CopySendChar(cstate, escapec);
				start = ptr++;	/* we include char in next run */
			}
			else if (IS_HIGHBIT_SET(c))
				ptr += pg_encoding_mblen(cstate->client_encoding, ptr);
			else
				ptr++;
		}
	}
	else
	{
		start = ptr;
		while ((c = *ptr) != '\0')
		{
			if ((unsigned char) c < (unsigned char) 0x20)
			{
				/*
				 * \r and \n must be escaped, the others are traditional. We
				 * prefer to dump these using the C-like notation, rather than
				 * a backslash and the literal character, because it makes the
				 * dump file a bit more proof against Microsoftish data
				 * mangling.
				 */
				switch (c)
				{
					case '\b':
						c = 'b';
						break;
					case '\f':
						c = 'f';
						break;
					case '\n':
						c = 'n';
						break;
					case '\r':
						c = 'r';
						break;
					case '\t':
						c = 't';
						break;
					case '\v':
						c = 'v';
						break;
					default:
						/* If it's the delimiter, must backslash it */
						if (c == delimc)
							break;
						/* All ASCII control chars are length 1 */
						ptr++;
						continue;		/* fall to end of loop */
				}
				/* if we get here, we need to convert the control char */
				DUMPSOFAR();
				CopySendChar(cstate, escapec);
				CopySendChar(cstate, c);
				start = ++ptr;	/* do not include char in next run */
			}
			else if (c == escapec || c == delimc)
			{
				DUMPSOFAR();
				CopySendChar(cstate, escapec);
				start = ptr++;	/* we include char in next run */
			}
			else
				ptr++;
		}
	}

	DUMPSOFAR();
}

/*
 * Send text representation of one attribute, with conversion and
 * CSV-style escaping
 */
static void
CopyAttributeOutCSV(CopyState cstate, char *string,
					bool use_quote, bool single_attr)
{
	char	   *ptr;
	char	   *start;
	char		c;
	char		delimc = cstate->delim[0];
	char		quotec;
	char		escapec = cstate->escape[0];

	/*
	 * MPP-8075. We may get called with cstate->quote == NULL.
	 */
	if (cstate->quote == NULL)
	{
		quotec = '"';
	}
	else
	{
		quotec = cstate->quote[0];
	}

	/* force quoting if it matches null_print (before conversion!) */
	if (!use_quote && strcmp(string, cstate->null_print) == 0)
		use_quote = true;

	if (cstate->need_transcoding)
		ptr = pg_server_to_custom(string, 
								  strlen(string),
								  cstate->client_encoding,
								  cstate->enc_conversion_proc);
	else
		ptr = string;

	/*
	 * Make a preliminary pass to discover if it needs quoting
	 */
	if (!use_quote)
	{
		/*
		 * Because '\.' can be a data value, quote it if it appears alone on a
		 * line so it is not interpreted as the end-of-data marker.
		 */
		if (single_attr && strcmp(ptr, "\\.") == 0)
			use_quote = true;
		else
		{
			char	   *tptr = ptr;

			while ((c = *tptr) != '\0')
			{
				if (c == delimc || c == quotec || c == '\n' || c == '\r')
				{
					use_quote = true;
					break;
				}
				if (IS_HIGHBIT_SET(c) && cstate->encoding_embeds_ascii)
					tptr += pg_encoding_mblen(cstate->client_encoding, tptr);
				else
					tptr++;
			}
		}
	}

	if (use_quote)
	{
		CopySendChar(cstate, quotec);

		/*
		 * We adopt the same optimization strategy as in CopyAttributeOutText
		 */
		start = ptr;
		while ((c = *ptr) != '\0')
		{
			if (c == quotec || c == escapec)
			{
				DUMPSOFAR();
				CopySendChar(cstate, escapec);
				start = ptr;	/* we include char in next run */
			}
			if (IS_HIGHBIT_SET(c) && cstate->encoding_embeds_ascii)
				ptr += pg_encoding_mblen(cstate->client_encoding, ptr);
			else
				ptr++;
		}
		DUMPSOFAR();

		CopySendChar(cstate, quotec);
	}
	else
	{
		/* If it doesn't need quoting, we can just dump it as-is */
		CopySendString(cstate, ptr);
	}
}

/*
 * CopyGetAttnums - build an integer list of attnums to be copied
 *
 * The input attnamelist is either the user-specified column list,
 * or NIL if there was none (in which case we want all the non-dropped
 * columns).
 *
 * rel can be NULL ... it's only used for error reports.
 */
List *
CopyGetAttnums(TupleDesc tupDesc, Relation rel, List *attnamelist)
{
	List	   *attnums = NIL;

	if (attnamelist == NIL)
	{
		/* Generate default column list */
		Form_pg_attribute *attr = tupDesc->attrs;
		int			attr_count = tupDesc->natts;
		int			i;

		for (i = 0; i < attr_count; i++)
		{
			if (attr[i]->attisdropped)
				continue;
			attnums = lappend_int(attnums, i + 1);
		}
	}
	else
	{
		/* Validate the user-supplied list and extract attnums */
		ListCell   *l;

		foreach(l, attnamelist)
		{
			char	   *name = strVal(lfirst(l));
			int			attnum;
			int			i;

			/* Lookup column name */
			attnum = InvalidAttrNumber;
			for (i = 0; i < tupDesc->natts; i++)
			{
				if (tupDesc->attrs[i]->attisdropped)
					continue;
				if (namestrcmp(&(tupDesc->attrs[i]->attname), name) == 0)
				{
					attnum = tupDesc->attrs[i]->attnum;
					break;
				}
			}
			if (attnum == InvalidAttrNumber)
			{
				if (rel != NULL)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
					errmsg("column \"%s\" of relation \"%s\" does not exist",
						   name, RelationGetRelationName(rel))));
				else
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" does not exist",
									name)));
			}
			/* Check for duplicates */
			if (list_member_int(attnums, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_COLUMN),
						 errmsg("column \"%s\" specified more than once",
								name)));
			attnums = lappend_int(attnums, attnum);
		}
	}

	return attnums;
}

#define COPY_FIND_MD_DELIM \
md_delim = memchr(line_start, COPY_METADATA_DELIM, Min(32, cstate->line_buf.len)); \
if(md_delim && (md_delim != line_start)) \
{ \
	value_len = md_delim - line_start + 1; \
	*md_delim = '\0'; \
} \
else \
{ \
	cstate->md_error = true; \
}	

/*
 * CopyExtractRowMetaData - extract embedded row number from data.
 *
 * If data is being parsed in execute mode the parser (QE) doesn't
 * know the original line number (in the original file) of the current
 * row. Therefore the QD sends this information along with the data.
 * other metadata that the QD sends includes whether the data was
 * converted to server encoding (should always be the case, unless
 * encoding error happened and we're in error log mode).
 *
 * in:
 *    line_buf: <original_num>^<buf_converted>^<data for this row>
 *    lineno: ?
 *    line_buf_converted: ?
 *
 * out:
 *    line_buf: <data for this row>
 *    lineno: <original_num>
 *    line_buf_converted: <t/f>
 */
static
void CopyExtractRowMetaData(CopyState cstate)
{
	char *md_delim = NULL; /* position of the metadata delimiter */
	
	/*
	 * Line_buf may have already skipped an OID column if WITH OIDS defined,
	 * so we need to start from cursor not always from beginning of linebuf.
	 */
	char *line_start = cstate->line_buf.data + cstate->line_buf.cursor;
	int  value_len = 0;

	cstate->md_error = false;
	
	/* look for the first delimiter, and extract lineno */
	COPY_FIND_MD_DELIM;
	
	/* 
	 * make sure MD exists. that should always be the case
	 * unless we run into an edge case - see MPP-8052. if that 
	 * happens md_error is now set. we raise an error. 
	 */
	if(cstate->md_error)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("COPY metadata not found. This probably means that there is a "
						"mixture of newline types in the data. Use the NEWLINE keyword "
						"in order to resolve this reliably.")));

	cstate->cur_lineno = atoi(line_start);

	*md_delim = COPY_METADATA_DELIM; /* restore the line_buf byte after setting it to \0 */

	/* reposition line buf cursor to see next metadata value (skip lineno) */
	cstate->line_buf.cursor += value_len;
	line_start = cstate->line_buf.data + cstate->line_buf.cursor;

	/* look for the second delimiter, and extract line_buf_converted */
	COPY_FIND_MD_DELIM;
	Assert(*line_start == '0' || *line_start == '1'); 
	cstate->line_buf_converted = atoi(line_start);
	
	*md_delim = COPY_METADATA_DELIM;
	cstate->line_buf.cursor += value_len;
}

/*
 * error context callback for COPY FROM
 */
static void
copy_in_error_callback(void *arg)
{
	CopyState	cstate = (CopyState) arg;
	char buffer[20];

	/*
	 * If we saved the error context from a QE in cdbcopy.c append it here.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && cstate->executor_err_context.len > 0)
	{
		errcontext("%s", cstate->executor_err_context.data);
		return;
	}

	/* don't need to print out context if error wasn't local */
	if (cstate->error_on_executor)
		return;

	if (cstate->binary)
	{
		/* can't usefully display the data */
		if (cstate->cur_attname)
			errcontext("COPY %s, line %s, column %s",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, cstate->cur_lineno),
					   cstate->cur_attname);
		else
			errcontext("COPY %s, line %s",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, cstate->cur_lineno));
	}
	else
	{
	if (cstate->cur_attname)
	{
		/* error is relevant to a particular column */
		char	   *att_buf;

		att_buf = limit_printout_length(cstate->attribute_buf.data);

		errcontext("COPY %s, line %s, column %s",
				   cstate->cur_relname,
				   linenumber_atoi(buffer, cstate->cur_lineno),
				   att_buf);
		pfree(att_buf);
	}
	else
	{
		/* error is relevant to a particular line */
		if (cstate->line_buf_converted || !cstate->need_transcoding)
		{
			char	   *line_buf;

			line_buf = extract_line_buf(cstate);
			truncateEolStr(line_buf, cstate->eol_type);

			errcontext("COPY %s, line %s: \"%s\"",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, cstate->cur_lineno),
					   line_buf);
			pfree(line_buf);
		}
		else
		{
			/*
			 * Here, the line buffer is still in a foreign encoding,
			 * and indeed it's quite likely that the error is precisely
			 * a failure to do encoding conversion (ie, bad data).	We
			 * dare not try to convert it, and at present there's no way
			 * to regurgitate it without conversion.  So we have to punt
			 * and just report the line number.
			 */
			errcontext("COPY %s, line %s",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, cstate->cur_lineno));
		}
	}
}
}

/*
 * If our (copy of) linebuf has the embedded original row number and other
 * row-specific metadata, remove it. It is not part of the actual data, and
 * should not be displayed.
 *
 * we skip this step, however, if md_error was previously set by
 * CopyExtractRowMetaData. That should rarely happen, though.
 *
 * Returned value is a palloc'ed string to print.  The caller should pfree it.
 */
static char *
extract_line_buf(CopyState cstate)
{
	char	   *line_buf = cstate->line_buf.data;

	if (cstate->err_loc_type == ROWNUM_EMBEDDED && !cstate->md_error)
	{
		/* the following is a compacted mod of CopyExtractRowMetaData */
		int value_len = 0;
		char *line_start = cstate->line_buf.data;
		char *lineno_delim = memchr(line_start, COPY_METADATA_DELIM,
									Min(32, cstate->line_buf.len));

		if (lineno_delim && (lineno_delim != line_start))
		{
			/*
			 * we only continue parsing metadata if the first extraction above
			 * succeeded. there are some edge cases where we may not have a line
			 * with MD to parse, for example if some non-copy related error
			 * propagated here and we don't yet have a proper data line.
			 * see MPP-11328
			 */
			value_len = lineno_delim - line_start + 1;
			line_start += value_len;

			lineno_delim = memchr(line_start, COPY_METADATA_DELIM,
								  Min(32, cstate->line_buf.len));

			if (lineno_delim && (lineno_delim != line_start))
			{
				value_len = lineno_delim - line_start + 1;
				line_start += value_len;
				line_buf = line_start;
			}
		}
	}

	/*
	 * Finally allocate a new buffer and trim the string to a reasonable
	 * length.  We need a copy since this might be called from non-ERROR
	 * context like NOTICE, and we should preserve the original.
	 */
	return limit_printout_length(line_buf);
}

/*
 * Make sure we don't print an unreasonable amount of COPY data in a message.
 *
 * It would seem a lot easier to just use the sprintf "precision" limit to
 * truncate the string.  However, some versions of glibc have a bug/misfeature
 * that vsnprintf will always fail (return -1) if it is asked to truncate
 * a string that contains invalid byte sequences for the current encoding.
 * So, do our own truncation.  We return a pstrdup'd copy of the input.
 */
char *
limit_printout_length(const char *str)
{
#define MAX_COPY_DATA_DISPLAY 100

	int			slen = strlen(str);
	int			len;
	char	   *res;

	/* Fast path if definitely okay */
	if (slen <= MAX_COPY_DATA_DISPLAY)
		return pstrdup(str);

	/* Apply encoding-dependent truncation */
	len = pg_mbcliplen(str, slen, MAX_COPY_DATA_DISPLAY);

	/*
	 * Truncate, and add "..." to show we truncated the input.
	 */
	res = (char *) palloc(len + 4);
	memcpy(res, str, len);
	strcpy(res + len, "...");

	return res;
}


static void
attr_get_key(CopyState cstate, CdbCopy *cdbCopy, int original_lineno_for_qe,
			 unsigned int target_seg,
			 AttrNumber p_nattrs, AttrNumber *attrs,
			 Form_pg_attribute *attr_descs, int *attr_offsets, bool *attr_nulls,
			 FmgrInfo *in_functions, Oid *typioparams, Datum *values)
{
	AttrNumber p_index;

	/*
	 * Since we only need the internal format of values that
	 * we want to hash on (partitioning keys only), we want to
	 * skip converting the other values so we can run faster.
	 */
	for (p_index = 0; p_index < p_nattrs; p_index++)
	{
		ListCell *cur;

		/*
		 * For this partitioning key, search for its location in the attr list.
		 * (note that fields may be out of order, so this is necessary).
		 */
		foreach(cur, cstate->attnumlist)
		{
			int			attnum = lfirst_int(cur);
			int			m = attnum - 1;
			char	   *string;
			bool		isnull;

			if (attnum == attrs[p_index])
			{
				string = cstate->attribute_buf.data + attr_offsets[m];

				if (attr_nulls[m])
					isnull = true;
				else
					isnull = false;

				if (cstate->csv_mode && isnull &&
					cstate->force_notnull_flags[m])
				{
					string = cstate->null_print;		/* set to NULL string */
					isnull = false;
				}

				/* we read an SQL NULL, no need to do anything */
				if (!isnull)
				{
					cstate->cur_attname = NameStr(attr_descs[m]->attname);

					values[m] = InputFunctionCall(&in_functions[m],
												  string,
												  typioparams[m],
												  attr_descs[m]->atttypmod);

					attr_nulls[m] = false;
					cstate->cur_attname = NULL;
				}		/* end if (!isnull) */

				break;	/* go to next partitioning key
						 * attribute */
			}
		}		/* end foreach */
	}			/* end for partitioning indexes */
}

/*
 * The following are custom versions of the string function strchr().
 * As opposed to the original strchr which searches through
 * a string until the target character is found, or a NULL is
 * found, this version will not return when a NULL is found.
 * Instead it will search through a pre-defined length of
 * bytes and will return only if the target character(s) is reached.
 *
 * If our client encoding is not a supported server encoding, we
 * know that it is not safe to look at each character as trailing
 * byte in a multibyte character may be a 7-bit ASCII equivalent.
 * Therefore we use pg_encoding_mblen to skip to the end of the
 * character.
 *
 * returns:
 *	 pointer to c - if c is located within the string.
 *	 NULL - if c was not found in specified length of search. Note:
 *			this DOESN'T mean that a '\0' was reached.
 */
char *
scanTextLine(CopyState cstate, const char *s, char eol, size_t len)
{
		
	if (cstate->encoding_embeds_ascii && !cstate->line_buf_converted)
	{
		int			mblen;
		const char *end = s + len;
		
		/* we may need to skip the end of a multibyte char from the previous buffer */
		s += cstate->missing_bytes;
		
		mblen = pg_encoding_mblen(cstate->client_encoding, s);

		for (; *s != eol && s < end; s += mblen)
			mblen = pg_encoding_mblen(cstate->client_encoding, s);

		/* 
		 * MPP-10802
		 * if last char is a partial mb char (the rest of its bytes are in the next 
		 * buffer) save # of missing bytes for this char and skip them next time around 
		 */
		cstate->missing_bytes = (s > end ? s - end : 0);
			
		return ((*s == eol) ? (char *) s : NULL);
	}
	else
		return memchr(s, eol, len);
}


char *
scanCSVLine(CopyState cstate, const char *s, char eol, char escapec, char quotec, size_t len)
{
	const char *start = s;
	const char *end = start + len;
	
	if (cstate->encoding_embeds_ascii && !cstate->line_buf_converted)
	{
		int			mblen;
		
		/* we may need to skip the end of a multibyte char from the previous buffer */
		s += cstate->missing_bytes;
		
		mblen = pg_encoding_mblen(cstate->client_encoding, s);
		
		for ( ; *s != eol && s < end ; s += mblen)
		{
			if (cstate->in_quote && *s == escapec)
				cstate->last_was_esc = !cstate->last_was_esc;
			if (*s == quotec && !cstate->last_was_esc)
				cstate->in_quote = !cstate->in_quote;
			if (*s != escapec)
				cstate->last_was_esc = false;

			mblen = pg_encoding_mblen(cstate->client_encoding, s);
		}
		
		/* 
		 * MPP-10802
		 * if last char is a partial mb char (the rest of its bytes are in the next 
		 * buffer) save # of missing bytes for this char and skip them next time around 
		 */
		cstate->missing_bytes = (s > end ? s - end : 0);
	}
	else
		/* safe to scroll byte by byte */
	{	
		for ( ; *s != eol && s < end ; s++)
		{
			if (cstate->in_quote && *s == escapec)
				cstate->last_was_esc = !cstate->last_was_esc;
			if (*s == quotec && !cstate->last_was_esc)
				cstate->in_quote = !cstate->in_quote;
			if (*s != escapec)
				cstate->last_was_esc = false;
		}
	}

	if (s == end)
		return NULL;
	
	if (*s == eol)
		cstate->last_was_esc = false;

	return ((*s == eol) ? (char *) s : NULL);
}

/* remove end of line chars from end of a buffer */
void truncateEol(StringInfo buf, EolType eol_type)
{
	int one_back = buf->len - 1;
	int two_back = buf->len - 2;

	if(eol_type == EOL_CRLF)
	{
		if(buf->len < 2)
			return;

		if(buf->data[two_back] == '\r' &&
		   buf->data[one_back] == '\n')
		{
			buf->data[two_back] = '\0';
			buf->data[one_back] = '\0';
			buf->len -= 2;
		}
	}
	else
	{
		if(buf->len < 1)
			return;

		if(buf->data[one_back] == '\r' ||
		   buf->data[one_back] == '\n')
		{
			buf->data[one_back] = '\0';
			buf->len--;
		}
	}
}

/* wrapper for truncateEol */
void
truncateEolStr(char *str, EolType eol_type)
{
	StringInfoData buf;

	buf.data = str;
	buf.len = strlen(str);
	buf.maxlen = buf.len;
	truncateEol(&buf, eol_type);
}

/*
 * concatenateEol
 *
 * add end of line chars to end line buf.
 *
 */
static void concatenateEol(CopyState cstate)
{
	switch (cstate->eol_type)
	{
		case EOL_LF:
			appendStringInfo(&cstate->line_buf, "\n");
			break;
		case EOL_CR:
			appendStringInfo(&cstate->line_buf, "\r");
			break;
		case EOL_CRLF:
			appendStringInfo(&cstate->line_buf, "\r\n");
			break;
		case EOL_UNKNOWN:
			appendStringInfo(&cstate->line_buf, "\n");
			break;

	}
}

/*
 * Escape any single quotes or backslashes in given string (from initdb.c)
 */
static char *
escape_quotes(const char *src)
{
	int			len = strlen(src),
				i,
				j;
	char	   *result = palloc(len * 2 + 1);

	for (i = 0, j = 0; i < len; i++)
	{
		if ((src[i]) == '\'' || (src[i]) == '\\')
			result[j++] = src[i];
		result[j++] = src[i];
	}
	result[j] = '\0';
	return result;
}

/*
 * copy_dest_startup --- executor startup
 */
static void
copy_dest_startup(DestReceiver *self __attribute__((unused)), int operation __attribute__((unused)), TupleDesc typeinfo __attribute__((unused)))
{
	/* no-op */
}

/*
 * copy_dest_receive --- receive one tuple
 */
static void
copy_dest_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_copy    *myState = (DR_copy *) self;
	CopyState	cstate = myState->cstate;

	/* Make sure the tuple is fully deconstructed */
	slot_getallattrs(slot);

	/* And send the data */
	CopyOneRowTo(cstate, InvalidOid, slot_get_values(slot), slot_get_isnull(slot));
}

/*
 * copy_dest_shutdown --- executor end
 */
static void
copy_dest_shutdown(DestReceiver *self __attribute__((unused)))
{
	/* no-op */
}

/*
 * copy_dest_destroy --- release DestReceiver object
 */
static void
copy_dest_destroy(DestReceiver *self)
{
	pfree(self);
}

/*
 * CreateCopyDestReceiver -- create a suitable DestReceiver object
 */
DestReceiver *
CreateCopyDestReceiver(void)
{
	DR_copy    *self = (DR_copy *) palloc(sizeof(DR_copy));

	self->pub.receiveSlot = copy_dest_receive;
	self->pub.rStartup = copy_dest_startup;
	self->pub.rShutdown = copy_dest_shutdown;
	self->pub.rDestroy = copy_dest_destroy;
	self->pub.mydest = DestCopyOut;

	self->cstate = NULL;		/* will be set later */

	return (DestReceiver *) self;
}


static void CopyInitPartitioningState(EState *estate)
{
	if (estate->es_result_partitions)
	{
		estate->es_partition_state =
 			createPartitionState(estate->es_result_partitions,
								 estate->es_num_result_relations);
	}
}

/*
 * Initialize data loader parsing state
 */
static void CopyInitDataParser(CopyState cstate)
{
	cstate->fe_eof = false;
	cstate->cur_relname = RelationGetRelationName(cstate->rel);
	cstate->cur_lineno = 0;
	cstate->cur_attname = NULL;
	cstate->null_print_len = strlen(cstate->null_print);

	if (cstate->csv_mode)
	{
		cstate->in_quote = false;
		cstate->last_was_esc = false;
		cstate->num_consec_csv_err = 0;
	}

	/* Set up data buffer to hold a chunk of data */
	MemSet(cstate->raw_buf, ' ', RAW_BUF_SIZE * sizeof(char));
	cstate->raw_buf[RAW_BUF_SIZE] = '\0';
	cstate->line_done = true;
	cstate->raw_buf_done = false;
}

/*
 * CopyCheckIsLastLine
 *
 * This routine checks if the line being looked at is the last line of data.
 * If it is, it makes sure that this line is terminated with an EOL. We must
 * do this check in order to support files that don't end up EOL before EOF,
 * because we want to treat that last line as normal - and be able to pre
 * process it like the other lines (remove metadata chars, encoding conversion).
 *
 * See MPP-4406 for an example of why this is needed.
 *
 * Notice: if line_buf is empty, no need to add EOL
 */
static bool CopyCheckIsLastLine(CopyState cstate)
{
	if (cstate->fe_eof && cstate->line_buf.len > 0)
	{
		concatenateEol(cstate);
		return true;
	}
	
	return false;
}

/*
 * setEncodingConversionProc
 *
 * COPY and External tables use a custom path to the encoding conversion
 * API because external tables have their own encoding (which is not
 * necessarily client_encoding). We therefore have to set the correct
 * encoding conversion function pointer ourselves, to be later used in
 * the conversion engine.
 *
 * The code here mimics a part of SetClientEncoding() in mbutils.c
 */
void setEncodingConversionProc(CopyState cstate, int client_encoding, bool iswritable)
{
	Oid		conversion_proc;
	
	/*
	 * COPY FROM and RET: convert from client to server
	 * COPY TO   and WET: convert from server to client
	 */
	if (iswritable)
		conversion_proc = FindDefaultConversionProc(GetDatabaseEncoding(),
													client_encoding);
	else		
		conversion_proc = FindDefaultConversionProc(client_encoding,
												    GetDatabaseEncoding());
	
	if (OidIsValid(conversion_proc))
	{
		/* conversion proc found */
		cstate->enc_conversion_proc = palloc(sizeof(FmgrInfo));
		fmgr_info(conversion_proc, cstate->enc_conversion_proc);
	}
	else
	{
		/* no conversion function (both encodings are probably the same) */
		cstate->enc_conversion_proc = NULL;
	}
}

/*
 * preProcessDataLine
 *
 * When Done reading a complete data line set input row number for error report
 * purposes (this also removes any metadata that was concatenated to the data
 * by the QD during COPY) and convert it to server encoding if transcoding is
 * needed.
 */
static
void preProcessDataLine(CopyState cstate)
{
	char	   *cvt;
	bool		force_transcoding = false;

	/*
	 * Increment line count by 1 if we have access to all the original
	 * data rows and can count them reliably (ROWNUM_ORIGINAL). However
	 * if we have ROWNUM_EMBEDDED the original row number for this row
	 * was sent to us with the data (courtesy of the data distributor), so
	 * get that number instead.
	 */
	if(cstate->err_loc_type == ROWNUM_ORIGINAL)
	{
		cstate->cur_lineno++;
	}
	else if(cstate->err_loc_type == ROWNUM_EMBEDDED)
	{
		Assert(Gp_role == GP_ROLE_EXECUTE);
		
		/*
		 * Extract various metadata sent to us from the QD COPY about this row:
		 * 1) the original line number of the row.
		 * 2) if the row was converted to server encoding or not
		 */
		CopyExtractRowMetaData(cstate); /* sets cur_lineno internally */
		
		/* check if QD sent us a badly encoded row, still in client_encoding, 
		 * in order to catch the encoding error ourselves. if line_buf_converted
		 * is false after CopyExtractRowMetaData then we must transcode and catch
		 * the error. Verify that we are indeed in SREH error log mode. that's
		 * the only valid path for receiving an unconverted data row.
		 */
		if (!cstate->line_buf_converted)
		{
			Assert(cstate->errMode == SREH_LOG);
			force_transcoding = true; 
		}
			
	}
	else
	{
		Assert(false); /* byte offset not yet supported */
	}
	
	if (cstate->need_transcoding || force_transcoding)
	{
		cvt = (char *) pg_custom_to_server(cstate->line_buf.data,
										   cstate->line_buf.len,
										   cstate->client_encoding,
										   cstate->enc_conversion_proc);
		
		Assert(!force_transcoding); /* if force is 't' we must have failed in the conversion */
		
		if (cvt != cstate->line_buf.data)
		{
			/* transfer converted data back to line_buf */
			RESET_LINEBUF;
			appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
			pfree(cvt);
		}
	}
	/* indicate that line buf is in server encoding */
	cstate->line_buf_converted = true;
}

void CopyEolStrToType(CopyState cstate)
{
	if (pg_strcasecmp(cstate->eol_str, "lf") == 0)
	{
		cstate->eol_type = EOL_LF;
		cstate->eol_ch[0] = '\n';
		cstate->eol_ch[1] = '\0';
	}
	else if (pg_strcasecmp(cstate->eol_str, "cr") == 0)
	{
		cstate->eol_type = EOL_CR;
		cstate->eol_ch[0] = '\r';
		cstate->eol_ch[1] = '\0';		
	}
	else if (pg_strcasecmp(cstate->eol_str, "crlf") == 0)
	{
		cstate->eol_type = EOL_CRLF;
		cstate->eol_ch[0] = '\r';
		cstate->eol_ch[1] = '\n';		
		
	}
	else /* error. must have been validated in CopyValidateControlChars() ! */
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("internal error in CopySetEolType. Trying to set NEWLINE %s", 
						 cstate->eol_str)));
}

static GpDistributionData *
InitDistributionData(CopyState cstate, Form_pg_attribute *attr,
                     AttrNumber num_phys_attrs,
                     EState *estate, bool multi_dist_policy)
{
	GpDistributionData *distData = palloc(sizeof(GpDistributionData));
	/* Variables for cdbpolicy */
	GpPolicy *policy; /* the partitioning policy for this table */
	AttrNumber p_nattrs; /* num of attributes in the distribution policy */
	Oid *p_attr_types; /* types for each policy attribute */
	HTAB *hashmap = NULL;
	CdbHash *cdbHash = NULL;
	AttrNumber h_attnum; /* hash key attribute number */
	int p_index;
	int total_segs = getgpsegmentCount();
	int i = 0;

	if (!multi_dist_policy)
	{
		policy = GpPolicyCopy(CurrentMemoryContext, cstate->rel->rd_cdbpolicy);

		if (policy)
			p_nattrs = policy->nattrs; /* number of partitioning keys */
		else
			p_nattrs = 0;
		/* Create hash API reference */
		cdbHash = makeCdbHash(total_segs);
	}
	else
	{
		/*
		 * This is a partitioned table that has multiple, different
		 * distribution policies.
		 *
		 * We build up a fake policy comprising the set of all columns used
		 * to distribute all children in the partition configuration. That way
		 * we're sure to parse all necessary columns in the input data and we
		 * have all column types handy.
		 */
		List *cols = NIL;
		ListCell *lc;
		HASHCTL hash_ctl;

		partition_get_policies_attrs(estate->es_result_partitions,
		                             cstate->rel->rd_cdbpolicy, &cols);
		MemSet(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(Oid);
		hash_ctl.entrysize = sizeof(cdbhashdata);
		hash_ctl.hash = oid_hash;
		hash_ctl.hcxt = CurrentMemoryContext;

		hashmap = hash_create("partition cdb hash map",
		                      100 /* XXX: need a better value, but what? */,
		                      &hash_ctl,
		                      HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);
		p_nattrs = list_length(cols);
		policy = palloc(sizeof(GpPolicy) + sizeof(AttrNumber) * p_nattrs);
		i = 0;
		foreach (lc, cols)
			policy->attrs[i++] = lfirst_int(lc);
	}

	/*
	 * Extract types for each partition key from the tuple descriptor,
	 * and convert them when necessary. We don't want to do this
	 * for each tuple since get_typtype() is quite expensive when called
	 * lots of times.
	 * The array key for p_attr_types is the attribute number of the attribute
	 * in question.
	 */
	p_attr_types = (Oid *) palloc0(num_phys_attrs * sizeof(Oid));

	for (i = 0; i < p_nattrs; i++)
	{
		h_attnum = policy->attrs[i];

		/*
		 * get the data type of this attribute. If it's an
		 * array type use anyarray, or else just use as is.
		 */
		if (attr[h_attnum - 1]->attndims > 0)
			p_attr_types[h_attnum - 1] = ANYARRAYOID;
		else
		{
			/* If this type is a domain type, get its base type. */
			p_attr_types[h_attnum - 1] = attr[h_attnum - 1]->atttypid;
			if (get_typtype(p_attr_types[h_attnum - 1]) == 'd')
				p_attr_types[h_attnum - 1] = getBaseType(
				        p_attr_types[h_attnum - 1]);
		}
	}

	/*
	 * for optimized parsing - get the last field number in the
	 * file that we need to parse to have all values for the hash keys.
	 * (If the table has an empty distribution policy, then we don't need
	 * to parse any attributes really... just send the row away using
	 * a special cdbhash function designed for this purpose).
	 */
	cstate->last_hash_field = 0;

	for (p_index = 0; p_index < p_nattrs; p_index++)
	{
		i = 1;

		/*
		 * for this partitioning key, search for its location in the attr list.
		 * (note that fields may be out of order).
		 */
		ListCell *cur;
		foreach (cur, cstate->attnumlist)
		{
			int attnum = lfirst_int(cur);

			if (attnum == policy->attrs[p_index])
			{
				if (i > cstate->last_hash_field)
					cstate->last_hash_field = i;
			}
			if (estate->es_result_partitions)
			{
				if (attnum == estate->es_partition_state->max_partition_attr)
				{
					if (i > cstate->last_hash_field)
						cstate->last_hash_field = i;
				}
			}
			i++;
		}
	}

	distData->policy = policy;
	distData->p_nattrs = p_nattrs;
	distData->p_attr_types = p_attr_types;
	distData->cdbHash = cdbHash;
	distData->hashmap = hashmap;

	return distData;
}

static void
FreeDistributionData(GpDistributionData *distData)
{
	if (distData)
	{
		pfree(distData->policy);
		pfree(distData->p_attr_types);
		if (distData->cdbHash)
		{
			pfree(distData->cdbHash);
		}
		if (distData->hashmap)
		{
			pfree(distData->hashmap);
		}
		pfree(distData);

	}
}

static void
InitPartitionData(PartitionData *partitionData, EState *estate, Form_pg_attribute *attr,
				  AttrNumber num_phys_attrs, MemoryContext ctxt)
{
	Datum *part_values = NULL;
	Oid *part_typio = NULL;
	FmgrInfo *part_infuncs = NULL;
	AttrNumber *part_attnum = NULL;
	int part_attnums = 0;

	PartitionNode *n = estate->es_result_partitions;
	MemoryContext cxt_save;

	List *pattnums = get_partition_attrs(n);
	ListCell *lc;
	int ii = 0;

	cxt_save = MemoryContextSwitchTo(ctxt);

	part_values = palloc0(num_phys_attrs * sizeof(Datum));
	part_typio = palloc(num_phys_attrs * sizeof(Oid));
	part_infuncs = palloc(num_phys_attrs * sizeof(FmgrInfo));
	part_attnum = palloc(num_phys_attrs * sizeof(AttrNumber));
	part_attnums = list_length(pattnums);
	MemoryContextSwitchTo(cxt_save);

	foreach (lc, pattnums)
	{
		AttrNumber attnum = (AttrNumber) lfirst_int(lc);
		Oid in_func_oid;

		getTypeInputInfo(attr[attnum - 1]->atttypid, &in_func_oid,
		                 &part_typio[attnum - 1]);
		fmgr_info(in_func_oid, &part_infuncs[attnum - 1]);
		part_attnum[ii++] = attnum;
	}
	partitionData->part_values = part_values;
	partitionData->part_typio = part_typio;
	partitionData->part_infuncs = part_infuncs;
	partitionData->part_attnum = part_attnum;
	partitionData->part_attnums = part_attnums;
}

static void
FreePartitionData(PartitionData *partitionData)
{
	if (partitionData)
	{
		if(partitionData->part_values)
		{
			pfree(partitionData->part_values);
			pfree(partitionData->part_typio);
			pfree(partitionData->part_infuncs);
			pfree(partitionData->part_attnum);
		}
		pfree(partitionData);
	}
}

/* Get distribution policy for specific part */
static GpDistributionData *
GetDistributionPolicyForPartition(CopyState cstate, EState *estate,
                                  PartitionData *partitionData, HTAB *hashmap,
                                  Oid *p_attr_types,
                                  GetAttrContext *getAttrContext,
                                  MemoryContext ctxt)
{
	ResultRelInfo *resultRelInfo;
	Datum *values_for_partition;
	GpPolicy *part_policy = NULL; /* policy for specific part */
	AttrNumber part_p_nattrs = 0; /* partition policy max attno */
	CdbHash *part_hash = NULL;
	int target_seg = 0; /* not used in attr_get_key function */

	if (!cstate->binary)
	{
		/*
		 * Text/CSV: Ensure we parse all partition attrs.
		 * Q: Wouldn't this potentially reparse values (and miss defaults)?
		 *    Why not merge with he other attr_get_key call
		 *    (replace part_values with values)?
		 */
		MemSet(partitionData->part_values, 0,
		       getAttrContext->num_phys_attrs * sizeof(Datum));
		attr_get_key(cstate, getAttrContext->cdbCopy,
		             getAttrContext->original_lineno_for_qe, target_seg,
		             partitionData->part_attnums, partitionData->part_attnum,
		             getAttrContext->attr, getAttrContext->attr_offsets,
		             getAttrContext->nulls, partitionData->part_infuncs,
		             partitionData->part_typio, partitionData->part_values);
		values_for_partition = partitionData->part_values;
	}
	else
	{
		/*
		 * Binary: We've made sure to parse partition attrs above.
		 */
		values_for_partition = getAttrContext->values;
	}

	/* values_get_partition() calls palloc() */
	MemoryContext save_cxt = MemoryContextSwitchTo(ctxt);
	GpDistributionData *distData = palloc(sizeof(GpDistributionData));
	distData->p_attr_types = p_attr_types;
	resultRelInfo = values_get_partition(values_for_partition,
	                                     getAttrContext->nulls,
	                                     getAttrContext->tupDesc, estate);
	MemoryContextSwitchTo(save_cxt);

	/*
	 * If we a partition set with differing policies,
	 * get the policy for this particular child partition.
	 */
	if (hashmap)
	{
		bool found;
		cdbhashdata *d;
		Oid relid = resultRelInfo->ri_RelationDesc->rd_id;

		d = hash_search(hashmap, &(relid), HASH_ENTER, &found);
		if (found)
		{
			part_policy = d->policy;
			part_p_nattrs = part_policy->nattrs;
			part_hash = d->cdbHash;
		}
		else
		{
			Relation rel = heap_open(relid, NoLock);
			MemoryContextSwitchTo(ctxt);

			/*
			 * Make sure this all persists the current
			 * iteration.
			 */
			d->relid = relid;
			part_hash = d->cdbHash = makeCdbHash(
			        getAttrContext->cdbCopy->total_segs);
			part_policy = d->policy = GpPolicyCopy(ctxt, rel->rd_cdbpolicy);
			part_p_nattrs = part_policy->nattrs;
			heap_close(rel, NoLock);
			MemoryContextSwitchTo(save_cxt);
		}
	}
	distData->policy = part_policy;
	distData->p_nattrs = part_p_nattrs;
	distData->cdbHash = part_hash;

	return distData;
}

static unsigned int
GetTargetSeg(GpDistributionData *distData, Datum *baseValues, bool *baseNulls)
{
	unsigned int target_seg = 0;
	CdbHash *cdbHash = distData->cdbHash;
	GpPolicy *policy = distData->policy; /* the partitioning policy for this table */
	AttrNumber p_nattrs = distData->p_nattrs; /* num of attributes in the distribution policy */
	Oid *p_attr_types = distData->p_attr_types;

	if (!policy)
	{
		elog(FATAL, "Bad or undefined policy. (%p)", policy);
	}

	/*
	 * At this point in the code, baseValues[x] is final for this
	 * data row -- either the input data, a null or a default
	 * value is in there, and constraints applied.
	 *
	 * Perform a cdbhash on this data row. Perform a hash operation
	 * on each attribute.
	 */
	Assert(PointerIsValid(cdbHash));
	/* Assert does not activate in production build */
	if (!cdbHash)
	{
		elog(FATAL, "Bad cdb_hash: %p", cdbHash);
	}
	cdbhashinit(cdbHash);

	AttrNumber h_attnum;
	Datum h_key;
	for (int i = 0; i < p_nattrs; i++)
	{
		/* current attno from the policy */
		h_attnum = policy->attrs[i];

		h_key = baseValues[h_attnum - 1]; /* value of this attr */
		if (!baseNulls[h_attnum - 1])
			cdbhash(cdbHash, h_key, p_attr_types[h_attnum - 1]);
		else
			cdbhashnull(cdbHash);
	}

	/*
	 * If this is a relation with an empty policy, there is no
	 * hash key to use, therefore use cdbhashnokey() to pick a
	 * hash value for us.
	 */
	if (p_nattrs == 0)
		cdbhashnokey(cdbHash);

	target_seg = cdbhashreduce(cdbHash); /* hash result segment */

	return target_seg;
}

static ProgramPipes*
open_program_pipes(char *command, bool forwrite)
{
	int save_errno;
	pqsigfunc save_SIGPIPE;
	/* set up extvar */
	extvar_t extvar;
	memset(&extvar, 0, sizeof(extvar));

	external_set_env_vars(&extvar, command, false, NULL, NULL, false, 0);

	ProgramPipes *program_pipes = palloc(sizeof(ProgramPipes));
	program_pipes->pid = -1;
	program_pipes->pipes[0] = -1;
	program_pipes->pipes[1] = -1;
	program_pipes->shexec = make_command(command, &extvar);

	/*
	 * Preserve the SIGPIPE handler and set to default handling.  This
	 * allows "normal" SIGPIPE handling in the command pipeline.  Normal
	 * for PG is to *ignore* SIGPIPE.
	 */
	save_SIGPIPE = pqsignal(SIGPIPE, SIG_DFL);

	program_pipes->pid = popen_with_stderr(program_pipes->pipes, program_pipes->shexec, forwrite);

	save_errno = errno;

	/* Restore the SIGPIPE handler */
	pqsignal(SIGPIPE, save_SIGPIPE);

	if (program_pipes->pid == -1)
	{
		errno = save_errno;
		pfree(program_pipes);
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
				 errmsg("can not start command: %s", command)));
	}

	return program_pipes;
}

static void
close_program_pipes(CopyState cstate, bool ifThrow)
{
	Assert(cstate->is_program);

	int ret = 0;
	StringInfoData sinfo;
	initStringInfo(&sinfo);

	if (cstate->copy_file)
	{
		fclose(cstate->copy_file);
		cstate->copy_file = NULL;
	}

	if (kill(cstate->program_pipes->pid, 0) == 0) /* process exists */
	{
		ret = pclose_with_stderr(cstate->program_pipes->pid, cstate->program_pipes->pipes, &sinfo);
	}

	if (ret == 0)
	{
		/* pclose() ended successfully; no errors to reflect */
		;
	}
	else if (ret == -1 && ifThrow)
	{
		/* pclose()/wait4() ended with an error; errno should be valid */
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("can not close pipe: %m")));
	}
	else if (ifThrow)
	{
		/*
		 * pclose() returned the process termination state.  The interpretExitCode() function
		 * generates a descriptive message from the exit code.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_SQL_ROUTINE_EXCEPTION),
				 errmsg("command error message: %s", sinfo.data)));
	}
}
