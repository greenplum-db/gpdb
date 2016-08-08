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

PG_MODULE_MAGIC;

#define NAPTIME_PER_CYCLE 100	/* max sleep time between cycles (100ms) */

static struct
{
	XLogRecPtr	Write;
	XLogRecPtr	Flush;
} LogstreamResult;

static void test_XLogWalRcvProcessMsg(unsigned char type,
								char *buf, Size len);
static void test_XLogWalRcvWrite(char *buf, Size nbytes, XLogRecPtr recptr);
static void test_XLogWalRcvSendReply(void);
static void test_ProcessWalSndrMessage(char *type, XLogRecPtr walEnd,
						   TimestampTz sendTime);

Datum test_connect(PG_FUNCTION_ARGS);
Datum test_disconnect(PG_FUNCTION_ARGS);
Datum test_receive(PG_FUNCTION_ARGS);
Datum test_send(PG_FUNCTION_ARGS);
Datum test_scenario1(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(test_connect);
PG_FUNCTION_INFO_V1(test_disconnect);
PG_FUNCTION_INFO_V1(test_receive);
PG_FUNCTION_INFO_V1(test_send);
PG_FUNCTION_INFO_V1(test_scenario1);

Datum
test_connect(PG_FUNCTION_ARGS)
{
	char *conninfo = TextDatumGetCString(PG_GETARG_DATUM(0));
	bool	result;

	result = walrcv_connect(conninfo, GetRedoRecPtr());
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

Datum
test_scenario1(PG_FUNCTION_ARGS)
{
	char *conninfo = TextDatumGetCString(PG_GETARG_DATUM(0));
	unsigned char	type;
	char   *buf;
	int		len;

	if (!walrcv_connect(conninfo, GetRedoRecPtr()))
		elog(ERROR, "could not connect");

	for (;;)
	{
		CHECK_FOR_INTERRUPTS();
		if (walrcv_receive(NAPTIME_PER_CYCLE, &type, &buf, &len))
		{
			elog(INFO, "got message: type = %c", type);

			/* Accept the received data, and process it */
			test_XLogWalRcvProcessMsg(type, buf, len);

			/* Receive any more data we can without sleeping */
			while (walrcv_receive(0, &type, &buf, &len))
				test_XLogWalRcvProcessMsg(type, buf, len);

			/* Let the primary know that we received some data. */
			test_XLogWalRcvSendReply();
		}
		else
		{
			/*
			 *
			 * We didn't receive anything new, but send a status update to the
			 * master anyway, to report any progress in applying WAL.
			 */
			test_XLogWalRcvSendReply();
		}
	}

	PG_RETURN_NULL();
}

/*
 * Accept the message from XLOG stream, and process it.
 */
static void
test_XLogWalRcvProcessMsg(unsigned char type, char *buf, Size len)
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

				test_ProcessWalSndrMessage("wal records",
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
				test_ProcessWalSndrMessage("keep alive",
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
			elog(INFO, "would open: %s", recvFilePath);
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
test_ProcessWalSndrMessage(char *type, XLogRecPtr walEnd,
						   TimestampTz sendTime)
{
	elog(INFO, "%s: %X/%X at %s", type,
				walEnd.xlogid, walEnd.xrecoff,
				timestamptz_to_str(sendTime));
}
