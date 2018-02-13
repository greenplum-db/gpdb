/*-------------------------------------------------------------------------
 *
 * Small tests for gp_libpqwalreceiver
 *
 *-----------------------------------------------------------------------*/

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "access/xlog_internal.h"
#include "replication/walprotocol.h"
#include "replication/walreceiver.h"
#include "cdb/cdbappendonlyam.h"

PG_MODULE_MAGIC;

#define NAPTIME_PER_CYCLE 100	/* max sleep time between cycles (100ms) */

static struct
{
	XLogRecPtr	Write;
	XLogRecPtr	Flush;
} LogstreamResult;

static void test_XLogWalRcvProcessMsg(unsigned char type, char *buf,
									  Size len, XLogRecPtr *logStreamStart);
static void test_XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr);
static void test_XLogWalRcvSendReply(void);
static void test_PrintLog(char *type, XLogRecPtr walPtr,
						  TimestampTz sendTime);

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

static void
string_to_xlogrecptr(text *location, XLogRecPtr *rec)
{
	char *locationstr = DatumGetCString(
		DirectFunctionCall1(textout,
							PointerGetDatum(location)));

	if (sscanf(locationstr, "%X/%X", &rec->xlogid, &rec->xrecoff) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not parse transaction log location \"%s\"",
						locationstr)));
}

Datum
test_connect(PG_FUNCTION_ARGS)
{
	char *conninfo = TextDatumGetCString(PG_GETARG_DATUM(0));
	text *location = PG_GETARG_TEXT_P(1);
	bool result;
	XLogRecPtr startpoint;

	string_to_xlogrecptr(location, &startpoint);

	result = walrcv_connect(conninfo, startpoint);
	PG_RETURN_BOOL(result);
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
	bool		result;
	unsigned char type;
	char	   *buf;
	int			len;

	result = walrcv_receive(NAPTIME_PER_CYCLE, &type, &buf, &len);

	PG_RETURN_BOOL(result);
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
	text	   *start_location = PG_GETARG_TEXT_P(0);
	text       *end_location = PG_GETARG_TEXT_P(1);
	XLogRecPtr startpoint;
	XLogRecPtr endpoint;
	unsigned char type;
	char   *buf;
	int     len;

	string_to_xlogrecptr(start_location, &startpoint);
	string_to_xlogrecptr(end_location, &endpoint);

	for (int i=0; i < NUM_RETRIES; i++)
	{
		if (walrcv_receive(NAPTIME_PER_CYCLE, &type, &buf, &len))
		{
			XLogRecPtr logStreamStart;
			/* Accept the received data, and process it */
			test_XLogWalRcvProcessMsg(type, buf, len, &logStreamStart);

			/* Compare received everthing from start */
			if (startpoint.xlogid != logStreamStart.xlogid ||
				startpoint.xrecoff != logStreamStart.xrecoff)
			{
				elog(ERROR, "Start point (%X/%X) differs from expected (%X/%X)",
					 logStreamStart.xlogid, logStreamStart.xrecoff,
					 startpoint.xlogid, startpoint.xrecoff);
			}

			/* Compare received everything till end */
			if (endpoint.xlogid != LogstreamResult.Write.xlogid ||
				endpoint.xrecoff != LogstreamResult.Write.xrecoff)
			{
				elog(ERROR, "End point (%X/%X) differs from expected (%X/%X)",
					 LogstreamResult.Write.xlogid, LogstreamResult.Write.xrecoff,
					 endpoint.xlogid, endpoint.xrecoff);
			}

			PG_RETURN_BOOL(true);
		}

		elog(LOG, "walrcv_receive didn't return anything, retry...%d", i);
	}

	PG_RETURN_BOOL(false);
}

/*
 * Accept the message from XLOG stream, and process it.
 */
static void
test_XLogWalRcvProcessMsg(unsigned char type, char *buf,
						  Size len, XLogRecPtr *logStreamStart)
{
	switch (type)
	{
		case 'w':				/* WAL records */
			{
				WalDataMessageHeader msghdr;

				if (len < sizeof(WalDataMessageHeader))
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid WAL message received from primary")));
				/* memcpy is required here for alignment reasons */
				memcpy(&msghdr, buf, sizeof(WalDataMessageHeader));
				logStreamStart->xlogid = msghdr.dataStart.xlogid;
				logStreamStart->xrecoff = msghdr.dataStart.xrecoff;

				test_PrintLog("wal start records",
							  msghdr.dataStart, msghdr.sendTime);
				test_PrintLog("wal end records",
							  msghdr.walEnd, msghdr.sendTime);

				buf += sizeof(WalDataMessageHeader);
				len -= sizeof(WalDataMessageHeader);

				test_XLogWalRcvWrite(buf, len, msghdr.dataStart);
				break;
			}
		case 'k':				/* Keepalive */
			{
				PrimaryKeepaliveMessage keepalive;

				if (len != sizeof(PrimaryKeepaliveMessage))
					ereport(ERROR,
							(errcode(ERRCODE_PROTOCOL_VIOLATION),
							 errmsg_internal("invalid keepalive message received from primary")));
				/* memcpy is required here for alignment reasons */
				memcpy(&keepalive, buf, sizeof(PrimaryKeepaliveMessage));

				elog(INFO, "keep alive: %X/%X at %s",
						keepalive.walEnd.xlogid,
						keepalive.walEnd.xrecoff,
						timestamptz_to_str(keepalive.sendTime));
				test_PrintLog("keep alive",
							  keepalive.walEnd, keepalive.sendTime);
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
	static char		   *recvFilePath = NULL;
	static TimeLineID recvFileTLI = 1;
	static uint32 recvId = 0;
	static uint32 recvSeg = 0;

	while (nbytes > 0)
	{
		int			segbytes;
		int			startoff;
		int			byteswritten;

		if (recvFilePath == NULL || !XLByteInSeg(recptr, recvId, recvSeg))
		{
			if (recvFilePath == NULL)
			{
				recvFilePath = palloc0(MAXFNAMELEN);
			}

			XLByteToSeg(recptr, recvId, recvSeg);
			XLogFileName(recvFilePath, recvFileTLI, recvId, recvSeg);
			elog(DEBUG1, "would open: %s", recvFilePath);
			recvFileTLI = 1;
		}

		startoff = recptr.xrecoff % XLogSegSize;

		if (startoff + nbytes > XLogSegSize)
			segbytes = XLogSegSize - startoff;
		else
			segbytes = nbytes;

		/* Assuming segbytes are written */
		byteswritten = segbytes;

		XLByteAdvance(recptr, byteswritten);

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
	char		buf[sizeof(StandbyReplyMessage) + 1];
	TimestampTz now;
	static StandbyReplyMessage reply_message;

	now = GetCurrentTimestamp();

	reply_message.write = LogstreamResult.Write;
	reply_message.flush = LogstreamResult.Flush;
	reply_message.apply = LogstreamResult.Flush;
	reply_message.sendTime = now;

	elog(DEBUG1, "sending: write = %X/%X, flush = %X/%X, apply = %X/%X at %s",
			reply_message.write.xlogid,
			reply_message.write.xrecoff,
			reply_message.flush.xlogid,
			reply_message.flush.xrecoff,
			reply_message.apply.xlogid,
			reply_message.apply.xrecoff,
			timestamptz_to_str(now));

	buf[0] = 'r';
	memcpy(&buf[1], &reply_message, sizeof(StandbyReplyMessage));
	walrcv_send(buf, sizeof(StandbyReplyMessage) + 1);
}

/*
 * Just show the walEnd/sendTime information
 */
static void
test_PrintLog(char *type, XLogRecPtr walPtr,
						   TimestampTz sendTime)
{
	elog(DEBUG1, "%s: %X/%X at %s", type, walPtr.xlogid, walPtr.xrecoff,
		 timestamptz_to_str(sendTime));
}


/*
 * Verify that XLOG records are being generated for AO tables and are getting
 * shipped to the WAL receiver.
 */
Datum
test_xlog_ao(PG_FUNCTION_ARGS)
{
	int		  num_found = 0;

	PG_RETURN_INT32(num_found);
}
