/*-------------------------------------------------------------------------
 *
 * postinit.c
 *	  postgres initialization utilities
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/init/postinit.c,v 1.191 2009/06/11 14:49:05 momjian Exp $
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_tablespace.h"
#include "catalog/indexing.h"
#include "libpq/auth.h"
#include "libpq/hba.h"
#include "libpq/libpq-be.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbutil.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/fts.h"
#include "postmaster/postmaster.h"
#include "replication/walsender.h"
#include "storage/backendid.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/backend_cancel.h"
#include "utils/flatfiles.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/plancache.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/relcache.h"
#include "utils/resscheduler.h"
#include "utils/sharedsnapshot.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#include "utils/session_state.h"
#include "codegen/codegen_wrapper.h"


static HeapTuple GetDatabaseTuple(const char *dbname);
static HeapTuple GetDatabaseTupleByOid(Oid dboid);
static void PerformAuthentication(Port *port);
static void CheckMyDatabase(const char *name, bool am_superuser);
static void ProcessRoleGUC(void);
static void InitCommunication(void);
static void ShutdownPostgres(int code, Datum arg);
static bool ThereIsAtLeastOneRole(void);
static void process_startup_options(Port *port, bool am_superuser);

#ifdef USE_ORCA
extern void InitGPOPT();
extern void TerminateGPOPT();
#endif


/*** InitPostgres support ***/

/*
 * FindMyDatabase
 *
 * Get oids of the database and default tablespace by the database name.
 * Returns true if succeeded.
 *
 * XXX: This shouldn't be necessary, but for now it's here for fts, etc.
 */
bool
FindMyDatabase(const char *dbname, Oid *db_id, Oid *db_tablespace)
{
	HeapTuple		tuple;
	Form_pg_database dbform;

	tuple = GetDatabaseTuple(dbname);

	if (!HeapTupleIsValid(tuple))
		return false;

	/* Return oids to the caller */
	dbform = (Form_pg_database) GETSTRUCT(tuple);
	*db_id = HeapTupleGetOid(tuple);
	*db_tablespace = dbform->dattablespace;

	/* Be tidy */
	pfree(tuple);

	return true;
}

/*
 * GetDatabaseTuple -- fetch the pg_database row for a database
 *
 * This is used during backend startup when we don't yet have any access to
 * system catalogs in general.	In the worst case, we can seqscan pg_database
 * using nothing but the hard-wired descriptor that relcache.c creates for
 * pg_database.  In more typical cases, relcache.c was able to load
 * descriptors for both pg_database and its indexes from the shared relcache
 * cache file, and so we can do an indexscan.  criticalSharedRelcachesBuilt
 * tells whether we got the cached descriptors.
 */
static HeapTuple
GetDatabaseTuple(const char *dbname)
{
	HeapTuple	tuple;
	Relation	relation;
	SysScanDesc scan;
	ScanKeyData key[1];

	/*
	 * form a scan key
	 */
	ScanKeyInit(&key[0],
				Anum_pg_database_datname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(dbname));

	/*
	 * Open pg_database and fetch a tuple.	Force heap scan if we haven't yet
	 * built the critical shared relcache entries (i.e., we're starting up
	 * without a shared relcache cache file).
	 */
	relation = heap_open(DatabaseRelationId, AccessShareLock);
	scan = systable_beginscan(relation, DatabaseNameIndexId,
							  criticalSharedRelcachesBuilt,
							  SnapshotNow,
							  1, key);

	tuple = systable_getnext(scan);

	/* Must copy tuple before releasing buffer */
	if (HeapTupleIsValid(tuple))
		tuple = heap_copytuple(tuple);

	/* all done */
	systable_endscan(scan);
	heap_close(relation, AccessShareLock);

	return tuple;
}

/*
 * GetDatabaseTupleByOid -- as above, but search by database OID
 */
static HeapTuple
GetDatabaseTupleByOid(Oid dboid)
{
	HeapTuple	tuple;
	Relation	relation;
	SysScanDesc scan;
	ScanKeyData key[1];

	/*
	 * form a scan key
	 */
	ScanKeyInit(&key[0],
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(dboid));

	/*
	 * Open pg_database and fetch a tuple.	Force heap scan if we haven't yet
	 * built the critical shared relcache entries (i.e., we're starting up
	 * without a shared relcache cache file).
	 */
	relation = heap_open(DatabaseRelationId, AccessShareLock);
	scan = systable_beginscan(relation, DatabaseOidIndexId,
							  criticalSharedRelcachesBuilt,
							  SnapshotNow,
							  1, key);

	tuple = systable_getnext(scan);

	/* Must copy tuple before releasing buffer */
	if (HeapTupleIsValid(tuple))
		tuple = heap_copytuple(tuple);

	/* all done */
	systable_endscan(scan);
	heap_close(relation, AccessShareLock);

	return tuple;
}


/*
 * PerformAuthentication -- authenticate a remote client
 *
 * returns: nothing.  Will not return at all if there's any failure.
 */
static void
PerformAuthentication(Port *port)
{
	/* This should be set already, but let's make sure */
	ClientAuthInProgress = true;	/* limit visibility of log messages */

	/*
	 * In EXEC_BACKEND case, we didn't inherit the contents of pg_hba.conf
	 * etcetera from the postmaster, and have to load them ourselves.  Note we
	 * are loading them into the startup transaction's memory context, not
	 * PostmasterContext, but that shouldn't matter.
	 *
	 * FIXME: [fork/exec] Ugh.	Is there a way around this overhead?
	 */
#ifdef EXEC_BACKEND
	if (!load_hba())
	{
		/*
		 * It makes no sense to continue if we fail to load the HBA file,
		 * since there is no way to connect to the database in this case.
		 */
		ereport(FATAL,
				(errmsg("could not load pg_hba.conf")));
	}
	load_ident();
#endif

	/*
	 * Set up a timeout in case a buggy or malicious client fails to respond
	 * during authentication.  Since we're inside a transaction and might do
	 * database access, we have to use the statement_timeout infrastructure.
	 */
	if (!enable_sig_alarm(AuthenticationTimeout * 1000, true))
		elog(FATAL, "could not set timer for authorization timeout");

	/*
	 * Now perform authentication exchange.
	 */
	ClientAuthentication(port); /* might not return, if failure */

	/*
	 * Done with authentication.  Disable the timeout, and log if needed.
	 */
	if (!disable_sig_alarm(true))
		elog(FATAL, "could not disable timer for authorization timeout");

	if (Log_connections)
	{
		if (am_walsender)
			ereport(LOG,
					(errmsg("replication connection authorized: user=%s",
							port->user_name)));
		else
			ereport(LOG,
					(errmsg("connection authorized: user=%s database=%s",
							port->user_name, port->database_name)));
	}

	set_ps_display("startup", false);

	ClientAuthInProgress = false;		/* client_min_messages is active now */
}


/*
 * ProcessRoleGUC --
 * We now process pg_authid.rolconfig separately from InitializeSessionUserId,
 * since it's too early to access toast table before initializing all
 * relcaches in phase3.
 */
static void
ProcessRoleGUC(void)
{
	Oid			roleId;
	HeapTuple	roleTup;
	Datum		datum;
	bool		isnull;

	/* This should have been set by now */
	roleId = GetUserId();
	Assert(OidIsValid(roleId));

	roleTup = SearchSysCache1(AUTHOID,
							  ObjectIdGetDatum(roleId));
	if (!HeapTupleIsValid(roleTup))
		ereport(FATAL,
				(errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
				 errmsg("role %u does not exist", roleId), errSendAlert(false)));

	/*
	 * Set up user-specific configuration variables.  This is a good place to
	 * do it so we don't have to read pg_authid twice during session startup.
	 */
	datum = SysCacheGetAttr(AUTHOID, roleTup,
							Anum_pg_authid_rolconfig, &isnull);
	if (!isnull)
	{
		ArrayType  *a = DatumGetArrayTypeP(datum);

		/*
		 * We process all the options at SUSET level.  We assume that the
		 * right to insert an option into pg_authid was checked when it was
		 * inserted.
		 */
		ProcessGUCArray(a, PGC_SUSET, PGC_S_USER, GUC_ACTION_SET);
	}

	ReleaseSysCache(roleTup);
}

/*
 * CheckMyDatabase -- fetch information from the pg_database entry for our DB
 */
static void
CheckMyDatabase(const char *name, bool am_superuser)
{
	HeapTuple	tup;
	Form_pg_database dbform;
	char	   *collate;
	char	   *ctype;

	/* Fetch our pg_database row normally, via syscache */
	tup = SearchSysCache(DATABASEOID,
						 ObjectIdGetDatum(MyDatabaseId),
						 0, 0, 0);
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for database %u", MyDatabaseId);
	dbform = (Form_pg_database) GETSTRUCT(tup);

	/* This recheck is strictly paranoia */
	if (strcmp(name, NameStr(dbform->datname)) != 0)
		ereport(FATAL,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database \"%s\" has disappeared from pg_database",
						name),
				 errdetail("Database OID %u now seems to belong to \"%s\".",
						   MyDatabaseId, NameStr(dbform->datname))));

	/*
	 * Check permissions to connect to the database.
	 *
	 * These checks are not enforced when in standalone mode, so that there is
	 * a way to recover from disabling all access to all databases, for
	 * example "UPDATE pg_database SET datallowconn = false;".
	 *
	 * We do not enforce them for the autovacuum worker processes either.
	 */
	if (IsUnderPostmaster && !IsAutoVacuumWorkerProcess())
	{
		/*
		 * Check that the database is currently allowing connections.
		 */
		if (!dbform->datallowconn)
			ereport(FATAL,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
			 errmsg("database \"%s\" is not currently accepting connections",
					name)));

		/*
		 * Check privilege to connect to the database.	(The am_superuser test
		 * is redundant, but since we have the flag, might as well check it
		 * and save a few cycles.)
		 */
		if (!am_superuser &&
			pg_database_aclcheck(MyDatabaseId, GetUserId(),
								 ACL_CONNECT) != ACLCHECK_OK)
			ereport(FATAL,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied for database \"%s\"", name),
					 errdetail("User does not have CONNECT privilege.")));

		/*
		 * Check connection limit for this database.
		 *
		 * There is a race condition here --- we create our PGPROC before
		 * checking for other PGPROCs.	If two backends did this at about the
		 * same time, they might both think they were over the limit, while
		 * ideally one should succeed and one fail.  Getting that to work
		 * exactly seems more trouble than it is worth, however; instead we
		 * just document that the connection limit is approximate.
		 */
		if (dbform->datconnlimit >= 0 &&
			!am_superuser &&
			CountDBBackends(MyDatabaseId) > dbform->datconnlimit)
			ereport(FATAL,
					(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
					 errmsg("too many connections for database \"%s\"",
							name)));
	}

	/*
	 * OK, we're golden.  Next to-do item is to save the encoding info out of
	 * the pg_database tuple.
	 */
	SetDatabaseEncoding(dbform->encoding);
	/* Record it as a GUC internal option, too */
	SetConfigOption("server_encoding", GetDatabaseEncodingName(),
					PGC_INTERNAL, PGC_S_OVERRIDE);
	/* If we have no other source of client_encoding, use server encoding */
	SetConfigOption("client_encoding", GetDatabaseEncodingName(),
					PGC_BACKEND, PGC_S_DEFAULT);

	/* assign locale variables */
	collate = NameStr(dbform->datcollate);
	ctype = NameStr(dbform->datctype);

	if (setlocale(LC_COLLATE, collate) == NULL)
		ereport(FATAL,
			(errmsg("database locale is incompatible with operating system"),
			 errdetail("The database was initialized with LC_COLLATE \"%s\", "
					   " which is not recognized by setlocale().", collate),
			 errhint("Recreate the database with another locale or install the missing locale.")));

	if (setlocale(LC_CTYPE, ctype) == NULL)
		ereport(FATAL,
			(errmsg("database locale is incompatible with operating system"),
			 errdetail("The database was initialized with LC_CTYPE \"%s\", "
					   " which is not recognized by setlocale().", ctype),
			 errhint("Recreate the database with another locale or install the missing locale.")));

	/* Make the locale settings visible as GUC variables, too */
	SetConfigOption("lc_collate", collate, PGC_INTERNAL, PGC_S_OVERRIDE);
	SetConfigOption("lc_ctype", ctype, PGC_INTERNAL, PGC_S_OVERRIDE);

	/* Use the right encoding in translated messages */
#ifdef ENABLE_NLS
	pg_bind_textdomain_codeset(textdomain(NULL));
#endif

	/*
	 * Lastly, set up any database-specific configuration variables.
	 */
	if (IsUnderPostmaster)
	{
		Datum		datum;
		bool		isnull;

		datum = SysCacheGetAttr(DATABASEOID, tup, Anum_pg_database_datconfig,
								&isnull);
		if (!isnull)
		{
			ArrayType  *a = DatumGetArrayTypeP(datum);

			/*
			 * We process all the options at SUSET level.  We assume that the
			 * right to insert an option into pg_database was checked when it
			 * was inserted.
			 */
			ProcessGUCArray(a, PGC_SUSET, PGC_S_DATABASE, GUC_ACTION_SET);
		}
	}

	ReleaseSysCache(tup);
}



/* --------------------------------
 *		InitCommunication
 *
 *		This routine initializes stuff needed for ipc, locking, etc.
 *		it should be called something more informative.
 * --------------------------------
 */
static void
InitCommunication(void)
{
	/*
	 * initialize shared memory and semaphores appropriately.
	 */
	if (!IsUnderPostmaster)		/* postmaster already did this */
	{
		/*
		 * We're running a postgres bootstrap process or a standalone backend.
		 * Create private "shmem" and semaphores.
		 */
		CreateSharedMemoryAndSemaphores(true, 0);
	}
}

/*
 * pg_split_opts -- split a string of options and append it to an argv array
 *
 * The caller is responsible for ensuring the argv array is large enough.  The
 * maximum possible number of arguments added by this routine is
 * (strlen(optstr) + 1) / 2.
 *
 * Because some option values can contain spaces we allow escaping using
 * backslashes, with \\ representing a literal backslash.
 */
void
pg_split_opts(char **argv, int *argcp, char *optstr)
{
	StringInfoData s;

	initStringInfo(&s);

	while (*optstr)
	{
		bool last_was_escape = false;

		resetStringInfo(&s);

		/* skip over leading space */
		while (isspace((unsigned char) *optstr))
			optstr++;

		if (*optstr == '\0')
			break;

		/*
		 * Parse a single option + value, stopping at the first space, unless
		 * it's escaped.
		 */
		while (*optstr)
		{
			if (isspace((unsigned char) *optstr) && !last_was_escape)
				break;

			if (!last_was_escape && *optstr == '\\')
				last_was_escape = true;
			else
			{
				last_was_escape = false;
				appendStringInfoChar(&s, *optstr);
			}

			optstr++;
		}

		/* now store the option */
		argv[(*argcp)++] = pstrdup(s.data);
	}
	resetStringInfo(&s);
}


/*
 * Early initialization of a backend (either standalone or under postmaster).
 * This happens even before InitPostgres.
 *
 * If you're wondering why this is separate from InitPostgres at all:
 * the critical distinction is that this stuff has to happen before we can
 * run XLOG-related initialization, which is done before InitPostgres --- in
 * fact, for cases such as the background writer process, InitPostgres may
 * never be done at all.
 */
void
BaseInit(void)
{
	/*
	 * Attach to shared memory and semaphores, and initialize our
	 * input/output/debugging file descriptors.
	 */
	InitCommunication();
	DebugFileOpen();

	/* Do local initialization of file, storage and buffer managers */
	InitFileAccess();
	smgrinit();
	InitBufferPoolAccess();

	/* Initialize llvm library if USE_CODEGEN is defined */
	init_codegen();
}

/*
 * Make sure we reserve enough connections for FTS handler.
 */
static void check_superuser_connection_limit()
{
	if (!am_ftshandler &&
		!HaveNFreeProcs(RESERVED_FTS_CONNECTIONS))
		ereport(FATAL,
				(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
						errmsg("connection limit exceeded for superusers (need "
									   "at least %d connections reserved for FTS handler)",
							   RESERVED_FTS_CONNECTIONS),
						errSendAlert(true)));
}

/* --------------------------------
 * InitPostgres
 *		Initialize POSTGRES.
 *
 * The database can be specified by name, using the in_dbname parameter, or by
 * OID, using the dboid parameter.	In the latter case, the actual database
 * name can be returned to the caller in out_dbname.  If out_dbname isn't
 * NULL, it must point to a buffer of size NAMEDATALEN.
 *
 * In bootstrap mode no parameters are used.
 *
 * The return value indicates whether the userID is a superuser.  (That
 * can only be tested inside a transaction, so we want to do it during
 * the startup transaction rather than doing a separate one in postgres.c.)
 *
 * As of PostgreSQL 8.2, we expect InitProcess() was already called, so we
 * already have a PGPROC struct ... but it's not filled in yet.
 *
 * Note:
 *		Be very careful with the order of calls in the InitPostgres function.
 * --------------------------------
 */
void
InitPostgres(const char *in_dbname, Oid dboid, const char *username,
			 char *out_dbname)
{
	bool		bootstrap = IsBootstrapProcessingMode();
	bool		autovacuum = IsAutoVacuumWorkerProcess();
	bool		am_superuser;
	char	   *fullpath;
	char		dbname[NAMEDATALEN];

	/*
	 * Add my PGPROC struct to the ProcArray.
	 *
	 * Once I have done this, I am visible to other backends!
	 */
	InitProcessPhase2();

	/* Initialize SessionState entry */
	SessionState_Init();
	/* Initialize memory protection */
	GPMemoryProtect_Init();

#ifdef USE_ORCA
	/* Initialize GPOPT */
	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Optimizer));
	{
		InitGPOPT();
	}
	END_MEMORY_ACCOUNT();
#endif

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	SharedInvalBackendInit(false);

	if (MyBackendId > MaxBackends || MyBackendId <= 0)
		elog(FATAL, "bad backend id: %d", MyBackendId);

	/* Now that we have a BackendId, we can participate in ProcSignal */
	ProcSignalInit(MyBackendId);

	/*
	 * bufmgr needs another initialization call too
	 */
	InitBufferPoolBackend();

	/*
	 * Initialize local process's access to XLOG.  In bootstrap case we may
	 * skip this since StartupXLOG() was run instead.
	 */
	if (!bootstrap)
		InitXLOGAccess();

	/*
	 * Initialize the relation cache and the system catalog caches.  Note that
	 * no catalog access happens here; we only set up the hashtable structure.
	 * We must do this before starting a transaction because transaction abort
	 * would try to touch these hashtables.
	 */
	RelationCacheInitialize();
	InitCatalogCache();
	InitPlanCache();

	/* Initialize portal manager */
	EnablePortalManager();

	/* Initialize stats collection --- must happen before first xact */
	if (!bootstrap)
		pgstat_initialize();

	/*
	 * Load relcache entries for the shared system catalogs.  This must create
	 * at least entries for pg_database and catalogs used for authentication.
	 */
	RelationCacheInitializePhase2();

	/*
	 * Set up process-exit callback to do pre-shutdown cleanup.  This has to
	 * be after we've initialized all the low-level modules like the buffer
	 * manager, because during shutdown this has to run before the low-level
	 * modules start to close down.  On the other hand, we want it in place
	 * before we begin our first transaction --- if we fail during the
	 * initialization transaction, as is entirely possible, we need the
	 * AbortTransaction call to clean up.
	 */
	on_shmem_exit(ShutdownPostgres, 0);

	/* TODO: autovacuum launcher should be done here? */

	/*
	 * Start a new transaction here before first access to db, and get a
	 * snapshot.  We don't have a use for the snapshot itself, but we're
	 * interested in the secondary effect that it sets RecentGlobalXmin.
	 *
	 * Skip these steps if we are responding to a FTS message on mirror.
	 * Mirror operates in standby mode and is not ready to start a
	 * transaction or create a snapshot.  Neither are they required to
	 * respond to a FTS message.
	 */
	if (!bootstrap && !(am_ftshandler && AreWeAMirror))
	{
		StartTransactionCommand();
		(void) GetTransactionSnapshot();
	}

	/*
	 * Figure out our postgres user id, and see if we are a superuser.
	 *
	 * In standalone mode and in the autovacuum process, we use a fixed id,
	 * otherwise we figure it out from the authenticated user name.
	 */
	if (bootstrap || autovacuum)
	{
		InitializeSessionUserIdStandalone();
		am_superuser = true;
	}
	else if (!IsUnderPostmaster)
	{
		InitializeSessionUserIdStandalone();
		am_superuser = true;
		if (!ThereIsAtLeastOneRole())
			ereport(WARNING,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("no roles are defined in this database system"),
					 errhint("You should immediately run CREATE USER \"%s\" CREATEUSER;.",
							 username)));
	}
	else if (am_ftshandler && AreWeAMirror)
	{
		/*
		 * A mirror must receive and act upon FTS messages.  Performing proper
		 * authentication involves reading pg_authid.  Heap access is not
		 * possible on mirror, which is in standby mode.
		 */
		FakeClientAuthentication(MyProcPort);
		InitializeSessionUserIdStandalone();
		am_superuser = true;
	}
	else
	{
		/* normal multiuser case */
		Assert(MyProcPort != NULL);
		PerformAuthentication(MyProcPort);
		InitializeSessionUserId(username);
		am_superuser = superuser();
		BackendCancelInit(MyBackendId);
	}

	/*
	 * Binary upgrades only allowed super-user connections
	 */
	if (IsBinaryUpgrade && !am_superuser)
	{
		ereport(FATAL,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to connect in binary upgrade mode")));
	}

	/*
	 * If we're trying to shut down, only superusers can connect.
	 */
	if (!am_superuser &&
		MyProcPort != NULL &&
		MyProcPort->canAcceptConnections == CAC_WAITBACKUP)
		ereport(FATAL,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to connect during database shutdown")));

	/*
	 * Check a normal user hasn't connected to a superuser reserved slot.
	 */
	if (!am_superuser &&
		ReservedBackends > 0 &&
		!HaveNFreeProcs(ReservedBackends))
		ereport(FATAL,
				(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				 errmsg("connection limit exceeded for non-superusers"),
				 errSendAlert(true)));

	if (am_superuser)
		check_superuser_connection_limit();

	/*
	 * If walsender or fts handler, we don't want to connect to any particular
	 * database. Just finish the backend startup by processing any options from
	 * the startup packet, and we're done.
	 */
	if (am_walsender || am_ftshandler)
	{
		Assert(!bootstrap);

		/*
		 * We don't have replication role, which existed in postgres.
		 */
		if (!am_superuser)
			ereport(FATAL,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser role to start walsender")));

		/* process any options passed in the startup packet */
		if (MyProcPort != NULL)
			process_startup_options(MyProcPort, am_superuser);

		/* Apply PostAuthDelay as soon as we've read all options */
		if (PostAuthDelay > 0)
			pg_usleep(PostAuthDelay * 1000000L);

		/* initialize client encoding */
		InitializeClientEncoding();

		/* report this backend in the PgBackendStatus array */
		pgstat_bestart();

		/* close the transaction we started above */
		if (!(am_ftshandler && AreWeAMirror))
			CommitTransactionCommand();

		return;
	}

	/*
	 * Set up the global variables holding database id and path.  But note we
	 * won't actually try to touch the database just yet.
	 *
	 * We take a shortcut in the bootstrap case, otherwise we have to look up
	 * the db name in pg_database.
	 */
	if (bootstrap)
	{
		MyDatabaseId = TemplateDbOid;
		MyDatabaseTableSpace = DEFAULTTABLESPACE_OID;
	}
	else if (in_dbname != NULL)
	{
		HeapTuple	tuple;
		Form_pg_database dbform;

		tuple = GetDatabaseTuple(in_dbname);
		if (!HeapTupleIsValid(tuple))
			ereport(FATAL,
					(errcode(ERRCODE_UNDEFINED_DATABASE),
					 errmsg("database \"%s\" does not exist", in_dbname)));
		dbform = (Form_pg_database) GETSTRUCT(tuple);
		MyDatabaseId = HeapTupleGetOid(tuple);
		MyDatabaseTableSpace = dbform->dattablespace;
		/* take database name from the caller, just for paranoia */
		strlcpy(dbname, in_dbname, sizeof(dbname));
		pfree(tuple);
	}
	else
	{
		/* caller specified database by OID */
		HeapTuple	tuple;
		Form_pg_database dbform;

		tuple = GetDatabaseTupleByOid(dboid);
		if (!HeapTupleIsValid(tuple))
			ereport(FATAL,
					(errcode(ERRCODE_UNDEFINED_DATABASE),
					 errmsg("database %u does not exist", dboid)));
		dbform = (Form_pg_database) GETSTRUCT(tuple);
		MyDatabaseId = HeapTupleGetOid(tuple);
		MyDatabaseTableSpace = dbform->dattablespace;
		Assert(MyDatabaseId == dboid);
		strlcpy(dbname, NameStr(dbform->datname), sizeof(dbname));
		/* pass the database name back to the caller */
		if (out_dbname)
			strcpy(out_dbname, dbname);
		pfree(tuple);
	}

	/* Now we can mark our PGPROC entry with the database ID */
	/* (We assume this is an atomic store so no lock is needed) */
	MyProc->databaseId = MyDatabaseId;

	/*
	 * Now, take a writer's lock on the database we are trying to connect to.
	 * If there is a concurrently running DROP DATABASE on that database, this
	 * will block us until it finishes (and has committed its update of
	 * pg_database).
	 *
	 * Note that the lock is not held long, only until the end of this startup
	 * transaction.  This is OK since we are already advertising our use of
	 * the database in the PGPROC array; anyone trying a DROP DATABASE after
	 * this point will see us there.
	 *
	 * Note: use of RowExclusiveLock here is reasonable because we envision
	 * our session as being a concurrent writer of the database.  If we had a
	 * way of declaring a session as being guaranteed-read-only, we could use
	 * AccessShareLock for such sessions and thereby not conflict against
	 * CREATE DATABASE.
	 */
	if (!bootstrap)
		LockSharedObject(DatabaseRelationId, MyDatabaseId, 0,
						 RowExclusiveLock);

	/*
	 * Recheck pg_database to make sure the target database hasn't gone away.
	 * If there was a concurrent DROP DATABASE, this ensures we will die
	 * cleanly without creating a mess.
	 */
	if (!bootstrap)
	{
		HeapTuple	tuple;

		tuple = GetDatabaseTuple(dbname);
		if (!HeapTupleIsValid(tuple) ||
			MyDatabaseId != HeapTupleGetOid(tuple) ||
			MyDatabaseTableSpace != ((Form_pg_database) GETSTRUCT(tuple))->dattablespace)
			ereport(FATAL,
					(errcode(ERRCODE_UNDEFINED_DATABASE),
					 errmsg("database \"%s\" does not exist", dbname),
			   errdetail("It seems to have just been dropped or renamed.")));
	}

	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

	if (!bootstrap)
	{
		if (access(fullpath, F_OK) == -1)
		{
			if (errno == ENOENT)
				ereport(FATAL,
						(errcode(ERRCODE_UNDEFINED_DATABASE),
						 errmsg("database \"%s\" does not exist",
								dbname),
					errdetail("The database subdirectory \"%s\" is missing.",
							  fullpath)));
			else
				ereport(FATAL,
						(errcode_for_file_access(),
						 errmsg("could not access directory \"%s\": %m",
								fullpath)));
		}

		ValidatePgVersion(fullpath);
	}

	SetDatabasePath(fullpath);

	/*
	 * It's now possible to do real access to the system catalogs.
	 *
	 * Load relcache entries for the system catalogs.  This must create at
	 * least the minimum set of "nailed-in" cache entries.
	 */
	RelationCacheInitializePhase3();

	/*
	 * Now we have full access to catalog including toast tables,
	 * we can process pg_authid.rolconfig.  This ought to come before
	 * processing startup options so that it can override the settings.
	 */
	if (!bootstrap)
		ProcessRoleGUC();

	/* set up ACL framework (so CheckMyDatabase can check permissions) */
	initialize_acl();

	/*
	 * Re-read the pg_database row for our database, check permissions and set
	 * up database-specific GUC settings.  We can't do this until all the
	 * database-access infrastructure is up.  (Also, it wants to know if the
	 * user is a superuser, so the above stuff has to happen first.)
	 */
	if (!bootstrap)
		CheckMyDatabase(dbname, am_superuser);

	/*
	 * Now process any command-line switches and any additional GUC variable
	 * settings passed in the startup packet.	We couldn't do this before
	 * because we didn't know if client is a superuser.
	 */
	if (MyProcPort != NULL)
		process_startup_options(MyProcPort, am_superuser);

	/*
	 * Maintenance Mode: allow superuser to connect when
	 * gp_maintenance_conn GUC is set.  We cannot check it until
	 * process_startup_options parses the GUC.
	 */
	if (gp_maintenance_mode && Gp_role == GP_ROLE_DISPATCH &&
		!(am_superuser && gp_maintenance_conn))
		ereport(FATAL,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("maintenance mode: connected by superuser only"),
				 errSendAlert(false)));

	/*
	 * MPP:  If we were started in utility mode then we only want to allow
	 * incoming sessions that specify gp_session_role=utility as well.  This
	 * lets the bash scripts start the QD in utility mode and connect in but
	 * protect ourselves from normal clients who might be trying to connect to
	 * the system while we startup.
	 */
	if ((Gp_role == GP_ROLE_UTILITY) && (Gp_session_role != GP_ROLE_UTILITY))
	{
		ereport(FATAL,
				(errcode(ERRCODE_CANNOT_CONNECT_NOW),
				 errmsg("System was started in master-only utility mode - only utility mode connections are allowed")));
	}

	/* Apply PostAuthDelay as soon as we've read all options */
	if (PostAuthDelay > 0)
		pg_usleep(PostAuthDelay * 1000000L);

	/* set default namespace search path */
	InitializeSearchPath();

	/* initialize client encoding */
	InitializeClientEncoding();

	/* report this backend in the PgBackendStatus array */
	if (!bootstrap)
		pgstat_bestart();
		
	/* 
     * MPP package setup 
     *
     * Primary function is to establish connctions to the qExecs.
     * This is SKIPPED when the database is in bootstrap mode or 
     * Is not UnderPostmaster.
     */
    if (!bootstrap && IsUnderPostmaster)
    {
		cdb_setup();
		on_proc_exit( cdb_cleanup, 0 );
    }

    /* 
     * MPP SharedSnapshot Setup
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		addSharedSnapshot("Query Dispatcher", gp_session_id);
	}
    else if (Gp_segment == -1 && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
    {
		/* 
		 * Entry db singleton QE is a user of the shared snapshot -- not a creator.
		 * The lookup will occur once the distributed snapshot has been received.
		 */	
		lookupSharedSnapshot("Entry DB Singleton", "Query Dispatcher", gp_session_id);
    }
    else if (Gp_role == GP_ROLE_EXECUTE)
	{
		if (Gp_is_writer)
		{
			addSharedSnapshot("Writer qExec", gp_session_id);
		}
		else
		{
			/*
			 * NOTE: This assumes that the Slot has already been
			 *       allocated by the writer.  Need to make sure we
			 *       always allocate the writer qExec first.
			 */			 			
			lookupSharedSnapshot("Reader qExec", "Writer qExec", gp_session_id);
		}
	}

	/* close the transaction we started above */
	if (!bootstrap)
		CommitTransactionCommand();

	return;
}

/*
 * Process any command-line switches and any additional GUC variable
 * settings passed in the startup packet.
 */
static void
process_startup_options(Port *port, bool am_superuser)
{
	GucContext	gucctx;
	ListCell   *gucopts;

	gucctx = am_superuser ? PGC_SUSET : PGC_BACKEND;

	/*
	 * First process any command-line switches that were included in the
	 * startup packet, if we are in a regular backend.
	 */
	if (port->cmdline_options != NULL)
	{
		/*
		 * The maximum possible number of commandline arguments that could
		 * come from port->cmdline_options is (strlen + 1) / 2; see
		 * pg_split_opts().
		 */
		char	  **av;
		int			maxac;
		int			ac;

		maxac = 2 + (strlen(port->cmdline_options) + 1) / 2;

		av = (char **) palloc(maxac * sizeof(char *));
		ac = 0;

		av[ac++] = "postgres";

		pg_split_opts(av, &ac, port->cmdline_options);

		av[ac] = NULL;

		Assert(ac < maxac);

		(void) process_postgres_switches(ac, av, gucctx, NULL);
	}

	/*
	 * Process any additional GUC variable settings passed in startup packet.
	 * These are handled exactly like command-line variables.
	 */
	gucopts = list_head(port->guc_options);
	while (gucopts)
	{
		char	   *name;
		char	   *value;

		name = lfirst(gucopts);
		gucopts = lnext(gucopts);

		value = lfirst(gucopts);
		gucopts = lnext(gucopts);

		SetConfigOption(name, value, gucctx, PGC_S_CLIENT);
	}
}

/*
 * Backend-shutdown callback.  Do cleanup that we want to be sure happens
 * before all the supporting modules begin to nail their doors shut via
 * their own callbacks.
 *
 * User-level cleanup, such as temp-relation removal and UNLISTEN, happens
 * via separate callbacks that execute before this one.  We don't combine the
 * callbacks because we still want this one to happen if the user-level
 * cleanup fails.
 */
static void
ShutdownPostgres(int code, Datum arg)
{
	/* Make sure we've killed any active transaction */
	AbortOutOfAnyTransaction();

	/*
	 * If there was a segment OOM for which we haven't already reported
	 * our usage, report now.
	 */
	ReportOOMConsumption();

#ifdef USE_ORCA
	START_MEMORY_ACCOUNT(MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_Optimizer));
	{
		TerminateGPOPT();
	}
	END_MEMORY_ACCOUNT();
#endif

	/* Disable memory protection */
	GPMemoryProtect_Shutdown();
	/* Release SessionState entry */
	SessionState_Shutdown();

	/*
	 * User locks are not released by transaction end, so be sure to release
	 * them explicitly.
	 */
	LockReleaseAll(USER_LOCKMETHOD, true);
}


/*
 * Returns true if at least one role is defined in this database cluster.
 */
static bool
ThereIsAtLeastOneRole(void)
{
	Relation	pg_authid_rel;
	HeapScanDesc scan;
	bool		result;

	pg_authid_rel = heap_open(AuthIdRelationId, AccessShareLock);

	scan = heap_beginscan(pg_authid_rel, SnapshotNow, 0, NULL);
	result = (heap_getnext(scan, ForwardScanDirection) != NULL);

	heap_endscan(scan);
	heap_close(pg_authid_rel, AccessShareLock);

	return result;
}
