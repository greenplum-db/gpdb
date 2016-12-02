/*-------------------------------------------------------------------------
 *
 * changetrackingdump.c
 *		Simple utility for dumping Greenplum changetracking files.
 *
 * Usage: changetrackingdump [options] changetrackingfile [ changetrackingfile ... ] > output
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#include <getopt_long.h>

#include "libpq-fe.h"
#include "pqexpbuffer.h"

#include "storage/itemptr.h"
#include "cdb/cdbresynchronizechangetracking.h"

#define CTDUMP_INVALID_OID -1

static int		logFd;		/* kernel FD for current input file */
static uint32	logSeg;		/* current log file segment */
static int32	logPageOff;	/* offset of current page in file */
static int		logRecOff;	/* offset of next record in page */
static char		pageBuffer[CHANGETRACKING_BLCKSZ];	/* current page */
static char		*readCTRecordBuf = NULL; /* ReadCTRecord result area */
static char		*readCTHeaderBuf = NULL; /* Header result area */
static char		*readMDRecordBuf = NULL; /* ReadMDRecord result area */
static int		recs_read = 0;
static int		recs_in_page = 0;
static int		block_number = 0;
static int		file_is_log = 0;

static PGconn		*conn = NULL; /* Connection for translating oids of global objects */
static PGconn		*lastDbConn = NULL; /* Connection for translating oids of per database objects */
static PGresult		*res;
static PQExpBuffer	dbQry;

/* command-line parameters */
const char 		*pghost = NULL; /* connection host */
const char 		*pgport = NULL; /* connection port */
const char 		*username = NULL; /* connection username */

/* Oids used for checking if we need to search for an objects name or if we can use the last one */
static Oid 		lastDbOid = CTDUMP_INVALID_OID;
static Oid 		lastSpcOid = CTDUMP_INVALID_OID;
static Oid 		lastRelOid = CTDUMP_INVALID_OID;

/* Buffers to hold objects names */
static char 		spaceName[NAMEDATALEN] = "";
static char 		dbName[NAMEDATALEN]  = "";
static char 		relName[NAMEDATALEN]  = "";


/* prototypes */
static void exit_gracefuly(int status);
static PGconn * DBConnect(const char *host, const char *port, char *database, const char *user);
static void dumpCTRecord(ChangeTrackingRecord *record);

/* Read another page, if possible */
static bool
readChangeTrackingPage(void)
{
	size_t nread = read(logFd, pageBuffer, CHANGETRACKING_BLCKSZ);

	if (nread == CHANGETRACKING_BLCKSZ)
	{
		logPageOff += CHANGETRACKING_BLCKSZ;
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

	if (password)
		free(password);
	
	return conn;
}


/*
 * Attempt to read a ChangeTracking record into readCTRecordBuf.
 */
static bool
ReadCTRecord(void)
{
	char	   *buffer;
	ChangeTrackingRecord *record;
	ChangeTrackingPageHeader	*header;
	uint32		header_len = sizeof(ChangeTrackingPageHeader);
	uint32		record_len = sizeof(ChangeTrackingRecord);

	if (logRecOff <= 0 || recs_read == recs_in_page)
	{
		/* Need to advance to new page */
		if (!readChangeTrackingPage())
			return false;
		
		logRecOff = 0;
		recs_read = 0;
		
		/* read page header */
		memcpy(readCTHeaderBuf, pageBuffer, header_len);
		header = (ChangeTrackingPageHeader *)readCTHeaderBuf;
		
		printf("-----------------------------------\n");
		printf("block version: %d\n", header->blockversion);
		printf("block number : %d\n", ++block_number);
		printf("records in block: %d\n", header->numrecords);
		printf("-----------------------------------\n");
		
		recs_in_page = header->numrecords;
		
		logRecOff += sizeof(ChangeTrackingPageHeader);
	}
	
	
	record = (ChangeTrackingRecord *) (pageBuffer + logRecOff);
	buffer = readCTRecordBuf;
	
	memcpy(buffer, record, record_len);
	record = (ChangeTrackingRecord *) buffer;
	logRecOff += record_len;
	recs_read++;
	
	return true;
}

/*
 * Attempt to read a ChangeTracking meta data into readCTRecordBuf.
 */
static void
DumpMDRecord(void)
{
	ChangeTrackingMetaRecord *record;
	uint32		record_len = sizeof(ChangeTrackingMetaRecord);
	int			numread = 0;
	
	/* we ask to read BLCKSZ even though there are less bytes there */
	numread = read(logFd, pageBuffer, CHANGETRACKING_METABUFLEN);
	
	if (numread <= 0)
	{
		printf("no data in MD file!");
		return;
	}
	else if (numread < CHANGETRACKING_METABUFLEN)
	{
		printf("found a corrupted MD record! only %d bytes were read", numread);
		return;
	}
		
	memcpy(readMDRecordBuf, pageBuffer, record_len);
	record = (ChangeTrackingMetaRecord *)readMDRecordBuf;
		
	printf("-----------------------------------\n");
	printf("Resync LSN end: [%X/%X]\n", record->resync_lsn_end.xlogid, record->resync_lsn_end.xrecoff);
	printf("Resync is full? : %s\n", (record->resync_mode_full ? "yes" : "no"));
	printf("Resync transition completed? : %s\n", (record->resync_transition_completed ? "yes" : "no"));
	printf("Insync transition completed? : %s\n", (record->insync_transition_completed ? "yes" : "no"));	
	printf("-----------------------------------\n");
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
			strlcpy(spaceName, PQgetvalue(res, 0, 0), sizeof(spaceName));
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
			strlcpy(dbName, PQgetvalue(res, 0, 0), sizeof(dbName));

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
			strlcpy(relName, PQgetvalue(res, 0, 0), sizeof(relName));
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

/* ======= the following taken from itemptr.h ======== */
static char *
ItemPointerToBufferX(char *buffer, ItemPointer tid)
{
	// Do not assert valid ItemPointer -- it is ok if it is (0,0)...
	BlockNumber blockNumber = BlockIdGetBlockNumber(&tid->ip_blkid);
	OffsetNumber offsetNumber = tid->ip_posid;
	
	sprintf(buffer,
		    "(%u,%u)",
		    blockNumber, 
		    offsetNumber);

	return buffer;
}

static char itemPointerBuffer[50];

static char *
ItemPointerToStringX(ItemPointer tid)
{
	return ItemPointerToBufferX(itemPointerBuffer, tid);
}

/* ======= end ======== */

static void
dumpCTRecord(ChangeTrackingRecord *record)
{
	
	printf("(%04d/%04d): ", recs_read, recs_in_page);

	getSpaceName(record->relFileNode.spcNode);
	getDbName(record->relFileNode.dbNode);
	getRelName(record->relFileNode.relNode);

	printf("space %8s, db %8s, tblname %8s, ", spaceName, dbName, relName);
	printf("xlogloc [%X/%X] ", record->xlogLocation.xlogid, record->xlogLocation.xrecoff);	
	printf("block number %4d ", record->bufferPoolBlockNum);
	printf("persistent tid %s ", ItemPointerToStringX(&record->persistentTid));
	printf("persistent sn " INT64_FORMAT, record->persistentSerialNum);
	printf("\n");
}


static void
dumpChangeTracking(char* fname)
{
	char	*fnamebase;

	fnamebase = strrchr(fname, '/');
	if (fnamebase)
		fnamebase++;
	else
		fnamebase = fname;
	
	/* verify it's a valid changetracking file */
	if (strcmp(fnamebase, "CT_LOG_FULL") != 0 &&
		strcmp(fnamebase, "CT_LOG_COMPACT") != 0 &&
		strcmp(fnamebase, "CT_METADATA") != 0)
	{
		fprintf(stderr, "file '%s' is not a change tracking log or meta file.\n", fnamebase);
		return;
	}
	
	if (strncmp(fnamebase, "CT_LOG_", strlen("CT_LOG_")) == 0)
		file_is_log = 1; /* CT */
	else
		file_is_log = 0; /* MD */
	
	printf("\ndumping change tracking %s file %s:\n\n", (file_is_log ? "log" : "meta"), fname);
	
	logSeg = 1;
	
	if(file_is_log)
	{
		/* dump CT file */
		
		logPageOff = -CHANGETRACKING_BLCKSZ;		/* so 1st increment in readXLogPage gives 0 */
		logRecOff = 0;
		recs_read = recs_in_page = 0;
		
		readCTRecordBuf = (char *) malloc(sizeof(ChangeTrackingRecord));
		readCTHeaderBuf = (char *) malloc(sizeof(ChangeTrackingPageHeader));
		
		while (ReadCTRecord())
		{
			dumpCTRecord((ChangeTrackingRecord *) readCTRecordBuf);
		}
		
		free(readCTRecordBuf);
		free(readCTHeaderBuf);
	}
	else
	{
		/* dump MD file */
		
		readMDRecordBuf = (char *) malloc(sizeof(ChangeTrackingMetaRecord));
		DumpMDRecord();		
		free(readMDRecordBuf);
	}
}

static void
help(void)
{
	printf("\nchangetrackingdump ... \n\n");
	printf("Description:\n");
	printf("  dumps the contents of a changetracking log file or meta file\n");
	printf("\nUsage:\n");
	printf("  changetrackingdump [OPTION]... filename\n");
	printf("\nConnection options:\n");
	printf("  -h, --host=HOST           database server host or socket directory\n");
	printf("  -p, --port=PORT           database server port number\n");
	printf("  -U, --username=NAME       connect as specified database user\n\n");
	exit(0);
}

int
main(int argc, char** argv)
{
	int	c, i, optindex;

	static struct option long_options[] = {
		{"host", required_argument, NULL, 'h'},
		{"port", required_argument, NULL, 'p'},
		{"username", required_argument, NULL, 'U'},
		{"help", no_argument, NULL, '?'},
		{NULL, 0, NULL, 0}
	};
	
	if (argc == 1 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-?"))
		help();

	while ((c = getopt_long(argc, argv, "h:p:U:",
							long_options, &optindex)) != -1)
	{
		switch (c)
		{
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
				fprintf(stderr, "Try \"changetrackingdump --help\" for more information.\n");
				exit(1);
		}
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
		dumpChangeTracking(fname);
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
