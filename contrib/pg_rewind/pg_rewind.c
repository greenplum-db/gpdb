/*-------------------------------------------------------------------------
 *
 * pg_rewind.c
 *	  Synchronizes an old master server to a new timeline
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 2013-2014 VMware, Inc. All Rights Reserved.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include "pg_rewind.h"
#include "fetch.h"
#include "filemap.h"

#include "access/timeline.h"
#include "access/xlog_internal.h"
#include "catalog/catversion.h"
#include "catalog/pg_control.h"
#include "storage/bufpage.h"

#include "getopt_long.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *progname);

static void createBackupLabel(XLogRecPtr startpoint, TimeLineID starttli,
				  XLogRecPtr checkpointloc);

static void digestControlFile(ControlFileData *ControlFile, char *source, size_t size);
static void updateControlFile(ControlFileData *ControlFile,
							  char *datadir);
static void syncTargetDirectory(const char *argv0);
static void sanityChecks(void);
static void findCommonAncestorTimeline(XLogRecPtr *recptr, TimeLineID *tli);

static ControlFileData ControlFile_target;
static ControlFileData ControlFile_source;

const char *progname;

char *datadir_target = NULL;
char *datadir_source = NULL;
char *connstr_source = NULL;

int verbose;
int dry_run;

static void
usage(const char *progname)
{
	printf("%s resynchronizes a cluster with another copy of the cluster.\n\n", progname);
	printf("Usage:\n  %s [OPTION]...\n\n", progname);
	printf("Options:\n");
	printf("  -D, --target-pgdata=DIRECTORY\n");
	printf("                 existing data directory to modify\n");
	printf("  --source-pgdata=DIRECTORY\n");
	printf("                 source data directory to sync with\n");
	printf("  --source-server=CONNSTR\n");
	printf("                 source server to sync with\n");
	printf("  -v, --verbose  write a lot of progress messages\n");
	printf("  -n, --dry-run  stop before modifying anything\n");
	printf("  -V, --version  output version information, then exit\n");
	printf("  -?, --help     show this help, then exit\n");
	printf("\n");
	printf("Report bugs to https://github.com/vmware/pg_rewind.\n");
}


int
main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"help", no_argument, NULL, '?'},
		{"target-pgdata", required_argument, NULL, 'D'},
		{"source-pgdata", required_argument, NULL, 1},
		{"source-server", required_argument, NULL, 2},
		{"version", no_argument, NULL, 'V'},
		{"dry-run", no_argument, NULL, 'n'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
	int			option_index;
	int			c;
	XLogRecPtr	divergerec;
	TimeLineID	lastcommontli;
	XLogRecPtr	chkptrec;
	TimeLineID	chkpttli;
	XLogRecPtr	chkptredo;
	size_t		size;
	char	   *buffer;
	bool		rewind_needed;
	XLogRecPtr	endrec;
	TimeLineID	endtli;
	ControlFileData	ControlFile_new;

	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_rewind"));
	progname = get_progname(argv[0]);

	/* Set default parameter values */
	verbose = 0;
	dry_run = 0;

	/* Process command-line arguments */
	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_rewind " PG_REWIND_VERSION);
			exit(0);
		}
	}

	while ((c = getopt_long(argc, argv, "D:vn", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case '?':
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
			case ':':
				exit(1);
			case 'v':
				verbose = 1;
				break;
			case 'n':
				dry_run = 1;
				break;

			case 'D':	/* -D or --target-pgdata */
				datadir_target = pg_strdup(optarg);
				break;

			case 1:		/* --source-pgdata */
				datadir_source = pg_strdup(optarg);
				break;
			case 2:		/* --source-server */
				connstr_source = pg_strdup(optarg);
				break;
		}
	}

	/* No source given? Show usage */
	if (datadir_source == NULL && connstr_source == NULL)
	{
		fprintf(stderr, "%s: no source specified\n", progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	if (datadir_source != NULL && connstr_source != NULL)
	{
		fprintf(stderr, _("%s: only one of --source-pgdata or --source-server can be specified\n"), progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	if (datadir_target == NULL)
	{
		fprintf(stderr, "%s: no target data directory specified\n", progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	if (argc != optind)
	{
		fprintf(stderr, "%s: invalid arguments\n", progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	/*
	 * Don't allow pg_rewind to be run as root, to avoid overwriting the
	 * ownership of files in the data directory. We need only check for root
	 * -- any other user won't have sufficient permissions to modify files in
	 * the data directory.
	 */
#ifndef WIN32
	if (geteuid() == 0)
	{
		fprintf(stderr, "cannot be executed by \"root\"\n"
				"You must run %s as the PostgreSQL superuser.\n",
				progname);
		exit(1);
	}
#endif

	/*
	 * Connect to remote server
	 */
	if (connstr_source)
		libpqConnect(connstr_source);

	/*
	 * Ok, we have all the options and we're ready to start. Read in all the
	 * information we need from both clusters.
	 */
	buffer = slurpFile(datadir_target, "global/pg_control", &size);
	digestControlFile(&ControlFile_target, buffer, size);
	pg_free(buffer);

	buffer = fetchFile("global/pg_control", &size);
	digestControlFile(&ControlFile_source, buffer, size);
	pg_free(buffer);

	sanityChecks();

	/*
	 * If both clusters are already on the same timeline, there's nothing
	 * to do.
	 */
	if (ControlFile_target.checkPointCopy.ThisTimeLineID == ControlFile_source.checkPointCopy.ThisTimeLineID)
	{
		fprintf(stderr, "source and target cluster are both on the same timeline.\n");
		printf("No rewind required.\n");
		exit(0);
	}

	findCommonAncestorTimeline(&divergerec, &lastcommontli);
	printf("The servers diverged at WAL position %X/%X on timeline %u.\n",
		   (uint32) (divergerec >> 32), (uint32) divergerec, lastcommontli);

	/*
	 * Check for the possibility that the target is in fact a direct ancestor
	 * of the source. In that case, there is no divergent history in the
	 * target that needs rewinding.
	 */
	if (ControlFile_target.checkPoint >= divergerec)
	{
		rewind_needed = true;
	}
	else
	{
		XLogRecPtr chkptendrec;

		/* Read the checkpoint record on the target to see where it ends. */
		chkptendrec = readOneRecord(datadir_target,
									ControlFile_target.checkPoint,
									ControlFile_target.checkPointCopy.ThisTimeLineID);

		/*
		 * If the histories diverged exactly at the end of the shutdown
		 * checkpoint record on the target, there are no WAL records in the
		 * target that don't belong in the source's history, and no rewind is
		 * needed.
		 */
		if (chkptendrec == divergerec)
			rewind_needed = false;
		else
			rewind_needed = true;
	}

	if (!rewind_needed)
	{
		printf("No rewind required.\n");
		exit(0);
	}
	findLastCheckpoint(datadir_target, divergerec, lastcommontli,
					   &chkptrec, &chkpttli, &chkptredo);
	printf("Rewinding from Last common checkpoint at %X/%X on timeline %u\n",
		   (uint32) (chkptrec >> 32), (uint32) chkptrec,
		   chkpttli);

	/*
	 * Install necessary functions of pg_rewind_support before fetching
	 * the remote file list if a running source server is used for the
	 * rewind operation.
	 */
	if (connstr_source)
		libpqInitSupport();

	/*
	 * Build the filemap, by comparing the remote and local data directories
	 */
	(void) filemap_create();
	fetchRemoteFileList();
	traverse_datadir(datadir_target, &process_local_file);

	/*
	 * Read the target WAL from last checkpoint before the point of fork, to
	 * extract all the pages that were modified on the target cluster after
	 * the fork. We can stop reading after reaching the final shutdown record.
	 * XXX: If we supported rewinding a server that was not shut down cleanly,
	 * we would need to replay until the end of WAL here.
	 */
	extractPageMap(datadir_target, chkptrec, lastcommontli,
				   ControlFile_target.checkPoint);
	filemap_finalize();

	/* XXX: this is probably too verbose even in verbose mode */
	if (verbose)
		print_filemap();

	/* Ok, we're ready to start copying things over. */
	executeFileMap();

	createBackupLabel(chkptredo, chkpttli, chkptrec);

	printf("creating backup label and updating control file\n");

	/*
	 * Update control file of target. Make it ready to perform archive
	 * recovery when restarting.
	 *
	 * minRecoveryPoint is set to the current WAL insert location in the
	 * source server. Like in an online backup, it's important that we recover
	 * all the WAL that was generated while we copied the files over.
	 */
	memcpy(&ControlFile_new, &ControlFile_source, sizeof(ControlFileData));

	if (connstr_source)
	{
		endrec = libpqGetCurrentXlogInsertLocation();
		endtli = ControlFile_source.checkPointCopy.ThisTimeLineID;
	}
	else
	{
		endrec = ControlFile_source.checkPoint;
		endtli = ControlFile_source.checkPointCopy.ThisTimeLineID;
	}
	ControlFile_new.minRecoveryPoint = endrec;
	ControlFile_new.minRecoveryPointTLI = endtli;
	ControlFile_new.state = DB_IN_ARCHIVE_RECOVERY;
	updateControlFile(&ControlFile_new, datadir_target);

	syncTargetDirectory(argv[0]);

	printf("Done!\n");

	/*
	 * At this point we are basically completely done, so remove the support
	 * bundle from source server if needed.
	 */
	if (connstr_source)
		libpqFinishSupport();
	return 0;
}

static void
sanityChecks(void)
{
	/* Check that there's no backup_label in either cluster */
	/* Check system_id match */
	if (ControlFile_target.system_identifier != ControlFile_source.system_identifier)
	{
		fprintf(stderr, "source and target clusters are from different systems\n");
		exit(1);
	}
	/* check version */
	if (ControlFile_target.pg_control_version != PG_CONTROL_VERSION ||
		ControlFile_source.pg_control_version != PG_CONTROL_VERSION ||
		ControlFile_target.catalog_version_no != CATALOG_VERSION_NO ||
		ControlFile_source.catalog_version_no != CATALOG_VERSION_NO)
	{
		fprintf(stderr, "clusters are not compatible with this version of pg_rewind\n");
		exit(1);
	}

	/*
	 * Target cluster need to use checksums or hint bit wal-logging, this to
	 * prevent from data corruption that could occur because of hint bits.
	 */
	if (ControlFile_target.data_checksum_version != PG_DATA_CHECKSUM_VERSION &&
		!ControlFile_target.wal_log_hints)
	{
		fprintf(stderr, "target master need to use either data checksums or \"wal_log_hints = on\".\n");
		exit(1);
	}

	/*
	 * Target cluster better not be running. This doesn't guard against someone
	 * starting the cluster concurrently. Also, this is probably more strict
	 * than necessary; it's OK if the master was not shut down cleanly, as
	 * long as it isn't running at the moment.
	 */
	if (ControlFile_target.state != DB_SHUTDOWNED &&
		ControlFile_target.state != DB_SHUTDOWNED_IN_RECOVERY)
	{
		fprintf(stderr, "target master must be shut down cleanly.\n");
		exit(1);
	}

    /*
     * When the source is a data directory, also require that the source
     * server is shut down. There isn't any very strong reason for this
     * limitation, but better safe than sorry.
     */
	if (datadir_source &&
		ControlFile_source.state != DB_SHUTDOWNED &&
		ControlFile_source.state != DB_SHUTDOWNED_IN_RECOVERY)
	{
		fprintf(stderr, "source data directory must be shut down cleanly.\n");
		exit(1);
	}
}

/*
 * Determine the TLI of the last common timeline in the histories of the two
 * clusters. *tli is set to the last common timeline, and *recptr is set to
 * the position where the histories diverged (ie. the first WAL record that's
 * not the same in both clusters).
 *
 * Control files of both clusters must be read into ControlFile_target/source
 * before calling this.
 */
static void
findCommonAncestorTimeline(XLogRecPtr *recptr, TimeLineID *tli)
{
	TimeLineID	targettli;
	TimeLineHistoryEntry *sourceHistory;
	int			nentries;
	int			i;
	TimeLineID	sourcetli;

	targettli = ControlFile_target.checkPointCopy.ThisTimeLineID;
	sourcetli = ControlFile_source.checkPointCopy.ThisTimeLineID;

	/* Timeline 1 does not have a history file, so no need to check */
	if (sourcetli == 1)
	{
		sourceHistory = (TimeLineHistoryEntry *) pg_malloc(sizeof(TimeLineHistoryEntry));
		sourceHistory->tli = sourcetli;
		sourceHistory->begin = sourceHistory->end = InvalidXLogRecPtr;
		nentries = 1;
	}
	else
	{
		char		path[MAXPGPATH];
		char	   *histfile;

		TLHistoryFilePath(path, sourcetli);
		histfile = fetchFile(path, NULL);

		sourceHistory = rewind_parseTimeLineHistory(histfile,
													ControlFile_source.checkPointCopy.ThisTimeLineID,
													&nentries);
		pg_free(histfile);
	}

	/*
	 * Trace the history backwards, until we hit the target timeline.
	 *
	 * TODO: This assumes that there are no timeline switches on the target
	 * cluster after the fork.
	 */
	for (i = nentries - 1; i >= 0; i--)
	{
		TimeLineHistoryEntry *entry = &sourceHistory[i];
		if (entry->tli == targettli)
		{
			/* found it */
			*recptr = entry->end;
			*tli = entry->tli;

			free(sourceHistory);
			return;
		}
	}

	fprintf(stderr, "could not find common ancestor of the source and target cluster's timelines\n");
	exit(1);
}


/*
 * Create a backup_label file that forces recovery to begin at the last common
 * checkpoint.
 */
static void
createBackupLabel(XLogRecPtr startpoint, TimeLineID starttli, XLogRecPtr checkpointloc)
{
	XLogSegNo	startsegno;
	char		BackupLabelFilePath[MAXPGPATH];
	FILE	   *fp;
	time_t		stamp_time;
	char		strfbuf[128];
	char		xlogfilename[MAXFNAMELEN];
	struct tm  *tmp;

	if (dry_run)
		return;

	XLByteToSeg(startpoint, startsegno);
	XLogFileName(xlogfilename, starttli, startsegno);

	/*
	 * TODO: move old file out of the way, if any. And perhaps create the
	 * file with temporary name first and rename in place after it's done.
	 */
	snprintf(BackupLabelFilePath, MAXPGPATH,
			 "%s/backup_label" /* BACKUP_LABEL_FILE */, datadir_target);

	/*
	 * Construct backup label file
	 */

	fp = fopen(BackupLabelFilePath, "wb");

	stamp_time = time(NULL);
	tmp = localtime(&stamp_time);
	strftime(strfbuf, sizeof(strfbuf), "%Y-%m-%d %H:%M:%S %Z", tmp);
	fprintf(fp, "START WAL LOCATION: %X/%X (file %s)\n",
			(uint32) (startpoint >> 32), (uint32) startpoint, xlogfilename);
	fprintf(fp, "CHECKPOINT LOCATION: %X/%X\n",
			(uint32) (checkpointloc >> 32), (uint32) checkpointloc);
	fprintf(fp, "BACKUP METHOD: pg_rewind\n");
	fprintf(fp, "BACKUP FROM: standby\n");
	fprintf(fp, "START TIME: %s\n", strfbuf);
	/* omit LABEL: line */

	if (fclose(fp) != 0)
	{
		fprintf(stderr, _("could not write backup label file \"%s\": %s\n"),
				BackupLabelFilePath, strerror(errno));
		exit(2);
	}
}

/*
 * Check CRC of control file
 */
static void
checkControlFile(ControlFileData *ControlFile)
{
	pg_crc32	crc;

	/* Calculate CRC */
	INIT_CRC32(crc);
	COMP_CRC32(crc,
			   (char *) ControlFile,
			   offsetof(ControlFileData, crc));
	FIN_CRC32(crc);

	/* And simply compare it */
	if (!EQ_CRC32(crc, ControlFile->crc))
	{
		fprintf(stderr, "unexpected control file CRC\n");
		exit(1);
	}
}

/*
 * Verify control file contents in the buffer src, and copy it to *ControlFile.
 */
static void
digestControlFile(ControlFileData *ControlFile, char *src, size_t size)
{
	if (size != PG_CONTROL_SIZE)
	{
		fprintf(stderr, "unexpected control file size %d, expected %d\n",
				(int) size, PG_CONTROL_SIZE);
		exit(1);
	}
	memcpy(ControlFile, src, sizeof(ControlFileData));

	/* Additional checks on control file */
	checkControlFile(ControlFile);
}

/*
 * Update a control file with fresh content
 */
static void
updateControlFile(ControlFileData *ControlFile, char *datadir)
{
	char    path[MAXPGPATH];
	char	buffer[PG_CONTROL_SIZE];
	FILE    *fp;

	if (dry_run)
		return;

	/* Recalculate CRC of control file */
	INIT_CRC32(ControlFile->crc);
	COMP_CRC32(ControlFile->crc,
			   (char *) ControlFile,
			   offsetof(ControlFileData, crc));
	FIN_CRC32(ControlFile->crc);

	/*
	 * Write out PG_CONTROL_SIZE bytes into pg_control by zero-padding
	 * the excess over sizeof(ControlFileData) to avoid premature EOF
	 * related errors when reading it.
	 */
	memset(buffer, 0, PG_CONTROL_SIZE);
	memcpy(buffer, ControlFile, sizeof(ControlFileData));

	snprintf(path, MAXPGPATH,
			 "%s/global/pg_control", datadir);

	if ((fp  = fopen(path, "wb")) == NULL)
	{
		fprintf(stderr,"Could not open the pg_control file\n");
		exit(1);
	}

	if (fwrite(buffer, 1,
			   PG_CONTROL_SIZE, fp) != PG_CONTROL_SIZE)
	{
		fprintf(stderr,"Could not write the pg_control file\n");
		exit(1);
	}

	if (fclose(fp))
	{
		fprintf(stderr,"Could not close the pg_control file\n");
		exit(1);
	}
}

/*
 * Sync data directory to ensure that what has been generated up to now is
 * persistent in case of a crash, and this is done once globally for
 * performance reasons as sync requests on individual files would be
 * a negative impact on the running time of pg_rewind. This is invoked at
 * the end of processing once everything has been processed and written.
 */
static void
syncTargetDirectory(const char *argv0)
{
	int		ret;
	char	exec_path[MAXPGPATH];
	char	cmd[MAXPGPATH];

	if (dry_run)
		return;

	if (verbose)
		fprintf(stderr, "syncing target data directory via initdb -S\n");

	/* Grab and invoke initdb to perform the sync */
	if ((ret = find_other_exec(argv0, "initdb",
							   "initdb (PostgreSQL) " PG_VERSION "\n",
							   exec_path)) < 0)
	{
		char	full_path[MAXPGPATH];

		if (find_my_exec(argv0, full_path) < 0)
			strlcpy(full_path, progname, sizeof(full_path));

		if (ret == -1)
			fprintf(stderr, "The program \"initdb\" is needed by %s but was \n"
					"not found in the same directory as \"%s\".\n"
					"Check your installation.\n", progname, full_path);
		else
			fprintf(stderr, "The program \"postgres\" was found by \"%s\" \n"
					"but was not the same version as %s.\n"
					"Check your installation.\n", progname, full_path);
		exit(1);
	}

	/* now run initdb */
	if (verbose)
		snprintf(cmd, MAXPGPATH, "\"%s\" -D \"%s\" -S",
				 exec_path, datadir_target);
	else
		snprintf(cmd, MAXPGPATH, "\"%s\" -D \"%s\" -S > \"%s\"",
				 exec_path, datadir_target, DEVNULL);

	if (system(cmd) != 0)
	{
		fprintf(stderr, "sync of target directory with initdb -S failed\n");
		exit(1);
	}

	if (verbose)
		fprintf(stderr, "sync of target directory with initdb -S done\n");
}
