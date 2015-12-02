#if _MSC_VER >= 1400
#define errcode __msvc_errcode
#include <crtdefs.h>
#undef errcode
#endif
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "postgres.h"
#include "executor/spi.h"
#include "utils/lsyscache.h"
#include "fmgr.h"
#include "funcapi.h"
#include "xlogviewer.h"
#include "access/xact.h"
#include "access/clog.h"
#include "access/htup.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/xlog_internal.h"
#include "catalog/pg_control.h"

PG_MODULE_MAGIC;

/* XXX these ought to be in smgr.h, but are not */
#define XLOG_SMGR_CREATE	0x10
#define XLOG_SMGR_TRUNCATE	0x20

#define GET_TEXT(cstrp) DatumGetTextP(DirectFunctionCall1(textin, CStringGetDatum(cstrp)))
#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

typedef struct XLogViewerContext
{
	int logFd;
	uint32 logId;
	uint32 logSeg;
	int logRecOff;
	int32 logPageOff;
	XLogRecPtr curRecPtr;
} XLogViewerContext;

/*
 * Internal declarations
 */
bool readXLogPage(int *, int32 *, char *);
bool recordIsValid(XLogRecord *, XLogRecPtr *);
XLogRecord *readRecord(int *, int *, int32 *, XLogRecPtr *, uint32, uint32, bool);
const char *getXactName(RmgrId, uint8);

PG_FUNCTION_INFO_V1(xlogviewer);
Datum
xlogviewer(PG_FUNCTION_ARGS)
{
	FuncCallContext		*funcctx;
	XLogViewerContext	*context;
	int					call_cntr;
	int					max_calls;
	TupleDesc			tupdesc;
	AttInMetadata		*attinmeta;
	int					fd;

	int					logRecOff;
	int32				logPageOff;
	XLogRecord 			*record;
	XLogRecPtr			curRecPtr; 
	uint32				logId;
	uint32				logSeg;
	TimeLineID			logTLI;
	char				*fnamebase;
	bool				ignore_errors = (PG_NARGS() == 2?PG_GETARG_BOOL(1):false);

	const char * const RM_names[RM_MAX_ID+1] = {
		"XLOG ",					/* 0 */
		"XACT ",					/* 1 */
		"SMGR ",					/* 2 */
		"CLOG ",					/* 3 */
		"DBASE",					/* 4 */
		"TBSPC",					/* 5 */
		"MXACT",					/* 6 */
		"RM  7",					/* 7 */
		"RM  8",					/* 8 */
		"RM  9",					/* 9 */
		"HEAP ",					/* 10 */
		"BTREE",					/* 11 */
		"HASH ",					/* 12 */
		"RTREE",					/* 13 */
		"GIST ",					/* 14 */
		"SEQ  "						/* 15 */
	};

	/* stuff done only on the first call of the function */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext	oldcontext;
		char			*xlog_file = GET_STR(PG_GETARG_TEXT_P(0));

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		funcctx->max_calls = 1;

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context "
							"that cannot accept type record")));

		/* generate attribute metadata needed later to produce tuples from raw C strings */
		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;

		/* Try to open XLOG file */ 
		if ((fd = open(xlog_file, O_RDONLY | PG_BINARY, 0)) < 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Couldn't open xlog-file: %s", xlog_file)));
			SRF_RETURN_DONE(funcctx);
		}

		/*
		 * Extract logfile id and segment from file name
		 */
		fnamebase = strrchr(xlog_file, '/');
		if (fnamebase)
			fnamebase++;
		else
			fnamebase = xlog_file;
		if (sscanf(fnamebase, "%8x%8x%8x", &logTLI, &logId, &logSeg) != 3)
		{
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Can't recognize logfile name '%s'", fnamebase)));
			SRF_RETURN_DONE(funcctx);
		}

		context = palloc(sizeof(XLogViewerContext));

		context->logFd = fd;
		context->logId = logId;
		context->logSeg = logSeg;
		context->logPageOff = -BLCKSZ;
		context->logRecOff = 0;

		funcctx->user_fctx = (void *) context;

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	call_cntr = funcctx->call_cntr;
	max_calls = funcctx->max_calls;
	attinmeta = funcctx->attinmeta;

	context = (XLogViewerContext *) funcctx->user_fctx;
	fd = context->logFd;
	logId = context->logId;
	logSeg = context->logSeg;
	logRecOff = context->logRecOff;
	logPageOff = context->logPageOff;
	curRecPtr = context->curRecPtr;

	if (call_cntr < max_calls)    /* do when there is more left to send */
	{
		char		**values;
		HeapTuple	tuple;
		Datum		result;

		if((record = readRecord(&fd, &logRecOff, &logPageOff, &curRecPtr, logId, logSeg, ignore_errors)))
		{
			funcctx->max_calls += 1;

			context->logRecOff = logRecOff;
			context->logPageOff = logPageOff;
			context->curRecPtr = curRecPtr;

			/* build a tuple */
		       	values = (char **) palloc(7 * sizeof(char *));
		        values[0] = (char *) palloc(16 * sizeof(char));
		        values[1] = (char *) palloc(16 * sizeof(char));
		        values[2] = (char *) palloc(5 * sizeof(char));
		        values[3] = (char *) palloc(16 * sizeof(char));
		        values[4] = (char *) palloc(16 * sizeof(char));
		        values[5] = (char *) palloc(16 * sizeof(char));
		        values[6] = (char *) palloc(16 * sizeof(char));

		        snprintf(values[0], 16, "%d", record->xl_rmid);
		        snprintf(values[1], 16, "%d", record->xl_xid);
		        sprintf(values[2], "%s", RM_names[record->xl_rmid]);
		        snprintf(values[3], 16, "%2X", record->xl_info);
		        snprintf(values[4], 16, "%d", record->xl_len);
		        snprintf(values[5], 16, "%d", record->xl_tot_len);
		        snprintf(values[6], 16, "%s", getXactName(record->xl_rmid, record->xl_info));

			tuple = BuildTupleFromCStrings(attinmeta, values);

			/* make the tuple into a datum */
			result = HeapTupleGetDatum(tuple);
			SRF_RETURN_NEXT(funcctx, result);
			
		}
		else
		{
			close(fd);
			SRF_RETURN_DONE(funcctx);
		}
		
	}
	else    /* do when there is no more left */
	{
		close(fd);
		SRF_RETURN_DONE(funcctx);
	}
}

/* Read another page, if possible */
bool
readXLogPage(int *logFd, int32 *logPageOff, char *pageBuffer)
{
	size_t nread = read(*logFd, pageBuffer, BLCKSZ);

	if (nread == BLCKSZ)
	{
		*logPageOff += BLCKSZ;
		if (((XLogPageHeader) pageBuffer)->xlp_magic != XLOG_PAGE_MAGIC)
		{
			ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Bogus page magic number %04X at offset %X",
					 ((XLogPageHeader) pageBuffer)->xlp_magic, *logPageOff)));
		}
		return true;
	}
	if (nread != 0)
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("Partial page of %d bytes ignored",
				 (int) nread)));
	}
	return false;
}

/*
 * CRC-check an XLOG record.  We do not believe the contents of an XLOG
 * record (other than to the minimal extent of computing the amount of
 * data to read in) until we've checked the CRCs.
 *
 * We assume all of the record has been read into memory at *record.
 */
bool
recordIsValid(XLogRecord *record, XLogRecPtr *recptr)
{
	pg_crc32	crc;
	int			i;
	uint32		len = record->xl_len;
	BkpBlock	bkpb;
	char	   *blk;

	/* First the rmgr data */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, XLogRecGetData(record), len);

	/* Add in the backup blocks, if any */
	blk = (char *) XLogRecGetData(record) + len;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		uint32	blen;

		if (!(record->xl_info & XLR_SET_BKP_BLOCK(i)))
			continue;

		memcpy(&bkpb, blk, sizeof(BkpBlock));
		if (bkpb.hole_offset + bkpb.hole_length > BLCKSZ)
		{
			ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("incorrect hole size in record at %X/%X",
					 recptr->xlogid, recptr->xrecoff)));
			return false;
		}
		blen = sizeof(BkpBlock) + BLCKSZ - bkpb.hole_length;
		COMP_CRC32C(crc, blk, blen);
		blk += blen;
	}

	/* Check that xl_tot_len agrees with our calculation */
	if (blk != (char *) record + record->xl_tot_len)
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("incorrect total length in record at %X/%X",
				 recptr->xlogid, recptr->xrecoff)));
		return false;
	}

	/* Finally include the record header */
	COMP_CRC32C(crc, (char *) record + sizeof(pg_crc32), SizeOfXLogRecord - sizeof(pg_crc32));
	FIN_CRC32C(crc);

	if (!EQ_CRC32C(record->xl_crc, crc))
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("incorrect resource manager data checksum in record at %X/%X",
				 recptr->xlogid, recptr->xrecoff)));
		return false;
	}
	return true;
}

/*
 * Attempt to read an XLOG record into readRecordBuf.
 * Function adapted from xlogdump utility.
 */
XLogRecord *
readRecord(int *logFd, int *logRecOff, int32 *logPageOff, XLogRecPtr *curRecPtr, uint32 logId, uint32 logSeg, bool ignore_errors)
{
	char			*buffer;
	char			*readRecordBuf = NULL;
	XLogRecord 		*record;
	XLogContRecord	*contrecord;
	uint32			len,
					total_len;
	int				retries = 0;
	static char		pageBuffer[BLCKSZ];
	uint32			readRecordBufSize = 0;

restart:
	while (*logRecOff <= 0 || *logRecOff > BLCKSZ - SizeOfXLogRecord)
	{
		/* Need to advance to new page */
		if (! readXLogPage(logFd, logPageOff, pageBuffer))
			return false;
		*logRecOff = XLogPageHeaderSize((XLogPageHeader) pageBuffer);
		if ((((XLogPageHeader) pageBuffer)->xlp_info & ~XLP_LONG_HEADER) != 0)
		{
			ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("Unexpected page info flags %04X at offset %X",
				   ((XLogPageHeader) pageBuffer)->xlp_info, *logPageOff)));
			/* Check for a continuation record */
			if (((XLogPageHeader) pageBuffer)->xlp_info & XLP_FIRST_IS_CONTRECORD)
			{
				ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 	 errmsg("Skipping unexpected continuation record at offset %X",
					   *logPageOff)));
				contrecord = (XLogContRecord *) (pageBuffer + *logRecOff);
				*logRecOff += MAXALIGN(contrecord->xl_rem_len + SizeOfXLogContRecord);
			}
		}
	}

	curRecPtr->xlogid = logId;
	curRecPtr->xrecoff = logSeg * XLogSegSize + *logPageOff + *logRecOff;
	record = (XLogRecord *) (pageBuffer + *logRecOff);

	if (record->xl_len == 0)
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("ReadRecord: record with zero len at %u/%08X",
					curRecPtr->xlogid, curRecPtr->xrecoff)));
		/* Attempt to recover on new page, but give up after a few... */
		*logRecOff = 0;
		if (++retries > 4)
			return false;
		goto restart;
	}
	if (record->xl_tot_len < SizeOfXLogRecord + record->xl_len ||
		record->xl_tot_len > SizeOfXLogRecord + record->xl_len +
		XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + BLCKSZ))
	{
		ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("invalid record length at %X/%X",
					curRecPtr->xlogid, curRecPtr->xrecoff)));
		return false;
	}
	total_len = record->xl_tot_len;

	/*
	 * Allocate or enlarge readRecordBuf as needed.  To avoid useless
	 * small increases, round its size to a multiple of BLCKSZ, and make
	 * sure it's at least 4*BLCKSZ to start with.  (That is enough for all
	 * "normal" records, but very large commit or abort records might need
	 * more space.)
	 */
	if (total_len > readRecordBufSize)
	{
		uint32		newSize = total_len;

		newSize += BLCKSZ - (newSize % BLCKSZ);
		newSize = Max(newSize, 4 * BLCKSZ);
		if (readRecordBuf)
			pfree(readRecordBuf);
		readRecordBuf = (char *) palloc(newSize);
		if (!readRecordBuf)
		{
			readRecordBufSize = 0;
			/* We treat this as a "bogus data" condition */
			ereport(WARNING,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("record length %u at %X/%X too long",
					total_len, curRecPtr->xlogid, curRecPtr->xrecoff)));
			return false;
		}
		readRecordBufSize = newSize;
	}

	buffer = readRecordBuf;
	len = BLCKSZ - curRecPtr->xrecoff % BLCKSZ; /* available in block */
	if (total_len > len)
	{
		/* Need to reassemble record */
		uint32			gotlen = len;

		memcpy(buffer, record, len);
		record = (XLogRecord *) buffer;
		buffer += len;
		for (;;)
		{
			uint32	pageHeaderSize;

			if (! readXLogPage(logFd, logPageOff, pageBuffer))
			{
				/* XXX ought to be able to advance to new input file! */
				ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("Unable to read continuation page?")));
				return false;
			}
			if (!(((XLogPageHeader) pageBuffer)->xlp_info & XLP_FIRST_IS_CONTRECORD))
			{
				ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ReadRecord: there is no ContRecord flag in logfile %u seg %u off %u",
					   logId, logSeg, *logPageOff)));
				return false;
			}
			pageHeaderSize = XLogPageHeaderSize((XLogPageHeader) pageBuffer);
			contrecord = (XLogContRecord *) (pageBuffer + pageHeaderSize);
			if (contrecord->xl_rem_len == 0 || 
				total_len != (contrecord->xl_rem_len + gotlen))
			{
				ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("ReadRecord: invalid cont-record len %u in logfile %u seg %u off %u",
					   contrecord->xl_rem_len, logId, logSeg, *logPageOff)));
				return false;
			}
			len = BLCKSZ - pageHeaderSize - SizeOfXLogContRecord;
			if (contrecord->xl_rem_len > len)
			{
				memcpy(buffer, (char *)contrecord + SizeOfXLogContRecord, len);
				gotlen += len;
				buffer += len;
				continue;
			}
			memcpy(buffer, (char *) contrecord + SizeOfXLogContRecord,
				   contrecord->xl_rem_len);
			*logRecOff = MAXALIGN(pageHeaderSize + SizeOfXLogContRecord + contrecord->xl_rem_len);
			break;
		}
		if (!recordIsValid(record, curRecPtr) && !ignore_errors)
			return false;
		return record;
	}
	/* Record is contained in this page */
	memcpy(buffer, record, total_len);
	record = (XLogRecord *) buffer;
	*logRecOff += MAXALIGN(total_len);
	if (!recordIsValid(record, curRecPtr) && !ignore_errors)
		return false;
	return record;
}

const char *
getXactName (RmgrId rmid, uint8 info)
{
	switch (rmid)
	{
		case RM_XLOG_ID:
			if (info == XLOG_CHECKPOINT_SHUTDOWN ||
				info == XLOG_CHECKPOINT_ONLINE)
			{
				return "checkpoint";
			}
			else if (info == XLOG_NEXTOID)
			{
				return "nextOid";
			}
			break;
		case RM_XACT_ID:
			if (info == XLOG_XACT_COMMIT)
			{
				return "commit";
			}
			else if (info == XLOG_XACT_ABORT)
			{
				return "abort";
			}
			break;
		case RM_SMGR_ID:
			if (info == XLOG_SMGR_CREATE)
			{
				return "create rel";
			}
			else if (info == XLOG_SMGR_TRUNCATE)
			{
				return "truncate rel";
			}
			break;
		case RM_CLOG_ID:
			if (info == CLOG_ZEROPAGE)
			{
				return "zero clog page";
			}
			break;
		case RM_MULTIXACT_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_MULTIXACT_ZERO_OFF_PAGE:
				{
					return "zero offset page";
					break;
				}
				case XLOG_MULTIXACT_ZERO_MEM_PAGE:
				{
					return "zero members page";
					break;
				}
				case XLOG_MULTIXACT_CREATE_ID:
				{
					return "multixact create";
					break;
				}
			}
			break;
		case RM_HEAP_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_HEAP_INSERT:
				{
					return "insert";
					break;
				}
				case XLOG_HEAP_DELETE:
				{
					return "delete";
					break;
				}
				case XLOG_HEAP_UPDATE:
				{
					return "update";
					break;
				}
				case XLOG_HEAP_MOVE:
				{
					return "move";
					break;
				}
				case XLOG_HEAP_CLEAN:
				{
					return "clean";
					break;
				}
				case XLOG_HEAP_NEWPAGE:
				{
					return "newpage";
					break;
				}
				case XLOG_HEAP_LOCK:
				{
					return "lock";
					break;
				}
				case XLOG_HEAP_INPLACE:
				{
					return "inplace";
					break;
				}
			}
			break;
		case RM_BTREE_ID:
			/* XXX not complete dump of info ... */
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_BTREE_INSERT_LEAF:
				{
					return "insert_leaf";
					break;
				}
				case XLOG_BTREE_INSERT_UPPER:
				{
					return "insert_upper";
					break;
				}
				case XLOG_BTREE_INSERT_META:
				{
					return "insert_meta";
					break;
				}
				case XLOG_BTREE_SPLIT_L:
				{
					return "split_l";
					break;
				}
				case XLOG_BTREE_SPLIT_R:
				{
					return "split_r";
					break;
				}
				case XLOG_BTREE_SPLIT_L_ROOT:
				{
					return "split_l_root";
					break;
				}
				case XLOG_BTREE_SPLIT_R_ROOT:
				{
					return "split_r_root";
					break;
				}
				case XLOG_BTREE_DELETE:
				{
					return "delete";
					break;
				}
				case XLOG_BTREE_DELETE_PAGE:
				{
					return "delete_page";
					break;
				}
				case XLOG_BTREE_DELETE_PAGE_META:
				{
					return "delete_page_meta: index %u/%u/%u tid %u/%u deadblk %u\n";
					break;
				}
				case XLOG_BTREE_NEWROOT:
				{
					return "newroot";
					break;
				}
			}
			break;
		case RM_HASH_ID:
			break;
		case RM_GIST_ID:
			break;
		case RM_SEQ_ID:
			break;
	}
	return "";
}
