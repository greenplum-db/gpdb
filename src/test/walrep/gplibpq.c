/*-------------------------------------------------------------------------
 *
 * Small tests for gp_libpqwalreceiver
 *
 *-----------------------------------------------------------------------*/
#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"
#include "access/xlog_internal.h"
#include "replication/walreceiver.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbappendonlyxlog.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "utils/timestamp.h"

PG_MODULE_MAGIC;

#define NAPTIME_PER_CYCLE 100	/* max sleep time between cycles (100ms) */

static struct
{
	XLogRecPtr	Write;
	XLogRecPtr	Flush;
} LogstreamResult;


typedef struct CheckAoRecordResult
{
	char ao_xlog_record_type;
	uint32 xrecoff;
	Size len;
	xl_ao_target target;
} CheckAoRecordResult;

static void test_XLogWalRcvProcessMsg(unsigned char type, char *buf,
									  Size len, XLogRecPtr *logStreamStart);
static void test_XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr);
static void test_XLogWalRcvSendReply(void);
static void test_PrintLog(char *type, XLogRecPtr walPtr,
						  TimestampTz sendTime);

static uint32 check_ao_record_present(unsigned char type, char *buf, Size len,
									  uint32 xrecoff, CheckAoRecordResult *aorecordresults);

void _PG_init(void);

Datum test_connect(PG_FUNCTION_ARGS);
Datum test_disconnect(PG_FUNCTION_ARGS);
Datum test_receive(PG_FUNCTION_ARGS);
Datum test_send(PG_FUNCTION_ARGS);
Datum test_receive_and_verify(PG_FUNCTION_ARGS);
Datum test_xlog_ao(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(test_connect);
PG_FUNCTION_INFO_V1(test_disconnect);
PG_FUNCTION_INFO_V1(test_receive);
PG_FUNCTION_INFO_V1(test_send);
PG_FUNCTION_INFO_V1(test_receive_and_verify);
PG_FUNCTION_INFO_V1(test_xlog_ao);

/*
 * Module load callback.
 *
 * Initializes the libpqwalreceiver callbacks, by calling libpqwalreceiver's
 * initialization routine. In a real walreceiver, this is done during
 * the walreceiver process startup.
 */
void
_PG_init(void)
{
	libpqwalreceiver_PG_init();
}

Datum
test_connect(PG_FUNCTION_ARGS)
{
	char *conninfo = TextDatumGetCString(PG_GETARG_DATUM(0));

	walrcv_connect(conninfo);
	PG_RETURN_BOOL(true);
}

Datum
test_disconnect(PG_FUNCTION_ARGS)
{
	walrcv_disconnect();

	PG_RETURN_BOOL(true);
}

Datum
test_receive(PG_FUNCTION_ARGS)
{
	int		result;
	char	   *buf;

	result = walrcv_receive(NAPTIME_PER_CYCLE, &buf);

	PG_RETURN_INT32(result);
}

Datum
test_send(PG_FUNCTION_ARGS)
{
	test_XLogWalRcvSendReply();

	PG_RETURN_BOOL(true);
}

#define NUM_RETRIES 50

Datum
test_receive_and_verify(PG_FUNCTION_ARGS)
{
	XLogRecPtr startpoint = PG_GETARG_LSN(0);
	XLogRecPtr endpoint = PG_GETARG_LSN(1);
	char   *buf;
	int     len;
	TimeLineID  startpointTLI;

	/* For now hard-coding it to 1 */
	startpointTLI = 1;
	walrcv_startstreaming(startpointTLI, startpoint, NULL);

	for (int i=0; i < NUM_RETRIES; i++)
	{
		len = walrcv_receive(NAPTIME_PER_CYCLE, &buf);
		if (len > 0)
		{
			XLogRecPtr logStreamStart = InvalidXLogRecPtr;

			/* Accept the received data, and process it */
			test_XLogWalRcvProcessMsg(buf[0], &buf[1], len-1, &logStreamStart);

			/* Compare received everthing from start */
			if (startpoint != logStreamStart)
			{
				elog(ERROR, "Start point (%X/%X) differs from expected (%X/%X)",
					 (uint32) (logStreamStart >> 32), (uint32) logStreamStart,
					 (uint32) (startpoint >> 32), (uint32) startpoint);
			}

			/* Compare received everything till end */
			if (endpoint != LogstreamResult.Write)
			{
				elog(ERROR, "End point (%X/%X) differs from expected (%X/%X)",
					 (uint32) (LogstreamResult.Write >> 32), (uint32) LogstreamResult.Write,
					 (uint32) (endpoint >> 32), (uint32) endpoint);
			}

			PG_RETURN_BOOL(true);
		}

		elog(LOG, "walrcv_receive didn't return anything, retry...%d", i);
	}

	PG_RETURN_BOOL(false);
}

static void
test_XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len,
						  XLogRecPtr *logStreamStart)
{
	int			hdrlen;
	XLogRecPtr	dataStart;
	XLogRecPtr	walEnd;
	TimestampTz sendTime;
	bool		replyRequested;
	StringInfoData incoming_message;
	initStringInfo(&incoming_message);

	switch (type)
	{
		case 'w':				/* WAL records */
			{
				/* copy message to StringInfo */
				hdrlen = sizeof(int64) + sizeof(int64) + sizeof(int64);
				if (len < hdrlen)
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid WAL message received from primary")));
				appendBinaryStringInfo(&incoming_message, buf, hdrlen);

				/* read the fields */
				dataStart = pq_getmsgint64(&incoming_message);
				walEnd = pq_getmsgint64(&incoming_message);
				sendTime = IntegerTimestampToTimestampTz(
										  pq_getmsgint64(&incoming_message));
				*logStreamStart = dataStart;

				test_PrintLog("wal start records", dataStart, sendTime);
				test_PrintLog("wal end records", walEnd, sendTime);

				buf += hdrlen;
				len -= hdrlen;
				test_XLogWalRcvWrite(buf, len, dataStart);
				break;
			}
		case 'k':				/* Keepalive */
			{
				/* copy message to StringInfo */
				hdrlen = sizeof(int64) + sizeof(int64) + sizeof(char);
				if (len != hdrlen)
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid keepalive message received from primary")));
				appendBinaryStringInfo(&incoming_message, buf, hdrlen);

				/* read the fields */
				walEnd = pq_getmsgint64(&incoming_message);
				sendTime = IntegerTimestampToTimestampTz(
										  pq_getmsgint64(&incoming_message));
				replyRequested = pq_getmsgbyte(&incoming_message);

				elog(INFO, "keep alive: %X/%X at %s",
					 (uint32) (walEnd >> 32), (uint32) walEnd,
						timestamptz_to_str(sendTime));
				test_PrintLog("keep alive", walEnd, sendTime);

				break;
			}
		default:
			ereport(ERROR,
					(errcode(ERRCODE_PROTOCOL_VIOLATION),
					 errmsg_internal("invalid replication message type %d",
									 type)));
	}
}

/*
 * Write XLOG data to disk.
 */
static void
test_XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr)
{
	int			startoff;
	int			byteswritten;

	while (nbytes > 0)
	{
		int			segbytes;

		startoff = recptr % XLogSegSize;

		if (startoff + nbytes > XLogSegSize)
			segbytes = XLogSegSize - startoff;
		else
			segbytes = nbytes;

		/* Assuming segbytes are written */
		byteswritten = segbytes;
		recptr += byteswritten;

		nbytes -= byteswritten;
		buf += byteswritten;

		LogstreamResult.Write = recptr;
		/* Assuming it's flushed too */
		LogstreamResult.Flush = recptr;
	}
}

static void
test_XLogWalRcvSendReply(void)
{
	StringInfoData reply_message;
	initStringInfo(&reply_message);

	pq_sendbyte(&reply_message, 'r');
	pq_sendint64(&reply_message, LogstreamResult.Write);
	pq_sendint64(&reply_message, LogstreamResult.Flush);
	pq_sendint64(&reply_message, LogstreamResult.Flush);
	pq_sendint64(&reply_message, GetCurrentIntegerTimestamp());
	pq_sendbyte(&reply_message, false);

	walrcv_send(reply_message.data, reply_message.len);
}

/*
 * Just show the walEnd/sendTime information
 */
static void
test_PrintLog(char *type, XLogRecPtr walPtr,
						   TimestampTz sendTime)
{
	elog(DEBUG1, "%s: %X/%X at %s", type, (uint32) (walPtr >> 32), (uint32) walPtr,
		 timestamptz_to_str(sendTime));
}


/*
 * Verify that XLOG records are being generated for AO tables and are getting
 * shipped to the WAL receiver.
 */
Datum
test_xlog_ao(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	int			nattr = 8;
	Datum    values[8];
	bool   nulls[8];
	HeapTuple tuple;
	CheckAoRecordResult *aorecordresults;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		*/
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "recordlen", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "record_type", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "recordlen", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "spcNode", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "dbNode", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "relNode", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "segment_filenum", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "file_offset", INT8OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		aorecordresults = (CheckAoRecordResult *) palloc0(sizeof(CheckAoRecordResult) * 100);
		funcctx->user_fctx = (void *) aorecordresults;

		char         *conninfo = TextDatumGetCString(PG_GETARG_DATUM(0));
		XLogRecPtr    startpoint = PG_GETARG_LSN(1);
		TimeLineID  startpointTLI;
		char         *buf;
		int           len;
		uint32        xrecoff;

		xrecoff = (uint32)startpoint;

		walrcv_connect(conninfo);
		/* For now hard-coding it to 1 */
		startpointTLI = 1;
		walrcv_startstreaming(startpointTLI, startpoint, NULL);

		for (int i = 0; i < NUM_RETRIES; i++)
		{
			len = walrcv_receive(NAPTIME_PER_CYCLE, &buf);
			if (len > 0)
			{
				funcctx->max_calls = check_ao_record_present(buf[0], &buf[1],
															 len -1, xrecoff,
															 aorecordresults);
				break;
			}
			else
				elog(LOG, "walrcv_receive didn't return anything, retry...%d", i);
		}

		walrcv_disconnect();

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	aorecordresults = (CheckAoRecordResult *) funcctx->user_fctx;

	while(funcctx->call_cntr < funcctx->max_calls)
	{
		CheckAoRecordResult *result = &aorecordresults[funcctx->call_cntr];

		values[0] = Int32GetDatum(result->xrecoff);
		if(result->ao_xlog_record_type == XLOG_APPENDONLY_INSERT)
			values[1] = CStringGetTextDatum("XLOG_APPENDONLY_INSERT");
		if(result->ao_xlog_record_type == XLOG_APPENDONLY_TRUNCATE)
			values[1] = CStringGetTextDatum("XLOG_APPENDONLY_TRUNCATE");

		values[2] = Int32GetDatum(result->len);
		values[3] = ObjectIdGetDatum(result->target.node.spcNode);
		values[4] = ObjectIdGetDatum(result->target.node.dbNode);
		values[5] = ObjectIdGetDatum(result->target.node.relNode);
		values[6] = Int32GetDatum(result->target.segment_filenum);
		values[7] = Int64GetDatum(result->target.offset);

		MemSet(nulls, false, sizeof(nulls));

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}

	SRF_RETURN_DONE(funcctx);
}

/*
 * Verify that AO/AOCO XLOG record is present in buf.
 * Returns the number of AO/AOCO XLOG records found in buf.
 */
static uint32
check_ao_record_present(unsigned char type, char *buf, Size len,
						uint32 xrecoff,	CheckAoRecordResult *aorecordresults)
{
	uint32               i = 0;
	int                  num_found = 0;
	XLogRecPtr  dataStart;
	XLogRecPtr  walEnd;
	TimestampTz sendTime;
	MemSet(aorecordresults, 0, sizeof(CheckAoRecordResult));
	int         hdrlen = sizeof(int64) + sizeof(int64) + sizeof(int64);
	StringInfoData incoming_message;
	initStringInfo(&incoming_message);

	if (type != 'w')
		return num_found;

	if (len < hdrlen)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg_internal("invalid WAL message received from primary")));

	Assert(buf != NULL);
	appendBinaryStringInfo(&incoming_message, buf, hdrlen);

	/* read the fields */
	dataStart = pq_getmsgint64(&incoming_message);
	walEnd = pq_getmsgint64(&incoming_message);
	sendTime = IntegerTimestampToTimestampTz(
		pq_getmsgint64(&incoming_message));
	buf += hdrlen;
	len -= hdrlen;

	test_PrintLog("wal start record", dataStart, sendTime);
	test_PrintLog("wal end record", walEnd, sendTime);

	/* process the xlog records one at a time and check if it is an AO/AOCO record */
	while (i < len)
	{
		XLogRecord 		   *xlrec = (XLogRecord *)(buf + i);
		XLogPageHeaderData *hdr = (XLogPageHeaderData *)xlrec;
		uint8	            info = xlrec->xl_info & ~XLR_INFO_MASK;
		uint32 			    avail_in_block = XLOG_BLCKSZ - ((xrecoff + i) % XLOG_BLCKSZ);

		elog(DEBUG1, "len/offset:%u/%u, avail_in_block = %u",
			 xlrec->xl_tot_len, i, avail_in_block);

		if (hdr->xlp_magic == XLOG_PAGE_MAGIC)
		{
			/*
			 * If we encounter a page header, check if it is a continuation
			 * record and skip the page header and the remaining data to move
			 * on to the next xlog record. If it is not a continuation record,
			 * skip only the page header and move on to the next record.
			 */
			if (hdr->xlp_info & XLP_FIRST_IS_CONTRECORD)
			{
				elog(DEBUG1, "remaining length of record = %u", hdr->xlp_rem_len);
				if (hdr->xlp_rem_len > XLOG_BLCKSZ - XLogPageHeaderSize(hdr))
					i += XLOG_BLCKSZ;
				else
					i += MAXALIGN(XLogPageHeaderSize(hdr) + hdr->xlp_rem_len);
			}
			else
			{
				i += XLogPageHeaderSize(hdr);
				elog(DEBUG1, "XLOG_PAGE_MAGIC else, i:%u", i);
			}
		}
		else if (xlrec->xl_rmid == RM_APPEND_ONLY_ID)
		{
			CheckAoRecordResult *aorecordresult = &aorecordresults[num_found];
			aorecordresult->xrecoff = xrecoff + i;

			if (xlrec->xl_tot_len > avail_in_block)
			{
				Assert(avail_in_block >= sizeof(xl_ao_target));

				/*
				 * The AO record has been split across two pages. Create a
				 * temporary buffer to combine the split record back.
				 */
				char *tmpbuffer = (char *) palloc(xlrec->xl_tot_len);
				memcpy(tmpbuffer, buf + i, avail_in_block);

				/* Move on to the continuation record on the next page */
				i += avail_in_block;
				elog(DEBUG1, "AO record split found, i: %u, avail_in_block: %u", i, avail_in_block);

				/* Construct the continuation record to get the remaining length */
				XLogPageHeaderData *hdr_cont = (XLogPageHeaderData *)(buf + i);
				elog(DEBUG1, "combining AO record split with XLogContRecord xl_rem_len %u",
					 hdr_cont->xlp_rem_len);

				/*
				 * Move on to the second part of the split AO record and copy
				 * the second part into the temporary buffer. The second part
				 * does not contain a header and is directly after the
				 * continuation record (no MAXALIGN padding).
				 */
				i += XLogPageHeaderSize(hdr_cont);
				memcpy(tmpbuffer + avail_in_block,
					   buf + i,
					   hdr_cont->xlp_rem_len);

				/*
				 * Our split AO record is now combined back. Set the i to the
				 * next XLog record (may need MAXALIGN padding).
				 */
				xlrec = (XLogRecord *) tmpbuffer;
				i = MAXALIGN(i + hdr_cont->xlp_rem_len);
			}
			else
			{
				i += MAXALIGN(xlrec->xl_tot_len);
				elog(DEBUG1, "RM_APPEND_ONLY_ID, else, i: %u", i);
			}

			xl_ao_target *xlaorecord = (xl_ao_target*) XLogRecGetData(xlrec);

			aorecordresult->target.node.spcNode = xlaorecord->node.spcNode;
			aorecordresult->target.node.dbNode = xlaorecord->node.dbNode;
			aorecordresult->target.node.relNode = xlaorecord->node.relNode;
			aorecordresult->target.segment_filenum = xlaorecord->segment_filenum;
			aorecordresult->target.offset = xlaorecord->offset;
			aorecordresult->len = xlrec->xl_len;
			aorecordresult->ao_xlog_record_type = info;

			num_found++;

		}
		else
		{
			/*
			 * Either the entire record is too long to fit into the current
			 * page or there is not enough space remaining in the current page
			 * to write the XLogHeader. So skip the remaining bytes to move
			 * onto the next page.
			 */
			if (xlrec->xl_tot_len > avail_in_block || avail_in_block < SizeOfXLogRecord)
			{
				i += avail_in_block;
				elog(DEBUG1, "record too long, i: %u, avail_in_block: %u, xlrec->xl_tot_len: %u", i, avail_in_block, xlrec->xl_tot_len);
			}
			else
			{
				i += MAXALIGN(xlrec->xl_tot_len);
				elog(DEBUG1, "default, else, i: %u, xlrec->xl_tot_len: %u", i, xlrec->xl_tot_len);
			}
		}
	}
	return num_found;
}
