/*-------------------------------------------------------------------------
 *
 * copy.c
 *		Implements the COPY utility command
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/copy.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libpq-int.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <catalog/catalog.h>

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_type.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "optimizer/planner.h"
#include "nodes/makefuncs.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/portal.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"

#include "access/appendonlywriter.h"
#include "access/url.h"
#include "catalog/namespace.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbcopy.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "commands/queue.h"
#include "executor/execDML.h"
#include "nodes/makefuncs.h"
#include "postmaster/autostats.h"
#include "utils/metrics_utils.h"
#include "utils/resscheduler.h"
#include "utils/string_utils.h"


#define ISOCTAL(c) (((c) >= '0') && ((c) <= '7'))
#define OCTVALUE(c) ((c) - '0')


/*
 * These macros centralize code used to process line_buf and raw_buf buffers.
 * They are macros because they often do continue/break control and to avoid
 * function call overhead in tight COPY loops.
 *
 * We must use "if (1)" because the usual "do {...} while(0)" wrapper would
 * prevent the continue/break processing from working.  We end the "if (1)"
 * with "else ((void) 0)" to ensure the "if" does not unintentionally match
 * any "else" in the calling code, and to avoid any compiler warnings about
 * empty statements.  See http://www.cit.gu.edu.au/~anthony/info/C/C.macros.
 */

/*
 * This keeps the character read at the top of the loop in the buffer
 * even if there is more than one read-ahead.
 */
#define IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(extralen) \
if (1) \
{ \
	if (raw_buf_ptr + (extralen) >= copy_buf_len && !hit_eof) \
	{ \
		raw_buf_ptr = prev_raw_ptr; /* undo fetch */ \
		need_data = true; \
		continue; \
	} \
} else ((void) 0)

/* This consumes the remainder of the buffer and breaks */
#define IF_NEED_REFILL_AND_EOF_BREAK(extralen) \
if (1) \
{ \
	if (raw_buf_ptr + (extralen) >= copy_buf_len && hit_eof) \
	{ \
		if (extralen) \
			raw_buf_ptr = copy_buf_len; /* consume the partial character */ \
		/* backslash just before EOF, treat as data char */ \
		result = true; \
		break; \
	} \
} else ((void) 0)

/*
 * Transfer any approved data to line_buf; must do this to be sure
 * there is some room in raw_buf.
 */
#define REFILL_LINEBUF \
if (1) \
{ \
	if (raw_buf_ptr > cstate->raw_buf_index) \
	{ \
		appendBinaryStringInfo(&cstate->line_buf, \
							 cstate->raw_buf + cstate->raw_buf_index, \
							   raw_buf_ptr - cstate->raw_buf_index); \
		cstate->raw_buf_index = raw_buf_ptr; \
	} \
} else ((void) 0)

/* Undo any read-ahead and jump out of the block. */
#define NO_END_OF_COPY_GOTO \
if (1) \
{ \
	raw_buf_ptr = prev_raw_ptr + 1; \
	goto not_end_of_copy; \
} else ((void) 0)

static const char BinarySignature[11] = "PGCOPY\n\377\r\n\0";


/* non-export function prototypes */
static void EndCopy(CopyState cstate);
static CopyState BeginCopyTo(Relation rel, Node *query, const char *queryString, const Oid queryRelId,
					const char *filename, bool is_program, List *attnamelist,
					List *options, bool skip_ext_partition);
static void EndCopyTo(CopyState cstate, uint64 *processed);
static uint64 DoCopyTo(CopyState cstate);
static uint64 CopyToDispatch(CopyState cstate);
static uint64 CopyTo(CopyState cstate);
static uint64 CopyFrom(CopyState cstate);
static uint64 CopyDispatchOnSegment(CopyState cstate, const CopyStmt *stmt);
static uint64 CopyToQueryOnSegment(CopyState cstate);
static void CopyFromInsertBatch(CopyState cstate, EState *estate,
					CommandId mycid, int hi_options,
					ResultRelInfo *resultRelInfo, TupleTableSlot *myslot,
					BulkInsertState bistate,
					int nBufferedTuples, HeapTuple *bufferedTuples,
					uint64 firstBufferedLineNo);
static bool CopyReadLine(CopyState cstate);
static bool CopyReadLineText(CopyState cstate);
static int	CopyReadAttributesText(CopyState cstate, int stop_processing_at_field);
static int	CopyReadAttributesCSV(CopyState cstate, int stop_processing_at_field);
static Datum CopyReadBinaryAttribute(CopyState cstate,
						int column_no, FmgrInfo *flinfo,
						Oid typioparam, int32 typmod,
						bool *isnull, bool skip_parsing);
static void CopyAttributeOutText(CopyState cstate, char *string);
static void CopyAttributeOutCSV(CopyState cstate, char *string,
								bool use_quote, bool single_attr);

/* Low-level communications functions */
static void SendCopyBegin(CopyState cstate);
static void ReceiveCopyBegin(CopyState cstate);
static void SendCopyEnd(CopyState cstate);
static void CopySendData(CopyState cstate, const void *databuf, int datasize);
static void CopySendString(CopyState cstate, const char *str);
static void CopySendChar(CopyState cstate, char c);
static int  CopyGetData(CopyState cstate, void *databuf,
						int datasize);

static void CopySendInt32(CopyState cstate, int32 val);
static bool CopyGetInt32(CopyState cstate, int32 *val);
static void CopySendInt16(CopyState cstate, int16 val);
static bool CopyGetInt16(CopyState cstate, int16 *val);

static void SendCopyFromForwardedTuple(CopyState cstate,
						   CdbCopy *cdbCopy,
						   bool toAll,
						   int target_seg,
						   Relation rel,
						   int64 lineno,
						   char *line,
						   int line_len,
						   Oid tuple_oid,
						   Datum *values,
						   bool *nulls);
static void SendCopyFromForwardedHeader(CopyState cstate, CdbCopy *cdbCopy);
static void SendCopyFromForwardedError(CopyState cstate, CdbCopy *cdbCopy, char *errmsg);

static bool NextCopyFromDispatch(CopyState cstate, ExprContext *econtext,
								 Datum *values, bool *nulls, Oid *tupleOid);
static TupleTableSlot *NextCopyFromExecute(CopyState cstate, ExprContext *econtext, EState *estate,
								Oid *tupleOid);
static bool NextCopyFromRawFieldsX(CopyState cstate, char ***fields, int *nfields, int stop_processing_at_field);
static bool NextCopyFromX(CopyState cstate, ExprContext *econtext,
			  Datum *values, bool *nulls, Oid *tupleOid);
static void HandleCopyError(CopyState cstate);
static void HandleQDErrorFrame(CopyState cstate, char *p, int len);

static void CopyInitDataParser(CopyState cstate);
static void setEncodingConversionProc(CopyState cstate, int encoding, bool iswritable);

static GpDistributionData *InitDistributionData(CopyState cstate, EState *estate);
static void FreeDistributionData(GpDistributionData *distData);
static void InitCopyFromDispatchSplit(CopyState cstate, GpDistributionData *distData, EState *estate);
static Bitmapset *GetTargetKeyCols(Oid relid, PartitionNode *children, Bitmapset *needed_cols, bool distkeys, EState *estate);
static GpDistributionData *GetDistributionPolicyForPartition(GpDistributionData *mainDistData,
															 ResultRelInfo *resultRelInfo,
															 MemoryContext context);
static unsigned int
GetTargetSeg(GpDistributionData *distData, Datum *baseValues, bool *baseNulls);
static ProgramPipes *open_program_pipes(char *command, bool forwrite);
static void close_program_pipes(CopyState cstate, bool ifThrow);
static void cdbFlushInsertBatches(List *resultRels,
					  CopyState cstate,
					  EState *estate,
					  CommandId mycid,
					  int hi_options,
					  TupleTableSlot *baseSlot,
					  int firstBufferedLineNo);
CopyIntoClause*
MakeCopyIntoClause(CopyStmt *stmt);
static List *parse_joined_option_list(char *str, char *delimiter);

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

static volatile CopyState glob_cstate = NULL;


/* GPDB_91_MERGE_FIXME: passing through a global variable like this is ugly */
static CopyStmt *glob_copystmt = NULL;

/*
 * Testing GUC: When enabled, COPY FROM prints an INFO line to indicate which
 * fields are processed in the QD, and which in the QE.
 */
extern bool Test_copy_qd_qe_split;

/*
 * When doing a COPY FROM through the dispatcher, the QD reads the input from
 * the input file (or stdin or program), and forwards the data to the QE nodes,
 * where they will actually be inserted.
 *
 * Ideally, the QD would just pass through each line to the QE as is, and let
 * the QEs to do all the processing. Because the more processing the QD has
 * to do, the more likely it is to become a bottleneck.
 *
 * However, the QD needs to figure out which QE to send each row to. For that,
 * it needs to at least parse the distribution key. The distribution key might
 * also be a DEFAULTed column, in which case the DEFAULT value needs to be
 * evaluated in the QD. In that case, the QD must send the computed value
 * to the QE - we cannot assume that the QE can re-evaluate the expression and
 * arrive at the same value, at least not if the DEFAULT expression is volatile.
 *
 * Therefore, we need a flexible format between the QD and QE, where the QD
 * processes just enough of each input line to figure out where to send it.
 * It must send the values it had to parse and evaluate to the QE, as well
 * as the rest of the original input line, so that the QE can parse the rest
 * of it.
 *
 * The 'copy_from_dispatch_*' structs are used in the QD->QE stream. For each
 * input line, the QD constructs a 'copy_from_dispatch_row' struct, and sends
 * it to the QE. Before any rows, a QDtoQESignature is sent first, followed by
 * a 'copy_from_dispatch_header'. When QD encounters a recoverable error that
 * needs to be logged in the error log (LOG ERRORS SEGMENT REJECT LIMIT), it
 * sends the erroneous raw to a QE, in a 'copy_from_dispatch_error' struct.
 *
 *
 * COPY TO is simpler: The QEs form the output rows in the final form, and the QD
 * just collects and forwards them to the client. The QD doesn't need to parse
 * the rows at all.
 */
static const char QDtoQESignature[] = "PGCOPY-QD-TO-QE\n\377\r\n";

/* Header contains information that applies to all the rows that follow. */
typedef struct
{
	/*
	 * First field that should be processed in the QE. Any fields before
	 * this will be included as Datums in the rows that follow.
	 */
	int16		first_qe_processed_field;
	bool		file_has_oids;
} copy_from_dispatch_header;

typedef struct
{
	/*
	 * Information about this input line.
	 *
	 * 'relid' is the target relation's OID. Normally, the same as
	 * cstate->relid, but for a partitioned relation, it indicates the target
	 * partition. Note: this must be the first field, because InvalidOid means
	 * that this is actually a 'copy_from_dispatch_error' struct.
	 *
	 * 'lineno' is the input line number, for error reporting.
	 */
	int64		lineno;
	Oid			relid;

	uint32		line_len;			/* size of the included input line */
	uint32		residual_off;		/* offset in the line, where QE should
									 * process remaining fields */
	bool		delim_seen_at_end;  /* conveys to QE if QD saw a delim at end
									 * of its processing */
	uint16		fld_count;			/* # of fields that were processed in the
									 * QD. */

	/* If 'file_has_oids' was true, a tuple OID follows. */

	/* The input line follows. */

	/*
	 * For each field that was parsed in the QD already, the following data follows:
	 *
	 * int16	fieldnum;
	 * <data>
	 *
	 * NULL values are not included, any attributes that are not included in
	 * the message are implicitly NULL.
	 *
	 * For pass-by-value datatypes, the <data> is the raw Datum. For
	 * simplicity, it is always sent as a full-width 8-byte Datum, regardless
	 * of the datatype's length.
	 *
	 * For other fixed width datatypes, <data> is the datatype's value.
	 *
	 * For variable-length datatypes, <data> begins with a 4-byte length field,
	 * followed by the data. Cstrings (typlen = -2) are also sent in this
	 * format.
	 */
} copy_from_dispatch_row;

/* Size of the struct, without padding at the end. */
#define SizeOfCopyFromDispatchRow (offsetof(copy_from_dispatch_row, fld_count) + sizeof(uint16))

typedef struct
{
	int64		error_marker;	/* constant -1, to mark that this is an error
								 * frame rather than 'copy_from_dispatch_row' */
	int64		lineno;
	uint32		errmsg_len;
	uint32		line_len;
	bool		line_buf_converted;

	/* 'errmsg' follows */
	/* 'line' follows */
} copy_from_dispatch_error;

/* Size of the struct, without padding at the end. */
#define SizeOfCopyFromDispatchError (offsetof(copy_from_dispatch_error, line_buf_converted) + sizeof(bool))


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
		/* any error in old protocol will make us lose sync */
		pq_startmsgread();
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
		/* any error in old protocol will make us lose sync */
		pq_startmsgread();
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
	appendBinaryStringInfo(cstate->fe_msgbuf, databuf, datasize);
}

static void
CopySendString(CopyState cstate, const char *str)
{
	appendBinaryStringInfo(cstate->fe_msgbuf, str, strlen(str));
}

static void
CopySendChar(CopyState cstate, char c)
{
	appendStringInfoCharMacro(cstate->fe_msgbuf, c);
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

			if (fwrite(fe_msgbuf->data, fe_msgbuf->len, 1,
					   cstate->copy_file) != 1 ||
				ferror(cstate->copy_file))
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
		case COPY_CALLBACK:
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
		case COPY_CALLBACK:
			Insist(false); /* internal error */
			break;

	}

	resetStringInfo(fe_msgbuf);
}

/*
 * CopyGetData reads data from the source (file or frontend)
 *
 * Note: when copying from the frontend, we expect a proper EOF mark per
 * protocol; if the frontend simply drops the connection, we raise error.
 * It seems unwise to allow the COPY IN to complete normally in that case.
 *
 * NB: no data conversion is applied here.
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
						 errmsg("unexpected EOF on client connection with an open transaction")));
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
					HOLD_CANCEL_INTERRUPTS();
					pq_startmsgread();
					mtype = pq_getbyte();
					if (mtype == EOF)
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
								 errmsg("unexpected EOF on client connection with an open transaction")));
					if (pq_getmessage(cstate->fe_msgbuf, 0))
						ereport(ERROR,
								(errcode(ERRCODE_CONNECTION_FAILURE),
								 errmsg("unexpected EOF on client connection with an open transaction")));
					RESUME_CANCEL_INTERRUPTS();
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
		case COPY_CALLBACK:
			bytesread = cstate->data_source_cb(databuf, datasize, cstate->data_source_cb_extra);
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
 * CopyLoadRawBuf loads some more data into raw_buf
 *
 * Returns TRUE if able to obtain at least one more byte, else FALSE.
 *
 * If raw_buf_index < raw_buf_len, the unprocessed bytes are transferred
 * down to the start of the buffer and then we load more data after that.
 * This case is used only when a frontend multibyte character crosses a
 * bufferload boundary.
 */
static bool
CopyLoadRawBuf(CopyState cstate)
{
	int			nbytes;
	int			inbytes;

	if (cstate->raw_buf_index < cstate->raw_buf_len)
	{
		/* Copy down the unprocessed data */
		nbytes = cstate->raw_buf_len - cstate->raw_buf_index;
		memmove(cstate->raw_buf, cstate->raw_buf + cstate->raw_buf_index,
				nbytes);
	}
	else
		nbytes = 0;				/* no data need be saved */

	inbytes = CopyGetData(cstate, cstate->raw_buf + nbytes,
						  RAW_BUF_SIZE - nbytes);
	nbytes += inbytes;
	cstate->raw_buf[nbytes] = '\0';
	cstate->raw_buf_index = 0;
	cstate->raw_buf_len = nbytes;
	return (inbytes > 0);
}


/*
 *	 DoCopy executes the SQL COPY statement
 *
 * Either unload or reload contents of table <relation>, depending on <from>.
 * (<from> = TRUE means we are inserting into the table.)  In the "TO" case
 * we also support copying the output of an arbitrary SELECT, INSERT, UPDATE
 * or DELETE query.
 *
 * If <pipe> is false, transfer is between the table and the file named
 * <filename>.  Otherwise, transfer is between the table and our regular
 * input/output stream. The latter could be either stdin/stdout or a
 * socket, depending on whether we're running under Postmaster control.
 *
 * Do not allow a Postgres user without superuser privilege to read from
 * or write to a file.
 *
 * Do not allow the copy if user doesn't have proper permission to access
 * the table or the specifically requested columns.
 */
Oid
DoCopy(const CopyStmt *stmt, const char *queryString, uint64 *processed)
{
	CopyState	cstate;
	bool		is_from = stmt->is_from;
	bool		pipe = (stmt->filename == NULL || Gp_role == GP_ROLE_EXECUTE);
	Relation	rel;
	Oid			relid;
	Node	   *query = NULL;
	List	   *range_table = NIL;
	List	   *attnamelist = stmt->attlist;
	List	   *options;

	glob_cstate = NULL;
	glob_copystmt = (CopyStmt *) stmt;

	options = stmt->options;

	if (stmt->sreh && !is_from)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY single row error handling only available using COPY FROM")));

/* GPDB_91_MERGE_FIXME: this should probably be done earlier, e.g. in parser */
	/* Transfer any SREH options to the options list, so that BeginCopy can see them. */
	if (stmt->sreh)
	{
		SingleRowErrorDesc *sreh = (SingleRowErrorDesc *) stmt->sreh;

		options = list_copy(options);
		options = lappend(options, makeDefElem("sreh", (Node *) sreh));
	}

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

	if (stmt->relation)
	{
		TupleDesc	tupDesc;
		AclMode		required_access = (is_from ? ACL_INSERT : ACL_SELECT);
		List	   *attnums;
		ListCell   *cur;
		RangeTblEntry *rte;

		Assert(!stmt->query);

		/* Open and lock the relation, using the appropriate lock type. */
		rel = heap_openrv(stmt->relation,
						  (is_from ? RowExclusiveLock : AccessShareLock));

		if (is_from && !allowSystemTableMods && IsUnderPostmaster && IsSystemRelation(rel))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							errmsg("permission denied: \"%s\" is a system catalog",
								RelationGetRelationName(rel)),
								errhint("Make sure the configuration parameter allow_system_table_mods is set.")));
		}

		relid = RelationGetRelid(rel);

		rte = makeNode(RangeTblEntry);
		rte->rtekind = RTE_RELATION;
		rte->relid = RelationGetRelid(rel);
		rte->relkind = rel->rd_rel->relkind;
		rte->requiredPerms = required_access;
		range_table = list_make1(rte);

		tupDesc = RelationGetDescr(rel);
		attnums = CopyGetAttnums(tupDesc, rel, attnamelist);
		foreach (cur, attnums)
		{
			int			attno = lfirst_int(cur) -
			FirstLowInvalidHeapAttributeNumber;

			if (is_from)
				rte->insertedCols = bms_add_member(rte->insertedCols, attno);
			else
				rte->selectedCols = bms_add_member(rte->selectedCols, attno);
		}
		ExecCheckRTPerms(range_table, true);

		/*
		 * Permission check for row security policies.
		 *
		 * check_enable_rls will ereport(ERROR) if the user has requested
		 * something invalid and will otherwise indicate if we should enable
		 * RLS (returns RLS_ENABLED) or not for this COPY statement.
		 *
		 * If the relation has a row security policy and we are to apply it
		 * then perform a "query" copy and allow the normal query processing
		 * to handle the policies.
		 *
		 * If RLS is not enabled for this, then just fall through to the
		 * normal non-filtering relation handling.
		 */
		if (check_enable_rls(rte->relid, InvalidOid, false) == RLS_ENABLED)
		{
			SelectStmt *select;
			ColumnRef  *cr;
			ResTarget  *target;
			RangeVar   *from;

			if (is_from)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				   errmsg("COPY FROM not supported with row-level security"),
						 errhint("Use INSERT statements instead.")));

			/* Build target list */
			cr = makeNode(ColumnRef);

			if (!stmt->attlist)
				cr->fields = list_make1(makeNode(A_Star));
			else
				cr->fields = stmt->attlist;

			cr->location = 1;

			target = makeNode(ResTarget);
			target->name = NULL;
			target->indirection = NIL;
			target->val = (Node *) cr;
			target->location = 1;

			/*
			 * Build RangeVar for from clause, fully qualified based on the
			 * relation which we have opened and locked.
			 */
			from = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
								pstrdup(RelationGetRelationName(rel)),
								-1);

			/* Build query */
			select = makeNode(SelectStmt);
			select->targetList = list_make1(target);
			select->fromClause = list_make1(from);

			query = (Node *) select;

			/*
			 * Close the relation for now, but keep the lock on it to prevent
			 * changes between now and when we start the query-based COPY.
			 *
			 * We'll reopen it later as part of the query-based COPY.
			 */
			heap_close(rel, NoLock);
			rel = NULL;
		}
	}
	else
	{
		Assert(stmt->query);

		query = stmt->query;
		relid = InvalidOid;
		rel = NULL;
	}

	if (is_from)
	{
		Assert(rel);

		if (stmt->sreh && Gp_role != GP_ROLE_EXECUTE && !rel->rd_cdbpolicy)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY single row error handling only available for distributed user tables")));

		/*
		 * GPDB_91_MERGE_FIXME: is it possible to get to this point in the code
		 * with a temporary relation that belongs to another session? If so, the
		 * following code doesn't function as expected.
		 */
		/* check read-only transaction and parallel mode */
		if (XactReadOnly && !rel->rd_islocaltemp)
			PreventCommandIfReadOnly("COPY FROM");
		PreventCommandIfParallelMode("COPY FROM");

		cstate = BeginCopyFrom(rel, stmt->filename, stmt->is_program,
							   NULL, NULL, stmt->attlist, options);
		cstate->range_table = range_table;

		/*
		 * Error handling setup
		 */
		if (cstate->sreh)
		{
			/* Single row error handling requested */
			SingleRowErrorDesc *sreh = cstate->sreh;
			char		log_to_file = LOG_ERRORS_DISABLE;

			if (IS_LOG_TO_FILE(sreh->log_error_type))
			{
				cstate->errMode = SREH_LOG;
				/* LOG ERRORS PERSISTENTLY for COPY is not allowed for now. */
				log_to_file = LOG_ERRORS_ENABLE;
			}
			else
			{
				cstate->errMode = SREH_IGNORE;
			}
			cstate->cdbsreh = makeCdbSreh(sreh->rejectlimit,
										  sreh->is_limit_in_rows,
										  cstate->filename,
										  stmt->relation->relname,
										  log_to_file);
			if (rel)
				cstate->cdbsreh->relid = RelationGetRelid(rel);
		}
		else
		{
			/* No single row error handling requested. Use "all or nothing" */
			cstate->cdbsreh = NULL; /* default - no SREH */
			cstate->errMode = ALL_OR_NOTHING; /* default */
		}

		/* We must be a QE if we received the partitioning config */
		if (stmt->partitions)
		{
			Assert(Gp_role == GP_ROLE_EXECUTE);
			cstate->partitions = stmt->partitions;
		}

		PG_TRY();
		{
			if (Gp_role == GP_ROLE_DISPATCH && cstate->on_segment)
				*processed = CopyDispatchOnSegment(cstate, stmt);
			else
				*processed = CopyFrom(cstate);	/* copy from file to database */
		}
		PG_CATCH();
		{
			if (cstate->cdbCopy)
			{
				MemoryContext oldcontext = MemoryContextSwitchTo(cstate->copycontext);

				cdbCopyAbort(cstate->cdbCopy);
				cstate->cdbCopy = NULL;
				MemoryContextSwitchTo(oldcontext);
			}
			PG_RE_THROW();
		}
		PG_END_TRY();
		EndCopyFrom(cstate);
	}
	else
	{
		/*
		 * GPDB_91_MERGE_FIXME: ExecutorStart() is called in BeginCopyTo,
		 * but the TRY-CATCH block only starts here. If an error is
		 * thrown in-between, we would fail to call mppExecutorCleanup. We
		 * really should be using a ResourceOwner or something else for
		 * cleanup, instead of TRY-CATCH blocks...
		 *
		 * Update: I tried to fix this using the glob_cstate hack. It's ugly,
		 * but fixes at least some cases that came up in regression tests.
		 */
		PG_TRY();
		{
			cstate = BeginCopyTo(rel, query, queryString, relid,
								 stmt->filename, stmt->is_program,
								 stmt->attlist, options, stmt->skip_ext_partition);

			cstate->partitions = stmt->partitions;

			/*
			 * "copy t to file on segment"					CopyDispatchOnSegment
			 * "copy (select * from t) to file on segment"	CopyToQueryOnSegment
			 * "copy t/(select * from t) to file"			DoCopyTo
			 */
			if (Gp_role == GP_ROLE_DISPATCH && cstate->on_segment)
			{
				if (cstate->rel)
					*processed = CopyDispatchOnSegment(cstate, stmt);
				else
					*processed = CopyToQueryOnSegment(cstate);
			}
			else
				*processed = DoCopyTo(cstate);	/* copy from database to file */
		}
		PG_CATCH();
		{
			if (glob_cstate && glob_cstate->queryDesc)
			{
				/* should shutdown the mpp stuff such as interconnect and dispatch thread */
				mppExecutorCleanup(glob_cstate->queryDesc);
			}
			PG_RE_THROW();
		}
		PG_END_TRY();

		EndCopyTo(cstate, processed);
	}
	/*
	 * Close the relation.  If reading, we can release the AccessShareLock we
	 * got; if writing, we should hold the lock until end of transaction to
	 * ensure that updates will be committed before lock is released.
	 */
	if (rel != NULL)
		heap_close(rel, (is_from ? NoLock : AccessShareLock));

	/* Issue automatic ANALYZE if conditions are satisfied (MPP-4082). */
	if (Gp_role == GP_ROLE_DISPATCH && is_from)
		auto_stats(AUTOSTATS_CMDTYPE_COPY, relid, *processed, false /* inFunction */);

	return relid;
}

/*
 * Process the statement option list for COPY.
 *
 * Scan the options list (a list of DefElem) and transpose the information
 * into cstate, applying appropriate error checking.
 *
 * cstate is assumed to be filled with zeroes initially.
 *
 * This is exported so that external users of the COPY API can sanity-check
 * a list of options.  In that usage, cstate should be passed as NULL
 * (since external users don't know sizeof(CopyStateData)) and the collected
 * data is just leaked until CurrentMemoryContext is reset.
 *
 * Note that additional checking, such as whether column names listed in FORCE
 * QUOTE actually exist, has to be applied later.  This just checks for
 * self-consistency of the options list.
 */
void
ProcessCopyOptions(CopyState cstate,
				   bool is_from,
				   List *options,
				   int num_columns,
				   bool is_copy) /* false means external table */
{
	bool		format_specified = false;
	ListCell   *option;
	bool		delim_off = false;
	Oid			extprotocol_oid = InvalidOid;
	ExtTableEntry *exttbl = NULL;

	/* Support external use for option sanity checking */
	if (cstate == NULL)
		cstate = (CopyStateData *) palloc0(sizeof(CopyStateData));

	cstate->escape_off = false;
	cstate->file_encoding = -1;

	if (cstate->rel && rel_is_external_table(cstate->rel->rd_id))
		exttbl = GetExtTableEntry(cstate->rel->rd_id);

	if (exttbl && exttbl->urilocations)
	{
		char	   *location;
		char	   *protocol;
		Size		position;

		location = strVal(linitial(exttbl->urilocations));
		position = strchr(location, ':') - location;
		protocol = pnstrdup(location, position);
		extprotocol_oid = get_extprotocol_oid(protocol, true);
	}

	/* Extract options from the statement node tree */
	foreach(option, options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "format") == 0)
		{
			char	   *fmt = defGetString(defel);

			if (format_specified)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			format_specified = true;
			if (strcmp(fmt, "text") == 0)
				 /* default format */ ;
			else if (strcmp(fmt, "csv") == 0)
				cstate->csv_mode = true;
			else if (strcmp(fmt, "binary") == 0)
				cstate->binary = true;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("COPY format \"%s\" not recognized", fmt)));
		}
		else if (strcmp(defel->defname, "oids") == 0)
		{
			if (cstate->oids)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->oids = defGetBoolean(defel);
		}
		else if (strcmp(defel->defname, "freeze") == 0)
		{
			if (cstate->freeze)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->freeze = defGetBoolean(defel);
		}
		else if (strcmp(defel->defname, "delimiter") == 0)
		{
			if (cstate->delim)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->delim = defGetString(defel);

			if (cstate->delim && pg_strcasecmp(cstate->delim, "off") == 0)
				delim_off = true;
		}
		else if (strcmp(defel->defname, "null") == 0)
		{
			if (cstate->null_print)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->null_print = defGetString(defel);

			/*
			 * MPP-2010: unfortunately serialization function doesn't
			 * distinguish between 0x0 and empty string. Therefore we
			 * must assume that if NULL AS was indicated and has no value
			 * the actual value is an empty string.
			 */
			if(!cstate->null_print)
				cstate->null_print = "";
		}
		else if (strcmp(defel->defname, "header") == 0)
		{
			if (cstate->header_line)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->header_line = defGetBoolean(defel);
		}
		else if (strcmp(defel->defname, "quote") == 0)
		{
			if (cstate->quote)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->quote = defGetString(defel);
		}
		else if (strcmp(defel->defname, "escape") == 0)
		{
			if (cstate->escape)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->escape = defGetString(defel);
		}
		else if (strcmp(defel->defname, "force_quote") == 0)
		{
			if (cstate->force_quote || cstate->force_quote_all)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (defel->arg && IsA(defel->arg, A_Star))
				cstate->force_quote_all = true;
			else if (defel->arg && IsA(defel->arg, List))
				cstate->force_quote = (List *) defel->arg;
			else if (defel->arg && IsA(defel->arg, String))
			{
				if (strcmp(strVal(defel->arg), "*") == 0)
					cstate->force_quote_all = true;
				else
				{
					/* OPTIONS (force_quote 'c1,c2') */
					cstate->force_quote = parse_joined_option_list(strVal(defel->arg), ",");
				}
			}
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a list of column names",
								defel->defname)));
		}
		else if (strcmp(defel->defname, "force_not_null") == 0)
		{
			if (cstate->force_notnull)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (defel->arg && IsA(defel->arg, List))
				cstate->force_notnull = (List *) defel->arg;
			else if (defel->arg && IsA(defel->arg, String))
			{
				/* OPTIONS (force_not_null 'c1,c2') */
				cstate->force_notnull = parse_joined_option_list(strVal(defel->arg), ",");
			}
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a list of column names",
								defel->defname)));
		}
		else if (strcmp(defel->defname, "force_null") == 0)
		{
			if (cstate->force_null)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (defel->arg && IsA(defel->arg, List))
				cstate->force_null = (List *) defel->arg;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a list of column names",
								defel->defname)));
		}
		else if (strcmp(defel->defname, "convert_selectively") == 0)
		{
			/*
			 * Undocumented, not-accessible-from-SQL option: convert only the
			 * named columns to binary form, storing the rest as NULLs. It's
			 * allowed for the column list to be NIL.
			 */
			if (cstate->convert_selectively)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->convert_selectively = true;
			if (defel->arg == NULL || IsA(defel->arg, List))
				cstate->convert_select = (List *) defel->arg;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a list of column names",
								defel->defname)));
		}
		else if (strcmp(defel->defname, "encoding") == 0)
		{
			if (cstate->file_encoding >= 0)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->file_encoding = pg_char_to_encoding(defGetString(defel));
			if (cstate->file_encoding < 0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a valid encoding name",
								defel->defname)));
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
		else if (strcmp(defel->defname, "sreh") == 0)
		{
			if (defel->arg == NULL || !IsA(defel->arg, SingleRowErrorDesc))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("argument to option \"%s\" must be a list of column names",
								defel->defname)));
			if (cstate->sreh)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));

			cstate->sreh = (SingleRowErrorDesc *) defel->arg;
		}
		else if (strcmp(defel->defname, "on_segment") == 0)
		{
			if (cstate->on_segment)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			cstate->on_segment = TRUE;
		}
		else if (!extprotocol_oid)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("option \"%s\" not recognized", defel->defname)));
	}

	/*
	 * Check for incompatible options (must do these two before inserting
	 * defaults)
	 */
	if (cstate->binary && cstate->delim)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("COPY cannot specify DELIMITER in BINARY mode")));

	if (cstate->binary && cstate->null_print)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("COPY cannot specify NULL in BINARY mode")));

	cstate->eol_type = EOL_UNKNOWN;

	/* Set defaults for omitted options */
	if (!cstate->delim)
		cstate->delim = cstate->csv_mode ? "," : "\t";

	if (!cstate->null_print)
		cstate->null_print = cstate->csv_mode ? "" : "\\N";
	cstate->null_print_len = strlen(cstate->null_print);

	if (cstate->csv_mode)
	{
		if (!cstate->quote)
			cstate->quote = "\"";
		if (!cstate->escape)
			cstate->escape = cstate->quote;
	}

	if (!cstate->csv_mode && !cstate->escape)
		cstate->escape = "\\";			/* default escape for text mode */

	if (strchr(cstate->null_print, '\r') != NULL ||
		strchr(cstate->null_print, '\n') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY null representation cannot use newline or carriage return")));

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
	if (!cstate->csv_mode && !delim_off &&
		strchr("\\.abcdefghijklmnopqrstuvwxyz0123456789",
			   cstate->delim[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter cannot be \"%s\"", cstate->delim)));

	/* Check header */
	/*
	 * In PostgreSQL, HEADER is not allowed in text mode either, but in GPDB,
	 * only forbid it with BINARY.
	 */
	if (cstate->binary && cstate->header_line)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("COPY cannot specify HEADER in BINARY mode")));

	/* Check quote */
	if (!cstate->csv_mode && cstate->quote != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY quote available only in CSV mode")));

	if (cstate->csv_mode && strlen(cstate->quote) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY quote must be a single one-byte character")));

	if (cstate->csv_mode && cstate->delim[0] == cstate->quote[0] && !delim_off)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter and quote must be different")));

	/* Check escape */
	if (cstate->csv_mode && cstate->escape != NULL && strlen(cstate->escape) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY escape in CSV format must be a single character")));

	if (!cstate->csv_mode && cstate->escape != NULL &&
		(strchr(cstate->escape, '\r') != NULL ||
		strchr(cstate->escape, '\n') != NULL))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY escape representation in text format cannot use newline or carriage return")));

	if (!cstate->csv_mode && cstate->escape != NULL && strlen(cstate->escape) != 1)
	{
		if (pg_strcasecmp(cstate->escape, "off") != 0)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY escape must be a single character, or [OFF/off] to disable escapes")));
	}

	/* Check force_quote */
	if (!cstate->csv_mode && (cstate->force_quote || cstate->force_quote_all))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY force quote available only in CSV mode")));
	if ((cstate->force_quote || cstate->force_quote_all) && is_from)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY force quote only available using COPY TO")));

	/* Check force_notnull */
	if (!cstate->csv_mode && cstate->force_notnull != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY force not null available only in CSV mode")));
	if (cstate->force_notnull != NIL && !is_from)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			  errmsg("COPY force not null only available using COPY FROM")));

	/* Check force_null */
	if (!cstate->csv_mode && cstate->force_null != NIL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY force null available only in CSV mode")));

	if (cstate->force_null != NIL && !is_from)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY force null only available using COPY FROM")));

	/* Don't allow the CSV quote char to appear in the null string. */
	if (cstate->csv_mode &&
		strchr(cstate->null_print, cstate->quote[0]) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("CSV quote character must not appear in the NULL specification")));

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
		if (strlen(cstate->delim) != 1 && !delim_off)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY delimiter must be a single one-byte character, or \'off\'")));
	}
	else
	{
		/* multi byte encoding such as utf8 */
		if ((strlen(cstate->delim) != 1 || IS_HIGHBIT_SET(cstate->delim[0])) && !delim_off )
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY delimiter must be a single one-byte character, or \'off\'")));
	}

	/* Disallow end-of-line characters */
	if (strchr(cstate->delim, '\r') != NULL ||
		strchr(cstate->delim, '\n') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter cannot be newline or carriage return")));

	if (!cstate->csv_mode && strchr(cstate->delim, '\\') != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("COPY delimiter cannot be backslash")));

	if (strchr(cstate->null_print, cstate->delim[0]) != NULL && !delim_off)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("COPY delimiter must not appear in the NULL specification")));

	if (delim_off)
	{

		/*
		 * We don't support delimiter 'off' for COPY because the QD COPY
		 * sometimes internally adds columns to the data that it sends to
		 * the QE COPY modules, and it uses the delimiter for it. There
		 * are ways to work around this but for now it's not important and
		 * we simply don't support it.
		 */
		if (is_copy)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("using no delimiter is only supported for external tables")));

		if (num_columns != 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("using no delimiter is only possible for a single column table")));

	}


	/* Check header */
	if (cstate->header_line)
	{
		if (!is_copy && Gp_role == GP_ROLE_DISPATCH)
		{
			/* (exttab) */
			if (is_from)
			{
				/* RET */
				ereport(NOTICE,
						(errmsg("HEADER means that each one of the data files has a header row")));
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

	if (cstate->fill_missing && !is_from)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("fill missing fields only available for data loading, not unloading")));

	/*
	 * NEWLINE
	 */
	if (cstate->eol_str)
	{
		if (!is_from)
		{
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_YET),
					errmsg("newline currently available for data loading only, not unloading")));
		}
		else
		{
			if (pg_strcasecmp(cstate->eol_str, "lf") == 0)
				cstate->eol_type = EOL_NL;
			else if (pg_strcasecmp(cstate->eol_str, "cr") == 0)
				cstate->eol_type = EOL_CR;
			else if (pg_strcasecmp(cstate->eol_str, "crlf") == 0)
				cstate->eol_type = EOL_CRNL;
			else
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("invalid value for NEWLINE \"%s\"",
								cstate->eol_str),
						 errhint("Valid options are: 'LF', 'CRLF' and 'CR'.")));
		}
	}

	if (cstate->escape != NULL && pg_strcasecmp(cstate->escape, "off") == 0)
	{
		cstate->escape_off = true;
	}
}

/*
 * Common setup routines used by BeginCopyFrom and BeginCopyTo.
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
 */
CopyState
BeginCopy(bool is_from,
		  Relation rel,
		  Node *raw_query,
		  const char *queryString,
		  const Oid queryRelId,
		  List *attnamelist,
		  List *options,
		  TupleDesc tupDesc)
{
	CopyState	cstate;
	int			num_phys_attrs;
	MemoryContext oldcontext;
	bool is_copy = true;
	int num_columns = 0;

	/* Allocate workspace and zero all fields */
	cstate = (CopyStateData *) palloc0(sizeof(CopyStateData));

	glob_cstate = cstate;

	/*
	 * We allocate everything used by a cstate in a new memory context. This
	 * avoids memory leaks during repeated use of COPY in a query.
	 */
	cstate->copycontext = AllocSetContextCreate(CurrentMemoryContext,
												"COPY",
												ALLOCSET_DEFAULT_MINSIZE,
												ALLOCSET_DEFAULT_INITSIZE,
												ALLOCSET_DEFAULT_MAXSIZE);

	oldcontext = MemoryContextSwitchTo(cstate->copycontext);

	/*
	 * Since external scan calls BeginCopyFrom to init CopyStateData.
	 * Current relation may be an external relation.
	 */
	if (rel != NULL && rel_is_external_table(RelationGetRelid(rel)))
	{
		is_copy = false;
		num_columns = rel->rd_att->natts;
	}

	/* Greenplum needs this to detect custom protocol */
	if (rel)
		cstate->rel = rel;

	/* Extract options from the statement node tree */
	ProcessCopyOptions(cstate, is_from, options,
					   num_columns, /* pass correct value when COPY supports no delim */
					   is_copy);


	/* Process the source/target relation or query */
	if (rel)
	{
		Assert(!raw_query);

		tupDesc = RelationGetDescr(cstate->rel);

		/* Don't allow COPY w/ OIDs to or from a table without them */
		if (cstate->oids && !cstate->rel->rd_rel->relhasoids)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("table \"%s\" does not have OIDs",
							RelationGetRelationName(cstate->rel))));
	}
	else if(raw_query)
	{
		List	   *rewritten;
		Query	   *query;
		PlannedStmt *plan;
		DestReceiver *dest;

		Assert(!is_from);
		cstate->rel = NULL;

		/* Don't allow COPY w/ OIDs from a query */
		if (cstate->oids)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY (query) WITH OIDS is not supported")));

		/*
		 * Run parse analysis and rewrite.  Note this also acquires sufficient
		 * locks on the source table(s).
		 *
		 * Because the parser and planner tend to scribble on their input, we
		 * make a preliminary copy of the source querytree.  This prevents
		 * problems in the case that the COPY is in a portal or plpgsql
		 * function and is executed repeatedly.  (See also the same hack in
		 * DECLARE CURSOR and PREPARE.)  XXX FIXME someday.
		 */
		rewritten = pg_analyze_and_rewrite((Node *) copyObject(raw_query),
										   queryString, NULL, 0);

		/* check that we got back something we can work with */
		if (rewritten == NIL)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			 errmsg("DO INSTEAD NOTHING rules are not supported for COPY")));
		}
		else if (list_length(rewritten) > 1)
		{
			ListCell   *lc;

			/* examine queries to determine which error message to issue */
			foreach(lc, rewritten)
			{
				Query	   *q = (Query *) lfirst(lc);

				if (q->querySource == QSRC_QUAL_INSTEAD_RULE)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("conditional DO INSTEAD rules are not supported for COPY")));
				if (q->querySource == QSRC_NON_INSTEAD_RULE)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("DO ALSO rules are not supported for the COPY")));
			}

			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("multi-statement DO INSTEAD rules are not supported for COPY")));
		}

		query = (Query *) linitial(rewritten);


		if (cstate->on_segment && IsA(query, Query))
		{
			query->parentStmtType = PARENTSTMTTYPE_COPY;
		}
		/* Query mustn't use INTO, either */
		if (query->utilityStmt != NULL &&
			IsA(query->utilityStmt, CreateTableAsStmt))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY (SELECT INTO) is not supported")));

		Assert(query->utilityStmt == NULL);

		/*
		 * Similarly the grammar doesn't enforce the presence of a RETURNING
		 * clause, but this is required here.
		 */
		if (query->commandType != CMD_SELECT &&
			query->returningList == NIL)
		{
			Assert(query->commandType == CMD_INSERT ||
				   query->commandType == CMD_UPDATE ||
				   query->commandType == CMD_DELETE);

			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("COPY query must have a RETURNING clause")));
		}

		/* plan the query */
		plan = pg_plan_query(query, 0, NULL);

		/*
		 * With row level security and a user using "COPY relation TO", we
		 * have to convert the "COPY relation TO" to a query-based COPY (eg:
		 * "COPY (SELECT * FROM relation) TO"), to allow the rewriter to add
		 * in any RLS clauses.
		 *
		 * When this happens, we are passed in the relid of the originally
		 * found relation (which we have locked).  As the planner will look up
		 * the relation again, we double-check here to make sure it found the
		 * same one that we have locked.
		 */
		if (queryRelId != InvalidOid)
		{
			/*
			 * Note that with RLS involved there may be multiple relations,
			 * and while the one we need is almost certainly first, we don't
			 * make any guarantees of that in the planner, so check the whole
			 * list and make sure we find the original relation.
			 */
			if (!list_member_oid(plan->relationOids, queryRelId))
				ereport(ERROR,
						(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				errmsg("relation referenced by COPY statement has changed")));
		}

		/*
		 * Use a snapshot with an updated command ID to ensure this query sees
		 * results of any previously executed queries.
		 */
		PushCopiedSnapshot(GetActiveSnapshot());
		UpdateActiveSnapshotCommandId();

		/* Create dest receiver for COPY OUT */
		dest = CreateDestReceiver(DestCopyOut);
		((DR_copy *) dest)->cstate = cstate;

		/* Create a QueryDesc requesting no output */
		cstate->queryDesc = CreateQueryDesc(plan, queryString,
											GetActiveSnapshot(),
											InvalidSnapshot,
											dest, NULL,
											GP_INSTRUMENT_OPTS);
		if (cstate->on_segment)
			cstate->queryDesc->plannedstmt->copyIntoClause =
					MakeCopyIntoClause(glob_copystmt);

		/* GPDB hook for collecting query info */
		if (query_info_collect_hook)
			(*query_info_collect_hook)(METRICS_QUERY_SUBMIT, cstate->queryDesc);

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

	/* Convert FORCE_QUOTE name list to per-column flags, check validity */
	cstate->force_quote_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (cstate->force_quote_all)
	{
		int			i;

		for (i = 0; i < num_phys_attrs; i++)
			cstate->force_quote_flags[i] = true;
	}
	else if (cstate->force_quote)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_quote);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				   errmsg("FORCE_QUOTE column \"%s\" not referenced by COPY",
						  NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_quote_flags[attnum - 1] = true;
		}
	}

	/* Convert FORCE_NOT_NULL name list to per-column flags, check validity */
	cstate->force_notnull_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (cstate->force_notnull)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_notnull);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
				errmsg("FORCE_NOT_NULL column \"%s\" not referenced by COPY",
					   NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_notnull_flags[attnum - 1] = true;
		}
	}

	/* Convert FORCE_NULL name list to per-column flags, check validity */
	cstate->force_null_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));
	if (cstate->force_null)
	{
		List	   *attnums;
		ListCell   *cur;

		attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->force_null);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					errmsg("FORCE_NULL column \"%s\" not referenced by COPY",
						   NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->force_null_flags[attnum - 1] = true;
		}
	}

	/* Convert convert_selectively name list to per-column flags */
	if (cstate->convert_selectively)
	{
		List	   *attnums;
		ListCell   *cur;

		cstate->convert_select_flags = (bool *) palloc0(num_phys_attrs * sizeof(bool));

		attnums = CopyGetAttnums(tupDesc, cstate->rel, cstate->convert_select);

		foreach(cur, attnums)
		{
			int			attnum = lfirst_int(cur);

			if (!list_member_int(cstate->attnumlist, attnum))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						 errmsg_internal("selected column \"%s\" not referenced by COPY",
							 NameStr(tupDesc->attrs[attnum - 1]->attname))));
			cstate->convert_select_flags[attnum - 1] = true;
		}
	}

	/* Use client encoding when ENCODING option is not specified. */
	if (cstate->file_encoding < 0)
		cstate->file_encoding = pg_get_client_encoding();

	/*
	 * Set up encoding conversion info.  Even if the file and server encodings
	 * are the same, we must apply pg_any_to_server() to validate data in
	 * multibyte encodings.
	 *
	 * In COPY_EXECUTE mode, the dispatcher has already done the conversion.
	 */
	if (cstate->dispatch_mode != COPY_DISPATCH)
	{
		cstate->need_transcoding =
			((cstate->file_encoding != GetDatabaseEncoding() ||
			  pg_database_encoding_max_length() > 1));
		/* See Multibyte encoding comment above */
		cstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(cstate->file_encoding);
		setEncodingConversionProc(cstate, cstate->file_encoding, !is_from);
	}
	else
	{
		cstate->need_transcoding = false;
		cstate->encoding_embeds_ascii = PG_ENCODING_IS_CLIENT_ONLY(cstate->file_encoding);
	}

	cstate->copy_dest = COPY_FILE;	/* default */

	MemoryContextSwitchTo(oldcontext);

	return cstate;
}

/*
 * Dispatch a COPY ON SEGMENT statement to QEs.
 */
static uint64
CopyDispatchOnSegment(CopyState cstate, const CopyStmt *stmt)
{
	CopyStmt   *dispatchStmt;
	List	   *all_relids;
	CdbPgResults pgresults = {0};
	int			i;
	uint64		processed = 0;
	uint64		rejected = 0;
	dispatchStmt = copyObject((Node *) stmt);

	/* add in partitions for dispatch */
	dispatchStmt->partitions = RelationBuildPartitionDesc(cstate->rel, false);

	all_relids = list_make1_oid(RelationGetRelid(cstate->rel));

	/* add in AO segno map for dispatch */
	if (dispatchStmt->is_from)
	{
		if (rel_is_partitioned(RelationGetRelid(cstate->rel)))
		{
			PartitionNode *pn = RelationBuildPartitionDesc(cstate->rel, false);

			all_relids = list_concat(all_relids, all_partition_relids(pn));
		}
	}

	dispatchStmt->skip_ext_partition = cstate->skip_ext_partition;

	CdbDispatchUtilityStatement((Node *) dispatchStmt,
								DF_NEED_TWO_PHASE |
								DF_WITH_SNAPSHOT |
								DF_CANCEL_ON_ERROR,
								NIL,
								&pgresults);

	/*
	 * GPDB_91_MERGE_FIXME: SREH handling seems to be handled in a different
	 * place for every type of copy. This should be consolidated with the
	 * others.
	 */
	for (i = 0; i < pgresults.numResults; ++i)
	{
		struct pg_result *result = pgresults.pg_results[i];

		processed += result->numCompleted;
		rejected += result->numRejected;
	}

	if (rejected)
		ReportSrehResults(NULL, rejected);

	cdbdisp_clearCdbPgResults(&pgresults);
	return processed;
}


/*
 * Modify the filename in cstate->filename, and cstate->cdbsreh if any,
 * for COPY ON SEGMENT.
 *
 * Replaces the "<SEGID>" token in the filename with this segment's ID.
 */
static void
MangleCopyFileName(CopyState cstate)
{
	char	   *filename = cstate->filename;
	StringInfoData filepath;

	initStringInfo(&filepath);
	appendStringInfoString(&filepath, filename);

	replaceStringInfoString(&filepath, "<SEG_DATA_DIR>", DataDir);

	if (strstr(filename, "<SEGID>") == NULL)
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
}

/*
 * Release resources allocated in a cstate for COPY TO/FROM.
 */
static void
EndCopy(CopyState cstate)
{
	if (cstate->is_program)
	{
		close_program_pipes(cstate, true);
	}
	else
	{
		if (cstate->filename != NULL && FreeFile(cstate->copy_file))
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not close file \"%s\": %m",
							cstate->filename)));
	}

	/* Clean up single row error handling related memory */
	if (cstate->cdbsreh)
		destroyCdbSreh(cstate->cdbsreh);

	MemoryContextDelete(cstate->copycontext);
	pfree(cstate);
}

CopyIntoClause*
MakeCopyIntoClause(CopyStmt *stmt)
{
	CopyIntoClause *copyIntoClause;
	copyIntoClause = makeNode(CopyIntoClause);

	copyIntoClause->is_program = stmt->is_program;
	copyIntoClause->filename = stmt->filename;
	copyIntoClause->options = stmt->options;
	copyIntoClause->attlist = stmt->attlist;

	return copyIntoClause;
}

CopyState
BeginCopyToOnSegment(QueryDesc *queryDesc)
{
	CopyState	cstate;
	MemoryContext oldcontext;
	ListCell   *cur;

	TupleDesc	tupDesc;
	int			num_phys_attrs;
	Form_pg_attribute *attr;
	char	   *filename;
	CopyIntoClause *copyIntoClause;

	Assert(Gp_role == GP_ROLE_EXECUTE);

	copyIntoClause = queryDesc->plannedstmt->copyIntoClause;
	tupDesc = queryDesc->tupDesc;
	cstate = BeginCopy(false, NULL, NULL, NULL, InvalidOid, copyIntoClause->attlist,
					   copyIntoClause->options, tupDesc);
	oldcontext = MemoryContextSwitchTo(cstate->copycontext);

	cstate->null_print_client = cstate->null_print;		/* default */

	/* We use fe_msgbuf as a per-row buffer regardless of copy_dest */
	cstate->fe_msgbuf = makeStringInfo();

	cstate->filename = pstrdup(copyIntoClause->filename);
	cstate->is_program = copyIntoClause->is_program;

	if (cstate->on_segment)
		MangleCopyFileName(cstate);
	filename = cstate->filename;

	if (cstate->is_program)
	{
		cstate->program_pipes = open_program_pipes(cstate->filename, true);
		cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_W);

		if (cstate->copy_file == NULL)
			ereport(ERROR,
					(errmsg("could not execute command \"%s\": %m",
							cstate->filename)));
	}
	else
	{
		mode_t oumask; /* Pre-existing umask value */
		struct stat st;

		/*
		 * Prevent write to relative path ... too easy to shoot oneself in
		 * the foot by overwriting a database file ...
		 */
		if (!is_absolute_path(filename))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_NAME),
							errmsg("relative path not allowed for COPY to file")));

		oumask = umask(S_IWGRP | S_IWOTH);
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

	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
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
	 * anyway.  (We don't need a whole econtext as CopyFrom does.)
	 */
	cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "COPY TO",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	if (cstate->binary)
	{
		/* Generate header for a binary copy */
		int32		tmp;

		/* Signature */
		CopySendData(cstate, BinarySignature, 11);
		/* Flags field */
		tmp = 0;
		if (cstate->oids)
			tmp |= (1 << 16);
		CopySendInt32(cstate, tmp);
		/* No header extension */
		tmp = 0;
		CopySendInt32(cstate, tmp);
	}
	else
	{
		/* if a header has been requested send the line */
		if (cstate->header_line)
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

	MemoryContextSwitchTo(oldcontext);
	return cstate;
}

/*
 * Setup CopyState to read tuples from a table or a query for COPY TO.
 */
static CopyState
BeginCopyTo(Relation rel,
			Node *query,
			const char *queryString,
			const Oid queryRelId,
			const char *filename,
			bool is_program,
			List *attnamelist,
			List *options,
			bool skip_ext_partition)
{
	CopyState	cstate;
	MemoryContext oldcontext;

	if (rel != NULL && rel->rd_rel->relkind != RELKIND_RELATION)
	{
		if (rel->rd_rel->relkind == RELKIND_VIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from view \"%s\"",
							RelationGetRelationName(rel)),
					 errhint("Try the COPY (SELECT ...) TO variant.")));
		else if (rel->rd_rel->relkind == RELKIND_MATVIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from materialized view \"%s\"",
							RelationGetRelationName(rel)),
					 errhint("Try the COPY (SELECT ...) TO variant.")));
		else if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from foreign table \"%s\"",
							RelationGetRelationName(rel)),
					 errhint("Try the COPY (SELECT ...) TO variant.")));
		else if (rel->rd_rel->relkind == RELKIND_SEQUENCE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from sequence \"%s\"",
							RelationGetRelationName(rel))));
		else
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from non-table relation \"%s\"",
							RelationGetRelationName(rel))));
	}

	cstate = BeginCopy(false, rel, query, queryString, queryRelId, attnamelist,
					   options, NULL);
	oldcontext = MemoryContextSwitchTo(cstate->copycontext);

	cstate->skip_ext_partition = skip_ext_partition;

	/* Determine the mode */
	if (Gp_role == GP_ROLE_DISPATCH && !cstate->on_segment &&
		cstate->rel && cstate->rel->rd_cdbpolicy)
	{
		cstate->dispatch_mode = COPY_DISPATCH;
	}
	else
		cstate->dispatch_mode = COPY_DIRECT;

	if (rel != NULL && rel_has_external_partition(rel->rd_id))
	{
		if (!cstate->skip_ext_partition)
		{
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy from relation \"%s\" which has external partition(s)",
							RelationGetRelationName(rel)),
					 errhint("Try the COPY (SELECT ...) TO variant.")));
		}
		else
		{
			ereport(NOTICE,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("COPY ignores external partition(s)")));
		}
	}

	bool		pipe = (filename == NULL || (Gp_role == GP_ROLE_EXECUTE && !cstate->on_segment));

	if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
	{
		/* in ON SEGMENT mode, we don't open anything on the dispatcher. */

		if (filename == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("STDOUT is not supported by 'COPY ON SEGMENT'")));
	}
	else if (pipe)
	{
		Assert(!is_program || Gp_role == GP_ROLE_EXECUTE);	/* the grammar does not allow this */
		if (whereToSendOutput != DestRemote)
			cstate->copy_file = stdout;
	}
	else
	{
		cstate->filename = pstrdup(filename);
		cstate->is_program = is_program;

		if (cstate->on_segment)
			MangleCopyFileName(cstate);
		filename = cstate->filename;

		if (is_program)
		{
			cstate->program_pipes = open_program_pipes(cstate->filename, true);
			cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_W);

			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not execute command \"%s\": %m",
								cstate->filename)));
		}
		else
		{
			mode_t oumask; /* Pre-existing umask value */
			struct stat st;

			/*
			 * Prevent write to relative path ... too easy to shoot oneself in
			 * the foot by overwriting a database file ...
			 */
			if (!is_absolute_path(filename))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_NAME),
								errmsg("relative path not allowed for COPY to file")));

			oumask = umask(S_IWGRP | S_IWOTH);
			PG_TRY();
			{
				cstate->copy_file = AllocateFile(cstate->filename, PG_BINARY_W);
			}
			PG_CATCH();
			{
				umask(oumask);
				PG_RE_THROW();
			}
			PG_END_TRY();
			umask(oumask);
			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
								errmsg("could not open file \"%s\" for writing: %m", filename)));

			// Increase buffer size to improve performance  (cmcdevitt)
			setvbuf(cstate->copy_file, NULL, _IOFBF, 393216); // 384 Kbytes

			if (fstat(fileno(cstate->copy_file), &st))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m",
								cstate->filename)));

			if (S_ISDIR(st.st_mode))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
								errmsg("\"%s\" is a directory", filename)));
		}
	}

	MemoryContextSwitchTo(oldcontext);

	return cstate;
}

/*
 * Set up CopyState for writing to a foreign or external table.
 */
CopyState
BeginCopyToForeignTable(Relation forrel, List *options)
{
	CopyState	cstate;

	Assert(rel_is_external_table(RelationGetRelid(forrel)) || RelationIsForeign(forrel));

	cstate = BeginCopy(false, forrel, NULL, NULL, InvalidOid, NIL, options, NULL);

	cstate->dispatch_mode = COPY_DIRECT;

	/*
	 * We use COPY_CALLBACK to mean that the each line should be
	 * left in fe_msgbuf. There is no actual callback!
	 */
	cstate->copy_dest = COPY_CALLBACK;

	/*
	 * Some more initialization, that in the normal COPY TO codepath, is done
	 * in CopyTo() itself.
	 */
	cstate->null_print_client = cstate->null_print;		/* default */
	if (cstate->need_transcoding)
		cstate->null_print_client = pg_server_to_custom(cstate->null_print,
														cstate->null_print_len,
														cstate->file_encoding,
														cstate->enc_conversion_proc);

	return cstate;
}

/*
 * This intermediate routine exists mainly to localize the effects of setjmp
 * so we don't need to plaster a lot of variables with "volatile".
 */
static uint64
DoCopyTo(CopyState cstate)
{
	bool		pipe = (cstate->filename == NULL);
	bool		fe_copy = (pipe && whereToSendOutput == DestRemote);
	uint64		processed;

#ifdef FAULT_INJECTOR
	FaultInjector_InjectFaultIfSet("DoCopyToFail", DDLNotSpecified, "", "");
#endif

	PG_TRY();
	{
		if (fe_copy)
			SendCopyBegin(cstate);

		/*
		 * We want to dispatch COPY TO commands only in the case that
		 * we are the dispatcher and we are copying from a user relation
		 * (a relation where data is distributed in the segment databases).
		 * Otherwize, if we are not the dispatcher *or* if we are
		 * doing COPY (SELECT) we just go straight to work, without
		 * dispatching COPY commands to executors.
		 */
		if (Gp_role == GP_ROLE_DISPATCH && cstate->rel && cstate->rel->rd_cdbpolicy)
			processed = CopyToDispatch(cstate);
		else
			processed = CopyTo(cstate);

		if (fe_copy)
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

	return processed;
}

void EndCopyToOnSegment(CopyState cstate)
{
	Assert(Gp_role == GP_ROLE_EXECUTE);

	if (cstate->binary)
	{
		/* Generate trailer for a binary copy */
		CopySendInt16(cstate, -1);

		/* Need to flush out the trailer */
		CopySendEndOfRow(cstate);
	}

	MemoryContextDelete(cstate->rowcontext);

	EndCopy(cstate);
}

/*
 * Clean up storage and release resources for COPY TO.
 */
static void
EndCopyTo(CopyState cstate, uint64 *processed)
{
	if (cstate->queryDesc != NULL)
	{
		/* Close down the query and free resources. */
		ExecutorFinish(cstate->queryDesc);
		ExecutorEnd(cstate->queryDesc);
		if (cstate->queryDesc->es_processed > 0)
			*processed = cstate->queryDesc->es_processed;
		FreeQueryDesc(cstate->queryDesc);
		PopActiveSnapshot();
	}

	/* Clean up storage */
	EndCopy(cstate);
}

/*
 * Copy FROM relation TO file, in the dispatcher. Starts a COPY TO command on
 * each of the executors and gathers all the results and writes it out.
 */
static uint64
CopyToDispatch(CopyState cstate)
{
	CopyStmt   *stmt = glob_copystmt;
	TupleDesc	tupDesc;
	int			num_phys_attrs;
	int			attr_count;
	Form_pg_attribute *attr;
	CdbCopy    *cdbCopy;
	uint64		processed = 0;

	tupDesc = cstate->rel->rd_att;
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(cstate->attnumlist);

	/* We use fe_msgbuf as a per-row buffer regardless of copy_dest */
	cstate->fe_msgbuf = makeStringInfo();

	cdbCopy = makeCdbCopy(cstate, false);

	/* XXX: lock all partitions */

	/*
	 * Start a COPY command in every db of every segment in Greenplum Database.
	 *
	 * From this point in the code we need to be extra careful
	 * about error handling. ereport() must not be called until
	 * the COPY command sessions are closed on the executors.
	 * Calling ereport() will leave the executors hanging in
	 * COPY state.
	 */
	elog(DEBUG5, "COPY command sent to segdbs");

	PG_TRY();
	{
		bool		done;

		cdbCopyStart(cdbCopy, stmt,
					 RelationBuildPartitionDesc(cstate->rel, false),
					 cstate->file_encoding);

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
										cstate->file_encoding,
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
		do
		{
			bool		copy_cancel = (QueryCancelPending ? true : false);

			/* get a chunk of data rows from the QE's */
			done = cdbCopyGetData(cdbCopy, copy_cancel, &processed);

			/* send the chunk of data rows to destination (file or stdout) */
			if (cdbCopy->copy_out_buf.len > 0) /* conditional is important! */
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
		} while(!done);

		cdbCopyEnd(cdbCopy, NULL, NULL);

		/* now it's safe to destroy the whole dispatcher state */
		CdbDispatchCopyEnd(cdbCopy);
	}
    /* catch error from CopyStart, CopySendEndOfRow or CopyToDispatchFlush */
	PG_CATCH();
	{
		MemoryContext oldcontext = MemoryContextSwitchTo(cstate->copycontext);

		cdbCopyAbort(cdbCopy);

		MemoryContextSwitchTo(oldcontext);
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

	pfree(cdbCopy);

	return processed;
}

static uint64
CopyToQueryOnSegment(CopyState cstate)
{
	Assert(Gp_role != GP_ROLE_EXECUTE);

	/* run the plan --- the dest receiver will send tuples */
	ExecutorRun(cstate->queryDesc, ForwardScanDirection, 0L);
	return 0;
}

/*
 * Copy from relation or query TO file.
 */
static uint64
CopyTo(CopyState cstate)
{
	TupleDesc	tupDesc;
	int			num_phys_attrs;
	Form_pg_attribute *attr;
	ListCell   *cur;
	uint64		processed = 0;
	List	   *target_rels;
	ListCell *lc;

	if (cstate->rel)
		tupDesc = RelationGetDescr(cstate->rel);
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
	 * anyway.  (We don't need a whole econtext as CopyFrom does.)
	 */
	cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "COPY TO",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	if (!cstate->binary)
	{
		/*
		 * For non-binary copy, we need to convert null_print to file
		 * encoding, because it will be sent directly with CopySendString.
		 */
		if (cstate->need_transcoding)
			cstate->null_print_client = pg_server_to_custom(cstate->null_print,
															cstate->null_print_len,
															cstate->file_encoding,
															cstate->enc_conversion_proc);
	}

	if (Gp_role == GP_ROLE_EXECUTE && !cstate->on_segment)
	{
		/* header should not be printed in execute mode. */
	}
	else if (cstate->binary)
	{
		/* Generate header for a binary copy */
		int32		tmp;

		/* Signature */
		CopySendData(cstate, BinarySignature, 11);
		/* Flags field */
		tmp = 0;
		if (cstate->oids)
			tmp |= (1 << 16);
		CopySendInt32(cstate, tmp);
		/* No header extension */
		tmp = 0;
		CopySendInt32(cstate, tmp);
	}
	else
	{
		/* if a header has been requested send the line */
		if (cstate->header_line)
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

	if (cstate->partitions)
	{
		List	   *relids = all_partition_relids(cstate->partitions);

		target_rels = NIL;
		foreach(lc, relids)
		{
			Oid relid = lfirst_oid(lc);
			Relation rel = heap_open(relid, AccessShareLock);

			target_rels = lappend(target_rels, rel);
		}
	}
	else
		target_rels = list_make1(cstate->rel);

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
					processed++;
				}

				heap_endscan(scandesc);
			}
			else if (RelationIsAoRows(rel))
			{
				TupleTableSlot	*slot = MakeSingleTupleTableSlot(tupDesc);
				MemTupleBinding *mt_bind = create_memtuple_binding(tupDesc);

				aoscandesc = appendonly_beginscan(rel, GetActiveSnapshot(),
												  GetActiveSnapshot(), 0, NULL);

				while (appendonly_getnext(aoscandesc, ForwardScanDirection, slot))
				{
					MemTuple	tuple;
					Oid			tupleOid = InvalidOid;

					CHECK_FOR_INTERRUPTS();

					/* Extract all the values of the tuple */
					slot_getallattrs(slot);
					values = slot_get_values(slot);
					nulls = slot_get_isnull(slot);

					if (mtbind_has_oid(mt_bind))
					{
						tuple = TupGetMemTuple(slot);
						tupleOid = MemTupleGetOid(tuple, mt_bind);
					}

					/* Format and send the data */
					CopyOneRowTo(cstate, tupleOid, values, nulls);
					processed++;
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

				while (aocs_getnext(scan, ForwardScanDirection, slot))
				{
				    CHECK_FOR_INTERRUPTS();

				    slot_getallattrs(slot);
				    values = slot_get_values(slot);
				    nulls = slot_get_isnull(slot);

					CopyOneRowTo(cstate, InvalidOid, values, nulls);
					processed++;
				}

				ExecDropSingleTupleTableSlot(slot);
				aocs_endscan(scan);

				pfree(proj);
			}
			else if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
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
		processed = ((DR_copy *) cstate->queryDesc->dest)->processed;
	}

	if (Gp_role == GP_ROLE_EXECUTE && !cstate->on_segment)
	{
		/*
		 * Trailer should not be printed in execute mode. The dispatcher will
		 * write it once.
		 */
	}
	else if (cstate->binary)
	{
		/* Generate trailer for a binary copy */
		CopySendInt16(cstate, -1);

		/* Need to flush out the trailer */
		CopySendEndOfRow(cstate);
	}

	if (Gp_role == GP_ROLE_EXECUTE && cstate->on_segment)
		SendNumRows(0, processed);

	MemoryContextDelete(cstate->rowcontext);

	return processed;
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
	if (cstate->copy_dest != COPY_CALLBACK)
	{
		CopySendEndOfRow(cstate);
	}

	MemoryContextSwitchTo(oldcontext);
}

static char *
linenumber_atoi(char *buffer, size_t bufsz, int64 linenumber)
{
	if (linenumber < 0)
		snprintf(buffer, bufsz, "%s", "N/A");
	else
		snprintf(buffer, bufsz, INT64_FORMAT, linenumber);

	return buffer;
}

/*
 * error context callback for COPY FROM
 *
 * The argument for the error context must be CopyState.
 */
void
CopyFromErrorCallback(void *arg)
{
	CopyState	cstate = (CopyState) arg;
	char		buffer[20];

	if (cstate->binary)
	{
		/* can't usefully display the data */
		if (cstate->cur_attname)
			errcontext("COPY %s, line %s, column %s",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
					   cstate->cur_attname);
		else
			errcontext("COPY %s, line %s",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno));
	}
	else
	{
		if (cstate->cur_attname && cstate->cur_attval)
		{
			/* error is relevant to a particular column */
			char	   *attval;

			attval = limit_printout_length(cstate->cur_attval);
			errcontext("COPY %s, line %s, column %s: \"%s\"",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
					   cstate->cur_attname, attval);
			pfree(attval);
		}
		else if (cstate->cur_attname)
		{
			/* error is relevant to a particular column, value is NULL */
			errcontext("COPY %s, line %s, column %s: null input",
					   cstate->cur_relname,
					   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
					   cstate->cur_attname);
		}
		else
		{
			/*
			 * Error is relevant to a particular line.
			 *
			 * If line_buf still contains the correct line, and it's already
			 * transcoded, print it. If it's still in a foreign encoding, it's
			 * quite likely that the error is precisely a failure to do
			 * encoding conversion (ie, bad data). We dare not try to convert
			 * it, and at present there's no way to regurgitate it without
			 * conversion. So we have to punt and just report the line number.
			 */
			if (cstate->line_buf_valid &&
				(cstate->line_buf_converted || !cstate->need_transcoding))
			{
				char	   *lineval;

				lineval = limit_printout_length(cstate->line_buf.data);
				errcontext("COPY %s, line %s: \"%s\"",
						   cstate->cur_relname,
						   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno),
						   lineval);
				pfree(lineval);
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
						   linenumber_atoi(buffer, sizeof(buffer), cstate->cur_lineno));
			}
		}
	}
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

/*
 * Wrapper function of CopyFromInsertBatch
 *
 * Flush all relations have buffered tuples
 */
static void
cdbFlushInsertBatches(List *resultRels,
					  CopyState cstate,
					  EState *estate,
					  CommandId mycid,
					  int hi_options,
					  TupleTableSlot *baseSlot,
					  int firstBufferedLineNo)
{
	ListCell *cell;
	ResultRelInfo *oldRelInfo;
	oldRelInfo = estate->es_result_relation_info;
	foreach (cell, resultRels)
	{
		ResultRelInfo *relInfo = (ResultRelInfo *)lfirst(cell);
		if (relInfo->nBufferedTuples > 0)
		{
			estate->es_result_relation_info = relInfo;
			CopyFromInsertBatch(cstate, estate, mycid, hi_options,
								relInfo,
								baseSlot,
								relInfo->biState,
								relInfo->nBufferedTuples,
								relInfo->bufferedTuples,
								firstBufferedLineNo);
		}
		relInfo->nBufferedTuples = 0;
		relInfo->bufferedTuplesSize = 0;
	}
	estate->es_result_relation_info = oldRelInfo;
}

/*
 * Copy FROM file to relation.
 */
static uint64
CopyFrom(CopyState cstate)
{
	TupleDesc	tupDesc;
	AttrNumber	num_phys_attrs,
				attr_count;
	ResultRelInfo *resultRelInfo;
	ResultRelInfo *parentResultRelInfo;
	List *resultRelInfoList = NULL;
	EState	   *estate = CreateExecutorState(); /* for ExecConstraints() */
	ModifyTableState *mtstate;
	ExprContext *econtext;		/* used for ExecEvalExpr for default atts */
	TupleTableSlot *baseSlot;
	MemoryContext oldcontext = CurrentMemoryContext;

	ErrorContextCallback errcallback;
	CommandId	mycid = GetCurrentCommandId(true);
	int			hi_options = 0; /* start with default heap_insert options */
	CdbCopy    *cdbCopy = NULL;
	bool		is_check_distkey;
	GpDistributionData	*distData = NULL; /* distribution data used to compute target seg */
	uint64		processed = 0;
	bool		useHeapMultiInsert;
#define MAX_BUFFERED_TUPLES 1000
	int			nTotalBufferedTuples = 0;
	Size		totalBufferedTuplesSize = 0;
	int			i;
	Datum	   *baseValues;
	bool	   *baseNulls;
	GpDistributionData *part_distData = NULL;
	int			firstBufferedLineNo = 0;

	Assert(cstate->rel);

	/*
	 * The target must be a plain or foreign relation.
	 */
	if (cstate->rel->rd_rel->relkind != RELKIND_RELATION &&
		cstate->rel->rd_rel->relkind != RELKIND_FOREIGN_TABLE)
	{
		if (cstate->rel->rd_rel->relkind == RELKIND_VIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy to view \"%s\"",
							RelationGetRelationName(cstate->rel))));
		else if (cstate->rel->rd_rel->relkind == RELKIND_MATVIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot copy to materialized view \"%s\"",
							RelationGetRelationName(cstate->rel))));
		else if (cstate->rel->rd_rel->relkind == RELKIND_SEQUENCE)
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

	tupDesc = RelationGetDescr(cstate->rel);
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(cstate->attnumlist);

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
	 * is not always set correctly, since in rare cases rd_newRelfilenodeSubid
	 * can be cleared before the end of the transaction. The exact case is
	 * when a relation sets a new relfilenode twice in same transaction, yet
	 * the second one fails in an aborted subtransaction, e.g.
	 *
	 * BEGIN;
	 * TRUNCATE t;
	 * SAVEPOINT save;
	 * TRUNCATE t;
	 * ROLLBACK TO save;
	 * COPY ...
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
	/* createSubid is creation check, newRelfilenodeSubid is truncation check */
	if (cstate->rel->rd_createSubid != InvalidSubTransactionId ||
		cstate->rel->rd_newRelfilenodeSubid != InvalidSubTransactionId)
	{
		hi_options |= HEAP_INSERT_SKIP_FSM;
		/*
		 * The optimization to skip WAL has been disabled in GPDB. wal_level
		 * is hardcoded to 'archive' in GPDB, so it wouldn't have any effect
		 * anyway.
		 */
#if 0
		if (!XLogIsNeeded())
			hi_options |= HEAP_INSERT_SKIP_WAL;
#endif
	}

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * Optimize if new relfilenode was created in this subxact or one of its
	 * committed children and we won't see those rows later as part of an
	 * earlier scan or command. The subxact test ensures that if this subxact
	 * aborts then the frozen rows won't be visible after xact cleanup.  Note
	 * that the stronger test of exactly which subtransaction created it is
	 * crucial for correctness of this optimisation. The test for an earlier
	 * scan or command tolerates false negatives. FREEZE causes other sessions
	 * to see rows they would not see under MVCC, and a false negative merely
	 * spreads that anomaly to the current session.
	 */
	if (cstate->freeze)
	{
		/*
		 * Tolerate one registration for the benefit of FirstXactSnapshot.
		 * Scan-bearing queries generally create at least two registrations,
		 * though relying on that is fragile, as is ignoring ActiveSnapshot.
		 * Clear CatalogSnapshot to avoid counting its registration.  We'll
		 * still detect ongoing catalog scans, each of which separately
		 * registers the snapshot it uses.
		 */
		InvalidateCatalogSnapshot();
		if (!ThereAreNoPriorRegisteredSnapshots() || !ThereAreNoReadyPortals())
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TRANSACTION_STATE),
					 errmsg("cannot perform FREEZE because of prior transaction activity")));

		if (cstate->rel->rd_createSubid != GetCurrentSubTransactionId() &&
		 cstate->rel->rd_newRelfilenodeSubid != GetCurrentSubTransactionId())
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("cannot perform FREEZE because the table was not created or truncated in the current subtransaction")));

		hi_options |= HEAP_INSERT_FROZEN;
	}

	/*
	 * We need a ResultRelInfo so we can use the regular executor's
	 * index-entry-making machinery.  (There used to be a huge amount of code
	 * here that basically duplicated execUtils.c ...)
	 */
	resultRelInfo = makeNode(ResultRelInfo);
	InitResultRelInfo(resultRelInfo,
					  cstate->rel,
					  1,		/* dummy rangetable index */
					  0);
	ResultRelInfoChooseSegno(resultRelInfo);

	parentResultRelInfo = resultRelInfo;

	/* Verify the named relation is a valid target for INSERT */
	CheckValidResultRel(resultRelInfo->ri_RelationDesc, CMD_INSERT);

	ExecOpenIndices(resultRelInfo, false);

	resultRelInfo->ri_resultSlot = MakeSingleTupleTableSlot(resultRelInfo->ri_RelationDesc->rd_att);

	estate->es_result_relations = resultRelInfo;
	estate->es_num_result_relations = 1;
	estate->es_result_relation_info = resultRelInfo;
	estate->es_range_table = cstate->range_table;

	/*
	 * Look up partition hierarchy of the target table.
	 *
	 * In the QE, this is received from the QD, as part of the CopyStmt.
	 */
	if (cstate->dispatch_mode == COPY_DISPATCH)
		estate->es_result_partitions = RelationBuildPartitionDesc(cstate->rel, false);
	else
		estate->es_result_partitions = cstate->partitions;
	if (estate->es_result_partitions)
		estate->es_partition_state =
			createPartitionState(estate->es_result_partitions,
								 estate->es_num_result_relations);

	/* Set up a tuple slot too */
	baseSlot = ExecInitExtraTupleSlot(estate);
	ExecSetSlotDescriptor(baseSlot, tupDesc);
	/* Triggers might need a slot as well */
	estate->es_trig_tuple_slot = ExecInitExtraTupleSlot(estate);

	/*
	 * It's more efficient to prepare a bunch of tuples for insertion, and
	 * insert them in one heap_multi_insert() call, than call heap_insert()
	 * separately for every tuple. However, we can't do that if there are
	 * BEFORE/INSTEAD OF triggers, or we need to evaluate volatile default
	 * expressions. Such triggers or expressions might query the table we're
	 * inserting to, and act differently if the tuples that have already been
	 * processed and prepared for insertion are not there.  We also can't do
	 * it if the table is foreign.
	 */
	if ((resultRelInfo->ri_TrigDesc != NULL &&
		 (resultRelInfo->ri_TrigDesc->trig_insert_before_row ||
		  resultRelInfo->ri_TrigDesc->trig_insert_instead_row)) ||
		  resultRelInfo->ri_FdwRoutine != NULL ||
		cstate->volatile_defexprs || cstate->oids)
	{
		useHeapMultiInsert = false;
	}
	else
	{
		useHeapMultiInsert = true;
	}

	/*
	 * Set up a ModifyTableState so we can let FDW(s) init themselves for
	 * foreign-table result relation(s).
	 */
	mtstate = makeNode(ModifyTableState);
	mtstate->ps.plan = NULL;
	mtstate->ps.state = estate;
	mtstate->operation = CMD_INSERT;
	mtstate->resultRelInfo = estate->es_result_relations;

	if (resultRelInfo->ri_FdwRoutine != NULL &&
		resultRelInfo->ri_FdwRoutine->BeginForeignInsert != NULL)
		resultRelInfo->ri_FdwRoutine->BeginForeignInsert(mtstate,
														 resultRelInfo);

	/* Prepare to catch AFTER triggers. */
	AfterTriggerBeginQuery();

	/*
	 * Check BEFORE STATEMENT insertion triggers. It's debatable whether we
	 * should do this for COPY, since it's not really an "INSERT" statement as
	 * such. However, executing these triggers maintains consistency with the
	 * EACH ROW triggers that we already fire on COPY.
	 */
	ExecBSInsertTriggers(estate, resultRelInfo);

	econtext = GetPerTupleExprContext(estate);

	/* Set up callback to identify error line number */
	errcallback.callback = CopyFromErrorCallback;
	errcallback.arg = (void *) cstate;
	errcallback.previous = error_context_stack;
	error_context_stack = &errcallback;

	/*
	 * Do we need to check the distribution keys? Normally, the QD computes the
	 * target segment and sends the data to the correct segment. We don't need to
	 * verify that in the QE anymore. But in ON SEGMENT, we're reading data
	 * directly from a file, and there's no guarantee on what it contains, so we
	 * need to do the checking in the QE.
	 */
	is_check_distkey = (cstate->on_segment && Gp_role == GP_ROLE_EXECUTE && gp_enable_segment_copy_checking) ? true : false;

	/*
	 * Initialize information about distribution keys, needed to compute target
	 * segment for each row.
	 */
	if (cstate->dispatch_mode == COPY_DISPATCH || is_check_distkey)
	{
		distData = InitDistributionData(cstate, estate);

		/*
		 * If this table is distributed randomly, there is nothing to check,
		 * after all.
		 */
		if (!distData->hashmap &&
			(distData->policy == NULL || distData->policy->nattrs == 0))
			is_check_distkey = false;
	}

	/* Determine which fields we need to parse in the QD. */
	if (cstate->dispatch_mode == COPY_DISPATCH)
		InitCopyFromDispatchSplit(cstate, distData, estate);

	if (cstate->dispatch_mode == COPY_DISPATCH ||
		cstate->dispatch_mode == COPY_EXECUTOR)
	{
		/*
		 * Now split the attnumlist into the parts that are parsed in the QD, and
		 * in QE.
		 */
		ListCell   *lc;
		int			i = 0;
		List	   *qd_attnumlist = NIL;
		List	   *qe_attnumlist = NIL;
		int			first_qe_processed_field;

		first_qe_processed_field = cstate->first_qe_processed_field;
		if (cstate->file_has_oids)
			first_qe_processed_field--;

		foreach(lc, cstate->attnumlist)
		{
			int			attnum = lfirst_int(lc);

			if (i < first_qe_processed_field)
				qd_attnumlist = lappend_int(qd_attnumlist, attnum);
			else
				qe_attnumlist = lappend_int(qe_attnumlist, attnum);
			i++;
		}
		cstate->qd_attnumlist = qd_attnumlist;
		cstate->qe_attnumlist = qe_attnumlist;
	}

	if (cstate->dispatch_mode == COPY_DISPATCH)
	{
		/*
		 * We are the QD node, and we are receiving rows from client, or
		 * reading them from a file. We are not writing any data locally,
		 * instead, we determine the correct target segment for row,
		 * and forward each to the correct segment.
		 */

		/*
		 * pre-allocate buffer for constructing a message.
		 */
		cstate->dispatch_msgbuf = makeStringInfo();
		enlargeStringInfo(cstate->dispatch_msgbuf, SizeOfCopyFromDispatchRow);

		/*
		 * prepare to COPY data into segDBs:
		 * - set table partitioning information
		 * - set append only table relevant info for dispatch.
		 * - get the distribution policy for this table.
		 * - build a COPY command to dispatch to segdbs.
		 * - dispatch the modified COPY command to all segment databases.
		 * - prepare cdbhash for hashing on row values.
		 */
		cdbCopy = makeCdbCopy(cstate, true);

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
		elog(DEBUG5, "COPY command sent to segdbs");

		cdbCopyStart(cdbCopy, glob_copystmt,
					 estate->es_result_partitions, cstate->file_encoding);

		/*
		 * Skip header processing if dummy file get from master for COPY FROM ON
		 * SEGMENT
		 */
		if (!cstate->on_segment)
		{
			/*
			 * Send QD->QE header to all segments except:
			 * dummy file on master for COPY FROM ON SEGMENT
			 */
			if (!cstate->on_segment)
				SendCopyFromForwardedHeader(cstate, cdbCopy);
		}
	}

	CopyInitDataParser(cstate);

	for (;;)
	{
		TupleTableSlot *slot;
		bool		skip_tuple;
		Oid			loaded_oid = InvalidOid;
		unsigned int target_seg = 0;	/* result segment of cdbhash */

		CHECK_FOR_INTERRUPTS();

		if (nTotalBufferedTuples == 0)
		{
			/*
			 * Reset the per-tuple exprcontext. We can only do this if the
			 * tuple buffer is empty. (Calling the context the per-tuple
			 * memory context is a bit of a misnomer now.)
			 */
			ResetPerTupleExprContext(estate);
		}

		/* Switch into its memory context */
		MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

		/* Initialize all values for row to NULL */
		ExecClearTuple(baseSlot);
		baseValues = slot_get_values(baseSlot);
		baseNulls = slot_get_isnull(baseSlot);

		/*
		 * At this stage, we're dealing with tuples in the format of the parent
		 * table.
		 */
		estate->es_result_relation_info = parentResultRelInfo;

		if (cstate->dispatch_mode == COPY_EXECUTOR)
		{
			slot = NextCopyFromExecute(cstate, econtext, estate, &loaded_oid);
			if (slot == NULL)
				break;

			/*
			 * NextCopyFromExecute set up estate->es_result_relation_info,
			 * and stored the tuple in the correct slot.
			 */
			resultRelInfo = estate->es_result_relation_info;
		}
		else
		{
			if (cstate->dispatch_mode == COPY_DISPATCH)
			{
				if (!NextCopyFromDispatch(cstate, econtext, baseValues, baseNulls, &loaded_oid))
					break;
			}
			else
			{
				if (!NextCopyFrom(cstate, econtext, baseValues, baseNulls, &loaded_oid))
					break;
			}

			ExecStoreVirtualTuple(baseSlot);

			if (estate->es_result_partitions)
			{
				/*
				 * We might create a ResultRelInfo which needs to persist
				 * the per tuple context.
				 */
				bool		success;

				MemoryContextSwitchTo(estate->es_query_cxt);

				PG_TRY();
				{
					resultRelInfo = slot_get_partition(baseSlot,  estate, true);
					success = true;
				}
				PG_CATCH();
				{
					/* after all the prep work let cdbsreh do the real work */
					HandleCopyError(cstate);
					success = false;
				}
				PG_END_TRY();

				if (!success)
					continue;

				estate->es_result_relation_info = resultRelInfo;
			}

			/*
			 * And now we can form the input tuple.
			 *
			 * The resulting tuple is stored in 'slot'
			 */
			MemoryContextSwitchTo(estate->es_query_cxt);
			slot = reconstructPartitionTupleSlot(baseSlot, resultRelInfo);
			MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

			if (cstate->dispatch_mode == COPY_DISPATCH)
			{
				/* In QD, compute the target segment to send this row to. */
				part_distData = GetDistributionPolicyForPartition(
																  distData,
																  resultRelInfo,
																  cstate->copycontext);

				target_seg = GetTargetSeg(part_distData, slot_get_values(slot), slot_get_isnull(slot));
			}
			else if (is_check_distkey)
			{
				/*
				 * In COPY FROM ON SEGMENT, check the distribution key in the
				 * QE.
				 */
				part_distData = GetDistributionPolicyForPartition(
																  distData,
																  resultRelInfo,
																  cstate->copycontext);

				if (part_distData->policy->nattrs != 0)
				{
					target_seg = GetTargetSeg(part_distData, slot_get_values(slot), slot_get_isnull(slot));

					if (GpIdentity.segindex != target_seg)
					{
						PG_TRY();
						{
							ereport(ERROR,
									(errcode(ERRCODE_INTEGRITY_CONSTRAINT_VIOLATION),
									 errmsg("value of distribution key doesn't belong to segment with ID %d, it belongs to segment with ID %d",
											GpIdentity.segindex, target_seg)));
						}
						PG_CATCH();
						{
							HandleCopyError(cstate);
						}
						PG_END_TRY();
					}
				}
			}
		}

		/*
		 * Triggers and stuff need to be invoked in query context.
		 */
		MemoryContextSwitchTo(estate->es_query_cxt);

		/* 'slot' was set above already */

		/* Partitions don't support triggers yet */
		Assert(!(estate->es_result_partitions &&
				 resultRelInfo->ri_TrigDesc));

		skip_tuple = false;

		/*
		 * Initialize "insertion desc" if the target requires that. Note that
		 * we do this (also) in the QD, even though all the data will be
		 * inserted in the QEs, because we nevertheless need to create the
		 * pg_aoseg rows in the QD.
		 */
		{
			char		relstorage;

			relstorage = RelinfoGetStorage(resultRelInfo);
			if (relstorage == RELSTORAGE_AOROWS &&
				resultRelInfo->ri_aoInsertDesc == NULL)
			{
				ResultRelInfoChooseSegno(resultRelInfo);
				resultRelInfo->ri_aoInsertDesc =
					appendonly_insert_init(resultRelInfo->ri_RelationDesc,
										   resultRelInfo->ri_aosegno, false);
			}
			else if (relstorage == RELSTORAGE_AOCOLS &&
					 resultRelInfo->ri_aocsInsertDesc == NULL)
			{
				ResultRelInfoChooseSegno(resultRelInfo);
				resultRelInfo->ri_aocsInsertDesc =
					aocs_insert_init(resultRelInfo->ri_RelationDesc,
									 resultRelInfo->ri_aosegno, false);
			}
		}

		if (cstate->dispatch_mode == COPY_DISPATCH)
		{
			bool send_to_all = part_distData &&
							   GpPolicyIsReplicated(part_distData->policy);

			/* in the QD, forward the row to the correct segment(s). */
			SendCopyFromForwardedTuple(cstate, cdbCopy, send_to_all,
									   send_to_all ? 0 : target_seg,
									   resultRelInfo->ri_RelationDesc,
									   cstate->cur_lineno,
									   cstate->line_buf.data,
									   cstate->line_buf.len,
									   loaded_oid,
									   slot_get_values(slot),
									   slot_get_isnull(slot));
			skip_tuple = true;
			processed++;
		}

		/* BEFORE ROW INSERT Triggers */
		if (!skip_tuple &&
			resultRelInfo->ri_TrigDesc &&
			resultRelInfo->ri_TrigDesc->trig_insert_before_row)
		{
			slot = ExecBRInsertTriggers(estate, resultRelInfo, slot);

			if (slot == NULL)	/* "do nothing" */
				skip_tuple = true;
			else	/* trigger might have changed tuple */
			{
				/*
				 * nothing to do in GPDB, since we extract the right kind of
				 * tuple from the slot only later.
				 */
			}
		}

		if (!skip_tuple)
		{
			char		relstorage = RelinfoGetStorage(resultRelInfo);
			ItemPointerData insertedTid;

			/* Check the constraints of the tuple */
			if (resultRelInfo->ri_FdwRoutine == NULL &&
				resultRelInfo->ri_RelationDesc->rd_att->constr)
				ExecConstraints(resultRelInfo, slot, estate);

			/* OK, store the tuple and create index entries for it */
			if (useHeapMultiInsert && relstorage == RELSTORAGE_HEAP)
			{
				HeapTuple	tuple;
				if (resultRelInfo->nBufferedTuples == 0)
					firstBufferedLineNo = cstate->cur_lineno;

				resultRelInfoList = list_append_unique_ptr(resultRelInfoList, resultRelInfo);
				if (resultRelInfo->bufferedTuples == NULL)
				{
					resultRelInfo->bufferedTuples = palloc(MAX_BUFFERED_TUPLES * sizeof(HeapTuple));
					resultRelInfo->nBufferedTuples = 0;
					resultRelInfo->bufferedTuplesSize = 0;
				}

				MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
				/* Add this tuple to the tuple buffer */
				tuple = ExecCopySlotHeapTuple(slot);
				resultRelInfo->bufferedTuples[resultRelInfo->nBufferedTuples++] = tuple;
				resultRelInfo->bufferedTuplesSize += tuple->t_len;
				nTotalBufferedTuples++;
				totalBufferedTuplesSize += tuple->t_len;
				MemoryContextSwitchTo(estate->es_query_cxt);

				/*
				 * If the buffer filled up, flush it. Also flush if the total
				 * size of all the tuples in the buffer becomes large, to
				 * avoid using large amounts of memory for the buffers when
				 * the tuples are exceptionally wide.
				 */
				/*
				 * GPDB_92_MERGE_FIXME: MAX_BUFFERED_TUPLES and 65535 might not be
				 * the best value for partition table
				 */
				if (nTotalBufferedTuples == MAX_BUFFERED_TUPLES ||
					totalBufferedTuplesSize > 65535)
				{
					cdbFlushInsertBatches(resultRelInfoList, cstate, estate, mycid, hi_options,
										  slot, firstBufferedLineNo);
					nTotalBufferedTuples = 0;
					totalBufferedTuplesSize = 0;
				}
			}
			else
			{
				List	   *recheckIndexes = NIL;

				if (relstorage == RELSTORAGE_AOROWS)
				{
					MemTuple	mtuple;

					mtuple = ExecFetchSlotMemTuple(slot);

					/* inserting into an append only relation */
					appendonly_insert(resultRelInfo->ri_aoInsertDesc, mtuple, loaded_oid,
									  (AOTupleId *) &insertedTid);
				}
				else if (relstorage == RELSTORAGE_AOCOLS)
				{
					aocs_insert(resultRelInfo->ri_aocsInsertDesc, slot);
					insertedTid = *slot_get_ctid(slot);
				}
				else if (resultRelInfo->ri_FdwRoutine != NULL)
				{
					HeapTuple tuple;

					slot = resultRelInfo->ri_FdwRoutine->ExecForeignInsert(estate,
																		   resultRelInfo,
																		   slot,
																		   NULL);

					if (slot == NULL)		/* "do nothing" */
						continue;

					/*
					 * AFTER ROW Triggers might reference the tableoid
					 * column, so (re-)initialize tts_tableOid before
					 * evaluating them.
					 */
					slot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

					/* FDW might have changed tuple */
					tuple = ExecMaterializeSlot(slot);
					ItemPointerSetInvalid(&insertedTid);
				}
				else
				{
					HeapTuple tuple;
					tuple = ExecFetchSlotHeapTuple(slot);

					if (cstate->file_has_oids)
						HeapTupleSetOid(tuple, loaded_oid);
					/* OK, store the tuple and create index entries for it */
					heap_insert(resultRelInfo->ri_RelationDesc, tuple, mycid, hi_options,
								resultRelInfo->biState,
								GetCurrentTransactionId());
					insertedTid = tuple->t_self;
				}

				if (resultRelInfo->ri_NumIndices > 0)
					recheckIndexes = ExecInsertIndexTuples(slot, &insertedTid,
														   estate, false, NULL,
														   NIL);

				/* AFTER ROW INSERT Triggers */
				if (resultRelInfo->ri_TrigDesc &&
					resultRelInfo->ri_TrigDesc->trig_insert_after_row)
				{
					HeapTuple tuple;

					tuple = ExecFetchSlotHeapTuple(slot);
					ExecARInsertTriggers(estate, resultRelInfo, tuple,
										 recheckIndexes);
				}

				list_free(recheckIndexes);
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
			processed++;
			if (cstate->cdbsreh)
				cstate->cdbsreh->processed++;
		}
	}

	/*
	 * After processed data from QD, which is empty and just for workflow, now
	 * to process the data on segment, only one shot if cstate->on_segment &&
	 * Gp_role == GP_ROLE_DISPATCH
	 */
	if (cstate->on_segment && Gp_role == GP_ROLE_EXECUTE)
	{
		CopyInitDataParser(cstate);
	}
	elog(DEBUG1, "Segment %u, Copied %lu rows.", GpIdentity.segindex, processed);
	/* Flush any remaining buffered tuples */
	if (useHeapMultiInsert)
		cdbFlushInsertBatches(resultRelInfoList, cstate, estate, mycid, hi_options,
							  baseSlot, firstBufferedLineNo);

	/* Done, clean up */
	error_context_stack = errcallback.previous;

	MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * Done reading input data and sending it off to the segment
	 * databases Now we would like to end the copy command on
	 * all segment databases across the cluster.
	 */
	if (cstate->dispatch_mode == COPY_DISPATCH)
	{
		int64		total_completed_from_qes;
		int64		total_rejected_from_qes;

		cdbCopyEnd(cdbCopy,
				   &total_completed_from_qes,
				   &total_rejected_from_qes);
		if (cstate->cdbsreh)
		{
			/* emit a NOTICE with number of rejected rows */
			uint64		total_rejected = 0;
			uint64		total_rejected_from_qd = cstate->cdbsreh->rejectcount;

			/*
			 * If error log has been requested, then we send the row to the segment
			 * so that it can be written in the error log file. The segment process
			 * counts it again as a rejected row. So we ignore the reject count
			 * from the master and only consider the reject count from segments.
			 */
			if (IS_LOG_TO_FILE(cstate->cdbsreh->logerrors))
				total_rejected_from_qd = 0;

			total_rejected = total_rejected_from_qd + total_rejected_from_qes;

			ReportSrehResults(cstate->cdbsreh, total_rejected);
		}
	}

	/*
	 * In the old protocol, tell pqcomm that we can process normal protocol
	 * messages again.
	 */
	if (cstate->copy_dest == COPY_OLD_FE)
		pq_endmsgread();

	/*
	 * In the old protocol, tell pqcomm that we can process normal protocol
	 * messages again.
	 */
	if (cstate->copy_dest == COPY_OLD_FE)
		pq_endmsgread();

	/* Execute AFTER STATEMENT insertion triggers */
	ExecASInsertTriggers(estate, resultRelInfo);

	/* Handle queued AFTER triggers */
	AfterTriggerEndQuery(estate);

	/*
	 * If SREH and in executor mode send the number of rejected
	 * rows to the client (QD COPY).
	 * If COPY ... FROM/TO ... ON SEGMENT, then we need to send the number of
	 * completed rows as well.
	 */
	if ((cstate->errMode != ALL_OR_NOTHING && cstate->dispatch_mode == COPY_EXECUTOR)
		|| cstate->on_segment)
	{
		SendNumRows((cstate->errMode != ALL_OR_NOTHING) ? cstate->cdbsreh->rejectcount : 0,
				cstate->on_segment ? processed : 0);
	}

	/* update AO tuple counts */
	if (cstate->dispatch_mode == COPY_DISPATCH)
	{
		for (i = estate->es_num_result_relations - 1; i >= 0; i--)
		{
			resultRelInfo = &estate->es_result_relations[i];

			if (relstorage_is_ao(RelinfoGetStorage(resultRelInfo)))
			{
				int64 tupcount;

				tupcount = processed;

				/* find out which segnos the result rels in the QE's used */
				ResultRelInfoChooseSegno(resultRelInfo);

				if (resultRelInfo->ri_aoInsertDesc)
					resultRelInfo->ri_aoInsertDesc->insertCount += tupcount;
				if (resultRelInfo->ri_aocsInsertDesc)
					resultRelInfo->ri_aocsInsertDesc->insertCount += tupcount;
			}
		}
	}

	/* NB: do not pfree baseValues/baseNulls and partValues/partNulls here, since
	 * there may be duplicate free in ExecDropSingleTupleTableSlot; if not, they
	 * would be freed by FreeExecutorState anyhow */
	ExecResetTupleTable(estate->es_tupleTable, false);

	/* Allow the FDW to shut down */
	if (parentResultRelInfo->ri_FdwRoutine != NULL &&
		parentResultRelInfo->ri_FdwRoutine->EndForeignInsert != NULL)
		parentResultRelInfo->ri_FdwRoutine->EndForeignInsert(estate,
															 parentResultRelInfo);

	/*
	 * Finalize appends and close relations we opened.
	 *
	 * The main target relation is included in the array, but we want to keep
	 * that open, and let the caller close it. Increment the refcount so
	 * that it's still open, even though we close it in the loop.
	 */
	RelationIncrementReferenceCount(cstate->rel);
	resultRelInfo = estate->es_result_relations;
	for (i = estate->es_num_result_relations; i > 0; i--)
	{
		CloseResultRelInfo(resultRelInfo);
		resultRelInfo++;
	}

	MemoryContextSwitchTo(oldcontext);

	FreeDistributionData(distData);

	FreeExecutorState(estate);

	/*
	 * If we skipped writing WAL, then we need to sync the heap (but not
	 * indexes since those use WAL anyway)
	 */
	if (hi_options & HEAP_INSERT_SKIP_WAL)
	{
		/* disabled in GPDB. */
		elog(ERROR, "unexpected SKIP-WAL flag set");
	}

	return processed;
}

/*
 * A subroutine of CopyFrom, to write the current batch of buffered heap
 * tuples to the heap. Also updates indexes and runs AFTER ROW INSERT
 * triggers.
 */
static void
CopyFromInsertBatch(CopyState cstate, EState *estate, CommandId mycid,
					int hi_options, ResultRelInfo *resultRelInfo,
					TupleTableSlot *myslot, BulkInsertState bistate,
					int nBufferedTuples, HeapTuple *bufferedTuples,
					uint64 firstBufferedLineNo)
{
	MemoryContext oldcontext;
	int			i;
	uint64		save_cur_lineno;

	/*
	 * Print error context information correctly, if one of the operations
	 * below fail.
	 */
	cstate->line_buf_valid = false;
	save_cur_lineno = cstate->cur_lineno;

	/*
	 * heap_multi_insert leaks memory, so switch to short-lived memory context
	 * before calling it.
	 */
	oldcontext = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
	heap_multi_insert(resultRelInfo->ri_RelationDesc,
					  bufferedTuples,
					  nBufferedTuples,
					  mycid,
					  hi_options,
					  bistate,
					  GetCurrentTransactionId());
	MemoryContextSwitchTo(oldcontext);

	/*
	 * If there are any indexes, update them for all the inserted tuples, and
	 * run AFTER ROW INSERT triggers.
	 */
	if (resultRelInfo->ri_NumIndices > 0)
	{
		for (i = 0; i < nBufferedTuples; i++)
		{
			List	   *recheckIndexes;

			cstate->cur_lineno = firstBufferedLineNo + i;
			ExecStoreHeapTuple(bufferedTuples[i], myslot, InvalidBuffer, false);
			recheckIndexes =
				ExecInsertIndexTuples(myslot, &(bufferedTuples[i]->t_self),
									  estate, false, NULL, NIL);
			ExecARInsertTriggers(estate, resultRelInfo,
								 bufferedTuples[i],
								 recheckIndexes);
			list_free(recheckIndexes);
		}
	}

	/*
	 * There's no indexes, but see if we need to run AFTER ROW INSERT triggers
	 * anyway.
	 */
	else if (resultRelInfo->ri_TrigDesc != NULL &&
			 resultRelInfo->ri_TrigDesc->trig_insert_after_row)
	{
		for (i = 0; i < nBufferedTuples; i++)
		{
			cstate->cur_lineno = firstBufferedLineNo + i;
			ExecARInsertTriggers(estate, resultRelInfo,
								 bufferedTuples[i],
								 NIL);
		}
	}

	/* reset cur_lineno to where we were */
	cstate->cur_lineno = save_cur_lineno;
}

/*
 * Setup to read tuples from a file for COPY FROM.
 *
 * 'rel': Used as a template for the tuples
 * 'filename': Name of server-local file to read
 * 'attnamelist': List of char *, columns to include. NIL selects all cols.
 * 'options': List of DefElem. See copy_opt_item in gram.y for selections.
 *
 * Returns a CopyState, to be passed to NextCopyFrom and related functions.
 */
CopyState
BeginCopyFrom(Relation rel,
			  const char *filename,
			  bool is_program,
			  copy_data_source_cb data_source_cb,
			  void *data_source_cb_extra,
			  List *attnamelist,
			  List *options)
{
	CopyState	cstate;
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs,
				num_defaults;
	FmgrInfo   *in_functions;
	Oid		   *typioparams;
	int			attnum;
	Oid			in_func_oid;
	int		   *defmap;
	ExprState **defexprs;
	MemoryContext oldcontext;
	bool		volatile_defexprs;

	cstate = BeginCopy(true, rel, NULL, NULL, InvalidOid, attnamelist, options, NULL);
	oldcontext = MemoryContextSwitchTo(cstate->copycontext);

	/*
	 * Determine the mode
	 */
	if (cstate->on_segment || data_source_cb)
		cstate->dispatch_mode = COPY_DIRECT;
	else if (Gp_role == GP_ROLE_DISPATCH &&
			 cstate->rel && cstate->rel->rd_cdbpolicy &&
			 cstate->rel->rd_cdbpolicy->ptype != POLICYTYPE_ENTRY)
		cstate->dispatch_mode = COPY_DISPATCH;
	else if (Gp_role == GP_ROLE_EXECUTE)
		cstate->dispatch_mode = COPY_EXECUTOR;
	else
		cstate->dispatch_mode = COPY_DIRECT;

	/* Initialize state variables */
	cstate->fe_eof = false;
	// cstate->eol_type = EOL_UNKNOWN; /* GPDB: don't overwrite value set in ProcessCopyOptions */
	cstate->cur_relname = RelationGetRelationName(cstate->rel);
	cstate->cur_lineno = 0;
	cstate->cur_attname = NULL;
	cstate->cur_attval = NULL;

	/* Set up variables to avoid per-attribute overhead. */
	initStringInfo(&cstate->attribute_buf);
	initStringInfo(&cstate->line_buf);
	cstate->line_buf_converted = false;
	cstate->raw_buf = (char *) palloc(RAW_BUF_SIZE + 1);
	cstate->raw_buf_index = cstate->raw_buf_len = 0;

	tupDesc = RelationGetDescr(cstate->rel);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	num_defaults = 0;
	volatile_defexprs = false;

	/*
	 * Pick up the required catalog information for each attribute in the
	 * relation, including the input function, the element type (to pass to
	 * the input function), and info about defaults and constraints. (Which
	 * input function we use depends on text/binary format choice.)
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

		/* TODO: is force quote array necessary for default conversion */

		/* Get default info if needed */
		if (!list_member_int(cstate->attnumlist, attnum))
		{
			/* attribute is NOT to be copied from input */
			/* use default value if one exists */
			Expr	   *defexpr = (Expr *) build_column_default(cstate->rel,
																attnum);

			if (defexpr != NULL)
			{
				/* Run the expression through planner */
				defexpr = expression_planner(defexpr);

				/* Initialize executable expression in copycontext */
				defexprs[num_defaults] = ExecInitExpr(defexpr, NULL);
				defmap[num_defaults] = attnum - 1;
				num_defaults++;

				/*
				 * If a default expression looks at the table being loaded,
				 * then it could give the wrong answer when using
				 * multi-insert. Since database access can be dynamic this is
				 * hard to test for exactly, so we use the much wider test of
				 * whether the default expression is volatile. We allow for
				 * the special case of when the default expression is the
				 * nextval() of a sequence which in this specific case is
				 * known to be safe for use with the multi-insert
				 * optimisation. Hence we use this special case function
				 * checker rather than the standard check for
				 * contain_volatile_functions().
				 */
				if (!volatile_defexprs)
					volatile_defexprs = contain_volatile_functions_not_nextval((Node *) defexpr);
			}
		}
	}

	/* We keep those variables in cstate. */
	cstate->in_functions = in_functions;
	cstate->typioparams = typioparams;
	cstate->defmap = defmap;
	cstate->defexprs = defexprs;
	cstate->volatile_defexprs = volatile_defexprs;
	cstate->num_defaults = num_defaults;
	cstate->is_program = is_program;

	bool		pipe = (filename == NULL || cstate->dispatch_mode == COPY_EXECUTOR);

	if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
	{
		/* open nothing */

		if (filename == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("STDIN is not supported by 'COPY ON SEGMENT'")));
	}
	else if (data_source_cb)
	{
		cstate->copy_dest = COPY_CALLBACK;
		cstate->data_source_cb = data_source_cb;
		cstate->data_source_cb_extra = data_source_cb_extra;
	}
	else if (pipe)
	{
		Assert(!is_program || cstate->dispatch_mode == COPY_EXECUTOR);	/* the grammar does not allow this */
		if (whereToSendOutput == DestRemote)
			ReceiveCopyBegin(cstate);
		else
			cstate->copy_file = stdin;
	}
	else
	{
		cstate->filename = pstrdup(filename);

		if (cstate->on_segment)
			MangleCopyFileName(cstate);

		if (cstate->is_program)
		{
			cstate->program_pipes = open_program_pipes(cstate->filename, false);
			cstate->copy_file = fdopen(cstate->program_pipes->pipes[0], PG_BINARY_R);
			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not execute command \"%s\": %m",
								cstate->filename)));
		}
		else
		{
			struct stat st;
			char	   *filename = cstate->filename;

			cstate->copy_file = AllocateFile(filename, PG_BINARY_R);
			if (cstate->copy_file == NULL)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\" for reading: %m",
								filename)));

			// Increase buffer size to improve performance  (cmcdevitt)
			setvbuf(cstate->copy_file, NULL, _IOFBF, 393216); // 384 Kbytes

			if (fstat(fileno(cstate->copy_file), &st))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m",
								cstate->filename)));

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
	bool		shouldDispatch = (Gp_role == GP_ROLE_DISPATCH &&
								  rel->rd_cdbpolicy != NULL);
	if (shouldDispatch)
	{
		Oid			relid = RelationGetRelid(cstate->rel);
		List	   *all_relids = NIL;

		all_relids = lappend_oid(all_relids, relid);

		if (rel_is_partitioned(relid))
		{
			PartitionNode *pn = RelationBuildPartitionDesc(cstate->rel, false);
			all_relids = list_concat(all_relids, all_partition_relids(pn));
		}
	}

	if (cstate->on_segment && Gp_role == GP_ROLE_DISPATCH)
	{
		/* nothing to do */
	}
	else if (cstate->dispatch_mode == COPY_EXECUTOR && cstate->copy_dest != COPY_CALLBACK)
	{
		/* Read special header from QD */
		static const size_t sigsize = sizeof(QDtoQESignature);
		char		readSig[sigsize];
		copy_from_dispatch_header header_frame;

		if (CopyGetData(cstate, &readSig, sigsize) != sigsize ||
			memcmp(readSig, QDtoQESignature, sigsize) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("QD->QE COPY communication signature not recognized")));

		if (CopyGetData(cstate, &header_frame, sizeof(header_frame)) != sizeof(header_frame))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("invalid QD->QD COPY communication header")));

		cstate->file_has_oids = header_frame.file_has_oids;
		cstate->first_qe_processed_field = header_frame.first_qe_processed_field;
	}
	else if (!cstate->binary)
	{
		/* must rely on user to tell us... */
		cstate->file_has_oids = cstate->oids;
	}
	else
	{
		/* Read and verify binary header */
		char		readSig[11];
		int32		tmp;

		/* Signature */
		if (CopyGetData(cstate, readSig, 11) != 11 ||
			memcmp(readSig, BinarySignature, 11) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("COPY file signature not recognized")));
		/* Flags field */
		if (!CopyGetInt32(cstate, &tmp))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					errmsg("invalid COPY file header (missing flags)")));
		cstate->file_has_oids = (tmp & (1 << 16)) != 0;
		tmp &= ~(1 << 16);
		if ((tmp >> 16) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unrecognized critical flags in COPY file header")));
		/* Header extension length */
		if (!CopyGetInt32(cstate, &tmp) ||
			tmp < 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("invalid COPY file header (missing length)")));
		/* Skip extension header, if present */
		while (tmp-- > 0)
		{
			if (CopyGetData(cstate, readSig, 1) != 1)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("invalid COPY file header (wrong length)")));
		}
	}

	if (cstate->file_has_oids && cstate->binary)
	{
		getTypeBinaryInputInfo(OIDOID,
							   &in_func_oid, &cstate->oid_typioparam);
		fmgr_info(in_func_oid, &cstate->oid_in_function);
	}

	/* create workspace for CopyReadAttributes results */
	if (!cstate->binary)
	{
		AttrNumber	attr_count = list_length(cstate->attnumlist);
		int			nfields = cstate->file_has_oids ? (attr_count + 1) : attr_count;

		cstate->max_fields = nfields;
		cstate->raw_fields = (char **) palloc(nfields * sizeof(char *));
	}

	MemoryContextSwitchTo(oldcontext);

	return cstate;
}

/*
 * Read raw fields in the next line for COPY FROM in text or csv mode.
 * Return false if no more lines.
 *
 * An internal temporary buffer is returned via 'fields'. It is valid until
 * the next call of the function. Since the function returns all raw fields
 * in the input file, 'nfields' could be different from the number of columns
 * in the relation.
 *
 * NOTE: force_not_null option are not applied to the returned fields.
 */
bool
NextCopyFromRawFields(CopyState cstate, char ***fields, int *nfields)
{
	return NextCopyFromRawFieldsX(cstate, fields, nfields, -1);
}

static bool
NextCopyFromRawFieldsX(CopyState cstate, char ***fields, int *nfields,
					   int stop_processing_at_field)
{
	int			fldct;
	bool		done;

	/* only available for text or csv input */
	Assert(!cstate->binary);

	/* on input just throw the header line away */
	if (cstate->cur_lineno == 0 && cstate->header_line)
	{
		cstate->cur_lineno++;
		if (CopyReadLine(cstate))
			return false;		/* done */
	}

	cstate->cur_lineno++;

	/* Actually read the line into memory here */
	done = CopyReadLine(cstate);

	/*
	 * EOF at start of line means we're done.  If we see EOF after some
	 * characters, we act as though it was newline followed by EOF, ie,
	 * process the line and then exit loop on next iteration.
	 */
	if (done && cstate->line_buf.len == 0)
		return false;

	/* Parse the line into de-escaped field values */
	if (cstate->csv_mode)
		fldct = CopyReadAttributesCSV(cstate, stop_processing_at_field);
	else
		fldct = CopyReadAttributesText(cstate, stop_processing_at_field);

	*fields = cstate->raw_fields;
	*nfields = fldct;
	return true;
}

bool
NextCopyFrom(CopyState cstate, ExprContext *econtext,
			   Datum *values, bool *nulls, Oid *tupleOid)
{
	if (!cstate->cdbsreh)
		return NextCopyFromX(cstate, econtext, values, nulls, tupleOid);
	else
	{
		MemoryContext oldcontext = CurrentMemoryContext;

		for (;;)
		{
			bool		got_error = false;
			bool		result = false;

			PG_TRY();
			{
				result = NextCopyFromX(cstate, econtext, values, nulls, tupleOid);
			}
			PG_CATCH();
			{
				HandleCopyError(cstate); /* cdbsreh->processed is updated inside here */
				got_error = true;
				MemoryContextSwitchTo(oldcontext);
			}
			PG_END_TRY();

			if (result)
				cstate->cdbsreh->processed++;

			if (!got_error)
				return result;
		}
	}
}

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
static void
HandleCopyError(CopyState cstate)
{
	if (cstate->errMode == ALL_OR_NOTHING)
	{
		/* re-throw error and abort */
		PG_RE_THROW();
	}
	/* SREH must only handle data errors. all other errors must not be caught */
	if (ERRCODE_TO_CATEGORY(elog_geterrcode()) != ERRCODE_DATA_EXCEPTION)
	{
		/* re-throw error and abort */
		PG_RE_THROW();
	}
	else
	{
		/* SREH - release error state and handle error */
		MemoryContext oldcontext;
		ErrorData	*edata;
		char	   *errormsg;
		CdbSreh	   *cdbsreh = cstate->cdbsreh;

		cdbsreh->processed++;

		oldcontext = MemoryContextSwitchTo(cstate->cdbsreh->badrowcontext);

		/* save a copy of the error info */
		edata = CopyErrorData();

		FlushErrorState();

		/*
		 * set the error message. Use original msg and add column name if available.
		 * We do this even if we're not logging the errors, because
		 * ErrorIfRejectLimit() below will use this information in the error message,
		 * if the error count is reached.
		 */
		cdbsreh->rawdata = cstate->line_buf.data;

		cdbsreh->is_server_enc = cstate->line_buf_converted;
		cdbsreh->linenumber = cstate->cur_lineno;
		if (cstate->cur_attname)
		{
			errormsg =  psprintf("%s, column %s",
								 edata->message, cstate->cur_attname);
		}
		else
		{
			errormsg = edata->message;
		}
		cstate->cdbsreh->errmsg = errormsg;

		if (IS_LOG_TO_FILE(cstate->cdbsreh->logerrors))
		{
			if (Gp_role == GP_ROLE_DISPATCH && !cstate->on_segment)
			{
				cstate->cdbsreh->rejectcount++;

				SendCopyFromForwardedError(cstate, cstate->cdbCopy, errormsg);
			}
			else
			{
				/* after all the prep work let cdbsreh do the real work */
				if (Gp_role == GP_ROLE_DISPATCH)
				{
					cstate->cdbsreh->rejectcount++;
				}
				else
				{
					HandleSingleRowError(cstate->cdbsreh);
				}
			}
		}
		else
			cstate->cdbsreh->rejectcount++;

		ErrorIfRejectLimitReached(cstate->cdbsreh);

		MemoryContextSwitchTo(oldcontext);
		MemoryContextReset(cstate->cdbsreh->badrowcontext);
	}
}


/*
 * Read next tuple from file for COPY FROM. Return false if no more tuples.
 *
 * 'econtext' is used to evaluate default expression for each columns not
 * read from the file. It can be NULL when no default values are used, i.e.
 * when all columns are read from the file.
 *
 * 'values' and 'nulls' arrays must be the same length as columns of the
 * relation passed to BeginCopyFrom. This function fills the arrays.
 * Oid of the tuple is returned with 'tupleOid' separately.
 */
static bool
NextCopyFromX(CopyState cstate, ExprContext *econtext,
			 Datum *values, bool *nulls, Oid *tupleOid)
{
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs,
				attr_count,
				num_defaults = cstate->num_defaults;
	FmgrInfo   *in_functions = cstate->in_functions;
	Oid		   *typioparams = cstate->typioparams;
	int			i;
	int			nfields;
	bool		isnull;
	int		   *defmap = cstate->defmap;
	ExprState **defexprs = cstate->defexprs;
	List	   *attnumlist;
	bool		file_has_oids;
	int			stop_processing_at_field;

	/*
	 * Figure out what fields we're going to process in this process.
	 *
	 * In the QD, set 'stop_processing_at_field' so that we only those
	 * fields that are needed in the QD.
	 */
	switch (cstate->dispatch_mode)
	{
		case COPY_DIRECT:
			stop_processing_at_field = -1;
			attnumlist = cstate->attnumlist;
			file_has_oids = cstate->file_has_oids;
			break;

		case COPY_DISPATCH:
			stop_processing_at_field = cstate->first_qe_processed_field;
			attnumlist = cstate->qd_attnumlist;
			file_has_oids = cstate->file_has_oids;
			break;

		case COPY_EXECUTOR:
			stop_processing_at_field = -1;
			attnumlist = cstate->qe_attnumlist;
			file_has_oids = false;	/* already handled in QD. */
			break;

		default:
			elog(ERROR, "unexpected COPY dispatch mode %d", cstate->dispatch_mode);
	}

	tupDesc = RelationGetDescr(cstate->rel);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;
	attr_count = list_length(attnumlist);
	nfields = file_has_oids ? (attr_count + 1) : attr_count;

	/* Initialize all values for row to NULL */
	MemSet(values, 0, num_phys_attrs * sizeof(Datum));
	MemSet(nulls, true, num_phys_attrs * sizeof(bool));

	if (!cstate->binary)
	{
		char	  **field_strings;
		ListCell   *cur;
		int			fldct;
		int			fieldno;
		char	   *string;

		/* read raw fields in the next line */
		if (cstate->dispatch_mode != COPY_EXECUTOR)
		{
			if (!NextCopyFromRawFieldsX(cstate, &field_strings, &fldct,
										stop_processing_at_field))
				return false;
		}
		else
		{
			/*
			 * We have received the raw line from the QD, and we just
			 * need to split it into raw fields.
			 */
			if (cstate->stopped_processing_at_delim &&
				cstate->line_buf.cursor <= cstate->line_buf.len)
			{
				if (cstate->csv_mode)
					fldct = CopyReadAttributesCSV(cstate, -1);
				else
					fldct = CopyReadAttributesText(cstate, -1);
			}
			else
				fldct = 0;
			field_strings = cstate->raw_fields;
		}

		/* check for overflowing fields */
		if (nfields > 0 && fldct > nfields)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));

		fieldno = 0;

		/* Read the OID field if present */
		if (file_has_oids)
		{
			if (fieldno >= fldct)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("missing data for OID column")));
			string = field_strings[fieldno++];

			if (string == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("null OID in COPY data")));
			else if (cstate->oids && tupleOid != NULL)
			{
				cstate->cur_attname = "oid";
				cstate->cur_attval = string;
				*tupleOid = DatumGetObjectId(DirectFunctionCall1(oidin,
												   CStringGetDatum(string)));
				if (*tupleOid == InvalidOid)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("invalid OID in COPY data")));
				cstate->cur_attname = NULL;
				cstate->cur_attval = NULL;
			}
		}

		/*
		 * A completely empty line is not allowed with FILL MISSING FIELDS. Without
		 * FILL MISSING FIELDS, it's almost surely an error, but not always:
		 * a table with a single text column, for example, needs to accept empty
		 * lines.
		 */
		if (cstate->line_buf.len == 0 &&
			cstate->fill_missing &&
			list_length(cstate->attnumlist) > 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("missing data for column \"%s\", found empty data line",
							NameStr(attr[1]->attname))));
		}

		/* Loop to read the user attributes on the line. */
		foreach(cur, attnumlist)
		{
			int			attnum = lfirst_int(cur);
			int			m = attnum - 1;

			if (fieldno >= fldct)
			{
				/*
				 * Some attributes are missing. In FILL MISSING FIELDS mode,
				 * treat them as NULLs.
				 */
				if (!cstate->fill_missing)
					ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("missing data for column \"%s\"",
								NameStr(attr[m]->attname))));
				fieldno++;
				string = NULL;
			}
			else
				string = field_strings[fieldno++];

			if (cstate->convert_select_flags &&
				!cstate->convert_select_flags[m])
			{
				/* ignore input field, leaving column as NULL */
				continue;
			}

			if (cstate->csv_mode)
			{
				if (string == NULL &&
					cstate->force_notnull_flags[m])
				{
					/*
					 * FORCE_NOT_NULL option is set and column is NULL -
					 * convert it to the NULL string.
					 */
					string = cstate->null_print;
				}
				else if (string != NULL && cstate->force_null_flags[m]
						 && strcmp(string, cstate->null_print) == 0)
				{
					/*
					 * FORCE_NULL option is set and column matches the NULL
					 * string. It must have been quoted, or otherwise the
					 * string would already have been set to NULL. Convert it
					 * to NULL as specified.
					 */
					string = NULL;
				}
			}

			cstate->cur_attname = NameStr(attr[m]->attname);
			cstate->cur_attval = string;
			values[m] = InputFunctionCall(&in_functions[m],
										  string,
										  typioparams[m],
										  attr[m]->atttypmod);
			if (string != NULL)
				nulls[m] = false;
			cstate->cur_attname = NULL;
			cstate->cur_attval = NULL;
		}

		Assert(fieldno == nfields);
	}
	else if (attr_count)
	{
		/* binary */
		int16		fld_count;
		ListCell   *cur;

		cstate->cur_lineno++;

		if (!CopyGetInt16(cstate, &fld_count))
		{
			/* EOF detected (end of file, or protocol-level EOF) */
			return false;
		}

		if (fld_count == -1)
		{
			/*
			 * Received EOF marker.  In a V3-protocol copy, wait for the
			 * protocol-level EOF, and complain if it doesn't come
			 * immediately.  This ensures that we correctly handle CopyFail,
			 * if client chooses to send that now.
			 *
			 * Note that we MUST NOT try to read more data in an old-protocol
			 * copy, since there is no protocol-level EOF marker then.  We
			 * could go either way for copy from file, but choose to throw
			 * error if there's data after the EOF marker, for consistency
			 * with the new-protocol case.
			 */
			char		dummy;

			if (cstate->copy_dest != COPY_OLD_FE &&
				CopyGetData(cstate, &dummy, 1) > 0)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("received copy data after EOF marker")));
			return false;
		}

		if (fld_count != attr_count)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("row field count is %d, expected %d",
							(int) fld_count, attr_count)));

		if (file_has_oids)
		{
			Oid			loaded_oid;

			cstate->cur_attname = "oid";
			loaded_oid =
				DatumGetObjectId(CopyReadBinaryAttribute(cstate,
														 0,
													&cstate->oid_in_function,
													  cstate->oid_typioparam,
														 -1,
														 &isnull,
														 false));
			if (isnull || loaded_oid == InvalidOid)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("invalid OID in COPY data")));
			cstate->cur_attname = NULL;
			if (cstate->oids && tupleOid != NULL)
				*tupleOid = loaded_oid;
		}

		i = 0;
		foreach(cur, attnumlist)
		{
			int			attnum = lfirst_int(cur);
			int			m = attnum - 1;

			cstate->cur_attname = NameStr(attr[m]->attname);
			i++;
			values[m] = CopyReadBinaryAttribute(cstate,
												i,
												&in_functions[m],
												typioparams[m],
												attr[m]->atttypmod,
												&nulls[m],
												false);
			cstate->cur_attname = NULL;
		}
	}

	/*
	 * Now compute and insert any defaults available for the columns not
	 * provided by the input data.  Anything not processed here or above will
	 * remain NULL.
	 *
	 * GPDB: The defaults are always computed in the QD, and are included
	 * in the QD->QE stream as pre-computed Datums. Funny indentation, to
	 * keep the indentation of the code inside the same as in upstream.
	 * (We could improve this, and compute immutable defaults that don't
	 * affect which segment the row belongs to, in the QE.)
	 */
  if (cstate->dispatch_mode != COPY_EXECUTOR)
  {
	for (i = 0; i < num_defaults; i++)
	{
		/*
		 * The caller must supply econtext and have switched into the
		 * per-tuple memory context in it.
		 */
		Assert(econtext != NULL);
		Assert(CurrentMemoryContext == econtext->ecxt_per_tuple_memory);

		values[defmap[i]] = ExecEvalExpr(defexprs[i], econtext,
										 &nulls[defmap[i]], NULL);
	}
  }

	return true;
}


/*
 * Like NextCopyFrom(), but used in the QD, when we want to parse the
 * input line only partially. We only want to parse enough fields needed
 * to determine which target segment to forward the row to.
 *
 * (The logic is actually within NextCopyFrom(). This is a separate
 * function just for symmetry with NextCopyFromExecute()).
 */
static bool
NextCopyFromDispatch(CopyState cstate, ExprContext *econtext,
					 Datum *values, bool *nulls, Oid *tupleOid)
{
	return NextCopyFrom(cstate, econtext, values, nulls, tupleOid);
}

/*
 * Like NextCopyFrom(), but used in the QE, when we're reading pre-processed
 * rows from the QD.
 */
static TupleTableSlot *
NextCopyFromExecute(CopyState cstate, ExprContext *econtext,
					EState *estate, Oid *tupleOid)
{
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	bool		file_has_oids = cstate->file_has_oids;
	int			i;
	AttrNumber	num_phys_attrs;
	copy_from_dispatch_row frame;
	int			r;
	ResultRelInfo *resultRelInfo;
	TupleTableSlot *baseSlot;
	TupleTableSlot *slot;
	Datum	   *baseValues;
	bool	   *baseNulls;
	Datum	   *values;
	bool	   *nulls;
	bool		got_error;

	/*
	 * The code below reads the 'copy_from_dispatch_row' struct, and only
	 * then checks if it was actually a 'copy_from_dispatch_error' struct.
	 * That only works when 'copy_from_dispatch_error' is larger than
	 *'copy_from_dispatch_row'.
	 */
	StaticAssertStmt(SizeOfCopyFromDispatchError >= SizeOfCopyFromDispatchRow,
					 "copy_from_dispatch_error must be larger than copy_from_dispatch_row");

	/*
	 * If we encounter an error while parsing the row (or we receive a row from
	 * the QD that was already marked as an erroneous row), we loop back here
	 * until we get a good row.
	 */
retry:
	got_error = false;

	r = CopyGetData(cstate, (char *) &frame, SizeOfCopyFromDispatchRow);
	if (r == 0)
		return NULL;
	if (r != SizeOfCopyFromDispatchRow)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));
	if (frame.lineno == -1)
	{
		HandleQDErrorFrame(cstate, (char *) &frame, SizeOfCopyFromDispatchRow);
		goto retry;
	}

	/* Prepare for parsing the input line */
	resultRelInfo = estate->es_result_relation_info;
	baseSlot = resultRelInfo->ri_resultSlot;
	tupDesc = RelationGetDescr(resultRelInfo->ri_RelationDesc);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;

	/* Initialize all values for row to NULL */
	ExecClearTuple(baseSlot);
	baseValues = slot_get_values(baseSlot);
	baseNulls = slot_get_isnull(baseSlot);
	MemSet(baseValues, 0, num_phys_attrs * sizeof(Datum));
	MemSet(baseNulls, true, num_phys_attrs * sizeof(bool));

	/* check for overflowing fields */
	if (frame.fld_count < 0 || frame.fld_count > num_phys_attrs)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("extra data after last expected column")));

	/* Read the OID field, if present */
	if (file_has_oids)
	{
		Oid			loaded_oid;

		if (CopyGetData(cstate, &loaded_oid, sizeof(Oid)) != sizeof(Oid))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("unexpected EOF in COPY data")));

		if (loaded_oid == InvalidOid)
		{
			cstate->cur_attname = "oid";
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("invalid OID in COPY data")));
		}
		*tupleOid = loaded_oid;
	}

	/*
	 * Read the input line into 'line_buf'.
	 */
	resetStringInfo(&cstate->line_buf);
	enlargeStringInfo(&cstate->line_buf, frame.line_len);
	if (CopyGetData(cstate, cstate->line_buf.data, frame.line_len) != frame.line_len)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));
	cstate->line_buf.data[frame.line_len] = '\0';
	cstate->line_buf.len = frame.line_len;
	cstate->line_buf.cursor = frame.residual_off;
	cstate->line_buf_valid = true;
	cstate->line_buf_converted = true;
	cstate->cur_lineno = frame.lineno;
	cstate->stopped_processing_at_delim = frame.delim_seen_at_end;

	/*
	 * Parse any fields from the input line that were not processed in the
	 * QD already.
	 */
	if (!cstate->cdbsreh)
	{
		if (!NextCopyFromX(cstate, econtext, baseValues, baseNulls, NULL))
		{
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("unexpected EOF in COPY data")));
		}
	}
	else
	{
		MemoryContext oldcontext = CurrentMemoryContext;
		bool		result;

		PG_TRY();
		{
			result = NextCopyFromX(cstate, econtext,
								   baseValues, baseNulls, tupleOid);

			if (!result)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("unexpected EOF in COPY data")));
		}
		PG_CATCH();
		{
			HandleCopyError(cstate);
			got_error = true;
			MemoryContextSwitchTo(oldcontext);
		}
		PG_END_TRY();
	}

	ExecStoreVirtualTuple(baseSlot);

	/*
	 * Remap the values to the form expected by the target partition.
	 */
	if (frame.relid != RelationGetRelid(resultRelInfo->ri_RelationDesc))
	{
		MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

		resultRelInfo = targetid_get_partition(frame.relid, estate, true);
		slot = reconstructPartitionTupleSlot(baseSlot, resultRelInfo);

		MemoryContextSwitchTo(oldcontext);

		/* since resultRelInfo has changed, refresh these values */
		tupDesc = RelationGetDescr(resultRelInfo->ri_RelationDesc);
		attr = tupDesc->attrs;
		num_phys_attrs = tupDesc->natts;
	}
	else
		slot = baseSlot;

	/*
	 * Read any attributes that were processed in the QD already. The attribute
	 * numbers in the message are already in terms of the target partition, so
	 * we do this after remapping and switching to the partition slot.
	 */
	values = slot_get_values(slot);
	nulls = slot_get_isnull(slot);
	for (i = 0; i < frame.fld_count; i++)
	{
		int16		attnum;
		int			m;
		int32		len;
		Datum		value;

		if (CopyGetData(cstate, &attnum, sizeof(attnum)) != sizeof(attnum))
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("unexpected EOF in COPY data")));

		if (attnum < 1 || attnum > num_phys_attrs)
			elog(ERROR, "invalid attnum received from QD: %d (num physical attributes: %d)",
				 attnum, num_phys_attrs);
		m = attnum - 1;

		cstate->cur_attname = NameStr(attr[m]->attname);

		if (attr[attnum - 1]->attbyval)
		{
			if (CopyGetData(cstate, &value, sizeof(Datum)) != sizeof(Datum))
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 errmsg("unexpected EOF in COPY data")));
		}
		else
		{
			char	   *p;

			if (attr[attnum - 1]->attlen > 0)
			{
				len = attr[attnum - 1]->attlen;

				p = palloc(len);
				if (CopyGetData(cstate, p, len) != len)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unexpected EOF in COPY data")));
			}
			else if (attr[attnum - 1]->attlen == -1)
			{
				/* For simplicity, varlen's are always transmitted in "long" format */
				if (CopyGetData(cstate, &len, sizeof(len)) != sizeof(len))
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unexpected EOF in COPY data")));
				if (len < VARHDRSZ)
					elog(ERROR, "invalid varlen length received from QD: %d", len);
				p = palloc(len);
				SET_VARSIZE(p, len);
				if (CopyGetData(cstate, p + VARHDRSZ, len - VARHDRSZ) != len - VARHDRSZ)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unexpected EOF in COPY data")));
			}
			else if (attr[attnum - 1]->attlen == -2)
			{
				/*
				 * Like the varlen case above, cstrings are sent with a length
				 * prefix and no terminator, so we have to NULL-terminate in
				 * memory after reading them in.
				 */
				if (CopyGetData(cstate, &len, sizeof(len)) != sizeof(len))
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unexpected EOF in COPY data")));
				p = palloc(len + 1);
				if (CopyGetData(cstate, p, len) != len)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unexpected EOF in COPY data")));
				p[len] = '\0';
			}
			else
			{
				elog(ERROR, "attribute %d has invalid length %d",
					 attnum, attr[attnum - 1]->attlen);
			}
			value = PointerGetDatum(p);
		}

		cstate->cur_attname = NULL;

		values[m] = value;
		nulls[m] = false;
	}

	if (got_error)
		goto retry;

	ExecStoreVirtualTuple(slot);
	estate->es_result_relation_info = resultRelInfo;

	/*
	 * Here we should compute defaults for any columns for which we didn't
	 * get a default from the QD. But at the moment, all defaults are evaluated
	 * in the QD.
	 */

	return slot;
}

/*
 * Parse and handle an "error frame" from QD.
 *
 * The caller has already read part of the frame; 'p' points to that part,
 * of length 'len'.
 */
static void
HandleQDErrorFrame(CopyState cstate, char *p, int len)
{
	CdbSreh *cdbsreh = cstate->cdbsreh;
	MemoryContext oldcontext;
	copy_from_dispatch_error errframe;
	char	   *errormsg;
	char	   *line;
	int			r;

	Assert(len <= SizeOfCopyFromDispatchError);

	Assert(Gp_role == GP_ROLE_EXECUTE);

	oldcontext = MemoryContextSwitchTo(cdbsreh->badrowcontext);

	/*
	 * Copy the part of the struct that the caller had already read, and
	 * read the rest.
	 */
	memcpy(&errframe, p, len);

	r = CopyGetData(cstate, ((char *) &errframe) + len, SizeOfCopyFromDispatchError - len);
	if (r != SizeOfCopyFromDispatchError - len)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));

	errormsg = palloc(errframe.errmsg_len + 1);
	line = palloc(errframe.line_len + 1);

	r = CopyGetData(cstate, errormsg, errframe.errmsg_len);
	if (r != errframe.errmsg_len)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));
	errormsg[errframe.errmsg_len] = '\0';

	r = CopyGetData(cstate, line, errframe.line_len);
	if (r != errframe.line_len)
		ereport(ERROR,
				(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
				 errmsg("unexpected EOF in COPY data")));
	line[errframe.line_len] = '\0';

	cdbsreh->linenumber = errframe.lineno;
	cdbsreh->rawdata = line;
	cdbsreh->errmsg = errormsg;
	cdbsreh->is_server_enc = errframe.line_buf_converted;

	HandleSingleRowError(cdbsreh);

	MemoryContextSwitchTo(oldcontext);

}

/*
 * Inlined versions of appendBinaryStringInfo and enlargeStringInfo, for
 * speed.
 *
 * NOTE: These versions don't NULL-terminate the string. We don't need
 * it here.
 */
#define APPEND_MSGBUF_NOCHECK(buf, ptr, datalen) \
	do { \
		memcpy((buf)->data + (buf)->len, ptr, (datalen)); \
		(buf)->len += (datalen); \
	} while(0)

#define APPEND_MSGBUF(buf, ptr, datalen) \
	do { \
		if ((buf)->len + (datalen) >= (buf)->maxlen) \
			enlargeStringInfo((buf), (datalen)); \
		memcpy((buf)->data + (buf)->len, ptr, (datalen)); \
		(buf)->len += (datalen); \
	} while(0)

#define ENLARGE_MSGBUF(buf, needed) \
	do { \
		if ((buf)->len + (needed) >= (buf)->maxlen) \
			enlargeStringInfo((buf), (needed)); \
	} while(0)

/*
 * This is the sending counterpart of NextCopyFromExecute. Used in the QD,
 * to send a row to a QE.
 */
static void
SendCopyFromForwardedTuple(CopyState cstate,
						   CdbCopy *cdbCopy,
						   bool toAll,
						   int target_seg,
						   Relation rel,
						   int64 lineno,
						   char *line,
						   int line_len,
						   Oid tuple_oid,
						   Datum *values,
						   bool *nulls)
{
	TupleDesc	tupDesc;
	Form_pg_attribute *attr;
	copy_from_dispatch_row *frame;
	StringInfo	msgbuf;
	int			num_sent_fields;
	AttrNumber	num_phys_attrs;
	int			i;

	if (!OidIsValid(RelationGetRelid(rel)))
		elog(ERROR, "invalid target table OID in COPY");

	tupDesc = RelationGetDescr(rel);
	attr = tupDesc->attrs;
	num_phys_attrs = tupDesc->natts;

	/*
	 * Reset the message buffer, and reserve enough space for the header,
	 * the OID if any, and the residual line.
	 */
	msgbuf = cstate->dispatch_msgbuf;
	ENLARGE_MSGBUF(msgbuf, SizeOfCopyFromDispatchRow + sizeof(Oid) + cstate->line_buf.len);

	/* the header goes to the beginning of the struct, but it will be filled in later. */
	msgbuf->len = SizeOfCopyFromDispatchRow;

	/*
	 * After the header, the OID from the input row, if any.
	 */
	if (cstate->file_has_oids)
		APPEND_MSGBUF_NOCHECK(msgbuf, &tuple_oid, sizeof(Oid));

	/*
	 * Next, any residual text that we didn't process in the QD.
	 */
	APPEND_MSGBUF_NOCHECK(msgbuf, cstate->line_buf.data, cstate->line_buf.len);

	/*
	 * Append attributes to the buffer.
	 */
	num_sent_fields = 0;
	for (i = 0; i < num_phys_attrs; i++)
	{
		int16		attnum = i + 1;

		/* NULLs are simply left out of the message. */
		if (nulls[i])
			continue;

		/*
		 * Make sure we have room for the attribute number. While we're at it,
		 * also reserve room for the Datum, if it's a by-value datatype, or for
		 * the length field, if it's a varlena. Allocating both in one call
		 * saves one size-check.
		 */
		ENLARGE_MSGBUF(msgbuf, sizeof(int16) + sizeof(Datum));

		/* attribute number comes first */
		APPEND_MSGBUF_NOCHECK(msgbuf, &attnum, sizeof(int16));

		if (attr[i]->attbyval)
		{
			/* we already reserved space for this above, so we can just memcpy */
			APPEND_MSGBUF_NOCHECK(msgbuf, &values[i], sizeof(Datum));
		}
		else
		{
			if (attr[i]->attlen > 0)
			{
				APPEND_MSGBUF(msgbuf, DatumGetPointer(values[i]), attr[i]->attlen);
			}
			else if (attr[attnum - 1]->attlen == -1)
			{
				int32		len;
				char	   *ptr;

				/* For simplicity, varlen's are always transmitted in "long" format */
				Assert(!VARATT_IS_SHORT(values[i]));
				len = VARSIZE(values[i]);
				ptr = VARDATA(values[i]);

				/* we already reserved space for this int */
				APPEND_MSGBUF_NOCHECK(msgbuf, &len, sizeof(int32));
				APPEND_MSGBUF(msgbuf, ptr, len - VARHDRSZ);
			}
			else if (attr[attnum - 1]->attlen == -2)
			{
				/*
				 * These attrs are NULL-terminated in memory, but we send
				 * them length-prefixed (like the varlen case above) so that
				 * the receiver can preallocate a data buffer.
				 */
				int32		len;
				size_t		slen;
				char	   *ptr;

				ptr = DatumGetPointer(values[i]);
				slen = strlen(ptr);

				if (slen > PG_INT32_MAX)
				{
					elog(ERROR, "attribute %d is too long (%lld bytes)",
						 attnum, (long long) slen);
				}

				len = (int32) slen;

				APPEND_MSGBUF_NOCHECK(msgbuf, &len, sizeof(int32));
				APPEND_MSGBUF(msgbuf, ptr, len);
			}
			else
			{
				elog(ERROR, "attribute %d has invalid length %d",
					 attnum, attr[attnum - 1]->attlen);
			}
		}

		num_sent_fields++;
	}

	/*
	 * Fill in the header. We reserved room for this at the beginning of the
	 * buffer.
	 */
	frame = (copy_from_dispatch_row *) msgbuf->data;
	frame->lineno = lineno;
	frame->relid = RelationGetRelid(rel);
	frame->line_len = cstate->line_buf.len;
	frame->residual_off = cstate->line_buf.cursor;
	frame->fld_count = num_sent_fields;
	frame->delim_seen_at_end = cstate->stopped_processing_at_delim;

	if (toAll)
		cdbCopySendDataToAll(cdbCopy, msgbuf->data, msgbuf->len);
	else
		cdbCopySendData(cdbCopy, target_seg, msgbuf->data, msgbuf->len);
}

static void
SendCopyFromForwardedHeader(CopyState cstate, CdbCopy *cdbCopy)
{
	copy_from_dispatch_header header_frame;

	cdbCopySendDataToAll(cdbCopy, QDtoQESignature, sizeof(QDtoQESignature));

	memset(&header_frame, 0, sizeof(header_frame));
	header_frame.file_has_oids = cstate->file_has_oids;
	header_frame.first_qe_processed_field = cstate->first_qe_processed_field;

	cdbCopySendDataToAll(cdbCopy, (char *) &header_frame, sizeof(header_frame));
}

static void
SendCopyFromForwardedError(CopyState cstate, CdbCopy *cdbCopy, char *errormsg)
{
	copy_from_dispatch_error *errframe;
	StringInfo	msgbuf;
	int			target_seg;
	int			errormsg_len = strlen(errormsg);

	msgbuf = cstate->dispatch_msgbuf;
	resetStringInfo(msgbuf);
	enlargeStringInfo(msgbuf, SizeOfCopyFromDispatchError);
	/* allocate space for the header (we'll fill it in last). */
	msgbuf->len = SizeOfCopyFromDispatchError;

	appendBinaryStringInfo(msgbuf, errormsg, errormsg_len);
	appendBinaryStringInfo(msgbuf, cstate->line_buf.data, cstate->line_buf.len);

	errframe = (copy_from_dispatch_error *) msgbuf->data;

	errframe->error_marker = -1;
	errframe->lineno = cstate->cur_lineno;
	errframe->line_len = cstate->line_buf.len;
	errframe->errmsg_len = errormsg_len;
	errframe->line_buf_converted = cstate->line_buf_converted;

	/* send the bad data row to a random QE (via roundrobin) */
	if (cstate->lastsegid == cdbCopy->total_segs)
		cstate->lastsegid = 0; /* start over from first segid */

	target_seg = (cstate->lastsegid++ % cdbCopy->total_segs);

	cdbCopySendData(cdbCopy, target_seg, msgbuf->data, msgbuf->len);
}

/*
 * Clean up storage and release resources for COPY FROM.
 */
void
EndCopyFrom(CopyState cstate)
{
	/* No COPY FROM related resources except memory. */

	EndCopy(cstate);
}

/*
 * Read the next input line and stash it in line_buf, with conversion to
 * server encoding.
 *
 * Result is true if read was terminated by EOF, false if terminated
 * by newline.  The terminating newline or EOF marker is not included
 * in the final value of line_buf.
 */
static bool
CopyReadLine(CopyState cstate)
{
	bool		result;

	resetStringInfo(&cstate->line_buf);
	cstate->line_buf_valid = true;

	/* Mark that encoding conversion hasn't occurred yet */
	cstate->line_buf_converted = false;

	/* Parse data and transfer into line_buf */
	result = CopyReadLineText(cstate);

	if (result)
	{
		/*
		 * Reached EOF.  In protocol version 3, we should ignore anything
		 * after \. up to the protocol end of copy data.  (XXX maybe better
		 * not to treat \. as special?)
		 */
		if (cstate->copy_dest == COPY_NEW_FE)
		{
			do
			{
				cstate->raw_buf_index = cstate->raw_buf_len;
			} while (CopyLoadRawBuf(cstate));
		}
	}
	else
	{
		/*
		 * If we didn't hit EOF, then we must have transferred the EOL marker
		 * to line_buf along with the data.  Get rid of it.
		 */
		switch (cstate->eol_type)
		{
			case EOL_NL:
				Assert(cstate->line_buf.len >= 1);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
				cstate->line_buf.len--;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				break;
			case EOL_CR:
				Assert(cstate->line_buf.len >= 1);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\r');
				cstate->line_buf.len--;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				break;
			case EOL_CRNL:
				Assert(cstate->line_buf.len >= 2);
				Assert(cstate->line_buf.data[cstate->line_buf.len - 2] == '\r');
				Assert(cstate->line_buf.data[cstate->line_buf.len - 1] == '\n');
				cstate->line_buf.len -= 2;
				cstate->line_buf.data[cstate->line_buf.len] = '\0';
				break;
			case EOL_UNKNOWN:
				/* shouldn't get here */
				Assert(false);
				break;
		}
	}

	/* Done reading the line.  Convert it to server encoding. */
	if (cstate->need_transcoding)
	{
		char	   *cvt;

		cvt = pg_any_to_server(cstate->line_buf.data,
							   cstate->line_buf.len,
							   cstate->file_encoding);
		if (cvt != cstate->line_buf.data)
		{
			/* transfer converted data back to line_buf */
			resetStringInfo(&cstate->line_buf);
			appendBinaryStringInfo(&cstate->line_buf, cvt, strlen(cvt));
			pfree(cvt);
		}
	}

	/* Now it's safe to use the buffer in error messages */
	cstate->line_buf_converted = true;

	return result;
}

/*
 * CopyReadLineText - inner loop of CopyReadLine for text mode
 */
static bool
CopyReadLineText(CopyState cstate)
{
	char	   *copy_raw_buf;
	int			raw_buf_ptr;
	int			copy_buf_len;
	bool		need_data = false;
	bool		hit_eof = false;
	bool		result = false;
	char		mblen_str[2];

	/* CSV variables */
	bool		first_char_in_line = true;
	bool		in_quote = false,
				last_was_esc = false;
	char		quotec = '\0';
	char		escapec = '\0';

	if (cstate->csv_mode)
	{
		quotec = cstate->quote[0];
		escapec = cstate->escape[0];
		/* ignore special escape processing if it's the same as quotec */
		if (quotec == escapec)
			escapec = '\0';
	}

	mblen_str[1] = '\0';

	/*
	 * The objective of this loop is to transfer the entire next input line
	 * into line_buf.  Hence, we only care for detecting newlines (\r and/or
	 * \n) and the end-of-copy marker (\.).
	 *
	 * In CSV mode, \r and \n inside a quoted field are just part of the data
	 * value and are put in line_buf.  We keep just enough state to know if we
	 * are currently in a quoted field or not.
	 *
	 * These four characters, and the CSV escape and quote characters, are
	 * assumed the same in frontend and backend encodings.
	 *
	 * For speed, we try to move data from raw_buf to line_buf in chunks
	 * rather than one character at a time.  raw_buf_ptr points to the next
	 * character to examine; any characters from raw_buf_index to raw_buf_ptr
	 * have been determined to be part of the line, but not yet transferred to
	 * line_buf.
	 *
	 * For a little extra speed within the loop, we copy raw_buf and
	 * raw_buf_len into local variables.
	 */
	copy_raw_buf = cstate->raw_buf;
	raw_buf_ptr = cstate->raw_buf_index;
	copy_buf_len = cstate->raw_buf_len;

	for (;;)
	{
		int			prev_raw_ptr;
		char		c;

		/*
		 * Load more data if needed.  Ideally we would just force four bytes
		 * of read-ahead and avoid the many calls to
		 * IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(), but the COPY_OLD_FE protocol
		 * does not allow us to read too far ahead or we might read into the
		 * next data, so we read-ahead only as far we know we can.  One
		 * optimization would be to read-ahead four byte here if
		 * cstate->copy_dest != COPY_OLD_FE, but it hardly seems worth it,
		 * considering the size of the buffer.
		 */
		if (raw_buf_ptr >= copy_buf_len || need_data)
		{
			REFILL_LINEBUF;

			/*
			 * Try to read some more data.  This will certainly reset
			 * raw_buf_index to zero, and raw_buf_ptr must go with it.
			 */
			if (!CopyLoadRawBuf(cstate))
				hit_eof = true;
			raw_buf_ptr = 0;
			copy_buf_len = cstate->raw_buf_len;

			/*
			 * If we are completely out of data, break out of the loop,
			 * reporting EOF.
			 */
			if (copy_buf_len <= 0)
			{
				result = true;
				break;
			}
			need_data = false;
		}

		/* OK to fetch a character */
		prev_raw_ptr = raw_buf_ptr;
		c = copy_raw_buf[raw_buf_ptr++];

		if (cstate->csv_mode)
		{
			/*
			 * If character is '\\' or '\r', we may need to look ahead below.
			 * Force fetch of the next character if we don't already have it.
			 * We need to do this before changing CSV state, in case one of
			 * these characters is also the quote or escape character.
			 *
			 * Note: old-protocol does not like forced prefetch, but it's OK
			 * here since we cannot validly be at EOF.
			 */
			if (c == '\\' || c == '\r')
			{
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
			}

			/*
			 * Dealing with quotes and escapes here is mildly tricky. If the
			 * quote char is also the escape char, there's no problem - we
			 * just use the char as a toggle. If they are different, we need
			 * to ensure that we only take account of an escape inside a
			 * quoted field and immediately preceding a quote char, and not
			 * the second in an escape-escape sequence.
			 */
			if (in_quote && c == escapec)
				last_was_esc = !last_was_esc;
			if (c == quotec && !last_was_esc)
				in_quote = !in_quote;
			if (c != escapec)
				last_was_esc = false;

			/*
			 * Updating the line count for embedded CR and/or LF chars is
			 * necessarily a little fragile - this test is probably about the
			 * best we can do.  (XXX it's arguable whether we should do this
			 * at all --- is cur_lineno a physical or logical count?)
			 */
			if (in_quote && c == (cstate->eol_type == EOL_NL ? '\n' : '\r'))
				cstate->cur_lineno++;
		}

		/* Process \r */
		if (c == '\r' && (!cstate->csv_mode || !in_quote))
		{
			/* Check for \r\n on first line, _and_ handle \r\n. */
			if (cstate->eol_type == EOL_UNKNOWN ||
				cstate->eol_type == EOL_CRNL)
			{
				/*
				 * If need more data, go back to loop top to load it.
				 *
				 * Note that if we are at EOF, c will wind up as '\0' because
				 * of the guaranteed pad of raw_buf.
				 */
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);

				/* get next char */
				c = copy_raw_buf[raw_buf_ptr];

				if (c == '\n')
				{
					raw_buf_ptr++;		/* eat newline */
					cstate->eol_type = EOL_CRNL;		/* in case not set yet */

					/*
					 * GPDB: end of line. Since we don't error out if we find a
					 * bare CR or LF in CRLF mode, break here instead.
					 */
					break;
				}
				else
				{
					/*
					 * GPDB_91_MERGE_FIXME: these commented-out blocks (as well
					 * as the restructured newline checks) are here because we
					 * allow the user to manually set the newline mode, and
					 * therefore don't error out on bare CR/LF in the middle of
					 * a column. Instead, they will be included verbatim.
					 *
					 * This probably has other fallout -- but so does changing
					 * the behavior. Discuss.
					 */
#if 0
					/* found \r, but no \n */
					if (cstate->eol_type == EOL_CRNL)
						ereport(ERROR,
								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								 !cstate->csv_mode ?
							errmsg("literal carriage return found in data") :
							errmsg("unquoted carriage return found in data"),
								 !cstate->csv_mode ?
						errhint("Use \"\\r\" to represent carriage return.") :
								 errhint("Use quoted CSV field to represent carriage return.")));
#endif

					/* GPDB: only reset eol_type if it's currently unknown. */
					if (cstate->eol_type == EOL_UNKNOWN)
					{
						/*
						 * if we got here, it is the first line and we didn't find
						 * \n, so don't consume the peeked character
						 */
						cstate->eol_type = EOL_CR;
					}
				}
			}
#if 0 /* GPDB_91_MERGE_FIXME: see above. */
			else if (cstate->eol_type == EOL_NL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 !cstate->csv_mode ?
						 errmsg("literal carriage return found in data") :
						 errmsg("unquoted carriage return found in data"),
						 !cstate->csv_mode ?
					   errhint("Use \"\\r\" to represent carriage return.") :
						 errhint("Use quoted CSV field to represent carriage return.")));
#endif
			/* GPDB: a CR only ends the line in CR mode. */
			if (cstate->eol_type == EOL_CR)
			{
				/* If reach here, we have found the line terminator */
				break;
			}
		}

		/* Process \n */
		if (c == '\n' && (!cstate->csv_mode || !in_quote))
		{
#if 0 /* GPDB_91_MERGE_FIXME: see above. */
			if (cstate->eol_type == EOL_CR || cstate->eol_type == EOL_CRNL)
				ereport(ERROR,
						(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
						 !cstate->csv_mode ?
						 errmsg("literal newline found in data") :
						 errmsg("unquoted newline found in data"),
						 !cstate->csv_mode ?
						 errhint("Use \"\\n\" to represent newline.") :
					 errhint("Use quoted CSV field to represent newline.")));
#endif
			/* GPDB: only reset eol_type if it's currently unknown. */
			if (cstate->eol_type == EOL_UNKNOWN)
				cstate->eol_type = EOL_NL;	/* in case not set yet */

			/* GPDB: a LF only ends the line in LF mode. */
			if (cstate->eol_type == EOL_NL)
			{
				/* If reach here, we have found the line terminator */
				break;
			}
		}

		/*
		 * In CSV mode, we only recognize \. alone on a line.  This is because
		 * \. is a valid CSV data value.
		 */
		if (c == '\\' && (!cstate->csv_mode || first_char_in_line))
		{
			char		c2;

			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
			IF_NEED_REFILL_AND_EOF_BREAK(0);

			/* -----
			 * get next character
			 * Note: we do not change c so if it isn't \., we can fall
			 * through and continue processing for file encoding.
			 * -----
			 */
			c2 = copy_raw_buf[raw_buf_ptr];

			if (c2 == '.')
			{
				raw_buf_ptr++;	/* consume the '.' */

				/*
				 * Note: if we loop back for more data here, it does not
				 * matter that the CSV state change checks are re-executed; we
				 * will come back here with no important state changed.
				 */
				if (cstate->eol_type == EOL_CRNL)
				{
					/* Get the next character */
					IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
					/* if hit_eof, c2 will become '\0' */
					c2 = copy_raw_buf[raw_buf_ptr++];

					if (c2 == '\n')
					{
						if (!cstate->csv_mode)
						{
							cstate->raw_buf_index = raw_buf_ptr;
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("end-of-copy marker does not match previous newline style")));
						}
						else
							NO_END_OF_COPY_GOTO;
					}
					else if (c2 != '\r')
					{
						if (!cstate->csv_mode)
						{
							cstate->raw_buf_index = raw_buf_ptr;
							ereport(ERROR,
									(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
									 errmsg("end-of-copy marker corrupt")));
						}
						else
							NO_END_OF_COPY_GOTO;
					}
				}

				/* Get the next character */
				IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(0);
				/* if hit_eof, c2 will become '\0' */
				c2 = copy_raw_buf[raw_buf_ptr++];

				if (c2 != '\r' && c2 != '\n')
				{
					if (!cstate->csv_mode)
					{
						cstate->raw_buf_index = raw_buf_ptr;
						ereport(ERROR,
								(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
								 errmsg("end-of-copy marker corrupt")));
					}
					else
						NO_END_OF_COPY_GOTO;
				}

				if ((cstate->eol_type == EOL_NL && c2 != '\n') ||
					(cstate->eol_type == EOL_CRNL && c2 != '\n') ||
					(cstate->eol_type == EOL_CR && c2 != '\r'))
				{
					cstate->raw_buf_index = raw_buf_ptr;
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("end-of-copy marker does not match previous newline style")));
				}

				/*
				 * Transfer only the data before the \. into line_buf, then
				 * discard the data and the \. sequence.
				 */
				if (prev_raw_ptr > cstate->raw_buf_index)
					appendBinaryStringInfo(&cstate->line_buf,
									 cstate->raw_buf + cstate->raw_buf_index,
									   prev_raw_ptr - cstate->raw_buf_index);
				cstate->raw_buf_index = raw_buf_ptr;
				result = true;	/* report EOF */
				break;
			}
			else if (!cstate->csv_mode)

				/*
				 * If we are here, it means we found a backslash followed by
				 * something other than a period.  In non-CSV mode, anything
				 * after a backslash is special, so we skip over that second
				 * character too.  If we didn't do that \\. would be
				 * considered an eof-of copy, while in non-CSV mode it is a
				 * literal backslash followed by a period.  In CSV mode,
				 * backslashes are not special, so we want to process the
				 * character after the backslash just like a normal character,
				 * so we don't increment in those cases.
				 */
				raw_buf_ptr++;
		}

		/*
		 * This label is for CSV cases where \. appears at the start of a
		 * line, but there is more text after it, meaning it was a data value.
		 * We are more strict for \. in CSV mode because \. could be a data
		 * value, while in non-CSV mode, \. cannot be a data value.
		 */
not_end_of_copy:

		/*
		 * Process all bytes of a multi-byte character as a group.
		 *
		 * We only support multi-byte sequences where the first byte has the
		 * high-bit set, so as an optimization we can avoid this block
		 * entirely if it is not set.
		 */
		if (cstate->encoding_embeds_ascii && IS_HIGHBIT_SET(c))
		{
			int			mblen;

			mblen_str[0] = c;
			/* All our encodings only read the first byte to get the length */
			mblen = pg_encoding_mblen(cstate->file_encoding, mblen_str);
			IF_NEED_REFILL_AND_NOT_EOF_CONTINUE(mblen - 1);
			IF_NEED_REFILL_AND_EOF_BREAK(mblen - 1);
			raw_buf_ptr += mblen - 1;
		}
		first_char_in_line = false;
	}							/* end of outer loop */

	/*
	 * Transfer any still-uncopied data to line_buf.
	 */
	REFILL_LINEBUF;

	return result;
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
 * Parse the current line into separate attributes (fields),
 * performing de-escaping as needed.
 *
 * The input is in line_buf.  We use attribute_buf to hold the result
 * strings.  cstate->raw_fields[k] is set to point to the k'th attribute
 * string, or NULL when the input matches the null marker string.
 * This array is expanded as necessary.
 *
 * (Note that the caller cannot check for nulls since the returned
 * string would be the post-de-escaping equivalent, which may look
 * the same as some valid data string.)
 *
 * delim is the column delimiter string (must be just one byte for now).
 * null_print is the null marker string.  Note that this is compared to
 * the pre-de-escaped input string.
 *
 * The return value is the number of fields actually read.
 */
static int
CopyReadAttributesText(CopyState cstate, int stop_processing_at_field)
{
	char		delimc = cstate->delim[0];
	char		escapec = cstate->escape_off ? delimc : cstate->escape[0];
	int			fieldno;
	char	   *output_ptr;
	char	   *cur_ptr;
	char	   *line_end_ptr;

	/*
	 * We need a special case for zero-column tables: check that the input
	 * line is empty, and return.
	 */
	if (cstate->max_fields <= 0)
	{
		if (cstate->line_buf.len != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));
		return 0;
	}

	resetStringInfo(&cstate->attribute_buf);

	/*
	 * The de-escaped attributes will certainly not be longer than the input
	 * data line, so we can just force attribute_buf to be large enough and
	 * then transfer data without any checks for enough space.  We need to do
	 * it this way because enlarging attribute_buf mid-stream would invalidate
	 * pointers already stored into cstate->raw_fields[].
	 */
	if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
		enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
	output_ptr = cstate->attribute_buf.data;

	/* set pointer variables for loop */
	cur_ptr = cstate->line_buf.data + cstate->line_buf.cursor;
	line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;

	/* Outer loop iterates over fields */
	fieldno = 0;
	for (;;)
	{
		bool		found_delim = false;
		char	   *start_ptr;
		char	   *end_ptr;
		int			input_len;
		bool		saw_non_ascii = false;

		/*
		 * In QD, stop once we have processed the last field we need in the QD.
		 */
		if (fieldno == stop_processing_at_field)
		{
			cstate->stopped_processing_at_delim = true;
			break;
		}

		/* Make sure there is enough space for the next value */
		if (fieldno >= cstate->max_fields)
		{
			cstate->max_fields *= 2;
			cstate->raw_fields =
				repalloc(cstate->raw_fields, cstate->max_fields * sizeof(char *));
		}

		/* Remember start of field on both input and output sides */
		start_ptr = cur_ptr;
		cstate->raw_fields[fieldno] = output_ptr;

		/*
		 * Scan data for field.
		 *
		 * Note that in this loop, we are scanning to locate the end of field
		 * and also speculatively performing de-escaping.  Once we find the
		 * end-of-field, we can match the raw field contents against the null
		 * marker string.  Only after that comparison fails do we know that
		 * de-escaping is actually the right thing to do; therefore we *must
		 * not* throw any syntax errors before we've done the null-marker
		 * check.
		 */
		for (;;)
		{
			char		c;

			end_ptr = cur_ptr;
			if (cur_ptr >= line_end_ptr)
				break;
			c = *cur_ptr++;
			if (c == delimc)
			{
				found_delim = true;
				break;
			}
			if (c == escapec)
			{
				if (cur_ptr >= line_end_ptr)
					break;
				c = *cur_ptr++;
				switch (c)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						{
							/* handle \013 */
							int			val;

							val = OCTVALUE(c);
							if (cur_ptr < line_end_ptr)
							{
								c = *cur_ptr;
								if (ISOCTAL(c))
								{
									cur_ptr++;
									val = (val << 3) + OCTVALUE(c);
									if (cur_ptr < line_end_ptr)
									{
										c = *cur_ptr;
										if (ISOCTAL(c))
										{
											cur_ptr++;
											val = (val << 3) + OCTVALUE(c);
										}
									}
								}
							}
							c = val & 0377;
							if (c == '\0' || IS_HIGHBIT_SET(c))
								saw_non_ascii = true;
						}
						break;
					case 'x':
						/* Handle \x3F */
						if (cur_ptr < line_end_ptr)
						{
							char		hexchar = *cur_ptr;

							if (isxdigit((unsigned char) hexchar))
							{
								int			val = GetDecimalFromHex(hexchar);

								cur_ptr++;
								if (cur_ptr < line_end_ptr)
								{
									hexchar = *cur_ptr;
									if (isxdigit((unsigned char) hexchar))
									{
										cur_ptr++;
										val = (val << 4) + GetDecimalFromHex(hexchar);
									}
								}
								c = val & 0xff;
								if (c == '\0' || IS_HIGHBIT_SET(c))
									saw_non_ascii = true;
							}
						}
						break;
					case 'b':
						c = '\b';
						break;
					case 'f':
						c = '\f';
						break;
					case 'n':
						c = '\n';
						break;
					case 'r':
						c = '\r';
						break;
					case 't':
						c = '\t';
						break;
					case 'v':
						c = '\v';
						break;

						/*
						 * in all other cases, take the char after '\'
						 * literally
						 */
				}
			}

			/* Add c to output string */
			*output_ptr++ = c;
		}

		/* Check whether raw input matched null marker */
		input_len = end_ptr - start_ptr;
		if (input_len == cstate->null_print_len &&
			strncmp(start_ptr, cstate->null_print, input_len) == 0)
			cstate->raw_fields[fieldno] = NULL;
		else
		{
			/*
			 * At this point we know the field is supposed to contain data.
			 *
			 * If we de-escaped any non-7-bit-ASCII chars, make sure the
			 * resulting string is valid data for the db encoding.
			 */
			if (saw_non_ascii)
			{
				char	   *fld = cstate->raw_fields[fieldno];

				pg_verifymbstr(fld, output_ptr - fld, false);
			}
		}

		/* Terminate attribute value in output area */
		*output_ptr++ = '\0';

		fieldno++;
		/* Done if we hit EOL instead of a delim */
		if (!found_delim)
		{
			cstate->stopped_processing_at_delim = false;
			break;
		}
	}

	/*
	 * Make note of the stopping point in 'line_buf.cursor', so that we
	 * can send the rest to the QE later.
	 */
	cstate->line_buf.cursor = cur_ptr - cstate->line_buf.data;

	/* Clean up state of attribute_buf */
	output_ptr--;
	Assert(*output_ptr == '\0');
	cstate->attribute_buf.len = (output_ptr - cstate->attribute_buf.data);

	return fieldno;
}

/*
 * Parse the current line into separate attributes (fields),
 * performing de-escaping as needed.  This has exactly the same API as
 * CopyReadAttributesText, except we parse the fields according to
 * "standard" (i.e. common) CSV usage.
 */
static int
CopyReadAttributesCSV(CopyState cstate, int stop_processing_at_field)
{
	char		delimc = cstate->delim[0];
	char		quotec = cstate->quote[0];
	char		escapec = cstate->escape[0];
	int			fieldno;
	char	   *output_ptr;
	char	   *cur_ptr;
	char	   *line_end_ptr;

	/*
	 * We need a special case for zero-column tables: check that the input
	 * line is empty, and return.
	 */
	if (cstate->max_fields <= 0)
	{
		if (cstate->line_buf.len != 0)
			ereport(ERROR,
					(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
					 errmsg("extra data after last expected column")));
		return 0;
	}

	resetStringInfo(&cstate->attribute_buf);

	/*
	 * The de-escaped attributes will certainly not be longer than the input
	 * data line, so we can just force attribute_buf to be large enough and
	 * then transfer data without any checks for enough space.  We need to do
	 * it this way because enlarging attribute_buf mid-stream would invalidate
	 * pointers already stored into cstate->raw_fields[].
	 */
	if (cstate->attribute_buf.maxlen <= cstate->line_buf.len)
		enlargeStringInfo(&cstate->attribute_buf, cstate->line_buf.len);
	output_ptr = cstate->attribute_buf.data;

	/* set pointer variables for loop */
	cur_ptr = cstate->line_buf.data + cstate->line_buf.cursor;
	line_end_ptr = cstate->line_buf.data + cstate->line_buf.len;

	/* Outer loop iterates over fields */
	fieldno = 0;
	for (;;)
	{
		bool		found_delim = false;
		bool		saw_quote = false;
		char	   *start_ptr;
		char	   *end_ptr;
		int			input_len;

		/*
		 * In QD, stop once we have processed the last field we need in the QD.
		 */
		if (fieldno == stop_processing_at_field)
		{
			cstate->stopped_processing_at_delim = true;
			break;
		}

		/* Make sure there is enough space for the next value */
		if (fieldno >= cstate->max_fields)
		{
			cstate->max_fields *= 2;
			cstate->raw_fields =
				repalloc(cstate->raw_fields, cstate->max_fields * sizeof(char *));
		}

		/* Remember start of field on both input and output sides */
		start_ptr = cur_ptr;
		cstate->raw_fields[fieldno] = output_ptr;

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
				end_ptr = cur_ptr;
				if (cur_ptr >= line_end_ptr)
					goto endfield;
				c = *cur_ptr++;
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
				*output_ptr++ = c;
			}

			/* In quote */
			for (;;)
			{
				end_ptr = cur_ptr;
				if (cur_ptr >= line_end_ptr)
					ereport(ERROR,
							(errcode(ERRCODE_BAD_COPY_FILE_FORMAT),
							 errmsg("unterminated CSV quoted field")));

				c = *cur_ptr++;

				/* escape within a quoted field */
				if (c == escapec)
				{
					/*
					 * peek at the next char if available, and escape it if it
					 * is an escape char or a quote char
					 */
					if (cur_ptr < line_end_ptr)
					{
						char		nextc = *cur_ptr;

						if (nextc == escapec || nextc == quotec)
						{
							*output_ptr++ = nextc;
							cur_ptr++;
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
				*output_ptr++ = c;
			}
		}
endfield:

		/* Terminate attribute value in output area */
		*output_ptr++ = '\0';

		/* Check whether raw input matched null marker */
		input_len = end_ptr - start_ptr;
		if (!saw_quote && input_len == cstate->null_print_len &&
			strncmp(start_ptr, cstate->null_print, input_len) == 0)
			cstate->raw_fields[fieldno] = NULL;

		fieldno++;
		/* Done if we hit EOL instead of a delim */
		if (!found_delim)
		{
			cstate->stopped_processing_at_delim = false;
			break;
		}
	}

	/*
	 * Make note of the stopping point in 'line_buf.cursor', so that we
	 * can send the rest to the QE later.
	 */
	cstate->line_buf.cursor = cur_ptr - cstate->line_buf.data;

	/* Clean up state of attribute_buf */
	output_ptr--;
	Assert(*output_ptr == '\0');
	cstate->attribute_buf.len = (output_ptr - cstate->attribute_buf.data);

	return fieldno;
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
								  cstate->file_encoding,
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
	 * are infrequent.  To avoid overhead from calling CopySendData once per
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
				ptr += pg_encoding_mblen(cstate->file_encoding, ptr);
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
								  cstate->file_encoding,
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
					tptr += pg_encoding_mblen(cstate->file_encoding, tptr);
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
				ptr += pg_encoding_mblen(cstate->file_encoding, ptr);
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

/* remove end of line chars from end of a buffer */
void truncateEol(StringInfo buf, EolType eol_type)
{
	int one_back = buf->len - 1;
	int two_back = buf->len - 2;

	if(eol_type == EOL_CRNL)
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
 * copy_dest_startup --- executor startup
 */
static void
copy_dest_startup(DestReceiver *self pg_attribute_unused(), int operation pg_attribute_unused(), TupleDesc typeinfo pg_attribute_unused())
{
	if (Gp_role != GP_ROLE_EXECUTE)
		return;
	DR_copy    *myState = (DR_copy *) self;
	myState->cstate = BeginCopyToOnSegment(myState->queryDesc);
}

/*
 * copy_dest_receive --- receive one tuple
 */
static bool
copy_dest_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_copy    *myState = (DR_copy *) self;
	CopyState	cstate = myState->cstate;

	/* Make sure the tuple is fully deconstructed */
	slot_getallattrs(slot);

	/* And send the data */
	CopyOneRowTo(cstate, InvalidOid, slot_get_values(slot), slot_get_isnull(slot));
	myState->processed++;

	return true;
}

/*
 * copy_dest_shutdown --- executor end
 */
static void
copy_dest_shutdown(DestReceiver *self pg_attribute_unused())
{
	if (Gp_role != GP_ROLE_EXECUTE)
		return;
	DR_copy    *myState = (DR_copy *) self;
	EndCopyToOnSegment(myState->cstate);
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

	self->cstate = NULL;		/* need to be set later */
	self->queryDesc = NULL;		/* need to be set later */
	self->processed = 0;

	return (DestReceiver *) self;
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

	/* Set up data buffer to hold a chunk of data */
	MemSet(cstate->raw_buf, ' ', RAW_BUF_SIZE * sizeof(char));
	cstate->raw_buf[RAW_BUF_SIZE] = '\0';
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
static void
setEncodingConversionProc(CopyState cstate, int encoding, bool iswritable)
{
	Oid		conversion_proc;
	
	/*
	 * COPY FROM and RET: convert from file to server
	 * COPY TO   and WET: convert from server to file
	 */
	if (iswritable)
		conversion_proc = FindDefaultConversionProc(GetDatabaseEncoding(), encoding);
	else		
		conversion_proc = FindDefaultConversionProc(encoding, GetDatabaseEncoding());
	
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

static GpDistributionData *
InitDistributionData(CopyState cstate, EState *estate)
{
	GpDistributionData *distData;
	GpPolicy   *policy;
	HTAB	   *hashmap;
	CdbHash	   *cdbHash;
	bool		multi_dist_policy;

	multi_dist_policy = estate->es_result_partitions
		&& !partition_policies_equal(cstate->rel->rd_cdbpolicy,
									 estate->es_result_partitions);
	if (!multi_dist_policy)
	{
		/*
		 * A non-partitioned table, or all the partitions have identical
		 * distribution policies.
		 */
		policy = GpPolicyCopy(cstate->rel->rd_cdbpolicy);
		cdbHash = makeCdbHashForRelation(cstate->rel);
		hashmap = NULL;
	}
	else
	{
		/*
		 * This is a partitioned table that has multiple, different
		 * distribution policies.
		 *
		 * We don't support partitioned tables with arbitrary distribution
		 * policies in the partitions. But we do support having randomly
		 * distributed partitions, in a hash partitioned table. Also, it's
		 * possible that the physical attribute numbers of the distribution
		 * keys are different in different partitions, even if they were
		 * specified with the same DISTRIBUTED BY columns, if some partitions
		 * contain dropped columns.
		 */
		HASHCTL		hash_ctl;

		MemSet(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(Oid);
		hash_ctl.entrysize = sizeof(GpDistributionData);
		hash_ctl.hash = oid_hash;
		hash_ctl.hcxt = CurrentMemoryContext;

		hashmap = hash_create("partition cdb hash map",
		                      100, /* initial guess */
		                      &hash_ctl,
		                      HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

		/* We will use the policies from individual partitions. */
		policy = NULL;
		cdbHash = NULL;
	}

	distData = palloc(sizeof(GpDistributionData));
	distData->policy = policy;
	distData->cdbHash = cdbHash;
	distData->hashmap = hashmap;

	return distData;
}

static void
FreeDistributionData(GpDistributionData *distData)
{
	if (distData)
	{
		if (distData->policy)
			pfree(distData->policy);
		if (distData->cdbHash)
			pfree(distData->cdbHash);
		if (distData->hashmap)
			hash_destroy(distData->hashmap);
		pfree(distData);
	}
}

/*
 * Compute which fields need to be processed in the QD, and which ones can
 * be delayed to the QE.
 */
static void
InitCopyFromDispatchSplit(CopyState cstate, GpDistributionData *distData,
						  EState *estate)
{
	int			first_qe_processed_field = 0;
	Bitmapset  *needed_cols = NULL;
	ListCell   *lc;

	if (cstate->binary)
	{
		foreach(lc, cstate->attnumlist)
		{
			AttrNumber attnum = lfirst_int(lc);
			needed_cols = bms_add_member(needed_cols, attnum);
			first_qe_processed_field++;
		}
	}
	else
	{
		int			fieldno;
		/*
		 * We need all the columns that form the distribution key.
		 */
		if (distData->policy)
		{
			for (int i = 0; i < distData->policy->nattrs; i++)
				needed_cols = bms_add_member(needed_cols, distData->policy->attrs[i]);
		}

		/*
		 * If the target is partitioned, get the columns needed for partitioning
		 * keys, and for distribution keys of each partition.
		 */
		if (estate->es_result_partitions)
			needed_cols = GetTargetKeyCols(RelationGetRelid(estate->es_result_relation_info->ri_RelationDesc),
										   estate->es_result_partitions, needed_cols,
										   distData->policy == NULL, estate);

		/* Get the max fieldno that contains one of the needed attributes. */
		fieldno = 0;
		foreach(lc, cstate->attnumlist)
		{
			AttrNumber attnum = lfirst_int(lc);

			if (bms_is_member(attnum, needed_cols))
				first_qe_processed_field = fieldno + 1;
			fieldno++;
		}
	}

	/* If the file contains OIDs, it's the first field. */
	if (cstate->file_has_oids)
		first_qe_processed_field++;

	cstate->first_qe_processed_field = first_qe_processed_field;

	if (Test_copy_qd_qe_split)
	{
		if (first_qe_processed_field ==
			list_length(cstate->attnumlist) + (cstate->file_has_oids ? 1 : 0))
			elog(INFO, "all fields will be processed in the QD");
		else
			elog(INFO, "first field processed in the QE: %d", first_qe_processed_field);
	}
}

/*
 * Recursive helper function for InitCopyFromDispatchSplit(), to get
 * the columns needed in QD for a partition and its subpartitions.
 */
static Bitmapset *
GetTargetKeyCols(Oid relid, PartitionNode *children, Bitmapset *needed_cols,
				 bool distkeys, EState *estate)
{
	int			i;

	/*
	 * Partition key columns.
	 *
	 * Note: paratts[] stores the attribute numbers, in terms of the root
	 * partition. That's exactly what we want.
	 */
	if (children)
	{
		for (i = 0; i < children->part->parnatts; i++)
			needed_cols = bms_add_member(needed_cols, children->part->paratts[i]);
	}

	/*
	 * Distribution key columns
	 *
	 * These are in terms of the partition itself! We need to map them to
	 * the root partition's attribute numbers.
	 *
	 * (At the moment, this is more complicated than necessary, because GPDB
	 * doesn't support partitions that have differing distribution policies,
	 * except that child partitions can be randomly distributed, even though
	 * the parent is hash distributed.)
	 */
	if (distkeys)
	{
		ResultRelInfo *partrr;
		GpPolicy *partPolicy;
		TupleConversionMap *map;

		partrr = targetid_get_partition(relid, estate, false);
		map = partrr->ri_partInsertMap;
		partPolicy = partrr->ri_RelationDesc->rd_cdbpolicy;

		if (partPolicy)
		{
			for (i = 0; i < partPolicy->nattrs; i++)
			{
				AttrNumber	partAttNum = partPolicy->attrs[i];
				AttrNumber	parentAttNum;

				/* Map this partition's attribute number to the parent's. */
				if (map)
				{
					parentAttNum = map->attrMap[partAttNum - 1];
					if (parentAttNum > map->indesc->natts)
						elog(ERROR, "could not find mapping partition distribution key column %d in parent relation",
							 partAttNum);
				}
				else
					parentAttNum = partAttNum;

				needed_cols = bms_add_member(needed_cols, parentAttNum);
			}
		}
	}

	/* Recurse to subpartitions */
	if (children)
	{
		for (i = 0; i < children->num_rules; i++)
		{
			PartitionRule *pr = children->rules[i];

			needed_cols = GetTargetKeyCols(pr->parchildrelid, pr->children,
										   needed_cols, distkeys, estate);
		}
	}

	return needed_cols;
}

/* Get distribution policy for specific part */
static GpDistributionData *
GetDistributionPolicyForPartition(GpDistributionData *mainDistData,
								  ResultRelInfo *resultRelInfo,
								  MemoryContext context)
{

	/*
	 * If we are copying into a partitioned table whose partitions have
	 * differing distribution policies, get the policy for this particular
	 * child partition.
	 */
	if (mainDistData->hashmap)
	{
		Oid			relid;
		GpDistributionData *d;
		bool		found;

		relid = resultRelInfo->ri_RelationDesc->rd_id;

		d = hash_search(mainDistData->hashmap, &(relid), HASH_ENTER, &found);
		if (!found)
		{
			Relation	rel = resultRelInfo->ri_RelationDesc;
			MemoryContext oldcontext;

			/*
			 * Make sure this all persists the current iteration.
			 */
			oldcontext = MemoryContextSwitchTo(context);

			d->cdbHash = makeCdbHashForRelation(rel);
			d->policy = GpPolicyCopy(rel->rd_cdbpolicy);

			MemoryContextSwitchTo(oldcontext);
		}

		return d;
	}
	else
		return mainDistData;
}

static unsigned int
GetTargetSeg(GpDistributionData *distData, Datum *baseValues, bool *baseNulls)
{
	unsigned int target_seg;
	CdbHash	   *cdbHash = distData->cdbHash;
	GpPolicy   *policy = distData->policy; /* the partitioning policy for this table */
	AttrNumber	p_nattrs;	/* num of attributes in the distribution policy */

	/*
	 * These might be NULL, if we're called with a "main" GpDistributionData,
	 * for a partitioned table with heterogenous partitions. The caller
	 * should've used GetDistributionPolicyForPartition() to get the right
	 * distdata object for the partition.
	 */
	if (!policy)
		elog(ERROR, "missing distribution policy.");
	if (!cdbHash)
		elog(ERROR, "missing cdbhash");

	/*
	 * At this point in the code, baseValues[x] is final for this
	 * data row -- either the input data, a null or a default
	 * value is in there, and constraints applied.
	 *
	 * Perform a cdbhash on this data row. Perform a hash operation
	 * on each attribute.
	 */
	p_nattrs = policy->nattrs;
	if (p_nattrs > 0)
	{
		cdbhashinit(cdbHash);

		for (int i = 0; i < p_nattrs; i++)
		{
			/* current attno from the policy */
			AttrNumber h_attnum = policy->attrs[i];

			cdbhash(cdbHash, i + 1,
					baseValues[h_attnum - 1],
					baseNulls[h_attnum - 1]);
		}

		target_seg = cdbhashreduce(cdbHash); /* hash result segment */
	}
	else
	{
		/*
		 * Randomly distributed. Pick a segment at random.
		 */
		target_seg = cdbhashrandomseg(policy->numsegments);
	}

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

	elog(DEBUG5, "COPY ... PROGRAM command: %s", program_pipes->shexec);
	if (program_pipes->pid == -1)
	{
		errno = save_errno;
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

	/* just return if pipes not created, like when relation does not exist */
	if (!cstate->program_pipes)
	{
		return;
	}
	
	ret = pclose_with_stderr(cstate->program_pipes->pid, cstate->program_pipes->pipes, &sinfo);

	if (ret == 0 || !ifThrow)
	{
		return;
	}

	if (ret == -1)
	{
		/* pclose()/wait4() ended with an error; errno should be valid */
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("can not close pipe: %m")));
	}
	else if (!WIFSIGNALED(ret))
	{
		/*
		 * pclose() returned the process termination state.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_SQL_ROUTINE_EXCEPTION),
				 errmsg("command error message: %s", sinfo.data)));
	}
}

static List *
parse_joined_option_list(char *str, char *delimiter)
{
	char	   *token;
	char	   *comma;
	const char *whitespace = " \t\n\r";
	List	   *cols = NIL;
	int			encoding = GetDatabaseEncoding();

	token = strtokx2(str, whitespace, delimiter, "\"",
					 0, false, false, encoding);

	while (token)
	{
		if (token[0] == ',')
			break;

		cols = lappend(cols, makeString(pstrdup(token)));

		/* consume the comma if any */
		comma = strtokx2(NULL, whitespace, delimiter, "\"",
						 0, false, false, encoding);
		if (!comma || comma[0] != ',')
			break;

		token = strtokx2(NULL, whitespace, delimiter, "\"",
						 0, false, false, encoding);
	}

	return cols;
}
