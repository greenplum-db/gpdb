/*
 * gpmodcontrol.c
 *
 *
 * gpmodcontrol is a tool to convert pg_control file from one to another version.
 * This supports between only two versions.  Downgrade mode is used in order to
 * rollback in case of some failure.  The tool will be executed by gpmigrator to
 * change the structure at the beginning of upgrade sequence.
 *
 * All of old definitions are copied from the previous version.
 *
 * It is not recommended for normal users to directly use this tool.
 *
 */
#include "postgres.h"

#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>

#include "access/xlog_internal.h"
#include "catalog/catversion.h"
#include "catalog/pg_control.h"

/* Definitions from GP_VERSION_NUM == 40200 */
#define OLD_PG_CONTROL_VERSION 822
#define OLD_CATALOG_VERSION_NO 201109210

/* System status indicator for old version */
typedef enum OldDBState
{
	OLD_DB_STARTUP = 0,
	OLD_DB_SHUTDOWNED,
	OLD_DB_SHUTDOWNING,
	OLD_DB_IN_CRASH_RECOVERY,
	OLD_DB_IN_ARCHIVE_RECOVERY,
	OLD_DB_IN_PRODUCTION
} OldDBState;

/*
 * ControlFileData of old version
 */
typedef struct OldControlFileData
{
	/*
	 * Unique system identifier --- to ensure we match up xlog files with the
	 * installation that produced them.
	 */
	uint64		system_identifier;

	/*
	 * Version identifier information.	Keep these fields at the same offset,
	 * especially pg_control_version; they won't be real useful if they move
	 * around.	(For historical reasons they must be 8 bytes into the file
	 * rather than immediately at the front.)
	 *
	 * pg_control_version identifies the format of pg_control itself.
	 * catalog_version_no identifies the format of the system catalogs.
	 *
	 * There are additional version identifiers in individual files; for
	 * example, WAL logs contain per-page magic numbers that can serve as
	 * version cues for the WAL log.
	 */
	uint32		pg_control_version;		/* PG_CONTROL_VERSION */
	uint32		catalog_version_no;		/* see catversion.h */

	/*
	 * System status data
	 */
	DBState		state;			/* see enum above */
	time_t		time;			/* time stamp of last pg_control update */
	uint32		logId;			/* current log file id */
	uint32		logSeg;			/* current log file segment, + 1 */
	XLogRecPtr	checkPoint;		/* last check point record ptr */
	XLogRecPtr	prevCheckPoint; /* previous check point record ptr */

	CheckPoint	checkPointCopy; /* copy of last check point record */

	XLogRecPtr	minRecoveryPoint;		/* must replay xlog to here */

	/*
	 * This data is used to check for hardware-architecture compatibility of
	 * the database and the backend executable.  We need not check endianness
	 * explicitly, since the pg_control version will surely look wrong to a
	 * machine of different endianness, but we do need to worry about MAXALIGN
	 * and floating-point format.  (Note: storage layout nominally also
	 * depends on SHORTALIGN and INTALIGN, but in practice these are the same
	 * on all architectures of interest.)
	 *
	 * Testing just one double value is not a very bulletproof test for
	 * floating-point compatibility, but it will catch most cases.
	 */
	uint32		maxAlign;		/* alignment requirement for tuples */
	double		floatFormat;	/* constant 1234567.0 */
#define FLOATFORMAT_VALUE	1234567.0

	/*
	 * This data is used to make sure that configuration of this database is
	 * compatible with the backend executable.
	 */
	uint32		blcksz;			/* data block size for this DB */
	uint32		relseg_size;	/* blocks per segment of large relation */

	uint32		xlog_blcksz;	/* block size within WAL files */
	uint32		xlog_seg_size;	/* size of each WAL segment */

	uint32		nameDataLen;	/* catalog name field width */
	uint32		indexMaxKeys;	/* max number of columns in an index */

	/* flag indicating internal format of timestamp, interval, time */
	uint32		enableIntTimes; /* int64 storage enabled? */

	/* active locales */
	uint32		localeBuflen;
	char		lc_collate[LOCALE_NAME_BUFLEN];
	char		lc_ctype[LOCALE_NAME_BUFLEN];

	/* CRC of all above ... MUST BE LAST! */
	pg_crc32	crc;
} OldControlFileData;

typedef struct ControlFileData NewControlFileData;

/*
 * A union of common fields + newer/older control file.
 */
typedef union CommonControlFile
{
	struct
	{
		uint64		system_identifier;		/* unique system identifier */
		uint32		pg_control_version;		/* PG_CONTROL_VERSION */
		uint32		catalog_version_no;		/* see catversion.h */
		int			state;					/* see enum above */
	} common;
	NewControlFileData	newer;
	OldControlFileData	older;
} CommonControlFile;

static void
usage(const char *progname)
{
	printf(_("%s modifies control information of a Greenplum Database cluster.\n\n"), progname);
	printf
		(
		 _(
		   "Usage:\n"
		   "  %s [OPTION] [DATADIR]\n\n"
		   "Options:\n"
		   "  --help                    show this help, then exit\n"
		   "  --version                 output version information, then exit\n"
		   "  --create-old <source>     create old version pg_control\n"
		   "  --downgrade               downgrade from new version to old\n"
		   ),
		 progname
		);
	printf(_("\nIf no data directory (DATADIR) is specified, "
			 "the environment variable PGDATA\nis used.\n\n"));
}

static const char *
dbStateOld(OldDBState state)
{
	switch (state)
	{
		case OLD_DB_STARTUP:
			return _("starting up");
		case OLD_DB_SHUTDOWNED:
			return _("shut down");
		case OLD_DB_SHUTDOWNING:
			return _("shutting down");
		case OLD_DB_IN_CRASH_RECOVERY:
			return _("in crash recovery");
		case OLD_DB_IN_ARCHIVE_RECOVERY:
			return _("in archive recovery");
		case OLD_DB_IN_PRODUCTION:
			return _("in production");
	}
	return _("unrecognized status code");
}

static const char *
dbState(DBState state)
{
	switch (state)
	{
		case DB_STARTUP:
			return _("starting up");
		case DB_SHUTDOWNED:
			return _("shut down");
		case DB_SHUTDOWNING:
			return _("shutting down");
		case DB_IN_CRASH_RECOVERY:
			return _("in crash recovery");
		case DB_IN_STANDBY_MODE:
			return _("in standby mode");
		case DB_IN_STANDBY_PROMOTED:
			return _("in standby mode (promoted)");
		case DB_IN_STANDBY_NEW_TLI_SET:
			return _("in standby mode (new tli set)");
		case DB_IN_PRODUCTION:
			return _("in production");
	}
	return _("unrecognized status code");
}

/*
 * This is for test purpose.  From text output of pg_controldata of older version
 * this creates an actual pg_control file of older version.
 */
static int
CreateOldControlFile(const char *progname, char *ControlFilePath, char *sourcefile)
{
	OldControlFileData ControlFile;
	FILE   *fp;
#define BUFLEN 512
	char	buf[BUFLEN];
	char	tmp[BUFLEN];
	const char *strptime_fmt = "%c";
	pg_crc32	crc;
	int		fd;

	/* Use this for texts including spaces. */
#define ALL_TRAIL "%512[^\n]s"

	if ((fp = fopen(sourcefile, "r")) == NULL)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, sourcefile, strerror(errno));
		exit(2);
	}

	memset(&ControlFile, 0, sizeof(OldControlFileData));

	while (fgets(buf, BUFLEN, fp) != NULL)
	{
		if (sscanf(buf, "pg_control version number:            %u",
				&ControlFile.pg_control_version) == 1)
			continue;
		if (sscanf(buf, "Catalog version number:               %u",
				&ControlFile.catalog_version_no) == 1)
			continue;
		if (sscanf(buf, "Database system identifier:           " UINT64_FORMAT,
				&ControlFile.system_identifier) == 1)
			continue;
		if (sscanf(buf, "Database cluster state:               " ALL_TRAIL,
				tmp) == 1)
		{
			if (strcmp(tmp, "starting up") == 0)
				ControlFile.state = OLD_DB_STARTUP;
			else if (strcmp(tmp, "shut down") == 0)
				ControlFile.state = OLD_DB_SHUTDOWNED;
			else if (strcmp(tmp, "shutting down") == 0)
				ControlFile.state = OLD_DB_SHUTDOWNING;
			else if (strcmp(tmp, "in crash recovery") == 0)
				ControlFile.state = OLD_DB_IN_CRASH_RECOVERY;
			else if (strcmp(tmp, "in archive recovery") == 0)
				ControlFile.state = OLD_DB_IN_ARCHIVE_RECOVERY;
			else if (strcmp(tmp, "in production") == 0)
				ControlFile.state = OLD_DB_IN_PRODUCTION;
			continue;
		}
		if (sscanf(buf, "pg_control last modified:             " ALL_TRAIL,
					tmp) == 1)
		{
			struct tm t;

			if (strptime(tmp, strptime_fmt, &t) != NULL)
				ControlFile.time = mktime(&t);
			continue;
		}
		if (sscanf(buf, "Current log file ID:                  %u",
				&ControlFile.logId) == 1)
			continue;
		if (sscanf(buf, "Next log file segment:                %u",
				&ControlFile.logSeg) == 1)
			continue;
		if (sscanf(buf, "Latest checkpoint location:           %X/%X",
				&ControlFile.checkPoint.xlogid,
				&ControlFile.checkPoint.xrecoff) == 2)
			continue;
		if (sscanf(buf, "Prior checkpoint location:            %X/%X",
				&ControlFile.prevCheckPoint.xlogid,
				&ControlFile.prevCheckPoint.xrecoff) == 2)
			continue;
		if (sscanf(buf, "Latest checkpoint's REDO location:    %X/%X",
				&ControlFile.checkPointCopy.redo.xlogid,
				&ControlFile.checkPointCopy.redo.xrecoff) == 2)
			continue;
		if (sscanf(buf, "Latest checkpoint's UNDO location:    %X/%X",
				&ControlFile.checkPointCopy.undo.xlogid,
				&ControlFile.checkPointCopy.undo.xrecoff) == 2)
			continue;
		if (sscanf(buf, "Latest checkpoint's TimeLineID:       %u",
				&ControlFile.checkPointCopy.ThisTimeLineID) == 1)
			continue;
		if (sscanf(buf, "Latest checkpoint's NextXID:          %u/%u",
				&ControlFile.checkPointCopy.nextXidEpoch,
				&ControlFile.checkPointCopy.nextXid) == 2)
			continue;
		if (sscanf(buf, "Latest checkpoint's NextOID:          %u",
				&ControlFile.checkPointCopy.nextOid) == 1)
			continue;
		if (sscanf(buf, "Latest checkpoint's NextMultiXactId:  %u",
				&ControlFile.checkPointCopy.nextMulti) == 1)
			continue;
		if (sscanf(buf, "Latest checkpoint's NextMultiOffset:  %u",
				&ControlFile.checkPointCopy.nextMultiOffset) == 1)
			continue;
		if (sscanf(buf, "Time of latest checkpoint:            " ALL_TRAIL,
				tmp) == 1)
		{
			struct tm t;
			if (strptime(tmp, strptime_fmt, &t) != NULL)
				ControlFile.checkPointCopy.time = mktime(&t);
			continue;
		}

		if (sscanf(buf, "Minimum recovery ending location:     %X/%X",
				&ControlFile.minRecoveryPoint.xlogid,
				&ControlFile.minRecoveryPoint.xrecoff) == 2)
			continue;

		if (sscanf(buf, "Maximum data alignment:               %u",
				&ControlFile.maxAlign) == 1)
			continue;
		if (sscanf(buf, "Database block size:                  %u",
				&ControlFile.blcksz) == 1)
			continue;
		if (sscanf(buf, "Blocks per segment of large relation: %u",
				&ControlFile.relseg_size) == 1)
			continue;
		if (sscanf(buf, "WAL block size:                       %u",
				&ControlFile.xlog_blcksz) == 1)
			continue;
		if (sscanf(buf, "Bytes per WAL segment:                %u",
				&ControlFile.xlog_seg_size) == 1)
			continue;
		if (sscanf(buf, "Maximum length of identifiers:        %u",
				&ControlFile.nameDataLen) == 1)
			continue;
		if (sscanf(buf, "Maximum columns in an index:          %u",
				&ControlFile.indexMaxKeys) == 1)
			continue;
		if (sscanf(buf, "Date/time type storage:               " ALL_TRAIL,
				tmp) == 1)
		{
			if (strcmp(tmp, "64-bit integers") == 0)
				ControlFile.enableIntTimes = true;
			else if(strcmp(tmp, "floating-point numbers") == 0)
				ControlFile.enableIntTimes = false;
			continue;
		}
		if (sscanf(buf, "Maximum length of locale name:        %u",
				&ControlFile.localeBuflen) == 1)
			continue;
		if (sscanf(buf, "LC_COLLATE:                           " ALL_TRAIL,
				ControlFile.lc_collate) == 1)
			continue;
		if (sscanf(buf, "LC_CTYPE:                             " ALL_TRAIL,
				ControlFile.lc_ctype) == 1)
			continue;
	}
	fclose(fp);

	/* recalcualte the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, &ControlFile, offsetof(ControlFileData, crc));
	FIN_CRC32C(crc);
	ControlFile.crc = crc;

	if ((fd = open(ControlFilePath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
				progname, "pg_control", strerror(errno));
		exit(2);
	}
	if (write(fd, &ControlFile, sizeof(OldControlFileData)) !=
			sizeof(OldControlFileData))
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;

		printf(_("\n\nFATAL ERROR\n"
				 "could not write to control file: %s\n"),
				 strerror(errno));
		close(fd);
		exit(1);
	}
	if (close(fd))
	{
		printf(_("\n\nFATAL ERROR\n"
				 "could not close control file: %s\n"),
			   strerror(errno));
		exit(1);
	}

	return 0;
}

/*
 * ModifyControlFile --
 * This is the workhorse of control file modification.  downgrade = true if
 * this is from newer to older.
 */
static int
ModifyControlFile(const char *progname, char *ControlFilePath, bool downgrade)
{
	CommonControlFile FromControlFile;
	CommonControlFile ToControlFile;

	size_t sizeOfFromControlFileData;
	size_t sizeOfToControlFileData;
	size_t offsetOfCRCFrom;
	size_t offsetOfCRCTo;
	int		FromDBStateShutdowned;
	int		ToDBStateShutdowned;
	uint32	To_pg_control_version;
	uint32	From_pg_control_version;
	uint32	To_catalog_version_no;

	int			fd;
	pg_crc32	crc, crcvalue;

	if (downgrade)
	{
		sizeOfFromControlFileData = sizeof(NewControlFileData);
		sizeOfToControlFileData = sizeof(OldControlFileData);
		offsetOfCRCFrom = offsetof(NewControlFileData, crc);
		offsetOfCRCTo = offsetof(OldControlFileData, crc);
		FromDBStateShutdowned = DB_SHUTDOWNED;
		ToDBStateShutdowned = OLD_DB_SHUTDOWNED;
		To_pg_control_version = OLD_PG_CONTROL_VERSION;
		From_pg_control_version = PG_CONTROL_VERSION;
		To_catalog_version_no = OLD_CATALOG_VERSION_NO;
	}
	else
	{
		sizeOfFromControlFileData = sizeof(OldControlFileData);
		sizeOfToControlFileData = sizeof(NewControlFileData);
		offsetOfCRCFrom = offsetof(OldControlFileData, crc);
		offsetOfCRCTo = offsetof(NewControlFileData, crc);
		FromDBStateShutdowned = OLD_DB_SHUTDOWNED;
		ToDBStateShutdowned = DB_SHUTDOWNED;
		To_pg_control_version = PG_CONTROL_VERSION;
		From_pg_control_version = OLD_PG_CONTROL_VERSION;
		To_catalog_version_no = CATALOG_VERSION_NO;
	}


#define FLAGS (O_RDWR | O_EXCL | PG_BINARY)
	if ((fd = open(ControlFilePath, FLAGS, S_IRUSR | S_IWUSR)) == -1)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, ControlFilePath, strerror(errno));
		exit(2);
	}

	/*
	 * read the file content.
	 */
	if (read(fd, &FromControlFile, sizeOfFromControlFileData) !=
			sizeOfFromControlFileData)
	{
		fprintf(stderr, _("%s: could not read file \"%s\": %s\n"),
				progname, ControlFilePath, strerror(errno));
		close(fd);
		exit(2);
	}

	/*
	 * Check the version to modify from.  We should do this before the
	 * CRC check, as offset to CRC might be different.
	 */
	if (FromControlFile.common.pg_control_version != From_pg_control_version)
	{
		if (FromControlFile.common.pg_control_version == To_pg_control_version)
		{
			/* nothing to do */
			close(fd);
			exit(0);
		}
		fprintf(stderr, _("%s: unexpected control file version: %d\n"),
				progname, FromControlFile.common.pg_control_version);
		close(fd);
		exit(2);
	}

	/* Check the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, &FromControlFile, offsetOfCRCFrom);
	FIN_CRC32C(crc);

	if (downgrade)
		crcvalue = FromControlFile.newer.crc;
	else
		crcvalue = FromControlFile.older.crc;

	if (!EQ_LEGACY_CRC32(crc, crcvalue))
	{
		/* Check the CRC using old algorithm. */
		INIT_LEGACY_CRC32(crc);
		COMP_LEGACY_CRC32(crc, (char *) &FromControlFile, offsetOfCRCFrom);
		FIN_LEGACY_CRC32(crc);

		if (!EQ_LEGACY_CRC32(crc, crcvalue))
			printf(_("WARNING: Calculated CRC checksum does not match value stored in file.\n"
					 "Either the file is corrupt, or it has a different layout than this program\n"
					 "is expecting.  The results below are untrustworthy.\n\n"));
	}

	if (FromControlFile.common.state != FromDBStateShutdowned)
	{
		const char	   *state_str;

		if (downgrade)
			state_str = dbState(FromControlFile.newer.state);
		else
			state_str = dbStateOld(FromControlFile.older.state);

		/* only upgrade shutdown systems */
		printf(_("\n\nFATAL ERROR\n\n"
				 "Invalid database state for upgrade: %s\n"
				 "Action: shutdown the database and try again.\n"),
			   state_str);

		close(fd);
		exit(1);
	}

	printf(_("\nCatalog version number: %u\n"),
		   FromControlFile.common.catalog_version_no);

#define ASSIGN_FIELD(field) \
	do { \
		if (downgrade) \
			ToControlFile.older.field = FromControlFile.newer.field; \
		else \
			ToControlFile.newer.field = FromControlFile.older.field; \
	} while (0)

	/* Copy compatible values */
	ASSIGN_FIELD(system_identifier);
	ToControlFile.common.pg_control_version = To_pg_control_version;
	ToControlFile.common.catalog_version_no = To_catalog_version_no;
	/* We have checked it was shutdowned. */
	ToControlFile.common.state = ToDBStateShutdowned;
	ASSIGN_FIELD(time);
	ASSIGN_FIELD(logId);
	ASSIGN_FIELD(logSeg);
	ASSIGN_FIELD(checkPoint);
	ASSIGN_FIELD(prevCheckPoint);
	ASSIGN_FIELD(checkPointCopy);
	/*
	 * In 8220430, we bumped the initial log from 0 to 1.  See BootStrapXLOG.
	 */
	if (!downgrade &&
		ToControlFile.newer.logId == 0 && ToControlFile.newer.logSeg == 0)
	{
		NewControlFileData *tofile = (NewControlFileData *) &ToControlFile;
		tofile->logId = 0;
		tofile->logSeg = 1;
		tofile->checkPoint.xlogid = 0;
		tofile->checkPoint.xrecoff = XLogSegSize + SizeOfXLogLongPHD;
		tofile->checkPointCopy.redo = tofile->checkPoint;
		tofile->checkPointCopy.undo = tofile->checkPoint;
	}
	ASSIGN_FIELD(minRecoveryPoint);
	if (!downgrade)
	{
		NewControlFileData *tofile = (NewControlFileData *) &ToControlFile;
		tofile->backupStartPoint.xlogid = 0;
		tofile->backupStartPoint.xrecoff = 0;
		tofile->backupEndRequired = false;
	}
	ASSIGN_FIELD(maxAlign);
	ASSIGN_FIELD(floatFormat);
	ASSIGN_FIELD(blcksz);
	ASSIGN_FIELD(relseg_size);
	ASSIGN_FIELD(xlog_blcksz);
	ASSIGN_FIELD(xlog_seg_size);
	ASSIGN_FIELD(nameDataLen);
	ASSIGN_FIELD(indexMaxKeys);
	ASSIGN_FIELD(enableIntTimes);
	ASSIGN_FIELD(localeBuflen);
	if (downgrade)
	{
		memcpy(&ToControlFile.older.lc_collate[0],
			   &FromControlFile.newer.lc_collate[0],
			   LOCALE_NAME_BUFLEN);
		memcpy(&ToControlFile.older.lc_ctype[0],
			   &FromControlFile.newer.lc_ctype[0],
			   LOCALE_NAME_BUFLEN);
	}
	else
	{
		memcpy(&ToControlFile.newer.lc_collate[0],
			   &FromControlFile.older.lc_collate[0],
			   LOCALE_NAME_BUFLEN);
		memcpy(&ToControlFile.newer.lc_ctype[0],
			   &FromControlFile.older.lc_ctype[0],
			   LOCALE_NAME_BUFLEN);
	}

	/* recalcualte the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, &ToControlFile, offsetOfCRCTo);
	FIN_CRC32C(crc);
	if (downgrade)
		ToControlFile.older.crc = crc;
	else
		ToControlFile.newer.crc = crc;

	printf(_("From pg_control version number: %u\n"),
		   FromControlFile.common.pg_control_version);

	printf(_("Setting version number to: %u\n"),
		   ToControlFile.common.pg_control_version);

	errno = 0;
	if (lseek(fd, 0, SEEK_SET) < 0)
	{
		printf(_("\n\nFATAL ERROR\n"
				 "could not seek control file: %s\n"),
				 strerror(errno));
		close(fd);
		exit(1);

	}
	if (write(fd, &ToControlFile, sizeOfToControlFileData) !=
			sizeOfToControlFileData)
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;

		printf(_("\n\nFATAL ERROR\n"
				 "could not write to control file: %s\n"),
				 strerror(errno));
		close(fd);
		exit(1);
	}

	if (close(fd))
	{
		printf(_("\n\nFATAL ERROR\n"
				 "could not close control file: %s\n"),
			   strerror(errno));
		exit(1);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	char		ControlFilePath[MAXPGPATH];
	char	   *DataDir;
	const char *progname;
	int			argno = 1;
	char	   *create_old_source = NULL;
	bool		downgrade = false;

	set_pglocale_pgservice(argv[0], "gpmodcontrol");

	progname = get_progname(argv[0]);

	if (argc > 1)
	{
		argno = 1;

		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("gpmodontrol (Greenplum Database) " PG_VERSION);
			exit(0);
		}
		if (strcmp(argv[1], "--create-old") == 0)
		{
			if (argc > 2)
			{
				create_old_source = argv[2];
				argno = 3;
			}
			else
			{
				fprintf(stderr, "%s: missing source file argument\n", progname);
				exit(2);
			}
		}
		if (strcmp(argv[1], "--downgrade") == 0)
		{
			downgrade = true;
			argno++;
		}
	}

	if (argc > argno)
		DataDir = argv[argno];
	else
		DataDir = getenv("PGDATA");

	if (DataDir == NULL)
	{
		fprintf(stderr, _("%s: no data directory specified\n"), progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		usage(progname);
		exit(1);
	}

	snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", DataDir);

	if (create_old_source != NULL)
		return CreateOldControlFile(progname, ControlFilePath, create_old_source);
	else
		return ModifyControlFile(progname, ControlFilePath, downgrade);
}
