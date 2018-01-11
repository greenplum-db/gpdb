/*-------------------------------------------------------------------------
 *
 * basebackup.c
 *	  code for taking a base backup and streaming it to a standby
 *
 * Portions Copyright (c) 2010-2012, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/basebackup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "miscadmin.h"
#include "access/genam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"		/* for pg_start/stop_backup */
#include "cdb/cdbvars.h"
#include "catalog/catalog.h"
#include "catalog/pg_database.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_filespace_entry.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "replication/basebackup.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "utils/fmgroids.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"

typedef struct
{
	const char *label;
	bool		progress;
	bool		fastcheckpoint;
	bool		nowait;
	bool		includewal;
	List	   *exclude;
} basebackup_options;


static bool match_exclude_list(char *path, List *exclude);
static int64 sendDir(char *path, int basepathlen, List *exclude, bool sizeonly);
static void sendFile(char *readfilename, char *tarfilename,
		 struct stat * statbuf);
static void sendFileWithContent(const char *filename, const char *content);
static void _tarWriteHeader(const char *filename, const char *linktarget,
				struct stat * statbuf);
static void send_int8_string(StringInfoData *buf, int64 intval);
static void SendBackupHeader(List *tablespaces);
static void base_backup_cleanup(int code, Datum arg);
static void perform_base_backup(basebackup_options *opt, DIR *tblspcdir);
static void parse_basebackup_options(List *options, basebackup_options *opt);
static void SendXlogRecPtrResult(XLogRecPtr ptr);
static List *get_filespaces_to_send(basebackup_options *opt);

/*
 * Size of each block sent into the tar stream for larger files.
 *
 * XLogSegSize *MUST* be evenly dividable by this
 */
#define TAR_SEND_SIZE 32768

typedef struct
{
	char	   *oid;
	char	   *path;
	int64		size;
} tablespaceinfo;

typedef struct
{
	char	   *oid;
	char	   *primary_path;
	char	   *standby_path;
	int64		size;
	bool		xlogdir;
} filespaceinfo;


/*
 * Called when ERROR or FATAL happens in perform_base_backup() after
 * we have started the backup - make sure we end it!
 */
static void
base_backup_cleanup(int code, Datum arg)
{
	do_pg_abort_backup();
}

/*
 * Actually do a base backup for the specified tablespaces.
 *
 * This is split out mainly to avoid complaints about "variable might be
 * clobbered by longjmp" from stupider versions of gcc.
 */
static void
perform_base_backup(basebackup_options *opt, DIR *tblspcdir)
{
	XLogRecPtr	startptr;
	XLogRecPtr	endptr;
	char	   *labelfile;

	startptr = do_pg_start_backup(opt->label, opt->fastcheckpoint, &labelfile);
	Assert(!XLogRecPtrIsInvalid(startptr));

	elogif(!debug_basebackup, LOG,
		   "basebackup perform -- "
		   "Basebackup start xlog location = %X/%X",
		   startptr.xlogid, startptr.xrecoff);

	/*
	 * Set xlogCleanUpTo so that checkpoint process knows
	 * which old xlog files should not be cleaned
	 */
	WalSndSetXLogCleanUpTo(startptr);

	SIMPLE_FAULT_INJECTOR(BaseBackupPostCreateCheckpoint);

	SendXlogRecPtrResult(startptr);

	PG_ENSURE_ERROR_CLEANUP(base_backup_cleanup, (Datum) 0);
	{
		List	   *filespaces = NIL;
		ListCell   *lc;

		/* Collect information about all filespaces, including pg_system */
		filespaces = get_filespaces_to_send(opt);

		/* Send filespace header */
		SendBackupHeader(filespaces);

		/* Send off our filespaces one by one */
		foreach(lc, filespaces)
		{
			filespaceinfo *fi = (filespaceinfo *) lfirst(lc);
			StringInfoData buf;

			/* Send CopyOutResponse message */
			pq_beginmessage(&buf, 'H');
			pq_sendbyte(&buf, 0);		/* overall format */
			pq_sendint(&buf, 0, 2);		/* natts */
			pq_endmessage(&buf);

			/* In the main tar, include the backup_label first. */
			if (fi->primary_path == NULL)
				sendFileWithContent(BACKUP_LABEL_FILE, labelfile);

			sendDir(fi->primary_path == NULL ? "." : fi->primary_path,
					fi->primary_path == NULL ? 1 : strlen(fi->primary_path),
					opt->exclude, false);

			/* In the main tar, include pg_control last. */
			if (fi->primary_path == NULL)
			{
				struct stat statbuf;

				if (lstat(XLOG_CONTROL_FILE, &statbuf) != 0)
				{
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not stat control file \"%s\": %m",
									XLOG_CONTROL_FILE)));
				}

				sendFile(XLOG_CONTROL_FILE, XLOG_CONTROL_FILE, &statbuf);

				elogif(debug_basebackup, LOG,
					   "basebackup perform -- Sent file %s." , XLOG_CONTROL_FILE);
			}

			/*
			 * If we're including WAL, and this is the main data directory we
			 * don't terminate the tar stream here. Instead, we will append
			 * the xlog files below and terminate it then. This is safe since
			 * the main data directory is always sent *last*.
			 */
			if (opt->includewal && fi->xlogdir)
			{
				Assert(lnext(lc) == NULL);
			}
			else
				pq_putemptymessage('c');		/* CopyDone */
		}
	}
	PG_END_ENSURE_ERROR_CLEANUP(base_backup_cleanup, (Datum) 0);

	endptr = do_pg_stop_backup(labelfile);

	if (opt->includewal)
	{
		/*
		 * We've left the last tar file "open", so we can now append the
		 * required WAL files to it.
		 */
		uint32		logid,
					logseg;
		uint32		endlogid,
					endlogseg;
		struct stat statbuf;

		MemSet(&statbuf, 0, sizeof(statbuf));
		statbuf.st_mode = S_IRUSR | S_IWUSR;
#ifndef WIN32
		statbuf.st_uid = geteuid();
		statbuf.st_gid = getegid();
#endif
		statbuf.st_size = XLogSegSize;
		statbuf.st_mtime = time(NULL);

		XLByteToSeg(startptr, logid, logseg);
		XLByteToPrevSeg(endptr, endlogid, endlogseg);

		while (true)
		{
			/* Send another xlog segment */
			char		tempfn[MAXPGPATH], fn[MAXPGPATH];
			int			i;

			/*
			 * XLogFilePath may produce full path as well as relative path.
			 * Here we are sendin this as part of the last tar file,
			 * so make sure it is relative path.
			 */
			XLogFileName(tempfn, ThisTimeLineID, logid, logseg);
			if (snprintf(fn, MAXPGPATH, "pg_xlog/%s", tempfn) > MAXPGPATH)
				elog(ERROR, "could not make xlog filename for %s", tempfn);

			_tarWriteHeader(fn, NULL, &statbuf);

			/* Send the actual WAL file contents, block-by-block */
			for (i = 0; i < XLogSegSize / TAR_SEND_SIZE; i++)
			{
				char		buf[TAR_SEND_SIZE];
				XLogRecPtr	ptr;

				ptr.xlogid = logid;
				ptr.xrecoff = logseg * XLogSegSize + TAR_SEND_SIZE * i;

				/*
				 * Some old compilers, e.g. gcc 2.95.3/x86, think that passing
				 * a struct in the same function as a longjump might clobber a
				 * variable.  bjm 2011-02-04
				 * http://lists.apple.com/archives/xcode-users/2003/Dec//msg000
				 * 51.html
				 */
				XLogRead(buf, ptr, TAR_SEND_SIZE);
				if (pq_putmessage('d', buf, TAR_SEND_SIZE))
					ereport(ERROR,
							(errmsg("base backup could not send data, aborting backup")));
			}

			elogif(debug_basebackup, LOG,
				   "basebackup perform -- Sent xlog file %s", fn);

			/*
			 * Files are always fixed size, and always end on a 512 byte
			 * boundary, so padding is never necessary.
			 */
			/* Advance to the next WAL file */
			NextLogSeg(logid, logseg);

			/* Have we reached our stop position yet? */
			if (logid > endlogid ||
				(logid == endlogid && logseg > endlogseg))
				break;
		}

		/* Send CopyDone message for the last tar file */
		pq_putemptymessage('c');
	}
	SendXlogRecPtrResult(endptr);
}

/*
 * Parse the base backup options passed down by the parser
 */
static void
parse_basebackup_options(List *options, basebackup_options *opt)
{
	ListCell   *lopt;
	bool		o_label = false;
	bool		o_progress = false;
	bool		o_fast = false;
	bool		o_nowait = false;
	bool		o_wal = false;

	MemSet(opt, 0, sizeof(*opt));
	opt->exclude = NIL;
	foreach(lopt, options)
	{
		DefElem    *defel = (DefElem *) lfirst(lopt);

		if (strcmp(defel->defname, "label") == 0)
		{
			if (o_label)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("duplicate option \"%s\"", defel->defname)));
			opt->label = strVal(defel->arg);
			o_label = true;
		}
		else if (strcmp(defel->defname, "progress") == 0)
		{
			if (o_progress)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("duplicate option \"%s\"", defel->defname)));
			opt->progress = true;
			o_progress = true;
		}
		else if (strcmp(defel->defname, "fast") == 0)
		{
			if (o_fast)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("duplicate option \"%s\"", defel->defname)));
			opt->fastcheckpoint = true;
			o_fast = true;
		}
		else if (strcmp(defel->defname, "nowait") == 0)
		{
			if (o_nowait)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("duplicate option \"%s\"", defel->defname)));
			opt->nowait = true;
			o_nowait = true;
		}
		else if (strcmp(defel->defname, "wal") == 0)
		{
			if (o_wal)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("duplicate option \"%s\"", defel->defname)));
			opt->includewal = true;
			o_wal = true;
		}
		else if (strcmp(defel->defname, "exclude") == 0)
		{
			/* EXCLUDE option can be specified multiple times */
			opt->exclude = lappend(opt->exclude, defel->arg);
		}
		else
			elog(ERROR, "option \"%s\" not recognized",
				 defel->defname);
	}
	if (opt->label == NULL)
		opt->label = "base backup";

	elogif(debug_basebackup, LOG,
			"basebackup options -- "
			"label = %s, "
			"progress = %s, "
			"fastcheckpoint = %s, "
			"nowait = %s, "
			"wal = %s",
			opt->label,
			opt->progress ? "true" : "false",
			opt->fastcheckpoint ? "true" : "false",
			opt->nowait ? "true" : "false",
			opt->includewal ? "true" : "false");
}


/*
 * SendBaseBackup() - send a complete base backup.
 *
 * The function will put the system into backup mode like pg_start_backup()
 * does, so that the backup is consistent even though we read directly from
 * the filesystem, bypassing the buffer cache.
 */
void
SendBaseBackup(BaseBackupCmd *cmd)
{
	DIR		   *dir;
	basebackup_options opt;

	parse_basebackup_options(cmd->options, &opt);

	WalSndSetState(WALSNDSTATE_BACKUP);

	if (update_process_title)
	{
		char		activitymsg[50];

		snprintf(activitymsg, sizeof(activitymsg), "sending backup \"%s\"",
				 opt.label);
		set_ps_display(activitymsg, false);
	}

	/*
	 * XXX: we should be able to take backup without connecting to database.
	 * Connect to template database to fetch rows from pg_filespace_entry
	 * in perform_base_backup.  While base backup is supposed to be finished
	 * only for the time basebackup application is connecting, which may
	 * continue the connection until the next WAL streaming, but we need
	 * to revisit this to read the filespace information without touching
	 * database.  Anyway the filespace is master-only and we will reconsider
	 * the design again when it comes to segment WAL replication.
	 */
	char *fullpath;
	StartTransactionCommand();
	(void) GetTransactionSnapshot();

	MyDatabaseId = TemplateDbOid;
	MyDatabaseTableSpace = DEFAULTTABLESPACE_OID;
	MyProc->databaseId = MyDatabaseId;
	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);
	SetDatabasePath(fullpath);
	RelationCacheInitializePhase3();

	LockSharedObject(DatabaseRelationId, MyDatabaseId, 0, RowExclusiveLock);

	/* Make sure we can open the directory with tablespaces in it */
	/* NB: This is not the case in GPDB, but safe to leave. */
	dir = AllocateDir("pg_tblspc");
	if (!dir)
		ereport(ERROR,
				(errmsg("could not open directory \"%s\": %m", "pg_tblspc")));

	perform_base_backup(&opt, dir);

	FreeDir(dir);

	CommitTransactionCommand();

}

static void
send_int8_string(StringInfoData *buf, int64 intval)
{
	char		is[32];

	sprintf(is, INT64_FORMAT, intval);
	pq_sendint(buf, strlen(is), 4);
	pq_sendbytes(buf, is, strlen(is));
}

static void
SendBackupHeader(List *filespaces)
{
	StringInfoData buf;
	ListCell   *lc;

	/* Construct and send the directory information */
	pq_beginmessage(&buf, 'T'); /* RowDescription */
	pq_sendint(&buf, 4, 2);		/* 4 fields */

	/* First field - fsefsoid */
	pq_sendstring(&buf, "fsefsoid");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, 0, 2);		/* attnum */
	pq_sendint(&buf, OIDOID, 4);	/* type oid */
	pq_sendint(&buf, 4, 2);		/* typlen */
	pq_sendint(&buf, 0, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	/* Second field - fselocationprimary */
	pq_sendstring(&buf, "fselocationprimary");
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);
	pq_sendint(&buf, TEXTOID, 4);
	pq_sendint(&buf, -1, 2);
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);

	/* Third field - fselocationstandby */
	pq_sendstring(&buf, "fselocationstandby");
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);
	pq_sendint(&buf, TEXTOID, 4);
	pq_sendint(&buf, -1, 2);
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);

	/* Fourth field - size */
	pq_sendstring(&buf, "size");
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);
	pq_sendint(&buf, INT8OID, 4);
	pq_sendint(&buf, 8, 2);
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);
	pq_endmessage(&buf);

	foreach(lc, filespaces)
	{
		filespaceinfo  *fi = lfirst(lc);

		/* Send one datarow message */
		pq_beginmessage(&buf, 'D');
		pq_sendint(&buf, 4, 2); /* number of columns */
		if (fi->primary_path == NULL)
		{
			/* base directory.  standby_path should be NULL */
			Assert(fi->standby_path == NULL);
			pq_sendint(&buf, -1, 4);	/* Length = -1 ==> NULL */
			pq_sendint(&buf, -1, 4);
			pq_sendint(&buf, -1, 4);
		}
		else
		{
			char	   *standby_path;

			pq_sendint(&buf, strlen(fi->oid), 4);		/* length */
			pq_sendbytes(&buf, fi->oid, strlen(fi->oid));
			pq_sendint(&buf, strlen(fi->primary_path), 4);		/* length */
			pq_sendbytes(&buf, fi->primary_path, strlen(fi->primary_path));
			/*
			 * If standby is not configured but we have user filespace,
			 * just send the original path...  pg_basebackup will fail
			 * in case of extract mode in the same host, but it is
			 * the same behaivor as postgres.
			 */
			if (fi->standby_path == NULL)
				standby_path = fi->primary_path;
			else
				standby_path = fi->standby_path;
			pq_sendint(&buf, strlen(standby_path), 4);		/* length */
			pq_sendbytes(&buf, standby_path, strlen(standby_path));
		}
		if (fi->size >= 0)
			send_int8_string(&buf, fi->size / 1024);
		else
			pq_sendint(&buf, -1, 4);	/* NULL */

		pq_endmessage(&buf);
	}

	/* Send a CommandComplete message */
	pq_puttextmessage('C', "SELECT");

	elogif(debug_basebackup, LOG, "basebackup header -- Sent basebackup header.");
}

/*
 * Send a single resultset containing just a single
 * XlogRecPtr record (in text format)
 */
static void
SendXlogRecPtrResult(XLogRecPtr ptr)
{
	StringInfoData buf;
	char		str[MAXFNAMELEN];

	snprintf(str, sizeof(str), "%X/%X", ptr.xlogid, ptr.xrecoff);

	pq_beginmessage(&buf, 'T'); /* RowDescription */
	pq_sendint(&buf, 1, 2);		/* 1 field */

	/* Field header */
	pq_sendstring(&buf, "recptr");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, 0, 2);		/* attnum */
	pq_sendint(&buf, TEXTOID, 4);		/* type oid */
	pq_sendint(&buf, -1, 2);
	pq_sendint(&buf, 0, 4);
	pq_sendint(&buf, 0, 2);
	pq_endmessage(&buf);

	/* Data row */
	pq_beginmessage(&buf, 'D');
	pq_sendint(&buf, 1, 2);		/* number of columns */
	pq_sendint(&buf, strlen(str), 4);	/* length */
	pq_sendbytes(&buf, str, strlen(str));
	pq_endmessage(&buf);

	/* Send a CommandComplete message */
	pq_puttextmessage('C', "SELECT");
}

/*
 * Inject a file with given name and content in the output tar stream.
 */
static void
sendFileWithContent(const char *filename, const char *content)
{
	struct stat statbuf;
	int			pad,
				len;

	len = strlen(content);

	/*
	 * Construct a stat struct for the backup_label file we're injecting in
	 * the tar.
	 */
	/* Windows doesn't have the concept of uid and gid */
#ifdef WIN32
	statbuf.st_uid = 0;
	statbuf.st_gid = 0;
#else
	statbuf.st_uid = geteuid();
	statbuf.st_gid = getegid();
#endif
	statbuf.st_mtime = time(NULL);
	statbuf.st_mode = S_IRUSR | S_IWUSR;
	statbuf.st_size = len;

	_tarWriteHeader(filename, NULL, &statbuf);
	/* Send the contents as a CopyData message */
	pq_putmessage('d', content, len);

	/* Pad to 512 byte boundary, per tar format requirements */
	pad = ((len + 511) & ~511) - len;
	if (pad > 0)
	{
		char		buf[512];

		MemSet(buf, 0, pad);
		pq_putmessage('d', buf, pad);
	}

	elogif(debug_basebackup, LOG,
			"basebackup send file -- Sent file '%s' with content \n%s.",
			filename, content);
}

/*
 * Check if client EXCLUDE option matches this path.  Current implementation
 * is only the exact match for the relative path from a filespace root (e.g.
 * "./pg_log" etc).
 */
static bool
match_exclude_list(char *path, List *exclude)
{
	ListCell	   *l;

	/* fast check */
	if (exclude == NIL)
		return false;

	foreach (l, exclude)
	{
		char	   *val = strVal(lfirst(l));

		if (strcmp(val, path) == 0)
			return true;
	}

	return false;
}

/*
 * Include all files from the given directory in the output tar stream. If
 * 'sizeonly' is true, we just calculate a total length and return it, without
 * actually sending anything.
 */
static int64
sendDir(char *path, int basepathlen, List *exclude, bool sizeonly)
{
	DIR		   *dir;
	struct dirent *de;
	char		pathbuf[MAXPGPATH];
	struct stat statbuf;
	int64		size = 0;

	dir = AllocateDir(path);
	while ((de = ReadDir(dir, path)) != NULL)
	{
		/* Skip special stuff */
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		/* Skip temporary files */
		if (strncmp(de->d_name,
					PG_TEMP_FILE_PREFIX,
					strlen(PG_TEMP_FILE_PREFIX)) == 0)
			continue;

		/*
		 * If there's a backup_label file, it belongs to a backup started by
		 * the user with pg_start_backup(). It is *not* correct for this
		 * backup, our backup_label is injected into the tar separately.
		 */
		if (strcmp(de->d_name, BACKUP_LABEL_FILE) == 0)
			continue;

		/*
		 * Check if the postmaster has signaled us to exit, and abort with an
		 * error in that case. The error handler further up will call
		 * do_pg_abort_backup() for us.
		 */
		if (ProcDiePending || walsender_ready_to_stop)
			ereport(ERROR,
				(errmsg("shutdown requested, aborting active base backup")));

		snprintf(pathbuf, MAXPGPATH, "%s/%s", path, de->d_name);

		/* Skip postmaster.pid and postmaster.opts in the data directory */
		if (strcmp(pathbuf, "./postmaster.pid") == 0 ||
			strcmp(pathbuf, "./postmaster.opts") == 0)
			continue;

		/* Skip pg_control here to back up it last */
		if (strcmp(pathbuf, "./global/pg_control") == 0)
			continue;

		if (lstat(pathbuf, &statbuf) != 0)
		{
			if (errno != ENOENT)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file or directory \"%s\": %m",
								pathbuf)));

			/* If the file went away while scanning, it's no error. */
			continue;
		}

		/*
		 * We can skip pg_xlog, the WAL segments need to be fetched from the
		 * WAL archive anyway. But include it as an empty directory anyway, so
		 * we get permissions right.
		 * The directory can be in a user filespace if we moved it with
		 * gpfilespace --movetransfilespace.
		 */
		if (strcmp(de->d_name, "pg_xlog") == 0)
		{
			if (!sizeonly)
			{
				/* If pg_xlog is a symlink, write it as a directory anyway */
#ifndef WIN32
				if (S_ISLNK(statbuf.st_mode))
#else
				if (pgwin32_is_junction(pathbuf))
#endif
					statbuf.st_mode = S_IFDIR | S_IRWXU;
				_tarWriteHeader(pathbuf + basepathlen + 1, NULL, &statbuf);
			}
			size += 512;		/* Size of the header just added */
			continue;			/* don't recurse into pg_xlog */
		}

		/* Skip if client does not want */
		if (match_exclude_list(pathbuf, exclude))
			continue;

		/* Allow symbolic links in pg_tblspc only */
		/* NB: This is not the case in GPDB, but safe to leave. */
		if (strcmp(path, "./pg_tblspc") == 0 &&
#ifndef WIN32
			S_ISLNK(statbuf.st_mode)
#else
			pgwin32_is_junction(pathbuf)
#endif
			)
		{
#if defined(HAVE_READLINK) || defined(WIN32)
			char		linkpath[MAXPGPATH];
			int			rllen;

			rllen = readlink(pathbuf, linkpath, sizeof(linkpath));
			if (rllen < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read symbolic link \"%s\": %m",
								pathbuf)));
			if (rllen >= sizeof(linkpath))
				ereport(ERROR,
						(errmsg("symbolic link \"%s\" target is too long",
								pathbuf)));
			linkpath[rllen] = '\0';

			if (!sizeonly)
				_tarWriteHeader(pathbuf + basepathlen + 1, linkpath, &statbuf);
			size += 512;		/* Size of the header just added */
#else

			/*
			 * If the platform does not have symbolic links, it should not be
			 * possible to have tablespaces - clearly somebody else created
			 * them. Warn about it and ignore.
			 */
			ereport(WARNING,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				  errmsg("tablespaces are not supported on this platform")));
			continue;
#endif   /* HAVE_READLINK */
		}
		else if (S_ISDIR(statbuf.st_mode))
		{
			/*
			 * Store a directory entry in the tar file so we can get the
			 * permissions right.
			 */
			if (!sizeonly)
				_tarWriteHeader(pathbuf + basepathlen + 1, NULL, &statbuf);
			size += 512;		/* Size of the header just added */

			/* call ourselves recursively for a directory */
			size += sendDir(pathbuf, basepathlen, exclude, sizeonly);
		}
		else if (S_ISREG(statbuf.st_mode))
		{
			/* Add size, rounded up to 512byte block */
			size += ((statbuf.st_size + 511) & ~511);
			if (!sizeonly)
				sendFile(pathbuf, pathbuf + basepathlen + 1, &statbuf);
			size += 512;		/* Size of the header of the file */
		}
		else
			ereport(WARNING,
					(errmsg("skipping special file \"%s\"", pathbuf)));
	}
	FreeDir(dir);

	elogif(debug_basebackup && !sizeonly, LOG,
			"baseabckup send dir -- Sent directory %s", path);

	return size;
}

/*****
 * Functions for handling tar file format
 *
 * Copied from pg_dump, but modified to work with libpq for sending
 */


/*
 * Utility routine to print possibly larger than 32 bit integers in a
 * portable fashion.  Filled with zeros.
 */
static void
print_val(char *s, uint64 val, unsigned int base, size_t len)
{
	int			i;

	for (i = len; i > 0; i--)
	{
		int			digit = val % base;

		s[i - 1] = '0' + digit;
		val = val / base;
	}
}

/*
 * Maximum file size for a tar member: The limit inherent in the
 * format is 2^33-1 bytes (nearly 8 GB).  But we don't want to exceed
 * what we can represent in pgoff_t.
 */
#define MAX_TAR_MEMBER_FILELEN (((int64) 1 << Min(33, sizeof(pgoff_t)*8 - 1)) - 1)

static int
_tarChecksum(char *header)
{
	int			i,
				sum;

	sum = 0;
	for (i = 0; i < 512; i++)
		if (i < 148 || i >= 156)
			sum += 0xFF & header[i];
	return sum + 256;			/* Assume 8 blanks in checksum field */
}

/* Given the member, write the TAR header & send the file */
static void
sendFile(char *readfilename, char *tarfilename, struct stat * statbuf)
{
	FILE	   *fp;
	char		buf[TAR_SEND_SIZE];
	size_t		cnt;
	pgoff_t		len = 0;
	size_t		pad;

	fp = AllocateFile(readfilename, "rb");
	if (fp == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m", readfilename)));

	/*
	 * Some compilers will throw a warning knowing this test can never be true
	 * because pgoff_t can't exceed the compared maximum on their platform.
	 */
	if (statbuf->st_size > MAX_TAR_MEMBER_FILELEN)
		ereport(ERROR,
				(errmsg("archive member \"%s\" too large for tar format",
						tarfilename)));

	_tarWriteHeader(tarfilename, NULL, statbuf);

	while ((cnt = fread(buf, 1, Min(sizeof(buf), statbuf->st_size - len), fp)) > 0)
	{
		/* Send the chunk as a CopyData message */
		if (pq_putmessage('d', buf, cnt))
			ereport(ERROR,
			   (errmsg("base backup could not send data, aborting backup")));

		len += cnt;

		if (len >= statbuf->st_size)
		{
			/*
			 * Reached end of file. The file could be longer, if it was
			 * extended while we were sending it, but for a base backup we can
			 * ignore such extended data. It will be restored from WAL.
			 */
			break;
		}
	}

	/* If the file was truncated while we were sending it, pad it with zeros */
	if (len < statbuf->st_size)
	{
		MemSet(buf, 0, sizeof(buf));
		while (len < statbuf->st_size)
		{
			cnt = Min(sizeof(buf), statbuf->st_size - len);
			pq_putmessage('d', buf, cnt);
			len += cnt;
		}
	}

	/* Pad to 512 byte boundary, per tar format requirements */
	pad = ((len + 511) & ~511) - len;
	if (pad > 0)
	{
		MemSet(buf, 0, pad);
		pq_putmessage('d', buf, pad);
	}

	FreeFile(fp);
}


static void
_tarWriteHeader(const char *filename, const char *linktarget,
				struct stat * statbuf)
{
	char		h[512];
	int			lastSum = 0;
	int			sum;

	memset(h, 0, sizeof(h));

	/* Name 100 */
	sprintf(&h[0], "%.99s", filename);
	if (linktarget != NULL || S_ISDIR(statbuf->st_mode))
	{
		/*
		 * We only support symbolic links to directories, and this is
		 * indicated in the tar format by adding a slash at the end of the
		 * name, the same as for regular directories.
		 */
		h[strlen(filename)] = '/';
		h[strlen(filename) + 1] = '\0';
	}

	/* Mode 8 */
	sprintf(&h[100], "%07o ", (int) statbuf->st_mode);

	/* User ID 8 */
	sprintf(&h[108], "%07o ", statbuf->st_uid);

	/* Group 8 */
	sprintf(&h[117], "%07o ", statbuf->st_gid);

	/* File size 12 - 11 digits, 1 space, no NUL */
	if (linktarget != NULL || S_ISDIR(statbuf->st_mode))
		/* Symbolic link or directory has size zero */
		print_val(&h[124], 0, 8, 11);
	else
		print_val(&h[124], statbuf->st_size, 8, 11);
	sprintf(&h[135], " ");

	/* Mod Time 12 */
	sprintf(&h[136], "%011o ", (int) statbuf->st_mtime);

	/* Checksum 8 */
	sprintf(&h[148], "%06o ", lastSum);

	if (linktarget != NULL)
	{
		/* Type - Symbolic link */
		sprintf(&h[156], "2");
		sprintf(&h[157], "%.99s", linktarget);
	}
	else if (S_ISDIR(statbuf->st_mode))
		/* Type - directory */
		sprintf(&h[156], "5");
	else
		/* Type - regular file */
		sprintf(&h[156], "0");

	/* Link tag 100 (NULL) */

	/* Magic 6 + Version 2 */
	sprintf(&h[257], "ustar00");

	/* User 32 */
	/* XXX: Do we need to care about setting correct username? */
	sprintf(&h[265], "%.31s", "postgres");

	/* Group 32 */
	/* XXX: Do we need to care about setting correct group name? */
	sprintf(&h[297], "%.31s", "postgres");

	/* Maj Dev 8 */
	sprintf(&h[329], "%6o ", 0);

	/* Min Dev 8 */
	sprintf(&h[337], "%6o ", 0);

	while ((sum = _tarChecksum(h)) != lastSum)
	{
		sprintf(&h[148], "%06o ", sum);
		lastSum = sum;
	}

	pq_putmessage('d', h, 512);
}

/*
 * Collect all information of filespaces to be sent.
 */
static List *
get_filespaces_to_send(basebackup_options *opt)
{
	List		   *filespaces = NIL;
	Oid				txnFilespaceOID;
	Relation		rel;
	ScanKeyData		scankey;
	SysScanDesc		sscan;
	HeapTuple		tuple;
	int16			standbyDbid;
	filespaceinfo  *xlog_fi = NULL;

	/*
	 * GpIdentity.dbid can be -1 in utility mode, but we don't support it.
	 */
	if (GpIdentity.dbid == -1)
		elog(ERROR, "basebackup is not supported with dbid -1");

	/*
	 * There is no pg_filespace_entry table on the segment, so currently we
	 * just send the default data directory for segments. GPDB_SEGWALREP_TODO:
	 * enhance to support transaction and user filespaces for segments.
	 */
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
	{
		filespaceinfo *fi = palloc0(sizeof(filespaceinfo));

		fi->primary_path = NULL;
		fi->standby_path = NULL;
		fi->size = -1;
		fi->xlogdir = true;

		filespaces = lappend(filespaces, fi);
		return filespaces;
	}

	txnFilespaceOID = primaryMirrorGetTxnFilespaceOID();
	/*
	 * Scan the filespace entries for this db.
	 */
	standbyDbid = GetStandbyDbid();

	rel = heap_open(FileSpaceEntryRelationId, AccessShareLock);

	/* SELECT * FROM pg_filespace_entry WHERE fsedbid = :1" */
	ScanKeyInit(&scankey,
				Anum_pg_filespace_entry_fsedbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(GpIdentity.dbid));

	sscan = systable_beginscan(rel, InvalidOid, false,
							   SnapshotNow, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Form_pg_filespace_entry pg_filespace_entry;
		bool			isnull;
		filespaceinfo  *fi;
		char		   *fselocation;
		char			*primary_fselocation = NULL;
		char			*standby_fselocation = NULL;

		pg_filespace_entry = (Form_pg_filespace_entry) GETSTRUCT(tuple);
		fselocation = TextDatumGetCString(
			heap_getattr(tuple,
						 Anum_pg_filespace_entry_fselocation,
						 RelationGetDescr(rel),
						 &isnull));

		Assert(strlen(fselocation) <= MAXPGPATH);
		Assert(!isnull);

		/* Save for logging purpose */
		primary_fselocation = fselocation;

		fi = palloc0(sizeof(filespaceinfo));
		fi->oid = DatumGetCString(DirectFunctionCall1(oidout,
						ObjectIdGetDatum(pg_filespace_entry->fsefsoid)));
		if (pg_filespace_entry->fsefsoid == SYSTEMFILESPACE_OID)
			fi->primary_path = NULL;
		else
			fi->primary_path = pstrdup(fselocation);

		/*
		 * If standby is configured, get its entry and attach it.
		 * For base directory, it is just a sanity check.
		 */
		if (OidIsValid(standbyDbid))
		{
			ScanKeyData		standby_scankey[2];
			SysScanDesc		standby_sscan;
			HeapTuple	standby_tuple;

			ScanKeyInit(&standby_scankey[0],
						Anum_pg_filespace_entry_fsefsoid,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(pg_filespace_entry->fsefsoid));
			ScanKeyInit(&standby_scankey[1],
						Anum_pg_filespace_entry_fsedbid,
						BTEqualStrategyNumber, F_INT2EQ,
						Int16GetDatum(standbyDbid));

			standby_sscan = systable_beginscan(rel, FileSpaceEntryFsefsoidFsedbidIndexId, true,
											   SnapshotNow, 2, standby_scankey);
			standby_tuple = systable_getnext(standby_sscan);
			if (!HeapTupleIsValid(standby_tuple))
				elog(ERROR, "cache lookup failed for filespace %u",
							pg_filespace_entry->fsefsoid);

			fselocation = TextDatumGetCString(
				heap_getattr(standby_tuple,
							 Anum_pg_filespace_entry_fselocation,
							 RelationGetDescr(rel),
							 &isnull));
			Assert(strlen(fselocation) <= MAXPGPATH);
			Assert(!isnull);

			/* Save for logging purpose */
			standby_fselocation = fselocation;

			/*
			 * For base directory, standby side is also set NULL.
			 */
			if (fi->primary_path == NULL)
				fi->standby_path = NULL;
			else
				fi->standby_path = pstrdup(fselocation);

			systable_endscan(standby_sscan);
		}
		fi->size = opt->progress ?
			sendDir(fselocation, strlen(fselocation), opt->exclude, true) : -1;

		if (txnFilespaceOID == pg_filespace_entry->fsefsoid)
			xlog_fi = fi;
		else
			filespaces = lappend(filespaces, fi);

		elogif(debug_basebackup, LOG,
				"basebackup pg_filespace_entry -- "
				"filespace oid = %s, "
				"filespace primary path = %s, "
				"filespace standby path = %s, "
				"filespace size = %lld, "
				"txn filespace = %s.",
				fi->oid,
				primary_fselocation,
				(standby_fselocation) ? standby_fselocation : "NA",
				(long long int)fi->size,
				(txnFilespaceOID == pg_filespace_entry->fsefsoid) ? "true" : "false");
	}

	/*
	 * Add the directory for pg_xlog (which could be different from basedir
	 * in case we configured txn filespace) last so that we can include xlogs
	 * after pg_stop_basebackup.
	 */
	Assert(xlog_fi != NULL);
	xlog_fi->xlogdir = true;
	filespaces = lappend(filespaces, xlog_fi);

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);

	return filespaces;
}
