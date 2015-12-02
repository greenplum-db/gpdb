/*-------------------------------------------------------------------------
 *
 * xlogdump.c
 *		Simple utility for dumping PostgreSQL XLOG files.
 *
 * Usage: xlogdump [options] xlogfile [ xlogfile ... ] >output
 *
 *
 * Portions Copyright (c) 1996-2004, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#include <getopt_long.h>

#include "access/bitmap.h"
#include "access/clog.h"
#include "access/htup.h"
#include "access/gist_private.h"
#include "access/tupmacs.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xlogmm.h"
#include "catalog/pg_control.h"

#include "libpq-fe.h"
#include "pqexpbuffer.h"

#include "xlogdump.h"


static int		logFd;		/* kernel FD for current input file */
static TimeLineID	logTLI;		/* current log file timeline */
static uint32		logId;		/* current log file id */
static uint32		logSeg;		/* current log file segment */
static int32		logPageOff;	/* offset of current page in file */
static int		logRecOff;	/* offset of next record in page */
static char		pageBuffer[XLOG_BLCKSZ];	/* current page */
static XLogRecPtr	curRecPtr;	/* logical address of current record */
static XLogRecPtr	prevRecPtr;	/* logical address of previous record */
static char		*readRecordBuf = NULL; /* ReadRecord result area */
static uint32		readRecordBufSize = 0;

static PGconn		*conn = NULL; /* Connection for translating oids of global objects */
static PGconn		*lastDbConn = NULL; /* Connection for translating oids of per database objects */
static PGresult		*res;
static PQExpBuffer	dbQry;

/* command-line parameters */
static bool 		transactions = false; /* when true we just aggregate transaction info */
static bool 		statements = false; /* when true we try to rebuild fake sql statements with the xlog data */
static bool 		hideTimestamps = false; /* remove timestamp from dump used for testing */
static char 		rmname[5] = "ALL  "; /* name of the operation we want to filter on the xlog */
const char 		*pghost = NULL; /* connection host */
const char 		*pgport = NULL; /* connection port */
const char 		*username = NULL; /* connection username */

/* Oids used for checking if we need to search for an objects name or if we can use the last one */
static Oid 		lastDbOid;
static Oid 		lastSpcOid;
static Oid 		lastRelOid;

/* Buffers to hold objects names */
static char 		spaceName[NAMEDATALEN] = "";
static char 		dbName[NAMEDATALEN]  = "";
static char 		relName[NAMEDATALEN]  = "";


/* struct to aggregate transactions */
transInfoPtr 		transactionsInfo = NULL;

/* prototypes */
static void exit_gracefuly(int status);
static PGconn * DBConnect(const char *host, const char *port, char *database, const char *user);
static void dumpXLogRecord(XLogRecord *record, bool header_only);
static void dumpGIST(XLogRecord *record);
void dump_xlog_btree_insert_meta(XLogRecord *record);

/* Read another page, if possible */
static bool
readXLogPage(void)
{
	size_t nread = read(logFd, pageBuffer, XLOG_BLCKSZ);

	if (nread == XLOG_BLCKSZ)
	{
		logPageOff += XLOG_BLCKSZ;
		if (((XLogPageHeader) pageBuffer)->xlp_magic != XLOG_PAGE_MAGIC)
		{
			printf("Bogus page magic number %04X at offset %X\n",
				   ((XLogPageHeader) pageBuffer)->xlp_magic, logPageOff);
		}
		return true;
	}
	if (nread != 0)
	{
		fprintf(stderr, "Partial page of %d bytes ignored\n",
			(int) nread);
	}
	return false;
}

/* 
 * Exit closing active database connections
 */
static void
exit_gracefuly(int status)
{
	destroyPQExpBuffer(dbQry);
	if(lastDbConn)
		PQfinish(lastDbConn);
	if(conn)
		PQfinish(conn);

	close(logFd);
	exit(status);
}

/*
 * Open a database connection
 */
static PGconn *
DBConnect(const char *host, const char *port, char *database, const char *user)
{
	char	*password = NULL;
	char	*password_prompt = NULL;
	bool	need_pass;
	PGconn	*conn = NULL;

	/* loop until we have a password if requested by backend */
	do
	{
		need_pass = false;

		conn = PQsetdbLogin(host,
	                     port,
	                     NULL,
	                     NULL,
	                     database,
	                     user,
	                     password);

		if (PQstatus(conn) == CONNECTION_BAD &&
			strcmp(PQerrorMessage(conn), PQnoPasswordSupplied) == 0 &&
			!feof(stdin))
		{
			PQfinish(conn);
			need_pass = true;
			free(password);
			password = NULL;
			printf("\nPassword: ");
			password = simple_prompt(password_prompt, 100, false);
		}
	} while (need_pass);

	/* Check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "Connection to database failed: %s",
			PQerrorMessage(conn));
		exit_gracefuly(1);
	}
	
	return conn;
}

/*
 * CRC-check an XLOG record.  We do not believe the contents of an XLOG
 * record (other than to the minimal extent of computing the amount of
 * data to read in) until we've checked the CRCs.
 *
 * We assume all of the record has been read into memory at *record.
 */
static bool
RecordIsValid(XLogRecord *record, XLogRecPtr recptr)
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
			printf("incorrect hole size in record at %X/%X\n",
				   recptr.xlogid, recptr.xrecoff);
			return false;
		}
		blen = sizeof(BkpBlock) + BLCKSZ - bkpb.hole_length;
		COMP_CRC32C(crc, blk, blen);
		blk += blen;
	}

	/* skip total xl_tot_len check if physical log has been removed. */
	//if (!(record->xl_info & XLR_BKP_REMOVABLE) ||
	//	record->xl_info & XLR_BKP_BLOCK_MASK)
	{
		/* Check that xl_tot_len agrees with our calculation */
		if (blk != (char *) record + record->xl_tot_len)
		{
			printf("incorrect total length in record at %X/%X\n",
				   recptr.xlogid, recptr.xrecoff);
			return false;
		}
	}

	/* Finally include the record header */
	COMP_CRC32C(crc, (char *) record + sizeof(pg_crc32), SizeOfXLogRecord - sizeof(pg_crc32));
	FIN_CRC32C(crc);

	if (!EQ_CRC32C(record->xl_crc, crc))
	{
		printf("incorrect resource manager data checksum in record at %X/%X\n",
			   recptr.xlogid, recptr.xrecoff);
		return false;
	}

	return true;
}

/*
 * Attempt to read an XLOG record into readRecordBuf.
 */
static bool
ReadRecord(void)
{
	char	   *buffer;
	XLogRecord *record;
	XLogContRecord *contrecord;
	uint32		len,
				total_len;
	int			retries = 0;

restart:
	while (logRecOff <= 0 || logRecOff > XLOG_BLCKSZ - SizeOfXLogRecord)
	{
		/* Need to advance to new page */
		if (! readXLogPage())
			return false;
		logRecOff = XLogPageHeaderSize((XLogPageHeader) pageBuffer);
		if ((((XLogPageHeader) pageBuffer)->xlp_info & ~XLP_LONG_HEADER) != 0)
		{
			printf("Unexpected page info flags %04X at offset %X\n",
				   ((XLogPageHeader) pageBuffer)->xlp_info, logPageOff);
			/* Check for a continuation record */
			if (((XLogPageHeader) pageBuffer)->xlp_info & XLP_FIRST_IS_CONTRECORD)
			{
				printf("Skipping unexpected continuation record at offset %X\n",
					   logPageOff);
				contrecord = (XLogContRecord *) (pageBuffer + logRecOff);
				logRecOff += MAXALIGN(contrecord->xl_rem_len + SizeOfXLogContRecord);
			}
		}
	}

	curRecPtr.xlogid = logId;
	curRecPtr.xrecoff = logSeg * XLogSegSize + logPageOff + logRecOff;
	record = (XLogRecord *) (pageBuffer + logRecOff);

	if (record->xl_len == 0)
	{
		/* Stop if XLOG_SWITCH was found. */
		if (record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH)
		{
			dumpXLogRecord(record, false);
			return false;
		}

		printf("ReadRecord: record with zero len at %u/%08X\n",
		   curRecPtr.xlogid, curRecPtr.xrecoff);

		/* Attempt to recover on new page, but give up after a few... */
		logRecOff = 0;
		if (++retries > 4)
			return false;
		goto restart;
	}
	if (record->xl_tot_len < SizeOfXLogRecord + record->xl_len ||
		record->xl_tot_len > SizeOfXLogRecord + record->xl_len +
		XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + BLCKSZ))
	{
		printf("invalid record length(expected %lu ~ %lu, actual %d) at %X/%X\n",
				(long unsigned int)SizeOfXLogRecord + record->xl_len,
				(long unsigned int)SizeOfXLogRecord + record->xl_len +
					XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + BLCKSZ),
				record->xl_tot_len,
			   curRecPtr.xlogid, curRecPtr.xrecoff);
		return false;
	}
	total_len = record->xl_tot_len;

	/*
	 * Allocate or enlarge readRecordBuf as needed.  To avoid useless
	 * small increases, round its size to a multiple of XLOG_BLCKSZ, and make
	 * sure it's at least 4*BLCKSZ to start with.  (That is enough for all
	 * "normal" records, but very large commit or abort records might need
	 * more space.)
	 */
	if (total_len > readRecordBufSize)
	{
		uint32		newSize = total_len;

		newSize += XLOG_BLCKSZ - (newSize % XLOG_BLCKSZ);
		newSize = Max(newSize, 4 * XLOG_BLCKSZ);
		if (readRecordBuf)
			free(readRecordBuf);
		readRecordBuf = (char *) malloc(newSize);
		if (!readRecordBuf)
		{
			readRecordBufSize = 0;
			/* We treat this as a "bogus data" condition */
			fprintf(stderr, "record length %u at %X/%X too long\n",
					total_len, curRecPtr.xlogid, curRecPtr.xrecoff);
			return false;
		}
		readRecordBufSize = newSize;
	}

	buffer = readRecordBuf;
	len = XLOG_BLCKSZ - curRecPtr.xrecoff % XLOG_BLCKSZ; /* available in block */
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

			if (! readXLogPage())
			{
				/* XXX ought to be able to advance to new input file! */
				fprintf(stderr, "Unable to read continuation page?\n");
				dumpXLogRecord(record, true);
				return false;
			}
			if (!(((XLogPageHeader) pageBuffer)->xlp_info & XLP_FIRST_IS_CONTRECORD))
			{
				printf("ReadRecord: there is no ContRecord flag in logfile %u seg %u off %u\n",
					   logId, logSeg, logPageOff);
				return false;
			}
			pageHeaderSize = XLogPageHeaderSize((XLogPageHeader) pageBuffer);
			contrecord = (XLogContRecord *) (pageBuffer + pageHeaderSize);
			if (contrecord->xl_rem_len == 0 || 
				total_len != (contrecord->xl_rem_len + gotlen))
			{
				printf("ReadRecord: invalid cont-record len %u in logfile %u seg %u off %u\n",
					   contrecord->xl_rem_len, logId, logSeg, logPageOff);
				return false;
			}
			len = XLOG_BLCKSZ - pageHeaderSize - SizeOfXLogContRecord;
			if (contrecord->xl_rem_len > len)
			{
				memcpy(buffer, (char *)contrecord + SizeOfXLogContRecord, len);
				gotlen += len;
				buffer += len;
				continue;
			}
			memcpy(buffer, (char *) contrecord + SizeOfXLogContRecord,
				   contrecord->xl_rem_len);
			logRecOff = MAXALIGN(pageHeaderSize + SizeOfXLogContRecord + contrecord->xl_rem_len);
			break;
		}
		if (!RecordIsValid(record, curRecPtr))
			return false;
		return true;
	}
	/* Record is contained in this page */
	memcpy(buffer, record, total_len);
	record = (XLogRecord *) buffer;
	logRecOff += MAXALIGN(total_len);
	if (!RecordIsValid(record, curRecPtr))
		return false;
	return true;
}

static char *
str_time(time_t tnow)
{
	static char buf[32];

	strftime(buf, sizeof(buf),
			 "%Y-%m-%d %H:%M:%S %Z",
			 localtime(&tnow));

	return buf;
}

/*
 * Atempt to read the name of tablespace into lastSpcName
 * (if there's a database connection and the oid changed since lastSpcOid)
 */
static void
getSpaceName(uint32 space)
{
	resetPQExpBuffer(dbQry);
	if((conn) && (lastSpcOid != space))
	{
		PQclear(res);
		appendPQExpBuffer(dbQry, "SELECT spcname FROM pg_tablespace WHERE oid = %i", space);
		res = PQexec(conn, dbQry->data);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			exit_gracefuly(1);
		}
		resetPQExpBuffer(dbQry);
		lastSpcOid = space;
		if(PQntuples(res) > 0)
		{
			strcpy(spaceName, PQgetvalue(res, 0, 0));
			return;
		}
	}
	else if(lastSpcOid == space)
		return;

	/* Didn't find the name, return string with oid */
	sprintf(spaceName, "%u", space);
	return;
}

/*
 * Atempt to get the name of database (if there's a database connection)
 */
static void
getDbName(uint32 db)
{
	resetPQExpBuffer(dbQry);
	if((conn) && (lastDbOid != db))
	{	
		PQclear(res);
		appendPQExpBuffer(dbQry, "SELECT datname FROM pg_database WHERE oid = %i", db);
		res = PQexec(conn, dbQry->data);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			exit_gracefuly(1);
		}
		resetPQExpBuffer(dbQry);
		lastDbOid = db;
		if(PQntuples(res) > 0)
		{
			strcpy(dbName, PQgetvalue(res, 0, 0));

			// Database changed makes new connection
			PQfinish(lastDbConn);
			lastDbConn = DBConnect(pghost, pgport, dbName, username);

			return;
		}
	}
	else if(lastDbOid == db)
		return;

	/* Didn't find the name, return string with oid */
	sprintf(dbName, "%u", db);
	return;
}

/*
 * Atempt to get the name of relation and copy to relName 
 * (if there's a database connection and the reloid changed)
 * Copy a string with oid if not found
 */
static void
getRelName(uint32 relid)
{
	resetPQExpBuffer(dbQry);
	if((conn) && (lastDbConn) && (lastRelOid != relid))
	{
		PQclear(res);
		/* Try the relfilenode and oid just in case the filenode has changed
		   If it has changed more than once we can't translate it's name */
		appendPQExpBuffer(dbQry, "SELECT relname, oid FROM pg_class WHERE relfilenode = %i OR oid = %i", relid, relid);
		res = PQexec(lastDbConn, dbQry->data);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			exit_gracefuly(1);
		}
		resetPQExpBuffer(dbQry);
		lastRelOid = relid;
		if(PQntuples(res) > 0)
		{
			strcpy(relName, PQgetvalue(res, 0, 0));
			/* copy the oid since it could be different from relfilenode */
			lastRelOid = (uint32) atoi(PQgetvalue(res, 0, 1));
			return;
		}
	}
	else if(lastRelOid == relid)
		return;
	
	/* Didn't find the name, return string with oid */
	sprintf(relName, "%u", relid);
	return;
}

/*
 * Print the field based on a chunk of xlog data and the field type
 * The maxfield len is just for error detection on variable length data,
 * actualy is based on the xlog record total lenght
 */
static int
printField(char *data, int offset, int type, uint32 maxFieldLen)
{
	int32 i, size;
	int64 bigint;
	int16 smallint;
	float4 floatNumber;
	float8 doubleNumber;
	Oid objectId;
	
	// Just print out the value of a specific data type from the data array
	switch(type)
	{
		case 700: //float4
			memcpy(&floatNumber, &data[offset], sizeof(float4));
			printf("%f", floatNumber);
			return sizeof(float4);
		case 701: //float8
			memcpy(&doubleNumber, &data[offset], sizeof(float8));
			printf("%f", doubleNumber);
			return sizeof(float8);
		case 16: //boolean
			printf("%c", (data[offset] == 0 ? 'f' : 't'));
			return MAXALIGN(sizeof(bool));
		case 1043: //varchar
		case 1042: //bpchar
		case 25: //text
		case 18: //char
			memcpy(&size, &data[offset], sizeof(int32));
			//@todo usar putc
			if(size > maxFieldLen || size < 0)
			{
				fprintf(stderr, "ERROR: Invalid field size\n");
				return 0;
			}
			for(i = sizeof(int32); i < size; i++)
				printf("%c", data[offset + i]);
				
			//return ( (size % sizeof(int)) ? size + sizeof(int) - (size % sizeof(int)):size);
			return MAXALIGN(size * sizeof(char));
		case 19: //name
			for(i = 0; i < NAMEDATALEN && data[offset + i] != '\0'; i++)
				printf("%c", data[offset + i]);
				
			return NAMEDATALEN;
		case 21: //smallint
			memcpy(&smallint, &data[offset], sizeof(int16));
			printf("%i", (int) smallint);
			return sizeof(int16);
		case 23: //int
			memcpy(&i, &data[offset], sizeof(int32));
			printf("%i", i);
			return sizeof(int32);
		case 26: //oid
			memcpy(&objectId, &data[offset], sizeof(Oid));
			printf("%i", (int) objectId);
			return sizeof(Oid);
		case 20: //bigint
			//@todo como imprimir int64?
			memcpy(&bigint, &data[offset], sizeof(int64));
			printf("%i", (int) bigint);
			return sizeof(int64);
		case 1005: //int2vector
			memcpy(&size, &data[offset], sizeof(int32));
			return MAXALIGN(size);
			
	}
	return 0;
}

/*
 * Print a update command that contains all the data on a xl_heap_update
 */
static void
printUpdate(xl_heap_update *xlrecord, uint32 datalen)
{
	char data[MaxHeapTupleSize];
	xl_heap_header hhead;
	int offset;
	bits8 nullBitMap[MaxNullBitmapLen];

	MemSet((char *) data, 0, MaxHeapTupleSize * sizeof(char));
	MemSet(nullBitMap, 0, MaxNullBitmapLen);
	
	if(datalen > MaxHeapTupleSize)
		return;

	/* Copy the heap header into hhead, 
	   the the heap data into data 
	   and the tuple null bitmap into nullBitMap */
	memcpy(&hhead, (char *) xlrecord + SizeOfHeapUpdate, SizeOfHeapHeader);
	memcpy(&data, (char *) xlrecord + hhead.t_hoff + 4, datalen);
	memcpy(&nullBitMap, (bits8 *) xlrecord + SizeOfHeapUpdate + SizeOfHeapHeader, BITMAPLEN(HeapTupleHeaderGetNatts(&hhead)) * sizeof(bits8));

	printf("UPDATE \"%s\" SET ", relName);

	// Get relation field names and types
	if((conn) && (lastDbConn))
	{
		int	i, rows = 0, fieldSize = 0;
		
		resetPQExpBuffer(dbQry);
		PQclear(res);
		appendPQExpBuffer(dbQry, "SELECT attname, atttypid from pg_attribute where attnum > 0 AND attrelid = '%i' ORDER BY attnum", lastRelOid);
		res = PQexec(lastDbConn, dbQry->data);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			exit_gracefuly(1);
		}
		resetPQExpBuffer(dbQry);
		rows = PQntuples(res);
		offset = 0;

		for(i = 0; i < rows; i++)
		{
			printf("%s%s = ", (i == 0 ? "" : ", "), PQgetvalue(res, i, 0));

			/* is the attribute value null? */
			if((hhead.t_infomask & HEAP_HASNULL) && (att_isnull(i, nullBitMap)))
			{
				printf("NULL");
			}
			else
			{
				printf("'");
				if(!(fieldSize = printField(data, offset, atoi(PQgetvalue(res, i, 1)), datalen)))
					break;

				printf("'");
				offset += fieldSize;
			}
		}
		printf(" WHERE ... ;\n");
	}
}

/*
 * Print a insert command that contains all the data on a xl_heap_insert
 */
static void
printInsert(xl_heap_insert *xlrecord, uint32 datalen)
{
	char data[MaxHeapTupleSize];
	xl_heap_header hhead;
	int offset;
	bits8 nullBitMap[MaxNullBitmapLen];

	MemSet((char *) data, 0, MaxHeapTupleSize * sizeof(char));
	MemSet(nullBitMap, 0, MaxNullBitmapLen);
	
	if(datalen > MaxHeapTupleSize)
		return;

	/* Copy the heap header into hhead, 
	   the the heap data into data 
	   and the tuple null bitmap into nullBitMap */
	memcpy(&hhead, (char *) xlrecord + SizeOfHeapInsert, SizeOfHeapHeader);
	memcpy(&data, (char *) xlrecord + hhead.t_hoff - 4, datalen);
	memcpy(&nullBitMap, (bits8 *) xlrecord + SizeOfHeapInsert + SizeOfHeapHeader, BITMAPLEN(HeapTupleHeaderGetNatts(&hhead)) * sizeof(bits8));
	
	printf("INSERT INTO \"%s\" (", relName);
	
	// Get relation field names and types
	if((conn) && (lastDbConn))
	{
		int	i, rows = 0, fieldSize = 0;
		
		resetPQExpBuffer(dbQry);
		PQclear(res);
		appendPQExpBuffer(dbQry, "SELECT attname, atttypid from pg_attribute where attnum > 0 AND attrelid = '%i' ORDER BY attnum", lastRelOid);
		res = PQexec(lastDbConn, dbQry->data);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
		{
			fprintf(stderr, "SELECT FAILED: %s", PQerrorMessage(conn));
			PQclear(res);
			exit_gracefuly(1);
		}
		resetPQExpBuffer(dbQry);
		rows = PQntuples(res);
		for(i = 0; i < rows; i++)
			printf("%s%s", (i == 0 ? "" : ", "), PQgetvalue(res, i, 0));

		printf(") VALUES (");
		offset = 0;

		for(i = 0; i < rows; i++)
		{
			/* is the attribute value null? */
			if((hhead.t_infomask & HEAP_HASNULL) && (att_isnull(i, nullBitMap)))
			{
				printf("%sNULL", (i == 0 ? "" : ", "));
			}
			else
			{
				printf("%s'", (i == 0 ? "" : ", "));
				if(!(fieldSize = printField(data, offset, atoi(PQgetvalue(res, i, 1)), datalen)))
				{
					printf("'");
					break;
				}
				else
					printf("'");

				offset += fieldSize;
			}
		}
		printf(");\n");
	}
}

typedef struct dtx_checkpoint
{
	uint32				len;
	TMGXACT_CHECKPOINT *tmgxact;
} dtx_checkpoint;

typedef struct mmxlog_checkpoint
{
	uint32			len;
	fspc_agg_state *fspc;
	tspc_agg_state *tspc;
	dbdir_agg_state *dbdir;
} mmxlog_checkpoint;

typedef struct prpt_checkpoint
{
	uint32			len;
	prepared_transaction_agg_state *prpt;
} prpt_checkpoint;

/*
 * Read mmxlog checkpoint info embedded in checkpoint xlog.  len is the known length
 * we can read up to, and this function returns the number of bytes of the mmxlog info
 * as well as pointers to each field into p_mmCheckpoint.  Returns 0 if len is not
 * long enough unexpectedly.
 */
static size_t
read_mmxlog_checkpoint(char *ptr0, int len, mmxlog_checkpoint *p_mmCheckpoint)
{
	fspc_agg_state *fspc;
	tspc_agg_state *tspc;
	dbdir_agg_state *dbdir;
	int fspc_len;
	int tspc_len;
	int dbdir_len;
	char *ptr;

	/* read filespace info and advance pointer */
	ptr = ptr0;
	fspc = (fspc_agg_state *) ptr;
	fspc_len = FSPC_CHECKPOINT_BYTES(fspc->count);
	if (len < fspc_len)
		return 0;
	ptr += fspc_len;
	len -= fspc_len;

	/* read tablespace info and advance pointer */
	tspc = (tspc_agg_state *) ptr;
	tspc_len = TSPC_CHECKPOINT_BYTES(tspc->count);
	if (len < tspc_len)
		return 0;
	ptr += tspc_len;
	len -= tspc_len;

	/* read database info and advance pointer */
	dbdir = (dbdir_agg_state *) ptr;
	dbdir_len = DBDIR_CHECKPOINT_BYTES(dbdir->count);
	if (len < dbdir_len)
		return 0;
	ptr += dbdir_len;
	len -= dbdir_len;

	/* pack returning values */
	if (p_mmCheckpoint != NULL)
	{
		p_mmCheckpoint->len = ptr - ptr0;
		p_mmCheckpoint->fspc = fspc;
		p_mmCheckpoint->tspc = tspc;
		p_mmCheckpoint->dbdir = dbdir;
	}

	return ptr - ptr0;
}

/*
 * Read DTX, mmxlog and prepared transaction payload of checkpoint record.
 * Each information is returned to the argument pointers.  Each is
 * initialized with 0/NULL, and if record looks corrupted we stop reading.
 */
static void
read_checkpoint_payload(XLogRecord *record,
						dtx_checkpoint *p_dtxCheckpoint,
						mmxlog_checkpoint *p_mmCheckpoint,
						prpt_checkpoint *p_prptCheckpoint)
{
	if (p_dtxCheckpoint != NULL)
		memset(p_dtxCheckpoint, 0, sizeof(dtx_checkpoint));
	if (p_mmCheckpoint != NULL)
		memset(p_mmCheckpoint, 0, sizeof(mmxlog_checkpoint));
	if (p_prptCheckpoint != NULL)
		memset(p_prptCheckpoint, 0, sizeof(prpt_checkpoint));

	if (record->xl_len > sizeof(CheckPoint))
	{
		uint32	remainderLen;
		TMGXACT_CHECKPOINT *dtxCheckpoint;
		uint32 dtxCheckpointLen;
		char *mmCheckpoint;
		uint32 mmCheckpointLen;
		prepared_transaction_agg_state *prptCheckpoint;
		uint32 prptCheckpointLen;

		remainderLen = record->xl_len - sizeof(CheckPoint);

		/* Read distributed transaction payload */
		dtxCheckpoint = (TMGXACT_CHECKPOINT *)
			(XLogRecGetData(record) + sizeof(CheckPoint));
		dtxCheckpointLen =
			TMGXACT_CHECKPOINT_BYTES(dtxCheckpoint->committedCount);
		/* Not enough length?  Don't return values. */
		if (remainderLen < dtxCheckpointLen)
			return;
		if (p_dtxCheckpoint != NULL)
		{
			p_dtxCheckpoint->len = dtxCheckpointLen;
			p_dtxCheckpoint->tmgxact = dtxCheckpoint;
		}

		remainderLen -= dtxCheckpointLen;

		/* Read mmxlog payload */
		mmCheckpoint = ((char *) dtxCheckpoint + dtxCheckpointLen);
		mmCheckpointLen =
			read_mmxlog_checkpoint(mmCheckpoint, remainderLen, p_mmCheckpoint);
		/* 0 means mmxlog could not be read (not enough length) */
		if (mmCheckpointLen == 0)
			return;

		remainderLen -= mmCheckpointLen;

		/* Read prepared transaction payload */
		prptCheckpoint = (prepared_transaction_agg_state *)
			((char *) mmCheckpoint + mmCheckpointLen);
		prptCheckpointLen =
				PREPARED_TRANSACTION_CHECKPOINT_BYTES(prptCheckpoint->count);
		/* Not enough length?  Don't return values. */
		if (remainderLen < prptCheckpointLen)
			return;
		if (p_prptCheckpoint != NULL)
		{
			p_prptCheckpoint->len = prptCheckpointLen;
			p_prptCheckpoint->prpt = prptCheckpoint;
		}
	}
}

static void
dumpXLogRecord(XLogRecord *record, bool header_only)
{
	int	i;
	char   	*blk;
	uint8	info = record->xl_info & ~XLR_INFO_MASK;

	/* check if the user wants a specific rmid */
	if(strcmp("ALL  ", rmname) && pg_strcasecmp(RM_names[record->xl_rmid], rmname))
		return;

	printf("%X/%X: prv %X/%X",
		   curRecPtr.xlogid, curRecPtr.xrecoff,
		   record->xl_prev.xlogid, record->xl_prev.xrecoff);

	if (!XLByteEQ(record->xl_prev, prevRecPtr))
		printf("(?)");

	printf("; xid %u; ", record->xl_xid);

	if (record->xl_rmid <= RM_MAX_ID)
		printf("%s", RM_names[record->xl_rmid]);
	else
		printf("RM %2d", record->xl_rmid);

	printf(" info %02X len %u tot_len %u\n", record->xl_info,
		   record->xl_len, record->xl_tot_len);

	if (header_only)
	{
		printf(" ** maybe continues to next segment **\n");
		return;
	}

	blk = (char*)XLogRecGetData(record) + record->xl_len;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		BkpBlock  bkb;
		Page pg;

		if (!(record->xl_info & (XLR_SET_BKP_BLOCK(i))))
			continue;
		memcpy(&bkb, blk, sizeof(BkpBlock));
		getSpaceName(bkb.node.spcNode);
		getDbName(bkb.node.dbNode);
		getRelName(bkb.node.relNode);
		pg = (Page)(blk + sizeof(BkpBlock));
		printf("bkpblock %d: ts %s db %s rel %s block %u hole %u len %u "
			   "lsn (%X,%X)\n", 
				i+1, spaceName, dbName, relName,
			   	bkb.block, bkb.hole_offset, bkb.hole_length,
				PageGetLSN(pg).xlogid, PageGetLSN(pg).xrecoff);

		blk += sizeof(BkpBlock) + (BLCKSZ - bkb.hole_length);
	}

	switch (record->xl_rmid)
	{
		case RM_XLOG_ID:
			if (info == XLOG_CHECKPOINT_SHUTDOWN ||
				info == XLOG_CHECKPOINT_ONLINE)
			{
				CheckPoint	*checkpoint = (CheckPoint*) XLogRecGetData(record);
				dtx_checkpoint dtxCheckpoint;
				mmxlog_checkpoint mmCheckpoint;
				prpt_checkpoint prptCheckpoint;

				if(!hideTimestamps)
					printf("checkpoint: redo %X/%X; undo %X/%X; tli %u; nextxid %u;\n"
						   "  nextoid %u; nextmulti %u; nextoffset %u; %s at %s\n",
						   checkpoint->redo.xlogid, checkpoint->redo.xrecoff,
						   checkpoint->undo.xlogid, checkpoint->undo.xrecoff,
						   checkpoint->ThisTimeLineID, checkpoint->nextXid, 
						   checkpoint->nextOid,
						   checkpoint->nextMulti,
						   checkpoint->nextMultiOffset,
						   (info == XLOG_CHECKPOINT_SHUTDOWN) ?
						   "shutdown" : "online",
						   str_time(checkpoint->time));
				else
					printf("checkpoint: redo %X/%X; undo %X/%X; tli %u; nextxid %u;\n"
						   "  nextoid %u; nextmulti %u; nextoffset %u; %s\n",
						   checkpoint->redo.xlogid, checkpoint->redo.xrecoff,
						   checkpoint->undo.xlogid, checkpoint->undo.xrecoff,
						   checkpoint->ThisTimeLineID, checkpoint->nextXid, 
						   checkpoint->nextOid,
						   checkpoint->nextMulti,
						   checkpoint->nextMultiOffset,
						   (info == XLOG_CHECKPOINT_SHUTDOWN) ?
						   "shutdown" : "online");

				read_checkpoint_payload(record,
										&dtxCheckpoint,
										&mmCheckpoint,
										&prptCheckpoint);

				if (dtxCheckpoint.len > 0)
				{
					int i, count;
					TMGXACT_CHECKPOINT *tmgxact;

					tmgxact = dtxCheckpoint.tmgxact;
					count = tmgxact->committedCount;
					for (i = 0; i < count; i++)
					{
						TMGXACT_LOG *log = &tmgxact->committedGxactArray[i];

						printf("  dtx: gxid = %u, gid = %s\n",
							   log->gxid, log->gid);
					}
				}

				if (mmCheckpoint.len > 0)
				{
					fspc_agg_state *f;
					tspc_agg_state *t;
					dbdir_agg_state *d;
					int i;

					f = mmCheckpoint.fspc;
					for (i = 0; i < f->count; i++)
					{
						fspc_map *m = &(f->maps[i]);

						printf("  filespace: filespaceoid = %u, dbid1 = %d, dbid = %d, path1 = %s, path2 = %s\n",
								m->filespaceoid,
								m->dbid1, m->dbid2,
								m->path1, m->path2);
					}

					t = mmCheckpoint.tspc;
					for (i = 0; i < t->count; i++)
					{
						tspc_map *m = &(t->maps[i]);
						printf("  tablespace: tablespaceoid = %u, filespaceoid = %u\n",
								m->tablespaceoid, m->filespaceoid);
					}

					d = mmCheckpoint.dbdir;
					for (i = 0; i < d->count; i++)
					{
						dbdir_map *m = &(d->maps[i]);
						printf("  database: tablespaceoid = %u, databaseoid = %u\n",
								m->tablespaceoid, m->databaseoid);
					}
				}

				if (prptCheckpoint.len > 0)
				{
					int i, count;
					prepared_transaction_agg_state *prpt;

					prpt = prptCheckpoint.prpt;
					count = prpt->count;
					for (i = 0; i < count; i++)
					{
						prpt_map *m = &prpt->maps[i];
						printf("  prepared transaction: xid = %u, xlogrecptr = %08X/%08X\n",
								m->xid, m->xlogrecptr.xlogid, m->xlogrecptr.xrecoff);
					}
				}

			}
			else if (info == XLOG_NEXTOID)
			{
				Oid		nextOid;

				memcpy(&nextOid, XLogRecGetData(record), sizeof(Oid));
				printf("nextOid: %u\n", nextOid);
			}
			else if (info == XLOG_SWITCH)
			{
				printf("switch:\n");
			}
			/* GPDB doesn't have XLOG_NOOP
			else if (info == XLOG_NOOP)
			{
				printf("noop:\n");
			}
			*/
			break;
		case RM_XACT_ID:
			if (info == XLOG_XACT_COMMIT)
			{
				xl_xact_commit	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				if(!hideTimestamps)
					printf("commit: %u at %s\n", record->xl_xid,
						   str_time(xlrec.xtime));
				else
					printf("commit: %u\n", record->xl_xid);
			}
			else if (info == XLOG_XACT_ABORT)
			{
				xl_xact_abort	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				if(!hideTimestamps)
					printf("abort: %u at %s\n", record->xl_xid,
						   str_time(xlrec.xtime));
				else
					printf("abort: %u\n", record->xl_xid);
			}
			else if (info == XLOG_XACT_PREPARE)
			{
 				printf("prepare\n");
			}
			else if (info == XLOG_XACT_COMMIT_PREPARED)
			{
				xl_xact_commit_prepared	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				printf("commit-prepared: %u\n", xlrec.xid);
			}
			else if (info == XLOG_XACT_ABORT_PREPARED)
			{
				xl_xact_abort_prepared	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				printf("abort-prepared: %u\n", xlrec.xid);
			}
			else if (info == XLOG_XACT_DISTRIBUTED_COMMIT)
			{
				xl_xact_commit	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				printf("distributed-commit: %u\n", record->xl_xid);
			}
			else if (info == XLOG_XACT_DISTRIBUTED_FORGET)
			{
				xl_xact_distributed_forget	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				printf("distributed-forget: %u, gid = %s, gxid = %u\n", record->xl_xid,
					       xlrec.gxact_log.gid, xlrec.gxact_log.gxid);
			}
			break;
		case RM_SMGR_ID:
			if (info == XLOG_SMGR_CREATE)
			{
				xl_smgr_create	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				getSpaceName(xlrec.rnode.spcNode);
				getDbName(xlrec.rnode.dbNode);
				getRelName(xlrec.rnode.relNode);
				printf("create rel: %s/%s/%s\n", 
						spaceName, dbName, relName);
			}
			else if (info == XLOG_SMGR_TRUNCATE)
			{
				xl_smgr_truncate	xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
				getSpaceName(xlrec.rnode.spcNode);
				getDbName(xlrec.rnode.dbNode);
				getRelName(xlrec.rnode.relNode);
				printf("truncate rel: %s/%s/%s at block %u\n",
					 spaceName, dbName, relName, xlrec.blkno);
			}
			break;
		case RM_CLOG_ID:
			if (info == CLOG_ZEROPAGE)
			{
				int		pageno;

				memcpy(&pageno, XLogRecGetData(record), sizeof(int));
				printf("zero clog page 0x%04x\n", pageno);
			}
			break;
		case RM_MULTIXACT_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_MULTIXACT_ZERO_OFF_PAGE:
				{
					int		pageno;

					memcpy(&pageno, XLogRecGetData(record), sizeof(int));
					printf("zero offset page 0x%04x\n", pageno);
					break;
				}
				case XLOG_MULTIXACT_ZERO_MEM_PAGE:
				{
					int		pageno;

					memcpy(&pageno, XLogRecGetData(record), sizeof(int));
					printf("zero members page 0x%04x\n", pageno);
					break;
				}
				case XLOG_MULTIXACT_CREATE_ID:
				{
					xl_multixact_create xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					printf("multixact create: %u off %u nxids %u\n",
						   xlrec.mid,
						   xlrec.moff,
						   xlrec.nxids);
					break;
				}
			}
			break;
		case RM_HEAP2_ID:
			switch (info)
			{
				case XLOG_HEAP2_FREEZE:
				{
					xl_heap_freeze xlrec;
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					printf("freeze: ts %d db %d rel %d block %d cutoff_xid %d\n",
						xlrec.heapnode.node.spcNode,
						xlrec.heapnode.node.dbNode,
						xlrec.heapnode.node.relNode,
						xlrec.block, xlrec.cutoff_xid
					);
				}
				break;
				/*
				 * GPDB doesn't yet have HEAP2
				 *
				case XLOG_HEAP2_CLEAN:
				case XLOG_HEAP2_CLEAN_MOVE:
				{
					xl_heap_clean xlrec;
					int total_off;
					int nunused = 0;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.node.spcNode);
					getDbName(xlrec.node.dbNode);
					getRelName(xlrec.node.relNode);
					total_off = (record->xl_len - SizeOfHeapClean) / sizeof(OffsetNumber);
					if (total_off > xlrec.nredirected + xlrec.ndead)
						nunused = total_off - (xlrec.nredirected + xlrec.ndead);
					printf("clean%s: ts %s db %s rel %s block %u redirected %d dead %d unused %d\n",
						info == XLOG_HEAP2_CLEAN_MOVE ? "_move" : "",
						spaceName, dbName, relName,
						xlrec.block,
						xlrec.nredirected, xlrec.ndead, nunused);
					break;
				}
				break;
				*/
			}
			break;
		case RM_HEAP_ID:
			switch (info & XLOG_HEAP_OPMASK)
			{
				case XLOG_HEAP_INSERT:
				{
					xl_heap_insert xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));

					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					if(statements)
						printInsert((xl_heap_insert *) XLogRecGetData(record), record->xl_len - SizeOfHeapInsert - SizeOfHeapHeader);

					printf("insert%s: ts %s db %s rel %s block %u off %u\n",
						   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
						   spaceName, dbName, relName,
						   ItemPointerGetBlockNumber(&xlrec.target.tid),
						   ItemPointerGetOffsetNumber(&xlrec.target.tid));
					/* If backup block doesn't exist, dump rmgr data. */
					if (!(record->xl_info & XLR_BKP_BLOCK_MASK))
					{
						xl_heap_header *header = (xl_heap_header *)
							(XLogRecGetData(record) + SizeOfHeapInsert);
						printf("header: t_infomask2 %d t_infomask %d t_hoff %d\n",
							header->t_infomask2,
							header->t_infomask,
							header->t_hoff);
					}
					else
						printf("header: none\n");

					break;
				}
				case XLOG_HEAP_DELETE:
				{
					xl_heap_delete xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);
					
					if(statements)
						printf("DELETE FROM %s WHERE ...", relName);
					
					printf("delete%s: ts %s db %s rel %s block %u off %u\n",
						   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
						   spaceName, dbName, relName,
						   ItemPointerGetBlockNumber(&xlrec.target.tid),
						   ItemPointerGetOffsetNumber(&xlrec.target.tid));
					break;
				}
				case XLOG_HEAP_UPDATE:
				//case XLOG_HEAP_HOT_UPDATE:   // GPDB doesn't have HOT yet
				{
					xl_heap_update xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					if(statements)
						printUpdate((xl_heap_update *) XLogRecGetData(record), record->xl_len - SizeOfHeapUpdate - SizeOfHeapHeader);

					printf("%supdate%s: ts %s db %s rel %s block %u off %u to block %u off %u\n",
						   /*(info & XLOG_HEAP_HOT_UPDATE) ? "hot_" : */ "",  
						   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
						   spaceName, dbName, relName,
						   ItemPointerGetBlockNumber(&xlrec.target.tid),
						   ItemPointerGetOffsetNumber(&xlrec.target.tid),
						   ItemPointerGetBlockNumber(&xlrec.newtid),
						   ItemPointerGetOffsetNumber(&xlrec.newtid));
					break;
				}
				case XLOG_HEAP_MOVE:
				{
					xl_heap_update xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);
					printf("move%s: ts %s db %s rel %s block %u off %u to block %u off %u\n",
						   (info & XLOG_HEAP_INIT_PAGE) ? "(init)" : "",
						   spaceName, dbName, relName,
						   ItemPointerGetBlockNumber(&xlrec.target.tid),
						   ItemPointerGetOffsetNumber(&xlrec.target.tid),
						   ItemPointerGetBlockNumber(&xlrec.newtid),
						   ItemPointerGetOffsetNumber(&xlrec.newtid));
					break;
				}
				case XLOG_HEAP_CLEAN:
				{
					xl_heap_clean xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.heapnode.node.spcNode);
					getDbName(xlrec.heapnode.node.dbNode);
					getRelName(xlrec.heapnode.node.relNode);
					printf("clean%s: ts %s db %s rel %s block %u\n",
						   (info & XLOG_HEAP_INIT_PAGE) ? "+INIT" : "",
						   spaceName, dbName, relName,
						   xlrec.block);
					break;
				}
				case XLOG_HEAP_NEWPAGE:
				{
					xl_heap_newpage xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.heapnode.node.spcNode);
					getDbName(xlrec.heapnode.node.dbNode);
					getRelName(xlrec.heapnode.node.relNode);
					printf("newpage: ts %s db %s rel %s block %u\n", 
							spaceName, dbName, relName,
						   xlrec.blkno);
					break;
				}
				case XLOG_HEAP_LOCK:
				{
					xl_heap_lock xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);
					printf("lock %s: ts %s db %s rel %s block %u off %u\n",
						   xlrec.shared_lock ? "shared" : "exclusive",
						   spaceName, dbName, relName,
						   ItemPointerGetBlockNumber(&xlrec.target.tid),
						   ItemPointerGetOffsetNumber(&xlrec.target.tid));
					break;
				}
#ifdef XLOG_HEAP_INPLACE
				case XLOG_HEAP_INPLACE:
				{
					xl_heap_inplace xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);
					printf("inplace: ts %s db %s rel %s block %u off %u\n", 
							spaceName, dbName, relName,
						   	ItemPointerGetBlockNumber(&xlrec.target.tid),
						   	ItemPointerGetOffsetNumber(&xlrec.target.tid));
					break;
				}
#endif
			}
			break;
		case RM_BTREE_ID:
			switch (info)
			{
				case XLOG_BTREE_INSERT_LEAF:
				{
					xl_btree_insert xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("insert_leaf: index %s/%s/%s tid %u/%u\n",
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid);
					break;
				}
				case XLOG_BTREE_INSERT_UPPER:
				{
					xl_btree_insert xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("insert_upper: index %s/%s/%s tid %u/%u\n",
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid);
					break;
				}
				case XLOG_BTREE_INSERT_META:
					dump_xlog_btree_insert_meta(record);
					break;

				case XLOG_BTREE_SPLIT_L:
				{
					xl_btree_split xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("split_l: index %s/%s/%s tid %u/%u otherblk %u\n", 
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.otherblk);
					break;
				}
				case XLOG_BTREE_SPLIT_R:
				{
					xl_btree_split xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("split_r: index %s/%s/%s tid %u/%u otherblk %u\n", 
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.otherblk);
					break;
				}
				case XLOG_BTREE_SPLIT_L_ROOT:
				{
					xl_btree_split xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("split_l_root: index %s/%s/%s tid %u/%u otherblk %u\n", 
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.otherblk);
					break;
				}
				case XLOG_BTREE_SPLIT_R_ROOT:
				{
					xl_btree_split xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("split_r_root: index %s/%s/%s tid %u/%u otherblk %u\n", 
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.otherblk);
					break;
				}

				case XLOG_BTREE_DELETE:
				{
					xl_btree_delete xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.btreenode.node.spcNode);
					getDbName(xlrec.btreenode.node.dbNode);
					getRelName(xlrec.btreenode.node.relNode);
					printf("delete: index %s/%s/%s block %u\n", 
							spaceName, dbName,	relName,
						   	xlrec.block);
					break;
				}
				case XLOG_BTREE_DELETE_PAGE:
				{
					xl_btree_delete_page xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("delete_page: index %s/%s/%s tid %u/%u deadblk %u\n",
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.deadblk);
					break;
				}
				case XLOG_BTREE_DELETE_PAGE_META:
				{
					xl_btree_delete_page xlrec;
					xl_btree_metadata md;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					memcpy(&md, XLogRecGetData(record) + sizeof(xlrec),
						sizeof(xl_btree_metadata));

					printf("delete_page_meta: index %s/%s/%s tid %u/%u deadblk %u root %u/%u froot %u/%u\n", 
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.deadblk,
							md.root, md.level, md.fastroot, md.fastlevel);
					break;
				}
				case XLOG_BTREE_NEWROOT:
				{
					xl_btree_newroot xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.btreenode.node.spcNode);
					getDbName(xlrec.btreenode.node.dbNode);
					getRelName(xlrec.btreenode.node.relNode);

					printf("newroot: index %s/%s/%s rootblk %u level %u\n", 
							spaceName, dbName, relName,
						   	xlrec.rootblk, xlrec.level);
					break;
				}
				case XLOG_BTREE_DELETE_PAGE_HALF:
				{
					xl_btree_delete_page xlrec;

					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.target.node.spcNode);
					getDbName(xlrec.target.node.dbNode);
					getRelName(xlrec.target.node.relNode);

					printf("delete_page_half: index %s/%s/%s tid %u/%u deadblk %u\n",
							spaceName, dbName, relName,
						   	BlockIdGetBlockNumber(&xlrec.target.tid.ip_blkid),
						   	xlrec.target.tid.ip_posid,
						   	xlrec.deadblk);
					break;
				}
				default:
					printf("a btree xlog type (%d) that isn't yet implemented in xlogdump", info);
					break;
			}
			break;
		case RM_BITMAP_ID:
			switch (info)
			{
				case XLOG_BITMAP_INSERT_NEWLOV:
				{
					xl_bm_newpage	xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_insert_newlov: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_new_blkno);

					break;
				}
				case XLOG_BITMAP_INSERT_LOVITEM:
				{
					xl_bm_lovitem	xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_insert_lovitem: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_lov_blkno);
					
					if (xlrec.bm_is_new_lov_blkno)
						printf("bitmap_insert_lovitem: index %s/%s/%s block %u\n",
								spaceName, dbName, relName,
								BM_METAPAGE);
					break;
				}
				case XLOG_BITMAP_INSERT_META:
				{
					xl_bm_metapage	xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_insert_meta: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							BM_METAPAGE);

					break;
				}
				case XLOG_BITMAP_INSERT_BITMAP_LASTWORDS:
				{
					xl_bm_bitmap_lastwords xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_insert_lastwords: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_lov_blkno);

					break;
				}
				case XLOG_BITMAP_INSERT_WORDS:
				{
					xl_bm_bitmapwords	xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_insert_words: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_lov_blkno);
					
					if (!xlrec.bm_is_last)
						printf("bitmap_insert_words: index %s/%s/%s block %u\n",
								spaceName, dbName, relName,
								xlrec.bm_next_blkno);

					break;
				}
				case XLOG_BITMAP_UPDATEWORD:
				{
					xl_bm_updateword	xlrec;
					
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_update_word: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_blkno);
				
					break;
				}
				case XLOG_BITMAP_UPDATEWORDS:
				{
					xl_bm_updatewords	xlrec;
				
					memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));
					getSpaceName(xlrec.bm_node.spcNode);
					getDbName(xlrec.bm_node.dbNode);
					getRelName(xlrec.bm_node.relNode);

					printf("bitmap_update_words: index %s/%s/%s block %u\n",
							spaceName, dbName, relName,
							xlrec.bm_first_blkno);

					
					if (xlrec.bm_two_pages)
						printf("bitmap_update_words: index %s/%s/%s block %u\n",
								spaceName, dbName, relName,
								xlrec.bm_second_blkno);

					if (xlrec.bm_new_lastpage)
						printf("bitmap_update_words: index %s/%s/%s block %u\n",
								spaceName, dbName, relName,
								xlrec.bm_lov_blkno);

					break;
				}
			}
			break;
		case RM_HASH_ID:
			break;
		case RM_GIST_ID:
			dumpGIST(record);
			break;
		case RM_SEQ_ID:
			break;
		case RM_MMXLOG_ID:
			{
				xl_mm_fs_obj xlrec;

				memcpy(&xlrec, XLogRecGetData(record), sizeof(xlrec));

				switch (info)
				{
					case MMXLOG_CREATE_DIR:
						printf("master mirror: create directory ");
						break;
					case MMXLOG_REMOVE_DIR:
						printf("master mirror: remove directory ");
						break;
					case MMXLOG_CREATE_FILE:
						printf("master mirror: create file ");
						break;
					case MMXLOG_REMOVE_FILE:
						printf("master mirror: remove file ");
						break;
				}

				printf("master dbid = %u, standby dbid = %u, master path = %s, standby path = %s\n",
					   xlrec.u.dbid.master, xlrec.u.dbid.mirror,
					   xlrec.master_path, xlrec.mirror_path);
				printf("filespace = %u, tablespace = %u, database = %u, relfilenode = %u, segnum = %u\n",
					   xlrec.filespace, xlrec.tablespace, xlrec.database, xlrec.relfilenode, xlrec.segnum);
			}
			break;
	}
}

/*
 * Adds a transaction to a linked list of transactions
 * If the transactions xid already is on the list it sums the total len and check for a status change
 */
static void
addTransaction(XLogRecord *record)
{
	uint8	info = record->xl_info & ~XLR_INFO_MASK;
	int	status = 0;
	if(record->xl_rmid == RM_XACT_ID)
	{
		if (info == XLOG_XACT_COMMIT)
			status = 1;
		else if (info == XLOG_XACT_ABORT)
			status = 2;
	}

	if(transactionsInfo != NULL)
	{
		transInfoPtr element = transactionsInfo;
		while (element->next != NULL || element->xid == record->xl_xid)
		{
			if(element->xid == record->xl_xid)
			{
				element->tot_len += record->xl_tot_len;
				if(element->status == 0)
					element->status = status;
				return;
			}
			element = element->next;
		}
		element->next = (transInfoPtr) malloc(sizeof(transInfo));
		element = element->next;
		element->xid = record->xl_xid;
		element->tot_len = record->xl_tot_len;
		element->status = status;
		element->next = NULL;
		return;
	}
	else
	{
		transactionsInfo = (transInfoPtr) malloc(sizeof(transInfo));
		transactionsInfo->xid = record->xl_xid;
		transactionsInfo->tot_len = record->xl_tot_len;
		transactionsInfo->status = status;
		transactionsInfo->next = NULL;
	}
}

static void
dumpTransactions()
{
	transInfo * element = transactionsInfo;
	if(!element)
	{
		printf("\nCorrupt or incomplete transaction.\n");
		return;
	}

	while (element->next != NULL)
	{
		printf("\nxid: %u total length: %u status: %s", element->xid, element->tot_len, status_names[element->status]);
		element = element->next;
	}
	printf("\n");
}

static void
dumpXLog(char* fname)
{
	char	*fnamebase;

	printf("\n%s:\n\n", fname);
	/*
	 * Extract logfile id and segment from file name
	 */
	fnamebase = strrchr(fname, '/');
	if (fnamebase)
		fnamebase++;
	else
		fnamebase = fname;
	if (sscanf(fnamebase, "%8x%8x%8x", &logTLI, &logId, &logSeg) != 3)
	{
		fprintf(stderr, "Can't recognize logfile name '%s'\n", fnamebase);
		logTLI = logId = logSeg = 0;
	}
	logPageOff = -XLOG_BLCKSZ;		/* so 1st increment in readXLogPage gives 0 */
	logRecOff = 0;
	while (ReadRecord())
	{
		if(!transactions)
			dumpXLogRecord((XLogRecord *) readRecordBuf, false);
		else
			addTransaction((XLogRecord *) readRecordBuf);

		prevRecPtr = curRecPtr;
	}
	if(transactions)
		dumpTransactions();
}

static void
help(void)
{
	printf("xlogdump ... \n\n");
	printf("Usage:\n");
	printf("  xlogdump [OPTION]... [segment file]\n");
	printf("\nOptions controlling the output content:\n");
	printf("  -r, --rmname=OPERATION    Outuputs only the transaction log records\n"); 
	printf("                            containing the specified operation\n");
	printf("  -t, --transactions        Outuputs only transaction info: the xid,\n");
	printf("                            total length and status of each transaction\n");
	printf("  -s, --statements          Tries to build fake statements that produce the\n");
	printf("                            physical changes found within the xlog segments\n");
	printf("\nConnection options:\n");
	printf("  -h, --host=HOST           database server host or socket directory\n");
	printf("  -p, --port=PORT           database server port number\n");
	printf("  -U, --username=NAME       connect as specified database user\n\n");
	printf("Report bugs to <diogob@gmail.com>.\n");
	exit(0);
}

int
main(int argc, char** argv)
{
	int	c, i, optindex;

	static struct option long_options[] = {
		{"transactions", no_argument, NULL, 't'},
		{"statements", no_argument, NULL, 's'},
		{"hide-timestamps", no_argument, NULL, 'T'},	
		{"rmid", required_argument, NULL, 'r'},
		{"host", required_argument, NULL, 'h'},
		{"port", required_argument, NULL, 'p'},
		{"username", required_argument, NULL, 'U'},
		{"help", no_argument, NULL, '?'},
		{NULL, 0, NULL, 0}
	};
	
	if (argc == 1 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-?"))
		help();

	while ((c = getopt_long(argc, argv, "stcTr:h:p:U:",
							long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 's':			/* show statements */
				statements = true;
				break;

			case 't':			
				transactions = true;	/* show only transactions */
				break;

			case 'T':			/* hide timestamps (used for testing) */
				hideTimestamps = true;
				break;
			case 'r':			/* output only rmid passed */
				sprintf(rmname, "%-5s", optarg);
				break;
			case 'h':			/* host for tranlsting oids */
				pghost = optarg;
				break;
			case 'p':			/* port for translating oids */
				pgport = optarg;
				break;
			case 'U':			/* username for translating oids */
				username = optarg;
				break;
			default:
				fprintf(stderr, "Try \"xlogdump --help\" for more information.\n");
				exit(1);
		}
	}

	if (statements && transactions)
	{
		fprintf(stderr, "options \"statements\" (-s) and \"transactions\" (-t) cannot be used together\n");
		exit(1);
	}

	if (strcmp("ALL  ", rmname) && transactions)
	{
		fprintf(stderr, "options \"rmid\" (-r) and \"transactions\" (-t) cannot be used together\n");
		exit(1);
	}

	if(pghost)
	{
		conn = DBConnect(pghost, pgport, "template1", username);
	}
	dbQry = createPQExpBuffer();

	for (i = optind; i < argc; i++)
	{
		char *fname = argv[i];
		logFd = open(fname, O_RDONLY | PG_BINARY, 0);

		if (logFd < 0)
		{
			perror(fname);
			continue;
		}
		dumpXLog(fname);
	}

	exit_gracefuly(0);
	
	/* just to avoid a warning */
	return 0;
}

/*
 * Routines needed if headers were configured for ASSERT
 */
#ifndef assert_enabled
bool		assert_enabled = true;
#endif

int
ExceptionalCondition(const char *conditionName,
					 const char *errorType,
					 const char *fileName,
					 int lineNumber)
{
	fprintf(stderr, "TRAP: %s(\"%s\", File: \"%s\", Line: %d)\n",
			errorType, conditionName,
			fileName, lineNumber);

	abort();
	return 0;
}


typedef struct
{
    gistxlogPageUpdate *data;
    int         len;
    IndexTuple *itup;
    OffsetNumber *todelete;
} PageUpdateRecord;

/* copied from backend/access/gist/gistxlog.c */
static void
decodePageUpdateRecord(PageUpdateRecord *decoded, XLogRecord *record)
{
	char	   *begin = XLogRecGetData(record),
			   *ptr;
	int			i = 0,
				addpath = 0;

	decoded->data = (gistxlogPageUpdate *) begin;

	if (decoded->data->ntodelete)
	{
		decoded->todelete = (OffsetNumber *) (begin + sizeof(gistxlogPageUpdate) + addpath);
		addpath = MAXALIGN(sizeof(OffsetNumber) * decoded->data->ntodelete);
	}
	else
		decoded->todelete = NULL;

	decoded->len = 0;
	ptr = begin + sizeof(gistxlogPageUpdate) + addpath;
	while (ptr - begin < record->xl_len)
	{
		decoded->len++;
		ptr += IndexTupleSize((IndexTuple) ptr);
	}

	decoded->itup = (IndexTuple *) malloc(sizeof(IndexTuple) * decoded->len);

	ptr = begin + sizeof(gistxlogPageUpdate) + addpath;
	while (ptr - begin < record->xl_len)
	{
		decoded->itup[i] = (IndexTuple) ptr;
		ptr += IndexTupleSize(decoded->itup[i]);
		i++;
	}
}

/* copied from backend/access/gist/gistxlog.c */
typedef struct
{
    gistxlogPage *header;
    IndexTuple *itup;
} NewPage;

/* copied from backend/access/gist/gistxlog.c */
typedef struct
{
    gistxlogPageSplit *data;
    NewPage    *page;
} PageSplitRecord;

/* copied from backend/access/gist/gistxlog.c */
static void
decodePageSplitRecord(PageSplitRecord *decoded, XLogRecord *record)
{
	char	   *begin = XLogRecGetData(record),
			   *ptr;
	int			j,
				i = 0;

	decoded->data = (gistxlogPageSplit *) begin;
	decoded->page = (NewPage *) malloc(sizeof(NewPage) * decoded->data->npage);

	ptr = begin + sizeof(gistxlogPageSplit);
	for (i = 0; i < decoded->data->npage; i++)
	{
		Assert(ptr - begin < record->xl_len);
		decoded->page[i].header = (gistxlogPage *) ptr;
		ptr += sizeof(gistxlogPage);

		decoded->page[i].itup = (IndexTuple *)
			malloc(sizeof(IndexTuple) * decoded->page[i].header->num);
		j = 0;
		while (j < decoded->page[i].header->num)
		{
			Assert(ptr - begin < record->xl_len);
			decoded->page[i].itup[j] = (IndexTuple) ptr;
			ptr += IndexTupleSize((IndexTuple) ptr);
			j++;
		}
	}
}

void
dumpGIST(XLogRecord *record)
{
	int info = record->xl_info & ~XLR_INFO_MASK;
	switch (info)
	{
		case XLOG_GIST_PAGE_UPDATE:
		case XLOG_GIST_NEW_ROOT:
			{
				int i;
				PageUpdateRecord rec;
				decodePageUpdateRecord(&rec, record);

				printf("%s: rel=(%u/%u/%u) blk=%u key=(%d,%d) add=%d ntodelete=%d\n",
					info == XLOG_GIST_PAGE_UPDATE ? "page_update" : "new_root",
					rec.data->node.spcNode, rec.data->node.dbNode,
					rec.data->node.relNode,
					rec.data->blkno,
					ItemPointerGetBlockNumber(&rec.data->key),
					rec.data->key.ip_posid,
					rec.len,
					rec.data->ntodelete
				);
				for (i = 0; i < rec.len; i++)
				{
					printf("  itup[%d] points (%d, %d)\n",
						i,
						ItemPointerGetBlockNumber(&rec.itup[i]->t_tid),
						rec.itup[i]->t_tid.ip_posid
					);
				}
				for (i = 0; i < rec.data->ntodelete; i++)
				{
					printf("  todelete[%d] offset %d\n", i, rec.todelete[i]);
				}
				free(rec.itup);
			}
			break;
		case XLOG_GIST_PAGE_SPLIT:
			{
				int i;
				PageSplitRecord rec;

				decodePageSplitRecord(&rec, record);
				printf("page_split: orig %u key (%d,%d)\n",
					rec.data->origblkno,
					ItemPointerGetBlockNumber(&rec.data->key),
					rec.data->key.ip_posid
				);
				for (i = 0; i < rec.data->npage; i++)
				{
					printf("  page[%d] block %u tuples %d\n",
						i,
						rec.page[i].header->blkno,
						rec.page[i].header->num
					);
#if 0
					for (int j = 0; j < rec.page[i].header->num; j++)
					{
						NewPage *newpage = rec.page + i;
						printf("   itup[%d] points (%d,%d)\n",
							j,
							BlockIdGetBlockNumber(&newpage->itup[j]->t_tid.ip_blkid),
							newpage->itup[j]->t_tid.ip_posid
						);
					}
#endif
					free(rec.page[i].itup);
				}
			}
			break;
		case XLOG_GIST_INSERT_COMPLETE:
			{
				printf("insert_complete: \n");
			}
			break;
		case XLOG_GIST_CREATE_INDEX:
			printf("create_index: \n");
			break;
		case XLOG_GIST_PAGE_DELETE:
			printf("page_delete: \n");
			break;
	}
}

void dump_xlog_btree_insert_meta(XLogRecord *record)
{
	xl_btree_insert *xlrec = (xl_btree_insert *) XLogRecGetData(record);
	char *datapos;
	int datalen;
	xl_btree_metadata md;
	BlockNumber downlink;

	/* copied from btree_xlog_insert(nbtxlog.c:191) */
	datapos = (char *) xlrec + SizeOfBtreeInsert;
	datalen = record->xl_len - SizeOfBtreeInsert;

	getSpaceName(xlrec->target.node.spcNode);
	getDbName(xlrec->target.node.dbNode);
	getRelName(xlrec->target.node.relNode);

	/* downlink */
	memcpy(&downlink, datapos, sizeof(BlockNumber));
	datapos += sizeof(BlockNumber);
	datalen -= sizeof(BlockNumber);

	/* xl_insert_meta */
	memcpy(&md, datapos, sizeof(xl_btree_metadata));
	datapos += sizeof(xl_btree_metadata);
	datalen -= sizeof(xl_btree_metadata);

	printf("insert_meta: index %s/%s/%s tid %u/%u downlink %u froot %u/%u\n", 
		spaceName, dbName, relName,
		BlockIdGetBlockNumber(&xlrec->target.tid.ip_blkid),
		xlrec->target.tid.ip_posid,
		downlink,
		md.fastroot, md.fastlevel
	);
}
