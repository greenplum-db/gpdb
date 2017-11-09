/*-------------------------------------------------------------------------
 *
 * cdb_dump_agent.c
 *	  pg_dump is a utility for dumping out a postgres database
 *	  into a script file.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	pg_dump will read the system catalogs in a database and dump out a
 *	script that reproduces the schema in terms of SQL that is understood
 *	by PostgreSQL
 *
 *	Note that pg_dump runs in a serializable transaction, so it sees a
 *	consistent snapshot of the database including system catalogs.
 *	However, it relies in part on various specialized backend functions
 *	like pg_get_indexdef(), and those things tend to run on SnapshotNow
 *	time, ie they look at the currently committed state.  So it is
 *	possible to get 'cache lookup failed' error if someone performs DDL
 *	changes while a dump is happening. The window for this sort of thing
 *	is from the beginning of the serializable transaction to
 *	getSchemaData() (when pg_dump acquires AccessShareLock on every
 *	table it intends to dump). It isn't very large, but it can happen.
 *
 *	http://archives.postgresql.org/pgsql-bugs/2010-02/msg00187.php
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/bin/pg_dump/pg_dump.c,v 1.452 2006/10/07 20:59:04 petere Exp $
 *
 *-------------------------------------------------------------------------
 */

/*
 * Although this is not a backend module, we must include postgres.h anyway
 * so that we can include a bunch of backend include files.  pg_dump has
 * never pretended to be very independent of the backend anyhow ...
 * Is this still true?  PG 9 doesn't include this.
 */
#include "postgres.h"
#include "postgres_fe.h"

#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <poll.h>

#ifdef ENABLE_NLS
#include <locale.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "getopt_long.h"

#ifndef HAVE_INT_OPTRESET
int			optreset;
#endif

#include "access/htup.h"
#include "catalog/pg_class.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "commands/sequence.h"
#include "libpq/libpq-fs.h"

#include "libpq-fe.h"
#include "libpq/libpq-fs.h"
#include "libpq-int.h"

#include "pg_dump.h"
#include "pg_backup.h"
#include "pg_backup_archiver.h"
#include "dumputils.h"
#include "cdb_dump_include.h"
#include "cdb_backup_status.h"
#include "cdb_backup_state.h"
#include "cdb_dump_util.h"
#include "cdb_table.h"
#include <assert.h>
#include <limits.h>
#include <regex.h>
#include <sys/wait.h>
#include "lib/stringinfo.h"

#define DUMP_PREFIX (dump_prefix==NULL?"":dump_prefix)

extern char *optarg;
extern int	optind,
			opterr;
extern bool is_gpdump;

typedef struct
{
	const char *descr;			/* comment for an object */
	Oid			classoid;		/* object class (catalog OID) */
	Oid			objoid;			/* object OID */
	int			objsubid;		/* subobject (table column #) */
} CommentItem;


/* global decls */
bool		g_verbose;			/* User wants verbose narration of our
								 * activities. */
Archive    *g_fout;				/* the script file */
PGconn	   *g_conn;				/* the database connection */

/* mpp addition start */
PGconn	   *g_conn_status = NULL;		/* the status connection between main
										 * thread and agent */

/* mpp addition end */

/* various user-settable parameters */
bool		schemaOnly;
bool		dataOnly;
bool		aclsSkip;
bool		incrementalBackup;
bool		no_lock;
static char	*incrementalFilter = NULL;
char		fileFormat;
char		*filename;
int		compressLevel;
char		*postSchemaFileSuffix = "_post_data";

#ifdef USE_DDBOOST
	char            *g_pszDDBoostFile = NULL;
	int 		ret = 0;
	char		*temp_dd_file = NULL;
#endif

/* MPP additions start */
static char *selectSchemaName = NULL;	/* name of a single schema to dump */

int			preAndPostDataSchemaOnly;	/* int because getopt_long() */
int			preDataSchemaOnly;
int			postDataSchemaOnly;
/* MPP additions end */


/*
 * Indicates whether or not SET SESSION AUTHORIZATION statements should be emitted
 * instead of ALTER ... OWNER statements to establish object ownership.
 * Set through the --use-set-session-authorization option.
 */
static int	use_setsessauth = 0;

/* default, if no "inclusion" switches appear, is to dump everything */
static bool include_everything = true;

char		g_opaque_type[10];	/* name for the opaque type */

/* placeholders for the delimiters for comments */
char		g_comment_start[10];
char		g_comment_end[10];

/* flag to turn on/off dollar quoting */
static int	disable_dollar_quoting = 0;
static int	dump_inserts = 0;
static int	column_inserts = 0;

/* MPP Additions start */
static SegmentDatabase g_SegDB;
static int	plainText = 0;
static int	g_role = 0;
static char *g_CDBDumpInfo = NULL;
static char *g_CDBDumpKey = NULL;
static int	g_contentID = 0;
static int	g_dbID = 0;
static char *g_CDBPassThroughCredentials = NULL;
static pthread_t g_main_tid = (pthread_t) 0;
static pthread_t g_monitor_tid = (pthread_t) 0;
static char *g_pszCDBOutputDirectory = NULL;
static char *g_LastMsg = NULL;
static StatusOpList *g_pStatusOpList = NULL;
static char tableFileName[300];
static char schemaFileName[300];
static char *dump_prefix = NULL;
static bool g_is_old_format = false;

static const char *logInfo = "INFO";
static const char *logWarn = "WARN";
static const char *logError = "ERROR";
/* MPP Additions end */

static void help(const char *progname);
static void dumpTableData(Archive *fout, TableDataInfo *tdinfo);
static void dumpComment(Archive *fout, const char *target,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId);
static int findComments(Archive *fout, Oid classoid, Oid objoid,
			 CommentItem **items);
static int	collectComments(Archive *fout, CommentItem **items);
static void dumpDumpableObject(Archive *fout, DumpableObject *dobj);
static void dumpNamespace(Archive *fout, NamespaceInfo *nspinfo);
static void dumpType(Archive *fout, TypeInfo *tinfo);
static void dumpTypeStorageOptions(Archive *fout, TypeStorageOptions *tstorageoptions);
static void dumpBaseType(Archive *fout, TypeInfo *tinfo);
static void dumpDomain(Archive *fout, TypeInfo *tinfo);
static void dumpCompositeType(Archive *fout, TypeInfo *tinfo);
static void dumpShellType(Archive *fout, ShellTypeInfo *stinfo);
static void dumpProcLang(Archive *fout, ProcLangInfo *plang);
static void dumpFunc(Archive *fout, FuncInfo *finfo);
static char *getFuncOwner(Oid funcOid, const char *templateField);
static void dumpPlTemplateFunc(Oid funcOid, const char *templateField, PQExpBuffer buffer);
static void dumpCast(Archive *fout, CastInfo *cast);
static void dumpOpr(Archive *fout, OprInfo *oprinfo);
static void dumpOpclass(Archive *fout, OpclassInfo *opcinfo);
static void dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo);
static void dumpConversion(Archive *fout, ConvInfo *convinfo);
static void dumpRule(Archive *fout, RuleInfo *rinfo);
static void dumpAgg(Archive *fout, AggInfo *agginfo);
static void dumpExtProtocol(Archive *fout, ExtProtInfo *protinfo);
static void dumpTrigger(Archive *fout, TriggerInfo *tginfo);
static void dumpTable(Archive *fout, TableInfo *tbinfo);
static void dumpTableSchema(Archive *fout, TableInfo *tbinfo);
static void dumpAttrDef(Archive *fout, AttrDefInfo *adinfo);
static void dumpSequence(Archive *fout, TableInfo *tbinfo);
static void dumpIndex(Archive *fout, IndxInfo *indxinfo);
static void dumpConstraint(Archive *fout, ConstraintInfo *coninfo);
static void dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo);
static void dumpTSParser(Archive *fout, TSParserInfo *prsinfo);
static void dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo);
static void dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo);
static void dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo);
static void dumpExtension(Archive *fout, ExtensionInfo *extinfo);

static void dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name,
		const char *tag, const char *nspname, const char *owner,
		const char *acls);

static void getDependencies(void);
static void setExtPartDependency(TableInfo *tblinfo, int numTables);
static void getTableData(TableInfo *tblinfo, int numTables, bool oids);
static char *format_function_arguments(FuncInfo *finfo, char *funcargs);
static char *format_function_arguments_old(FuncInfo *finfo, int nallargs,
						  char **allargtypes,
						  char **argmodes,
						  char **argnames);
static char *format_function_signature(FuncInfo *finfo, bool honor_quotes);
static bool is_returns_table_function(int nallargs, char **argmodes);
static char *format_table_function_columns(FuncInfo *finfo, int nallargs,
							  char **allargtypes,
							  char **argmodes,
							  char **argnames);
static const char *convertRegProcReference(const char *proc);
static const char *convertOperatorReference(const char *opr);
static const char *convertTSFunction(Oid funcOid);
static char *getFormattedTypeName(Oid oid, OidOptions opts);
static bool hasBlobs(Archive *AH);
static int	dumpBlobs(Archive *AH, void *arg __attribute__((unused)));
static int	dumpBlobComments(Archive *AH, void *arg __attribute__((unused)));
static void dumpDatabase(Archive *AH);
static void dumpEncoding(Archive *AH);
static void dumpStdStrings(Archive *AH);
static void dumpAOStorageOpts(Archive *AH);
static const char *getAttrName(int attrnum, TableInfo *tblInfo);
static const char *fmtCopyColumnList(const TableInfo *ti);

/* MPP additions start */
static void _check_database_version(ArchiveHandle *AH);
extern void makeSureMonitorThreadEnds(int TaskRC, char *pszErrorMsg);
static void *monitorThreadProc(void *arg __attribute__((unused)));
static void myHandler(int signo);
static void addDistributedBy(PQExpBuffer q, TableInfo *tbinfo, int actual_atts);
static char *formCDatabaseFilePathName(char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID);
static char *formGenericFilePathName(char *keyword, char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID, const char *dumpSuffix);
static void dumpMain(bool oids, const char *dumpencoding, int outputBlobs, int plainText, RestoreOptions *ropt);
static void dumpDatabaseDefinition(void);
static void installSignalHandlers(void);
extern void reset(void);
static bool testAttributeEncodingSupport(void);
static bool isGPDB4300OrLater(void);
static bool isGPDB(void);
static bool isGPDB5000OrLater(void);
Archive *makeArchive(char *filename);
void updateDDBoostArchive(ArchiveHandle *AH);
char *formPostDumpFilePathName(char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID);
static void dumpDatabaseConfig(const char *dbname, PQExpBuffer *buf);
static void makeAlterConfigCommand(const char *arrayitem, const char *type, const char *name, PQExpBuffer *buf);

/* MPP additions end */

#ifdef USE_DDBOOST
#include "ddp_api.h"
static int dd_boost_enabled = 0; /* Is set to 1 if we are doing a backup onto Data Domain system */
static int dd_boost_buf_size = 0;
static char *ddboost_storage_unit = NULL;
static void dumpDatabaseDefinitionToDDBoost(void);

#ifndef MAX_PATH_NAME
#define MAX_PATH_NAME 1024
#endif

#ifndef DDBOOST_POOL_SIZE
#define DDBOOST_POOL_SIZE (32 * 1024 * 2048)
#endif

/* DDBoost routines */
static int cleanupDDSystem(void);

static char *g_pszDDBoostDir = "db_dumps";
extern ddp_client_info_t dd_client_info;

static ddp_inst_desc_t ddp_inst = DDP_INVALID_DESCRIPTOR;
static ddp_conn_desc_t ddp_conn = DDP_INVALID_DESCRIPTOR;

static ddp_path_t path1 = {0};
static char *DEFAULT_BACKUP_DIRECTORY = NULL;

char *log_message_path = NULL;

static int createDDBoostDir(ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit, char *path_name);
static int updateArchiveWithDDFile(ArchiveHandle *AH, char *g_pszDDBoostFile, const char *g_pszDDBoostDir);
#endif

/*
 * If GPDB version is 4.3, pg_proc has prodataaccess column.
 */
static bool
isGPDB4300OrLater(void)
{
	bool	retValue = false;

	PQExpBuffer query;
	PGresult   *res;

	query = createPQExpBuffer();
	appendPQExpBuffer(query,
			"select attnum from pg_catalog.pg_attribute "
			"where attrelid = 'pg_catalog.pg_proc'::regclass and "
			"attname = 'prodataaccess'");
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) == 1)
		retValue = true;

	PQclear(res);
	destroyPQExpBuffer(query);

	return retValue;
}
/*
 * testAttributeEncodingSupport - tests whether or not the current GP
 * database includes support for column encoding.
 */
static bool
testAttributeEncodingSupport(void)
{
	PQExpBuffer query;
	PGresult   *res;
	bool		isSupported;

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT 1 from pg_catalog.pg_class where relnamespace = 11 and relname  = 'pg_attribute_encoding';");
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	isSupported = (PQntuples(res) == 1);

	PQclear(res);
	destroyPQExpBuffer(query);

	return isSupported;
}

/*
 * Check if we are talking to GPDB
 */
static bool
isGPDB(void)
{
	PQExpBuffer query;
	PGresult   *res;
	char	   *ver;
	bool		retValue = false;

	query = createPQExpBuffer();
	appendPQExpBuffer(query, "select version()");
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ver = (PQgetvalue(res, 0, 0));
	if (strstr(ver, "Greenplum") != NULL)
		retValue = true;

	PQclear(res);
	destroyPQExpBuffer(query);
	return retValue;
}

/*
 * If GPDB version is 5.0, pg_proc has provariadic column.
 * When we have version() returns GPDB version instead of "main build dev" or
 * something similar, we'll fix this function (MPP-17313)
 */
static bool
isGPDB5000OrLater(void)
{
	bool	retValue = false;

	if (isGPDB() == true)
	{
		PQExpBuffer query;
		PGresult   *res;

		query = createPQExpBuffer();
		appendPQExpBuffer(query,"select attnum from pg_catalog.pg_attribute "
						  "where attrelid = 'pg_catalog.pg_proc'::regclass "
						  "and attname = 'provariadic'");
		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) == 1)
			retValue = true;

		PQclear(res);
		destroyPQExpBuffer(query);
	}

	return retValue;
}


int
main(int argc, char **argv)
{
	int			c;
	const char *format = "p";
	const char *dbname = NULL;
	const char *pghost = NULL;
	const char *pgport = NULL;
	const char *username = NULL;
	const char *dumpencoding = NULL;
	bool		oids = false;
	enum trivalue prompt_password = TRI_DEFAULT;
	int			compressLevel = -1;
	int			outputClean = 0;
	int			outputCreateDB = 0;
	bool		outputBlobs = false;
	int			outputNoOwner = 0;
	char	   *outputSuperuser = NULL;

	/* MPP additions start */
	PGresult   *res = NULL;
	unsigned int nBytes;
	char	   *pszDBPswd;
	ArchiveHandle *AH;
	StatusOp   *pOp;

	/* MPP additions end */

	RestoreOptions *ropt;

	static int	disable_triggers = 0;

	static struct option long_options[] = {
		{"data-only", no_argument, NULL, 'a'},
		{"blobs", no_argument, NULL, 'b'},
		{"clean", no_argument, NULL, 'c'},
		{"create", no_argument, NULL, 'C'},
		{"file", required_argument, NULL, 'f'},
		{"format", required_argument, NULL, 'F'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"incremental", no_argument, NULL, 'I'},
		{"no-reconnect", no_argument, NULL, 'R'},
		{"oids", no_argument, NULL, 'o'},
		{"no-owner", no_argument, NULL, 'O'},
		{"port", required_argument, NULL, 'p'},
		{"schema", required_argument, NULL, 'n'},
		{"exclude-schema", required_argument, NULL, 'N'},
		{"schema-only", no_argument, NULL, 's'},
		{"superuser", required_argument, NULL, 'S'},
		{"table", required_argument, NULL, 't'},
		{"exclude-table", required_argument, NULL, 'T'},
		{"no-password", no_argument, NULL, 'w'},
		{"password", no_argument, NULL, 'W'},
		{"username", required_argument, NULL, 'U'},
		{"verbose", no_argument, NULL, 'v'},
		{"no-privileges", no_argument, NULL, 'x'},
		{"no-acl", no_argument, NULL, 'x'},
		{"compress", required_argument, NULL, 'Z'},
		{"encoding", required_argument, NULL, 'E'},
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'V'},
		/* mpp additions */
		{"gp-k", required_argument, NULL, 1},
		{"gp-d", required_argument, NULL, 2},
		{"table-file", required_argument, NULL, 3},
		{"exclude-table-file", required_argument, NULL, 4},
		{"prefix", required_argument, NULL, 5},
		/* end mpp additions */

		/*
		 * the following options don't have an equivalent short option letter
		 */
		{"attribute-inserts", no_argument, &column_inserts, 1},
		{"column-inserts", no_argument, &column_inserts, 1},
		{"disable-dollar-quoting", no_argument, &disable_dollar_quoting, 1},
		{"disable-triggers", no_argument, &disable_triggers, 1},
		{"inserts", no_argument, &dump_inserts, 1},
		{"use-set-session-authorization", no_argument, &use_setsessauth, 1},

		/* START MPP ADDITION */

		/*
		 * the following are mpp specific, and don't have an equivalent short
		 * option
		 */
		{"pre-and-post-data-schema-only", no_argument, &preAndPostDataSchemaOnly, 1},
		/* END MPP ADDITION */

#ifdef USE_DDBOOST
		{"dd_boost_file", required_argument, NULL, 6},
		{"dd_boost_enabled", no_argument, NULL, 7},
		{"dd_boost_dir", required_argument, NULL, 8},
		{"dd_boost_buf_size", required_argument, NULL, 9},
		{"ddboost-storage-unit", required_argument, NULL, 18},
#endif
		{"incremental-filter", required_argument, NULL, 10},
		{"netbackup-service-host", required_argument, NULL, 11},
		{"netbackup-policy", required_argument, NULL, 12},
		{"netbackup-schedule", required_argument, NULL, 13},
		{"netbackup-block-size", required_argument, NULL, 14},
		{"netbackup-keyword", required_argument, NULL, 15},
		{"no-lock", no_argument, NULL, 16},
		{"schema-file", required_argument, NULL, 17},
		{"old-format", no_argument, NULL, 19},
		{NULL, 0, NULL, 0}
	};
	int			optindex;

	set_pglocale_pgservice(argv[0], "pg_dump");

	g_verbose = false;
	is_gpdump = true;

	strcpy(g_comment_start, "-- ");
	g_comment_end[0] = '\0';
	strcpy(g_opaque_type, "opaque");

	dataOnly = schemaOnly = dump_inserts = column_inserts = false;
	preAndPostDataSchemaOnly = preDataSchemaOnly = postDataSchemaOnly = false;
	incrementalBackup = false;

	progname = get_progname(argv[0]);

	/* Set default options based on progname */
	if (strcmp(progname, "pg_backup") == 0)
		format = "c";

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_dump (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

#ifdef USE_DDBOOST
	dd_boost_enabled = 0;
#endif


	while ((c = getopt_long(argc, argv, "abcCdDE:f:F:h:iIn:N:oOp:RsS:t:T:uU:vwWxX:Z:",
							long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 'a':			/* Dump data only */
				dataOnly = true;
				break;

			case 'b':			/* Dump blobs */
				outputBlobs = true;
				break;

			case 'c':			/* clean (i.e., drop) schema prior to create */
				outputClean = 1;
				break;

			case 'C':			/* Create DB */
				outputCreateDB = 1;
				break;

			case 'd':			/* dump data as proper insert strings */
				dump_inserts = true;
				fprintf(stderr," --inserts is preferred over -d.  -d is deprecated.\n");
				break;

			case 'D':			/* dump data as proper insert strings with
								 * attr names */
				dump_inserts = true;
				column_inserts = true;
				fprintf(stderr," --column-inserts is preferred over -D.  -D is deprecated.\n");
				break;

			case 'E':			/* Dump encoding */
				dumpencoding = optarg;
				break;

			case 'f':
				filename = optarg;
				break;

			case 'F':
				format = optarg;
				break;

			case 'h':			/* server host */
				pghost = optarg;
				break;

			case 'i':
				/* ignored, deprecated option */
				break;

			case 'I':
				incrementalBackup = true;
				break;

			case 'n':			/* include schema(s) */
				simple_string_list_append(&schema_include_patterns, optarg);
				include_everything = false;
				break;

			case 'N':			/* exclude schema(s) */
				simple_string_list_append(&schema_exclude_patterns, optarg);
				break;

			case 'o':			/* Dump oids */
				oids = true;
				break;

			case 'O':			/* Don't reconnect to match owner */
				outputNoOwner = 1;
				break;

			case 'p':			/* server port */
				pgport = optarg;
				break;

			case 'R':
				/* no-op, still accepted for backwards compatibility */
				break;

			case 's':			/* dump schema only */
				schemaOnly = true;
				break;

			case 'S':			/* Username for superuser in plain text output */
				outputSuperuser = pg_strdup(optarg);
				break;

			case 't':			/* include table(s) */
				simple_string_list_append(&table_include_patterns, optarg);
				include_everything = false;
				break;

			case 'T':			/* exclude table(s) */
				simple_string_list_append(&table_exclude_patterns, optarg);
				break;

			case 'u':
				prompt_password = TRI_YES;
				username = simple_prompt("User name: ", 100, true);
				break;

			case 'U':
				username = optarg;
				break;

			case 'v':			/* verbose */
				g_verbose = true;
				break;

			case 'w':
				prompt_password = TRI_NO;
				break;

			case 'W':
				prompt_password = TRI_YES;
				break;

			case 'x':			/* skip ACL dump */
				aclsSkip = true;
				break;

			case 'X':
				/* -X is a deprecated alternative to long options */
				if (strcmp(optarg, "disable-dollar-quoting") == 0)
					disable_dollar_quoting = 1;
				else if (strcmp(optarg, "disable-triggers") == 0)
					disable_triggers = 1;
				else if (strcmp(optarg, "use-set-session-authorization") == 0)
					use_setsessauth = 1;
				else
				{
					fprintf(stderr,
							_("%s: invalid -X option -- %s\n"),
							progname, optarg);
					fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
					exit(1);
				}
				break;

			case 'Z':			/* Compression Level */
				compressLevel = atoi(optarg);
				break;

			case 0:
				/* This covers the long options equivalent to -X xxx. */
				break;


			case 1:				/* MPP Dump Info Format is Key_role_dbid */
				g_CDBDumpInfo = pg_strdup(optarg);
				if (!ParseCDBDumpInfo((char *) progname, g_CDBDumpInfo, &g_CDBDumpKey, &g_role, &g_contentID, &g_dbID, &g_CDBPassThroughCredentials))
					exit(1);
				break;

			case 2:				/* --gp-d CDB Output Directory */
				g_pszCDBOutputDirectory = pg_strdup(optarg);
				break;

			case 3: 			/*	--table-file */
				if (!open_file_and_append_to_list(optarg, &table_include_patterns, "include tables list"))
				{
					fprintf(stderr,
							_("%s: invalid --table-file option -- can't open file %s\n"),
							progname, optarg);
					exit(1);
				}
				include_everything = false;
				if (strlcpy(tableFileName, optarg, sizeof(tableFileName)) >= sizeof(tableFileName))
				{
					fprintf(stderr,
							_("%s: invalid --table-file option, filename too long\n"),
							progname);
					exit(1);
				}
				break;
			case 4: 			/*	--exclude-table-file */
				if (!open_file_and_append_to_list(optarg, &table_exclude_patterns, "exclude tables list"))
				{
					fprintf(stderr,
							_("%s: invalid --exclude-table-file option -- can't open file %s\n"),
							progname, optarg);
					exit(1);
				}
				break;
			case 5:
				dump_prefix = pg_strdup(optarg);
				break;

#ifdef USE_DDBOOST
			case 6:
				g_pszDDBoostFile = pg_strdup(optarg);
				break;
			case 7:
				dd_boost_enabled = 1;
				break;
			case 8:
				g_pszDDBoostDir = pg_strdup(optarg);
				break;
			case 9:
				sscanf(optarg, "%d", &dd_boost_buf_size);
				break;
			case 18:
				ddboost_storage_unit = pg_strdup(optarg);
				break;
#endif
			case 10:
				incrementalFilter = pg_strdup(optarg);
				break;
			/* Hack to pass NetBackup params to gp_dump_agent */
			case 11:
				break;
			case 12:
				break;
			case 13:
				break;
			case 14:
				break;
			case 15:
				break;
			case 16:
				no_lock = true;
				break;
			case 17:            /*  --schema-file */
				if (!open_file_and_append_to_list(optarg, &schema_include_patterns, "include schema list"))
				{
					fprintf(stderr,
						_("%s: invalid --schema-file option -- can't open file %s\n"),
							progname, optarg);
					exit(1);
				}
				include_everything = false;
				strncpy(schemaFileName, optarg, sizeof(optarg)+1);
				break;
			case 19:
				g_is_old_format = true;
				break;

			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
		}
	}

	/* for master, enable the pre data schema dump first,
	 * once finished, enable the post schema dump
	 */
	if(preAndPostDataSchemaOnly)
	{
		preDataSchemaOnly = true;
		postDataSchemaOnly = false;
	}

	if (g_role == ROLE_MASTER && incrementalFilter)
	{
		table_include_patterns.head = NULL;
		table_include_patterns.tail = NULL;
		if (!open_file_and_append_to_list(incrementalFilter, &table_include_patterns, "include tables list"))
		{
			fprintf(stderr,
					_("%s: invalid --incremental-filter option -- can't open file %s\n"),
					progname, incrementalFilter);
			exit(1);
		}
		include_everything = false;
		if (strlcpy(tableFileName, incrementalFilter, sizeof(tableFileName)) >= sizeof(tableFileName))
		{
			fprintf(stderr,
					_("%s: invalid --incremental-filter option -- filename too long\n"),
					progname);
			exit(1);
		}
	}

	if (optind < (argc - 1))
	{
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind + 1]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit(1);
	}

	/* Get database name from command line */
	if (optind < argc)
		dbname = argv[optind];

	/* --column-inserts implies --inserts */
	if (column_inserts)
		dump_inserts = 1;

	if (dataOnly && schemaOnly)
	{
		mpp_err_msg(logError, progname, "options \"schema only\" (-s) and \"data only\" (-a) cannot be used together\n");
		exit(1);
	}

	if (dataOnly && outputClean)
	{
		/*
		 * we already check for this error in gp_dump (main thread). if we get
		 * here now it means that user specified '-c' and gp_dump appended
		 * "data only" to the agent of all the segment databases. since '-c'
		 * is only needed on the master agent and not on the seg databases
		 * agents, we just set it to false here.
		 */
		outputClean = false;
	}

	if (dump_inserts && oids)
	{
		write_msg(NULL, "options --inserts/--column-inserts (-d, -D) and OID (-o) options cannot be used together\n");
		write_msg(NULL, "(The INSERT command cannot set OIDs.)\n");
		exit(1);
	}

	/* MPP addition start */
	if (g_CDBDumpInfo == NULL)
	{
		mpp_err_msg(logError, progname, "missing required parameter gp-k (backup key)\n");
		exit(1);
	}
	/* MPP addition end */

#ifdef USE_DDBOOST
	if (dd_boost_enabled)
	{
		/* The storage unit is created by the gpcrondump, before all the agents are executed */
		/* Hence it is always false here														*/
		/* remote is always false when doing backup to primary DDR */
		int err = DD_ERR_NONE;
		err = ddp_init("gp_dump");
		if (err != DD_ERR_NONE)
		{
			mpp_err_msg("ERROR", "ddboost", "ddboost init failed. Err = %d\n", err);
			exit(1);
		}

		ret = initDDSystem(&ddp_inst, &ddp_conn, &dd_client_info, &ddboost_storage_unit, false, &DEFAULT_BACKUP_DIRECTORY, false);

		if (ret)
		{
			mpp_err_msg(logError, progname, "Error connecting to DDboost. Check parameters\n");
			exit(1);
		}
	}
#endif

	fileFormat = format[0];

	g_fout = makeArchive(filename);

	if (g_fout == NULL)
	{
		mpp_err_msg(logError, progname, "could not open output file \"%s\" for writing\n", filename);
		exit(1);
	}

	/* Let the archiver know how noisy to be */
	g_fout->verbose = g_verbose;
	g_fout->minRemoteVersion = 80200;	/* we can handle back to 8.2 */
	g_fout->maxRemoteVersion = parse_version(PG_VERSION);
	if (g_fout->maxRemoteVersion < 0)
	{
		mpp_err_msg(logError, progname, "could not parse version string \"%s\"\n", PG_VERSION);
		exit(1);
	}

	/* MPP additions start */
	AH = (ArchiveHandle *) g_fout;
	AH->dieFuncPtr = makeSureMonitorThreadEnds; /* set a function to call upon die */

	/* set the connection parameters for this segdb */
	g_SegDB.dbid = g_dbID;
	g_SegDB.role = g_role;
	g_SegDB.port = pgport ? atoi(pgport) : 5432;
	g_SegDB.pszHost = pghost ? pg_strdup(pghost) : NULL;
	g_SegDB.pszDBName = dbname ? pg_strdup(dbname) : NULL;
	g_SegDB.pszDBUser = username ? pg_strdup(username) : NULL;
	g_SegDB.pszDBPswd = NULL;

	/*
	 * If there is a password associated with the login, it will come through
	 * as a base64 encoded string as part of the g_CDBDumpInfo that was parsed
	 * above. So we must decode it to get the true password,
	 */
	if (g_CDBPassThroughCredentials != NULL && *g_CDBPassThroughCredentials != '\0')
	{
		pszDBPswd = Base64ToData(g_CDBPassThroughCredentials, &nBytes);
		if (pszDBPswd == NULL)
		{
			mpp_err_msg(logError, progname, "Invalid CDB Credentials:  %s\n", g_CDBPassThroughCredentials);
			exit(1);
		}
		if (nBytes > 0)
		{
			g_SegDB.pszDBPswd = malloc(nBytes + 1);
			if (g_SegDB.pszDBPswd == NULL)
			{
				mpp_err_msg(logError, progname, "Cannot allocate memory for CDB Credentials\n");
				exit(1);
			}

			memcpy(g_SegDB.pszDBPswd, pszDBPswd, nBytes);
			g_SegDB.pszDBPswd[nBytes] = '\0';
		}
	}
	/* MPP additions end */

	/* connect to the database we want to dump */
	g_conn = MakeDBConnection(&g_SegDB, false);

	if (PQstatus(g_conn) == CONNECTION_BAD)
	{
		exit_horribly(g_fout, NULL, "Connection to database \"%s\" failed: %s",
					  PQdb(g_conn), PQerrorMessage(g_conn));
	}

	/* check for version mismatch */
	AH->connection = g_conn;
	_check_database_version(AH);

#ifdef USE_DDBOOST

	if (dd_boost_enabled)
	{
		updateDDBoostArchive(AH);
	}
#endif

	if (g_CDBDumpKey != NULL)
	{
		/*
		 * Open the database again, for writing status info
		 */
		g_conn_status = MakeDBConnection(&g_SegDB, false);

		if (PQstatus(g_conn_status) == CONNECTION_BAD)
		{
			exit_horribly(g_fout, NULL, "Connection on host %s failed: %s",
						  StringNotNull(g_SegDB.pszHost, "localhost"),
						  PQerrorMessage(g_conn_status));
		}

		PQclear(res);

		g_main_tid = pthread_self();

		g_pStatusOpList = CreateStatusOpList();
		if (g_pStatusOpList == NULL)
		{
			exit_horribly(NULL, NULL, "cannot allocate memory for gp_backup_status operation\n");
		}

		/* Start monitoring for cancel requests */
		pthread_create(&g_monitor_tid,
					   NULL,
					   monitorThreadProc,
					   NULL);

		/* Install Ctrl-C interrupt handler, now that we have a connection */
		installSignalHandlers();

		/* create an initial status operator that indicates we are STARTing */
		pOp = CreateStatusOp(TASK_START, TASK_RC_SUCCESS, SUFFIX_START, TASK_MSG_SUCCESS);
		if (pOp == NULL)
			exit_horribly(g_fout, NULL, "cannot allocate memory for gp_backup_status operation\n");

		/* add our START status to the operator list */
		AddToStatusOpList(g_pStatusOpList, pOp);
	}

	/*
	 * Start serializable transaction to dump consistent data.
	 */
	do_sql_command(g_conn, "BEGIN");

	res = PQexec(g_conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		/*
		 * insert a status operator to indicate our status failure to get
		 * STATUS_SET_SERIALIZABLE
		 */
		StatusOp   *pOp = CreateStatusOp(TASK_SET_SERIALIZABLE, TASK_RC_FAILURE, SUFFIX_FAIL, "could not set transaction isolation level to serializable");

		if (pOp == NULL)
		{
			exit_horribly(g_fout, NULL, "could not allocate memory for gp_backup_status operation\n");
		}

		/* add status operator, to let the monitor thread see it */
		AddToStatusOpList(g_pStatusOpList, pOp);
		exit_horribly(g_fout, NULL, "could not set transaction isolation level to serializable: %s",
					  PQerrorMessage(g_conn));
	}
	PQclear(res);

	/*
	 * Do a select statement to make sure that the view of the data from
	 * within this transaction has been fixed
	 */
	res = PQexec(g_conn, "SELECT count(*) FROM pg_class");
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		/*
		 * insert a status operator to indicate our status failure to get
		 * STATUS_SET_SERIALIZABLE
		 */
		pOp = CreateStatusOp(TASK_SET_SERIALIZABLE, TASK_RC_FAILURE, SUFFIX_FAIL, "could not set transaction isolation level to serializable");
		if (pOp == NULL)
		{
			exit_horribly(g_fout, NULL, "could not allocate memory for gp_backup_status operation\n");
		}

		/* add status operator, to let the monitor thread see it */
		AddToStatusOpList(g_pStatusOpList, pOp);
		exit_horribly(g_fout, NULL, "First query within the transaction failed: %s",
					  PQerrorMessage(g_conn));
	}
	PQclear(res);

	/*
	 * If we got here it means we successfully set transaction isolation level
	 * to serializable.
	 */
	pOp = CreateStatusOp(TASK_SET_SERIALIZABLE, TASK_RC_SUCCESS, SUFFIX_SET_SERIALIZABLE, TASK_MSG_SUCCESS);
	if (pOp == NULL)
	{
		exit_horribly(g_fout, NULL, "could not allocate memory for gp_backup_status operation\n");
	}

	/* add status operator, to let the monitor thread see it */
	AddToStatusOpList(g_pStatusOpList, pOp);

	/*
	 * And finally we can do the actual output.
	 */
	if (plainText)
	{
		ropt = NewRestoreOptions();
		ropt->filename = (char *) filename;
		ropt->dropSchema = outputClean;
		ropt->aclsSkip = aclsSkip;
		ropt->superuser = outputSuperuser;
		ropt->createDB = outputCreateDB;
		ropt->noOwner = outputNoOwner;
		ropt->disable_triggers = disable_triggers;
		ropt->use_setsessauth = use_setsessauth;
		ropt->dataOnly = dataOnly;

		if (compressLevel == -1)
			ropt->compression = 0;
		else
			ropt->compression = compressLevel;

		ropt->suppressDumpWarnings = true;		/* We've already shown them */
	}
	else
		ropt = NULL;			/* silence compiler */

	/*
	 * do the database main dump, and write contents into the dump file
	 */
	mpp_err_msg(logInfo, progname, "Dumping database \"%s\"...\n", g_conn->dbName);

	dumpMain(oids, dumpencoding, outputBlobs, plainText, ropt);

	/*
	 * Dump CREATE DATABASE statement into a new dump file IFF this is the
	 * master node and data-only option is not set.
	 */
	if (g_role == ROLE_MASTER && !dataOnly  && !postDataSchemaOnly)
	{
		mpp_err_msg(logInfo, progname, "Dumping CREATE DATABASE statement for database \"%s\"\n", g_conn->dbName);
#ifdef USE_DDBOOST

		if(dd_boost_enabled)
			dumpDatabaseDefinitionToDDBoost();
		else
#endif
		dumpDatabaseDefinition();
	}

#ifdef USE_DDBOOST
	if(dd_boost_enabled)
		cleanupDDSystem();
#endif
	/*
	 * Make sure to report that we are done.
	 */
	makeSureMonitorThreadEnds(TASK_RC_SUCCESS, TASK_MSG_SUCCESS);
	DestroyStatusOpList(g_pStatusOpList);

	if (g_pszCDBOutputDirectory != NULL)
		free(g_pszCDBOutputDirectory);

	if (g_SegDB.pszHost)
		free(g_SegDB.pszHost);
	if (g_SegDB.pszDBUser)
		free(g_SegDB.pszDBUser);
	if (g_SegDB.pszDBPswd)
		free(g_SegDB.pszDBPswd);

	PQfinish(g_conn);

	mpp_err_msg(logInfo, progname, "Finished successfully\n");

	exit(0);
}

void
dumpMain(bool oids, const char *dumpencoding, int outputBlobs, int plainText, RestoreOptions *ropt)
{

	TableInfo  *tblinfo;
	DumpableObject **dobjs;
	StatusOp   *pOp;
	int			numObjs;
	int			numTables;
	int			i;
	const char *std_strings;

	/* Set the client encoding if requested */
	if (dumpencoding)
	{
		if (PQsetClientEncoding(g_conn, dumpencoding) < 0)
		{
			mpp_err_msg(logError, progname, "invalid client encoding \"%s\" specified\n",
						dumpencoding);
			exit(1);
		}
	}

	/*
	 * Get the active encoding and the standard_conforming_strings setting, so
	 * we know how to escape strings.
	 */
	g_fout->encoding = PQclientEncoding(g_conn);

	std_strings = PQparameterStatus(g_conn, "standard_conforming_strings");
	g_fout->std_strings = (std_strings && strcmp(std_strings, "on") == 0);

	/* Set the datestyle to ISO to ensure the dump's portability */
	do_sql_command(g_conn, "SET DATESTYLE = ISO");

	/*
	 * If supported, set extra_float_digits so that we can dump float data
	 * exactly (given correctly implemented float I/O code, anyway)
	 */
	do_sql_command(g_conn, "SET extra_float_digits TO 2");

	/*
	 * Let cdb_dump_include functions know whether or not to include
	 * partitioning support.
	 */
	g_gp_supportsPartitioning = g_fout->remoteVersion >= 80209;
	g_gp_supportsPartitionTemplates = g_fout->remoteVersion >= 80214;

	/* Let cdb_dump_include functions know whether to include opfamilies */
	g_gp_supportsOpfamilies = g_fout->remoteVersion >= 80300;

	g_gp_supportsAttributeEncoding = testAttributeEncodingSupport();

	/* Let cdb_dump_include functions know whether to include full text */
	g_gp_supportsFullText = g_fout->remoteVersion >= 80300;

	/* Let cdb_dump_include functions know whether to handle lanowner col */
	g_gp_supportsLanOwner = g_fout->remoteVersion >= 80300;

	/*
	 * Process the schema and table include/exclude lists and develop a list
	 * of table oids to be included in the dump.
	 */
	if (isFilteringAllowedNow(g_role, incrementalBackup, incrementalFilter))
	    convert_patterns_to_oids();

	/*
	 * Now scan the database and create DumpableObject structs for all the
	 * objects we intend to dump. LOCK those objects in ACCESS SHARE mode.
	 */
	tblinfo = getSchemaData(&numTables, g_role);

	/*
	 * If we got here it means we successfully got LOCKs on all our dumpable
	 * tables in this agent. we can now notify the main gp_dump thread that
	 * we're ok.
	 */
	pOp = CreateStatusOp(TASK_GOTLOCKS, TASK_RC_SUCCESS, SUFFIX_GOTLOCKS, TASK_MSG_SUCCESS);
	if (pOp == NULL)
	{
		exit_horribly(g_fout, NULL, "could not allocate memory for gp_backup_status operation\n");
	}

	/* add status operator, to let the monitor thread see it */
	AddToStatusOpList(g_pStatusOpList, pOp);

	if (shouldDumpSchemaOnly(g_role, incrementalBackup, (void*)table_include_patterns.head))
        goto skipalldata;

	if (!schemaOnly)
		getTableData(tblinfo, numTables, oids);

	/*
	 * Dumping blobs is now default unless we saw an inclusion switch or -s
	 * ... but even if we did see one of these, -b turns it back on.
	 */
	if (include_everything && !schemaOnly)
		outputBlobs = true;

	if (outputBlobs && hasBlobs(g_fout))
	{
		/* Add placeholders to allow correct sorting of blobs */
		DumpableObject *blobobj;

		blobobj = (DumpableObject *) malloc(sizeof(DumpableObject));
		blobobj->objType = DO_BLOBS;
		blobobj->catId = nilCatalogId;
		AssignDumpId(blobobj);
		blobobj->name = pg_strdup("BLOBS");

		blobobj = (DumpableObject *) malloc(sizeof(DumpableObject));
		blobobj->objType = DO_BLOB_COMMENTS;
		blobobj->catId = nilCatalogId;
		AssignDumpId(blobobj);
		blobobj->name = pg_strdup("BLOB COMMENTS");
	}

	/*
	 * Collect dependency data to assist in ordering the objects.
	 */
	getDependencies();

	setExtPartDependency(tblinfo, numTables);

	/*
	 * Sort the objects into a safe dump order (no forward references).
	 *
	 * In 7.3 or later, we can rely on dependency information to help us
	 * determine a safe order, so the initial sort is mostly for cosmetic
	 * purposes: we sort by name to ensure that logically identical schemas
	 * will dump identically.
	 */
	getDumpableObjects(&dobjs, &numObjs);

	sortDumpableObjectsByTypeName(dobjs, numObjs);

	sortDumpableObjects(dobjs, numObjs);

	/*
	 * Create archive TOC entries for all the objects to be dumped, in a safe
	 * order.
	 */

	/* First the special ENCODING and STDSTRINGS entries. */
	dumpEncoding(g_fout);
	dumpStdStrings(g_fout);
	dumpAOStorageOpts(g_fout);
	/* The database item is always next, unless we don't want it at all */
	if (include_everything && !dataOnly)
		dumpDatabase(g_fout);

	/* Now the rearrangeable objects. */
	for (i = 0; i < numObjs; i++)
		dumpDumpableObject(g_fout, dobjs[i]);

skipalldata:
	/*
	 * And finally we can do the actual output.
	 */
	if (plainText)
		RestoreArchive(g_fout, ropt);

	if(g_role == ROLE_MASTER && !dataOnly)
	{
		CloseArchive(g_fout);
		/* master needs to dump post schema if not dumping data. */
		preDataSchemaOnly = false;
		postDataSchemaOnly = true;

		/* --post-data-schema-only implies --schema-only */
		schemaOnly = true;

		fileFormat = 'f';	/*dump post schema data into local file first*/

		char *postDumpFileName = formPostDumpFilePathName(g_pszCDBOutputDirectory, g_CDBDumpKey, g_contentID, g_dbID);

		g_fout = makeArchive(postDumpFileName);

#ifdef USE_DDBOOST

		/* Only *_post_data files will be dumped on local GPDB directories */
		if (dd_boost_enabled)
		{
			updateDDBoostArchive((ArchiveHandle *)g_fout);
		}
#endif
		for (i = 0; i < numObjs; i++)
			dumpDumpableObject(g_fout, dobjs[i]);

		reset();

		CloseArchive(g_fout);
		free(postDumpFileName);

		postDataSchemaOnly = false;	/*Finish dump post schema, set false, main() dumps create database definition*/
	}
	else
	{
		reset();
		CloseArchive(g_fout);
	}
}


static void
help(const char *progname)
{
	printf(_("%s dumps a database as a text file or to other formats.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... [DBNAME]\n"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  -f, --file=FILENAME      output file name\n"));
	printf(_("  -F, --format=c|t|p       output file format (custom, tar, plain text)\n"));
	printf(_("  -i, --ignore-version     proceed even when server version mismatches\n"
			 "                           pg_dump version\n"));
	printf(_("  -v, --verbose            verbose mode\n"));
	printf(_("  -Z, --compress=0-9       compression level for compressed formats\n"));
	printf(_("  --help                   show this help, then exit\n"));
	printf(_("  --version                output version information, then exit\n"));

	printf(_("\nOptions controlling the output content:\n"));
	printf(_("  -a, --data-only             dump only the data, not the schema\n"));
	printf(_("  -b, --blobs                 include large objects in dump\n"));
	printf(_("  -c, --clean                 clean (drop) schema prior to create\n"));
	printf(_("  -C, --create                include commands to create database in dump\n"));
	printf(_("  -d, --inserts            dump data as INSERT, rather than COPY, commands\n"));
	printf(_("  -D, --column-inserts     dump data as INSERT commands with column names\n"));
	printf(_("  -E, --encoding=ENCODING     dump the data in encoding ENCODING\n"));
	printf(_("  -n, --schema=SCHEMA         dump the named schema(s) only\n"));
	printf(_("  -N, --exclude-schema=SCHEMA do NOT dump the named schema(s)\n"));
	printf(_("  -o, --oids                  include OIDs in dump\n"));
	printf(_("  -O, --no-owner              skip restoration of object ownership\n"
			 "                              in plain text format\n"));
	printf(_("  -s, --schema-only           dump only the schema, no data\n"));
	printf(_("  -S, --superuser=NAME        specify the superuser user name to use in\n"
			 "                              plain text format\n"));
	printf(_("  -t, --table=TABLE           dump the named table(s) only\n"));
	printf(_("  -T, --exclude-table=TABLE   do NOT dump the named table(s)\n"));
	printf(_("  -x, --no-privileges         do not dump privileges (grant/revoke)\n"));
	printf(_("  --disable-dollar-quoting    disable dollar quoting, use SQL standard quoting\n"));
	printf(_("  --disable-triggers          disable triggers during data-only restore\n"));
	printf(_("  --use-set-session-authorization\n"
			 "                              use SESSION AUTHORIZATION commands instead of\n"
	"                              ALTER OWNER commands to set ownership\n"));
	/* START MPP ADDITION */
	printf(("\nGreenplum Database specific options:\n"));
	printf(("   --gp-k=BACKUPKEY        key for CDB Backup\n"));
	printf(("   --gp-d=OUTPUTDIRECTORY  output directory for backup and error files\n"));
	/* END MPP ADDITION */

	printf(_("\nConnection options:\n"));
	printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(_("  -p, --port=PORT          database server port number\n"));
	printf(_("  -U, --username=NAME      connect as specified database user\n"));
	printf(_("  -W, --password           force password prompt (should happen automatically)\n"));

	printf(_("\nIf no database name is supplied, then the PGDATABASE environment\n"
			 "variable value is used.\n\n"));
	/* printf(_("Report bugs to <bugs@greenplum.org>.\n")); */
}

/*
 * exit_nicely: This function was copied from pg_dump.	We changed it so that we
 * can be sure that before exiting, the monitor thread is shut down.
 */
void
exit_nicely(void)
{
	char	   *lastMsg = g_LastMsg;
	char	   *pszErrorMsg;

	if (lastMsg == NULL)
		lastMsg = PQerrorMessage(g_conn);

	pszErrorMsg = MakeString("*** aborted because of error: %s\n", lastMsg);

	makeSureMonitorThreadEnds(TASK_RC_FAILURE, pszErrorMsg);

	if (pszErrorMsg != NULL)
	{
		mpp_err_msg(logError, progname, pszErrorMsg);
		free(pszErrorMsg);
	}

	PQfinish(g_conn);
	g_conn = NULL;

	if (g_LastMsg != NULL)
		free(g_LastMsg);

	exit(1);
}

/*
 *	Dump a table's contents for loading using the COPY command
 *	- this routine is called by the Archiver when it wants the table
 *	  to be dumped.
 */
static int
dumpTableData_copy(Archive *fout, void *dcontext)
{
	TableDataInfo *tdinfo = (TableDataInfo *) dcontext;
	TableInfo  *tbinfo = tdinfo->tdtable;
	const char *classname = tbinfo->dobj.name;
	const bool	hasoids = tbinfo->hasoids;
	const bool	oids = tdinfo->oids;
	PQExpBuffer q = createPQExpBuffer();
	PGresult   *res;
	int			ret;
	char	   *copybuf;
	const char *column_list;

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "dumping contents of table %s\n", classname);

	/*
	 * Make sure we are in proper schema.  We will qualify the table name
	 * below anyway (in case its name conflicts with a pg_catalog table); but
	 * this ensures reproducible results in case the table contains regproc,
	 * regclass, etc columns.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	/*
	 * If possible, specify the column list explicitly so that we have no
	 * possibility of retrieving data in the wrong column order.  (The default
	 * column ordering of COPY will not be what we want in certain corner
	 * cases involving ADD COLUMN and inheritance.)
	 */
	column_list = fmtCopyColumnList(tbinfo);

	if (oids && hasoids)
	{
		appendPQExpBuffer(q, "COPY %s %s WITH OIDS TO stdout IGNORE EXTERNAL PARTITIONS;",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname),
						  column_list);
	}
	else
	{
		appendPQExpBuffer(q, "COPY %s %s TO stdout IGNORE EXTERNAL PARTITIONS;",
						  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
										 classname),
						  column_list);
	}
	res = PQexec(g_conn, q->data);
	check_sql_result(res, g_conn, q->data, PGRES_COPY_OUT);
	PQclear(res);

	for (;;)
	{
		ret = PQgetCopyData(g_conn, &copybuf, 0);

		if (ret < 0)
			break;				/* done or error */

		if (copybuf)
		{
			WriteData(fout, copybuf, ret);
			PQfreemem(copybuf);
		}

		/*
		 * THROTTLE:
		 *
		 * There was considerable discussion in late July, 2000 regarding
		 * slowing down pg_dump when backing up large tables. Users with both
		 * slow & fast (multi-processor) machines experienced performance
		 * degradation when doing a backup.
		 *
		 * Initial attempts based on sleeping for a number of ms for each ms
		 * of work were deemed too complex, then a simple 'sleep in each loop'
		 * implementation was suggested. The latter failed because the loop
		 * was too tight. Finally, the following was implemented:
		 *
		 * If throttle is non-zero, then See how long since the last sleep.
		 * Work out how long to sleep (based on ratio). If sleep is more than
		 * 100ms, then sleep reset timer EndIf EndIf
		 *
		 * where the throttle value was the number of ms to sleep per ms of
		 * work. The calculation was done in each loop.
		 *
		 * Most of the hard work is done in the backend, and this solution
		 * still did not work particularly well: on slow machines, the ratio
		 * was 50:1, and on medium paced machines, 1:1, and on fast
		 * multi-processor machines, it had little or no effect, for reasons
		 * that were unclear.
		 *
		 * Further discussion ensued, and the proposal was dropped.
		 *
		 * For those people who want this feature, it can be implemented using
		 * gettimeofday in each loop, calculating the time since last sleep,
		 * multiplying that by the sleep ratio, then if the result is more
		 * than a preset 'minimum sleep time' (say 100ms), call the 'select'
		 * function to sleep for a sub period ie.
		 *
		 * select(0, NULL, NULL, NULL, &tvi);
		 *
		 * This will return after the interval specified in the structure tvi.
		 * Finally, call gettimeofday again to save the 'last sleep time'.
		 */
	}
	archprintf(fout, "\\.\n\n\n");

	if (ret == -2)
	{
		/* copy data transfer failed */
		mpp_err_msg(logError, progname, "Dumping the contents of table \"%s\" failed: PQgetCopyData() failed.\n", classname);
		mpp_err_msg(logError, progname, "Error message from server: %s", PQerrorMessage(g_conn));
		mpp_err_msg(logError, progname, "The command was: %s\n", q->data);
		exit_nicely();
	}

	/* Check command status and return to normal libpq state */
	res = PQgetResult(g_conn);
	check_sql_result(res, g_conn, q->data, PGRES_COMMAND_OK);
	PQclear(res);

	destroyPQExpBuffer(q);
	return 1;
}

static int
dumpTableData_insert(Archive *fout, void *dcontext)
{
	TableDataInfo *tdinfo = (TableDataInfo *) dcontext;
	TableInfo  *tbinfo = tdinfo->tdtable;
	const char *classname = tbinfo->dobj.name;
	PQExpBuffer q = createPQExpBuffer();
	PGresult   *res;
	int			tuple;
	int			nfields;
	int			field;

	/*
	 * Make sure we are in proper schema.  We will qualify the table name
	 * below anyway (in case its name conflicts with a pg_catalog table); but
	 * this ensures reproducible results in case the table contains regproc,
	 * regclass, etc columns.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "DECLARE _pg_dump_cursor CURSOR FOR "
					  "SELECT * FROM ONLY %s",
					  fmtQualifiedId(tbinfo->dobj.namespace->dobj.name,
									 classname));

	res = PQexec(g_conn, q->data);
	check_sql_result(res, g_conn, q->data, PGRES_COMMAND_OK);

	do
	{
		PQclear(res);

		res = PQexec(g_conn, "FETCH 100 FROM _pg_dump_cursor");
		check_sql_result(res, g_conn, "FETCH 100 FROM _pg_dump_cursor",
						 PGRES_TUPLES_OK);
		nfields = PQnfields(res);
		for (tuple = 0; tuple < PQntuples(res); tuple++)
		{
			archprintf(fout, "INSERT INTO %s ", fmtId(classname));
			if (nfields == 0)
			{
				/* corner case for zero-column table */
				archprintf(fout, "DEFAULT VALUES;\n");
				continue;
			}
			if (column_inserts)
			{
				resetPQExpBuffer(q);
				appendPQExpBuffer(q, "(");
				for (field = 0; field < nfields; field++)
				{
					if (field > 0)
						appendPQExpBuffer(q, ", ");
					appendPQExpBufferStr(q, fmtId(PQfname(res, field)));
				}
				appendPQExpBuffer(q, ") ");
				archputs(q->data, fout);
			}
			archprintf(fout, "VALUES (");
			for (field = 0; field < nfields; field++)
			{
				if (field > 0)
					archprintf(fout, ", ");
				if (PQgetisnull(res, tuple, field))
				{
					archprintf(fout, "NULL");
					continue;
				}

				/* XXX This code is partially duplicated in ruleutils.c */
				switch (PQftype(res, field))
				{
					case INT2OID:
					case INT4OID:
					case INT8OID:
					case OIDOID:
					case FLOAT4OID:
					case FLOAT8OID:
					case NUMERICOID:
						{
							/*
							 * These types are printed without quotes unless
							 * they contain values that aren't accepted by the
							 * scanner unquoted (e.g., 'NaN').	Note that
							 * strtod() and friends might accept NaN, so we
							 * can't use that to test.
							 *
							 * In reality we only need to defend against
							 * infinity and NaN, so we need not get too crazy
							 * about pattern matching here.
							 */
							const char *s = PQgetvalue(res, tuple, field);

							if (strspn(s, "0123456789 +-eE.") == strlen(s))
								archprintf(fout, "%s", s);
							else
								archprintf(fout, "'%s'", s);
						}
						break;

					case BITOID:
					case VARBITOID:
						archprintf(fout, "B'%s'",
								   PQgetvalue(res, tuple, field));
						break;

					case BOOLOID:
						if (strcmp(PQgetvalue(res, tuple, field), "t") == 0)
							archprintf(fout, "true");
						else
							archprintf(fout, "false");
						break;

					default:
						/* All other types are printed as string literals. */
						resetPQExpBuffer(q);
						appendStringLiteralAH(q,
											  PQgetvalue(res, tuple, field),
											  fout);
						archputs(q->data, fout);
						break;
				}
			}
			archprintf(fout, ");\n");
		}
	} while (PQntuples(res) > 0);

	PQclear(res);

	archprintf(fout, "\n\n");

	do_sql_command(g_conn, "CLOSE _pg_dump_cursor");

	destroyPQExpBuffer(q);
	return 1;
}


/*
 * dumpTableData -
 *	  dump the contents of a single table
 *
 * Actually, this just makes an ArchiveEntry for the table contents.
 */
static void
dumpTableData(Archive *fout, TableDataInfo *tdinfo)
{
	TableInfo  *tbinfo = tdinfo->tdtable;
	PQExpBuffer copyBuf = createPQExpBuffer();
	DataDumperPtr dumpFn;
	char	   *copyStmt;

	if (!dump_inserts)
	{
		/* Dump/restore using COPY */
		dumpFn = dumpTableData_copy;
		/* must use 2 steps here 'cause fmtId is nonreentrant */
		appendPQExpBuffer(copyBuf, "COPY %s ",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(copyBuf, "%s %sFROM stdin;\n",
						  fmtCopyColumnList(tbinfo),
					  (tdinfo->oids && tbinfo->hasoids) ? "WITH OIDS " : "");
		copyStmt = copyBuf->data;
	}
	else
	{
		/* Restore using INSERT */
		dumpFn = dumpTableData_insert;
		copyStmt = NULL;
	}

	ArchiveEntry(fout, tdinfo->dobj.catId, tdinfo->dobj.dumpId,
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "TABLE DATA", "", "", copyStmt,
				 tdinfo->dobj.dependencies, tdinfo->dobj.nDeps,
				 dumpFn, tdinfo);

	destroyPQExpBuffer(copyBuf);
}

/*
 * getTableData -
 *	  set up dumpable objects representing the contents of tables
 */
static void
getTableData(TableInfo *tblinfo, int numTables, bool oids)
{
	int			i;

	for (i = 0; i < numTables; i++)
	{
		/* Skip VIEWs (no data to dump) */
		if (tblinfo[i].relkind == RELKIND_VIEW)
			continue;
		/* START MPP ADDITION */
		/* Skip EXTERNAL TABLEs */
		if (tblinfo[i].relstorage == RELSTORAGE_EXTERNAL)
			continue;
		/* END MPP ADDITION */
		/* Skip SEQUENCEs (handled elsewhere) */
		if (tblinfo[i].relkind == RELKIND_SEQUENCE)
			continue;

		if (tblinfo[i].dobj.dump)
		{
			TableDataInfo *tdinfo;

			tdinfo = (TableDataInfo *) malloc(sizeof(TableDataInfo));

			tdinfo->dobj.objType = DO_TABLE_DATA;

			/*
			 * Note: use tableoid 0 so that this object won't be mistaken for
			 * something that pg_depend entries apply to.
			 */
			tdinfo->dobj.catId.tableoid = 0;
			tdinfo->dobj.catId.oid = tblinfo[i].dobj.catId.oid;
			AssignDumpId(&tdinfo->dobj);
			tdinfo->dobj.name = tblinfo[i].dobj.name;
			tdinfo->dobj.namespace = tblinfo[i].dobj.namespace;
			tdinfo->tdtable = &(tblinfo[i]);
			tdinfo->oids = oids;
			addObjectDependency(&tdinfo->dobj, tblinfo[i].dobj.dumpId);
		}
	}
}


/*
 * dumpDatabase:
 *	dump the database definition
 */
static void
dumpDatabase(Archive *AH)
{
	PQExpBuffer dbQry = createPQExpBuffer();
	PQExpBuffer delQry = createPQExpBuffer();
	PQExpBuffer createQry = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i_tableoid,
				i_oid,
				i_dba,
				i_encoding,
				i_tablespace;
	CatalogId	dbCatId;
	DumpId		dbDumpId;
	const char *datname,
			   *dba,
			   *encoding,
			   *tablespace;
	char	   *comment;

	datname = PQdb(g_conn);

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "saving database definition\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Get the database owner and parameters from pg_database */
	appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
					  "(%s datdba) as dba, "
					  "pg_encoding_to_char(encoding) as encoding, "
					  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) as tablespace, "
					  "shobj_description(oid, 'pg_database') as description "

					  "FROM pg_database "
					  "WHERE datname = ",
					  username_subquery);
	appendStringLiteralConn(dbQry, datname, g_conn);

	res = PQexec(g_conn, dbQry->data);
	check_sql_result(res, g_conn, dbQry->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	if (ntups <= 0)
	{
		mpp_err_msg(logError, progname, "missing pg_database entry for database \"%s\"\n",
					datname);
		exit_nicely();
	}

	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "query returned more than one (%d) pg_database entry for database \"%s\"\n",
					ntups, datname);
		exit_nicely();
	}

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_dba = PQfnumber(res, "dba");
	i_encoding = PQfnumber(res, "encoding");
	i_tablespace = PQfnumber(res, "tablespace");

	dbCatId.tableoid = atooid(PQgetvalue(res, 0, i_tableoid));
	dbCatId.oid = atooid(PQgetvalue(res, 0, i_oid));
	dba = PQgetvalue(res, 0, i_dba);
	encoding = PQgetvalue(res, 0, i_encoding);
	tablespace = PQgetvalue(res, 0, i_tablespace);

	appendPQExpBuffer(createQry, "CREATE DATABASE %s WITH TEMPLATE = template0",
					  fmtId(datname));
	if (strlen(encoding) > 0)
	{
		appendPQExpBuffer(createQry, " ENCODING = ");
		appendStringLiteralAH(createQry, encoding, AH);
	}
	if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0)
		appendPQExpBuffer(createQry, " TABLESPACE = %s",
						  fmtId(tablespace));
	appendPQExpBuffer(createQry, ";\n");

	appendPQExpBuffer(delQry, "DROP DATABASE %s;\n",
					  fmtId(datname));

	dbDumpId = createDumpId();

	ArchiveEntry(AH,
				 dbCatId,		/* catalog ID */
				 dbDumpId,		/* dump ID */
				 datname,		/* Name */
				 NULL,			/* Namespace */
				 NULL,			/* Tablespace */
				 dba,			/* Owner */
				 false,			/* with oids */
				 "DATABASE",	/* Desc */
				 createQry->data, /* Create */
				 delQry->data,	/* Del */
				 NULL,			/* Copy */
				 NULL,			/* Deps */
				 0,				/* # Deps */
				 NULL,			/* Dumper */
				 NULL);			/* Dumper Arg */

	/* Dump DB comment if any */

	/*
	 * 8.2 keeps comments on shared objects in a shared table, so we cannot
	 * use the dumpComment used for other database objects.
	 */
	comment = PQgetvalue(res, 0, PQfnumber(res, "description"));

	if (comment && strlen(comment))
	{
		resetPQExpBuffer(dbQry);
		appendPQExpBuffer(dbQry, "COMMENT ON DATABASE %s IS ", fmtId(datname));
		appendStringLiteralAH(dbQry, comment, AH);
		appendPQExpBuffer(dbQry, ";\n");

		ArchiveEntry(AH, dbCatId, createDumpId(), datname, NULL, NULL,
					 dba, false, "COMMENT", dbQry->data, "", NULL,
					 &dbDumpId, 1, NULL, NULL);
	}

	PQclear(res);

	destroyPQExpBuffer(dbQry);
	destroyPQExpBuffer(delQry);
	destroyPQExpBuffer(createQry);
}


/*
 * dumpEncoding: put the correct encoding into the archive
 */
static void
dumpEncoding(Archive *AH)
{
	const char *encname = pg_encoding_to_char(AH->encoding);
	PQExpBuffer qry = createPQExpBuffer();

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "saving encoding = %s\n", encname);

	appendPQExpBuffer(qry, "SET client_encoding = ");
	appendStringLiteralAH(qry, encname, AH);
	appendPQExpBuffer(qry, ";\n");

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "ENCODING", NULL, NULL, "",
				 false, "ENCODING", qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(qry);
}


/*
 * dumpStdStrings: put the correct escape string behavior into the archive
 */
static void
dumpStdStrings(Archive *AH)
{
	const char *stdstrings = AH->std_strings ? "on" : "off";
	PQExpBuffer qry = createPQExpBuffer();

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "saving standard_conforming_strings = %s\n",
					stdstrings);

	appendPQExpBuffer(qry, "SET standard_conforming_strings = '%s';\n",
					  stdstrings);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "STDSTRINGS", NULL, NULL, "",
				 false, "STDSTRINGS", qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(qry);
}

/*
 * Reset gp_default_storage_options GUC to factory defaults.  This
 * allows us to generate CREATE TABLE DMLs from pg_class.reloptions in
 * WITH () clause.
 */
static void
dumpAOStorageOpts(Archive *AH)
{
	PQExpBuffer qry = createPQExpBuffer();
	PGresult *res;
	if (g_verbose)
		mpp_err_msg(logInfo, progname, "initializing gp_default_storage_options"
					" GUC\n");

	res = PQexec(g_conn, "SET gp_default_storage_options=''");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		mpp_err_msg(logError, progname, "could not initialize "
					"gp_default_storage_options: %s\n", PQerrorMessage(g_conn));
		exit_nicely();
	}
	PQclear(res);
	res = PQexec(g_conn, "SHOW gp_default_storage_options");
	if (PQntuples(res) != 1)
	{
		mpp_err_msg(logError, progname, "could not read "
					"gp_default_storage_options: %s\n", PQerrorMessage(g_conn));
		exit_nicely();
	}
	appendPQExpBuffer(qry, "SET gp_default_storage_options = '%s';\n",
					  PQgetvalue(res, 0, 0));
	PQclear(res);
	ArchiveEntry(AH, nilCatalogId, createDumpId(), "AOSTORAGEOPTS",
				 NULL, NULL, "", false, "AOSTORAGEOPTS", qry->data, "",
				 NULL, NULL, 0, NULL, NULL);
	destroyPQExpBuffer(qry);
}

/*
 * hasBlobs:
 *	Test whether database contains any large objects
 */
static bool
hasBlobs(Archive *AH)
{
	bool		result;
	const char *blobQry;
	PGresult   *res;

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Check for BLOB OIDs */
	blobQry = "SELECT loid FROM pg_largeobject LIMIT 1";

	res = PQexec(g_conn, blobQry);
	check_sql_result(res, g_conn, blobQry, PGRES_TUPLES_OK);

	result = PQntuples(res) > 0;

	PQclear(res);

	return result;
}

/*
 * dumpBlobs:
 *	dump all blobs
 */
static int
dumpBlobs(Archive *AH, void *arg __attribute__((unused)))
{
	const char *blobQry;
	const char *blobFetchQry;
	PGresult   *res;
	char		buf[LOBBUFSIZE];
	int			i;
	int			cnt;

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "saving large objects\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Cursor to get all BLOB OIDs */
	blobQry = "DECLARE bloboid CURSOR FOR SELECT DISTINCT loid FROM pg_largeobject";

	res = PQexec(g_conn, blobQry);
	check_sql_result(res, g_conn, blobQry, PGRES_COMMAND_OK);

	/* Command to fetch from cursor */
	blobFetchQry = "FETCH 1000 IN bloboid";

	do
	{
		PQclear(res);

		/* Do a fetch */
		res = PQexec(g_conn, blobFetchQry);
		check_sql_result(res, g_conn, blobFetchQry, PGRES_TUPLES_OK);

		/* Process the tuples, if any */
		for (i = 0; i < PQntuples(res); i++)
		{
			Oid			blobOid;
			int			loFd;

			blobOid = atooid(PQgetvalue(res, i, 0));
			/* Open the BLOB */
			loFd = lo_open(g_conn, blobOid, INV_READ);
			if (loFd == -1)
			{
				mpp_err_msg(logError, progname, "dumpBlobs(): could not open large object: %s",
							PQerrorMessage(g_conn));
				exit_nicely();
			}

			StartBlob(AH, blobOid);

			/* Now read it in chunks, sending data to archive */
			do
			{
				cnt = lo_read(g_conn, loFd, buf, LOBBUFSIZE);
				if (cnt < 0)
				{
					mpp_err_msg(logError, progname, "dumpBlobs(): error reading large object: %s",
								PQerrorMessage(g_conn));
					exit_nicely();
				}

				WriteData(AH, buf, cnt);
			} while (cnt > 0);

			lo_close(g_conn, loFd);

			EndBlob(AH, blobOid);
		}
	} while (PQntuples(res) > 0);

	PQclear(res);

	return 1;
}

/*
 * dumpBlobComments
 *	dump all blob comments
 *
 * Since we don't provide any way to be selective about dumping blobs,
 * there's no need to be selective about their comments either.  We put
 * all the comments into one big TOC entry.
 */
static int
dumpBlobComments(Archive *AH, void *arg __attribute__((unused)))
{
	const char *blobQry;
	const char *blobFetchQry;
	PQExpBuffer commentcmd = createPQExpBuffer();
	PGresult   *res;
	int			i;

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "saving large object comments\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	/* Cursor to get all BLOB comments */
	blobQry = "DECLARE blobcmt CURSOR FOR SELECT loid, obj_description(loid, 'pg_largeobject') FROM (SELECT DISTINCT loid FROM pg_largeobject) ss";

	res = PQexec(g_conn, blobQry);
	check_sql_result(res, g_conn, blobQry, PGRES_COMMAND_OK);

	/* Command to fetch from cursor */
	blobFetchQry = "FETCH 100 IN blobcmt";

	do
	{
		PQclear(res);

		/* Do a fetch */
		res = PQexec(g_conn, blobFetchQry);
		check_sql_result(res, g_conn, blobFetchQry, PGRES_TUPLES_OK);

		/* Process the tuples, if any */
		for (i = 0; i < PQntuples(res); i++)
		{
			Oid			blobOid;
			char	   *comment;

			/* ignore blobs without comments */
			if (PQgetisnull(res, i, 1))
				continue;

			blobOid = atooid(PQgetvalue(res, i, 0));
			comment = PQgetvalue(res, i, 1);

			printfPQExpBuffer(commentcmd, "COMMENT ON LARGE OBJECT %u IS ",
							  blobOid);
			appendStringLiteralAH(commentcmd, comment, AH);
			appendPQExpBuffer(commentcmd, ";\n");

			archputs(commentcmd->data, AH);
		}
	} while (PQntuples(res) > 0);

	PQclear(res);

	archputs("\n", AH);

	destroyPQExpBuffer(commentcmd);

	return 1;
}


/*
 * dumpComment --
 *
 * This routine is used to dump any comments associated with the
 * object handed to this routine. The routine takes a constant character
 * string for the target part of the comment-creation command, plus
 * the namespace and owner of the object (for labeling the ArchiveEntry),
 * plus catalog ID and subid which are the lookup key for pg_description,
 * plus the dump ID for the object (for setting a dependency).
 * If a matching pg_description entry is found, it is dumped.
 *
 * Note: although this routine takes a dumpId for dependency purposes,
 * that purpose is just to mark the dependency in the emitted dump file
 * for possible future use by pg_restore.  We do NOT use it for determining
 * ordering of the comment in the dump file, because this routine is called
 * after dependency sorting occurs.  This routine should be called just after
 * calling ArchiveEntry() for the specified object.
 */
static void
dumpComment(Archive *fout, const char *target,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId)
{
	CommentItem *comments;
	int			ncomments;

	/* Comments are SCHEMA not data */
	if (dataOnly)
		return;

	/* Search for comments associated with catalogId, using table */
	ncomments = findComments(fout, catalogId.tableoid, catalogId.oid,
							 &comments);

	/* Is there one matching the subid? */
	while (ncomments > 0)
	{
		if (comments->objsubid == subid)
			break;
		comments++;
		ncomments--;
	}

	/* If a comment exists, build COMMENT ON statement */
	if (ncomments > 0)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "COMMENT ON %s IS ", target);
		appendStringLiteralAH(query, comments->descr, fout);
		appendPQExpBuffer(query, ";\n");

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 target, namespace, NULL, owner, false,
					 "COMMENT", query->data, "", NULL,
					 &(dumpId), 1,
					 NULL, NULL);

		destroyPQExpBuffer(query);
	}
}

/*
 * dumpTableComment --
 *
 * As above, but dump comments for both the specified table (or view)
 * and its columns.
 */
static void
dumpTableComment(Archive *fout, TableInfo *tbinfo,
				 const char *reltypename)
{
	CommentItem *comments;
	int			ncomments;
	PQExpBuffer query;
	PQExpBuffer target;

	/* Comments are SCHEMA not data */
	if (dataOnly)
		return;

	/* Search for comments associated with relation, using table */
	ncomments = findComments(fout,
							 tbinfo->dobj.catId.tableoid,
							 tbinfo->dobj.catId.oid,
							 &comments);

	/* If comments exist, build COMMENT ON statements */
	if (ncomments <= 0)
		return;

	query = createPQExpBuffer();
	target = createPQExpBuffer();

	while (ncomments > 0)
	{
		const char *descr = comments->descr;
		int			objsubid = comments->objsubid;

		if (objsubid == 0)
		{
			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "%s %s", reltypename,
							  fmtId(tbinfo->dobj.name));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s IS ", target->data);
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBuffer(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname,
						 false, "COMMENT", query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}
		else if (objsubid > 0 && objsubid <= tbinfo->numatts)
		{
			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "COLUMN %s.",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(target, "%s",
							  fmtId(tbinfo->attnames[objsubid - 1]));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s IS ", target->data);
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBuffer(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname,
						 false, "COMMENT", query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}

		comments++;
		ncomments--;
	}

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(target);
}

/*
 * findComments --
 *
 * Find the comment(s), if any, associated with the given object.  All the
 * objsubid values associated with the given classoid/objoid are found with
 * one search.
 */
static int
findComments(Archive *fout, Oid classoid, Oid objoid,
			 CommentItem **items)
{
	/* static storage for table of comments */
	static CommentItem *comments = NULL;
	static int	ncomments = -1;

	CommentItem *middle = NULL;
	CommentItem *low;
	CommentItem *high;
	int			nmatch;

	/* Get comments if we didn't already */
	if (ncomments < 0)
		ncomments = collectComments(fout, &comments);

	/*
	 * Do binary search to find some item matching the object.
	 */
	low = &comments[0];
	high = &comments[ncomments - 1];
	while (low <= high)
	{
		middle = low + (high - low) / 2;

		if (classoid < middle->classoid)
			high = middle - 1;
		else if (classoid > middle->classoid)
			low = middle + 1;
		else if (objoid < middle->objoid)
			high = middle - 1;
		else if (objoid > middle->objoid)
			low = middle + 1;
		else
			break;				/* found a match */
	}

	if (low > high)				/* no matches */
	{
		*items = NULL;
		return 0;
	}

	/*
	 * Now determine how many items match the object.  The search loop
	 * invariant still holds: only items between low and high inclusive could
	 * match.
	 */
	nmatch = 1;
	while (middle > low)
	{
		if (classoid != middle[-1].classoid ||
			objoid != middle[-1].objoid)
			break;
		middle--;
		nmatch++;
	}

	*items = middle;

	middle += nmatch;
	while (middle <= high)
	{
		if (classoid != middle->classoid ||
			objoid != middle->objoid)
			break;
		middle++;
		nmatch++;
	}

	return nmatch;
}

/*
 * collectComments --
 *
 * Construct a table of all comments available for database objects.
 * We used to do per-object queries for the comments, but it's much faster
 * to pull them all over at once, and on most databases the memory cost
 * isn't high.
 *
 * The table is sorted by classoid/objid/objsubid for speed in lookup.
 */
static int
collectComments(Archive *fout, CommentItem **items)
{
	PGresult   *res;
	PQExpBuffer query;
	int			i_description;
	int			i_classoid;
	int			i_objoid;
	int			i_objsubid;
	int			ntups;
	int			i;
	CommentItem *comments;

	/*
	 * Note we do NOT change source schema here; preserve the caller's
	 * setting, instead.
	 */

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT description, classoid, objoid, objsubid "
					  "FROM pg_catalog.pg_description "
					  "ORDER BY classoid, objoid, objsubid");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Construct lookup table containing OIDs in numeric form */

	i_description = PQfnumber(res, "description");
	i_classoid = PQfnumber(res, "classoid");
	i_objoid = PQfnumber(res, "objoid");
	i_objsubid = PQfnumber(res, "objsubid");

	ntups = PQntuples(res);

	comments = (CommentItem *) malloc(ntups * sizeof(CommentItem));

	for (i = 0; i < ntups; i++)
	{
		comments[i].descr = PQgetvalue(res, i, i_description);
		comments[i].classoid = atooid(PQgetvalue(res, i, i_classoid));
		comments[i].objoid = atooid(PQgetvalue(res, i, i_objoid));
		comments[i].objsubid = atoi(PQgetvalue(res, i, i_objsubid));
	}

	/* Do NOT free the PGresult since we are keeping pointers into it */
	destroyPQExpBuffer(query);

	*items = comments;
	return ntups;
}

/*
 * dumpDumpableObject
 *
 * This routine and its subsidiaries are responsible for creating
 * ArchiveEntries (TOC objects) for each object to be dumped.
 */
static void
dumpDumpableObject(Archive *fout, DumpableObject *dobj)
{
	/* mpp addition start */
	/* skip entries for mpp catalog */
	if (selectSchemaName == NULL)
		if ((dobj->objType == DO_NAMESPACE && strcmp(dobj->name, "gp_") == 0) ||
			(dobj->namespace != NULL && strcmp(dobj->namespace->dobj.name, "gp_") == 0))
			return;
	/* end mpp addition */

	switch (dobj->objType)
	{
		case DO_NAMESPACE:
			if (!postDataSchemaOnly)
				dumpNamespace(fout, (NamespaceInfo *) dobj);
			break;
		case DO_EXTENSION:
			if (!postDataSchemaOnly)
				dumpExtension(fout, (ExtensionInfo *) dobj);
			break;
		case DO_TYPE:
			if (!postDataSchemaOnly)
			dumpType(fout, (TypeInfo *) dobj);
			break;
		case DO_TYPE_STORAGE_OPTIONS:
			if (!postDataSchemaOnly)
				dumpTypeStorageOptions(fout, (TypeStorageOptions *) dobj);
			break;
		case DO_SHELL_TYPE:
			if (!postDataSchemaOnly)
			dumpShellType(fout, (ShellTypeInfo *) dobj);
			break;
		case DO_FUNC:
			if (!postDataSchemaOnly)
			dumpFunc(fout, (FuncInfo *) dobj);
			break;
		case DO_AGG:
			if (!postDataSchemaOnly)
			dumpAgg(fout, (AggInfo *) dobj);
			break;
		case DO_OPERATOR:
			if (!postDataSchemaOnly)
			dumpOpr(fout, (OprInfo *) dobj);
			break;
		case DO_EXTPROTOCOL:
			if (!postDataSchemaOnly)
			dumpExtProtocol(fout, (ExtProtInfo *) dobj);
			break;
		case DO_OPCLASS:
			if (!postDataSchemaOnly)
			dumpOpclass(fout, (OpclassInfo *) dobj);
			break;
		case DO_OPFAMILY:
			dumpOpfamily(fout, (OpfamilyInfo *) dobj);
			break;
		case DO_CONVERSION:
			if (!postDataSchemaOnly)
			dumpConversion(fout, (ConvInfo *) dobj);
			break;
		case DO_TABLE:
			if (!postDataSchemaOnly)
			dumpTable(fout, (TableInfo *) dobj);
			break;
		case DO_ATTRDEF:
			if (!postDataSchemaOnly)
			dumpAttrDef(fout, (AttrDefInfo *) dobj);
			break;
		case DO_INDEX:
			if (!preDataSchemaOnly)
			dumpIndex(fout, (IndxInfo *) dobj);
			break;
		case DO_RULE:
			if (!preDataSchemaOnly)
			dumpRule(fout, (RuleInfo *) dobj);
			break;
		case DO_TRIGGER:
			if (!preDataSchemaOnly)
			dumpTrigger(fout, (TriggerInfo *) dobj);
			break;
		case DO_CONSTRAINT:
			if (!preDataSchemaOnly)
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_FK_CONSTRAINT:
			if (!preDataSchemaOnly)
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_PROCLANG:
			if (!postDataSchemaOnly)
			dumpProcLang(fout, (ProcLangInfo *) dobj);
			break;
		case DO_CAST:
			if (!postDataSchemaOnly)
			dumpCast(fout, (CastInfo *) dobj);
			break;
		case DO_TABLE_DATA:
			if (!postDataSchemaOnly)
			dumpTableData(fout, (TableDataInfo *) dobj);
			break;
		case DO_DUMMY_TYPE:
			/* table rowtypes and array types are never dumped separately */
			break;
		case DO_TSPARSER:
			dumpTSParser(fout, (TSParserInfo *) dobj);
			break;
		case DO_TSDICT:
			dumpTSDictionary(fout, (TSDictInfo *) dobj);
			break;
		case DO_TSTEMPLATE:
			dumpTSTemplate(fout, (TSTemplateInfo *) dobj);
			break;
		case DO_TSCONFIG:
			dumpTSConfig(fout, (TSConfigInfo *) dobj);
			break;
		case DO_BLOBS:
			if (!postDataSchemaOnly)
			ArchiveEntry(fout, dobj->catId, dobj->dumpId,
						 dobj->name, NULL, NULL, "",
						 false, "BLOBS", "", "", NULL,
						 NULL, 0,
						 dumpBlobs, NULL);
			break;
		case DO_BLOB_COMMENTS:
			if (!postDataSchemaOnly)
			ArchiveEntry(fout, dobj->catId, dobj->dumpId,
						 dobj->name, NULL, NULL, "",
						 false, "BLOB COMMENTS", "", "", NULL,
						 NULL, 0,
						 dumpBlobComments, NULL);
			break;
		/*
		 * The TYPE_CACHE object is only used for the pg_type cache during
		 * binary_upgrade operation and should not be dumped. To keep the
		 * compilers and static analyzers happy we still need to handle
		 * the case though.
		 */
		case DO_TYPE_CACHE:
			break;
	}
}

/*
 * dumpNamespace
 *	  writes out to fout the queries to recreate a user-defined namespace
 */
static void
dumpNamespace(Archive *fout, NamespaceInfo *nspinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *qnspname;

	/* Skip if not to be dumped */
	if (!nspinfo->dobj.dump || dataOnly)
		return;

	/* don't dump dummy namespace from pre-7.3 source */
	if (strlen(nspinfo->dobj.name) == 0)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qnspname = pg_strdup(fmtId(nspinfo->dobj.name));

	appendPQExpBuffer(delq, "DROP SCHEMA %s;\n", qnspname);

	appendPQExpBuffer(q, "CREATE SCHEMA %s;\n", qnspname);

	ArchiveEntry(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId,
				 nspinfo->dobj.name,
				 NULL, NULL,
				 nspinfo->rolname,
				 false, "SCHEMA", q->data, delq->data, NULL,
				 nspinfo->dobj.dependencies, nspinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Schema Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "SCHEMA %s", qnspname);
	dumpComment(fout, q->data,
				NULL, nspinfo->rolname,
				nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);

	dumpACL(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId, "SCHEMA",
			qnspname, nspinfo->dobj.name, NULL,
			nspinfo->rolname, nspinfo->nspacl);

	free(qnspname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpExtension
 *	  writes out to fout the queries to recreate an extension
 */
static void
dumpExtension(Archive *fout, ExtensionInfo *extinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer labelq;
	char	   *qextname;

	/* Skip if not to be dumped */
	if (!extinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	labelq = createPQExpBuffer();

	qextname = strdup(fmtId(extinfo->dobj.name));

	appendPQExpBuffer(delq, "DROP EXTENSION %s;\n", qextname);
	appendPQExpBuffer(q, "CREATE EXTENSION IF NOT EXISTS %s WITH SCHEMA %s;\n",
					  qextname, fmtId(extinfo->namespace));

	appendPQExpBuffer(labelq, "EXTENSION %s", qextname);

	ArchiveEntry(fout, extinfo->dobj.catId, extinfo->dobj.dumpId,
				 extinfo->dobj.name,
				 NULL, NULL,
				 "",
				 false, "EXTENSION",
				 q->data, delq->data, NULL,
				 extinfo->dobj.dependencies, extinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Extension Comments and Security Labels */
	dumpComment(fout, labelq->data,
				NULL, "",
				extinfo->dobj.catId, 0, extinfo->dobj.dumpId);

	free(qextname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(labelq);
}

/*
 * dumpType
 *	  writes out to fout the queries to recreate a user-defined type
 */
static void
dumpType(Archive *fout, TypeInfo *tinfo)
{
	/* Skip if not to be dumped */
	if (!tinfo->dobj.dump || dataOnly)
		return;

	/* Dump out in proper style */
	if (tinfo->typtype == 'b')
		dumpBaseType(fout, tinfo);
	else if (tinfo->typtype == 'd')
		dumpDomain(fout, tinfo);
	else if (tinfo->typtype == 'c')
		dumpCompositeType(fout, tinfo);
}

/*
 * dumpBaseType
 *	  writes out to fout the queries to recreate a user-defined base type
 */
static void
dumpBaseType(Archive *fout, TypeInfo *tinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	char	   *typlen;
	char	   *typinput;
	char	   *typoutput;
	char	   *typreceive;
	char	   *typsend;
	char	   *typanalyze;
	Oid			typinputoid;
	Oid			typoutputoid;
	Oid			typreceiveoid;
	Oid			typsendoid;
	Oid			typanalyzeoid;
	char	   *typdelim;
	char	   *typbyval;
	char	   *typalign;
	char	   *typstorage;
	char	   *typdefault;
	bool		typdefault_is_literal = false;

	/* Set proper schema search path so regproc references list correctly */
	selectSourceSchema(tinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(query, "SELECT typlen, "
					  "typinput, typoutput, typreceive, typsend, "
					  "typanalyze, "
					  "typinput::pg_catalog.oid as typinputoid, "
					  "typoutput::pg_catalog.oid as typoutputoid, "
					  "typreceive::pg_catalog.oid as typreceiveoid, "
					  "typsend::pg_catalog.oid as typsendoid, "
					  "typanalyze::pg_catalog.oid as typanalyzeoid, "
					  "typdelim, typbyval, typalign, typstorage, "
					  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) as typdefaultbin, typdefault "
					  "FROM pg_catalog.pg_type "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  tinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	typlen = PQgetvalue(res, 0, PQfnumber(res, "typlen"));
	typinput = PQgetvalue(res, 0, PQfnumber(res, "typinput"));
	typoutput = PQgetvalue(res, 0, PQfnumber(res, "typoutput"));
	typreceive = PQgetvalue(res, 0, PQfnumber(res, "typreceive"));
	typsend = PQgetvalue(res, 0, PQfnumber(res, "typsend"));
	typanalyze = PQgetvalue(res, 0, PQfnumber(res, "typanalyze"));
	typinputoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typinputoid")));
	typoutputoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typoutputoid")));
	typreceiveoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typreceiveoid")));
	typsendoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typsendoid")));
	typanalyzeoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typanalyzeoid")));
	typdelim = PQgetvalue(res, 0, PQfnumber(res, "typdelim"));
	typbyval = PQgetvalue(res, 0, PQfnumber(res, "typbyval"));
	typalign = PQgetvalue(res, 0, PQfnumber(res, "typalign"));
	typstorage = PQgetvalue(res, 0, PQfnumber(res, "typstorage"));
	if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
	else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
	{
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
		typdefault_is_literal = true;	/* it needs quotes */
	}
	else
		typdefault = NULL;

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog.
	 * The reason we include CASCADE is that the circular dependency between
	 * the type and its I/O functions makes it impossible to drop the type any
	 * other way.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s.",
					  fmtId(tinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s CASCADE;\n",
					  fmtId(tinfo->dobj.name));

	appendPQExpBuffer(q,
					  "CREATE TYPE %s (\n"
					  "    INTERNALLENGTH = %s",
					  fmtId(tinfo->dobj.name),
					  (strcmp(typlen, "-1") == 0) ? "variable" : typlen);

	/* regproc result is correctly quoted as of 7.3 */
	appendPQExpBuffer(q, ",\n    INPUT = %s", typinput);
	appendPQExpBuffer(q, ",\n    OUTPUT = %s", typoutput);
	if (OidIsValid(typreceiveoid))
		appendPQExpBuffer(q, ",\n    RECEIVE = %s", typreceive);
	if (OidIsValid(typsendoid))
		appendPQExpBuffer(q, ",\n    SEND = %s", typsend);
	if (OidIsValid(typanalyzeoid))
		appendPQExpBuffer(q, ",\n    ANALYZE = %s", typanalyze);

	if (typdefault != NULL)
	{
		appendPQExpBuffer(q, ",\n    DEFAULT = ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	if (tinfo->isArray)
	{
		char	   *elemType;

		/* reselect schema in case changed by function dump */
		selectSourceSchema(tinfo->dobj.namespace->dobj.name);
		elemType = getFormattedTypeName(tinfo->typelem, zeroAsOpaque);
		appendPQExpBuffer(q, ",\n    ELEMENT = %s", elemType);
		free(elemType);
	}

	if (typdelim && strcmp(typdelim, ",") != 0)
	{
		appendPQExpBuffer(q, ",\n    DELIMITER = ");
		appendStringLiteralAH(q, typdelim, fout);
	}

	if (strcmp(typalign, "c") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = char");
	else if (strcmp(typalign, "s") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = int2");
	else if (strcmp(typalign, "i") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = int4");
	else if (strcmp(typalign, "d") == 0)
		appendPQExpBuffer(q, ",\n    ALIGNMENT = double");

	if (strcmp(typstorage, "p") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = plain");
	else if (strcmp(typstorage, "e") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = external");
	else if (strcmp(typstorage, "x") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = extended");
	else if (strcmp(typstorage, "m") == 0)
		appendPQExpBuffer(q, ",\n    STORAGE = main");

	if (strcmp(typbyval, "t") == 0)
		appendPQExpBuffer(q, ",\n    PASSEDBYVALUE");

	appendPQExpBuffer(q, "\n);\n");

	ArchiveEntry(fout, tinfo->dobj.catId, tinfo->dobj.dumpId,
				 tinfo->dobj.name,
				 tinfo->dobj.namespace->dobj.name,
				 NULL,
				 tinfo->rolname, false,
				 "TYPE", q->data, delq->data, NULL,
				 tinfo->dobj.dependencies, tinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Type Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "TYPE %s", fmtId(tinfo->dobj.name));
	dumpComment(fout, q->data,
				tinfo->dobj.namespace->dobj.name, tinfo->rolname,
				tinfo->dobj.catId, 0, tinfo->dobj.dumpId);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}



/*
 * dumpTypeStorageOptions
 *     writes out to fout the ALTER TYPE queries to set default storage options for type
 */
static void
dumpTypeStorageOptions(Archive *fout, TypeStorageOptions *tstorageoptions)
{
	PQExpBuffer q;
	PQExpBuffer delq;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Set proper schema search path so regproc references list correctly */
	selectSourceSchema(tstorageoptions->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "ALTER TYPE %s ", tstorageoptions->dobj.name);
	appendPQExpBuffer(q, " SET DEFAULT ENCODING (%s);\n", tstorageoptions->typoptions);

	ArchiveEntry(	fout
							, tstorageoptions->dobj.catId                 /* catalog ID  */
							, tstorageoptions->dobj.dumpId                /* dump ID     */
							, tstorageoptions->dobj.name                  /* type name   */
							, tstorageoptions->dobj.namespace->dobj.name  /* name space  */
							, NULL                                        /* table space */
							, tstorageoptions->rolname                    /* owner name  */
							, false                                       /* with oids   */
							, "TYPE STORAGE OPTIONS"                      /* Desc        */
							, q->data                                     /* ALTER...    */
							, ""                                          /* Del         */
							, NULL                                        /* Copy        */
							, NULL                                        /* Deps        */
							, 0                                           /* num Deps    */
							, NULL                                        /* Dumper      */
							, NULL                                        /* Dumper Arg  */
							);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);


}  /* end dumpTypeStorageOptions */



/*
 * dumpDomain
 *	  writes out to fout the queries to recreate a user-defined domain
 */
static void
dumpDomain(Archive *fout, TypeInfo *tinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i;
	char	   *typnotnull;
	char	   *typdefn;
	char	   *typdefault;
	bool		typdefault_is_literal = false;

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(tinfo->dobj.namespace->dobj.name);

	/* Fetch domain specific details */
	appendPQExpBuffer(query, "SELECT typnotnull, "
				"pg_catalog.format_type(typbasetype, typtypmod) as typdefn, "
					  "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) as typdefaultbin, typdefault "
					  "FROM pg_catalog.pg_type "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  tinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	typnotnull = PQgetvalue(res, 0, PQfnumber(res, "typnotnull"));
	typdefn = PQgetvalue(res, 0, PQfnumber(res, "typdefn"));
	if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
	else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
	{
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
		typdefault_is_literal = true;	/* it needs quotes */
	}
	else
		typdefault = NULL;

	appendPQExpBuffer(q,
					  "CREATE DOMAIN %s AS %s",
					  fmtId(tinfo->dobj.name),
					  typdefn);

	if (typnotnull[0] == 't')
		appendPQExpBuffer(q, " NOT NULL");

	if (typdefault != NULL)
	{
		appendPQExpBuffer(q, " DEFAULT ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	PQclear(res);

	/*
	 * Add any CHECK constraints for the domain
	 */
	for (i = 0; i < tinfo->nDomChecks; i++)
	{
		ConstraintInfo *domcheck = &(tinfo->domChecks[i]);

		if (!domcheck->separate)
			appendPQExpBuffer(q, "\n\tCONSTRAINT %s %s",
							  fmtId(domcheck->dobj.name), domcheck->condef);
	}

	appendPQExpBuffer(q, ";\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP DOMAIN %s.",
					  fmtId(tinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s;\n",
					  fmtId(tinfo->dobj.name));

	ArchiveEntry(fout, tinfo->dobj.catId, tinfo->dobj.dumpId,
				 tinfo->dobj.name,
				 tinfo->dobj.namespace->dobj.name,
				 NULL,
				 tinfo->rolname, false,
				 "DOMAIN", q->data, delq->data, NULL,
				 tinfo->dobj.dependencies, tinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Domain Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "DOMAIN %s", fmtId(tinfo->dobj.name));
	dumpComment(fout, q->data,
				tinfo->dobj.namespace->dobj.name, tinfo->rolname,
				tinfo->dobj.catId, 0, tinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpCompositeType
 *	  writes out to fout the queries to recreate a user-defined stand-alone
 *	  composite type
 */
static void
dumpCompositeType(Archive *fout, TypeInfo *tinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			i_attname;
	int			i_atttypdefn;
	int			i;

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(tinfo->dobj.namespace->dobj.name);

	/* Fetch type specific details */

	appendPQExpBuffer(query, "SELECT a.attname, "
			 "pg_catalog.format_type(a.atttypid, a.atttypmod) as atttypdefn "
					  "FROM pg_catalog.pg_type t, pg_catalog.pg_attribute a "
					  "WHERE t.oid = '%u'::pg_catalog.oid "
					  "AND a.attrelid = t.typrelid "
					  "AND NOT a.attisdropped "
					  "ORDER BY a.attnum ",
					  tinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting at least a single result */
	ntups = PQntuples(res);
	if (ntups < 1)
	{
		mpp_err_msg(logError, progname, "query yielded no rows: %s\n", query->data);
		exit_nicely();
	}

	i_attname = PQfnumber(res, "attname");
	i_atttypdefn = PQfnumber(res, "atttypdefn");

	appendPQExpBuffer(q, "CREATE TYPE %s AS (",
					  fmtId(tinfo->dobj.name));

	for (i = 0; i < ntups; i++)
	{
		char	   *attname;
		char	   *atttypdefn;

		attname = PQgetvalue(res, i, i_attname);
		atttypdefn = PQgetvalue(res, i, i_atttypdefn);

		appendPQExpBuffer(q, "\n\t%s %s", fmtId(attname), atttypdefn);
		if (i < ntups - 1)
			appendPQExpBuffer(q, ",");
	}
	appendPQExpBuffer(q, "\n);\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s.",
					  fmtId(tinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s;\n",
					  fmtId(tinfo->dobj.name));

	ArchiveEntry(fout, tinfo->dobj.catId, tinfo->dobj.dumpId,
				 tinfo->dobj.name,
				 tinfo->dobj.namespace->dobj.name,
				 NULL,
				 tinfo->rolname, false,
				 "TYPE", q->data, delq->data, NULL,
				 tinfo->dobj.dependencies, tinfo->dobj.nDeps,
				 NULL, NULL);


	/* Dump Type Comments */
	resetPQExpBuffer(q);

	appendPQExpBuffer(q, "TYPE %s", fmtId(tinfo->dobj.name));
	dumpComment(fout, q->data,
				tinfo->dobj.namespace->dobj.name, tinfo->rolname,
				tinfo->dobj.catId, 0, tinfo->dobj.dumpId);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpShellType
 *	  writes out to fout the queries to create a shell type
 *
 * We dump a shell definition in advance of the I/O functions for the type.
 */
static void
dumpShellType(Archive *fout, ShellTypeInfo *stinfo)
{
	PQExpBuffer q;

	/* Skip if not to be dumped */
	if (!stinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();

	/*
	 * Note the lack of a DROP command for the shell type; any required DROP
	 * is driven off the base type entry, instead.	This interacts with
	 * _printTocEntry()'s use of the presence of a DROP command to decide
	 * whether an entry needs an ALTER OWNER command.  We don't want to alter
	 * the shell type's owner immediately on creation; that should happen only
	 * after it's filled in, otherwise the backend complains.
	 */

	appendPQExpBuffer(q, "CREATE TYPE %s;\n",
					  fmtId(stinfo->dobj.name));

	ArchiveEntry(fout, stinfo->dobj.catId, stinfo->dobj.dumpId,
				 stinfo->dobj.name,
				 stinfo->dobj.namespace->dobj.name,
				 NULL,
				 stinfo->baseType->rolname, false,
				 "SHELL TYPE", q->data, "", NULL,
				 stinfo->dobj.dependencies, stinfo->dobj.nDeps,
				 NULL, NULL);

	destroyPQExpBuffer(q);
}

/*
 * Determine whether we want to dump definitions for procedural languages.
 * Since the languages themselves don't have schemas, we can't rely on
 * the normal schema-based selection mechanism.  We choose not to dump them
 * if --table was given.  We still want to dump then when --schema was
 * given, as there may be functions in the schema that require a procedural
 * language.
 *
 * For some backwards compatibility with the older behavior, we forcibly
 * dump a PL if its handler function (and validator if any) are in a
 * dumpable namespace.	That case is not checked here.
 */
static bool
shouldDumpProcLangs(void)
{
	/* If we have a table filter, don't dump languages */
	if (table_include_patterns.head != NULL || table_include_patterns.tail != NULL
		|| table_exclude_patterns.head != NULL || table_exclude_patterns.tail != NULL)
		return false;
	/* And they're schema not data */
	if (dataOnly)
		return false;
	return true;
}

/*
 * dumpProcLang
 *		  writes out to fout the queries to recreate a user-defined
 *		  procedural language
 */
static void
dumpProcLang(Archive *fout, ProcLangInfo *plang)
{
	PQExpBuffer defqry;
	PQExpBuffer delqry;
	bool		useParams;
	char	   *qlanname;
	char	   *lanschema;
	FuncInfo   *funcInfo;
	FuncInfo   *inlineInfo = NULL;
	FuncInfo   *validatorInfo = NULL;

	if (!plang->dobj.dump ||dataOnly)
		return;

	/*
	 * Try to find the support function(s).  It is not an error if we don't
	 * find them --- if the functions are in the pg_catalog schema, as is
	 * standard in 8.1 and up, then we won't have loaded them. (In this case
	 * we will emit a parameterless CREATE LANGUAGE command, which will
	 * require PL template knowledge in the backend to reload.)
	 */

	funcInfo = findFuncByOid(plang->lanplcallfoid);
	if (funcInfo != NULL && !funcInfo->dobj.dump)
		funcInfo = NULL;		/* treat not-dumped same as not-found */

	if (OidIsValid(plang->lanvalidator))
	{
		validatorInfo = findFuncByOid(plang->lanvalidator);
		if (validatorInfo != NULL && !validatorInfo->dobj.dump)
			validatorInfo = NULL;
	}

	if (OidIsValid(plang->laninline))
	{
		inlineInfo = findFuncByOid(plang->laninline);
		if (inlineInfo != NULL && !inlineInfo->dobj.dump)
			inlineInfo = NULL;
	}
	/*
	 * If the functions are dumpable then emit a traditional CREATE LANGUAGE
	 * with parameters.  Otherwise, dump only if shouldDumpProcLangs() says to
	 * dump it.
	 */
	useParams = (funcInfo != NULL &&
				 (inlineInfo != NULL || !OidIsValid(plang->laninline)) &&
				 (validatorInfo != NULL || !OidIsValid(plang->lanvalidator)));

	if (!useParams && !shouldDumpProcLangs())
		return;

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();

	qlanname = pg_strdup(fmtId(plang->dobj.name));

	/*
	 * If dumping a HANDLER clause, treat the language as being in the handler
	 * function's schema; this avoids cluttering the HANDLER clause. Otherwise
	 * it doesn't really have a schema.
	 */
	if (useParams)
		lanschema = funcInfo->dobj.namespace->dobj.name;
	else
		lanschema = NULL;

	appendPQExpBuffer(delqry, "DROP PROCEDURAL LANGUAGE %s;\n",
					  qlanname);

	appendPQExpBuffer(defqry, "CREATE %sPROCEDURAL LANGUAGE %s",
					  (useParams && plang->lanpltrusted) ? "TRUSTED " : "",
					  qlanname);
	if (useParams)
	{
		appendPQExpBuffer(defqry, " HANDLER %s",
						  fmtId(funcInfo->dobj.name));

		if (OidIsValid(plang->laninline))
		{
			appendPQExpBuffer(defqry, " INLINE ");
			/* Cope with possibility that inline is in different schema */
			if (inlineInfo->dobj.namespace != funcInfo->dobj.namespace)
				appendPQExpBuffer(defqry, "%s.",
							fmtId(inlineInfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(defqry, "%s",
							  fmtId(inlineInfo->dobj.name));
		}
		if (OidIsValid(plang->lanvalidator))
		{
			appendPQExpBuffer(defqry, " VALIDATOR ");
			/* Cope with possibility that validator is in different schema */
			if (validatorInfo->dobj.namespace != funcInfo->dobj.namespace)
				appendPQExpBuffer(defqry, "%s.",
							fmtId(validatorInfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(defqry, "%s",
							  fmtId(validatorInfo->dobj.name));
		}
	}
	appendPQExpBuffer(defqry, ";\n");

	/*
	 * If the language is one of those for which the call handler and
	 * validator functions are defined in pg_pltemplate, we must add ALTER
	 * FUNCTION ... OWNER statements to switch the functions to the user to
	 * whom the functions are assigned -OR- adjust the language owner to
	 * reflect the call handler owner so a SET SESSION AUTHORIZATION statement
	 * properly reflects the "language" owner.
	 *
	 * Functions specified in pg_pltemplate are entered into pg_proc under
	 * pg_catalog.	Functions in pg_catalog are omitted from the function list
	 * structure resulting in the references to them in this procedure to be
	 * NULL.
	 *
	 * TODO: Adjust for ALTER LANGUAGE ... OWNER support.
	 */
	if (use_setsessauth)
	{
		/*
		 * If using SET SESSION AUTHORIZATION statements to reflect
		 * language/function ownership, alter the LANGUAGE owner to reflect
		 * the owner of the call handler function (or the validator function)
		 * if the fuction is from pg_pltempate. (Other functions are
		 * explicitly created and not subject the user in effect with CREATE
		 * LANGUAGE.)
		 */
		char	   *languageOwner = NULL;

		if (funcInfo == NULL)
		{
			languageOwner = getFuncOwner(plang->lanplcallfoid, "tmplhandler");
		}
		else if (validatorInfo == NULL)
		{
			languageOwner = getFuncOwner(plang->lanvalidator, "tmplvalidator");
		}
		if (languageOwner != NULL)
		{
			free(plang->lanowner);
			plang->lanowner = languageOwner;
		}
	}
	else
	{
		/*
		 * If the call handler or validator is defined, check to see if it's
		 * one of the pre-defined ones.  If so, it won't have been dumped as a
		 * function so won't have the proper owner -- we need to emit an ALTER
		 * FUNCTION ... OWNER statement for it.
		 */
		if (funcInfo == NULL)
		{
			dumpPlTemplateFunc(plang->lanplcallfoid, "tmplhandler", defqry);
		}
		if (validatorInfo == NULL)
		{
			dumpPlTemplateFunc(plang->lanvalidator, "tmplvalidator", defqry);
		}
	}

	ArchiveEntry(fout, plang->dobj.catId, plang->dobj.dumpId,
				 plang->dobj.name,
				 lanschema, NULL, plang->lanowner,
				 false, "PROCEDURAL LANGUAGE",
				 defqry->data,
				 delqry->data,
				 NULL,
				 plang->dobj.dependencies, plang->dobj.nDeps,
				 NULL, NULL);

	/* Dump Proc Lang Comments */
	resetPQExpBuffer(defqry);
	appendPQExpBuffer(defqry, "LANGUAGE %s", qlanname);
	dumpComment(fout, defqry->data,
				NULL, "",
				plang->dobj.catId, 0, plang->dobj.dumpId);

	if (plang->lanpltrusted)
		dumpACL(fout, plang->dobj.catId, plang->dobj.dumpId, "LANGUAGE",
				qlanname, plang->dobj.name,
				lanschema,
				plang->lanowner, plang->lanacl);

	free(qlanname);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
}


/*
 * getFuncOwner - retrieves the "proowner" of the function identified by funcOid
 * if, and only if, funcOid represents a function specified in pg_pltemplate.
 */
static char *
getFuncOwner(Oid funcOid, const char *templateField)
{
	PGresult   *res;
	int			ntups;
	int			i_funcowner;
	char	   *functionOwner = NULL;
	PQExpBuffer query = createPQExpBuffer();

	/* Ensure we're in the proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(query,
					  "SELECT ( %s proowner ) AS funcowner "
					  "FROM pg_proc "
		"WHERE ( oid = %d AND proname IN ( SELECT %s FROM pg_pltemplate ) )",
					  username_subquery, funcOid, templateField);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups != 0)
	{
		i_funcowner = PQfnumber(res, "funcowner");
		functionOwner = pg_strdup(PQgetvalue(res, 0, i_funcowner));
	}

	PQclear(res);
	destroyPQExpBuffer(query);

	return functionOwner;
}


/*
 * dumpPlTemplateFunc - appends an "ALTER FUNCTION ... OWNER" statement for the
 * pg_pltemplate-defined language function specified to the PQExpBuffer provided.
 *
 * The ALTER FUNCTION statement is added if, and only if, the function is defined
 * in the pg_catalog schema AND is identified in the pg_pltemplate table.
 */
static void
dumpPlTemplateFunc(Oid funcOid, const char *templateField, PQExpBuffer buffer)
{
	PGresult   *res;
	int			ntups;
	int			i_signature;
	int			i_owner;
	char	   *functionSignature = NULL;
	char	   *ownerName = NULL;
	PQExpBuffer fquery = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	appendPQExpBuffer(fquery,
					  "SELECT p.oid::pg_catalog.regprocedure AS signature, "
					  "( %s proowner ) AS owner "
					  "FROM pg_pltemplate t, pg_proc p "
					  "WHERE p.oid = %d "
					  "AND proname = %s "
					  "AND pronamespace = ( SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog' )",
					  username_subquery, funcOid, templateField);

	res = PQexec(g_conn, fquery->data);
	check_sql_result(res, g_conn, fquery->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups != 0)
	{
		i_signature = PQfnumber(res, "signature");
		i_owner = PQfnumber(res, "owner");
		functionSignature = pg_strdup(PQgetvalue(res, 0, i_signature));
		ownerName = pg_strdup(PQgetvalue(res, 0, i_owner));

		if (functionSignature != NULL && ownerName != NULL)
		{
			appendPQExpBuffer(buffer, "ALTER FUNCTION %s OWNER TO %s;\n", functionSignature, ownerName);
		}

		free(functionSignature);
		free(ownerName);
	}

	PQclear(res);
	destroyPQExpBuffer(fquery);
}


/*
 * format_function_arguments: generate function name and argument list
 *
 * This is used when we can rely on pg_get_function_arguments to format
 * the argument list.
 */
static char *
format_function_arguments(FuncInfo *finfo, char *funcargs)
{
	PQExpBufferData fn;

	initPQExpBuffer(&fn);
	appendPQExpBuffer(&fn, "%s(%s)", fmtId(finfo->dobj.name), funcargs);
	return fn.data;
}

/*
 * format_function_arguments_old: generate function name and argument list
 *
 * The argument type names are qualified if needed.  The function name
 * is never qualified.
 *
 * This is used only with pre-GPDB 5.0 servers, so we aren't expecting to see
 * DEFAULT arguments.
 *
 * Any or all of allargtypes, argmodes, argnames may be NULL.
 */
static char *
format_function_arguments_old(FuncInfo *finfo, int nallargs,
						  char **allargtypes,
						  char **argmodes,
						  char **argnames)
{
	PQExpBufferData fn;
	int			j;

	initPQExpBuffer(&fn);
	appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
	for (j = 0; j < nallargs; j++)
	{
		Oid			typid;
		char	   *typname;
		const char *argmode;
		const char *argname;

		typid = allargtypes ? atooid(allargtypes[j]) : finfo->argtypes[j];
		typname = getFormattedTypeName(typid, zeroAsOpaque);

		if (argmodes)
		{
			switch (argmodes[j][0])
			{
				case PROARGMODE_IN:
					argmode = "";
					break;
				case PROARGMODE_OUT:
					argmode = "OUT ";
					break;
				case PROARGMODE_INOUT:
					argmode = "INOUT ";
					break;
				case PROARGMODE_VARIADIC:
					argmode = "VARIADIC ";
					break;
				case PROARGMODE_TABLE:
					/* skip table column's names */
					free(typname);
					continue;
				default:
					mpp_err_msg(logWarn, progname, "WARNING: bogus value in proargmodes array\n");
					argmode = "";
					break;
			}
		}
		else
			argmode = "";

		argname = argnames ? argnames[j] : (char *) NULL;
		if (argname && argname[0] == '\0')
			argname = NULL;

		appendPQExpBuffer(&fn, "%s%s%s%s%s",
						  (j > 0) ? ", " : "",
						  argmode,
						  argname ? fmtId(argname) : "",
						  argname ? " " : "",
						  typname);
		free(typname);
	}
	appendPQExpBuffer(&fn, ")");
	return fn.data;
}

/*
 *	is_returns_table_function: returns true if function id declared as
 *	RETURNS TABLE, i.e. at least one argument is PROARGMODE_TABLE
 */
static bool
is_returns_table_function(int nallargs, char **argmodes)
{
	int			j;

	if (argmodes)
		for (j = 0; j < nallargs; j++)
			if (argmodes[j][0] == PROARGMODE_TABLE)
				return true;

	return false;
}


/*
 * format_table_function_columns: generate column list for
 * table functions.
 */
static char *
format_table_function_columns(FuncInfo *finfo, int nallargs,
							  char **allargtypes,
							  char **argmodes,
							  char **argnames)
{
	PQExpBufferData fn;
	int			j;
	bool		first_column = true;

	initPQExpBuffer(&fn);
	appendPQExpBuffer(&fn, "(");

	for (j = 0; j < nallargs; j++)
	{
		Oid			typid;
		char	   *typname;

		/*
		 * argmodes are checked in format_function_arguments, it isn't necessary
		 * to check argmodes here again
		 */
		if (argmodes[j][0] == PROARGMODE_TABLE)
		{
			typid = allargtypes ? atooid(allargtypes[j]) : finfo->argtypes[j];
			typname = getFormattedTypeName(typid, zeroAsOpaque);

			/* column's name is always NOT NULL (checked in gram.y) */
			appendPQExpBuffer(&fn, "%s%s %s",
							  first_column ? "" : ", ",
							  fmtId(argnames[j]),
							  typname);
			free(typname);
			first_column = false;
		}
	}

	appendPQExpBuffer(&fn, ")");
	return fn.data;
}


/*
 * format_function_signature: generate function name and argument list
 *
 * This is like format_function_arguments except that only a minimal
 * list of input argument types is generated; this is sufficient to
 * reference the function, but not to define it.
 *
 * If honor_quotes is false then the function name is never quoted.
 * This is appropriate for use in TOC tags, but not in SQL commands.
 */
static char *
format_function_signature(FuncInfo *finfo, bool honor_quotes)
{
	PQExpBufferData fn;
	int			j;

	initPQExpBuffer(&fn);
	if (honor_quotes)
		appendPQExpBuffer(&fn, "%s(", fmtId(finfo->dobj.name));
	else
		appendPQExpBuffer(&fn, "%s(", finfo->dobj.name);
	for (j = 0; j < finfo->nargs; j++)
	{
		char	   *typname;

		typname = getFormattedTypeName(finfo->argtypes[j], zeroAsOpaque);

		appendPQExpBuffer(&fn, "%s%s",
						  (j > 0) ? ", " : "",
						  typname);
		free(typname);
	}
	appendPQExpBuffer(&fn, ")");
	return fn.data;
}


/*
 * dumpFunc:
 *	  dump out one function
 */
static void
dumpFunc(Archive *fout, FuncInfo *finfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delqry;
	PQExpBuffer asPart;
	PGresult   *res;
	char	   *funcsig;
	char	   *funcsig_tag;
	int			ntups;
	char	   *proretset;
	char	   *prosrc;
	char	   *probin;
	char       *funcargs;
	char       *funcresult;
	char	   *proallargtypes;
	char	   *proargmodes;
	char	   *proargnames;
	char	   *provolatile;
	char	   *proisstrict;
	char	   *prosecdef;
	char	   *lanname;
	char	   *prodataaccess;
	char	   *rettypename;
	int			nallargs;
	char	  **allargtypes = NULL;
	char	  **argmodes = NULL;
	char	  **argnames = NULL;
	bool		isGE43 = isGPDB4300OrLater();
	bool		isGE50 = isGPDB5000OrLater();

	/* Skip if not to be dumped */
	if (!finfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delqry = createPQExpBuffer();
	asPart = createPQExpBuffer();

	/* Set proper schema search path so type references list correctly */
	selectSourceSchema(finfo->dobj.namespace->dobj.name);

	/* Fetch function-specific details */
	if (isGE50)
	{
		/*
		 * In GPDB 5.0 and up we rely on pg_get_function_arguments and
		 * pg_get_function_result instead of examining proallargtypes etc.
		 */
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "pg_catalog.pg_get_function_arguments(oid) as funcargs, "
						  "pg_catalog.pg_get_function_result(oid) as funcresult, "
						  "provolatile, proisstrict, prosecdef, prodataaccess, "
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) as lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  finfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query,
						  "SELECT proretset, prosrc, probin, "
						  "proallargtypes, proargmodes, proargnames, "
						  "provolatile, proisstrict, prosecdef, %s"
						  "(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) as lanname "
						  "FROM pg_catalog.pg_proc "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  (isGE43 ? "prodataaccess, " : ""),
						  finfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	proretset = PQgetvalue(res, 0, PQfnumber(res, "proretset"));
	prosrc = PQgetvalue(res, 0, PQfnumber(res, "prosrc"));
	probin = PQgetvalue(res, 0, PQfnumber(res, "probin"));
	if (isGE50)
	{
		funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
		funcresult = PQgetvalue(res, 0, PQfnumber(res, "funcresult"));
		proallargtypes = proargmodes = proargnames = NULL;
	}
	else
	{
		proallargtypes = PQgetvalue(res, 0, PQfnumber(res, "proallargtypes"));
		proargmodes = PQgetvalue(res, 0, PQfnumber(res, "proargmodes"));
		proargnames = PQgetvalue(res, 0, PQfnumber(res, "proargnames"));
		funcargs = funcresult = NULL;
	}
	provolatile = PQgetvalue(res, 0, PQfnumber(res, "provolatile"));
	proisstrict = PQgetvalue(res, 0, PQfnumber(res, "proisstrict"));
	prosecdef = PQgetvalue(res, 0, PQfnumber(res, "prosecdef"));
	lanname = PQgetvalue(res, 0, PQfnumber(res, "lanname"));
	prodataaccess = PQgetvalue(res, 0, PQfnumber(res, "prodataaccess"));

	/*
	 * See backend/commands/define.c for details of how the 'AS' clause is
	 * used. In GPDB Paris and up, an unused probin is NULL (here ""); previous
	 * versions would set it to "-".  There are no known cases in which prosrc
	 * is unused, so the tests below for "-" are probably useless.
	 */
	if (probin[0] != '\0' && strcmp(probin, "-") != 0)
	{
		appendPQExpBuffer(asPart, "AS ");
		appendStringLiteralAH(asPart, probin, fout);
		if (strcmp(prosrc, "-") != 0)
		{
			appendPQExpBuffer(asPart, ", ");

			/*
			 * where we have bin, use dollar quoting if allowed and src
			 * contains quote or backslash; else use regular quoting.
			 */
			if (disable_dollar_quoting ||
			  (strchr(prosrc, '\'') == NULL && strchr(prosrc, '\\') == NULL))
				appendStringLiteralAH(asPart, prosrc, fout);
			else
				appendStringLiteralDQ(asPart, prosrc, NULL);
		}
	}
	else
	{
		if (strcmp(prosrc, "-") != 0)
		{
			appendPQExpBuffer(asPart, "AS ");
			/* with no bin, dollar quote src unconditionally if allowed */
			if (disable_dollar_quoting)
				appendStringLiteralAH(asPart, prosrc, fout);
			else
				appendStringLiteralDQ(asPart, prosrc, NULL);
		}
	}

	nallargs = finfo->nargs;	/* unless we learn different from allargs */

	if (proallargtypes && *proallargtypes)
	{
		int			nitems = 0;

		if (!parsePGArray(proallargtypes, &allargtypes, &nitems) ||
			nitems < finfo->nargs)
		{
			mpp_err_msg(logWarn, progname, "WARNING: could not parse proallargtypes array\n");
			if (allargtypes)
				free(allargtypes);
			allargtypes = NULL;
		}
		else
			nallargs = nitems;
	}

	if (proargmodes && *proargmodes)
	{
		int			nitems = 0;

		if (!parsePGArray(proargmodes, &argmodes, &nitems) ||
			nitems != nallargs)
		{
			mpp_err_msg(logWarn, progname, "WARNING: could not parse proargmodes array\n");
			if (argmodes)
				free(argmodes);
			argmodes = NULL;
		}
	}

	if (proargnames && *proargnames)
	{
		int			nitems = 0;

		if (!parsePGArray(proargnames, &argnames, &nitems) ||
			nitems != nallargs)
		{
			mpp_err_msg(logWarn, progname, "WARNING: could not parse proargnames array\n");
			if (argnames)
				free(argnames);
			argnames = NULL;
		}
	}

	if (funcargs)
		funcsig = format_function_arguments(finfo, funcargs);
	else
		funcsig = format_function_arguments_old(finfo, nallargs, allargtypes,
												argmodes, argnames);
	funcsig_tag = format_function_signature(finfo, false);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delqry, "DROP FUNCTION %s.%s;\n",
					  fmtId(finfo->dobj.namespace->dobj.name),
					  funcsig);

	appendPQExpBuffer(q, "CREATE FUNCTION %s ", funcsig);

	if (funcresult)
		appendPQExpBuffer(q, "RETURNS %s", funcresult);
	else
	{
		/* switch between RETURNS SETOF RECORD and RETURNS TABLE functions */
		if (!is_returns_table_function(nallargs, argmodes))
		{
			rettypename = getFormattedTypeName(finfo->prorettype, zeroAsOpaque);
			appendPQExpBuffer(q, "RETURNS %s%s",
							  (proretset[0] == 't') ? "SETOF " : "",
							  rettypename);
			free(rettypename);
		}
		else
		{
			char	   *func_cols;
			func_cols = format_table_function_columns(finfo, nallargs, allargtypes,
													  argmodes, argnames);
			appendPQExpBuffer(q, "RETURNS TABLE %s", func_cols);
			free(func_cols);
		}
	}

	appendPQExpBuffer(q, "\n    %s", asPart->data);
	appendPQExpBuffer(q, "\n    LANGUAGE %s", fmtId(lanname));

	if (provolatile[0] != PROVOLATILE_VOLATILE)
	{
		if (provolatile[0] == PROVOLATILE_IMMUTABLE)
			appendPQExpBuffer(q, " IMMUTABLE");
		else if (provolatile[0] == PROVOLATILE_STABLE)
			appendPQExpBuffer(q, " STABLE");
		else if (provolatile[0] != PROVOLATILE_VOLATILE)
		{
			mpp_err_msg(logError, progname, "unrecognized provolatile value for function \"%s\"\n",
						finfo->dobj.name);
			exit_nicely();
		}
	}

	if (proisstrict[0] == 't')
		appendPQExpBuffer(q, " STRICT");

	if (prosecdef[0] == 't')
		appendPQExpBuffer(q, " SECURITY DEFINER");

	if (prodataaccess[0] == PRODATAACCESS_NONE)
		appendPQExpBuffer(q, " NO SQL");
	else if (prodataaccess[0] == PRODATAACCESS_CONTAINS)
		appendPQExpBuffer(q, " CONTAINS SQL");
	else if (prodataaccess[0] == PRODATAACCESS_READS)
		appendPQExpBuffer(q, " READS SQL DATA");
	else if (prodataaccess[0] == PRODATAACCESS_MODIFIES)
		appendPQExpBuffer(q, " MODIFIES SQL DATA");

	appendPQExpBuffer(q, ";\n");

	ArchiveEntry(fout, finfo->dobj.catId, finfo->dobj.dumpId,
				 funcsig_tag,
				 finfo->dobj.namespace->dobj.name,
				 NULL,
				 finfo->rolname, false,
				 "FUNCTION", q->data, delqry->data, NULL,
				 finfo->dobj.dependencies, finfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Function Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "FUNCTION %s", funcsig);
	dumpComment(fout, q->data,
				finfo->dobj.namespace->dobj.name, finfo->rolname,
				finfo->dobj.catId, 0, finfo->dobj.dumpId);

	dumpACL(fout, finfo->dobj.catId, finfo->dobj.dumpId, "FUNCTION",
			funcsig, funcsig_tag,
			finfo->dobj.namespace->dobj.name,
			finfo->rolname, finfo->proacl);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(asPart);
	free(funcsig);
	free(funcsig_tag);
	if (allargtypes)
		free(allargtypes);
	if (argmodes)
		free(argmodes);
	if (argnames)
		free(argnames);
}


/*
 * Dump a user-defined cast
 */
static void
dumpCast(Archive *fout, CastInfo *cast)
{
	PQExpBuffer defqry;
	PQExpBuffer delqry;
	PQExpBuffer castsig;
	FuncInfo   *funcInfo = NULL;
	TypeInfo   *sourceInfo;
	TypeInfo   *targetInfo;

	if (!cast->dobj.dump || dataOnly)
		return;

	if (OidIsValid(cast->castfunc))
	{
		funcInfo = findFuncByOid(cast->castfunc);
		if (funcInfo == NULL)
			return;
	}

	/*
	 * As per discussion we dump casts if one or more of the underlying
	 * objects (the conversion function and the two data types) are not
	 * builtin AND if all of the non-builtin objects are included in the dump.
	 * Builtin meaning, the namespace name does not start with "pg_".
	 */
	sourceInfo = findTypeByOid(cast->castsource);
	targetInfo = findTypeByOid(cast->casttarget);

	if (sourceInfo == NULL || targetInfo == NULL)
		return;

	/*
	 * Skip this cast if all objects are from pg_
	 */
	if ((funcInfo == NULL ||
		 strncmp(funcInfo->dobj.namespace->dobj.name, "pg_", 3) == 0) &&
		strncmp(sourceInfo->dobj.namespace->dobj.name, "pg_", 3) == 0 &&
		strncmp(targetInfo->dobj.namespace->dobj.name, "pg_", 3) == 0)
		return;

	/*
	 * Skip cast if function isn't from pg_ and is not to be dumped.
	 */
	if (funcInfo &&
		strncmp(funcInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!funcInfo->dobj.dump)
		return;

	/*
	 * Same for the source type
	 */
	if (strncmp(sourceInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!sourceInfo->dobj.dump)
		return;

	/*
	 * and the target type.
	 */
	if (strncmp(targetInfo->dobj.namespace->dobj.name, "pg_", 3) != 0 &&
		!targetInfo->dobj.dump)
		return;

	/* Make sure we are in proper schema (needed for getFormattedTypeName) */
	selectSourceSchema("pg_catalog");

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();
	castsig = createPQExpBuffer();

	appendPQExpBuffer(delqry, "DROP CAST (%s AS %s);\n",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	appendPQExpBuffer(defqry, "CREATE CAST (%s AS %s) ",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	if (!OidIsValid(cast->castfunc))
		appendPQExpBuffer(defqry, "WITHOUT FUNCTION");
	else
	{
		/*
		 * Always qualify the function name, in case it is not in pg_catalog
		 * schema (format_function_signature won't qualify it).
		 */
		appendPQExpBuffer(defqry, "WITH FUNCTION %s.",
						  fmtId(funcInfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(defqry, "%s",
						  format_function_signature(funcInfo, true));
	}

	if (cast->castcontext == 'a')
		appendPQExpBuffer(defqry, " AS ASSIGNMENT");
	else if (cast->castcontext == 'i')
		appendPQExpBuffer(defqry, " AS IMPLICIT");
	appendPQExpBuffer(defqry, ";\n");

	appendPQExpBuffer(castsig, "CAST (%s AS %s)",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));

	ArchiveEntry(fout, cast->dobj.catId, cast->dobj.dumpId,
				 castsig->data,
				 "pg_catalog", NULL, "",
				 false, "CAST", defqry->data, delqry->data, NULL,
				 cast->dobj.dependencies, cast->dobj.nDeps,
				 NULL, NULL);

	/* Dump Cast Comments */
	resetPQExpBuffer(defqry);
	appendPQExpBuffer(defqry, "CAST (%s AS %s)",
					  getFormattedTypeName(cast->castsource, zeroAsNone),
					  getFormattedTypeName(cast->casttarget, zeroAsNone));
	dumpComment(fout, defqry->data,
				NULL, "",
				cast->dobj.catId, 0, cast->dobj.dumpId);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(castsig);
}

/*
 * dumpOpr
 *	  write out a single operator definition
 */
static void
dumpOpr(Archive *fout, OprInfo *oprinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer oprid;
	PQExpBuffer details;
	const char *name;
	PGresult   *res;
	int			ntups;
	int			i_oprkind;
	int			i_oprcode;
	int			i_oprleft;
	int			i_oprright;
	int			i_oprcom;
	int			i_oprnegate;
	int			i_oprrest;
	int			i_oprjoin;
	int			i_oprcanmerge;
	int			i_oprcanhash;
	char	   *oprkind;
	char	   *oprcode;
	char	   *oprleft;
	char	   *oprright;
	char	   *oprcom;
	char	   *oprnegate;
	char	   *oprrest;
	char	   *oprjoin;
	char	   *oprcanmerge;
	char	   *oprcanhash;

	/* Skip if not to be dumped */
	if (!oprinfo->dobj.dump || dataOnly)
		return;

	/*
	 * some operators are invalid because they were the result of user
	 * defining operators before commutators exist
	 */
	if (!OidIsValid(oprinfo->oprcode))
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	oprid = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(oprinfo->dobj.namespace->dobj.name);

	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT oprkind, "
						  "oprcode::pg_catalog.regprocedure, "
						  "oprleft::pg_catalog.regtype, "
						  "oprright::pg_catalog.regtype, "
						  "oprcom::pg_catalog.regoperator, "
						  "oprnegate::pg_catalog.regoperator, "
						  "oprrest::pg_catalog.regprocedure, "
						  "oprjoin::pg_catalog.regprocedure, "
						  "oprcanmerge, oprcanhash "
						  "from pg_catalog.pg_operator "
						  "where oid = '%u'::pg_catalog.oid",
						  oprinfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70300)
	{
		appendPQExpBuffer(query, "SELECT oprkind, "
						  "oprcode::pg_catalog.regprocedure, "
						  "oprleft::pg_catalog.regtype, "
						  "oprright::pg_catalog.regtype, "
						  "oprcom::pg_catalog.regoperator, "
						  "oprnegate::pg_catalog.regoperator, "
						  "oprrest::pg_catalog.regprocedure, "
						  "oprjoin::pg_catalog.regprocedure, "
						  "(oprlsortop != 0) as oprcanmerge, "
						  "oprcanhash "
						  "from pg_catalog.pg_operator "
						  "where oid = '%u'::pg_catalog.oid",
						  oprinfo->dobj.catId.oid);
	}
	else if (g_fout->remoteVersion >= 70100)
	{
		appendPQExpBuffer(query, "SELECT oprkind, oprcode, "
						  "CASE WHEN oprleft = 0 THEN '-' "
						  "ELSE format_type(oprleft, NULL) END as oprleft, "
						  "CASE WHEN oprright = 0 THEN '-' "
						  "ELSE format_type(oprright, NULL) END as oprright, "
						  "oprcom, oprnegate, oprrest, oprjoin, "
						  "(oprlsortop != 0) as oprcanmerge, "
						  "oprcanhash "
						  "from pg_operator "
						  "where oid = '%u'::oid",
						  oprinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT oprkind, oprcode, "
						  "CASE WHEN oprleft = 0 THEN '-'::name "
						  "ELSE (select typname from pg_type where oid = oprleft) END as oprleft, "
						  "CASE WHEN oprright = 0 THEN '-'::name "
						  "ELSE (select typname from pg_type where oid = oprright) END as oprright, "
						  "oprcom, oprnegate, oprrest, oprjoin, "
						  "(oprlsortop != 0) as oprcanmerge, "
						  "oprcanhash "
						  "from pg_operator "
						  "where oid = '%u'::oid",
						  oprinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	i_oprkind = PQfnumber(res, "oprkind");
	i_oprcode = PQfnumber(res, "oprcode");
	i_oprleft = PQfnumber(res, "oprleft");
	i_oprright = PQfnumber(res, "oprright");
	i_oprcom = PQfnumber(res, "oprcom");
	i_oprnegate = PQfnumber(res, "oprnegate");
	i_oprrest = PQfnumber(res, "oprrest");
	i_oprjoin = PQfnumber(res, "oprjoin");
	i_oprcanmerge = PQfnumber(res, "oprcanmerge");
	i_oprcanhash = PQfnumber(res, "oprcanhash");

	oprkind = PQgetvalue(res, 0, i_oprkind);
	oprcode = PQgetvalue(res, 0, i_oprcode);
	oprleft = PQgetvalue(res, 0, i_oprleft);
	oprright = PQgetvalue(res, 0, i_oprright);
	oprcom = PQgetvalue(res, 0, i_oprcom);
	oprnegate = PQgetvalue(res, 0, i_oprnegate);
	oprrest = PQgetvalue(res, 0, i_oprrest);
	oprjoin = PQgetvalue(res, 0, i_oprjoin);
	oprcanmerge = PQgetvalue(res, 0, i_oprcanmerge);
	oprcanhash = PQgetvalue(res, 0, i_oprcanhash);

	appendPQExpBuffer(details, "    PROCEDURE = %s",
					  convertRegProcReference(oprcode));

	appendPQExpBuffer(oprid, "%s (",
					  oprinfo->dobj.name);

	/*
	 * right unary means there's a left arg and left unary means there's a
	 * right arg
	 */
	if (strcmp(oprkind, "r") == 0 ||
		strcmp(oprkind, "b") == 0)
	{
		name = oprleft;

		appendPQExpBuffer(details, ",\n    LEFTARG = %s", name);
		appendPQExpBuffer(oprid, "%s", name);
	}
	else
		appendPQExpBuffer(oprid, "NONE");

	if (strcmp(oprkind, "l") == 0 ||
		strcmp(oprkind, "b") == 0)
	{
		name = oprright;

		appendPQExpBuffer(details, ",\n    RIGHTARG = %s", name);
		appendPQExpBuffer(oprid, ", %s)", name);
	}
	else
		appendPQExpBuffer(oprid, ", NONE)");

	name = convertOperatorReference(oprcom);
	if (name)
		appendPQExpBuffer(details, ",\n    COMMUTATOR = %s", name);

	name = convertOperatorReference(oprnegate);
	if (name)
		appendPQExpBuffer(details, ",\n    NEGATOR = %s", name);

	if (strcmp(oprcanmerge, "t") == 0)
		appendPQExpBuffer(details, ",\n    MERGES");

	if (strcmp(oprcanhash, "t") == 0)
		appendPQExpBuffer(details, ",\n    HASHES");

	name = convertRegProcReference(oprrest);
	if (name)
		appendPQExpBuffer(details, ",\n    RESTRICT = %s", name);

	name = convertRegProcReference(oprjoin);
	if (name)
		appendPQExpBuffer(details, ",\n    JOIN = %s", name);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR %s.%s;\n",
					  fmtId(oprinfo->dobj.namespace->dobj.name),
					  oprid->data);

	appendPQExpBuffer(q, "CREATE OPERATOR %s (\n%s\n);\n",
					  oprinfo->dobj.name, details->data);

	ArchiveEntry(fout, oprinfo->dobj.catId, oprinfo->dobj.dumpId,
				 oprinfo->dobj.name,
				 oprinfo->dobj.namespace->dobj.name,
				 NULL,
				 oprinfo->rolname,
				 false, "OPERATOR", q->data, delq->data, NULL,
				 oprinfo->dobj.dependencies, oprinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR %s", oprid->data);
	dumpComment(fout, q->data,
				oprinfo->dobj.namespace->dobj.name, oprinfo->rolname,
				oprinfo->dobj.catId, 0, oprinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(oprid);
	destroyPQExpBuffer(details);
}

/*
 * Convert a function reference obtained from pg_operator
 *
 * Returns what to print, or NULL if function references is InvalidOid
 *
 * In 7.3 the input is a REGPROCEDURE display; we have to strip the
 * argument-types part.  In prior versions, the input is a REGPROC display.
 */
static const char *
convertRegProcReference(const char *proc)
{
	char	   *name;
	char	   *paren;
	bool		inquote;

	/* In all cases "-" means a null reference */
	if (strcmp(proc, "-") == 0)
		return NULL;

	name = pg_strdup(proc);
	/* find non-double-quoted left paren */
	inquote = false;
	for (paren = name; *paren; paren++)
	{
		if (*paren == '(' && !inquote)
		{
			*paren = '\0';
			break;
		}
		if (*paren == '"')
			inquote = !inquote;
	}
	return name;
}

/*
 * Convert an operator cross-reference obtained from pg_operator
 *
 * Returns what to print, or NULL to print nothing
 *
 * In 7.3 and up the input is a REGOPERATOR display; we have to strip the
 * argument-types part, and add OPERATOR() decoration if the name is
 * schema-qualified.  In older versions, the input is just a numeric OID,
 * which we search our operator list for.
 */
static const char *
convertOperatorReference(const char *opr)
{
	char	   *name;
	char	   *oname;
	char	   *ptr;
	bool		inquote;
	bool		sawdot;

	/* In all cases "0" means a null reference */
	if (strcmp(opr, "0") == 0)
		return NULL;

	name = pg_strdup(opr);
	/* find non-double-quoted left paren, and check for non-quoted dot */
	inquote = false;
	sawdot = false;
	for (ptr = name; *ptr; ptr++)
	{
		if (*ptr == '"')
			inquote = !inquote;
		else if (*ptr == '.' && !inquote)
			sawdot = true;
		else if (*ptr == '(' && !inquote)
		{
			*ptr = '\0';
			break;
		}
	}
	/* If not schema-qualified, don't need to add OPERATOR() */
	if (!sawdot)
		return name;
	oname = malloc(strlen(name) + 11);
	sprintf(oname, "OPERATOR(%s)", name);
	free(name);
	return oname;
}

/*
 * Convert a function OID obtained from pg_ts_parser or pg_ts_template
 *
 * It is sufficient to use REGPROC rather than REGPROCEDURE, since the
 * argument lists of these functions are predetermined.  Note that the
 * caller should ensure we are in the proper schema, because the results
 * are search path dependent!
 */
static const char *
convertTSFunction(Oid funcOid)
{
	char	   *result;
	char		query[128];
	PGresult   *res;
	int			ntups;

	snprintf(query, sizeof(query),
			 "SELECT '%u'::pg_catalog.regproc", funcOid);
	res = PQexec(g_conn, query);
	check_sql_result(res, g_conn, query, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "query returned %d rows instead of one: %s\n",
				  ntups, query);
		exit_nicely();
	}

	result = strdup(PQgetvalue(res, 0, 0));

	PQclear(res);

	return result;
}

/*
 * dumpOpclass
 *	  write out a single operator class definition
 */
static void
dumpOpclass(Archive *fout, OpclassInfo *opcinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PGresult   *res;
	int			ntups;
	int			i_opcintype;
	int			i_opckeytype;
	int			i_opcdefault;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_amprocnum;
	int			i_amproc;
	char	   *opcintype;
	char	   *opckeytype;
	char	   *opcdefault;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *amprocnum;
	char	   *amproc;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opcinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(opcinfo->dobj.namespace->dobj.name);

	/* Get additional fields from the pg_opclass row */
	if (g_fout->remoteVersion >= 80300)
	{
		appendPQExpBuffer(query, "SELECT opcintype::pg_catalog.regtype, "
						  "opckeytype::pg_catalog.regtype, "
						  "opcdefault, "
						  "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcmethod) AS amname "
						  "FROM pg_catalog.pg_opclass "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT opcintype::pg_catalog.regtype, "
						  "opckeytype::pg_catalog.regtype, "
						  "opcdefault, "
						  "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcamid) AS amname "
						  "FROM pg_catalog.pg_opclass "
						  "WHERE oid = '%u'::pg_catalog.oid",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	i_opcintype = PQfnumber(res, "opcintype");
	i_opckeytype = PQfnumber(res, "opckeytype");
	i_opcdefault = PQfnumber(res, "opcdefault");
	i_amname = PQfnumber(res, "amname");

	opcintype = PQgetvalue(res, 0, i_opcintype);
	opckeytype = PQgetvalue(res, 0, i_opckeytype);
	opcdefault = PQgetvalue(res, 0, i_opcdefault);
	/* amname will still be needed after we PQclear res */
	amname = pg_strdup(PQgetvalue(res, 0, i_amname));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR CLASS %s",
					  fmtId(opcinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s",
					  fmtId(opcinfo->dobj.name));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR CLASS %s\n    ",
					  fmtId(opcinfo->dobj.name));
	if (strcmp(opcdefault, "t") == 0)
		appendPQExpBuffer(q, "DEFAULT ");
	appendPQExpBuffer(q, "FOR TYPE %s USING %s AS\n    ",
					  opcintype,
					  fmtId(amname));

	needComma = false;

	if (strcmp(opckeytype, "-") != 0)
	{
		appendPQExpBuffer(q, "STORAGE %s",
						  opckeytype);
		needComma = true;
	}

	PQclear(res);

	/*
	 * Now fetch and print the OPERATOR entries (pg_amop rows).
	 */
	resetPQExpBuffer(query);

	if (g_fout->remoteVersion >= 80300)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass
		 * by pg_depend entries.
		 */
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
						  "WHERE refclassid = 'pg_catalog.pg_opclass'::regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
						  "AND classid = 'pg_catalog.pg_amop'::regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator "
						  "FROM pg_catalog.pg_amop "
						  "WHERE amopclaid = '%u'::pg_catalog.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amopstrategy = PQfnumber(res, "amopstrategy");
	i_amopreqcheck = PQfnumber(res, "amopreqcheck");
	i_amopopr = PQfnumber(res, "amopopr");

	for (i = 0; i < ntups; i++)
	{
		amopstrategy = PQgetvalue(res, i, i_amopstrategy);
		amopreqcheck = PQgetvalue(res, i, i_amopreqcheck);
		amopopr = PQgetvalue(res, i, i_amopopr);

		if (needComma)
			appendPQExpBuffer(q, " ,\n    ");

		appendPQExpBuffer(q, "OPERATOR %s %s",
						  amopstrategy, amopopr);
		if (strcmp(amopreqcheck, "t") == 0)
			appendPQExpBuffer(q, " RECHECK");

		needComma = true;
	}

	PQclear(res);

	/*
	 * Now fetch and print the FUNCTION entries (pg_amproc rows).
	 */
	resetPQExpBuffer(query);

	if (g_fout->remoteVersion >= 80300)
	{
		/*
		 * Print only those opfamily members that are tied to the opclass
		 * by pg_depend entries.
		 */
		appendPQExpBuffer(query, "SELECT amprocnum, "
						  "amproc::pg_catalog.regprocedure "
						  "FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
						  "WHERE refclassid = 'pg_catalog.pg_opclass'::regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
						  "AND classid = 'pg_catalog.pg_amproc'::regclass "
						  "AND objid = ap.oid "
						  "ORDER BY amprocnum",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amprocnum, "
						  "amproc::pg_catalog.regprocedure "
						  "FROM pg_catalog.pg_amproc "
						  "WHERE amopclaid = '%u'::pg_catalog.oid "
						  "ORDER BY amprocnum",
						  opcinfo->dobj.catId.oid);
	}

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amprocnum = PQfnumber(res, "amprocnum");
	i_amproc = PQfnumber(res, "amproc");

	for (i = 0; i < ntups; i++)
	{
		amprocnum = PQgetvalue(res, i, i_amprocnum);
		amproc = PQgetvalue(res, i, i_amproc);

		if (needComma)
			appendPQExpBuffer(q, " ,\n    ");

		appendPQExpBuffer(q, "FUNCTION %s %s",
						  amprocnum, amproc);

		needComma = true;
	}

	PQclear(res);

	appendPQExpBuffer(q, ";\n");

	ArchiveEntry(fout, opcinfo->dobj.catId, opcinfo->dobj.dumpId,
				 opcinfo->dobj.name,
				 opcinfo->dobj.namespace->dobj.name,
				 NULL,
				 opcinfo->rolname,
				 false, "OPERATOR CLASS", q->data, delq->data, NULL,
				 opcinfo->dobj.dependencies, opcinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Class Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR CLASS %s",
					  fmtId(opcinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s",
					  fmtId(amname));
	dumpComment(fout, q->data,
				NULL, opcinfo->rolname,
				opcinfo->dobj.catId, 0, opcinfo->dobj.dumpId);

	free(amname);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpOpfamily
 *	  write out a single operator family definition
 */
static void
dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PGresult   *res;
	PGresult   *res_ops;
	PGresult   *res_procs;
	int			ntups;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_amprocnum;
	int			i_amproc;
	int			i_amproclefttype;
	int			i_amprocrighttype;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *amprocnum;
	char	   *amproc;
	char	   *amproclefttype;
	char	   *amprocrighttype;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opfinfo->dobj.dump || dataOnly)
		return;

	/*
	 * We want to dump the opfamily only if (1) it contains "loose" operators
	 * or functions, or (2) it contains an opclass with a different name or
	 * owner.  Otherwise it's sufficient to let it be created during creation
	 * of the contained opclass, and not dumping it improves portability of
	 * the dump.  Since we have to fetch the loose operators/funcs anyway,
	 * do that first.
	 */

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema so regoperator works correctly */
	selectSourceSchema(opfinfo->dobj.namespace->dobj.name);

	/*
	 * Fetch only those opfamily members that are tied directly to the opfamily
	 * by pg_depend entries.
	 */
	appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
					  "amopopr::pg_catalog.regoperator "
					  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
					  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
					  "AND refobjid = '%u'::pg_catalog.oid "
					  "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
					  "AND objid = ao.oid "
					  "ORDER BY amopstrategy",
					  opfinfo->dobj.catId.oid);

	res_ops = PQexec(g_conn, query->data);
	check_sql_result(res_ops, g_conn, query->data, PGRES_TUPLES_OK);

	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT amprocnum, "
					  "amproc::pg_catalog.regprocedure, "
					  "amproclefttype::pg_catalog.regtype, "
					  "amprocrighttype::pg_catalog.regtype "
					  "FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
					  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
					  "AND refobjid = '%u'::pg_catalog.oid "
					  "AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
					  "AND objid = ap.oid "
					  "ORDER BY amprocnum",
					  opfinfo->dobj.catId.oid);

	res_procs = PQexec(g_conn, query->data);
	check_sql_result(res_procs, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res_ops) == 0 && PQntuples(res_procs) == 0)
	{
		/* No loose members, so check contained opclasses */
		resetPQExpBuffer(query);

		appendPQExpBuffer(query, "SELECT 1 "
						  "FROM pg_catalog.pg_opclass c, pg_catalog.pg_opfamily f, pg_catalog.pg_depend "
						  "WHERE f.oid = '%u'::pg_catalog.oid "
						  "AND refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = f.oid "
						  "AND classid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND objid = c.oid "
						  "AND (opcname != opfname OR opcnamespace != opfnamespace OR opcowner != opfowner) "
						  "LIMIT 1",
						  opfinfo->dobj.catId.oid);

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) == 0)
		{
			/* no need to dump it, so bail out */
			PQclear(res);
			PQclear(res_ops);
			PQclear(res_procs);
			destroyPQExpBuffer(query);
			destroyPQExpBuffer(q);
			destroyPQExpBuffer(delq);
			return;
		}

		PQclear(res);
	}

	/* Get additional fields from the pg_opfamily row */
	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT "
					  "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opfmethod) AS amname "
					  "FROM pg_catalog.pg_opfamily "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  opfinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "Got %d rows instead of one from: %s",
				  ntups, query->data);
		exit_nicely();
	}

	i_amname = PQfnumber(res, "amname");

	/* amname will still be needed after we PQclear res */
	amname = strdup(PQgetvalue(res, 0, i_amname));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s;\n",
					  fmtId(amname));

	PQclear(res);

	/* Do we need an ALTER to add loose members? */
	if (PQntuples(res_ops) > 0 || PQntuples(res_procs) > 0)
	{
		appendPQExpBuffer(q, "ALTER OPERATOR FAMILY %s",
						  fmtId(opfinfo->dobj.name));
		appendPQExpBuffer(q, " USING %s ADD\n    ",
						  fmtId(amname));

		needComma = false;

		/*
		 * Now fetch and print the OPERATOR entries (pg_amop rows).
		 */
		ntups = PQntuples(res_ops);

		i_amopstrategy = PQfnumber(res_ops, "amopstrategy");
		i_amopreqcheck = PQfnumber(res_ops, "amopreqcheck");
		i_amopopr = PQfnumber(res_ops, "amopopr");

		for (i = 0; i < ntups; i++)
		{
			amopstrategy = PQgetvalue(res_ops, i, i_amopstrategy);
			amopreqcheck = PQgetvalue(res_ops, i, i_amopreqcheck);
			amopopr = PQgetvalue(res_ops, i, i_amopopr);

			if (needComma)
				appendPQExpBuffer(q, " ,\n    ");

			appendPQExpBuffer(q, "OPERATOR %s %s",
							  amopstrategy, amopopr);
			if (strcmp(amopreqcheck, "t") == 0)
				appendPQExpBuffer(q, " RECHECK");

			needComma = true;
		}

		/*
		 * Now fetch and print the FUNCTION entries (pg_amproc rows).
		 */
		ntups = PQntuples(res_procs);

		i_amprocnum = PQfnumber(res_procs, "amprocnum");
		i_amproc = PQfnumber(res_procs, "amproc");
		i_amproclefttype = PQfnumber(res_procs, "amproclefttype");
		i_amprocrighttype = PQfnumber(res_procs, "amprocrighttype");

		for (i = 0; i < ntups; i++)
		{
			amprocnum = PQgetvalue(res_procs, i, i_amprocnum);
			amproc = PQgetvalue(res_procs, i, i_amproc);
			amproclefttype = PQgetvalue(res_procs, i, i_amproclefttype);
			amprocrighttype = PQgetvalue(res_procs, i, i_amprocrighttype);

			if (needComma)
				appendPQExpBuffer(q, " ,\n    ");

			appendPQExpBuffer(q, "FUNCTION %s (%s, %s) %s",
							  amprocnum, amproclefttype, amprocrighttype,
							  amproc);

			needComma = true;
		}

		appendPQExpBuffer(q, ";\n");
	}

	ArchiveEntry(fout, opfinfo->dobj.catId, opfinfo->dobj.dumpId,
				 opfinfo->dobj.name,
				 opfinfo->dobj.namespace->dobj.name,
				 NULL,
				 opfinfo->rolname,
				 false, "OPERATOR FAMILY", q->data, delq->data, NULL,
				 opfinfo->dobj.dependencies, opfinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Operator Family Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "OPERATOR FAMILY %s",
					  fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(q, " USING %s",
					  fmtId(amname));
	dumpComment(fout, q->data,
				NULL, opfinfo->rolname,
				opfinfo->dobj.catId, 0, opfinfo->dobj.dumpId);

	free(amname);
	PQclear(res_ops);
	PQclear(res_procs);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpConversion
 *	  write out a single conversion definition
 */
static void
dumpConversion(Archive *fout, ConvInfo *convinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer details;
	PGresult   *res;
	int			ntups;
	int			i_conname;
	int			i_conforencoding;
	int			i_contoencoding;
	int			i_conproc;
	int			i_condefault;
	const char *conname;
	const char *conforencoding;
	const char *contoencoding;
	const char *conproc;
	bool		condefault;

	/* Skip if not to be dumped */
	if (!convinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(convinfo->dobj.namespace->dobj.name);

	/* Get conversion-specific details */
	appendPQExpBuffer(query, "SELECT conname, "
		 "pg_catalog.pg_encoding_to_char(conforencoding) AS conforencoding, "
		   "pg_catalog.pg_encoding_to_char(contoencoding) AS contoencoding, "
					  "conproc, condefault "
					  "FROM pg_catalog.pg_conversion c "
					  "WHERE c.oid = '%u'::pg_catalog.oid",
					  convinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	i_conname = PQfnumber(res, "conname");
	i_conforencoding = PQfnumber(res, "conforencoding");
	i_contoencoding = PQfnumber(res, "contoencoding");
	i_conproc = PQfnumber(res, "conproc");
	i_condefault = PQfnumber(res, "condefault");

	conname = PQgetvalue(res, 0, i_conname);
	conforencoding = PQgetvalue(res, 0, i_conforencoding);
	contoencoding = PQgetvalue(res, 0, i_contoencoding);
	conproc = PQgetvalue(res, 0, i_conproc);
	condefault = (PQgetvalue(res, 0, i_condefault)[0] == 't');

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP CONVERSION %s",
					  fmtId(convinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(convinfo->dobj.name));

	appendPQExpBuffer(q, "CREATE %sCONVERSION %s FOR ",
					  (condefault) ? "DEFAULT " : "",
					  fmtId(convinfo->dobj.name));
	appendStringLiteralAH(q, conforencoding, fout);
	appendPQExpBuffer(q, " TO ");
	appendStringLiteralAH(q, contoencoding, fout);
	/* regproc is automatically quoted in 7.3 and above */
	appendPQExpBuffer(q, " FROM %s;\n", conproc);

	ArchiveEntry(fout, convinfo->dobj.catId, convinfo->dobj.dumpId,
				 convinfo->dobj.name,
				 convinfo->dobj.namespace->dobj.name,
				 NULL,
				 convinfo->rolname,
				 false, "CONVERSION", q->data, delq->data, NULL,
				 convinfo->dobj.dependencies, convinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Conversion Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "CONVERSION %s", fmtId(convinfo->dobj.name));
	dumpComment(fout, q->data,
				convinfo->dobj.namespace->dobj.name, convinfo->rolname,
				convinfo->dobj.catId, 0, convinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(details);
}

/*
 * format_aggregate_signature: generate aggregate name and argument list
 *
 * The argument type names are qualified if needed.  The aggregate name
 * is never qualified.
 */
static char *
format_aggregate_signature(AggInfo *agginfo, Archive *fout, bool honor_quotes)
{
	PQExpBufferData buf;
	int			j;

	initPQExpBuffer(&buf);
	if (honor_quotes)
		appendPQExpBuffer(&buf, "%s",
						  fmtId(agginfo->aggfn.dobj.name));
	else
		appendPQExpBuffer(&buf, "%s", agginfo->aggfn.dobj.name);

	if (agginfo->aggfn.nargs == 0)
		appendPQExpBuffer(&buf, "(*)");
	else
	{
		appendPQExpBuffer(&buf, "(");
		for (j = 0; j < agginfo->aggfn.nargs; j++)
		{
			char	   *typname;

			typname = getFormattedTypeName(agginfo->aggfn.argtypes[j], zeroAsOpaque);

			appendPQExpBuffer(&buf, "%s%s",
							  (j > 0) ? ", " : "",
							  typname);
			free(typname);
		}
		appendPQExpBuffer(&buf, ")");
	}
	return buf.data;
}

/*
 * dumpAgg
 *	  write out a single aggregate definition
 */
static void
dumpAgg(Archive *fout, AggInfo *agginfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer details;
	char	   *aggsig;
	char	   *aggsig_tag;
	PGresult   *res;
	int			ntups;
	int			i_aggtransfn;
	int			i_aggfinalfn;
	int			i_aggsortop;
	int			i_aggtranstype;
	int			i_agginitval;
	int			i_aggprelimfn;
	int			i_convertok;
	int			i_aggordered;
	const char *aggtransfn;
	const char *aggfinalfn;
	const char *aggsortop;
	const char *aggtranstype;
	const char *agginitval;
	const char *aggprelimfn;
	bool		convertok;
	bool		aggordered;

	/* Skip if not to be dumped */
	if (!agginfo->aggfn.dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	details = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(agginfo->aggfn.dobj.namespace->dobj.name);

	appendPQExpBuffer(query, "SELECT aggtransfn, "
					  "aggfinalfn, aggtranstype::pg_catalog.regtype, "
					  "aggsortop::pg_catalog.regoperator, "
					  "agginitval, "
					  "aggprelimfn, "
					  "'t'::boolean as convertok, "
					  "aggordered "
					  "from pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
					  "where a.aggfnoid = p.oid "
					  "and p.oid = '%u'::pg_catalog.oid",
					  agginfo->aggfn.dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "Got %d rows instead of one from: %s",
					ntups, query->data);
		exit_nicely();
	}

	i_aggtransfn = PQfnumber(res, "aggtransfn");
	i_aggfinalfn = PQfnumber(res, "aggfinalfn");
	i_aggsortop = PQfnumber(res, "aggsortop");
	i_aggtranstype = PQfnumber(res, "aggtranstype");
	i_agginitval = PQfnumber(res, "agginitval");
	i_aggprelimfn = PQfnumber(res, "aggprelimfn");
	i_convertok = PQfnumber(res, "convertok");
	i_aggordered = PQfnumber(res, "aggordered");

	aggtransfn = PQgetvalue(res, 0, i_aggtransfn);
	aggfinalfn = PQgetvalue(res, 0, i_aggfinalfn);
	aggsortop = PQgetvalue(res, 0, i_aggsortop);
	aggtranstype = PQgetvalue(res, 0, i_aggtranstype);
	agginitval = PQgetvalue(res, 0, i_agginitval);
	aggprelimfn = PQgetvalue(res, 0, i_aggprelimfn);
	convertok = (PQgetvalue(res, 0, i_convertok)[0] == 't');
	aggordered = (PQgetvalue(res, 0, i_aggordered)[0] == 't');

	aggsig = format_aggregate_signature(agginfo, fout, true);
	aggsig_tag = format_aggregate_signature(agginfo, fout, false);

	if (!convertok)
	{
		mpp_err_msg(logWarn, progname, "WARNING: aggregate function %s could not be dumped correctly for this database version; ignored\n",
					aggsig);
		goto cleanup;
	}

	/* If using 7.3's regproc or regtype, data is already quoted */
	appendPQExpBuffer(details, "    SFUNC = %s,\n    STYPE = %s",
					  aggtransfn,
					  aggtranstype);

	if (!PQgetisnull(res, 0, i_agginitval))
	{
		appendPQExpBuffer(details, ",\n    INITCOND = ");
		appendStringLiteralAH(details, agginitval, fout);
	}

	if (!PQgetisnull(res, 0, i_aggprelimfn))
	{
		if (strcmp(aggprelimfn, "-") != 0)
			appendPQExpBuffer(details, ",\n    PREFUNC = %s",
							  aggprelimfn);
	}

	if (strcmp(aggfinalfn, "-") != 0)
	{
		appendPQExpBuffer(details, ",\n    FINALFUNC = %s",
						  aggfinalfn);
	}

	aggsortop = convertOperatorReference(aggsortop);
	if (aggsortop)
	{
		appendPQExpBuffer(details, ",\n    SORTOP = %s",
						  aggsortop);
	}

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP AGGREGATE %s.%s;\n",
					  fmtId(agginfo->aggfn.dobj.namespace->dobj.name),
					  aggsig);

	appendPQExpBuffer(q, "CREATE %s %s (\n%s\n);\n",
					  aggordered == true ? "ORDERED AGGREGATE" : "AGGREGATE",
					  aggsig, details->data);

	ArchiveEntry(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
				 aggsig_tag,
				 agginfo->aggfn.dobj.namespace->dobj.name,
				 NULL,
				 agginfo->aggfn.rolname,
				 false, "AGGREGATE", q->data, delq->data, NULL,
				 agginfo->aggfn.dobj.dependencies, agginfo->aggfn.dobj.nDeps,
				 NULL, NULL);

	/* Dump Aggregate Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "AGGREGATE %s", aggsig);
	dumpComment(fout, q->data,
			agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname,
				agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);

	/*
	 * Since there is no GRANT ON AGGREGATE syntax, we have to make the ACL
	 * command look like a function's GRANT; in particular this affects the
	 * syntax for zero-argument aggregates.
	 */
	free(aggsig);
	free(aggsig_tag);

	aggsig = format_function_signature(&agginfo->aggfn, true);
	aggsig_tag = format_function_signature(&agginfo->aggfn, false);

	dumpACL(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
			"FUNCTION",
			aggsig, aggsig_tag,
			agginfo->aggfn.dobj.namespace->dobj.name,
			agginfo->aggfn.rolname, agginfo->aggfn.proacl);

cleanup:
	if (aggsig)
		free(aggsig);
	if (aggsig_tag)
		free(aggsig_tag);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(details);
}

/*
 * getFunctionName - retrieves a function name from an oid
 *
 */
static char *
getFunctionName(Oid oid)
{
	char	   *result;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;

	if (oid == InvalidOid)
	{
		return NULL;
	}

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT proname FROM pg_proc WHERE oid = %u;",oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "query yielded %d rows instead of one: %s\n",
				  ntups, query->data);
		exit_nicely();
	}

	/* already quoted */
	result = pg_strdup(PQgetvalue(res, 0, 0));

	PQclear(res);
	destroyPQExpBuffer(query);

	return result;
}

/*
 * dumpExtProtocol
 *	  write out a single external protocol definition
 */
static void
dumpExtProtocol(Archive *fout, ExtProtInfo *ptcinfo)
{
#define FCOUNT	3
#define READFN_IDX 0
#define WRITEFN_IDX 1
#define VALIDFN_IDX 2

	typedef struct
	{
		Oid oid; 				/* func's oid */
		char* name; 			/* func name */
		FuncInfo* pfuncinfo; 	/* FuncInfo ptr */
		bool dumpable; 			/* should we dump this function */
		bool internal;			/* is it an internal function */
	} ProtoFunc;

	ProtoFunc	protoFuncs[FCOUNT];

	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *namecopy;
	int			i;

	/* Skip if not to be dumped */
	if (!ptcinfo->dobj.dump || dataOnly)
		return;

	/* init and fill the protoFuncs array */
	memset(protoFuncs, 0, sizeof(protoFuncs));
	protoFuncs[READFN_IDX].oid = ptcinfo->ptcreadid;
	protoFuncs[WRITEFN_IDX].oid = ptcinfo->ptcwriteid;
	protoFuncs[VALIDFN_IDX].oid = ptcinfo->ptcvalidid;

	for(i=0; i<FCOUNT; i++)
	{
		if (protoFuncs[i].oid == InvalidOid)
		{
			protoFuncs[i].dumpable = false;
			protoFuncs[i].internal = true;
		}
		else
		{
			protoFuncs[i].pfuncinfo = findFuncByOid(protoFuncs[i].oid);
			if(protoFuncs[i].pfuncinfo != NULL)
			{
				protoFuncs[i].dumpable = true;
				protoFuncs[i].name = pg_strdup(protoFuncs[i].pfuncinfo->dobj.name);
				protoFuncs[i].internal = false;
			}
			else
				protoFuncs[i].internal = true;
		}
	}

	/* if all funcs are internal then we do not need to dump this protocol */
	if(protoFuncs[READFN_IDX].internal && protoFuncs[WRITEFN_IDX].internal
			&& protoFuncs[VALIDFN_IDX].internal)
		return;

	/* obtain the function name for internal functions (if any) */
	for (i=0; i<FCOUNT; i++)
		if(protoFuncs[i].internal && protoFuncs[i].oid)
		{
			protoFuncs[i].name = getFunctionName(protoFuncs[i].oid);
			if(protoFuncs[i].name)
				protoFuncs[i].dumpable = true;
		}

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	appendPQExpBuffer(q, "CREATE %s PROTOCOL %s (",
			ptcinfo->ptctrusted == true ? "TRUSTED" : "",
			fmtId(ptcinfo->dobj.name));

	if (protoFuncs[READFN_IDX].dumpable)
	{
		appendPQExpBuffer(q, " readfunc = '%s'%s",
						  protoFuncs[READFN_IDX].name,
						  (protoFuncs[WRITEFN_IDX].dumpable ? "," : ""));
	}

	if (protoFuncs[WRITEFN_IDX].dumpable)
	{
		appendPQExpBuffer(q, " writefunc = '%s'%s",
						  protoFuncs[WRITEFN_IDX].name,
					      (protoFuncs[VALIDFN_IDX].dumpable ? "," : ""));
	}

	if (protoFuncs[VALIDFN_IDX].dumpable)
	{
		appendPQExpBuffer(q, " validatorfunc = '%s'",
						  protoFuncs[VALIDFN_IDX].name);
	}

	appendPQExpBuffer(q, ");\n");

	appendPQExpBuffer(delq, "DROP PROTOCOL %s;\n",
					  fmtId(ptcinfo->dobj.name));

	ArchiveEntry(fout, ptcinfo->dobj.catId, ptcinfo->dobj.dumpId,
				 ptcinfo->dobj.name,
				 NULL,
				 NULL,
				 ptcinfo->ptcowner,
				 false, "PROTOCOL",
				 q->data, delq->data, NULL,
				 ptcinfo->dobj.dependencies, ptcinfo->dobj.nDeps,
				 NULL, NULL);

	/* Handle the ACL */
	namecopy = pg_strdup(fmtId(ptcinfo->dobj.name));
	dumpACL(fout, ptcinfo->dobj.catId, ptcinfo->dobj.dumpId,
			"PROTOCOL",
			namecopy, ptcinfo->dobj.name,
			NULL, ptcinfo->ptcowner,
			ptcinfo->ptcacl);
	free(namecopy);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);

	for(i=0; i<FCOUNT; i++)
		if(protoFuncs[i].name)
			free (protoFuncs[i].name);
}

/*
 * dumpTSParser
 *	  write out a single text search parser
 */
static void
dumpTSParser(Archive *fout, TSParserInfo *prsinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!prsinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(prsinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH PARSER %s (\n",
					  fmtId(prsinfo->dobj.name));

	appendPQExpBuffer(q, "    START = %s,\n",
					  convertTSFunction(prsinfo->prsstart));
	appendPQExpBuffer(q, "    GETTOKEN = %s,\n",
					  convertTSFunction(prsinfo->prstoken));
	appendPQExpBuffer(q, "    END = %s,\n",
					  convertTSFunction(prsinfo->prsend));
	if (prsinfo->prsheadline != InvalidOid)
		appendPQExpBuffer(q, "    HEADLINE = %s,\n",
						  convertTSFunction(prsinfo->prsheadline));
	appendPQExpBuffer(q, "    LEXTYPES = %s );\n",
					  convertTSFunction(prsinfo->prslextype));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH PARSER %s",
					  fmtId(prsinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(prsinfo->dobj.name));

	ArchiveEntry(fout, prsinfo->dobj.catId, prsinfo->dobj.dumpId,
				 prsinfo->dobj.name,
				 prsinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH PARSER", q->data, delq->data, NULL,
				 prsinfo->dobj.dependencies, prsinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Parser Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH PARSER %s",
					  fmtId(prsinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, "",
				prsinfo->dobj.catId, 0, prsinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTSDictionary
 *	  write out a single text search dictionary
 */
static void
dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;
	char	   *nspname;
	char	   *tmplname;

	/* Skip if not to be dumped */
	if (!dictinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/* Fetch name and namespace of the dictionary's template */
	selectSourceSchema("pg_catalog");
	appendPQExpBuffer(query, "SELECT nspname, tmplname "
					  "FROM pg_ts_template p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = tmplnamespace",
					  dictinfo->dicttemplate);
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "query returned %d rows instead of one: %s\n",
				  ntups, query->data);
		exit_nicely();
	}
	nspname = PQgetvalue(res, 0, 0);
	tmplname = PQgetvalue(res, 0, 1);

	/* Make sure we are in proper schema */
	selectSourceSchema(dictinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH DICTIONARY %s (\n",
					  fmtId(dictinfo->dobj.name));

	appendPQExpBuffer(q, "    TEMPLATE = ");
	if (strcmp(nspname, dictinfo->dobj.namespace->dobj.name) != 0)
		appendPQExpBuffer(q, "%s.", fmtId(nspname));
	appendPQExpBuffer(q, "%s", fmtId(tmplname));

	PQclear(res);

	/* the dictinitoption can be dumped straight into the command */
	if (dictinfo->dictinitoption)
		appendPQExpBuffer(q, ",\n    %s", dictinfo->dictinitoption);

	appendPQExpBuffer(q, " );\n");

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH DICTIONARY %s",
					  fmtId(dictinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(dictinfo->dobj.name));

	ArchiveEntry(fout, dictinfo->dobj.catId, dictinfo->dobj.dumpId,
				 dictinfo->dobj.name,
				 dictinfo->dobj.namespace->dobj.name,
				 NULL,
				 dictinfo->rolname,
				 false, "TEXT SEARCH DICTIONARY", q->data, delq->data, NULL,
				 dictinfo->dobj.dependencies, dictinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Dictionary Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH DICTIONARY %s",
					  fmtId(dictinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, dictinfo->rolname,
				dictinfo->dobj.catId, 0, dictinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpTSTemplate
 *	  write out a single text search template
 */
static void
dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!tmplinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(tmplinfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH TEMPLATE %s (\n",
					  fmtId(tmplinfo->dobj.name));

	if (tmplinfo->tmplinit != InvalidOid)
		appendPQExpBuffer(q, "    INIT = %s,\n",
						  convertTSFunction(tmplinfo->tmplinit));
	appendPQExpBuffer(q, "    LEXIZE = %s );\n",
					  convertTSFunction(tmplinfo->tmpllexize));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH TEMPLATE %s",
					  fmtId(tmplinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(tmplinfo->dobj.name));

	ArchiveEntry(fout, tmplinfo->dobj.catId, tmplinfo->dobj.dumpId,
				 tmplinfo->dobj.name,
				 tmplinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH TEMPLATE", q->data, delq->data, NULL,
				 tmplinfo->dobj.dependencies, tmplinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Template Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH TEMPLATE %s",
					  fmtId(tmplinfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, "",
				tmplinfo->dobj.catId, 0, tmplinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTSConfig
 *	  write out a single text search configuration
 */
static void
dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	char	   *nspname;
	char	   *prsname;
	int			ntups,
				i;
	int			i_tokenname;
	int			i_dictname;

	/* Skip if not to be dumped */
	if (!cfginfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/* Fetch name and namespace of the config's parser */
	selectSourceSchema("pg_catalog");
	appendPQExpBuffer(query, "SELECT nspname, prsname "
					  "FROM pg_ts_parser p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = prsnamespace",
					  cfginfo->cfgparser);
	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "query returned %d rows instead of one: %s\n",
				  ntups, query->data);
		exit_nicely();
	}
	nspname = PQgetvalue(res, 0, 0);
	prsname = PQgetvalue(res, 0, 1);

	/* Make sure we are in proper schema */
	selectSourceSchema(cfginfo->dobj.namespace->dobj.name);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH CONFIGURATION %s (\n",
					  fmtId(cfginfo->dobj.name));

	appendPQExpBuffer(q, "    PARSER = ");
	if (strcmp(nspname, cfginfo->dobj.namespace->dobj.name) != 0)
		appendPQExpBuffer(q, "%s.", fmtId(nspname));
	appendPQExpBuffer(q, "%s );\n", fmtId(prsname));

	PQclear(res);

	resetPQExpBuffer(query);
	appendPQExpBuffer(query,
					  "SELECT \n"
					  "  ( SELECT alias FROM pg_catalog.ts_token_type('%u'::pg_catalog.oid) AS t \n"
					  "    WHERE t.tokid = m.maptokentype ) AS tokenname, \n"
					  "  m.mapdict::pg_catalog.regdictionary AS dictname \n"
					  "FROM pg_catalog.pg_ts_config_map AS m \n"
					  "WHERE m.mapcfg = '%u' \n"
					  "ORDER BY m.mapcfg, m.maptokentype, m.mapseqno",
					  cfginfo->cfgparser, cfginfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);

	i_tokenname = PQfnumber(res, "tokenname");
	i_dictname = PQfnumber(res, "dictname");

	for (i = 0; i < ntups; i++)
	{
		char	   *tokenname = PQgetvalue(res, i, i_tokenname);
		char	   *dictname = PQgetvalue(res, i, i_dictname);

		if (i == 0 ||
			strcmp(tokenname, PQgetvalue(res, i - 1, i_tokenname)) != 0)
		{
			/* starting a new token type, so start a new command */
			if (i > 0)
				appendPQExpBuffer(q, ";\n");
			appendPQExpBuffer(q, "\nALTER TEXT SEARCH CONFIGURATION %s\n",
							  fmtId(cfginfo->dobj.name));
			/* tokenname needs quoting, dictname does NOT */
			appendPQExpBuffer(q, "    ADD MAPPING FOR %s WITH %s",
							  fmtId(tokenname), dictname);
		}
		else
			appendPQExpBuffer(q, ", %s", dictname);
	}

	if (ntups > 0)
		appendPQExpBuffer(q, ";\n");

	PQclear(res);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "DROP TEXT SEARCH CONFIGURATION %s",
					  fmtId(cfginfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, ".%s;\n",
					  fmtId(cfginfo->dobj.name));

	ArchiveEntry(fout, cfginfo->dobj.catId, cfginfo->dobj.dumpId,
				 cfginfo->dobj.name,
				 cfginfo->dobj.namespace->dobj.name,
				 NULL,
				 cfginfo->rolname,
			   false, "TEXT SEARCH CONFIGURATION", q->data, delq->data, NULL,
				 cfginfo->dobj.dependencies, cfginfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Configuration Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "TEXT SEARCH CONFIGURATION %s",
					  fmtId(cfginfo->dobj.name));
	dumpComment(fout, q->data,
				NULL, cfginfo->rolname,
				cfginfo->dobj.catId, 0, cfginfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}
/*----------
 * Write out grant/revoke information
 *
 * 'objCatId' is the catalog ID of the underlying object.
 * 'objDumpId' is the dump ID of the underlying object.
 * 'type' must be TABLE, FUNCTION, LANGUAGE, SCHEMA, DATABASE, or TABLESPACE.
 * 'name' is the formatted name of the object.	Must be quoted etc. already.
 * 'tag' is the tag for the archive entry (typ. unquoted name of object).
 * 'nspname' is the namespace the object is in (NULL if none).
 * 'owner' is the owner, NULL if there is no owner (for languages).
 * 'acls' is the string read out of the fooacl system catalog field;
 * it will be parsed here.
 *----------
 */
static void
dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name,
		const char *tag, const char *nspname, const char *owner,
		const char *acls)
{
	PQExpBuffer sql;

	/* Do nothing if ACL dump is not enabled */
	if (dataOnly || aclsSkip)
		return;

	sql = createPQExpBuffer();

	if (!buildACLCommands(name, type, acls, owner, fout->remoteVersion, sql))
	{
		mpp_err_msg(logError, progname, "could not parse ACL list (%s) for object \"%s\" (%s)\n",
					acls, name, type);
		exit_nicely();
	}

	if (sql->len > 0)
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag, nspname,
					 NULL,
					 owner ? owner : "",
					 false, "ACL", sql->data, "", NULL,
					 &(objDumpId), 1,
					 NULL, NULL);

	destroyPQExpBuffer(sql);
}

/*
 * dumpTable
 *	  write out to fout the declarations (not data) of a user-defined table
 */
static void
dumpTable(Archive *fout, TableInfo *tbinfo)
{
	char	   *namecopy;

	if (tbinfo->dobj.dump)
	{
		if (tbinfo->relkind == RELKIND_SEQUENCE)
			dumpSequence(fout, tbinfo);
		else if (!dataOnly)
			dumpTableSchema(fout, tbinfo);

		/* Handle the ACL here */
		namecopy = pg_strdup(fmtId(tbinfo->dobj.name));
		dumpACL(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				(tbinfo->relkind == RELKIND_SEQUENCE) ? "SEQUENCE" : "TABLE",
				namecopy, tbinfo->dobj.name,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tbinfo->relacl);
		free(namecopy);
	}
}

static void
dumpExternal(TableInfo *tbinfo, PQExpBuffer query, PQExpBuffer q, PQExpBuffer delq)
{
		PGresult   *res;
		char	   *urilocations;
		char	   *execlocations;
		char	   *location;
		char	   *fmttype;
		char	   *fmtopts;
		char	   *command = NULL;
		char	   *rejlim;
		char	   *rejlimtype;
		char	   *errnspname;
		char	   *errtblname;
		char	   *extencoding;
		char	   *writable = NULL;
		char	   *tmpstring = NULL;
		char 	   *tabfmt = NULL;
		char	   *customfmt = NULL;
		bool		isweb = false;
		bool		iswritable = false;
		char	   *options;
		bool		gpdb5OrLater = isGPDB5000OrLater();
		char	   *on_clause;

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP EXTERNAL TABLE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		/* Now get required information from pg_exttable */
		if (gpdb5OrLater)
		{
			appendPQExpBuffer(query,
						  "SELECT x.urilocation, x.execlocation, x.fmttype, x.fmtopts, x.command, "
								  "x.rejectlimit, x.rejectlimittype, "
						      "(SELECT relname "
						          "FROM pg_catalog.pg_class "
								  "WHERE Oid=x.fmterrtbl) AS errtblname, "
								  "x.fmterrtbl = x.reloid AS errortofile , "
								  "pg_catalog.pg_encoding_to_char(x.encoding), "
								  "x.writable, "
								  "array_to_string(ARRAY( "
								  "SELECT pg_catalog.quote_ident(option_name) || ' ' || "
								  "pg_catalog.quote_literal(option_value) "
								  "FROM pg_options_to_table(x.options) "
								  "ORDER BY option_name"
								  "), E',\n    ') AS options "
						  "FROM pg_catalog.pg_exttable x, pg_catalog.pg_class c "
						  "WHERE x.reloid = c.oid AND c.oid = '%u'::oid ", tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 80214)
		{
			appendPQExpBuffer(query,
					   "SELECT x.location, x.fmttype, x.fmtopts, x.command, "
							  "x.rejectlimit, x.rejectlimittype, "
						 "n.nspname AS errnspname, d.relname AS errtblname, "
					"pg_catalog.pg_encoding_to_char(x.encoding), x.writable "
							  "FROM pg_catalog.pg_class c "
					 "JOIN pg_catalog.pg_exttable x ON ( c.oid = x.reloid ) "
				"LEFT JOIN pg_catalog.pg_class d ON ( d.oid = x.fmterrtbl ) "
							  "LEFT JOIN pg_catalog.pg_namespace n ON ( n.oid = d.relnamespace ) "
							  "WHERE c.oid = '%u'::oid ",
							  tbinfo->dobj.catId.oid);
		}
		else if (g_fout->remoteVersion >= 80205)
		{

			appendPQExpBuffer(query,
					   "SELECT x.location, x.fmttype, x.fmtopts, x.command, "
							  "x.rejectlimit, x.rejectlimittype, "
						 "n.nspname AS errnspname, d.relname AS errtblname, "
			  "pg_catalog.pg_encoding_to_char(x.encoding), null as writable "
							  "FROM pg_catalog.pg_class c "
					 "JOIN pg_catalog.pg_exttable x ON ( c.oid = x.reloid ) "
				"LEFT JOIN pg_catalog.pg_class d ON ( d.oid = x.fmterrtbl ) "
							  "LEFT JOIN pg_catalog.pg_namespace n ON ( n.oid = d.relnamespace ) "
							  "WHERE c.oid = '%u'::oid ",
							  tbinfo->dobj.catId.oid);
		}
		else
		{
			/* not SREH and encoding colums yet */
			appendPQExpBuffer(query,
					   "SELECT x.location, x.fmttype, x.fmtopts, x.command, "
							  "-1 as rejectlimit, null as rejectlimittype,"
							  "null as errnspname, null as errtblname, "
							  "null as encoding, null as writable "
					  "FROM pg_catalog.pg_exttable x, pg_catalog.pg_class c "
							  "WHERE x.reloid = c.oid AND c.oid = '%u'::oid",
							  tbinfo->dobj.catId.oid);
		}

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) != 1)
		{
			if (PQntuples(res) < 1)
				write_msg(NULL, "query to obtain definition of external table "
						  "\"%s\" returned no data\n",
						  tbinfo->dobj.name);
			else
				write_msg(NULL, "query to obtain definition of external table "
						  "\"%s\" returned more than one definition\n",
						  tbinfo->dobj.name);
			exit_nicely();

		}


		if (gpdb5OrLater)
		{
			urilocations = PQgetvalue(res, 0, 0);
			execlocations = PQgetvalue(res, 0, 1);
			fmttype = PQgetvalue(res, 0, 2);
			fmtopts = PQgetvalue(res, 0, 3);
			command = PQgetvalue(res, 0, 4);
			rejlim = PQgetvalue(res, 0, 5);
			rejlimtype = PQgetvalue(res, 0, 6);
			errnspname = PQgetvalue(res, 0, 7);
			errtblname = PQgetvalue(res, 0, 8);
			extencoding = PQgetvalue(res, 0, 9);
			writable = PQgetvalue(res, 0, 10);
			options = PQgetvalue(res, 0, 11);

			on_clause = execlocations;
		}
		else
		{
			urilocations = PQgetvalue(res, 0, 0);
			fmttype = PQgetvalue(res, 0, 1);
			fmtopts = PQgetvalue(res, 0, 2);
			command = PQgetvalue(res, 0, 3);
			rejlim = PQgetvalue(res, 0, 4);
			rejlimtype = PQgetvalue(res, 0, 5);
			errnspname = PQgetvalue(res, 0, 6);
			errtblname = PQgetvalue(res, 0, 7);
			extencoding = PQgetvalue(res, 0, 8);
			writable = PQgetvalue(res, 0, 9);
			execlocations = "";
			options = "";

			if (command && strlen(command) > 0)
				on_clause = command;
			else
				on_clause = NULL;
		}

		if ((command && strlen(command) > 0) ||
			(strncmp(urilocations + 1, "http", strlen("http")) == 0))
			isweb = true;

		if (writable && writable[0] == 't')
			iswritable = true;

		appendPQExpBuffer(q, "CREATE %sEXTERNAL %sTABLE %s (",
						  (iswritable ? "WRITABLE " : ""),
						  (isweb ? "WEB " : ""),
						  fmtId(tbinfo->dobj.name));

		int actual_atts = 0;
		int j;
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/* Is the attribute not dropped? */
			if (shouldPrintColumn(tbinfo, j))
			{
				/* Format properly if not first attr */
				if (actual_atts > 0)
					appendPQExpBuffer(q, ",");
				appendPQExpBuffer(q, "\n    ");

				/* Attribute name */
				appendPQExpBuffer(q, "%s ",
								  fmtId(tbinfo->attnames[j]));

				/* Attribute type */
				appendPQExpBuffer(q, "%s",
								  tbinfo->atttypnames[j]);

				actual_atts++;
			}
		}

		appendPQExpBuffer(q, "\n)");

		if (command && strlen(command) > 0)
		{
			/* add EXECUTE clause */
			tmpstring = escape_backslashes(command, true);
			appendPQExpBuffer(q, " EXECUTE E'%s' ", tmpstring);
			free(tmpstring);
			tmpstring = NULL;
		}
		else
		{
			/* add LOCATION clause, remove '{"' and '"}' */
			urilocations[strlen(urilocations) - 1] = '\0';
			urilocations++;

			/* the URI of custom protocol will contains \"\" and need to be removed */

			location = nextToken(&urilocations, ",");

			if (location[0] == '\"')
			{
				location++;
				location[strlen(location) - 1] = '\0';
			}
			appendPQExpBuffer(q, " LOCATION (\n    '%s'", location);
			for (; (location = nextToken(&urilocations, ",")) != NULL;)
			{
				if (location[0] == '\"')
				{
					location++;
					location[strlen(location) - 1] = '\0';
				}
				appendPQExpBuffer(q, ",\n    '%s'", location);
			}
			appendPQExpBuffer(q, "\n) ");
		}

		/*
		 * Add ON clause (unless WRITABLE table, which doesn't allow ON).
		 * ON clauses were up until 5.0 supported only on EXECUTE, in 5.0
		 * and thereafter they are allowed on all external tables.
		 */
		if (!iswritable && on_clause)
		{
			/* remove curly braces */
			on_clause[strlen(on_clause) - 1] = '\0';
			on_clause++;

			if (strncmp(on_clause, "HOST:", strlen("HOST:")) == 0)
				appendPQExpBuffer(q, "ON HOST '%s' ", on_clause + strlen("HOST:"));
			else if (strncmp(on_clause, "PER_HOST", strlen("PER_HOST")) == 0)
				appendPQExpBufferStr(q, "ON HOST ");
			else if (strncmp(on_clause, "MASTER_ONLY", strlen("MASTER_ONLY")) == 0)
				appendPQExpBufferStr(q, "ON MASTER ");
			else if (strncmp(on_clause, "SEGMENT_ID:", strlen("SEGMENT_ID:")) == 0)
				appendPQExpBuffer(q, "ON SEGMENT %s ", on_clause + strlen("SEGMENT_ID:"));
			else if (strncmp(on_clause, "TOTAL_SEGS:", strlen("TOTAL_SEGS:")) == 0)
				appendPQExpBuffer(q, "ON %s ", on_clause + strlen("TOTAL_SEGS:"));
			else if (strncmp(on_clause, "ALL_SEGMENTS", strlen("ALL_SEGMENTS")) == 0)
				appendPQExpBufferStr(q, "ON ALL ");
			else
			{
				write_msg(NULL, "illegal ON clause catalog information \"%s\" "
						  "for command '%s' on table \"%s\"\n",
						  on_clause, command, fmtId(tbinfo->dobj.name));
				exit_nicely();
			}
		}
		appendPQExpBufferChar(q, '\n');

		/* add FORMAT clause */
		tmpstring = escape_fmtopts_string((const char *) fmtopts);

		switch (fmttype[0])
		{
			case 't':
				tabfmt = "text";
				break;
			case 'b':
				/*
				 * b denotes that a custom format is used.
				 * the fmtopts string should be formatted as:
				 * a1 = 'val1',...,an = 'valn'
				 *
				 */
				tabfmt = "custom";
				customfmt = custom_fmtopts_string(tmpstring);
				break;
			case 'a':
				tabfmt = "avro";
				customfmt = custom_fmtopts_string(tmpstring);
				break;
			case 'p':
				tabfmt = "parquet";
				customfmt = custom_fmtopts_string(tmpstring);
				break;
			default:
				tabfmt = "csv";
		}
		appendPQExpBuffer(q, "FORMAT '%s' (%s)\n",
						  tabfmt,
						  customfmt ? customfmt : tmpstring);
		free(tmpstring);
		tmpstring = NULL;
		if (customfmt)
		{
			free(customfmt);
			customfmt = NULL;
		}

		if (gpdb5OrLater)
		{
			appendPQExpBuffer(q, "OPTIONS (\n %s\n )\n", options);
		}

		if (g_fout->remoteVersion >= 80205)
		{
			/* add ENCODING clause */
			appendPQExpBuffer(q, "ENCODING '%s'", extencoding);

			/* add Single Row Error Handling clause (if any) */
			if (rejlim && strlen(rejlim) > 0)
			{
				appendPQExpBuffer(q, "\n");

				/*
				 * Error tables were removed in 5.0 and replaced with file
				 * error logging. The catalog syntax for identifying error
				 * logging is however still using the pg_exttable.fmterrtbl
				 * attribute so we use the errtblname for emitting LOG ERRORS.
				 */
				if (errtblname && strlen(errtblname) > 0)
					appendPQExpBuffer(q, "LOG ERRORS ");

				/* reject limit */
				appendPQExpBuffer(q, "SEGMENT REJECT LIMIT %s", rejlim);

				/* reject limit type */
				if (rejlimtype[0] == 'r')
					appendPQExpBuffer(q, " ROWS");
				else
					appendPQExpBuffer(q, " PERCENT");
			}
		}

		/* DISTRIBUTED BY clause (if WRITABLE table) */
		if (iswritable)
			addDistributedBy(q, tbinfo, actual_atts);

		appendPQExpBuffer(q, ";\n");

		PQclear(res);
}

/*
 * dumpTableSchema
 *	  write the declaration (not data) of one user-defined table or view
 */
static void
dumpTableSchema(Archive *fout, TableInfo *tbinfo)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PGresult   *res;
	int			numParents;
	TableInfo **parents;
	int			actual_atts;	/* number of attrs in this CREATE statment */
	char	   *reltypename;
	char	   *storage;
	int			j,
				k;

	/* Make sure we are in proper schema */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	/* Is it a table or a view? */
	if (tbinfo->relkind == RELKIND_VIEW)
	{
		char	   *viewdef;

		reltypename = "VIEW";

		/* Fetch the view definition */
		appendPQExpBuffer(query,
		 "SELECT pg_catalog.pg_get_viewdef('%u'::pg_catalog.oid) as viewdef",
						  tbinfo->dobj.catId.oid);

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) != 1)
		{
			if (PQntuples(res) < 1)
				mpp_err_msg(logError, progname, "query to obtain definition of view \"%s\" returned no data\n",
							tbinfo->dobj.name);
			else
				mpp_err_msg(logError, progname, "query to obtain definition of view \"%s\" returned more than one definition\n",
							tbinfo->dobj.name);
			exit_nicely();
		}

		viewdef = PQgetvalue(res, 0, 0);

		if (strlen(viewdef) == 0)
		{
			mpp_err_msg(logError, progname, "definition of view \"%s\" appears to be empty (length zero)\n",
						tbinfo->dobj.name);
			exit_nicely();
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP VIEW %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		appendPQExpBuffer(q, "CREATE VIEW %s AS\n    %s\n",
						  fmtId(tbinfo->dobj.name), viewdef);

		PQclear(res);
	}
	/* START MPP ADDITION */
	else if (tbinfo->relstorage == RELSTORAGE_EXTERNAL)
	{
		reltypename = "EXTERNAL TABLE";
		dumpExternal(tbinfo, query, q, delq);
	}
	else if (tbinfo->relstorage == RELSTORAGE_FOREIGN)
	{
		char	   *srvname = NULL;
		char	   *tbloptions = NULL;

		reltypename = "FOREIGN TABLE";

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP FOREIGN TABLE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		/* Now get required information from pg_foreign_table */
		appendPQExpBuffer(query,
						  "SELECT s.srvname, array_to_string(ARRAY("
				  "SELECT option_name || ' ' || quote_literal(option_value) "
			  "FROM pg_options_to_table(f.tbloptions)), ', ') AS tbloptions "
						  "FROM pg_catalog.pg_foreign_table f, pg_catalog.pg_class c, pg_catalog.pg_foreign_server s "
		"WHERE c.oid = f.reloid AND f.server = s.oid AND c.oid = '%u'::oid ",
						  tbinfo->dobj.catId.oid);

		res = PQexec(g_conn, query->data);
		check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) != 1)
		{
			if (PQntuples(res) < 1)
				write_msg(NULL, "query to obtain definition of foreign table "
						  "\"%s\" returned no data\n",
						  tbinfo->dobj.name);
			else
				write_msg(NULL, "query to obtain definition of foreign table "
						  "\"%s\" returned more than one definition\n",
						  tbinfo->dobj.name);
			exit_nicely();

		}

		srvname = PQgetvalue(res, 0, 0);
		if (!PQgetisnull(res, 0, 1))
			tbloptions = PQgetvalue(res, 0, 1);

		appendPQExpBuffer(q, "CREATE FOREIGN TABLE %s (", fmtId(tbinfo->dobj.name));

		actual_atts = 0;
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/* Is this one of the table's own attrs, and not dropped ? */
			if (tbinfo->attislocal[j] && !tbinfo->attisdropped[j])
			{
				/* Format properly if not first attr */
				if (actual_atts > 0)
					appendPQExpBuffer(q, ",");
				appendPQExpBuffer(q, "\n    ");

				/* Attribute name */
				appendPQExpBuffer(q, "%s ",
								  fmtId(tbinfo->attnames[j]));

				/* Attribute type */
				appendPQExpBuffer(q, "%s",
								  tbinfo->atttypnames[j]);

				actual_atts++;
			}
		}

		appendPQExpBuffer(q, "\n) ");
		appendPQExpBuffer(q, "SERVER %s ", fmtId(srvname));

		if (tbloptions)
			appendPQExpBuffer(q, "OPTIONS (%s)", tbloptions);

		appendPQExpBuffer(q, ";\n");
		PQclear(res);
	}
	/* END MPP ADDITION */
	else
	{
		reltypename = "TABLE";
		numParents = tbinfo->numParents;
		parents = tbinfo->parents;

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP TABLE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		appendPQExpBuffer(q, "CREATE TABLE %s (",
						  fmtId(tbinfo->dobj.name));

		actual_atts = 0;
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/* Is this one of the table's own attrs, and not dropped ? */
			if (tbinfo->attislocal[j] && !tbinfo->attisdropped[j])
			{
				/* Format properly if not first attr */
				if (actual_atts > 0)
					appendPQExpBuffer(q, ",");
				appendPQExpBuffer(q, "\n    ");

				/* Attribute name */
				appendPQExpBuffer(q, "%s ",
								  fmtId(tbinfo->attnames[j]));

				/* Attribute type */
				appendPQExpBuffer(q, "%s",
								  tbinfo->atttypnames[j]);

				/*
				 * Default value --- suppress if inherited
				 * and not to be printed separately.
				 */
				if (tbinfo->attrdefs[j] != NULL &&
					!tbinfo->attrdefs[j]->separate)
					appendPQExpBuffer(q, " DEFAULT %s",
									  tbinfo->attrdefs[j]->adef_expr);

				/*
				 * Not Null constraint --- suppress if inherited
				 */
				if (tbinfo->notnull[j] && !tbinfo->inhNotNull[j])
					appendPQExpBuffer(q, " NOT NULL");

				/* Column Storage attributes */
				if (tbinfo->attencoding[j] != NULL)
					appendPQExpBuffer(q, " ENCODING (%s)",
										tbinfo->attencoding[j]);

				actual_atts++;
			}
		}

		/*
		 * Add non-inherited CHECK constraints, if any.
		 */
		for (j = 0; j < tbinfo->ncheck; j++)
		{
			ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

			if (constr->separate || !constr->conislocal)
				continue;

			if (actual_atts > 0)
				appendPQExpBuffer(q, ",\n    ");

			appendPQExpBuffer(q, "CONSTRAINT %s ",
							  fmtId(constr->dobj.name));
			appendPQExpBuffer(q, "%s", constr->condef);

			actual_atts++;
		}

		appendPQExpBuffer(q, "\n)");

		/*
		 * Emit the INHERITS clause if this table has parents.
		 */
		if (numParents > 0)
		{
			appendPQExpBuffer(q, "\nINHERITS (");
			for (k = 0; k < numParents; k++)
			{
				TableInfo  *parentRel = parents[k];

				if (k > 0)
					appendPQExpBuffer(q, ", ");
				if (parentRel->dobj.namespace != tbinfo->dobj.namespace)
					appendPQExpBuffer(q, "%s.",
								fmtId(parentRel->dobj.namespace->dobj.name));
				appendPQExpBuffer(q, "%s",
								  fmtId(parentRel->dobj.name));
			}
			appendPQExpBuffer(q, ")");
		}

		if (tbinfo->reloptions && strlen(tbinfo->reloptions) > 0)
			appendPQExpBuffer(q, "\nWITH (%s)", tbinfo->reloptions);

		/* START MPP ADDITION */

		/* dump distributed by clause */
		addDistributedBy(q, tbinfo, actual_atts);

		/* lets see about partitioning */
		if (g_gp_supportsPartitioning)
		{
			resetPQExpBuffer(query);

			/* MPP-6297: dump by tablename */
			if (g_gp_supportsPartitionTemplates)
				/* use 4.x version of function */
				appendPQExpBuffer(query, "SELECT "
				   "pg_get_partition_def('%u'::pg_catalog.oid, true, true) ",
								  tbinfo->dobj.catId.oid);
			else	/* use 3.x version of function */
				appendPQExpBuffer(query, "SELECT "
						 "pg_get_partition_def('%u'::pg_catalog.oid, true) ",
								  tbinfo->dobj.catId.oid);

			res = PQexec(g_conn, query->data);
			check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

			if (PQntuples(res) != 1)
			{
				if (PQntuples(res) < 1)
					write_msg(NULL, "query to obtain definition of table \"%s\" returned no data\n",
							  tbinfo->dobj.name);
				else
					write_msg(NULL, "query to obtain definition of table \"%s\" returned more than one definition\n",
							  tbinfo->dobj.name);
				exit_nicely();
			}
			if (!PQgetisnull(res, 0, 0))
				appendPQExpBuffer(q, " %s", PQgetvalue(res, 0, 0));

			PQclear(res);

			/*
			 * MPP-6095: dump ALTER TABLE statements for subpartition
			 * templates
			 */
			if (g_gp_supportsPartitionTemplates)
			{
				resetPQExpBuffer(query);

				appendPQExpBuffer(
								  query, "SELECT "
								  "pg_get_partition_template_def('%u'::pg_catalog.oid, true, true) ",
								  tbinfo->dobj.catId.oid);

				res = PQexec(g_conn, query->data);
				check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

				if (PQntuples(res) != 1)
				{
					if (PQntuples(res) < 1)
						write_msg(
								  NULL,
								  "query to obtain definition of table \"%s\" returned no data\n",
								  tbinfo->dobj.name);
					else
						write_msg(
								  NULL,
								  "query to obtain definition of table \"%s\" returned more than one definition\n",
								  tbinfo->dobj.name);
					exit_nicely();
				}

				/*
				 * MPP-9537: terminate (with semicolon) the previous
				 * statement, and dump the template definitions
				 */
				if (!PQgetisnull(res, 0, 0) &&
					PQgetlength(res, 0, 0))
					appendPQExpBuffer(q, ";\n %s", PQgetvalue(res, 0, 0));

				PQclear(res);
			}
		}

		/* END MPP ADDITION */

		/* Exchange external partition */
		if (g_gp_supportsPartitioning)
		{
			int i = 0;
			int ntups = 0;
			char *relname = NULL;
			char *parname = NULL;
			int i_relname = 0;
			int i_parname = 0;
			resetPQExpBuffer(query);

			appendPQExpBuffer(query, "SELECT "
						      "c.relname, pr.parname FROM pg_partition_rule pr "
						      "join pg_class c ON pr.parchildrelid = c.oid "
						      "join pg_partition p ON p.oid = pr.paroid "
						      "WHERE p.parrelid = %u AND c.relstorage = 'x';", tbinfo->dobj.catId.oid);

			res = PQexec(g_conn, query->data);
			check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

			ntups = PQntuples(res);
			i_relname = PQfnumber(res, "relname");
			i_parname = PQfnumber(res, "parname");

			appendPQExpBuffer(q, ";\n");

			for (i = 0; i < ntups; i++)
			{
				char tmpExtTable[500] = {0};
				relname = pg_strdup(PQgetvalue(res, i, i_relname));
				parname = pg_strdup(PQgetvalue(res, i, i_parname));
				snprintf(tmpExtTable, sizeof(tmpExtTable), "%s%s", relname, EXT_PARTITION_NAME_POSTFIX);
				appendPQExpBuffer(q, "ALTER TABLE %s.", fmtId(tbinfo->dobj.namespace->dobj.name));
				appendPQExpBuffer(q, "%s ", fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(q, "EXCHANGE PARTITION %s ", fmtId(parname));
				appendPQExpBuffer(q, "WITH TABLE %s WITHOUT VALIDATION; ", fmtId(tmpExtTable));

				appendPQExpBuffer(q, "\n");

				appendPQExpBuffer(q, "DROP TABLE %s; ", fmtId(tmpExtTable));

				appendPQExpBuffer(q, "\n");
				if (relname)
					free(relname);
				if (parname)
					free(parname);
			}

			PQclear(res);
		}

		appendPQExpBuffer(q, "\n");

		/*
		 * MPP-25549: Dump ALTER statements for subpartition tables being
		 * set to different schema other than the parent
		 */
		if (g_gp_supportsPartitioning)
		{
			int ntups = 0;
			int i = 0;
			int i_schemaname = 0;
			int i_relname = 0;
			char *schemaname = NULL;
			char *relname = NULL;
			resetPQExpBuffer(query);

			appendPQExpBuffer(query,
			    "SELECT partitionschemaname, partitiontablename FROM pg_catalog.pg_partitions "
				" WHERE partitionschemaname != schemaname AND schemaname = ");
			appendStringLiteralConn(query, tbinfo->dobj.namespace->dobj.name, g_conn);
			appendPQExpBuffer(query, " AND tablename = ");
			appendStringLiteralConn(query, tbinfo->dobj.name, g_conn);

			res = PQexec(g_conn, query->data);
			check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

			ntups = PQntuples(res);
			i_relname = PQfnumber(res, "partitiontablename");
			i_schemaname = PQfnumber(res, "partitionschemaname");

			for (i = 0; i < ntups; i++)
			{
				schemaname = pg_strdup(PQgetvalue(res, i, i_schemaname));
				relname = pg_strdup(PQgetvalue(res, i, i_relname));
				appendPQExpBuffer(q, "ALTER TABLE %s ", fmtId(relname));
				appendPQExpBuffer(q, "SET SCHEMA %s;", fmtId(schemaname));

				appendPQExpBuffer(q, "\n");
				free(schemaname);
				free(relname);
			}

			PQclear(res);
		}

		/* MPP-1890 */
		if (numParents > 0)
			DetectChildConstraintDropped(tbinfo, q);

		/* Loop dumping statistics and storage statements */
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/*
			 * Dump per-column statistics information. We only issue an ALTER
			 * TABLE statement if the attstattarget entry for this column is
			 * non-negative (i.e. it's not the default value)
			 */
			if (tbinfo->attstattarget[j] >= 0 &&
				!tbinfo->attisdropped[j])
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(q, "ALTER COLUMN %s ",
								  fmtId(tbinfo->attnames[j]));
				appendPQExpBuffer(q, "SET STATISTICS %d;\n",
								  tbinfo->attstattarget[j]);
			}

			/*
			 * Dump per-column storage information.  The statement is only
			 * dumped if the storage has been changed from the type's default.
			 * An inherited column can have
			 * its storage type changed independently from the parent
			 * specification.
			 */
			if (!tbinfo->attisdropped[j] && tbinfo->attstorage[j] != tbinfo->typstorage[j])
			{
				switch (tbinfo->attstorage[j])
				{
					case 'p':
						storage = "PLAIN";
						break;
					case 'e':
						storage = "EXTERNAL";
						break;
					case 'm':
						storage = "MAIN";
						break;
					case 'x':
						storage = "EXTENDED";
						break;
					default:
						storage = NULL;
				}

				/*
				 * Only dump the statement if it's a storage type we recognize
				 */
				if (storage != NULL)
				{
					appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
									  fmtId(tbinfo->dobj.name));
					appendPQExpBuffer(q, "ALTER COLUMN %s ",
									  fmtId(tbinfo->attnames[j]));
					appendPQExpBuffer(q, "SET STORAGE %s;\n",
									  storage);
				}
			}
		}
	}

	ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
			(tbinfo->relkind == RELKIND_VIEW) ? NULL : tbinfo->reltablespace,
				 tbinfo->rolname,
				 (strcmp(reltypename, "TABLE") == 0 ||
				  strcmp(reltypename, "EXTERNAL TABLE") == 0 ||
		strcmp(reltypename, "FOREIGN TABLE") == 0) ? tbinfo->hasoids : false,
				 reltypename, q->data, delq->data, NULL,
				 tbinfo->dobj.dependencies, tbinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump Table Comments */
	dumpTableComment(fout, tbinfo, reltypename);

	/* Dump comments on inlined table constraints */
	for (j = 0; j < tbinfo->ncheck; j++)
	{
		ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

		if (constr->separate || !constr->conislocal)
			continue;

		dumpTableConstraintComment(fout, constr);
	}

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpAttrDef --- dump an attribute's default-value declaration
 */
static void
dumpAttrDef(Archive *fout, AttrDefInfo *adinfo)
{
	TableInfo  *tbinfo = adinfo->adtable;
	int			adnum = adinfo->adnum;
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Only print it if "separate" mode is selected */
	if (!tbinfo->dobj.dump || !adinfo->separate || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	appendPQExpBuffer(q, "ALTER TABLE %s ",
					  fmtId(tbinfo->dobj.name));
	appendPQExpBuffer(q, "ALTER COLUMN %s SET DEFAULT %s;\n",
					  fmtId(tbinfo->attnames[adnum - 1]),
					  adinfo->adef_expr);

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delq, "ALTER TABLE %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delq, "%s ",
					  fmtId(tbinfo->dobj.name));
	appendPQExpBuffer(delq, "ALTER COLUMN %s DROP DEFAULT;\n",
					  fmtId(tbinfo->attnames[adnum - 1]));

	ArchiveEntry(fout, adinfo->dobj.catId, adinfo->dobj.dumpId,
				 tbinfo->attnames[adnum - 1],
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname,
				 false, "DEFAULT", q->data, delq->data, NULL,
				 adinfo->dobj.dependencies, adinfo->dobj.nDeps,
				 NULL, NULL);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * getAttrName: extract the correct name for an attribute
 *
 * The array tblInfo->attnames[] only provides names of user attributes;
 * if a system attribute number is supplied, we have to fake it.
 * We also do a little bit of bounds checking for safety's sake.
 */
static const char *
getAttrName(int attrnum, TableInfo *tblInfo)
{
	if (attrnum > 0 && attrnum <= tblInfo->numatts)
		return tblInfo->attnames[attrnum - 1];
	switch (attrnum)
	{
		case SelfItemPointerAttributeNumber:
			return "ctid";
		case ObjectIdAttributeNumber:
			return "oid";
		case MinTransactionIdAttributeNumber:
			return "xmin";
		case MinCommandIdAttributeNumber:
			return "cmin";
		case MaxTransactionIdAttributeNumber:
			return "xmax";
		case MaxCommandIdAttributeNumber:
			return "cmax";
		case TableOidAttributeNumber:
			return "tableoid";
	}
	mpp_err_msg(logError, progname, "invalid column number %d for table \"%s\"\n",
				attrnum, tblInfo->dobj.name);
	exit_nicely();
	return NULL;				/* keep compiler quiet */
}

/*
 * dumpIndex
 *	  write out to fout a user-defined index
 */
static void
dumpIndex(Archive *fout, IndxInfo *indxinfo)
{
	TableInfo  *tbinfo = indxinfo->indextable;
	PQExpBuffer q;
	PQExpBuffer delq;

	if (dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/*
	 * If there's an associated constraint, don't dump the index per se, but
	 * do dump any comment for it.	(This is safe because dependency ordering
	 * will have ensured the constraint is emitted first.)
	 */
	if (indxinfo->indexconstraint == 0)
	{
		/* Plain secondary index */
		appendPQExpBuffer(q, "%s;\n", indxinfo->indexdef);

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, " ON %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP INDEX IF EXISTS %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, "%s;\n",
						  fmtId(indxinfo->dobj.name));

		ArchiveEntry(fout, indxinfo->dobj.catId, indxinfo->dobj.dumpId,
					 indxinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "INDEX", q->data, delq->data, NULL,
					 indxinfo->dobj.dependencies, indxinfo->dobj.nDeps,
					 NULL, NULL);
	}

	/* Dump Index Comments */
	resetPQExpBuffer(q);
	appendPQExpBuffer(q, "INDEX %s",
					  fmtId(indxinfo->dobj.name));
	dumpComment(fout, q->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				indxinfo->dobj.catId, 0, indxinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpConstraint
 *	  write out to fout a user-defined constraint
 */
static void
dumpConstraint(Archive *fout, ConstraintInfo *coninfo)
{
	TableInfo  *tbinfo = coninfo->contable;
	PQExpBuffer q;
	PQExpBuffer delq;

	/* Skip if not to be dumped */
	if (!coninfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	if (coninfo->contype == 'p' || coninfo->contype == 'u')
	{
		/* Index-related constraint */
		IndxInfo   *indxinfo;
		int			k;

		indxinfo = (IndxInfo *) findObjectByDumpId(coninfo->conindex);

		if (indxinfo == NULL)
		{
			mpp_err_msg(logError, progname, "missing index for constraint \"%s\"\n",
						coninfo->dobj.name);
			exit_nicely();
		}

		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s (",
						  fmtId(coninfo->dobj.name),
						  coninfo->contype == 'p' ? "PRIMARY KEY" : "UNIQUE");

		for (k = 0; k < indxinfo->indnkeys; k++)
		{
			int			indkey = (int) indxinfo->indkeys[k];
			const char *attname;

			if (indkey == InvalidAttrNumber)
				break;
			attname = getAttrName(indkey, tbinfo);

			appendPQExpBuffer(q, "%s%s",
							  (k == 0) ? "" : ", ",
							  fmtId(attname));
		}

		appendPQExpBuffer(q, ")");

		if (indxinfo->options && strlen(indxinfo->options) > 0)
			appendPQExpBuffer(q, " WITH (%s)", indxinfo->options);

		appendPQExpBuffer(q, ";\n");

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, " ON %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "select pg_temp.drop_table_constraint_only_if_exists('%s',",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, " '%s',",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(delq, " '%s');\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "CONSTRAINT", q->data, delq->data, NULL,
					 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'f')
	{
		/*
		 * XXX Potentially wrap in a 'SET CONSTRAINTS OFF' block so that the
		 * current table data is not processed
		 */
		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
						  fmtId(coninfo->dobj.name),
						  coninfo->condef);

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "select pg_temp.drop_table_constraint_only_if_exists('%s',",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delq, " '%s',",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(delq, " '%s');\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname, false,
					 "FK CONSTRAINT", q->data, delq->data, NULL,
					 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'c' && tbinfo)
	{
		/* CHECK constraint on a table */

		/* Ignore if not to be dumped separately */
		if (coninfo->separate)
		{
			/* not ONLY since we want it to propagate to children */
			appendPQExpBuffer(q, "ALTER TABLE %s\n",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			/*
			 * DROP must be fully qualified in case same name appears in
			 * pg_catalog
			 */
			appendPQExpBuffer(delq, "ALTER TABLE %s.",
							  fmtId(tbinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(delq, "%s ",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname, false,
						 "CHECK CONSTRAINT", q->data, delq->data, NULL,
						 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
						 NULL, NULL);
		}
	}
	else if (coninfo->contype == 'c' && tbinfo == NULL)
	{
		/* CHECK constraint on a domain */
		TypeInfo   *tinfo = coninfo->condomain;

		/* Ignore if not to be dumped separately */
		if (coninfo->separate)
		{
			appendPQExpBuffer(q, "ALTER DOMAIN %s\n",
							  fmtId(tinfo->dobj.name));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			/*
			 * DROP must be fully qualified in case same name appears in
			 * pg_catalog
			 */
			appendPQExpBuffer(delq, "ALTER DOMAIN %s.",
							  fmtId(tinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(delq, "%s ",
							  fmtId(tinfo->dobj.name));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tinfo->dobj.namespace->dobj.name,
						 NULL,
						 tinfo->rolname, false,
						 "CHECK CONSTRAINT", q->data, delq->data, NULL,
						 coninfo->dobj.dependencies, coninfo->dobj.nDeps,
						 NULL, NULL);
		}
	}
	else
	{
		mpp_err_msg(logError, progname, "unrecognized constraint type: %c\n", coninfo->contype);
		exit_nicely();
	}

	/* Dump Constraint Comments --- only works for table constraints */
	if (tbinfo && coninfo->separate)
		dumpTableConstraintComment(fout, coninfo);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpTableConstraintComment --- dump a constraint's comment if any
 *
 * This is split out because we need the function in two different places
 * depending on whether the constraint is dumped as part of CREATE TABLE
 * or as a separate ALTER command.
 */
static void
dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo)
{
	TableInfo  *tbinfo = coninfo->contable;
	PQExpBuffer q = createPQExpBuffer();

	appendPQExpBuffer(q, "CONSTRAINT %s ",
					  fmtId(coninfo->dobj.name));
	appendPQExpBuffer(q, "ON %s",
					  fmtId(tbinfo->dobj.name));
	dumpComment(fout, q->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				coninfo->dobj.catId, 0,
			 coninfo->separate ? coninfo->dobj.dumpId : tbinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
}

static void
dumpSequence(Archive *fout, TableInfo *tbinfo)
{
	PGresult   *res;
	char	   *last,
			   *incby,
			   *maxv = NULL,
			   *minv = NULL,
			   *cache;
	char		bufm[100],
				bufx[100];
	bool		cycled,
				called;
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer delqry = createPQExpBuffer();

	/* Make sure we are in proper schema */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	snprintf(bufm, sizeof(bufm), INT64_FORMAT, SEQ_MINVALUE);
	snprintf(bufx, sizeof(bufx), INT64_FORMAT, SEQ_MAXVALUE);

	appendPQExpBuffer(query,
					  "SELECT sequence_name, last_value, increment_by, "
				   "CASE WHEN increment_by > 0 AND max_value = %s THEN NULL "
				   "     WHEN increment_by < 0 AND max_value = -1 THEN NULL "
					  "     ELSE max_value "
					  "END AS max_value, "
					"CASE WHEN increment_by > 0 AND min_value = 1 THEN NULL "
				   "     WHEN increment_by < 0 AND min_value = %s THEN NULL "
					  "     ELSE min_value "
					  "END AS min_value, "
					  "cache_value, is_cycled, is_called from %s",
					  bufx, bufm,
					  fmtId(tbinfo->dobj.name));

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		mpp_err_msg(logError, progname, "query to get data of sequence \"%s\" returned %d rows (expected 1)\n",
					tbinfo->dobj.name, PQntuples(res));
		exit_nicely();
	}

	/* Disable this check: it fails if sequence has been renamed */
#ifdef NOT_USED
	if (strcmp(PQgetvalue(res, 0, 0), tbinfo->dobj.name) != 0)
	{
		mpp_err_msg(logError, progname, "query to get data of sequence \"%s\" returned name \"%s\"\n",
					tbinfo->dobj.name, PQgetvalue(res, 0, 0));
		exit_nicely();
	}
#endif

	last = PQgetvalue(res, 0, 1);
	incby = PQgetvalue(res, 0, 2);
	if (!PQgetisnull(res, 0, 3))
		maxv = PQgetvalue(res, 0, 3);
	if (!PQgetisnull(res, 0, 4))
		minv = PQgetvalue(res, 0, 4);
	cache = PQgetvalue(res, 0, 5);
	cycled = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);
	called = (strcmp(PQgetvalue(res, 0, 7), "t") == 0);

	/*
	 * The logic we use for restoring sequences is as follows:
	 *
	 * Add a CREATE SEQUENCE statement as part of a "schema" dump (use
	 * last_val for start if called is false, else use min_val for start_val).
	 * Also, if the sequence is owned by a column, add an ALTER SEQUENCE OWNED
	 * BY command for it.
	 *
	 * Add a 'SETVAL(seq, last_val, iscalled)' as part of a "data" dump.
	 */
	if (!dataOnly)
	{
		resetPQExpBuffer(delqry);

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delqry, "DROP SEQUENCE %s.",
						  fmtId(tbinfo->dobj.namespace->dobj.name));
		appendPQExpBuffer(delqry, "%s;\n",
						  fmtId(tbinfo->dobj.name));

		resetPQExpBuffer(query);
		appendPQExpBuffer(query,
						  "CREATE SEQUENCE %s\n",
						  fmtId(tbinfo->dobj.name));

		if (!called)
			appendPQExpBuffer(query, "    START WITH %s\n", last);

		appendPQExpBuffer(query, "    INCREMENT BY %s\n", incby);

		if (maxv)
			appendPQExpBuffer(query, "    MAXVALUE %s\n", maxv);
		else
			appendPQExpBuffer(query, "    NO MAXVALUE\n");

		if (minv)
			appendPQExpBuffer(query, "    MINVALUE %s\n", minv);
		else
			appendPQExpBuffer(query, "    NO MINVALUE\n");

		appendPQExpBuffer(query,
						  "    CACHE %s%s",
						  cache, (cycled ? "\n    CYCLE" : ""));

		appendPQExpBuffer(query, ";\n");

		ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
					 tbinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname,
					 false, "SEQUENCE", query->data, delqry->data, NULL,
					 tbinfo->dobj.dependencies, tbinfo->dobj.nDeps,
					 NULL, NULL);

		/*
		 * If the sequence is owned by a table column, emit the ALTER for it
		 * as a separate TOC entry immediately following the sequence's own
		 * entry.  It's OK to do this rather than using full sorting logic,
		 * because the dependency that tells us it's owned will have forced
		 * the table to be created first.  We can't just include the ALTER in
		 * the TOC entry because it will fail if we haven't reassigned the
		 * sequence owner to match the table's owner.
		 *
		 * We need not schema-qualify the table reference because both
		 * sequence and table must be in the same schema.
		 */
		if (OidIsValid(tbinfo->owning_tab))
		{
			TableInfo  *owning_tab = findTableByOid(tbinfo->owning_tab);

			if (owning_tab && owning_tab->dobj.dump)
			{
				resetPQExpBuffer(query);
				appendPQExpBuffer(query, "ALTER SEQUENCE %s",
								  fmtId(tbinfo->dobj.name));
				appendPQExpBuffer(query, " OWNED BY %s",
								  fmtId(owning_tab->dobj.name));
				appendPQExpBuffer(query, ".%s;\n",
						fmtId(owning_tab->attnames[tbinfo->owning_col - 1]));

				ArchiveEntry(fout, nilCatalogId, createDumpId(),
							 tbinfo->dobj.name,
							 tbinfo->dobj.namespace->dobj.name,
							 NULL,
							 tbinfo->rolname,
						   false, "SEQUENCE OWNED BY", query->data, "", NULL,
							 &(tbinfo->dobj.dumpId), 1,
							 NULL, NULL);
			}
		}

		/* Dump Sequence Comments */
		resetPQExpBuffer(query);
		appendPQExpBuffer(query, "SEQUENCE %s", fmtId(tbinfo->dobj.name));
		dumpComment(fout, query->data,
					tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
					tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);
	}

	if (!schemaOnly)
	{
		resetPQExpBuffer(query);
		appendPQExpBuffer(query, "SELECT pg_catalog.setval(");
		appendStringLiteralAH(query, fmtId(tbinfo->dobj.name), fout);
		appendPQExpBuffer(query, ", %s, %s);\n",
						  last, (called ? "true" : "false"));

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tbinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname,
					 false, "SEQUENCE SET", query->data, "", NULL,
					 &(tbinfo->dobj.dumpId), 1,
					 NULL, NULL);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
}

static void
dumpTrigger(Archive *fout, TriggerInfo *tginfo)
{
	TableInfo  *tbinfo = tginfo->tgtable;
	PQExpBuffer query;
	PQExpBuffer delqry;
	const char *p;
	int			findx;

	if (dataOnly)
		return;

	query = createPQExpBuffer();
	delqry = createPQExpBuffer();

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delqry, "DROP TRIGGER IF EXISTS %s ",
					  fmtId(tginfo->dobj.name));
	appendPQExpBuffer(delqry, "ON %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delqry, "%s;\n",
					  fmtId(tbinfo->dobj.name));

	if (tginfo->tgisconstraint)
	{
		appendPQExpBuffer(query, "CREATE CONSTRAINT TRIGGER ");
		appendPQExpBufferStr(query, fmtId(tginfo->tgconstrname));
	}
	else
	{
		appendPQExpBuffer(query, "CREATE TRIGGER ");
		appendPQExpBufferStr(query, fmtId(tginfo->dobj.name));
	}
	appendPQExpBuffer(query, "\n    ");

	/* Trigger type */
	findx = 0;
	if (TRIGGER_FOR_BEFORE(tginfo->tgtype))
		appendPQExpBuffer(query, "BEFORE");
	else
		appendPQExpBuffer(query, "AFTER");
	if (TRIGGER_FOR_INSERT(tginfo->tgtype))
	{
		appendPQExpBuffer(query, " INSERT");
		findx++;
	}
	if (TRIGGER_FOR_DELETE(tginfo->tgtype))
	{
		if (findx > 0)
			appendPQExpBuffer(query, " OR DELETE");
		else
			appendPQExpBuffer(query, " DELETE");
		findx++;
	}
	if (TRIGGER_FOR_UPDATE(tginfo->tgtype))
	{
		if (findx > 0)
			appendPQExpBuffer(query, " OR UPDATE");
		else
			appendPQExpBuffer(query, " UPDATE");
	}
	appendPQExpBuffer(query, " ON %s\n",
					  fmtId(tbinfo->dobj.name));

	if (tginfo->tgisconstraint)
	{
		if (OidIsValid(tginfo->tgconstrrelid))
		{
			/* If we are using regclass, name is already quoted */
			appendPQExpBuffer(query, "    FROM %s\n    ",
							  tginfo->tgconstrrelname);
		}
		if (!tginfo->tgdeferrable)
			appendPQExpBuffer(query, "NOT ");
		appendPQExpBuffer(query, "DEFERRABLE INITIALLY ");
		if (tginfo->tginitdeferred)
			appendPQExpBuffer(query, "DEFERRED\n");
		else
			appendPQExpBuffer(query, "IMMEDIATE\n");
	}

	if (TRIGGER_FOR_ROW(tginfo->tgtype))
		appendPQExpBuffer(query, "    FOR EACH ROW\n    ");
	else
		appendPQExpBuffer(query, "    FOR EACH STATEMENT\n    ");

	appendPQExpBuffer(query, "EXECUTE PROCEDURE %s(",
					  tginfo->tgfname);

	p = tginfo->tgargs;
	for (findx = 0; findx < tginfo->tgnargs; findx++)
	{
		const char *s = p;

		/* Set 'p' to end of arg string. marked by '\000' */
		for (;;)
		{
			p = strchr(p, '\\');
			if (p == NULL)
			{
				mpp_err_msg(logError, progname, "invalid argument string (%s) for trigger \"%s\" on table \"%s\"\n",
							tginfo->tgargs,
							tginfo->dobj.name,
							tbinfo->dobj.name);
				exit_nicely();
			}
			p++;
			if (*p == '\\')		/* is it '\\'? */
			{
				p++;
				continue;
			}
			if (p[0] == '0' && p[1] == '0' && p[2] == '0')		/* is it '\000'? */
				break;
		}
		p--;

		appendPQExpBufferChar(query, '\'');
		while (s < p)
		{
			if (*s == '\'')
				appendPQExpBufferChar(query, '\'');

			/*
			 * bytea unconditionally doubles backslashes, so we suppress the
			 * doubling for standard_conforming_strings.
			 */
			if (fout->std_strings && *s == '\\' && s[1] == '\\')
				s++;
			appendPQExpBufferChar(query, *s++);
		}
		appendPQExpBufferChar(query, '\'');
		appendPQExpBuffer(query,
						  (findx < tginfo->tgnargs - 1) ? ", " : "");
		p = p + 4;
	}
	appendPQExpBuffer(query, ");\n");

	if (!tginfo->tgenabled)
	{
		appendPQExpBuffer(query, "\nALTER TABLE %s ",
						  fmtId(tbinfo->dobj.name));
		appendPQExpBuffer(query, "DISABLE TRIGGER %s;\n",
						  fmtId(tginfo->dobj.name));
	}

	ArchiveEntry(fout, tginfo->dobj.catId, tginfo->dobj.dumpId,
				 tginfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "TRIGGER", query->data, delqry->data, NULL,
				 tginfo->dobj.dependencies, tginfo->dobj.nDeps,
				 NULL, NULL);

	resetPQExpBuffer(query);
	appendPQExpBuffer(query, "TRIGGER %s ",
					  fmtId(tginfo->dobj.name));
	appendPQExpBuffer(query, "ON %s",
					  fmtId(tbinfo->dobj.name));

	dumpComment(fout, query->data,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tginfo->dobj.catId, 0, tginfo->dobj.dumpId);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
}

/*
 * dumpRule
 *		Dump a rule
 */
static void
dumpRule(Archive *fout, RuleInfo *rinfo)
{
	TableInfo  *tbinfo = rinfo->ruletable;
	PQExpBuffer query;
	PQExpBuffer cmd;
	PQExpBuffer delcmd;
	PGresult   *res;

	/* Skip if not to be dumped */
	if (!rinfo->dobj.dump || dataOnly)
		return;

	/*
	 * If it is an ON SELECT rule that is created implicitly by CREATE VIEW,
	 * we do not want to dump it as a separate object.
	 */
	if (!rinfo->separate)
		return;

	/*
	 * Make sure we are in proper schema.
	 */
	selectSourceSchema(tbinfo->dobj.namespace->dobj.name);

	query = createPQExpBuffer();
	cmd = createPQExpBuffer();
	delcmd = createPQExpBuffer();

	appendPQExpBuffer(query,
	  "SELECT pg_catalog.pg_get_ruledef('%u'::pg_catalog.oid) AS definition",
					  rinfo->dobj.catId.oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		mpp_err_msg(logError, progname, "query to get rule \"%s\" for table \"%s\" failed: wrong number of rows returned",
					rinfo->dobj.name, tbinfo->dobj.name);
		exit_nicely();
	}

	printfPQExpBuffer(cmd, "%s\n", PQgetvalue(res, 0, 0));

	/*
	 * DROP must be fully qualified in case same name appears in pg_catalog
	 */
	appendPQExpBuffer(delcmd, "DROP RULE IF EXISTS %s ",
					  fmtId(rinfo->dobj.name));
	appendPQExpBuffer(delcmd, "ON %s.",
					  fmtId(tbinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(delcmd, "%s;\n",
					  fmtId(tbinfo->dobj.name));

	ArchiveEntry(fout, rinfo->dobj.catId, rinfo->dobj.dumpId,
				 rinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "RULE", cmd->data, delcmd->data, NULL,
				 rinfo->dobj.dependencies, rinfo->dobj.nDeps,
				 NULL, NULL);

	/* Dump rule comments */
	resetPQExpBuffer(query);
	appendPQExpBuffer(query, "RULE %s",
					  fmtId(rinfo->dobj.name));
	appendPQExpBuffer(query, " ON %s",
					  fmtId(tbinfo->dobj.name));
	dumpComment(fout, query->data,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				rinfo->dobj.catId, 0, rinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(cmd);
	destroyPQExpBuffer(delcmd);
}

/*
 * setExtPartDependency -
 */
static void
setExtPartDependency(TableInfo *tblinfo, int numTables)
{
	int	i;
	int	j;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &(tblinfo[i]);
		Oid parrelid = tbinfo->parrelid;

		if (parrelid == 0)
			continue;

		for (j = 0; j < numTables; j++)
		{
			TableInfo  *ti = &(tblinfo[j]);
			if (ti->dobj.catId.oid != parrelid)
				continue;
			addObjectDependency(&ti->dobj, tbinfo->dobj.dumpId);
			removeObjectDependency(&tbinfo->dobj, ti->dobj.dumpId);
		}
	}
}

/*
 * getDependencies --- obtain available dependency data
 */
static void
getDependencies(void)
{
	PQExpBuffer query;
	PGresult   *res;
	int			ntups,
				i;
	int			i_classid,
				i_objid,
				i_refclassid,
				i_refobjid,
				i_deptype;
	DumpableObject *dobj,
			   *refdobj;

	if (g_verbose)
		mpp_err_msg(logInfo, progname, "reading dependency data\n");

	/* Make sure we are in proper schema */
	selectSourceSchema("pg_catalog");

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT "
					  "classid, objid, refclassid, refobjid, deptype "
					  "FROM pg_depend "
					  "WHERE deptype != 'p' AND deptype != 'e' "
					  "ORDER BY 1,2");

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_classid = PQfnumber(res, "classid");
	i_objid = PQfnumber(res, "objid");
	i_refclassid = PQfnumber(res, "refclassid");
	i_refobjid = PQfnumber(res, "refobjid");
	i_deptype = PQfnumber(res, "deptype");

	/*
	 * Since we ordered the SELECT by referencing ID, we can expect that
	 * multiple entries for the same object will appear together; this saves
	 * on searches.
	 */
	dobj = NULL;

	for (i = 0; i < ntups; i++)
	{
		CatalogId	objId;
		CatalogId	refobjId;
		char		deptype;

		objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
		objId.oid = atooid(PQgetvalue(res, i, i_objid));
		refobjId.tableoid = atooid(PQgetvalue(res, i, i_refclassid));
		refobjId.oid = atooid(PQgetvalue(res, i, i_refobjid));
		deptype = *(PQgetvalue(res, i, i_deptype));

		if (dobj == NULL ||
			dobj->catId.tableoid != objId.tableoid ||
			dobj->catId.oid != objId.oid)
			dobj = findObjectByCatalogId(objId);

		/*
		 * Failure to find objects mentioned in pg_depend is not unexpected,
		 * since for example we don't collect info about TOAST tables.
		 */
		if (dobj == NULL)
		{
#ifdef NOT_USED
			fprintf(stderr, "no referencing object %u %u\n",
					objId.tableoid, objId.oid);
#endif
			continue;
		}

		refdobj = findObjectByCatalogId(refobjId);

		if (refdobj == NULL)
		{
#ifdef NOT_USED
			fprintf(stderr, "no referenced object %u %u\n",
					refobjId.tableoid, refobjId.oid);
#endif
			continue;
		}

		/*
		 * Ordinarily, table rowtypes have implicit dependencies on their
		 * tables.	However, for a composite type the implicit dependency goes
		 * the other way in pg_depend; which is the right thing for DROP but
		 * it doesn't produce the dependency ordering we need. So in that one
		 * case, we reverse the direction of the dependency.
		 */
		if (deptype == 'i' &&
			dobj->objType == DO_TABLE &&
			refdobj->objType == DO_TYPE)
			addObjectDependency(refdobj, dobj->dumpId);
		else
			/* normal case */
			addObjectDependency(dobj, refdobj->dumpId);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}


/*
 * getFormattedTypeName - retrieve a nicely-formatted type name for the
 * given type name.
 *
 * NB: in 7.3 and up the result may depend on the currently-selected
 * schema; this is why we don't try to cache the names.
 */
static char *
getFormattedTypeName(Oid oid, OidOptions opts)
{
	char	   *result;
	PQExpBuffer query;
	PGresult   *res;
	int			ntups;

	if (oid == 0)
	{
		if ((opts & zeroAsOpaque) != 0)
			return pg_strdup(g_opaque_type);
		else if ((opts & zeroAsAny) != 0)
			return pg_strdup("'any'");
		else if ((opts & zeroAsStar) != 0)
			return pg_strdup("*");
		else if ((opts & zeroAsNone) != 0)
			return pg_strdup("NONE");
	}

	query = createPQExpBuffer();
	appendPQExpBuffer(query, "SELECT pg_catalog.format_type('%u'::pg_catalog.oid, NULL)",
					  oid);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "query yielded %d rows instead of one: %s\n",
					ntups, query->data);
		exit_nicely();
	}

	/* already quoted */
	result = pg_strdup(PQgetvalue(res, 0, 0));

	PQclear(res);
	destroyPQExpBuffer(query);

	return result;
}

/*
 * Return a column list clause for the given relation.
 *
 * Special case: if there are no undropped columns in the relation, return
 * "", not an invalid "()" column list.
 */
static const char *
fmtCopyColumnList(const TableInfo *ti)
{
	static PQExpBuffer q = NULL;
	int			numatts = ti->numatts;
	char	  **attnames = ti->attnames;
	bool	   *attisdropped = ti->attisdropped;
	bool		needComma;
	int			i;

	if (q)						/* first time through? */
		resetPQExpBuffer(q);
	else
		q = createPQExpBuffer();

	appendPQExpBuffer(q, "(");
	needComma = false;
	for (i = 0; i < numatts; i++)
	{
		if (attisdropped[i])
			continue;
		if (needComma)
			appendPQExpBuffer(q, ", ");
		appendPQExpBuffer(q, "%s", fmtId(attnames[i]));
		needComma = true;
	}

	if (!needComma)
		return "";				/* no undropped columns */

	appendPQExpBuffer(q, ")");
	return q->data;
}

/*
 * Start MPP Additions
 */

/*
 * monitorThreadProc: This function runs in a thread and owns the other
 * connection to the database g_conn_status. It listens using this connection
 * for notifications.  A notification only arrives if we are to cancel.
 * In that case, we cause the interrupt handler to be called by sending ourselves
 * a SIGINT signal.  The other duty of this thread, since it owns the g_conn_status,
 * is to periodically examine a linked list of gp_backup_status insert command
 * requests.  If any are there, it causes an insert and a notify to be sent.
 */
void *
monitorThreadProc(void *arg __attribute__((unused)))
{
	int			sock;
	bool		bGotFinished;
	bool		bGotCancelRequest;
	int			PID;
	PGnotify   *notify;
	StatusOp   *pOp;
	BackupStateMachine *pState;
	char	   *pszMsg;
	struct pollfd *pollInput;
	int 		pollResult = 0;
	int 		pollTimeout;

	/* Block SIGINT and SIGKILL signals (we want them only ion main thread) */
	sigset_t	newset;

	sigemptyset(&newset);
	sigaddset(&newset, SIGINT);
	sigaddset(&newset, SIGKILL);
	pthread_sigmask(SIG_SETMASK, &newset, NULL);

	mpp_err_msg(logInfo, progname, "Starting monitor thread\n");

	pState = CreateBackupStateMachine(g_CDBDumpKey, g_role, g_dbID);
	if (pState == NULL)
	{
		mpp_err_msg(logError, progname, "Failed to allocate memory, canceling dump agent on segment with dbid %d\n", g_dbID);
		pthread_kill(g_main_tid, SIGINT);
	}

	/* Issue Listen command  */
	DoCancelNotifyListen(g_conn_status, true, g_CDBDumpKey, g_role, g_dbID, -1, SUFFIX_MASTER_PROBE);

	/* Now wait for a cancel notification */
	sock = PQsocket(g_conn_status);
	bGotFinished = false;
	bGotCancelRequest = false;
	PID = PQbackendPID(g_conn_status);

	pollInput = (struct pollfd *)malloc(sizeof(struct pollfd));

	/* Once we've seen the TASK_FINISH insert request, we know to leave */
	while (!bGotFinished)
	{

		/* Replacing select() by poll() here to overcome the limitations of
		select() to handle large socket file descriptor values.
		*/

		pollInput->fd = sock;
		pollInput->events = POLLIN;
		pollInput->revents = 0;
		pollTimeout = 2000;
		pollResult = poll(pollInput, 1, pollTimeout);

		if(pollResult < 0)
		{
			mpp_err_msg(logError, progname, "poll failed for backup key %s, instid %d, segid %d failed\n",
						g_CDBDumpKey, g_role, g_dbID);
			bGotCancelRequest = true;
		}

		if (PQstatus(g_conn_status) == CONNECTION_BAD)
		{
			mpp_err_msg(logError, progname, "Status Connection went down for backup key %s, instid %d, segid %d\n",
						g_CDBDumpKey, g_role, g_dbID);
			bGotCancelRequest = true;
		}

		PQconsumeInput(g_conn_status);

		while (NULL != (notify = PQnotifies(g_conn_status)))
		{
			/*
			 * Need to make sure that the notify is from another process,
			 * since this processes also issues notifies that aren't cancel
			 * requests.
			 */

			if (notify->be_pid != PID)
			{
				if (strncasecmp(pState->pszNotifyRelNameMasterProbe, notify->relname,
							strlen(pState->pszNotifyRelNameMasterProbe)) == 0)
				{
					/* Notify status alive */
					DoCancelNotifyListen(g_conn_status, false, g_CDBDumpKey, g_role, g_dbID, -1, SUFFIX_SEGMENT_PROBE);
				}
				else
				{
					mpp_err_msg(logInfo, progname, "Notification received that we need to cancel for backup key %s\n",
							g_CDBDumpKey /* , g_dbID */ );
					bGotCancelRequest = true;
					break;
				}
			}

			PQfreemem(notify);
		}

		if (bGotCancelRequest)
		{
			mpp_err_msg(logInfo, progname, "Canceling seg with dbid %d\n", /* g_CDBDumpKey, */ g_dbID);
			DestroyBackupStateMachine(pState);
			pthread_kill(g_main_tid, SIGINT);
		}

		/*
		 * Now deal with any insert requests, where we need to notify of
		 * status update
		 */

		Lock(g_pStatusOpList);

		/* get the next status operator that gp_dump_agent put in there */
		while (NULL != (pOp = TakeFromStatusOpList(g_pStatusOpList)))
		{
			/* NOTIFY of status change */
			DoCancelNotifyListen(g_conn_status, false, g_CDBDumpKey, g_role, g_dbID, -1, pOp->pszSuffix);

			/* does this status op indicates we are done? */
			if (pOp->TaskID == TASK_FINISH)
			{
				bGotFinished = true;

				if (pOp->TaskRC == TASK_RC_SUCCESS)
					pszMsg = "Succeeded\n";
				else
					pszMsg = pOp->pszMsg;

				/* write out our success or failure message */
				mpp_err_msg(pOp->TaskRC == TASK_RC_SUCCESS ? logInfo : logError, progname, pszMsg);
			}

			if (pOp->TaskID == TASK_SET_SERIALIZABLE)
				mpp_err_msg(logInfo, progname, "TASK_SET_SERIALIZABLE\n");
			else if (pOp->TaskID == TASK_GOTLOCKS)
				mpp_err_msg(logInfo, progname, "TASK_GOTLOCKS\n");

			FreeStatusOp(pOp);
		}

		Unlock(g_pStatusOpList);
	}							/* while (!bGotFinished)  */

	/* Close the g_conn_status connection. */
	PQfinish(g_conn_status);
	DestroyBackupStateMachine(pState);

	return NULL;
}

/*
 * my_handler: This function is the signal handler for both SIGINT and SIGTERM signals.
 * It checks to see whether there is an active transaction running on the g_conn connection
 * If so, it issues a PQrequestCancel.	This will cause the current statement to fail, which will
 * cause gp_dump_agent to terminate gracefully.
 * If there's no active transaction, it does nothing.
 * This is set up to always run in the context of the main thread.
 */
void
myHandler(int signo)
{
	if (g_conn != NULL && PQtransactionStatus(g_conn) == PQTRANS_ACTIVE)
	{
		mpp_err_msg(logInfo, progname, "in my_handler before PQrequestCancel\n");
		PQrequestCancel(g_conn);
	}
	else
	{
		mpp_err_msg(logInfo, progname, "in my_handler not in active transaction\n");
	}
}

/*
 *	addDistributedBy
 *
 *	find the distribution policy of the passed in relation and append the
 *	DISTRIBUTED BY clause to the passed in dump buffer (q).
 */
static void
addDistributedBy(PQExpBuffer q, TableInfo *tbinfo, int actual_atts)
{
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	char	   *policydef = NULL;
	char	   *policycol = NULL;

	appendPQExpBuffer(query,
				"SELECT attrnums from pg_namespace as n, pg_class as c, gp_distribution_policy as p "
				"WHERE c.relname = ");

	appendStringLiteralConn(query, tbinfo->dobj.name, g_conn);

	appendPQExpBuffer(query,
				"AND c.relnamespace=n.oid "
				"AND c.oid = p.localoid "
				"AND n.nspname= ");

	if (tbinfo->dobj.namespace->dobj.name)
		appendStringLiteralConn(query, tbinfo->dobj.namespace->dobj.name, g_conn);
	else
		appendStringLiteralConn(query, "public", g_conn);

	res = PQexec(g_conn, query->data);
	check_sql_result(res, g_conn, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		/*
		 * There is no entry in the policy table for this table. Report an
		 * error unless this is a zero attribute table (actual_atts == 0).
		 */
		if (PQntuples(res) < 1 && actual_atts > 0)
		{
			/* if this is a catalog table we allow dumping it, skip the error */
			if (strncmp(tbinfo->dobj.namespace->dobj.name, "pg_", 3) != 0)
			{
				mpp_err_msg(logError, progname, "query to obtain distribution policy of table \"%s\" returned no data\n",
							tbinfo->dobj.name);
				exit_nicely();
			}
		}

		/*
		 * There is more than 1 entry in the policy table for this table.
		 * Report an error.
		 */
		if (PQntuples(res) > 1)
		{
			mpp_err_msg(logError, progname, "query to obtain distribution policy of table \"%s\" returned more than one policy\n",
						tbinfo->dobj.name);
			exit_nicely();
		}
	}
	else
	{
		/*
		 * There is exactly 1 policy entry for this table (either a concrete
		 * one or NULL).
		 */
		policydef = PQgetvalue(res, 0, 0);

		if (strlen(policydef) > 0)
		{
			/* policy indicates one or more columns to distribute on */
			policydef[strlen(policydef) - 1] = '\0';
			policydef++;
			policycol = nextToken(&policydef, ",");
			appendPQExpBuffer(q, " DISTRIBUTED BY (%s",
							  fmtId(tbinfo->attnames[atoi(policycol) - 1]));
			for (; (policycol = nextToken(&policydef, ",")) != NULL;)
			{
				appendPQExpBuffer(q, " ,%s",
							   fmtId(tbinfo->attnames[atoi(policycol) - 1]));
			}
			appendPQExpBuffer(q, ")");
		}
		else
		{
			/* policy has an empty policy - distribute randomly */
			appendPQExpBuffer(q, " DISTRIBUTED RANDOMLY");
		}

	}

	PQclear(res);
	destroyPQExpBuffer(query);

}

char *
formCDatabaseFilePathName(char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID)
{
	return formGenericFilePathName("cdatabase", pszBackupDirectory, pszBackupKey, pszInstID, pszSegID, NULL);
}

char *
formPostDumpFilePathName(char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID)
{
	return formGenericFilePathName("dump", pszBackupDirectory, pszBackupKey, pszInstID, pszSegID, "_post_data_temp");
}

/* formBackupFilePathName( char* pszBackupDirectory, char* pszBackupKey ) returns char*
*
* This function takes the directory and timestamp key and creates
* the path and filename of the backup output file
* based on the naming convention for this.
*/
static char *
formGenericFilePathName(char *keyword, char *pszBackupDirectory, char *pszBackupKey, int pszInstID, int pszSegID, const char *dumpSuffix)
{
	/* First form the prefix */
	char		szFileNamePrefix[1 + PATH_MAX];
	int			len;
	char	   *pszBackupFileName;

	if (g_is_old_format)
		pszInstID = (pszInstID == 1) ? 1 : 0;
	snprintf(szFileNamePrefix, 1 + PATH_MAX, "%sgp_%s_%d_%d_", DUMP_PREFIX, keyword, pszInstID, pszSegID);

	/* Now add up the length of the pieces */
	len = strlen(pszBackupDirectory);
	if (pszBackupDirectory[strlen(pszBackupDirectory) - 1] != '/')
		len++;

	len += strlen(szFileNamePrefix);
	len += strlen(pszBackupKey);

	if(dumpSuffix != NULL)
	{
		len += strlen(dumpSuffix);
	}

	if (len > PATH_MAX)
	{
		mpp_err_msg(logWarn, progname, "Backup catalog FileName based on path %s and key %s too long",
					pszBackupDirectory, pszBackupKey);
		exit_nicely();
	}

	pszBackupFileName = (char *) malloc(sizeof(char) * (1 + len));
	if (pszBackupFileName == NULL)
	{
		mpp_err_msg(logError, progname, "out of memory");
		exit_nicely();
	}

	strcpy(pszBackupFileName, pszBackupDirectory);
	if (pszBackupDirectory[strlen(pszBackupDirectory) - 1] != '/')
		strcat(pszBackupFileName, "/");

	strcat(pszBackupFileName, szFileNamePrefix);
	strcat(pszBackupFileName, pszBackupKey);

	if(dumpSuffix != NULL)
	{
		strcat(pszBackupFileName, dumpSuffix);
	}

	return pszBackupFileName;
}

/*
 * Dump the database definition.
 *
 * This is a very similar version to pg_dump's dumpDatabase() but
 * we make it more straight forward here and write it to the file.
 */
static void
dumpDatabaseDefinition()
{
	PGresult   *res;
	FILE	   *fcat;
	char	   *pszBackupFileName;
	PQExpBuffer dbQry = createPQExpBuffer();
	PQExpBuffer createQry = createPQExpBuffer();
	int			ntups;
	int			i_dba,
				i_encoding,
				i_tablespace,
				i_datacl;
	const char *datname,
			   *dba,
			   *encoding,
			   *tablespace,
			   *datacl,
			   *fdbname;

	datname = PQdb(g_conn);		/* the database we are dumping */

	/* Make sure we are in proper schema */
	/* MPP addition start: comment this line out */
	/*//selectSourceSchema("pg_catalog"); */
	/* MPP addition end */

	pszBackupFileName = formCDatabaseFilePathName(g_pszCDBOutputDirectory,
											   g_CDBDumpKey, g_contentID, g_dbID);

	/*
	 * Make sure we can create this file before we spin off sh cause we don't
	 * get a good error message from sh if we can't write to the file
	 */
	fcat = fopen(pszBackupFileName, "w");
	if (fcat == NULL)
	{
		mpp_err_msg(logError, progname, "Error creating file %s in gp_dump_agent",
					pszBackupFileName);
		exit_nicely();
	}
	free(pszBackupFileName);

	fprintf(fcat, "--\n-- Database creation\n--\n\n");

	appendPQExpBuffer(dbQry, "SELECT "
	  "(SELECT rolname FROM pg_catalog.pg_roles WHERE oid = datdba) as dba, "
					  "pg_encoding_to_char(encoding) as encoding, "
					  "datacl, "
					  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) as tablespace "
					  "FROM pg_database "
					  "WHERE datname = ");
	appendStringLiteralConn(dbQry, datname, g_conn);

	res = PQexec(g_conn, dbQry->data);
	check_sql_result(res, g_conn, dbQry->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	if (ntups <= 0)
	{
		mpp_err_msg(logError, progname, "missing pg_database entry for database \"%s\"\n",
					datname);
		exit_nicely();
	}

	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "query returned more than one (%d) pg_database entry for database \"%s\"\n",
					ntups, datname);
		exit_nicely();
	}

	i_dba = PQfnumber(res, "dba");
	i_encoding = PQfnumber(res, "encoding");
	i_tablespace = PQfnumber(res, "tablespace");
	i_datacl = PQfnumber(res, "datacl");

	dba = PQgetvalue(res, 0, i_dba);
	encoding = PQgetvalue(res, 0, i_encoding);
	tablespace = PQgetvalue(res, 0, i_tablespace);
	datacl = PQgetvalue(res, 0, i_datacl);

	appendPQExpBuffer(createQry, "CREATE DATABASE %s WITH TEMPLATE = template0",
					  fmtId(datname));
	if (strlen(encoding) > 0)
	{
		appendPQExpBuffer(createQry, " ENCODING = ");
		appendStringLiteralConn(createQry, encoding, g_conn);
	}
	if (strlen(dba) > 0)
	{
		appendPQExpBuffer(createQry, " OWNER = %s", fmtId(dba));
	}

	if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0)
		appendPQExpBuffer(createQry, " TABLESPACE = %s",
						  fmtId(tablespace));
	appendPQExpBuffer(createQry, ";\n");

	// Don't use fmtId on owner for buildACLCommands, as this is used by the function internally

	fdbname = strdup(fmtId(datname));
	if(!buildACLCommands(fdbname, "DATABASE", datacl, dba,
							  g_fout->remoteVersion, createQry)){
		mpp_err_msg(logError, progname, "could not parse ACL list (%s) for object \"%s\" (%s)\n",
					datacl, fdbname, "DATABASE");
		exit_nicely();
	}

    dumpDatabaseConfig(datname, &createQry);

	/* write the CREATE DATABASE and ALTER DATABASE commands to the file */
	if (fcat != NULL)
	{
		fprintf(fcat, "%s", createQry->data);
		fclose(fcat);
	}

	PQclear(res);
	free((void*) fdbname);
	destroyPQExpBuffer(dbQry);
	destroyPQExpBuffer(createQry);
}

#ifdef USE_DDBOOST
/*
 * Dump the database definition.
 *
 * This is a very similar version to pg_dump's dumpDatabase() but
 * we make it more straight forward here and write it to the file.
 */
static void
dumpDatabaseDefinitionToDDBoost()
{
	PGresult   *res;
	char	   *pszBackupFileName;
	PQExpBuffer dbQry = createPQExpBuffer();
	PQExpBuffer createQry = createPQExpBuffer();
	int			ntups;
	int			i_dba,
				i_encoding,
				i_tablespace,
				i_datacl;
	const char *datname,
			   *dba,
			   *encoding,
			   *tablespace,
			   *datacl,
			   *fdbname;
	int err = 0;
	ddp_uint64_t ret_count, nmemb;
	ddp_uint64_t offset = 0;
	ddp_file_desc_t handle = DDP_INVALID_DESCRIPTOR;
	char *createStmt = "--\n-- Database creation\n--\n\n";


	datname = PQdb(g_conn);		/* the database we are dumping */

	/* Make sure we are in proper schema */
	/* MPP addition start: comment this line out */
	/*//selectSourceSchema("pg_catalog"); */
	/* MPP addition end */

	pszBackupFileName = formCDatabaseFilePathName(g_pszDDBoostDir,
								   g_CDBDumpKey, g_contentID, g_dbID);

	/*
	 * Make sure we can create this file before we spin off sh cause we don't
	 * get a good error message from sh if we can't write to the file
	 */
	path1.su_name = ddboost_storage_unit;
	path1.path_name = g_pszDDBoostDir;

	err = createDDBoostDir(ddp_conn, path1.su_name, path1.path_name);
	if (err)
	{
		mpp_err_msg(logError, progname, "Error creating directory on ddboost\n");
		exit_nicely();
	}

	path1.path_name = pszBackupFileName;
	err = ddp_open_file(ddp_conn, &path1, DDP_O_CREAT | DDP_O_RDWR , 0777, &handle);

	if (err)
	{
		mpp_err_msg(logError, progname, "Error creating file %s on ddboost from gp_dump_agent",
					pszBackupFileName);
		exit_nicely();
	}
	free(pszBackupFileName);

	//fprintf(fcat, "--\n-- Database creation\n--\n\n");
	err = ddp_write(handle, createStmt, strlen(createStmt), 0, &ret_count);
	if(err)
	{
		mpp_err_msg(logError, progname, "Write to cdatabase file failed\n");
		exit_nicely();
	}

	offset += ret_count;

	appendPQExpBuffer(dbQry, "SELECT "
	  "(SELECT rolname FROM pg_catalog.pg_roles WHERE oid = datdba) as dba, "
					  "pg_encoding_to_char(encoding) as encoding, "
					  "datacl, "
					  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) as tablespace "
					  "FROM pg_database "
					  "WHERE datname = ");
	appendStringLiteralConn(dbQry, datname, g_conn);

	res = PQexec(g_conn, dbQry->data);
	check_sql_result(res, g_conn, dbQry->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	if (ntups <= 0)
	{
		mpp_err_msg(logError, progname, "missing pg_database entry for database \"%s\"\n",
					datname);
		exit_nicely();
	}

	if (ntups != 1)
	{
		mpp_err_msg(logError, progname, "query returned more than one (%d) pg_database entry for database \"%s\"\n",
					ntups, datname);
		exit_nicely();
	}

	i_dba = PQfnumber(res, "dba");
	i_encoding = PQfnumber(res, "encoding");
	i_tablespace = PQfnumber(res, "tablespace");
	i_datacl = PQfnumber(res, "datacl");

	dba = PQgetvalue(res, 0, i_dba);
	encoding = PQgetvalue(res, 0, i_encoding);
	tablespace = PQgetvalue(res, 0, i_tablespace);
	datacl = PQgetvalue(res, 0, i_datacl);

	appendPQExpBuffer(createQry, "CREATE DATABASE %s WITH TEMPLATE = template0",
					  fmtId(datname));
	if (strlen(encoding) > 0)
	{
		appendPQExpBuffer(createQry, " ENCODING = ");
		appendStringLiteralConn(createQry, encoding, g_conn);
	}
	if (strlen(dba) > 0)
	{
		appendPQExpBuffer(createQry, " OWNER = %s", dba);
	}

	if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0)
		appendPQExpBuffer(createQry, " TABLESPACE = %s",
						  fmtId(tablespace));
	appendPQExpBuffer(createQry, ";\n");

	// Don't use fmtId on owner for buildACLCommands, as this is used by the function internally

	fdbname = strdup(fmtId(datname));
	if(!buildACLCommands(fdbname, "DATABASE", datacl, dba,
	                     g_fout->remoteVersion, createQry)){
		mpp_err_msg(logError, progname, "could not parse ACL list (%s) for object \"%s\" (%s)\n",
									datacl, fdbname, "DATABASE");
		exit_nicely();
	}

	dumpDatabaseConfig(datname, &createQry);

	/* write the CREATE DATABASE and ALTER DATABASE commands to the file */
	nmemb = strlen(createQry->data);
	err = ddp_write(handle, createQry->data, nmemb, offset, &ret_count);
	if (ret_count != nmemb)
	{
		mpp_err_msg(logError, progname, "write to cdatabase file failed on ddboost\n");
		exit_nicely();
	}

	ddp_close_file(handle);
	PQclear(res);
	free((void*) fdbname);
	destroyPQExpBuffer(dbQry);
	destroyPQExpBuffer(createQry);
}
#endif

/*
 * Dump database-specific configuration such as search_path,
 * optimizer, and gp_default_storage_options
 */
static void
dumpDatabaseConfig(const char *dbname, PQExpBuffer *buf)
{
	int			count = 1;
    PQExpBuffer qryBuf = createPQExpBuffer();

	for (;;)
	{
		PGresult   *res;

		printfPQExpBuffer(qryBuf, "SELECT datconfig[%d] FROM pg_database WHERE datname = ", count);
		appendStringLiteralConn(qryBuf, dbname, g_conn);
		appendPQExpBuffer(qryBuf, ";");

		res = PQexec(g_conn, qryBuf->data);
		if (!PQgetisnull(res, 0, 0))
		{
			makeAlterConfigCommand(PQgetvalue(res, 0, 0),
								   "DATABASE", dbname, buf);
			PQclear(res);
			count++;
		}
		else
		{
			PQclear(res);
			break;
		}
	}

	destroyPQExpBuffer(qryBuf);
}

/*
 * Helper function for dumpDatabaseConfig().
 */
static void
makeAlterConfigCommand(const char *arrayitem,
					   const char *type, const char *name, PQExpBuffer *buf)
{
	char	   *pos;
	char	   *mine;

	mine = strdup(arrayitem);
	pos = strchr(mine, '=');
	if (pos == NULL)
		return;

	*pos = 0;
	appendPQExpBuffer(*buf, "ALTER %s %s ", type, fmtId(name));
	appendPQExpBuffer(*buf, "SET %s TO ", fmtId(mine));

	/*
	 * Some GUC variable names are 'LIST' type and hence must not be quoted.
	 */
	if (pg_strcasecmp(mine, "DateStyle") == 0
		|| pg_strcasecmp(mine, "search_path") == 0)
		appendPQExpBuffer(*buf, "%s", pos + 1);
	else
		appendStringLiteralConn(*buf, pos + 1, g_conn);
	appendPQExpBuffer(*buf, ";\n");

	free(mine);
}

static int
_parse_version(ArchiveHandle *AH, const char *versionString)
{
	int			v;

	v = parse_version(versionString);
	if (v < 0)
		die_horribly(AH, NULL, "could not parse version string \"%s\"\n", versionString);

	return v;
}

/*
* _check_database_version: This was copied here from pg_backup_db.c because it's declared static there.
 * We need to call it directly because in pg_dump.c it's called from ConnectDatabase
 * which we are not using.
 */
static void
_check_database_version(ArchiveHandle *AH)
{
	int			myversion;
	const char *remoteversion_str;
	int			remoteversion;

	myversion = _parse_version(AH, PG_VERSION);
	remoteversion_str = PQparameterStatus(AH->connection, "server_version");
	if (!remoteversion_str)
		die_horribly(AH, NULL, "could not get server_version from libpq\n");

	remoteversion = _parse_version(AH, remoteversion_str);

	AH->public.remoteVersionStr = pg_strdup(remoteversion_str);
	AH->public.remoteVersion = remoteversion;

	if (myversion != remoteversion
		&& (remoteversion < AH->public.minRemoteVersion ||
			remoteversion > AH->public.maxRemoteVersion))
	{
		mpp_err_msg(logInfo, progname, "server version: %s; %s version: %s\n",
					remoteversion_str, progname, PG_VERSION);

		die_horribly(AH, NULL, "aborting because of version mismatch  (Use the -i option to proceed anyway.)\n");
	}
}

/*
 * makeSureMonitorThreadEnds: This function adds a request for a TASK_FINISH
 * status row, and then waits for the g_monitor_tid to end.
 * This will happen as soon as the monitor thread processes this request.
 */
void
makeSureMonitorThreadEnds(int TaskRC, char *pszErrorMsg)
{
	StatusOp   *pOp;
	char	   *pszSuffix;

	if (g_monitor_tid)
	{
		if (TaskRC == TASK_RC_SUCCESS)
			pszSuffix = SUFFIX_SUCCEED;
		else
			pszSuffix = SUFFIX_FAIL;

		pOp = CreateStatusOp(TASK_FINISH, TaskRC, pszSuffix, pszErrorMsg);
		if (pOp == NULL)
		{
			/*
			 * This is really bad, since the only way we cab shut down
			 * gracefully is if we can insert a row into the ststus table.	Oh
			 * well, at least log a message and exit.
			 */
			mpp_err_msg(logError, progname, "*** aborted because of fatal memory allocation error\n");
			exit(1);
		}
		else
			AddToStatusOpList(g_pStatusOpList, pOp);

		pthread_join(g_monitor_tid, NULL);
	}
}


/*
 * End MPP Additions
 */

static void
installSignalHandlers(void)
{
	struct sigaction act,
				oact;

	/* Install Ctrl-C interrupt handler, now that we have a connection */
	act.sa_handler = myHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_flags |= SA_RESTART;
	if (sigaction(SIGINT, &act, &oact) < 0)
		mpp_err_msg(logError, progname, "Error trying to set SIGINT interrupt handler\n");

	act.sa_handler = myHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_flags |= SA_RESTART;
	if (sigaction(SIGTERM, &act, &oact) < 0)
		mpp_err_msg(logError, progname, "Error trying to set SIGTERM interrupt handler\n");
}

#ifdef USE_DDBOOST
int
cleanupDDSystem(void)
{
	int err = DD_ERR_NONE;

	/* Close the connection */
	err = ddp_disconnect(ddp_conn);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost disconnect failed. Err = %d\n", err);
		return err;
	}

	err = ddp_instance_destroy(ddp_inst);
	if (err)
	{
		mpp_err_msg(logError, progname, "ddboost instance destroy failed. Err = %d\n", err);
		return err;
	}

	ddp_shutdown();
	return 0;
}

int
updateArchiveWithDDFile(ArchiveHandle *AH, char *g_pszDDBoostFile, const char *g_pszDDBoostDir)
{
	int err = 0;
	char *dir_name = "db_dumps";

	path1.su_name = ddboost_storage_unit;

	if (g_pszDDBoostDir)
		path1.path_name  = pg_strdup(g_pszDDBoostDir);
	else
		path1.path_name = dir_name;

	err = createDDBoostDir(ddp_conn, path1.su_name, path1.path_name);

	if (err)
		return err;

       	return 0;
}

int
createDDBoostDir(ddp_conn_desc_t ddp_conn, char *ddboost_storage_unit, char *path_name)
{
	char *pch = NULL;
	ddp_path_t path = {0};
	int err = 0;
	char *full_path = NULL;

	full_path = (char*)malloc(MAX_PATH_NAME);
	memset(full_path, 0, MAX_PATH_NAME);

	pch = strtok(path_name, " /");
	while(pch != NULL)
	{
		path.su_name = ddboost_storage_unit;
		strcat(full_path, "/");
		strcat(full_path, pch);
		path.path_name = full_path;

		err = ddp_mkdir(ddp_conn, &path, 0755);
		if ((err != 0) && (err != DD_ERR_EXIST))
		{
			mpp_err_msg(logError, progname, "mkdir failed. err %d\n", err);
			free(full_path);
			return err;
		}

		pch = strtok(NULL, " /");
	}

	free(full_path);
	return 0;
}
#endif

Archive *
makeArchive(char *filename)
{
	/* open the output file */
	switch (fileFormat)
	{
		case 'a':
		case 'A':
			plainText = 1;
			g_fout = CreateArchive(filename, archNull, 0, archModeAppend);
			break;

		case 'c':
		case 'C':
			g_fout = CreateArchive(filename, archCustom, compressLevel, archModeWrite);
			break;

		case 'f':
		case 'F':
			g_fout = CreateArchive(filename, archFiles, compressLevel, archModeWrite);
			break;

		case 'p':
		case 'P':
			plainText = 1;
			g_fout = CreateArchive(filename, archNull, 0, archModeWrite);
			break;

		case 't':
		case 'T':
			g_fout = CreateArchive(filename, archTar, compressLevel, archModeWrite);
			break;

		default:
			mpp_err_msg(logError, progname, "invalid output format \"%s\" specified\n", fileFormat);
			exit(1);
	}
	return g_fout;
}

#ifdef USE_DDBOOST
void
updateDDBoostArchive(ArchiveHandle *AH)
{
	if(postDataSchemaOnly)
	{
		temp_dd_file = (char*)malloc(MAX_PATH_NAME);
		if(!temp_dd_file)
		{
			mpp_err_msg(logError, progname, "Could not allocate memory\n");
			exit(1);
		}
		sprintf(temp_dd_file, "%s_post_data", g_pszDDBoostFile);
		ret = updateArchiveWithDDFile(AH, temp_dd_file, g_pszDDBoostDir);
		free(temp_dd_file);
	}
	else
		ret = updateArchiveWithDDFile(AH, g_pszDDBoostFile, g_pszDDBoostDir);
	if (ret)
	{
		mpp_err_msg(logError, progname, "Could not open file on ddboost. Err %d\n", ret);
		exit(1);
	}
}
#endif
