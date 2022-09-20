/*-------------------------------------------------------------------------
 *
 * pg_dump.c
 *	  pg_dump is a utility for dumping out a postgres database
 *	  into a script file.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	pg_dump will read the system catalogs in a database and dump out a
 *	script that reproduces the schema in terms of SQL that is understood
 *	by PostgreSQL
 *
 *	Note that pg_dump runs in a transaction-snapshot mode transaction,
 *	so it sees a consistent snapshot of the database including system
 *	catalogs. However, it relies in part on various specialized backend
 *	functions like pg_get_indexdef(), and those things tend to look at
 *	the currently committed state.  So it is possible to get 'cache
 *	lookup failed' error if someone performs DDL changes while a dump is
 *	happening. The window for this sort of thing is from the acquisition
 *	of the transaction snapshot to getSchemaData() (when pg_dump acquires
 *	AccessShareLock on every table it intends to dump). It isn't very large,
 *	but it can happen.
 *
 *	http://archives.postgresql.org/pgsql-bugs/2010-02/msg00187.php
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/pg_dump.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <unistd.h>
#include <ctype.h>
#ifdef ENABLE_NLS
#include <locale.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "getopt_long.h"

#include "access/attnum.h"
#include "access/sysattr.h"
#include "access/transam.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_class.h"
#include "catalog/pg_magic_oid.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "catalog/gp_policy.h"
#include "libpq/libpq-fs.h"

#include "pg_backup_archiver.h"
#include "pg_backup_db.h"
#include "pg_backup_utils.h"
#include "dumputils.h"
#include "fe_utils/connect.h"
#include "parallel.h"

typedef struct
{
	Oid			roleoid;		/* role's OID */
	const char *rolename;		/* role's name */
} RoleNameItem;

typedef struct
{
	const char *descr;			/* comment for an object */
	Oid			classoid;		/* object class (catalog OID) */
	Oid			objoid;			/* object OID */
	int			objsubid;		/* subobject (table column #) */
} CommentItem;

typedef struct
{
	const char *provider;		/* label provider of this security label */
	const char *label;			/* security label for an object */
	Oid			classoid;		/* object class (catalog OID) */
	Oid			objoid;			/* object OID */
	int			objsubid;		/* subobject (table column #) */
} SecLabelItem;

/* global decls */
bool		g_verbose;			/* User wants verbose narration of our
								 * activities. */

/* various user-settable parameters */
static bool schemaOnly;
static bool dataOnly;
static int	dumpSections;		/* bitmask of chosen sections */
static bool aclsSkip;
static const char *lockWaitTimeout;

/* START MPP ADDITION */
bool		dumpPolicy;
bool		isGPbackend;

/* END MPP ADDITION */

/*
 * Object inclusion/exclusion lists
 *
 * The string lists record the patterns given by command-line switches,
 * which we then convert to lists of OIDs of matching objects.
 */
static SimpleStringList schema_include_patterns = {NULL, NULL};
static SimpleOidList schema_include_oids = {NULL, NULL};
static SimpleStringList schema_exclude_patterns = {NULL, NULL};
static SimpleOidList schema_exclude_oids = {NULL, NULL};

static SimpleStringList table_include_patterns = {NULL, NULL};
static SimpleOidList table_include_oids = {NULL, NULL};
static SimpleStringList table_exclude_patterns = {NULL, NULL};
static SimpleOidList table_exclude_oids = {NULL, NULL};
static SimpleStringList tabledata_exclude_patterns = {NULL, NULL};
static SimpleOidList tabledata_exclude_oids = {NULL, NULL};

static SimpleStringList relid_string_list = {NULL, NULL};
static SimpleStringList funcid_string_list = {NULL, NULL};
static SimpleOidList function_include_oids = {NULL, NULL};

static SimpleOidList preassigned_oids = {NULL, NULL};

/* default, if no "inclusion" switches appear, is to dump everything */
static bool include_everything = true;

char		g_opaque_type[10];	/* name for the opaque type */

/* placeholders for the delimiters for comments */
char		g_comment_start[10];
char		g_comment_end[10];

static const CatalogId nilCatalogId = {0, 0};

const char *EXT_PARTITION_NAME_POSTFIX = "_external_partition__";

/* sorted table of role names */
static RoleNameItem *rolenames = NULL;
static int	nrolenames = 0;

/* flags for various command-line long options */
static int	binary_upgrade = 0;
static int	disable_dollar_quoting = 0;
static int	dump_inserts = 0;
static int	column_inserts = 0;
static int	if_exists = 0;
static int	no_security_labels = 0;
static int	no_synchronized_snapshots = 0;
static int	no_unlogged_table_data = 0;
static int	serializable_deferrable = 0;

/*
 * Macro for producing quoted, schema-qualified name of a dumpable object.
 * Note implicit dependence on "fout"; we should get rid of that argument.
 */
#define fmtQualifiedDumpable(obj) \
	fmtQualifiedId(fout->remoteVersion, \
				   (obj)->dobj.namespace->dobj.name, \
				   (obj)->dobj.name)
static DumpId binary_upgrade_dumpid;

static void help(const char *progname);
static void setup_connection(Archive *AH, const char *dumpencoding,
				 char *use_role);
static ArchiveFormat parseArchiveFormat(const char *format, ArchiveMode *mode);
static void expand_schema_name_patterns(Archive *fout,
							SimpleStringList *patterns,
							SimpleOidList *oids);
static void expand_table_name_patterns(Archive *fout,
						   SimpleStringList *patterns,
						   SimpleOidList *oids);
static NamespaceInfo *findNamespace(Archive *fout, Oid nsoid, Oid objoid);
static void dumpTableData(Archive *fout, TableDataInfo *tdinfo);
static void refreshMatViewData(Archive *fout, TableDataInfo *tdinfo);
static void guessConstraintInheritance(TableInfo *tblinfo, int numTables);
static void dumpComment(Archive *fout, const char *type, const char *name,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId);
static const char *getRoleName(const char *roleoid_str);
static void collectRoleNames(Archive *fout);
static int findComments(Archive *fout, Oid classoid, Oid objoid,
			 CommentItem **items);
static int	collectComments(Archive *fout, CommentItem **items);
static void dumpSecLabel(Archive *fout, const char *type, const char *name,
			 const char *namespace, const char *owner,
			 CatalogId catalogId, int subid, DumpId dumpId);
static int findSecLabels(Archive *fout, Oid classoid, Oid objoid,
			  SecLabelItem **items);
static int	collectSecLabels(Archive *fout, SecLabelItem **items);
static void dumpDumpableObject(Archive *fout, DumpableObject *dobj);
static void dumpNamespace(Archive *fout, NamespaceInfo *nspinfo);
static void dumpExtension(Archive *fout, ExtensionInfo *extinfo);
static void dumpType(Archive *fout, TypeInfo *tyinfo);
static void dumpBaseType(Archive *fout, TypeInfo *tyinfo);
static void dumpTypeStorageOptions(Archive *fout, TypeInfo *tyinfo);
static void dumpEnumType(Archive *fout, TypeInfo *tyinfo);
static void dumpRangeType(Archive *fout, TypeInfo *tyinfo);
static void dumpUndefinedType(Archive *fout, TypeInfo *tyinfo);
static void dumpDomain(Archive *fout, TypeInfo *tyinfo);
static void dumpCompositeType(Archive *fout, TypeInfo *tyinfo);
static void dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo);
static void dumpShellType(Archive *fout, ShellTypeInfo *stinfo);
static void dumpProcLang(Archive *fout, ProcLangInfo *plang);
static void dumpFunc(Archive *fout, FuncInfo *finfo);
static void dumpCast(Archive *fout, CastInfo *cast);
static void dumpOpr(Archive *fout, OprInfo *oprinfo);
static void dumpOpclass(Archive *fout, OpclassInfo *opcinfo);
static void dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo);
static void dumpCollation(Archive *fout, CollInfo *convinfo);
static void dumpConversion(Archive *fout, ConvInfo *convinfo);
static void dumpRule(Archive *fout, RuleInfo *rinfo);
static void dumpAgg(Archive *fout, AggInfo *agginfo);
static void dumpExtProtocol(Archive *fout, ExtProtInfo *ptcinfo);
static void dumpTrigger(Archive *fout, TriggerInfo *tginfo);
static void dumpEventTrigger(Archive *fout, EventTriggerInfo *evtinfo);
static void dumpTable(Archive *fout, TableInfo *tbinfo);
static void dumpTableSchema(Archive *fout, TableInfo *tbinfo);
static void dumpAttrDef(Archive *fout, AttrDefInfo *adinfo);
static void dumpSequence(Archive *fout, TableInfo *tbinfo);
static void dumpSequenceData(Archive *fout, TableDataInfo *tdinfo);
static void dumpIndex(Archive *fout, IndxInfo *indxinfo);
static void dumpConstraint(Archive *fout, ConstraintInfo *coninfo);
static void dumpTableConstraintComment(Archive *fout, ConstraintInfo *coninfo);
static void dumpTSParser(Archive *fout, TSParserInfo *prsinfo);
static void dumpTSDictionary(Archive *fout, TSDictInfo *dictinfo);
static void dumpTSTemplate(Archive *fout, TSTemplateInfo *tmplinfo);
static void dumpTSConfig(Archive *fout, TSConfigInfo *cfginfo);
static void dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo);
static void dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo);
static void dumpUserMappings(Archive *fout,
				 const char *servername, const char *namespace,
				 const char *owner, CatalogId catalogId, DumpId dumpId);
static void dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo);

static void dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name, const char *subname,
		const char *nspname, const char *owner,
		const char *acls);

static void getDependencies(Archive *fout);
static void BuildArchiveDependencies(Archive *fout);
static void findDumpableDependencies(ArchiveHandle *AH, DumpableObject *dobj,
						 DumpId **dependencies, int *nDeps, int *allocDeps);

static DumpableObject *createBoundaryObjects(void);
static void addBoundaryDependencies(DumpableObject **dobjs, int numObjs,
						DumpableObject *boundaryObjs);

static void getDomainConstraints(Archive *fout, TypeInfo *tyinfo);
static void getTableData(TableInfo *tblinfo, int numTables, bool oids);
static void makeTableDataInfo(TableInfo *tbinfo, bool oids);
static void buildMatViewRefreshDependencies(Archive *fout);
static void getTableDataFKConstraints(void);
static char *format_function_arguments(FuncInfo *finfo, char *funcargs,
									   bool is_agg);
static char *format_function_signature(Archive *fout,
						  FuncInfo *finfo, bool honor_quotes);
static char *convertRegProcReference(Archive *fout,
						const char *proc);
static char *getFormattedOperatorName(Archive *fout, const char *oproid);
static const char *convertTSFunction(Archive *fout, Oid funcOid);
static char *getFormattedTypeName(Archive *fout, Oid oid, OidOptions opts);
static void getBlobs(Archive *fout);
static void dumpBlob(Archive *fout, BlobInfo *binfo);
static int	dumpBlobs(Archive *fout, void *arg);
static void dumpPreassignedOidArchiveEntry(Archive *fout, BinaryUpgradeInfo *binfo);
static void dumpPreassignedOidDefinition(Archive *fout, BinaryUpgradeInfo *binfo);
static void dumpDatabase(Archive *AH);
static void dumpEncoding(Archive *AH);
static void dumpStdStrings(Archive *AH);
static void binary_upgrade_set_namespace_oid(Archive *fout,
								PQExpBuffer upgrade_buffer,
								Oid pg_namespace_oid);
static void dumpSearchPath(Archive *AH);
static void binary_upgrade_set_type_oids_by_type_oid(Archive *fout,
													 PQExpBuffer upgrade_buffer,
													 const TypeInfo *tyinfo);
static void binary_upgrade_set_type_oids_by_rel(Archive *fout,
													PQExpBuffer upgrade_buffer,
													const TableInfo *tblinfo);
static void binary_upgrade_set_pg_class_oids(Archive *fout,
											 PQExpBuffer upgrade_buffer,
											 Oid pg_class_oid, bool is_index);
static void binary_upgrade_set_rel_ao_oids(Archive *fout,
											PQExpBuffer upgrade_buffer,
											const TableInfo *tblinfo);
static void binary_upgrade_set_bitmap_index_oids(Archive *fout,
											PQExpBuffer upgrade_buffer,
											const IndxInfo *idxinfo);
static void binary_upgrade_set_toast_oids_by_rel(Archive *fout,
											PQExpBuffer upgrade_buffer,
											const TableInfo *tblinfo);
static void binary_upgrade_extension_member(PQExpBuffer upgrade_buffer,
											DumpableObject *dobj,
											const char *objtype,
											const char *objname,
											const char *objnamespace);
static void binary_upgrade_extension_member(PQExpBuffer upgrade_buffer,
								DumpableObject *dobj,
								const char *objtype,
								const char *objname,
								const char *objnamespace);
static const char *getAttrName(int attrnum, TableInfo *tblInfo);
static const char *fmtCopyColumnList(const TableInfo *ti, PQExpBuffer buffer);
static bool nonemptyReloptions(const char *reloptions);
static void fmtReloptionsArray(Archive *fout, PQExpBuffer buffer,
				   const char *reloptions, const char *prefix);
static char *get_synchronized_snapshot(Archive *fout);
static void setupDumpWorker(Archive *AHX, RestoreOptions *ropt);


/* START MPP ADDITION */
static void setExtPartDependency(TableInfo *tblinfo, int numTables);
static char *format_table_function_columns(Archive *fout, FuncInfo *finfo, int nallargs,
							  char **allargtypes,
							  char **argmodes,
							  char **argnames);
static void expand_oid_patterns(SimpleStringList *patterns,
						   SimpleOidList *oids);

static bool is_returns_table_function(int nallargs, char **argmodes);
static bool testGPbackend(Archive *fout);
static char *nextToken(register char **stringp, register const char *delim);
static void addDistributedBy(Archive *fout, PQExpBuffer q, TableInfo *tbinfo, int actual_atts);
static void addDistributedByOld(Archive *fout, PQExpBuffer q, TableInfo *tbinfo, int actual_atts);
static bool isGPDB(Archive *fout);
static bool isGPDB6000OrLater(Archive *fout);

/* END MPP ADDITION */

/*
 * Check if we are talking to GPDB
 */
static bool
isGPDB(Archive *fout)
{
	static int	value = -1;		/* -1 = not known yet, 0 = no, 1 = yes */

	/* Query the server on first call, and cache the result */
	if (value == -1)
	{
		const char *query = "select pg_catalog.version()";
		PGresult   *res;
		char	   *ver;

		res = ExecuteSqlQuery(fout, query, PGRES_TUPLES_OK);

		ver = (PQgetvalue(res, 0, 0));
		if (strstr(ver, "Greenplum") != NULL)
			value = 1;
		else
			value = 0;

		PQclear(res);
	}
	return (value == 1) ? true : false;
}

static bool
isGPDB6000OrLater(Archive *fout)
{
	if (!isGPDB(fout))
		return false;		/* Not Greenplum at all. */

	/* GPDB 6 is based on PostgreSQL 8.4 */
	return fout->remoteVersion >= 80400;
}

int
main(int argc, char **argv)
{
	int			c;
	const char *filename = NULL;
	const char *format = "p";
	const char *dbname = NULL;
	const char *pghost = NULL;
	const char *pgport = NULL;
	const char *username = NULL;
	const char *dumpencoding = NULL;
	bool		oids = false;
	TableInfo  *tblinfo;
	int			numTables;
	DumpableObject **dobjs;
	int			numObjs;
	DumpableObject *boundaryObjs;
	int			i;
	int			numWorkers = 1;
	enum trivalue prompt_password = TRI_DEFAULT;
	int			compressLevel = -1;
	int			plainText = 0;
	int			outputClean = 0;
	int			outputCreateDB = 0;
	bool		outputBlobs = false;
	int			outputNoOwner = 0;
	char	   *outputSuperuser = NULL;
	char	   *use_role = NULL;
	int			optindex;
	RestoreOptions *ropt;
	ArchiveFormat archiveFormat = archUnknown;
	ArchiveMode archiveMode;
	Archive    *fout;			/* the script file */

	static int	disable_triggers = 0;
	static int	outputNoTablespaces = 0;
	static int	use_setsessauth = 0;

	/*
	 * The default value for gp_syntax_option depends upon whether or not the
	 * backend is a GP or non-GP backend -- a GP backend defaults to ENABLED.
	 */
	static enum
	{
		GPS_NOT_SPECIFIED, GPS_DISABLED, GPS_ENABLED
	}			gp_syntax_option = GPS_NOT_SPECIFIED;

	static struct option long_options[] = {
		{"binary-upgrade", no_argument, &binary_upgrade, 1},	/* not documented */
		{"data-only", no_argument, NULL, 'a'},
		{"blobs", no_argument, NULL, 'b'},
		{"clean", no_argument, NULL, 'c'},
		{"create", no_argument, NULL, 'C'},
		{"dbname", required_argument, NULL, 'd'},
		{"file", required_argument, NULL, 'f'},
		{"format", required_argument, NULL, 'F'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"jobs", 1, NULL, 'j'},
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

		/*
		 * the following options don't have an equivalent short option letter
		 */
		{"attribute-inserts", no_argument, &column_inserts, 1},
		{"column-inserts", no_argument, &column_inserts, 1},
		{"disable-dollar-quoting", no_argument, &disable_dollar_quoting, 1},
		{"disable-triggers", no_argument, &disable_triggers, 1},
		{"exclude-table-data", required_argument, NULL, 4},
		{"if-exists", no_argument, &if_exists, 1},
		{"inserts", no_argument, &dump_inserts, 1},
		{"lock-wait-timeout", required_argument, NULL, 2},
		{"no-tablespaces", no_argument, &outputNoTablespaces, 1},
		{"quote-all-identifiers", no_argument, &quote_all_identifiers, 1},
		{"role", required_argument, NULL, 3},
		{"section", required_argument, NULL, 5},
		{"serializable-deferrable", no_argument, &serializable_deferrable, 1},
		{"use-set-session-authorization", no_argument, &use_setsessauth, 1},
		{"no-security-labels", no_argument, &no_security_labels, 1},
		{"no-synchronized-snapshots", no_argument, &no_synchronized_snapshots, 1},
		{"no-unlogged-table-data", no_argument, &no_unlogged_table_data, 1},

		/* START MPP ADDITION */

		/*
		 * the following are mpp specific, and don't have an equivalent short
		 * option
		 */
		{"gp-syntax", no_argument, NULL, 1000},
		{"no-gp-syntax", no_argument, NULL, 1001},
		{"function-oids", required_argument, NULL, 1002},
		{"relation-oids", required_argument, NULL, 1003},
		/* END MPP ADDITION */
		{NULL, 0, NULL, 0}
	};

	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));

	/*
	 * Initialize what we need for parallel execution, especially for thread
	 * support on Windows.
	 */
	init_parallel_dump_utils();

	g_verbose = false;

	strcpy(g_comment_start, "-- ");
	g_comment_end[0] = '\0';
	strcpy(g_opaque_type, "opaque");

	dataOnly = schemaOnly = false;
	dumpSections = DUMP_UNSECTIONED;
	lockWaitTimeout = NULL;

	progname = get_progname(argv[0]);

	/* Set default options based on progname */
	if (strcmp(progname, "pg_backup") == 0)
		format = "c";

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help(progname);
			exit_nicely(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_dump (PostgreSQL) " PG_VERSION);
			exit_nicely(0);
		}
	}

	while ((c = getopt_long(argc, argv, "abcCd:E:f:F:h:ij:n:N:oOp:RsS:t:T:uU:vwWxZ:",
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

			case 'd':			/* database name */
				dbname = pg_strdup(optarg);
				break;

			case 'E':			/* Dump encoding */
				dumpencoding = pg_strdup(optarg);
				break;

			case 'f':
				filename = pg_strdup(optarg);
				break;

			case 'F':
				format = pg_strdup(optarg);
				break;

			case 'h':			/* server host */
				pghost = pg_strdup(optarg);
				break;

			case 'i':
				/* ignored, deprecated option */
				break;

			case 'j':			/* number of dump jobs */
				numWorkers = atoi(optarg);
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
				pgport = pg_strdup(optarg);
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
				username = pg_strdup(optarg);
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

			case 'Z':			/* Compression Level */
				compressLevel = atoi(optarg);
				if (compressLevel < 0 || compressLevel > 9)
				{
					write_msg(NULL, "compression level must be in range 0..9\n");
					exit_nicely(1);
				}
				break;

			case 0:
				/* This covers the long options. */
				break;

			case 2:				/* lock-wait-timeout */
				lockWaitTimeout = pg_strdup(optarg);
				break;

			case 3:				/* SET ROLE */
				use_role = pg_strdup(optarg);
				break;

			case 4:				/* exclude table(s) data */
				simple_string_list_append(&tabledata_exclude_patterns, optarg);
				break;

			case 5:				/* section */
				set_dump_section(optarg, &dumpSections);
				break;

			case 1000:				/* gp-syntax */
				if (gp_syntax_option != GPS_NOT_SPECIFIED)
				{
					write_msg(NULL, "options \"--gp-syntax\" and \"--no-gp-syntax\" cannot be used together\n");
					exit(1);
				}
				gp_syntax_option = GPS_ENABLED;
				break;

			case 1001:				/* no-gp-syntax */
				if (gp_syntax_option != GPS_NOT_SPECIFIED)
				{
					write_msg(NULL, "options \"--gp-syntax\" and \"--no-gp-syntax\" cannot be used together\n");
					exit(1);
				}
				gp_syntax_option = GPS_DISABLED;
				break;

			case 1002:
				simple_string_list_append(&funcid_string_list, optarg);
				include_everything = false;
				break;

			case 1003:
				simple_string_list_append(&relid_string_list, optarg);
				include_everything = false;
				break;

			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit_nicely(1);
		}
	}

	/*
	 * Non-option argument specifies database name as long as it wasn't
	 * already specified with -d / --dbname
	 */
	if (optind < argc && dbname == NULL)
		dbname = argv[optind++];

	/* Complain if any arguments remain */
	if (optind < argc)
	{
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	/* --column-inserts implies --inserts */
	if (column_inserts)
		dump_inserts = 1;

	if (dataOnly && schemaOnly)
	{
		write_msg(NULL, "options -s/--schema-only and -a/--data-only cannot be used together\n");
		exit_nicely(1);
	}

	if (dataOnly && outputClean)
	{
		write_msg(NULL, "options -c/--clean and -a/--data-only cannot be used together\n");
		exit_nicely(1);
	}

	if (dump_inserts && oids)
	{
		write_msg(NULL, "options --inserts/--column-inserts and -o/--oids cannot be used together\n");
		write_msg(NULL, "(The INSERT command cannot set OIDs.)\n");
		exit_nicely(1);
	}

	if (if_exists && !outputClean)
		exit_horribly(NULL, "option --if-exists requires option -c/--clean\n");

	/* Identify archive format to emit */
	archiveFormat = parseArchiveFormat(format, &archiveMode);

	/* archiveFormat specific setup */
	if (archiveFormat == archNull)
		plainText = 1;

	/* Custom and directory formats are compressed by default, others not */
	if (compressLevel == -1)
	{
#ifdef HAVE_LIBZ
		if (archiveFormat == archCustom || archiveFormat == archDirectory)
			compressLevel = Z_DEFAULT_COMPRESSION;
		else
#endif
			compressLevel = 0;
	}

#ifndef HAVE_LIBZ
	if (compressLevel != 0)
		write_msg(NULL, "WARNING: requested compression not available in this "
				  "installation -- archive will be uncompressed\n");
	compressLevel = 0;
#endif

	/*
	 * On Windows we can only have at most MAXIMUM_WAIT_OBJECTS (= 64 usually)
	 * parallel jobs because that's the maximum limit for the
	 * WaitForMultipleObjects() call.
	 */
	if (numWorkers <= 0
#ifdef WIN32
		|| numWorkers > MAXIMUM_WAIT_OBJECTS
#endif
		)
		exit_horribly(NULL, "invalid number of parallel jobs\n");

	/* Parallel backup only in the directory archive format so far */
	if (archiveFormat != archDirectory && numWorkers > 1)
		exit_horribly(NULL, "parallel backup only supported by the directory format\n");

	/* Open the output file */
	fout = CreateArchive(filename, archiveFormat, compressLevel, archiveMode,
						 setupDumpWorker);

	/* Register the cleanup hook */
	on_exit_close_archive(fout);

	if (fout == NULL)
		exit_horribly(NULL, "could not open output file \"%s\" for writing\n", filename);

	/* Let the archiver know how noisy to be */
	fout->verbose = g_verbose;

	/*
	 * We allow the server to be back to 8.3, and up to any minor release of
	 * our own major version.  (See also version check in pg_dumpall.c.)
	 */
	fout->minRemoteVersion = 80300;	/* we can handle back to 8.3 */
	fout->maxRemoteVersion = (PG_VERSION_NUM / 100) * 100 + 99;

	fout->numWorkers = numWorkers;

	/*
	 * Open the database using the Archiver, so it knows about it. Errors mean
	 * death.
	 */
	ConnectDatabase(fout, dbname, pghost, pgport, username, prompt_password, binary_upgrade);
	setup_connection(fout, dumpencoding, use_role);

	/*
	 * Determine whether or not we're interacting with a GP backend.
	 */
	isGPbackend = testGPbackend(fout);

	/*
	 * Now that the type of backend is known, determine the gp-syntax option
	 * value and set processing accordingly.
	 */
	switch (gp_syntax_option)
	{
		case GPS_NOT_SPECIFIED:
			dumpPolicy = isGPbackend;
			break;
		case GPS_DISABLED:
			dumpPolicy = false;
			break;
		case GPS_ENABLED:
			dumpPolicy = isGPbackend;
			if (!isGPbackend)
			{
				write_msg(NULL, "Server is not a Greenplum Database instance; --gp-syntax option ignored.\n");
			}
			break;
	}

	/*
	 * Disable security label support if server version < v9.1.x (prevents
	 * access to nonexistent pg_seclabel catalog)
	 */
	if (fout->remoteVersion < 90100)
		no_security_labels = 1;

	/*
	 * When running against 9.0 or later, check if we are in recovery mode,
	 * which means we are on a hot standby.
	 */
	if (fout->remoteVersion >= 90000)
	{
		PGresult   *res = ExecuteSqlQueryForSingleRow(fout, "SELECT pg_catalog.pg_is_in_recovery()");

		if (strcmp(PQgetvalue(res, 0, 0), "t") == 0)
		{
			/*
			 * On hot standby slaves, never try to dump unlogged table data,
			 * since it will just throw an error.
			 */
			no_unlogged_table_data = true;
		}
		PQclear(res);
	}

	/* check the version for the synchronized snapshots feature */
	if (numWorkers > 1 && fout->remoteVersion < 90200
		&& !no_synchronized_snapshots)
		exit_horribly(NULL,
		 "Synchronized snapshots are not supported by this server version.\n"
		  "Run with --no-synchronized-snapshots instead if you do not need\n"
					  "synchronized snapshots.\n");

	/* Expand schema selection patterns into OID lists */
	if (schema_include_patterns.head != NULL)
	{
		expand_schema_name_patterns(fout, &schema_include_patterns,
									&schema_include_oids);
		if (schema_include_oids.head == NULL)
			exit_horribly(NULL, "No matching schemas were found\n");
	}
	expand_schema_name_patterns(fout, &schema_exclude_patterns,
								&schema_exclude_oids);
	/* non-matching exclusion patterns aren't an error */

	/* Expand table selection patterns into OID lists */
	if (table_include_patterns.head != NULL)
	{
		expand_table_name_patterns(fout, &table_include_patterns,
								   &table_include_oids);
		if (table_include_oids.head == NULL)
			exit_horribly(NULL, "No matching tables were found\n");
	}
	expand_table_name_patterns(fout, &table_exclude_patterns,
							   &table_exclude_oids);

	expand_table_name_patterns(fout, &tabledata_exclude_patterns,
							   &tabledata_exclude_oids);

	/* non-matching exclusion patterns aren't an error */


	expand_oid_patterns(&relid_string_list, &table_include_oids);
	expand_oid_patterns(&funcid_string_list, &function_include_oids);

	/*
	 * Dumping blobs is now default unless we saw an inclusion switch or -s
	 * ... but even if we did see one of these, -b turns it back on.
	 */
	if (include_everything && !schemaOnly)
		outputBlobs = true;

	/*
	 * Collect role names so we can map object owner OIDs to names.
	 */
	collectRoleNames(fout);

	/*
	 * Now scan the database and create DumpableObject structs for all the
	 * objects we intend to dump.
	 */
	tblinfo = getSchemaData(fout, &numTables, binary_upgrade);

	if (fout->remoteVersion < 80400)
		guessConstraintInheritance(tblinfo, numTables);

	if (!schemaOnly)
	{
		getTableData(tblinfo, numTables, oids);
		buildMatViewRefreshDependencies(fout);
		if (dataOnly)
			getTableDataFKConstraints();
	}

	/*
	 * In binary-upgrade mode, we do not have to worry about the actual blob
	 * data or the associated metadata that resides in the pg_largeobject and
	 * pg_largeobject_metadata tables, respectivly.
	 *
	 * However, we do need to collect blob information as there may be
	 * comments or other information on blobs that we do need to dump out.
	 */
	if (outputBlobs || binary_upgrade)
		getBlobs(fout);

	/*
	 * Collect dependency data to assist in ordering the objects.
	 */
	getDependencies(fout);

	setExtPartDependency(tblinfo, numTables);

	/* Lastly, create dummy objects to represent the section boundaries */
	boundaryObjs = createBoundaryObjects();

	/* Get pointers to all the known DumpableObjects */
	getDumpableObjects(&dobjs, &numObjs);

	/*
	 * Add dummy dependencies to enforce the dump section ordering.
	 */
	addBoundaryDependencies(dobjs, numObjs, boundaryObjs);

	/* Sort the objects into a safe dump order (no forward references). */
	sortDumpableObjectsByTypeName(dobjs, numObjs);

	/* If we do a parallel dump, we want the largest tables to go first */
	if (archiveFormat == archDirectory && numWorkers > 1)
		sortDataAndIndexObjectsBySize(dobjs, numObjs);

	sortDumpableObjects(dobjs, numObjs,
						boundaryObjs[0].dumpId, boundaryObjs[1].dumpId);

	/*
	 * Create archive TOC entries for all the objects to be dumped, in a safe
	 * order.
	 */

	/* First the special ENCODING, STDSTRINGS, and SEARCHPATH entries. */
	dumpEncoding(fout);
	dumpStdStrings(fout);
	dumpSearchPath(fout);

	/* The database item is always next, unless we don't want it at all */
	if (include_everything && !dataOnly)
		dumpDatabase(fout);

	int binfo_index = -1;
	/* Now the rearrangeable objects. */
	for (i = 0; i < numObjs; i++)
	{
		dumpDumpableObject(fout, dobjs[i]);
		if (dobjs[i]->objType == DO_BINARY_UPGRADE)
			binfo_index = i;
	}

	/* Amend the Oid preassignment TOC with the actual Oids gathered */
	if (binary_upgrade && binfo_index >= 0)
		dumpPreassignedOidDefinition(fout, (BinaryUpgradeInfo *) dobjs[binfo_index]);

	/*
	 * Set up options info to ensure we dump what we want.
	 */
	ropt = NewRestoreOptions();
	ropt->filename = filename;
	ropt->dropSchema = outputClean;
	ropt->dataOnly = dataOnly;
	ropt->schemaOnly = schemaOnly;
	ropt->if_exists = if_exists;
	ropt->dumpSections = dumpSections;
	ropt->aclsSkip = aclsSkip;
	ropt->superuser = outputSuperuser;
	ropt->createDB = outputCreateDB;
	ropt->noOwner = outputNoOwner;
	ropt->noTablespace = outputNoTablespaces;
	ropt->disable_triggers = disable_triggers;
	ropt->use_setsessauth = use_setsessauth;
	ropt->binary_upgrade = binary_upgrade;

	if (compressLevel == -1)
		ropt->compression = 0;
	else
		ropt->compression = compressLevel;

	ropt->suppressDumpWarnings = true;	/* We've already shown them */

	ropt->binary_upgrade = binary_upgrade;

	SetArchiveRestoreOptions(fout, ropt);

	/*
	 * The archive's TOC entries are now marked as to which ones will actually
	 * be output, so we can set up their dependency lists properly. This isn't
	 * necessary for plain-text output, though.
	 */
	if (!plainText)
		BuildArchiveDependencies(fout);

	/*
	 * And finally we can do the actual output.
	 *
	 * Note: for non-plain-text output formats, the output file is written
	 * inside CloseArchive().  This is, um, bizarre; but not worth changing
	 * right now.
	 */
	if (plainText)
		RestoreArchive(fout);

	CloseArchive(fout);

	exit_nicely(0);
}


static void
help(const char *progname)
{
	printf(_("%s dumps a database as a text file or to other formats.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... [DBNAME]\n"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  -f, --file=FILENAME          output file or directory name\n"));
	printf(_("  -F, --format=c|d|t|p         output file format (custom, directory, tar,\n"
			 "                               plain text (default))\n"));
	printf(_("  -j, --jobs=NUM               use this many parallel jobs to dump\n"));
	printf(_("  -v, --verbose                verbose mode\n"));
	printf(_("  -V, --version                output version information, then exit\n"));
	printf(_("  -Z, --compress=0-9           compression level for compressed formats\n"));
	printf(_("  --lock-wait-timeout=TIMEOUT  fail after waiting TIMEOUT for a table lock\n"));
	printf(_("  -?, --help                   show this help, then exit\n"));

	printf(_("\nOptions controlling the output content:\n"));
	printf(_("  -a, --data-only              dump only the data, not the schema\n"));
	printf(_("  -b, --blobs                  include large objects in dump\n"));
	printf(_("  -c, --clean                  clean (drop) database objects before recreating\n"));
	printf(_("  -C, --create                 include commands to create database in dump\n"));
	printf(_("  -E, --encoding=ENCODING      dump the data in encoding ENCODING\n"));
	printf(_("  -n, --schema=SCHEMA          dump the named schema(s) only\n"));
	printf(_("  -N, --exclude-schema=SCHEMA  do NOT dump the named schema(s)\n"));
	printf(_("  -o, --oids                   include OIDs in dump\n"));
	printf(_("  -O, --no-owner               skip restoration of object ownership in\n"
			 "                               plain-text format\n"));
	printf(_("  -s, --schema-only            dump only the schema, no data\n"));
	printf(_("  -S, --superuser=NAME         superuser user name to use in plain-text format\n"));
	printf(_("  -t, --table=TABLE            dump the named table(s) only\n"));
	printf(_("  -T, --exclude-table=TABLE    do NOT dump the named table(s)\n"));
	printf(_("  -x, --no-privileges          do not dump privileges (grant/revoke)\n"));
	printf(_("  --binary-upgrade             for use by upgrade utilities only\n"));
	printf(_("  --column-inserts             dump data as INSERT commands with column names\n"));
	printf(_("  --disable-dollar-quoting     disable dollar quoting, use SQL standard quoting\n"));
	printf(_("  --disable-triggers           disable triggers during data-only restore\n"));
	printf(_("  --exclude-table-data=TABLE   do NOT dump data for the named table(s)\n"));
	printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
	printf(_("  --inserts                    dump data as INSERT commands, rather than COPY\n"));
	printf(_("  --no-security-labels         do not dump security label assignments\n"));
	printf(_("  --no-synchronized-snapshots  do not use synchronized snapshots in parallel jobs\n"));
	printf(_("  --no-tablespaces             do not dump tablespace assignments\n"));
	printf(_("  --no-unlogged-table-data     do not dump unlogged table data\n"));
	printf(_("  --quote-all-identifiers      quote all identifiers, even if not key words\n"));
	printf(_("  --section=SECTION            dump named section (pre-data, data, or post-data)\n"));
	printf(_("  --serializable-deferrable    wait until the dump can run without anomalies\n"));
	printf(_("  --use-set-session-authorization\n"
			 "                               use SET SESSION AUTHORIZATION commands instead of\n"
			 "                               ALTER OWNER commands to set ownership\n"));

	/* START MPP ADDITION */
	printf(_("  --gp-syntax                  dump with Greenplum Database syntax (default if gpdb)\n"));
	printf(_("  --no-gp-syntax               dump without Greenplum Database syntax (default if postgresql)\n"));
	printf(_("  --function-oids              dump only function(s) of given list of oids\n"));
	printf(_("  --relation-oids              dump only relation(s) of given list of oids\n"));
	/* END MPP ADDITION */

	printf(_("\nConnection options:\n"));
	printf(_("  -d, --dbname=DBNAME      database to dump\n"));
	printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(_("  -p, --port=PORT          database server port number\n"));
	printf(_("  -U, --username=NAME      connect as specified database user\n"));
	printf(_("  -w, --no-password        never prompt for password\n"));
	printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
	printf(_("  --role=ROLENAME          do SET ROLE before dump\n"));

	printf(_("\nIf no database name is supplied, then the PGDATABASE environment\n"
			 "variable value is used.\n\n"));
	printf(_("Report bugs to <bugs@greenplum.org>.\n"));
}

static void
setup_connection(Archive *AH, const char *dumpencoding, char *use_role)
{
	PGconn	   *conn = GetConnection(AH);
	const char *std_strings;

	PQclear(ExecuteSqlQueryForSingleRow(AH, ALWAYS_SECURE_SEARCH_PATH_SQL));

	/*
	 * Set the client encoding if requested.
	 */
	if (dumpencoding)
	{
		if (PQsetClientEncoding(conn, dumpencoding) < 0)
			exit_horribly(NULL, "invalid client encoding \"%s\" specified\n",
						  dumpencoding);
	}

	/*
	 * Get the active encoding and the standard_conforming_strings setting, so
	 * we know how to escape strings.
	 */
	AH->encoding = PQclientEncoding(conn);

	std_strings = PQparameterStatus(conn, "standard_conforming_strings");
	AH->std_strings = (std_strings && strcmp(std_strings, "on") == 0);

	/*
	 * Set the role if requested.  In a parallel dump worker, we'll be passed
	 * use_role == NULL, but AH->use_role is already set (if user specified it
	 * originally) and we should use that.
	 */
	if (!use_role && AH->use_role)
		use_role = AH->use_role;

	/* Set the role if requested */
	if (use_role)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
		ExecuteSqlStatement(AH, query->data);
		destroyPQExpBuffer(query);

		/* save it for possible later use by parallel workers */
		if (!AH->use_role)
			AH->use_role = pg_strdup(use_role);
	}

	/* Set the datestyle to ISO to ensure the dump's portability */
	ExecuteSqlStatement(AH, "SET DATESTYLE = ISO");

	/* Likewise, avoid using sql_standard intervalstyle */
	if (AH->remoteVersion >= 80400)
		ExecuteSqlStatement(AH, "SET INTERVALSTYLE = POSTGRES");

	/*
	 * If supported, set extra_float_digits so that we can dump float data
	 * exactly (given correctly implemented float I/O code, anyway)
	 */
	if (AH->remoteVersion >= 90000)
		ExecuteSqlStatement(AH, "SET extra_float_digits TO 3");
	else
		ExecuteSqlStatement(AH, "SET extra_float_digits TO 2");

	/*
	 * If synchronized scanning is supported, disable it, to prevent
	 * unpredictable changes in row ordering across a dump and reload.
	 */
	if (AH->remoteVersion >= 80300)
		ExecuteSqlStatement(AH, "SET synchronize_seqscans TO off");

	/*
	 * The default for enable_nestloop is off in GPDB. However, many of the queries
	 * that we issue best run with nested loop joins, so enable it.
	 */
	ExecuteSqlStatement(AH, "SET enable_nestloop TO on");

	/*
	 * Disable timeouts if supported.
	 */
	ExecuteSqlStatement(AH, "SET statement_timeout = 0");

	if (AH->remoteVersion >= 90300)
		ExecuteSqlStatement(AH, "SET lock_timeout = 0");

	/*
	 * Quote all identifiers, if requested.
	 */
	if (quote_all_identifiers)
		ExecuteSqlStatement(AH, "SET quote_all_identifiers = true");

	/*
	 * Initialize prepared-query state to "nothing prepared".  We do this here
	 * so that a parallel dump worker will have its own state.
	 */
	AH->is_prepared = (bool *) pg_malloc0(NUM_PREP_QUERIES * sizeof(bool));

	/*
	 * Start transaction-snapshot mode transaction to dump consistent data.
	 */
	ExecuteSqlStatement(AH, "BEGIN");
	if (AH->remoteVersion >= 90100)
	{
		/*
		 * To support the combination of serializable_deferrable with the jobs
		 * option we use REPEATABLE READ for the worker connections that are
		 * passed a snapshot.  As long as the snapshot is acquired in a
		 * SERIALIZABLE, READ ONLY, DEFERRABLE transaction, its use within a
		 * REPEATABLE READ transaction provides the appropriate integrity
		 * guarantees.  This is a kluge, but safe for back-patching.
		 */
		if (serializable_deferrable && AH->sync_snapshot_id == NULL)
			ExecuteSqlStatement(AH,
								"SET TRANSACTION ISOLATION LEVEL "
								"SERIALIZABLE, READ ONLY, DEFERRABLE");
		else
			ExecuteSqlStatement(AH,
								"SET TRANSACTION ISOLATION LEVEL "
								"REPEATABLE READ, READ ONLY");
	}
	else
	{
		ExecuteSqlStatement(AH,
							"SET TRANSACTION ISOLATION LEVEL "
							"SERIALIZABLE, READ ONLY");
	}

	if (AH->numWorkers > 1 && AH->remoteVersion >= 90200 && !no_synchronized_snapshots)
	{
		if (AH->sync_snapshot_id)
		{
			PQExpBuffer query = createPQExpBuffer();

			appendPQExpBufferStr(query, "SET TRANSACTION SNAPSHOT ");
			appendStringLiteralConn(query, AH->sync_snapshot_id, conn);
			ExecuteSqlStatement(AH, query->data);
			destroyPQExpBuffer(query);
		}
		else
			AH->sync_snapshot_id = get_synchronized_snapshot(AH);
	}
}

/* Set up connection for a parallel worker process */
static void
setupDumpWorker(Archive *AH, RestoreOptions *ropt)
{
	/*
	 * We want to re-select all the same values the master connection is
	 * using.  We'll have inherited directly-usable values in
	 * AH->sync_snapshot_id and AH->use_role, but we need to translate the
	 * inherited encoding value back to a string to pass to setup_connection.
	 */
	setup_connection(AH,
					 pg_encoding_to_char(AH->encoding),
					 NULL);
}

static char *
get_synchronized_snapshot(Archive *fout)
{
	char	   *query = "SELECT pg_catalog.pg_export_snapshot()";
	char	   *result;
	PGresult   *res;

	res = ExecuteSqlQueryForSingleRow(fout, query);
	result = pg_strdup(PQgetvalue(res, 0, 0));
	PQclear(res);

	return result;
}

static ArchiveFormat
parseArchiveFormat(const char *format, ArchiveMode *mode)
{
	ArchiveFormat archiveFormat;

	*mode = archModeWrite;

	if (pg_strcasecmp(format, "a") == 0 || pg_strcasecmp(format, "append") == 0)
	{
		/* This is used by pg_dumpall, and is not documented */
		archiveFormat = archNull;
		*mode = archModeAppend;
	}
	else if (pg_strcasecmp(format, "c") == 0)
		archiveFormat = archCustom;
	else if (pg_strcasecmp(format, "custom") == 0)
		archiveFormat = archCustom;
	else if (pg_strcasecmp(format, "d") == 0)
		archiveFormat = archDirectory;
	else if (pg_strcasecmp(format, "directory") == 0)
		archiveFormat = archDirectory;
	else if (pg_strcasecmp(format, "p") == 0)
		archiveFormat = archNull;
	else if (pg_strcasecmp(format, "plain") == 0)
		archiveFormat = archNull;
	else if (pg_strcasecmp(format, "t") == 0)
		archiveFormat = archTar;
	else if (pg_strcasecmp(format, "tar") == 0)
		archiveFormat = archTar;
	else
		exit_horribly(NULL, "invalid output format \"%s\" specified\n", format);
	return archiveFormat;
}

/*
 * Find the OIDs of all schemas matching the given list of patterns,
 * and append them to the given OID list.
 */
static void
expand_schema_name_patterns(Archive *fout,
							SimpleStringList *patterns,
							SimpleOidList *oids)
{
	PQExpBuffer query;
	PGresult   *res;
	SimpleStringListCell *cell;
	int			i;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	query = createPQExpBuffer();

	/*
	 * We use UNION ALL rather than UNION; this might sometimes result in
	 * duplicate entries in the OID list, but we don't care.
	 */

	for (cell = patterns->head; cell; cell = cell->next)
	{
		if (cell != patterns->head)
			appendPQExpBufferStr(query, "UNION ALL\n");
		appendPQExpBuffer(query,
						  "SELECT oid FROM pg_catalog.pg_namespace n\n");
		processSQLNamePattern(GetConnection(fout), query, cell->val, false,
							  false, NULL, "n.nspname", NULL, NULL);
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	for (i = 0; i < PQntuples(res); i++)
	{
		simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
	}

	PQclear(res);
	destroyPQExpBuffer(query);
}

/*
 * Find the OIDs of all tables matching the given list of patterns,
 * and append them to the given OID list.
 */
static void
expand_table_name_patterns(Archive *fout,
						   SimpleStringList *patterns, SimpleOidList *oids)
{
	PQExpBuffer query;
	PGresult   *res;
	SimpleStringListCell *cell;
	int			i;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	query = createPQExpBuffer();

	/*
	 * We use UNION ALL rather than UNION; this might sometimes result in
	 * duplicate entries in the OID list, but we don't care.
	 */

	for (cell = patterns->head; cell; cell = cell->next)
	{
		/*
		 * Query must remain ABSOLUTELY devoid of unqualified names.  This
		 * would be unnecessary given a pg_table_is_visible() variant taking a
		 * search_path argument.
		 */
		if (cell != patterns->head)
			appendPQExpBufferStr(query, "UNION ALL\n");
		appendPQExpBuffer(query,
						  "SELECT c.oid"
						  "\nFROM pg_catalog.pg_class c"
						  "\n     LEFT JOIN pg_catalog.pg_namespace n"
						  "\n     ON n.oid OPERATOR(pg_catalog.=) c.relnamespace"
						  "\nWHERE c.relkind OPERATOR(pg_catalog.=) ANY"
						  "\n    (array['%c', '%c', '%c', '%c', '%c'])\n",
						  RELKIND_RELATION, RELKIND_SEQUENCE, RELKIND_VIEW,
						  RELKIND_MATVIEW, RELKIND_FOREIGN_TABLE);
		processSQLNamePattern(GetConnection(fout), query, cell->val, true,
							  false, "n.nspname", "c.relname", NULL,
							  "pg_catalog.pg_table_is_visible(c.oid)");
	}

	ExecuteSqlStatement(fout, "RESET search_path");
	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
	PQclear(ExecuteSqlQueryForSingleRow(fout, ALWAYS_SECURE_SEARCH_PATH_SQL));

	for (i = 0; i < PQntuples(res); i++)
	{
		simple_oid_list_append(oids, atooid(PQgetvalue(res, i, 0)));
	}

	PQclear(res);
	destroyPQExpBuffer(query);
}

/*
 * checkExtensionMembership
 *		Determine whether object is an extension member, and if so,
 *		record an appropriate dependency and set the object's dump flag.
 *
 * It's important to call this for each object that could be an extension
 * member.  Generally, we integrate this with determining the object's
 * to-be-dumped-ness, since extension membership overrides other rules for that.
 *
 * Returns true if object is an extension member, else false.
 */
static bool
checkExtensionMembership(DumpableObject *dobj)
{
	ExtensionInfo *ext = findOwningExtension(dobj->catId);

	if (ext == NULL)
		return false;

	dobj->ext_member = true;

	/* Record dependency so that getDependencies needn't deal with that */
	addObjectDependency(dobj, ext->dobj.dumpId);

	/*
	 * Normally, mark the member object as not to be dumped.  But in binary
	 * upgrades, we still dump the members individually, since the idea is to
	 * exactly reproduce the database contents rather than replace the
	 * extension contents with something different.
	 */
	if (!binary_upgrade)
		dobj->dump = false;
	else
		dobj->dump = ext->dobj.dump;

	return true;
}

/*
 * Parse the OIDs matching the given list of patterns separated by non-digit
 * characters, and append them to the given OID list.
 */
static void
expand_oid_patterns(SimpleStringList *patterns, SimpleOidList *oids)
{
	SimpleStringListCell *cell;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	for (cell = patterns->head; cell; cell = cell->next)
	{
		const char *seperator = ",";
		char *oidstr = pg_strdup(cell->val);
		if (oidstr == NULL)
		{
			write_msg(NULL, "memory allocation failed for function \"expand_oid_patterns\"\n");
			exit_nicely(1);
		}

		char *token = strtok(oidstr, seperator);
		while (token)
		{
			if (strstr(seperator, token) == NULL)
				simple_oid_list_append(oids, atooid(token));

			token = strtok(NULL, seperator);
		}

		free(oidstr);
	}
}

/*
 * selectDumpableNamespace: policy-setting subroutine
 *		Mark a namespace as to be dumped or not
 *
 * Normally, we dump all extensions, or none of them if include_everything
 * is false (i.e., a --schema or --table switch was given).  However, in
 * binary-upgrade mode it's necessary to skip built-in extensions, since we
 * assume those will already be installed in the target database.  We identify
 * such extensions by their having OIDs in the range reserved for initdb.
 */
static void
selectDumpableNamespace(NamespaceInfo *nsinfo)
{
	if (checkExtensionMembership(&nsinfo->dobj))
		return;					/* extension membership overrides all else */

	/*
	 * If specific tables are being dumped, do not dump any complete
	 * namespaces. If specific namespaces are being dumped, dump just those
	 * namespaces. Otherwise, dump all non-system namespaces.
	 */
	if (table_include_oids.head != NULL)
		nsinfo->dobj.dump = false;
	else if (schema_include_oids.head != NULL)
		nsinfo->dobj.dump = simple_oid_list_member(&schema_include_oids,
												   nsinfo->dobj.catId.oid);
	else if (strncmp(nsinfo->dobj.name, "pg_", 3) == 0 ||
			 strcmp(nsinfo->dobj.name, "information_schema") == 0 ||
			 strcmp(nsinfo->dobj.name, "gp_toolkit") == 0)
		nsinfo->dobj.dump = false;
	else
		nsinfo->dobj.dump = true;

	/*
	 * In any case, a namespace can be excluded by an exclusion switch
	 */
	if (nsinfo->dobj.dump &&
		simple_oid_list_member(&schema_exclude_oids,
							   nsinfo->dobj.catId.oid))
		nsinfo->dobj.dump = false;
}

/*
 * selectDumpableTable: policy-setting subroutine
 *		Mark a table as to be dumped or not
 */
static void
selectDumpableTable(TableInfo *tbinfo)
{
	if (checkExtensionMembership(&tbinfo->dobj))
		return;					/* extension membership overrides all else */

	/*
	 * If specific tables are being dumped, dump just those tables; else, dump
	 * according to the parent namespace's dump flag.
	 */
	if (table_include_oids.head != NULL)
		tbinfo->dobj.dump = simple_oid_list_member(&table_include_oids,
												   tbinfo->dobj.catId.oid);
	else
		tbinfo->dobj.dump = tbinfo->dobj.namespace->dobj.dump;

	/*
	 * In any case, a table can be excluded by an exclusion switch
	 */
	if (tbinfo->dobj.dump &&
		simple_oid_list_member(&table_exclude_oids,
							   tbinfo->dobj.catId.oid))
		tbinfo->dobj.dump = false;
}

/*
 * selectDumpableType: policy-setting subroutine
 *		Mark a type as to be dumped or not
 *
 * If it's a table's rowtype or an autogenerated array type, we also apply a
 * special type code to facilitate sorting into the desired order.  (We don't
 * want to consider those to be ordinary types because that would bring tables
 * up into the datatype part of the dump order.)  We still set the object's
 * dump flag; that's not going to cause the dummy type to be dumped, but we
 * need it so that casts involving such types will be dumped correctly -- see
 * dumpCast.  This means the flag should be set the same as for the underlying
 * object (the table or base type).
 */
static void
selectDumpableType(TypeInfo *tyinfo)
{
	/* skip complex types, except for standalone composite types */
	if (OidIsValid(tyinfo->typrelid) &&
			tyinfo->typrelkind != RELKIND_COMPOSITE_TYPE)
	{
		TableInfo  *tytable = findTableByOid(tyinfo->typrelid);

		tyinfo->dobj.objType = DO_DUMMY_TYPE;
		if (tytable != NULL)
			tyinfo->dobj.dump = tytable->dobj.dump;
		else
			tyinfo->dobj.dump = false;
		return;
	}

	/* skip auto-generated array types */
	if (tyinfo->isArray)
	{
		tyinfo->dobj.objType = DO_DUMMY_TYPE;

		/*
		 * Fall through to set the dump flag; we assume that the subsequent
		 * rules will do the same thing as they would for the array's base
		 * type.  (We cannot reliably look up the base type here, since
		 * getTypes may not have processed it yet.)
		 */
	}

	if (checkExtensionMembership(&tyinfo->dobj))
		return;					/* extension membership overrides all else */

	/* dump only types in dumpable namespaces */
	if (!tyinfo->dobj.namespace->dobj.dump)
		tyinfo->dobj.dump = false;

	/* skip undefined placeholder types */
	else if (!tyinfo->isDefined)
		tyinfo->dobj.dump = false;

	/* skip auto-generated array types */
	else if (tyinfo->isArray)
		tyinfo->dobj.dump = false;

	else
		tyinfo->dobj.dump = true;
}

/*
 * selectDumpableFunction: policy-setting subroutine
 *		Mark a function as to be dumped or not
 */
static void
selectDumpableFunction(FuncInfo *finfo)
{
	/*
	 * If specific functions are being dumped, dump just those functions; else, dump
	 * according to the parent namespace's dump flag if parent namespace is not null;
	 * else, always dump the function.
	 */
	if (function_include_oids.head != NULL)
		finfo->dobj.dump = simple_oid_list_member(&function_include_oids,
												   finfo->dobj.catId.oid);
	else if (finfo->dobj.namespace)
		finfo->dobj.dump = finfo->dobj.namespace->dobj.dump;
	else
		finfo->dobj.dump = true;
}

/*
 * selectDumpableDefaultACL: policy-setting subroutine
 *		Mark a default ACL as to be dumped or not
 *
 * For per-schema default ACLs, dump if the schema is to be dumped.
 * Otherwise dump if we are dumping "everything".  Note that dataOnly
 * and aclsSkip are checked separately.
 */
static void
selectDumpableDefaultACL(DefaultACLInfo *dinfo)
{
	/* Default ACLs can't be extension members */

	if (dinfo->dobj.namespace)
		dinfo->dobj.dump = dinfo->dobj.namespace->dobj.dump;
	else
		dinfo->dobj.dump = include_everything;
}

/*
 * selectDumpableCast: policy-setting subroutine
 *		Mark a cast as to be dumped or not
 *
 * Casts do not belong to any particular namespace (since they haven't got
 * names), nor do they have identifiable owners.  To distinguish user-defined
 * casts from built-in ones, we must resort to checking whether the cast's
 * OID is in the range reserved for initdb.
 */
static void
selectDumpableCast(CastInfo *cast)
{
	if (checkExtensionMembership(&cast->dobj))
		return;					/* extension membership overrides all else */

	if (cast->dobj.catId.oid < (Oid) FirstNormalObjectId)
		cast->dobj.dump = false;
	else
		cast->dobj.dump = include_everything;
}

/*
 * selectDumpableProcLang: policy-setting subroutine
 *		Mark a procedural language as to be dumped or not
 *
 * Procedural languages do not belong to any particular namespace.  To
 * identify built-in languages, we must resort to checking whether the
 * language's OID is in the range reserved for initdb.
 */
static void
selectDumpableProcLang(ProcLangInfo *plang)
{
	if (checkExtensionMembership(&plang->dobj))
		return;					/* extension membership overrides all else */

	if (plang->dobj.catId.oid < (Oid) FirstNormalObjectId)
		plang->dobj.dump = false;
	else
		plang->dobj.dump = include_everything;
}

/*
 * selectDumpableExtension: policy-setting subroutine
 *		Mark an extension as to be dumped or not
 *
 * Normally, we dump all extensions, or none of them if include_everything
 * is false (i.e., a --schema or --table switch was given).  However, in
 * binary-upgrade mode it's necessary to skip built-in extensions, since we
 * assume those will already be installed in the target database.  We identify
 * such extensions by their having OIDs in the range reserved for initdb.
 */
static void
selectDumpableExtension(ExtensionInfo *extinfo)
{
	if (binary_upgrade && extinfo->dobj.catId.oid < (Oid) FirstNormalObjectId)
		extinfo->dobj.dump = false;
	else
		extinfo->dobj.dump = include_everything;
}

/*
 * selectDumpableObject: policy-setting subroutine
 *		Mark a generic dumpable object as to be dumped or not
 *
 * Use this only for object types without a special-case routine above.
 */
static void
selectDumpableObject(DumpableObject *dobj)
{
	if (checkExtensionMembership(dobj))
		return;					/* extension membership overrides all else */

	/*
	 * Default policy is to dump if parent namespace is dumpable, or for
	 * non-namespace-associated items, dump if we're dumping "everything".
	 */
	if (dobj->namespace)
		dobj->dump = dobj->namespace->dobj.dump;
	else
		dobj->dump = include_everything;
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

	/*
	 * Note: can't use getThreadLocalPQExpBuffer() here, we're calling fmtId
	 * which uses it already.
	 */
	PQExpBuffer clistBuf = createPQExpBuffer();
	PGconn	   *conn = GetConnection(fout);
	PGresult   *res;
	int			ret;
	char	   *copybuf;
	const char *column_list;

	if (g_verbose)
		write_msg(NULL, "dumping contents of table %s\n", classname);

	/*
	 * Specify the column list explicitly so that we have no possibility of
	 * retrieving data in the wrong column order.  (The default column
	 * ordering of COPY will not be what we want in certain corner cases
	 * involving ADD COLUMN and inheritance.)
	 */
	column_list = fmtCopyColumnList(tbinfo, clistBuf);

	if (oids && hasoids)
	{
		appendPQExpBuffer(q, "COPY %s %s WITH OIDS TO stdout;",
						  fmtQualifiedDumpable(tbinfo),
						  column_list);
	}
	else if (tdinfo->filtercond)
	{
		appendPQExpBufferStr(q, "COPY (SELECT ");
		/* klugery to get rid of parens in column list */
		if (strlen(column_list) > 2)
		{
			appendPQExpBufferStr(q, column_list + 1);
			q->data[q->len - 1] = ' ';
		}
		else
			appendPQExpBufferStr(q, "* ");
		appendPQExpBuffer(q, "FROM %s %s) TO stdout;",
						  fmtQualifiedDumpable(tbinfo),
						  tdinfo->filtercond);
	}
	else
	{
		appendPQExpBuffer(q, "COPY %s %s TO stdout;",
						  fmtQualifiedDumpable(tbinfo),
						  column_list);
	}
	res = ExecuteSqlQuery(fout, q->data, PGRES_COPY_OUT);
	PQclear(res);
	destroyPQExpBuffer(clistBuf);

	for (;;)
	{
		ret = PQgetCopyData(conn, &copybuf, 0);

		if (ret < 0)
			break;				/* done or error */

		if (copybuf)
		{
			WriteData(fout, copybuf, ret);
			PQfreemem(copybuf);
		}

		/* ----------
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
		 * If throttle is non-zero, then
		 *		See how long since the last sleep.
		 *		Work out how long to sleep (based on ratio).
		 *		If sleep is more than 100ms, then
		 *			sleep
		 *			reset timer
		 *		EndIf
		 * EndIf
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
		 * function to sleep for a subsecond period ie.
		 *
		 * select(0, NULL, NULL, NULL, &tvi);
		 *
		 * This will return after the interval specified in the structure tvi.
		 * Finally, call gettimeofday again to save the 'last sleep time'.
		 * ----------
		 */
	}
	archprintf(fout, "\\.\n\n\n");

	if (ret == -2)
	{
		/* copy data transfer failed */
		write_msg(NULL, "Dumping the contents of table \"%s\" failed: PQgetCopyData() failed.\n", classname);
		write_msg(NULL, "Error message from server: %s", PQerrorMessage(conn));
		write_msg(NULL, "The command was: %s\n", q->data);
		exit_nicely(1);
	}

	/* Check command status and return to normal libpq state */
	res = PQgetResult(conn);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		write_msg(NULL, "Dumping the contents of table \"%s\" failed: PQgetResult() failed.\n", classname);
		write_msg(NULL, "Error message from server: %s", PQerrorMessage(conn));
		write_msg(NULL, "The command was: %s\n", q->data);
		exit_nicely(1);
	}
	PQclear(res);

	/* Do this to ensure we've pumped libpq back to idle state */
	if (PQgetResult(conn) != NULL)
		write_msg(NULL, "WARNING: unexpected extra results during COPY of table \"%s\"\n",
				  classname);

	destroyPQExpBuffer(q);
	return 1;
}

/*
 * Dump table data using INSERT commands.
 *
 * Caution: when we restore from an archive file direct to database, the
 * INSERT commands emitted by this function have to be parsed by
 * pg_backup_db.c's ExecuteSimpleCommands(), which will not handle comments,
 * E'' strings, or dollar-quoted strings.  So don't emit anything like that.
 */
static int
dumpTableData_insert(Archive *fout, void *dcontext)
{
	TableDataInfo *tdinfo = (TableDataInfo *) dcontext;
	TableInfo  *tbinfo = tdinfo->tdtable;
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer insertStmt = NULL;
	PGresult   *res;
	int			tuple;
	int			nfields;
	int			field;


	appendPQExpBuffer(q, "DECLARE _pg_dump_cursor CURSOR FOR "
						"SELECT * FROM ONLY %s",
						fmtQualifiedDumpable(tbinfo));

	if (tdinfo->filtercond)
		appendPQExpBuffer(q, " %s", tdinfo->filtercond);

	ExecuteSqlStatement(fout, q->data);

	while (1)
	{
		res = ExecuteSqlQuery(fout, "FETCH 100 FROM _pg_dump_cursor",
							  PGRES_TUPLES_OK);
		nfields = PQnfields(res);
		for (tuple = 0; tuple < PQntuples(res); tuple++)
		{
			/*
			 * First time through, we build as much of the INSERT statement as
			 * possible in "insertStmt", which we can then just print for each
			 * line. If the table happens to have zero columns then this will
			 * be a complete statement, otherwise it will end in "VALUES(" and
			 * be ready to have the row's column values appended.
			 */
			if (insertStmt == NULL)
			{
				insertStmt = createPQExpBuffer();
				appendPQExpBuffer(insertStmt, "INSERT INTO %s ",
								  fmtQualifiedDumpable(tbinfo));

				/* corner case for zero-column table */
				if (nfields == 0)
				{
					appendPQExpBufferStr(insertStmt, "DEFAULT VALUES;\n");
				}
				else
				{
					/* append the list of column names if required */
					if (column_inserts)
					{
						appendPQExpBufferStr(insertStmt, "(");
						for (field = 0; field < nfields; field++)
						{
							if (field > 0)
								appendPQExpBufferStr(insertStmt, ", ");
							appendPQExpBufferStr(insertStmt,
												 fmtId(PQfname(res, field)));
						}
						appendPQExpBufferStr(insertStmt, ") ");
					}

					appendPQExpBufferStr(insertStmt, "VALUES (");
				}
			}

			archputs(insertStmt->data, fout);

			/* if it is zero-column table then we're done */
			if (nfields == 0)
				continue;

			for (field = 0; field < nfields; field++)
			{
				if (field > 0)
					archputs(", ", fout);
				if (PQgetisnull(res, tuple, field))
				{
					archputs("NULL", fout);
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
							 * scanner unquoted (e.g., 'NaN').  Note that
							 * strtod() and friends might accept NaN, so we
							 * can't use that to test.
							 *
							 * In reality we only need to defend against
							 * infinity and NaN, so we need not get too crazy
							 * about pattern matching here.
							 */
							const char *s = PQgetvalue(res, tuple, field);

							if (strspn(s, "0123456789 +-eE.") == strlen(s))
								archputs(s, fout);
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
							archputs("true", fout);
						else
							archputs("false", fout);
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
			archputs(");\n", fout);
		}

		if (PQntuples(res) <= 0)
		{
			PQclear(res);
			break;
		}
		PQclear(res);
	}

	archputs("\n\n", fout);

	ExecuteSqlStatement(fout, "CLOSE _pg_dump_cursor");

	destroyPQExpBuffer(q);
	if (insertStmt != NULL)
		destroyPQExpBuffer(insertStmt);

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
	PQExpBuffer clistBuf = createPQExpBuffer();
	DataDumperPtr dumpFn;
	char	   *copyStmt;

	if (!dump_inserts)
	{
		/* Dump/restore using COPY */
		dumpFn = dumpTableData_copy;
		/* must use 2 steps here 'cause fmtId is nonreentrant */
		appendPQExpBuffer(copyBuf, "COPY %s ",
						  fmtQualifiedDumpable(tbinfo));
		appendPQExpBuffer(copyBuf, "%s %sFROM stdin;\n",
						  fmtCopyColumnList(tbinfo, clistBuf),
					  (tdinfo->oids && tbinfo->hasoids) ? "WITH OIDS " : "");
		copyStmt = copyBuf->data;
	}
	else
	{
		/* Restore using INSERT */
		dumpFn = dumpTableData_insert;
		copyStmt = NULL;
	}

	/*
	 * Note: although the TableDataInfo is a full DumpableObject, we treat its
	 * dependency on its table as "special" and pass it to ArchiveEntry now.
	 * See comments for BuildArchiveDependencies.
	 */
	ArchiveEntry(fout, tdinfo->dobj.catId, tdinfo->dobj.dumpId,
				 tbinfo->dobj.name, tbinfo->dobj.namespace->dobj.name,
				 NULL, tbinfo->rolname,
				 false, "TABLE DATA", SECTION_DATA,
				 "", "", copyStmt,
				 &(tbinfo->dobj.dumpId), 1,
				 dumpFn, tdinfo);

	destroyPQExpBuffer(copyBuf);
	destroyPQExpBuffer(clistBuf);
}

/*
 * refreshMatViewData -
 *	  load or refresh the contents of a single materialized view
 *
 * Actually, this just makes an ArchiveEntry for the REFRESH MATERIALIZED VIEW
 * statement.
 */
static void
refreshMatViewData(Archive *fout, TableDataInfo *tdinfo)
{
	TableInfo  *tbinfo = tdinfo->tdtable;
	PQExpBuffer q;

	/* If the materialized view is not flagged as populated, skip this. */
	if (!tbinfo->relispopulated)
		return;

	q = createPQExpBuffer();

	appendPQExpBuffer(q, "REFRESH MATERIALIZED VIEW %s;\n",
					  fmtQualifiedDumpable(tbinfo));

	ArchiveEntry(fout,
				 tdinfo->dobj.catId,	/* catalog ID */
				 tdinfo->dobj.dumpId,	/* dump ID */
				 tbinfo->dobj.name,		/* Name */
				 tbinfo->dobj.namespace->dobj.name,		/* Namespace */
				 NULL,			/* Tablespace */
				 tbinfo->rolname,		/* Owner */
				 false,			/* with oids */
				 "MATERIALIZED VIEW DATA",		/* Desc */
				 SECTION_POST_DATA,		/* Section */
				 q->data,		/* Create */
				 "",			/* Del */
				 NULL,			/* Copy */
				 tdinfo->dobj.dependencies,		/* Deps */
				 tdinfo->dobj.nDeps,	/* # Deps */
				 NULL,			/* Dumper */
				 NULL);			/* Dumper Arg */

	destroyPQExpBuffer(q);
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
		if (tblinfo[i].dobj.dump)
			makeTableDataInfo(&(tblinfo[i]), oids);
	}
}

/*
 * Make a dumpable object for the data of this specific table
 *
 * Note: we make a TableDataInfo if and only if we are going to dump the
 * table data; the "dump" flag in such objects isn't used.
 */
static void
makeTableDataInfo(TableInfo *tbinfo, bool oids)
{
	TableDataInfo *tdinfo;

	/*
	 * Nothing to do if we already decided to dump the table.  This will
	 * happen for "config" tables.
	 */
	if (tbinfo->dataObj != NULL)
		return;

	/* Skip EXTERNAL TABLEs */
	if (tbinfo->relstorage == RELSTORAGE_EXTERNAL)
		return;
	/* Skip VIEWs (no data to dump) */
	if (tbinfo->relkind == RELKIND_VIEW)
		return;
	/* Skip FOREIGN TABLEs (no data to dump) */
	if (tbinfo->relkind == RELKIND_FOREIGN_TABLE)
		return;

	/* Don't dump data in unlogged tables, if so requested */
	if (tbinfo->relpersistence == RELPERSISTENCE_UNLOGGED &&
		no_unlogged_table_data)
		return;

	/* Check that the data is not explicitly excluded */
	if (simple_oid_list_member(&tabledata_exclude_oids,
							   tbinfo->dobj.catId.oid))
		return;

	/* OK, let's dump it */
	tdinfo = (TableDataInfo *) pg_malloc(sizeof(TableDataInfo));

	if (tbinfo->relkind == RELKIND_MATVIEW)
		tdinfo->dobj.objType = DO_REFRESH_MATVIEW;
	else
		tdinfo->dobj.objType = DO_TABLE_DATA;

	/*
	 * Note: use tableoid 0 so that this object won't be mistaken for
	 * something that pg_depend entries apply to.
	 */
	tdinfo->dobj.catId.tableoid = 0;
	tdinfo->dobj.catId.oid = tbinfo->dobj.catId.oid;
	AssignDumpId(&tdinfo->dobj);
	tdinfo->dobj.name = tbinfo->dobj.name;
	tdinfo->dobj.namespace = tbinfo->dobj.namespace;
	tdinfo->tdtable = tbinfo;
	tdinfo->oids = oids;
	tdinfo->filtercond = NULL;	/* might get set later */
	addObjectDependency(&tdinfo->dobj, tbinfo->dobj.dumpId);

	tbinfo->dataObj = tdinfo;
}

/*
 * The refresh for a materialized view must be dependent on the refresh for
 * any materialized view that this one is dependent on.
 *
 * This must be called after all the objects are created, but before they are
 * sorted.
 */
static void
buildMatViewRefreshDependencies(Archive *fout)
{
	PQExpBuffer query;
	PGresult   *res;
	int			ntups,
				i;
	int			i_classid,
				i_objid,
				i_refobjid;

	/* No Mat Views before 9.3. */
	if (fout->remoteVersion < 90300)
		return;

	query = createPQExpBuffer();

	ExecuteSqlStatement(fout, "SET gp_recursive_cte TO ON;");

	appendPQExpBufferStr(query, "WITH RECURSIVE w AS "
						 "( "
					"SELECT d1.objid, d2.refobjid, c2.relkind AS refrelkind "
						 "FROM pg_depend d1 "
						 "JOIN pg_class c1 ON c1.oid = d1.objid "
						 "AND c1.relkind = 'm' "
						 "JOIN pg_rewrite r1 ON r1.ev_class = d1.objid "
				  "JOIN pg_depend d2 ON d2.classid = 'pg_rewrite'::regclass "
						 "AND d2.objid = r1.oid "
						 "AND d2.refobjid <> d1.objid "
						 "JOIN pg_class c2 ON c2.oid = d2.refobjid "
						 "AND c2.relkind IN ('m','v') "
						 "WHERE d1.classid = 'pg_class'::regclass "
						 "UNION "
						 "SELECT w.objid, d3.refobjid, c3.relkind "
						 "FROM w "
						 "JOIN pg_rewrite r3 ON r3.ev_class = w.refobjid "
				  "JOIN pg_depend d3 ON d3.classid = 'pg_rewrite'::regclass "
						 "AND d3.objid = r3.oid "
						 "AND d3.refobjid <> w.refobjid "
						 "JOIN pg_class c3 ON c3.oid = d3.refobjid "
						 "AND c3.relkind IN ('m','v') "
						 ") "
			  "SELECT 'pg_class'::regclass::oid AS classid, objid, refobjid "
						 "FROM w "
						 "WHERE refrelkind = 'm'");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_classid = PQfnumber(res, "classid");
	i_objid = PQfnumber(res, "objid");
	i_refobjid = PQfnumber(res, "refobjid");

	for (i = 0; i < ntups; i++)
	{
		CatalogId	objId;
		CatalogId	refobjId;
		DumpableObject *dobj;
		DumpableObject *refdobj;
		TableInfo  *tbinfo;
		TableInfo  *reftbinfo;

		objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
		objId.oid = atooid(PQgetvalue(res, i, i_objid));
		refobjId.tableoid = objId.tableoid;
		refobjId.oid = atooid(PQgetvalue(res, i, i_refobjid));

		dobj = findObjectByCatalogId(objId);
		if (dobj == NULL)
			continue;

		Assert(dobj->objType == DO_TABLE);
		tbinfo = (TableInfo *) dobj;
		Assert(tbinfo->relkind == RELKIND_MATVIEW);
		dobj = (DumpableObject *) tbinfo->dataObj;
		if (dobj == NULL)
			continue;
		Assert(dobj->objType == DO_REFRESH_MATVIEW);

		refdobj = findObjectByCatalogId(refobjId);
		if (refdobj == NULL)
			continue;

		Assert(refdobj->objType == DO_TABLE);
		reftbinfo = (TableInfo *) refdobj;
		Assert(reftbinfo->relkind == RELKIND_MATVIEW);
		refdobj = (DumpableObject *) reftbinfo->dataObj;
		if (refdobj == NULL)
			continue;
		Assert(refdobj->objType == DO_REFRESH_MATVIEW);

		addObjectDependency(dobj, refdobj->dumpId);

		if (!reftbinfo->relispopulated)
			tbinfo->relispopulated = false;
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * getTableDataFKConstraints -
 *	  add dump-order dependencies reflecting foreign key constraints
 *
 * This code is executed only in a data-only dump --- in schema+data dumps
 * we handle foreign key issues by not creating the FK constraints until
 * after the data is loaded.  In a data-only dump, however, we want to
 * order the table data objects in such a way that a table's referenced
 * tables are restored first.  (In the presence of circular references or
 * self-references this may be impossible; we'll detect and complain about
 * that during the dependency sorting step.)
 */
static void
getTableDataFKConstraints(void)
{
	DumpableObject **dobjs;
	int			numObjs;
	int			i;

	/* Search through all the dumpable objects for FK constraints */
	getDumpableObjects(&dobjs, &numObjs);
	for (i = 0; i < numObjs; i++)
	{
		if (dobjs[i]->objType == DO_FK_CONSTRAINT)
		{
			ConstraintInfo *cinfo = (ConstraintInfo *) dobjs[i];
			TableInfo  *ftable;

			/* Not interesting unless both tables are to be dumped */
			if (cinfo->contable == NULL ||
				cinfo->contable->dataObj == NULL)
				continue;
			ftable = findTableByOid(cinfo->confrelid);
			if (ftable == NULL ||
				ftable->dataObj == NULL)
				continue;

			/*
			 * Okay, make referencing table's TABLE_DATA object depend on the
			 * referenced table's TABLE_DATA object.
			 */
			addObjectDependency(&cinfo->contable->dataObj->dobj,
								ftable->dataObj->dobj.dumpId);
		}
	}
	free(dobjs);
}


/*
 * guessConstraintInheritance:
 *	In pre-8.4 databases, we can't tell for certain which constraints
 *	are inherited.  We assume a CHECK constraint is inherited if its name
 *	matches the name of any constraint in the parent.  Originally this code
 *	tried to compare the expression texts, but that can fail for various
 *	reasons --- for example, if the parent and child tables are in different
 *	schemas, reverse-listing of function calls may produce different text
 *	(schema-qualified or not) depending on search path.
 *
 *	In 8.4 and up we can rely on the conislocal field to decide which
 *	constraints must be dumped; much safer.
 *
 *	This function assumes all conislocal flags were initialized to TRUE.
 *	It clears the flag on anything that seems to be inherited.
 */
static void
guessConstraintInheritance(TableInfo *tblinfo, int numTables)
{
	int			i,
				j,
				k;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &(tblinfo[i]);
		int			numParents;
		TableInfo **parents;
		TableInfo  *parent;

		/* Sequences and views never have parents */
		if (tbinfo->relkind == RELKIND_SEQUENCE ||
			tbinfo->relkind == RELKIND_VIEW)
			continue;

		/* Don't bother computing anything for non-target tables, either */
		if (!tbinfo->dobj.dump)
			continue;

		numParents = tbinfo->numParents;
		parents = tbinfo->parents;

		if (numParents == 0)
			continue;			/* nothing to see here, move along */

		/* scan for inherited CHECK constraints */
		for (j = 0; j < tbinfo->ncheck; j++)
		{
			ConstraintInfo *constr;

			constr = &(tbinfo->checkexprs[j]);

			for (k = 0; k < numParents; k++)
			{
				int			l;

				parent = parents[k];
				for (l = 0; l < parent->ncheck; l++)
				{
					ConstraintInfo *pconstr = &(parent->checkexprs[l]);

					if (strcmp(pconstr->dobj.name, constr->dobj.name) == 0)
					{
						constr->conislocal = false;
						break;
					}
				}
				if (!constr->conislocal)
					break;
			}
		}
	}
}

static void
dumpPreassignedOidArchiveEntry(Archive *fout, BinaryUpgradeInfo *binfo)
{
	PQExpBuffer	setoidquery;
	CatalogId	maxoidid;

	setoidquery = createPQExpBuffer();

	appendPQExpBufferStr(setoidquery,
						 "-- Placeholder for binary_upgrade.set_preassigned_oids()\n\n");

	maxoidid.oid = 0;
	maxoidid.tableoid = 0;

	char *tag = pg_strdup("binary_upgrade");

	ArchiveEntry(fout,
				 maxoidid,		/* catalog ID */
				 binfo->dobj.dumpId,		/* dump ID */
				 tag,		/* Name */
				 NULL,			/* Namespace */
				 NULL,			/* Tablespace */
				 "",			/* Owner */
				 false,			/* with oids */
				 tag,	/* Desc */
				 SECTION_PRE_DATA,		/* Section */
				 setoidquery->data, /* Create */
				 "",	/* Del */
				 NULL,			/* Copy */
				 NULL,			/* Deps */
				 0,				/* # Deps */
				 NULL,			/* Dumper */
				 NULL);			/* Dumper Arg */

	destroyPQExpBuffer(setoidquery);
	free(tag);
}

static void
dumpPreassignedOidDefinition(Archive *fout, BinaryUpgradeInfo *binfo)
{
	PQExpBuffer	setoidquery;
	SimpleOidListCell *cell;

	setoidquery = createPQExpBuffer();

	appendPQExpBufferStr(setoidquery,
						 "SELECT binary_upgrade.set_preassigned_oids(ARRAY[");
	for (cell = preassigned_oids.head; cell; cell = cell->next)
	{
		appendPQExpBuffer(setoidquery, "%u%s",
						  cell->val, (cell->next ? "," : ""));
	}
	appendPQExpBufferStr(setoidquery, "]::pg_catalog.oid[]);\n\n");

	AmendArchiveEntry(fout, binfo->dobj.dumpId, setoidquery->data);

	destroyPQExpBuffer(setoidquery);
}

/*
 * dumpDatabase:
 *	dump the database definition
 */
static void
dumpDatabase(Archive *fout)
{
	PQExpBuffer dbQry = createPQExpBuffer();
	PQExpBuffer delQry = createPQExpBuffer();
	PQExpBuffer creaQry = createPQExpBuffer();
	PQExpBuffer labelq = createPQExpBuffer();
	PGconn	   *conn = GetConnection(fout);
	PGresult   *res;
	int			i_tableoid,
				i_oid,
				i_datdba,
				i_encoding,
				i_collate,
				i_ctype,
				i_frozenxid,
				i_minmxid,
				i_tablespace;
	CatalogId	dbCatId;
	DumpId		dbDumpId;
	const char *datname,
			   *dba,
			   *encoding,
			   *collate,
			   *ctype,
			   *tablespace;
	uint32		frozenxid,
				minmxid;
	char	   *qdatname;

	datname = PQdb(conn);
	qdatname = pg_strdup(fmtId(datname));

	if (g_verbose)
		write_msg(NULL, "saving database definition\n");

	/* Get the database owner and parameters from pg_database */
	if (fout->remoteVersion >= 90300)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "datdba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
						  "datcollate, datctype, datfrozenxid, datminmxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
						  "shobj_description(oid, 'pg_database') AS description "

						  "FROM pg_database "
						  "WHERE datname = current_database()");
	}
	else if (fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "datdba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
						  "datcollate, datctype, datfrozenxid, 0 AS datminmxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
					  "shobj_description(oid, 'pg_database') AS description "

						  "FROM pg_database "
						  "WHERE datname = current_database()");
	}
	else
	{
		appendPQExpBuffer(dbQry, "SELECT tableoid, oid, "
						  "datdba, "
						  "pg_encoding_to_char(encoding) AS encoding, "
					   "NULL AS datcollate, NULL AS datctype, datfrozenxid, 0 AS datminmxid, "
						  "(SELECT spcname FROM pg_tablespace t WHERE t.oid = dattablespace) AS tablespace, "
					  "shobj_description(oid, 'pg_database') AS description "

						  "FROM pg_database "
						  "WHERE datname = current_database()");
	}

	res = ExecuteSqlQueryForSingleRow(fout, dbQry->data);

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_datdba = PQfnumber(res, "datdba");
	i_encoding = PQfnumber(res, "encoding");
	i_collate = PQfnumber(res, "datcollate");
	i_ctype = PQfnumber(res, "datctype");
	i_frozenxid = PQfnumber(res, "datfrozenxid");
	i_minmxid = PQfnumber(res, "datminmxid");
	i_tablespace = PQfnumber(res, "tablespace");

	dbCatId.tableoid = atooid(PQgetvalue(res, 0, i_tableoid));
	dbCatId.oid = atooid(PQgetvalue(res, 0, i_oid));
	dba = PQgetvalue(res, 0, i_datdba);
	encoding = PQgetvalue(res, 0, i_encoding);
	collate = PQgetvalue(res, 0, i_collate);
	ctype = PQgetvalue(res, 0, i_ctype);
	frozenxid = atooid(PQgetvalue(res, 0, i_frozenxid));
	minmxid = atooid(PQgetvalue(res, 0, i_minmxid));
	tablespace = PQgetvalue(res, 0, i_tablespace);

	appendPQExpBuffer(creaQry, "CREATE DATABASE %s WITH TEMPLATE = template0",
					  qdatname);
	if (strlen(encoding) > 0)
	{
		appendPQExpBufferStr(creaQry, " ENCODING = ");
		appendStringLiteralAH(creaQry, encoding, fout);
	}
	if (strlen(collate) > 0)
	{
		appendPQExpBufferStr(creaQry, " LC_COLLATE = ");
		appendStringLiteralAH(creaQry, collate, fout);
	}
	if (strlen(ctype) > 0)
	{
		appendPQExpBufferStr(creaQry, " LC_CTYPE = ");
		appendStringLiteralAH(creaQry, ctype, fout);
	}
	if (strlen(tablespace) > 0 && strcmp(tablespace, "pg_default") != 0)
		appendPQExpBuffer(creaQry, " TABLESPACE = %s",
						  fmtId(tablespace));
	appendPQExpBufferStr(creaQry, ";\n");

	if (binary_upgrade)
	{
		appendPQExpBufferStr(creaQry, "\n-- For binary upgrade, set datfrozenxid and datminmxid.\n");
		appendPQExpBufferStr(creaQry, "SET allow_system_table_mods = true;\n");
		appendPQExpBuffer(creaQry, "UPDATE pg_catalog.pg_database\n"
						  "SET datfrozenxid = '%u', datminmxid = '%u'\n"
						  "WHERE	datname = ",
						  frozenxid, minmxid);
		appendStringLiteralAH(creaQry, datname, fout);
		appendPQExpBufferStr(creaQry, ";\n");

		appendPQExpBuffer(creaQry, "RESET allow_system_table_mods;\n");
	}

	appendPQExpBuffer(delQry, "DROP DATABASE %s;\n",
					  qdatname);

	dbDumpId = createDumpId();

	ArchiveEntry(fout,
				 dbCatId,		/* catalog ID */
				 dbDumpId,		/* dump ID */
				 datname,		/* Name */
				 NULL,			/* Namespace */
				 NULL,			/* Tablespace */
				 dba,			/* Owner */
				 false,			/* with oids */
				 "DATABASE",	/* Desc */
				 SECTION_PRE_DATA,		/* Section */
				 creaQry->data, /* Create */
				 delQry->data,	/* Del */
				 NULL,			/* Copy */
				 NULL,			/* Deps */
				 0,				/* # Deps */
				 NULL,			/* Dumper */
				 NULL);			/* Dumper Arg */

	/*
	 * pg_largeobject and pg_largeobject_metadata come from the old system
	 * intact, so set their relfrozenxids and relminmxids.
	 */
	if (binary_upgrade)
	{
		PGresult   *lo_res;
		PQExpBuffer loFrozenQry = createPQExpBuffer();
		PQExpBuffer loOutQry = createPQExpBuffer();
		int			i_relfrozenxid, i_relminmxid;

		/*
		 * pg_largeobject
		 */
		if (fout->remoteVersion >= 90300)
			appendPQExpBuffer(loFrozenQry, "SELECT relfrozenxid, relminmxid\n"
							  "FROM pg_catalog.pg_class\n"
							  "WHERE oid = %u;\n",
							  LargeObjectRelationId);
		else
			appendPQExpBuffer(loFrozenQry, "SELECT relfrozenxid, 0 AS relminmxid\n"
							  "FROM pg_catalog.pg_class\n"
							  "WHERE oid = %u;\n",
							  LargeObjectRelationId);

		lo_res = ExecuteSqlQueryForSingleRow(fout, loFrozenQry->data);

		i_relfrozenxid = PQfnumber(lo_res, "relfrozenxid");
		i_relminmxid = PQfnumber(lo_res, "relminmxid");

		appendPQExpBufferStr(loOutQry, "\n-- For binary upgrade, set pg_largeobject relfrozenxid and relminmxid\n");
		appendPQExpBufferStr(loOutQry, "SET allow_system_table_mods = true;\n");
		appendPQExpBuffer(loOutQry, "UPDATE pg_catalog.pg_class\n"
						  "SET relfrozenxid = '%u', relminmxid = '%u'\n"
						  "WHERE oid = %u;\n",
						  atoi(PQgetvalue(lo_res, 0, i_relfrozenxid)),
						  atoi(PQgetvalue(lo_res, 0, i_relminmxid)),
						  LargeObjectRelationId);
		appendPQExpBuffer(loOutQry, "RESET allow_system_table_mods;\n");
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 "pg_largeobject", NULL, NULL, "",
					 false, "pg_largeobject", SECTION_PRE_DATA,
					 loOutQry->data, "", NULL,
					 NULL, 0,
					 NULL, NULL);

		PQclear(lo_res);

		/*
		 * pg_largeobject_metadata
		 */
		if (fout->remoteVersion >= 90000)
		{
			resetPQExpBuffer(loFrozenQry);
			resetPQExpBuffer(loOutQry);

			if (fout->remoteVersion >= 90300)
				appendPQExpBuffer(loFrozenQry, "SELECT relfrozenxid, relminmxid\n"
								  "FROM pg_catalog.pg_class\n"
								  "WHERE oid = %u;\n",
								  LargeObjectMetadataRelationId);
			else
				appendPQExpBuffer(loFrozenQry, "SELECT relfrozenxid, 0 AS relminmxid\n"
								  "FROM pg_catalog.pg_class\n"
								  "WHERE oid = %u;\n",
								  LargeObjectMetadataRelationId);

			lo_res = ExecuteSqlQueryForSingleRow(fout, loFrozenQry->data);

			i_relfrozenxid = PQfnumber(lo_res, "relfrozenxid");
			i_relminmxid = PQfnumber(lo_res, "relminmxid");

			appendPQExpBufferStr(loOutQry, "\n-- For binary upgrade, set pg_largeobject_metadata relfrozenxid and relminmxid\n");

			appendPQExpBufferStr(loOutQry, "SET allow_system_table_mods = true;\n");
			appendPQExpBuffer(loOutQry, "UPDATE pg_catalog.pg_class\n"
							  "SET relfrozenxid = '%u', relminmxid = '%u'\n"
							  "WHERE oid = %u;\n",
							  atoi(PQgetvalue(lo_res, 0, i_relfrozenxid)),
							  atoi(PQgetvalue(lo_res, 0, i_relminmxid)),
							  LargeObjectMetadataRelationId);
			appendPQExpBuffer(loOutQry, "RESET allow_system_table_mods;\n");
			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 "pg_largeobject_metadata", NULL, NULL, "",
						 false, "pg_largeobject_metadata", SECTION_PRE_DATA,
						 loOutQry->data, "", NULL,
						 NULL, 0,
						 NULL, NULL);

			PQclear(lo_res);
		}

		destroyPQExpBuffer(loFrozenQry);
		destroyPQExpBuffer(loOutQry);
	}

	/* Compute correct tag for archive entry */
	appendPQExpBuffer(labelq, "DATABASE %s", qdatname);

	/* Dump DB comment if any */
	char	   *comment = PQgetvalue(res, 0, PQfnumber(res, "description"));

	if (comment && *comment)
	{
		resetPQExpBuffer(dbQry);

		/*
			* Generates warning when loaded into a differently-named
			* database.
			*/
		appendPQExpBuffer(dbQry, "COMMENT ON DATABASE %s IS ", qdatname);
		appendStringLiteralAH(dbQry, comment, fout);
		appendPQExpBufferStr(dbQry, ";\n");

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
						labelq->data, NULL, NULL, dba,
						false, "COMMENT", SECTION_NONE,
						dbQry->data, "", NULL,
						&(dbDumpId), 1,
						NULL, NULL);
	}	

	/* Dump shared security label. */
	if (!no_security_labels && fout->remoteVersion >= 90200)
	{
		PGresult   *shres;
		PQExpBuffer seclabelQry;

		seclabelQry = createPQExpBuffer();

		buildShSecLabelQuery(conn, "pg_database", dbCatId.oid, seclabelQry);
		shres = ExecuteSqlQuery(fout, seclabelQry->data, PGRES_TUPLES_OK);
		resetPQExpBuffer(seclabelQry);
		emitShSecLabels(conn, shres, seclabelQry, "DATABASE", datname);
		if (seclabelQry->len > 0)
			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 labelq->data, NULL, NULL, dba,
						 false, "SECURITY LABEL", SECTION_NONE,
						 seclabelQry->data, "", NULL,
						 &(dbDumpId), 1,
						 NULL, NULL);
		destroyPQExpBuffer(seclabelQry);
		PQclear(shres);
	}

	PQclear(res);

	free(qdatname);
	destroyPQExpBuffer(dbQry);
	destroyPQExpBuffer(delQry);
	destroyPQExpBuffer(creaQry);
	destroyPQExpBuffer(labelq);
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
		write_msg(NULL, "saving encoding = %s\n", encname);

	appendPQExpBufferStr(qry, "SET client_encoding = ");
	appendStringLiteralAH(qry, encname, AH);
	appendPQExpBufferStr(qry, ";\n");

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "ENCODING", NULL, NULL, "",
				 false, "ENCODING", SECTION_PRE_DATA,
				 qry->data, "", NULL,
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
		write_msg(NULL, "saving standard_conforming_strings = %s\n",
				  stdstrings);

	appendPQExpBuffer(qry, "SET standard_conforming_strings = '%s';\n",
					  stdstrings);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "STDSTRINGS", NULL, NULL, "",
				 false, "STDSTRINGS", SECTION_PRE_DATA,
				 qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(qry);
}

/*
 * dumpSearchPath: record the active search_path in the archive
 */
static void
dumpSearchPath(Archive *AH)
{
	PQExpBuffer qry = createPQExpBuffer();
	PQExpBuffer path = createPQExpBuffer();
	PGresult   *res;
	char	  **schemanames = NULL;
	int			nschemanames = 0;
	int			i;

	/*
		* We use the result of current_schemas(), not the search_path GUC,
		* because that might contain wildcards such as "$user", which won't
		* necessarily have the same value during restore.  Also, this way
		* avoids listing schemas that may appear in search_path but not
		* actually exist, which seems like a prudent exclusion.
		*/
	res = ExecuteSqlQueryForSingleRow(AH,
								"SELECT pg_catalog.current_schemas(false)");

	if (!parsePGArray(PQgetvalue(res, 0, 0), &schemanames, &nschemanames))
		exit_horribly(NULL, "could not parse result of current_schemas()\n");

	/*
		* We use set_config(), not a simple "SET search_path" command,
		* because the latter has less-clean behavior if the search path is
		* empty.  While that's likely to get fixed at some point, it seems
		* like a good idea to be as backwards-compatible as possible in what
		* we put into archives.
		*/
	for (i = 0; i < nschemanames; i++)
	{
		if (i > 0)
			appendPQExpBufferStr(path, ", ");
		appendPQExpBufferStr(path, fmtId(schemanames[i]));
	}

	PQclear(res);

	appendPQExpBufferStr(qry, "SELECT pg_catalog.set_config('search_path', ");
	appendStringLiteralAH(qry, path->data, AH);
	appendPQExpBufferStr(qry, ", false);\n");

	if (g_verbose)
		write_msg(NULL, "saving search_path = %s\n", path->data);

	ArchiveEntry(AH, nilCatalogId, createDumpId(),
				 "SEARCHPATH", NULL, NULL, "",
				 false, "SEARCHPATH", SECTION_PRE_DATA,
				 qry->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Also save it in AH->searchpath, in case we're doing plain text dump */
	AH->searchpath = pg_strdup(qry->data);

	if (schemanames)
		free(schemanames);
	destroyPQExpBuffer(qry);
	destroyPQExpBuffer(path);
}


/*
 * getBlobs:
 *	Collect schema-level data about large objects
 */
static void
getBlobs(Archive *fout)
{
	PQExpBuffer blobQry = createPQExpBuffer();
	BlobInfo   *binfo;
	DumpableObject *bdata;
	PGresult   *res;
	int			ntups;
	int			i;
	int			i_oid;
	int			i_lomowner;
	int			i_lomacl;

	/* Verbose message */
	if (g_verbose)
		write_msg(NULL, "reading large objects\n");

	/* Fetch BLOB OIDs, and owner/ACL data if >= 9.0 */
	if (fout->remoteVersion >= 90000)
		appendPQExpBuffer(blobQry,
						  "SELECT oid, lomowner, lomacl"
						  " FROM pg_largeobject_metadata");
	else
		appendPQExpBufferStr(blobQry,
							 "SELECT DISTINCT loid AS oid, "
							 "NULL::name AS rolname, NULL::oid AS lomacl "
							 "FROM pg_largeobject");

	res = ExecuteSqlQuery(fout, blobQry->data, PGRES_TUPLES_OK);

	i_oid = PQfnumber(res, "oid");
	i_lomowner = PQfnumber(res, "lomowner");
	i_lomacl = PQfnumber(res, "lomacl");

	ntups = PQntuples(res);
	if (ntups > 0)
	{
		/*
		 * Each large object has its own BLOB archive entry.
		 */
		binfo = (BlobInfo *) pg_malloc(ntups * sizeof(BlobInfo));

		for (i = 0; i < ntups; i++)
		{
			binfo[i].dobj.objType = DO_BLOB;
			binfo[i].dobj.catId.tableoid = LargeObjectRelationId;
			binfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
			AssignDumpId(&binfo[i].dobj);
			binfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_oid));
			binfo[i].rolname = getRoleName(PQgetvalue(res, i, i_lomowner));
		}

		/*
		 * If we have any large objects, a "BLOBS" archive entry is needed.
		 * This is just a placeholder for sorting; it carries no data now.
		 */
		bdata = (DumpableObject *) pg_malloc(sizeof(DumpableObject));
		bdata->objType = DO_BLOB_DATA;
		bdata->catId = nilCatalogId;
		AssignDumpId(bdata);
		bdata->name = pg_strdup("BLOBS");
	}

	PQclear(res);
	destroyPQExpBuffer(blobQry);
}

/*
 * dumpBlob
 *
 * dump the definition (metadata) of the given large object
 */
static void
dumpBlob(Archive *fout, BlobInfo *binfo)
{
	PQExpBuffer cquery = createPQExpBuffer();
	PQExpBuffer dquery = createPQExpBuffer();

	appendPQExpBuffer(cquery,
					  "SELECT pg_catalog.lo_create('%s');\n",
					  binfo->dobj.name);

	appendPQExpBuffer(dquery,
					  "SELECT pg_catalog.lo_unlink('%s');\n",
					  binfo->dobj.name);

	ArchiveEntry(fout, binfo->dobj.catId, binfo->dobj.dumpId,
				 binfo->dobj.name,
				 NULL, NULL,
				 binfo->rolname, false,
				 "BLOB", SECTION_PRE_DATA,
				 cquery->data, dquery->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump comment if any */
	dumpComment(fout, "LARGE OBJECT", binfo->dobj.name,
				NULL, binfo->rolname,
				binfo->dobj.catId, 0, binfo->dobj.dumpId);

	/* Dump security label if any */
	dumpSecLabel(fout, "LARGE OBJECT", binfo->dobj.name,
				 NULL, binfo->rolname,
				 binfo->dobj.catId, 0, binfo->dobj.dumpId);

	/*
	 * Dump ACL if any
	 *
	 * Do not dump the ACL in binary-upgrade mode, however, as the ACL will be
	 * copied over by pg_upgrade as it is part of the pg_largeobject_metadata
	 * table.
	 */
	if (binfo->blobacl && !binary_upgrade)
		dumpACL(fout, binfo->dobj.catId, binfo->dobj.dumpId, "LARGE OBJECT",
				binfo->dobj.name, NULL,
				NULL, binfo->rolname, binfo->blobacl);

	destroyPQExpBuffer(cquery);
	destroyPQExpBuffer(dquery);
}

/*
 * dumpBlobs:
 *	dump the data contents of all large objects
 */
static int
dumpBlobs(Archive *fout, void *arg __attribute__((unused)))
{
	const char *blobQry;
	const char *blobFetchQry;
	PGconn	   *conn = GetConnection(fout);
	PGresult   *res;
	char		buf[LOBBUFSIZE];
	int			ntups;
	int			i;
	int			cnt;

	/*
	 * Do not dump out blob data in binary-upgrade mode, pg_upgrade will copy
	 * the pg_largeobject table over entirely from the old cluster.
	 */
	if (binary_upgrade)
		return 1;

	if (g_verbose)
		write_msg(NULL, "saving large objects\n");

	/*
	 * Currently, we re-fetch all BLOB OIDs using a cursor.  Consider scanning
	 * the already-in-memory dumpable objects instead...
	 */
	if (fout->remoteVersion >= 90000)
		blobQry = "DECLARE bloboid CURSOR FOR SELECT oid FROM pg_largeobject_metadata";
	else
		blobQry = "DECLARE bloboid CURSOR FOR SELECT DISTINCT loid FROM pg_largeobject";

	ExecuteSqlStatement(fout, blobQry);

	/* Command to fetch from cursor */
	blobFetchQry = "FETCH 1000 IN bloboid";

	do
	{
		/* Do a fetch */
		res = ExecuteSqlQuery(fout, blobFetchQry, PGRES_TUPLES_OK);

		/* Process the tuples, if any */
		ntups = PQntuples(res);
		for (i = 0; i < ntups; i++)
		{
			Oid			blobOid;
			int			loFd;

			blobOid = atooid(PQgetvalue(res, i, 0));
			/* Open the BLOB */
			loFd = lo_open(conn, blobOid, INV_READ);
			if (loFd == -1)
				exit_horribly(NULL, "could not open large object %u: %s",
							  blobOid, PQerrorMessage(conn));

			StartBlob(fout, blobOid);

			/* Now read it in chunks, sending data to archive */
			do
			{
				cnt = lo_read(conn, loFd, buf, LOBBUFSIZE);
				if (cnt < 0)
					exit_horribly(NULL, "error reading large object %u: %s",
								  blobOid, PQerrorMessage(conn));

				WriteData(fout, buf, cnt);
			} while (cnt > 0);

			lo_close(conn, loFd);

			EndBlob(fout, blobOid);
		}

		PQclear(res);
	} while (ntups > 0);

	return 1;
}

static void
binary_upgrade_set_namespace_oid(Archive *fout, PQExpBuffer upgrade_buffer,
								 Oid pg_namespace_oid)
{
	PQExpBuffer	upgrade_query = createPQExpBuffer();
	PGresult   *upgrade_res;
	char	   *pg_nspname;

	appendPQExpBuffer(upgrade_query,
					  "SELECT nspname "
					  "FROM pg_catalog.pg_namespace "
					  "WHERE oid = '%u'::pg_catalog.oid;", pg_namespace_oid);
	upgrade_res = ExecuteSqlQueryForSingleRow(fout, upgrade_query->data);
	pg_nspname = PQgetvalue(upgrade_res, 0, PQfnumber(upgrade_res, "nspname"));

	simple_oid_list_append(&preassigned_oids, pg_namespace_oid);
	appendPQExpBuffer(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_namespace oid\n");
	appendPQExpBuffer(upgrade_buffer,
	 "SELECT binary_upgrade.set_next_pg_namespace_oid('%u'::pg_catalog.oid, "
													 "$$%s$$::text);\n\n",
					  pg_namespace_oid, pg_nspname);
	PQclear(upgrade_res);
	destroyPQExpBuffer(upgrade_query);
}

static void
binary_upgrade_set_type_oids_by_type_oid(Archive *fout,
										 PQExpBuffer upgrade_buffer,
										 const TypeInfo *tyinfo)
{
	PQExpBuffer upgrade_query = createPQExpBuffer();
	Oid			pg_type_array_oid = tyinfo->typarrayoid;

	simple_oid_list_append(&preassigned_oids, tyinfo->dobj.catId.oid);
	appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, $$%s$$::text);\n\n",
						tyinfo->dobj.catId.oid, tyinfo->dobj.namespace->dobj.catId.oid, tyinfo->dobj.name);

	if (OidIsValid(pg_type_array_oid))
	{
		simple_oid_list_append(&preassigned_oids, pg_type_array_oid);
		appendPQExpBufferStr(upgrade_buffer,
							 "\n-- For binary upgrade, must preserve pg_type array oid\n");
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_array_pg_type_oid('%u'::pg_catalog.oid, "
						  "'%u'::pg_catalog.oid, $$%s$$::text);\n\n",
						  pg_type_array_oid, tyinfo->typarrayns,
						  tyinfo->typarrayname);
	}

	destroyPQExpBuffer(upgrade_query);
}

static void
binary_upgrade_set_type_oids_by_rel(Archive *fout,
										PQExpBuffer upgrade_buffer,
										const TableInfo *tblinfo)
{
	TypeInfo *typinfo = findTypeByOid(tblinfo->reltype);
	binary_upgrade_set_type_oids_by_type_oid(fout, upgrade_buffer,
											 typinfo);
}

static void
binary_upgrade_set_pg_class_oids(Archive *fout,
								 PQExpBuffer upgrade_buffer, Oid pg_class_oid,
								 bool is_index)
{
	if (!is_index)
	{
		TableInfo *tblinfo = findTableByOid(pg_class_oid);
		Assert(tblinfo != NULL);
		simple_oid_list_append(&preassigned_oids, pg_class_oid);
		appendPQExpBufferStr(upgrade_buffer,
						"\n-- For binary upgrade, must preserve pg_class oids\n");
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_heap_pg_class_oid('%u'::pg_catalog.oid, "
						  "'%u'::pg_catalog.oid, $$%s$$::text);\n",
						  tblinfo->dobj.catId.oid, tblinfo->dobj.namespace->dobj.catId.oid, tblinfo->dobj.name);

		/* Only tables have toast tables, not indexes */
		if (OidIsValid(tblinfo->toast_oid))
			binary_upgrade_set_toast_oids_by_rel(fout, upgrade_buffer, tblinfo);

		/* Set up any AO auxiliary tables with preallocated OIDs as well. */
		if (tblinfo->aotbl)
			binary_upgrade_set_rel_ao_oids(fout, upgrade_buffer, tblinfo);
	}
	else
	{
		IndxInfo *idxinfo = findIndexByOid(pg_class_oid);
		Assert(idxinfo != NULL);
		simple_oid_list_append(&preassigned_oids, pg_class_oid);
		appendPQExpBuffer(upgrade_buffer,
						  "SELECT binary_upgrade.set_next_index_pg_class_oid('%u'::pg_catalog.oid, "
							"'%u'::pg_catalog.oid, $$%s$$::text);\n",
						  idxinfo->dobj.catId.oid, idxinfo->dobj.namespace->dobj.catId.oid, idxinfo->dobj.name);

		/* Set up bitmap index auxiliary tables */
		if (idxinfo->bmidx && OidIsValid(idxinfo->bmidx->bmrelid))
			binary_upgrade_set_bitmap_index_oids(fout, upgrade_buffer, idxinfo);
	}
	appendPQExpBufferChar(upgrade_buffer, '\n');
}

/*
 * Adjust the names of all pg_bitmapindex objects for the given index
 * to match what they will be in the new cluster, using the OID of
 * the owning index. In many cases these will be the same as the
 * ones in the old cluster, but not always. Some operations can
 * cause the old pg_bitmapindex object names not to match its owner's
 * OID, but the new cluster will be using the correct name, and it's
 * the new cluster's name that we have to use
 */
static void
binary_upgrade_set_bitmap_index_oids(Archive *fout, PQExpBuffer upgrade_buffer, const IndxInfo *idxinfo)
{
	/* pg_bm heap table */
	simple_oid_list_append(&preassigned_oids, idxinfo->bmidx->bmrelid);
	appendPQExpBufferStr(upgrade_buffer,
					"\n-- For binary upgrade, must preserve pg_class oids\n");
	appendPQExpBuffer(upgrade_buffer,
					"SELECT binary_upgrade.set_next_heap_pg_class_oid('%u'::pg_catalog.oid, "
					"'%u'::pg_catalog.oid, 'pg_bm_%u'::text);\n",
					idxinfo->bmidx->bmrelid, PG_BITMAPINDEX_NAMESPACE, idxinfo->dobj.catId.oid);

	/* pg_bm composite type */
	simple_oid_list_append(&preassigned_oids, idxinfo->bmidx->bmreltype);
	appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
	 					"SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_bm_%u'::text);\n\n",
					  idxinfo->bmidx->bmreltype, PG_BITMAPINDEX_NAMESPACE, idxinfo->dobj.catId.oid);

	/* pg_bm index */
	simple_oid_list_append(&preassigned_oids, idxinfo->bmidx->bmidxid);
	appendPQExpBufferStr(upgrade_buffer,
					"\n-- For binary upgrade, must preserve pg_class oids\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_index_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_bm_%u_index'::text);\n",
						idxinfo->bmidx->bmidxid, PG_BITMAPINDEX_NAMESPACE, idxinfo->dobj.catId.oid);
}

/*
 * Adjust the names of all pg_aoseg objects for the given table
 * to match what they will be in the new cluster, using the OID of
 * the owning table. In many cases these will be the same as the
 * ones in the old cluster, but not always. Some operations can
 * cause the old pg_aoseg object names not to match its owner's OID, but
 * the new cluster will be using the correct name, and it's the new
 * cluster's name that we have to use
 */
static void
binary_upgrade_set_rel_ao_oids(Archive *fout, PQExpBuffer upgrade_buffer, const TableInfo *tblinfo)
{
	const char *aoseg_prefix = tblinfo->aotbl->columnstore ? "pg_aocsseg" : "pg_aoseg";

	/* pg_aoseg heap table */
	simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->segrelid);
	appendPQExpBufferStr(upgrade_buffer,
					"\n-- For binary upgrade, must preserve pg_class oids\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_heap_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, '%s_%u'::text);\n",
						tblinfo->aotbl->segrelid, PG_AOSEGMENT_NAMESPACE, aoseg_prefix, tblinfo->dobj.catId.oid);

	/* pg_aoseg composite type */
	simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->segreltype);
	appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
	 					"SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, '%s_%u'::text);\n",
					  tblinfo->aotbl->segreltype, PG_AOSEGMENT_NAMESPACE, aoseg_prefix, tblinfo->dobj.catId.oid);

	/* blkdir is optional. */
	if (OidIsValid(tblinfo->aotbl->blkdirrelid))
	{
		/* pg_aoblkdir heap table */
		simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->blkdirrelid);
		appendPQExpBufferStr(upgrade_buffer,
			"\n-- For binary upgrade, must preserve pg_class oids\n");
		appendPQExpBuffer(upgrade_buffer,
							"SELECT binary_upgrade.set_next_heap_pg_class_oid('%u'::pg_catalog.oid, "
							"'%u'::pg_catalog.oid, 'pg_aoblkdir_%d'::text);\n",
							tblinfo->aotbl->blkdirrelid, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);

		/* pg_aoblkdir composite type */
		simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->blkdirreltype);
		appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
		appendPQExpBuffer(upgrade_buffer,
							"SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid, "
							"'%u'::pg_catalog.oid, 'pg_aoblkdir_%d'::text);\n",
							tblinfo->aotbl->blkdirreltype, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);

		/* pg_aoblkdir index */
		simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->blkdiridxid);
		appendPQExpBufferStr(upgrade_buffer,
						"\n-- For binary upgrade, must preserve pg_class oids\n");
		appendPQExpBuffer(upgrade_buffer,
							"SELECT binary_upgrade.set_next_index_pg_class_oid('%u'::pg_catalog.oid, "
							"'%u'::pg_catalog.oid, 'pg_aoblkdir_%u_index'::text);\n",
							tblinfo->aotbl->blkdiridxid, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);
	}

	/* pg_aovisimap heap table */
	simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->visimaprelid);
	appendPQExpBufferStr(upgrade_buffer,
					"\n-- For binary upgrade, must preserve pg_class oids\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_heap_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_aovisimap_%u'::text);\n",
						tblinfo->aotbl->visimaprelid, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);

	/* pg_aovisimap composite type */
	simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->visimapreltype);
	appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
	 					"SELECT binary_upgrade.set_next_pg_type_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_aovisimap_%u'::text);\n\n",
					  tblinfo->aotbl->visimapreltype, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);

	/* pg_aovisimap index */
	simple_oid_list_append(&preassigned_oids, tblinfo->aotbl->visimapidxid);
	appendPQExpBufferStr(upgrade_buffer,
					"\n-- For binary upgrade, must preserve pg_class oids\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_index_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_aovisimap_%u_index'::text);\n",
						tblinfo->aotbl->visimapidxid, PG_AOSEGMENT_NAMESPACE, tblinfo->dobj.catId.oid);
}

/*
 * Adjust the names of pg_toast tables for the given table
 * to match what they will be in the new cluster, using the OID of
 * the owning table. In many cases these will be the same as the
 * ones in the old cluster, but not always. Some operations can
 * cause the old TOAST table name not to match its owner's OID, but
 * the new cluster will be using the correct name, and it's the new
 * cluster's name that we have to use
 */
static void
binary_upgrade_set_toast_oids_by_rel(Archive *fout, PQExpBuffer upgrade_buffer, const TableInfo *tblinfo)
{

 /*
	* One complexity is that the table definition might not require
	* the creation of a TOAST table, and the TOAST table might have
	* been created long after table creation, when the table was
	* loaded with wide data.  By setting the TOAST oid we force
	* creation of the TOAST heap and TOAST index by the backend so we
	* can cleanly copy the files during binary upgrade.
	*/

	/* pg_toast table */
	simple_oid_list_append(&preassigned_oids, tblinfo->toast_oid);
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_toast_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_toast_%u'::text);\n",
						tblinfo->toast_oid, PG_TOAST_NAMESPACE, tblinfo->dobj.catId.oid);

	/* pg_toast composite type */
	simple_oid_list_append(&preassigned_oids, tblinfo->toast_type);
	appendPQExpBufferStr(upgrade_buffer, "\n-- For binary upgrade, must preserve pg_type oid\n");
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_toast_pg_type_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_toast_%u'::text);\n\n",
					  tblinfo->toast_type, PG_TOAST_NAMESPACE, tblinfo->dobj.catId.oid);

	/* every toast table has an index */
	simple_oid_list_append(&preassigned_oids, tblinfo->toast_index);
	appendPQExpBuffer(upgrade_buffer,
						"SELECT binary_upgrade.set_next_index_pg_class_oid('%u'::pg_catalog.oid, "
						"'%u'::pg_catalog.oid, 'pg_toast_%u_index'::text);\n",
						tblinfo->toast_index, PG_TOAST_NAMESPACE, tblinfo->dobj.catId.oid);
}

/*
 * If the DumpableObject is a member of an extension, add a suitable
 * ALTER EXTENSION ADD command to the creation commands in upgrade_buffer.
 *
 * For somewhat historical reasons, objname should already be quoted,
 * but not objnamespace (if any).
 */
static void
binary_upgrade_extension_member(PQExpBuffer upgrade_buffer,
								DumpableObject *dobj,
								const char *objtype,
								const char *objname,
								const char *objnamespace)
{
	DumpableObject *extobj = NULL;
	int			i;

	if (!dobj->ext_member)
		return;

	/*
	 * Find the parent extension.  We could avoid this search if we wanted to
	 * add a link field to DumpableObject, but the space costs of that would
	 * be considerable.  We assume that member objects could only have a
	 * direct dependency on their own extension, not any others.
	 */
	for (i = 0; i < dobj->nDeps; i++)
	{
		extobj = findObjectByDumpId(dobj->dependencies[i]);
		if (extobj && extobj->objType == DO_EXTENSION)
			break;
		extobj = NULL;
	}
	if (extobj == NULL)
		exit_horribly(NULL, "could not find parent extension for %s %s\n",
					  objtype, objname);

	appendPQExpBufferStr(upgrade_buffer,
	  "\n-- For binary upgrade, handle extension membership the hard way\n");
	appendPQExpBuffer(upgrade_buffer, "ALTER EXTENSION %s ADD %s ",
					  fmtId(extobj->name),
					  objtype);
	if (objnamespace && *objnamespace)
		appendPQExpBuffer(upgrade_buffer, "%s.", fmtId(objnamespace));
	appendPQExpBuffer(upgrade_buffer, "%s;\n", objname);
}

/*
 * getNamespaces:
 *	  read all namespaces in the system catalogs and return them in the
 * NamespaceInfo* structure
 *
 *	numNamespaces is set to the number of namespaces read in
 */
NamespaceInfo *
getNamespaces(Archive *fout, int *numNamespaces)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	NamespaceInfo *nsinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_nspname;
	int			i_nspowner;
	int			i_nspacl;

	query = createPQExpBuffer();

	/*
	 * we fetch all namespaces including system ones, so that every object we
	 * read in can be linked to a containing namespace.
	 */
	appendPQExpBuffer(query, "SELECT tableoid, oid, nspname, "
					  "nspowner, "
					  "nspacl FROM pg_namespace");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	nsinfo = (NamespaceInfo *) pg_malloc(ntups * sizeof(NamespaceInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_nspname = PQfnumber(res, "nspname");
	i_nspowner = PQfnumber(res, "nspowner");
	i_nspacl = PQfnumber(res, "nspacl");

	for (i = 0; i < ntups; i++)
	{
		const char *nspowner;
	
		nsinfo[i].dobj.objType = DO_NAMESPACE;
		nsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		nsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&nsinfo[i].dobj);
		nsinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_nspname));
		nspowner = PQgetvalue(res, i, i_nspowner);
		nsinfo[i].nspowner = atooid(nspowner);
		nsinfo[i].rolname = getRoleName(nspowner);
		nsinfo[i].nspacl = pg_strdup(PQgetvalue(res, i, i_nspacl));

		/* Decide whether to dump this namespace */
		selectDumpableNamespace(&nsinfo[i]);
	}

	PQclear(res);
	destroyPQExpBuffer(query);

	*numNamespaces = ntups;

	return nsinfo;
}

/*
 * findNamespace:
 *		given a namespace OID and an object OID, look up the info read by
 *		getNamespaces
 *
 * NB: for pre-7.3 source database, we use object OID to guess whether it's
 * a system object or not.  In 7.3 and later there is no guessing, and we
 * don't use objoid at all.
 */
static NamespaceInfo *
findNamespace(Archive *fout, Oid nsoid, Oid objoid)
{
	NamespaceInfo *nsinfo;

	nsinfo = findNamespaceByOid(nsoid);

	if (nsinfo == NULL)
	{
		write_msg(NULL, "schema with OID %u does not exist\n", nsoid);
		exit_nicely(1);
	}

	return nsinfo;
}

/*
 * getExtensions:
 *	  read all extensions in the system catalogs and return them in the
 * ExtensionInfo* structure
 *
 *	numExtensions is set to the number of extensions read in
 */
ExtensionInfo *
getExtensions(Archive *fout, int *numExtensions)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	ExtensionInfo *extinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_extname;
	int			i_nspname;
	int			i_extrelocatable;
	int			i_extversion;
	int			i_extconfig;
	int			i_extcondition;

	query = createPQExpBuffer();

	appendPQExpBufferStr(query, "SELECT x.tableoid, x.oid, "
						 "x.extname, n.nspname, x.extrelocatable, x.extversion, x.extconfig, x.extcondition "
						 "FROM pg_extension x "
						 "JOIN pg_namespace n ON n.oid = x.extnamespace");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	extinfo = (ExtensionInfo *) pg_malloc(ntups * sizeof(ExtensionInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_extname = PQfnumber(res, "extname");
	i_nspname = PQfnumber(res, "nspname");
	i_extrelocatable = PQfnumber(res, "extrelocatable");
	i_extversion = PQfnumber(res, "extversion");
	i_extconfig = PQfnumber(res, "extconfig");
	i_extcondition = PQfnumber(res, "extcondition");

	for (i = 0; i < ntups; i++)
	{
		extinfo[i].dobj.objType = DO_EXTENSION;
		extinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		extinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&extinfo[i].dobj);
		extinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_extname));
		extinfo[i].namespace = pg_strdup(PQgetvalue(res, i, i_nspname));
		extinfo[i].relocatable = *(PQgetvalue(res, i, i_extrelocatable)) == 't';
		extinfo[i].extversion = pg_strdup(PQgetvalue(res, i, i_extversion));
		extinfo[i].extconfig = pg_strdup(PQgetvalue(res, i, i_extconfig));
		extinfo[i].extcondition = pg_strdup(PQgetvalue(res, i, i_extcondition));

		/* Decide whether we want to dump it */
		selectDumpableExtension(&(extinfo[i]));
	}

	PQclear(res);
	destroyPQExpBuffer(query);

	*numExtensions = ntups;

	return extinfo;
}

BinaryUpgradeInfo *
newBinaryUpgradeInfo(void)
{
	BinaryUpgradeInfo	*binfo;

	binfo = (BinaryUpgradeInfo *) pg_malloc0(sizeof(BinaryUpgradeInfo));

	binfo->dobj.objType = DO_BINARY_UPGRADE;
	AssignDumpId(&binfo->dobj);
	binfo->dobj.name = pg_strdup("__binary_upgrade");
	binary_upgrade_dumpid = binfo->dobj.dumpId;

	return binfo;
}

/*
 * getTypes:
 *	  read all types in the system catalogs and return them in the
 * TypeInfo* structure
 *
 *	numTypes is set to the number of types read in
 *
 * NB: this must run after getFuncs() because we assume we can do
 * findFuncByOid().
 */
TypeInfo *
getTypes(Archive *fout, int *numTypes)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TypeInfo   *tyinfo;
	ShellTypeInfo *stinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_typname;
	int			i_typnamespace;
	int			i_typacl;
	int			i_typowner;
	int			i_typinput;
	int			i_typoutput;
	int			i_typelem;
	int			i_typrelid;
	int			i_typrelkind;
	int			i_typtype;
	int			i_typisdefined;
	int			i_isarray;
	int			i_typstorage;
	int			i_typarrayoid;
	int			i_typarrayname;
	int			i_typarrayns;

	/*
	 * we include even the built-in types because those may be used as array
	 * elements by user-defined types
	 *
	 * we filter out the built-in types when we dump out the types
	 *
	 * same approach for undefined (shell) types and array types
	 *
	 * Note: as of 8.3 we can reliably detect whether a type is an
	 * auto-generated array type by checking the element type's typarray.
	 * (Before that the test is capable of generating false positives.) We
	 * still check for name beginning with '_', though, so as to avoid the
	 * cost of the subselect probe for all standard types.  This would have to
	 * be revisited if the backend ever allows renaming of array types.
	 */

	if (fout->remoteVersion >= 90200)
	{
		appendPQExpBuffer(query, "SELECT t.tableoid, t.oid, t.typname, "
						  "t.typnamespace, t.typacl, "
						  "t.typowner, "
						  "t.typinput::oid AS typinput, "
						  "t.typoutput::oid AS typoutput, t.typelem, t.typrelid, "
						  "CASE WHEN t.typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = t.typrelid) END AS typrelkind, "
						  "t.typtype, t.typisdefined, "
						  "t.typname[0] = '_' AND t.typelem != 0 AND "
						  "(SELECT typarray FROM pg_type te WHERE oid = t.typelem) = t.oid AS isarray, "
							"coalesce(array_to_string(e.typoptions, ', '), '') AS typstorage ");

			if (binary_upgrade)
				appendPQExpBuffer(query,
								", coalesce(t.typarray, 0) AS typarrayoid, "
								"coalesce(a.typname, '') AS typarrayname, "
								"coalesce(a.typnamespace, 0) AS typarrayns "
								"FROM pg_type t "
								"LEFT JOIN pg_catalog.pg_type a ON (t.typarray=a.oid) "
								"LEFT JOIN pg_type_encoding e ON t.oid = e.typid ");
			else
				appendPQExpBuffer(query,
									"FROM pg_type t "
									"LEFT JOIN pg_type_encoding e ON t.oid = e.typid ");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT t.tableoid, t.oid, t.typname, "
						  "t.typnamespace, NULL AS typacl, "
						  "t.typowner, "
						  "t.typinput::oid AS typinput, "
						  "t.typoutput::oid AS typoutput, t.typelem, t.typrelid, "
						  "CASE WHEN t.typrelid = 0 THEN ' '::\"char\" "
						  "ELSE (SELECT relkind FROM pg_class WHERE oid = t.typrelid) END AS typrelkind, "
						  "t.typtype, t.typisdefined, "
						  "t.typname[0] = '_' AND t.typelem != 0 AND "
						  "(SELECT typarray FROM pg_type te WHERE oid = t.typelem) = t.oid AS isarray, "
							"coalesce(array_to_string(e.typoptions, ', '), '') AS typstorage ");

			if (binary_upgrade)
				appendPQExpBuffer(query,
								", coalesce(t.typarray, 0) AS typarrayoid, "
								"coalesce(a.typname, '') AS typarrayname, "
								"coalesce(a.typnamespace, 0) AS typarrayns "
								"FROM pg_type t "
								"LEFT JOIN pg_catalog.pg_type a ON (t.typarray=a.oid) "
								"LEFT JOIN pg_type_encoding e ON t.oid = e.typid ");
			else
				appendPQExpBuffer(query,
									"FROM pg_type t "
									"LEFT JOIN pg_type_encoding e ON t.oid = e.typid ");
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	tyinfo = (TypeInfo *) pg_malloc(ntups * sizeof(TypeInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_typname = PQfnumber(res, "typname");
	i_typnamespace = PQfnumber(res, "typnamespace");
	i_typacl = PQfnumber(res, "typacl");
	i_typowner = PQfnumber(res, "typowner");
	i_typinput = PQfnumber(res, "typinput");
	i_typoutput = PQfnumber(res, "typoutput");
	i_typelem = PQfnumber(res, "typelem");
	i_typrelid = PQfnumber(res, "typrelid");
	i_typrelkind = PQfnumber(res, "typrelkind");
	i_typtype = PQfnumber(res, "typtype");
	i_typisdefined = PQfnumber(res, "typisdefined");
	i_isarray = PQfnumber(res, "isarray");
	i_typstorage = PQfnumber(res, "typstorage");
	i_typarrayoid = PQfnumber(res, "typarrayoid");
	i_typarrayname = PQfnumber(res, "typarrayname");
	i_typarrayns = PQfnumber(res, "typarrayns");

	for (i = 0; i < ntups; i++)
	{
		tyinfo[i].dobj.objType = DO_TYPE;
		tyinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		tyinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&tyinfo[i].dobj);
		tyinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_typname));
		tyinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_typnamespace)),
						  tyinfo[i].dobj.catId.oid);
		tyinfo[i].ftypname = NULL;	/* may get filled later */
		tyinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_typowner));
		tyinfo[i].typacl = pg_strdup(PQgetvalue(res, i, i_typacl));
		tyinfo[i].typelem = atooid(PQgetvalue(res, i, i_typelem));
		tyinfo[i].typrelid = atooid(PQgetvalue(res, i, i_typrelid));
		tyinfo[i].typrelkind = *PQgetvalue(res, i, i_typrelkind);
		tyinfo[i].typtype = *PQgetvalue(res, i, i_typtype);
		tyinfo[i].shellType = NULL;

		if (strcmp(PQgetvalue(res, i, i_typisdefined), "t") == 0)
			tyinfo[i].isDefined = true;
		else
			tyinfo[i].isDefined = false;

		if (strcmp(PQgetvalue(res, i, i_isarray), "t") == 0)
			tyinfo[i].isArray = true;
		else
			tyinfo[i].isArray = false;
		
		tyinfo[i].typstorage = pg_strdup(PQgetvalue(res, i, i_typstorage));

		if (binary_upgrade)
		{
			tyinfo[i].typarrayoid =  atooid(PQgetvalue(res, i, i_typarrayoid));
			tyinfo[i].typarrayname =  pg_strdup(PQgetvalue(res, i, i_typarrayname));
			tyinfo[i].typarrayns =  atooid(PQgetvalue(res, i, i_typarrayns));
		}

		/* Decide whether we want to dump it */
		selectDumpableType(&tyinfo[i]);

		/*
		 * If it's a domain, fetch info about its constraints, if any
		 */
		tyinfo[i].nDomChecks = 0;
		tyinfo[i].domChecks = NULL;
		if (tyinfo[i].dobj.dump && tyinfo[i].typtype == TYPTYPE_DOMAIN)
			getDomainConstraints(fout, &(tyinfo[i]));

		/*
		 * If it's a base type, make a DumpableObject representing a shell
		 * definition of the type.  We will need to dump that ahead of the I/O
		 * functions for the type.  Similarly, range types need a shell
		 * definition in case they have a canonicalize function.
		 *
		 * Note: the shell type doesn't have a catId.  You might think it
		 * should copy the base type's catId, but then it might capture the
		 * pg_depend entries for the type, which we don't want.
		 */
		if (tyinfo[i].dobj.dump && (tyinfo[i].typtype == TYPTYPE_BASE ||
									tyinfo[i].typtype == TYPTYPE_RANGE))
		{
			stinfo = (ShellTypeInfo *) pg_malloc(sizeof(ShellTypeInfo));
			stinfo->dobj.objType = DO_SHELL_TYPE;
			stinfo->dobj.catId = nilCatalogId;
			AssignDumpId(&stinfo->dobj);
			stinfo->dobj.name = pg_strdup(tyinfo[i].dobj.name);
			stinfo->dobj.namespace = tyinfo[i].dobj.namespace;
			stinfo->baseType = &(tyinfo[i]);
			tyinfo[i].shellType = stinfo;

			/*
			 * Initially mark the shell type as not to be dumped.  We'll only
			 * dump it if the I/O or canonicalize functions need to be dumped;
			 * this is taken care of while sorting dependencies.
			 */
			stinfo->dobj.dump = false;
		}
	}

	*numTypes = ntups;

	PQclear(res);

	destroyPQExpBuffer(query);

	return tyinfo;
}

/*
 * getOperators:
 *	  read all operators in the system catalogs and return them in the
 * OprInfo* structure
 *
 *	numOprs is set to the number of operators read in
 */
OprInfo *
getOperators(Archive *fout, int *numOprs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	OprInfo    *oprinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_oprname;
	int			i_oprnamespace;
	int			i_oprowner;
	int			i_oprkind;
	int			i_oprcode;

	/*
	 * find all operators, including builtin operators; we filter out
	 * system-defined operators at dump-out time.
	 */
	appendPQExpBuffer(query, "SELECT tableoid, oid, oprname, "
						"oprnamespace, "
						"oprowner, "
						"oprkind, "
						"oprcode::oid AS oprcode "
						"FROM pg_operator");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOprs = ntups;

	oprinfo = (OprInfo *) pg_malloc(ntups * sizeof(OprInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_oprname = PQfnumber(res, "oprname");
	i_oprnamespace = PQfnumber(res, "oprnamespace");
	i_oprowner = PQfnumber(res, "oprowner");
	i_oprkind = PQfnumber(res, "oprkind");
	i_oprcode = PQfnumber(res, "oprcode");

	for (i = 0; i < ntups; i++)
	{
		oprinfo[i].dobj.objType = DO_OPERATOR;
		oprinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		oprinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&oprinfo[i].dobj);
		oprinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_oprname));
		oprinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_oprnamespace)),
						  oprinfo[i].dobj.catId.oid);
		oprinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_oprowner));
		oprinfo[i].oprkind = (PQgetvalue(res, i, i_oprkind))[0];
		oprinfo[i].oprcode = atooid(PQgetvalue(res, i, i_oprcode));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(oprinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return oprinfo;
}

/*
 * getCollations:
 *	  read all collations in the system catalogs and return them in the
 * CollInfo* structure
 *
 *	numCollations is set to the number of collations read in
 */
CollInfo *
getCollations(Archive *fout, int *numCollations)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	CollInfo   *collinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_collname;
	int			i_collnamespace;
	int			i_collowner;

	/* Collations didn't exist pre-9.1 */
	if (fout->remoteVersion < 90100)
	{
		*numCollations = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	/*
	 * find all collations, including builtin collations; we filter out
	 * system-defined collations at dump-out time.
	 */

	appendPQExpBuffer(query, "SELECT tableoid, oid, collname, "
					  "collnamespace, "
					  "collowner "
					  "FROM pg_collation");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numCollations = ntups;

	collinfo = (CollInfo *) pg_malloc(ntups * sizeof(CollInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_collname = PQfnumber(res, "collname");
	i_collnamespace = PQfnumber(res, "collnamespace");
	i_collowner = PQfnumber(res, "collowner");

	for (i = 0; i < ntups; i++)
	{
		collinfo[i].dobj.objType = DO_COLLATION;
		collinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		collinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&collinfo[i].dobj);
		collinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_collname));
		collinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_collnamespace)),
						  collinfo[i].dobj.catId.oid);
		collinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_collowner));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(collinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return collinfo;
}

/*
 * getConversions:
 *	  read all conversions in the system catalogs and return them in the
 * ConvInfo* structure
 *
 *	numConversions is set to the number of conversions read in
 */
ConvInfo *
getConversions(Archive *fout, int *numConversions)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	ConvInfo   *convinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_conname;
	int			i_connamespace;
	int			i_conowner;

	query = createPQExpBuffer();

	/*
	 * find all conversions, including builtin conversions; we filter out
	 * system-defined conversions at dump-out time.
	 */

	appendPQExpBuffer(query, "SELECT tableoid, oid, conname, "
					  "connamespace, "
					  "conowner "
					  "FROM pg_conversion");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numConversions = ntups;

	convinfo = (ConvInfo *) pg_malloc(ntups * sizeof(ConvInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_conname = PQfnumber(res, "conname");
	i_connamespace = PQfnumber(res, "connamespace");
	i_conowner = PQfnumber(res, "conowner");

	for (i = 0; i < ntups; i++)
	{
		convinfo[i].dobj.objType = DO_CONVERSION;
		convinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		convinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&convinfo[i].dobj);
		convinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_conname));
		convinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_connamespace)),
						  convinfo[i].dobj.catId.oid);
		convinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_conowner));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(convinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return convinfo;
}

/*
 * getOpclasses:
 *	  read all opclasses in the system catalogs and return them in the
 * OpclassInfo* structure
 *
 *	numOpclasses is set to the number of opclasses read in
 */
OpclassInfo *
getOpclasses(Archive *fout, int *numOpclasses)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	OpclassInfo *opcinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_opcname;
	int			i_opcnamespace;
	int			i_opcowner;

	/*
	 * find all opclasses, including builtin opclasses; we filter out
	 * system-defined opclasses at dump-out time.
	 */
	appendPQExpBuffer(query, "SELECT tableoid, oid, opcname, "
						"opcnamespace, "
						"opcowner "
						"FROM pg_opclass");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOpclasses = ntups;

	opcinfo = (OpclassInfo *) pg_malloc(ntups * sizeof(OpclassInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_opcname = PQfnumber(res, "opcname");
	i_opcnamespace = PQfnumber(res, "opcnamespace");
	i_opcowner = PQfnumber(res, "opcowner");

	for (i = 0; i < ntups; i++)
	{
		opcinfo[i].dobj.objType = DO_OPCLASS;
		opcinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		opcinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&opcinfo[i].dobj);
		opcinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_opcname));
		opcinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_opcnamespace)),
						  opcinfo[i].dobj.catId.oid);
		opcinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_opcowner));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(opcinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return opcinfo;
}

/*
 * getOpfamilies:
 *	  read all opfamilies in the system catalogs and return them in the
 * OpfamilyInfo* structure
 *
 *	numOpfamilies is set to the number of opfamilies read in
 */
OpfamilyInfo *
getOpfamilies(Archive *fout, int *numOpfamilies)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	OpfamilyInfo *opfinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_opfname;
	int			i_opfnamespace;
	int			i_opfowner;

	query = createPQExpBuffer();

	/*
	 * find all opfamilies, including builtin opfamilies; we filter out
	 * system-defined opfamilies at dump-out time.
	 */

	appendPQExpBuffer(query, "SELECT tableoid, oid, opfname, "
					  "opfnamespace, "
					  "opfowner "
					  "FROM pg_opfamily");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numOpfamilies = ntups;

	opfinfo = (OpfamilyInfo *) pg_malloc(ntups * sizeof(OpfamilyInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_opfname = PQfnumber(res, "opfname");
	i_opfnamespace = PQfnumber(res, "opfnamespace");
	i_opfowner = PQfnumber(res, "opfowner");

	for (i = 0; i < ntups; i++)
	{
		opfinfo[i].dobj.objType = DO_OPFAMILY;
		opfinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		opfinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&opfinfo[i].dobj);
		opfinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_opfname));
		opfinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_opfnamespace)),
						  opfinfo[i].dobj.catId.oid);
		opfinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_opfowner));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(opfinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return opfinfo;
}

/*
 * getAggregates:
 *	  read all the user-defined aggregates in the system catalogs and
 * return them in the AggInfo* structure
 *
 * numAggs is set to the number of aggregates read in
 */
AggInfo *
getAggregates(Archive *fout, int *numAggs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	AggInfo    *agginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_aggname;
	int			i_aggnamespace;
	int			i_pronargs;
	int			i_proargtypes;
	int			i_proowner;
	int			i_aggacl;

	/*
	 * Find all user-defined aggregates.  See comment in getFuncs() for the
	 * rationale behind the filtering logic.
	 */

	appendPQExpBuffer(query, "SELECT tableoid, oid, proname AS aggname, "
						"pronamespace AS aggnamespace, "
						"pronargs, proargtypes, "
						"proowner, "
						"proacl AS aggacl "
						"FROM pg_proc p "
						"WHERE proisagg AND ("
						"pronamespace != "
						"(SELECT oid FROM pg_namespace "
						"WHERE nspname = 'pg_catalog')");

	if (binary_upgrade && fout->remoteVersion >= 90100)
		appendPQExpBufferStr(query,
								" OR EXISTS(SELECT 1 FROM pg_depend WHERE "
								"classid = 'pg_proc'::regclass AND "
								"objid = p.oid AND "
								"refclassid = 'pg_extension'::regclass AND "
								"deptype = 'e')");

	appendPQExpBufferChar(query, ')');

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numAggs = ntups;

	agginfo = (AggInfo *) pg_malloc(ntups * sizeof(AggInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_aggname = PQfnumber(res, "aggname");
	i_aggnamespace = PQfnumber(res, "aggnamespace");
	i_pronargs = PQfnumber(res, "pronargs");
	i_proargtypes = PQfnumber(res, "proargtypes");
	i_proowner = PQfnumber(res, "proowner");
	i_aggacl = PQfnumber(res, "aggacl");

	for (i = 0; i < ntups; i++)
	{
		agginfo[i].aggfn.dobj.objType = DO_AGG;
		agginfo[i].aggfn.dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		agginfo[i].aggfn.dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&agginfo[i].aggfn.dobj);
		agginfo[i].aggfn.dobj.name = pg_strdup(PQgetvalue(res, i, i_aggname));
		agginfo[i].aggfn.dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_aggnamespace)),
						  agginfo[i].aggfn.dobj.catId.oid);
		agginfo[i].aggfn.rolname = getRoleName(PQgetvalue(res, i, i_proowner));
		agginfo[i].aggfn.lang = InvalidOid;		/* not currently interesting */
		agginfo[i].aggfn.prorettype = InvalidOid;		/* not saved */
		agginfo[i].aggfn.proacl = pg_strdup(PQgetvalue(res, i, i_aggacl));
		agginfo[i].aggfn.nargs = atoi(PQgetvalue(res, i, i_pronargs));
		if (agginfo[i].aggfn.nargs == 0)
			agginfo[i].aggfn.argtypes = NULL;
		else
		{
			agginfo[i].aggfn.argtypes = (Oid *) pg_malloc(agginfo[i].aggfn.nargs * sizeof(Oid));
			parseOidArray(PQgetvalue(res, i, i_proargtypes),
						  agginfo[i].aggfn.argtypes,
						  agginfo[i].aggfn.nargs);
		}

		/* Decide whether we want to dump it */
		selectDumpableObject(&(agginfo[i].aggfn.dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return agginfo;
}

/*
 * getExtProtocols:
 *	  read all the user-defined protocols in the system catalogs and
 * return them in the ExtProtInfo* structure
 *
 * numExtProtocols is set to the number of protocols read in
 */
/*	Declared in pg_dump.h */
ExtProtInfo *
getExtProtocols(Archive *fout, int *numExtProtocols)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	ExtProtInfo *ptcinfo;
	int			i_oid;
	int			i_tableoid;
	int			i_ptcname;
	int			i_ptcowner;
	int			i_ptcacl;
	int			i_ptctrusted;
	int 		i_ptcreadid;
	int			i_ptcwriteid;
	int			i_ptcvalidid;

	/* find all user-defined external protocol */

	appendPQExpBuffer(query, "SELECT ptc.tableoid as tableoid, "
							 "       ptc.oid as oid, "
							 "       ptc.ptcname as ptcname, "
							 "       ptcreadfn as ptcreadoid, "
							 "       ptcwritefn as ptcwriteoid, "
							 "		 ptcvalidatorfn as ptcvaloid, "
							 "       ptcowner, "
							 "       ptc.ptctrusted as ptctrusted, "
							 "       ptc.ptcacl as ptcacl "
							 "FROM   pg_extprotocol ptc");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numExtProtocols = ntups;

	ptcinfo = (ExtProtInfo *) pg_malloc(ntups * sizeof(ExtProtInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_ptcname = PQfnumber(res, "ptcname");
	i_ptcowner = PQfnumber(res, "ptcowner");
	i_ptcacl = PQfnumber(res, "ptcacl");
	i_ptctrusted = PQfnumber(res, "ptctrusted");
	i_ptcreadid = PQfnumber(res, "ptcreadoid");
	i_ptcwriteid = PQfnumber(res, "ptcwriteoid");
	i_ptcvalidid = PQfnumber(res, "ptcvaloid");

	for (i = 0; i < ntups; i++)
	{
		ptcinfo[i].dobj.objType = DO_EXTPROTOCOL;
		ptcinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		ptcinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&ptcinfo[i].dobj);
		ptcinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_ptcname));
		ptcinfo[i].dobj.namespace = NULL;
		ptcinfo[i].rolname = getRoleName(PQgetvalue(res, i,  i_ptcowner));
		if (PQgetisnull(res, i, i_ptcreadid))
			ptcinfo[i].ptcreadid = InvalidOid;
		else
			ptcinfo[i].ptcreadid = atooid(PQgetvalue(res, i, i_ptcreadid));

		if (PQgetisnull(res, i, i_ptcwriteid))
			ptcinfo[i].ptcwriteid = InvalidOid;
		else
			ptcinfo[i].ptcwriteid = atooid(PQgetvalue(res, i, i_ptcwriteid));

		if (PQgetisnull(res, i, i_ptcvalidid))
			ptcinfo[i].ptcvalidid = InvalidOid;
		else
			ptcinfo[i].ptcvalidid = atooid(PQgetvalue(res, i, i_ptcvalidid));

		ptcinfo[i].ptcacl = pg_strdup(PQgetvalue(res, i, i_ptcacl));
		ptcinfo[i].ptctrusted = *(PQgetvalue(res, i, i_ptctrusted)) == 't';

		/* Decide whether we want to dump it */
		selectDumpableObject(&(ptcinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return ptcinfo;
}

/*
 * getFuncs:
 *	  read all the user-defined functions in the system catalogs and
 * return them in the FuncInfo* structure
 *
 * numFuncs is set to the number of functions read in
 */
FuncInfo *
getFuncs(Archive *fout, int *numFuncs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	FuncInfo   *finfo;
	int			i_tableoid;
	int			i_oid;
	int			i_proname;
	int			i_pronamespace;
	int			i_proowner;
	int			i_prolang;
	int			i_pronargs;
	int			i_proargtypes;
	int			i_prorettype;
	int			i_proacl;

	/*
	 * Find all interesting functions.  This is a bit complicated:
	 *
	 * 1. Always exclude aggregates; those are handled elsewhere.
	 *
	 * 2. Always exclude functions that are internally dependent on something
	 * else, since presumably those will be created as a result of creating
	 * the something else.  This currently acts only to suppress constructor
	 * functions for range types (so we only need it in 9.2 and up).  Note
	 * this is OK only because the constructors don't have any dependencies
	 * the range type doesn't have; otherwise we might not get creation
	 * ordering correct.
	 *
	 * 3. Otherwise, we normally exclude functions in pg_catalog.  However, if
	 * they're members of extensions and we are in binary-upgrade mode then
	 * include them, since we want to dump extension members individually in
	 * that mode.  Also, if they are used by casts then we need to gather the
	 * information about them, though they won't be dumped if they are
	 * built-in.
	 */

	appendPQExpBuffer(query,
						"SELECT tableoid, oid, proname, prolang, "
						"pronargs, proargtypes, prorettype, proacl, "
						"pronamespace, "
						"proowner "
						"FROM pg_proc p "
						"WHERE NOT proisagg");

	if (fout->remoteVersion >= 90200)
		appendPQExpBufferStr(query,
								"\n  AND NOT EXISTS (SELECT 1 FROM pg_depend "
								"WHERE classid = 'pg_proc'::regclass AND "
								"objid = p.oid AND deptype = 'i')");

	appendPQExpBuffer(query,
							"\n  AND ("
							"\n  pronamespace != "
							"(SELECT oid FROM pg_namespace "
							"WHERE nspname = 'pg_catalog')"
							"\n  OR EXISTS (SELECT 1 FROM pg_cast"
							"\n  WHERE pg_cast.oid > '%u'::oid"
							"\n  AND p.oid = pg_cast.castfunc)",
							FirstNormalObjectId);

	if (binary_upgrade)
		appendPQExpBufferStr(query,
								"\n  OR EXISTS(SELECT 1 FROM pg_depend WHERE "
								"classid = 'pg_proc'::regclass AND "
								"objid = p.oid AND "
								"refclassid = 'pg_extension'::regclass AND "
								"deptype = 'e')");

	appendPQExpBufferChar(query, ')');

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numFuncs = ntups;

	finfo = (FuncInfo *) pg_malloc0(ntups * sizeof(FuncInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_proname = PQfnumber(res, "proname");
	i_pronamespace = PQfnumber(res, "pronamespace");
	i_proowner = PQfnumber(res, "proowner");
	i_prolang = PQfnumber(res, "prolang");
	i_pronargs = PQfnumber(res, "pronargs");
	i_proargtypes = PQfnumber(res, "proargtypes");
	i_prorettype = PQfnumber(res, "prorettype");
	i_proacl = PQfnumber(res, "proacl");

	for (i = 0; i < ntups; i++)
	{
		finfo[i].dobj.objType = DO_FUNC;
		finfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		finfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&finfo[i].dobj);
		finfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_proname));
		finfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_pronamespace)),
						  finfo[i].dobj.catId.oid);
		finfo[i].rolname = getRoleName(PQgetvalue(res, i, i_proowner));
		finfo[i].lang = atooid(PQgetvalue(res, i, i_prolang));
		finfo[i].prorettype = atooid(PQgetvalue(res, i, i_prorettype));
		finfo[i].proacl = pg_strdup(PQgetvalue(res, i, i_proacl));
		finfo[i].nargs = atoi(PQgetvalue(res, i, i_pronargs));
		if (finfo[i].nargs == 0)
			finfo[i].argtypes = NULL;
		else
		{
			finfo[i].argtypes = (Oid *) pg_malloc(finfo[i].nargs * sizeof(Oid));
			parseOidArray(PQgetvalue(res, i, i_proargtypes),
						  finfo[i].argtypes, finfo[i].nargs);
		}

		/* Decide whether we want to dump it */
		selectDumpableFunction(&finfo[i]);
		selectDumpableObject(&(finfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return finfo;
}

/*
 * getTables
 *	  read all the user-defined tables (no indexes, no catalogs)
 * in the system catalogs return them in the TableInfo* structure
 *
 * numTables is set to the number of tables read in
 */
TableInfo *
getTables(Archive *fout, int *numTables)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	TableInfo  *tblinfo;
	bool 		lockTableDumped = false;
	int			i_reltableoid;
	int			i_reloid;
	int			i_relname;
	int			i_relnamespace;
	int			i_relkind;
	int			i_reltype;
	int			i_relacl;
	int			i_relowner;
	int			i_relchecks;
	int			i_relhastriggers;
	int			i_relhasindex;
	int			i_relhasrules;
	int			i_relhasoids;
	int			i_relfrozenxid;
	int			i_relminmxid;
	int			i_toastoid;
	int			i_toastfrozenxid;
	int			i_toastminmxid;
	int			i_relpersistence;
	int			i_relispopulated;
	int			i_relreplident;
	int			i_owning_tab;
	int			i_owning_col;
	int			i_reltablespace;
	int			i_reloptions;
	int			i_checkoption;
	int			i_toastreloptions;
	int			i_reloftype;
	int			i_relpages;
	int			i_relstorage;
	int			i_parrelid;
	int			i_parlevel;
	int			i_toast_type_oid;
	int			i_toast_index_oid;
	int			i_distclause;
	/*
	 * Find all the tables and table-like objects.
	 *
	 * We include system catalogs, so that we can work if a user table is
	 * defined to inherit from a system catalog (pretty weird, but...)
	 *
	 * We ignore relations that are not ordinary tables, sequences, views,
	 * materialized views, composite types, or foreign tables.
	 *
	 * Composite-type table entries won't be dumped as such, but we have to
	 * make a DumpableObject for them so that we can track dependencies of the
	 * composite type (pg_depend entries for columns of the composite type
	 * link to the pg_class entry not the pg_type entry).
	 *
	 * Note: in this phase we should collect only a minimal amount of
	 * information about each table, basically just enough to decide if it is
	 * interesting. We must fetch all tables in this phase because otherwise
	 * we cannot correctly identify inherited columns, owned sequences, etc.
	 */

	appendPQExpBuffer(query,
					  "SELECT c.tableoid, c.oid, c.relname, "
					  "c.relacl, c.relnamespace, c.relkind, c.reltype, "
					  "c.relowner, "
					  "c.relchecks, c.relhasoids, "
					  "c.relhasindex, c.relhasrules, c.relpages, "
					  "d.refobjid AS owning_tab, "
					  "d.refobjsubid AS owning_col, "
					  "tsp.spcname AS reltablespace, "
						"c.relfrozenxid, tc.relfrozenxid AS tfrozenxid, "
						"tc.oid AS toid, "
						"c.relstorage, "
						"p.parrelid as parrelid, "
						"pl.parlevel as parlevel, ");

	if (binary_upgrade)
		appendPQExpBufferStr(query,
							"tc.reltype AS toast_type_oid, "
							"i.indexrelid as toast_index_oid, ");

	/* GPDB5: We expect either an empty policy entry, or exactly
	 * 1 policy entry in gp_distribution_policy for a given table.
	 * If there is more than 1 entry in the policy table for an
	 * oid the scalar subquery will fail as intended.
	 */
	if (isGPDB6000OrLater(fout))
	 		appendPQExpBufferStr(query,
							"pg_catalog.pg_get_table_distributedby(c.oid) as distclause, ");
	else
			appendPQExpBufferStr(query,
							"(SELECT attrnums FROM pg_catalog.gp_distribution_policy p "
					  	"WHERE p.localoid = c.oid) as distclause, ");

	if (fout->remoteVersion >= 80400)
		appendPQExpBufferStr(query,
						  "c.relhastriggers, ");
	else
		appendPQExpBufferStr(query,
						  "(c.reltriggers <> 0) AS relhastriggers, ");

	if (fout->remoteVersion >= 90100)
		appendPQExpBufferStr(query,
						  "c.relpersistence, ");
	else
		appendPQExpBufferStr(query,
						  "'p' AS relpersistence, ");

	if (fout->remoteVersion >= 90300)
		appendPQExpBufferStr(query,
						  "c.relispopulated, ");
	else
		appendPQExpBufferStr(query,
						  "'t' as relispopulated, ");

	if (fout->remoteVersion >= 90400)
		appendPQExpBufferStr(query,
						  "c.relreplident, ");
	else
		appendPQExpBufferStr(query,
						  "'d' AS relreplident, ");

	if (fout->remoteVersion >= 90300)
		appendPQExpBufferStr(query,
						  "c.relminmxid, tc.relminmxid AS tminmxid, ");
	else
		appendPQExpBufferStr(query,
						  "0 AS relminmxid, 0 AS tminmxid, ");

	if (fout->remoteVersion >= 90300)
		appendPQExpBufferStr(query,
						  "array_remove(array_remove(c.reloptions,'check_option=local'),'check_option=cascaded') AS reloptions, "
						  "CASE WHEN 'check_option=local' = ANY (c.reloptions) THEN 'LOCAL'::text "
						  "WHEN 'check_option=cascaded' = ANY (c.reloptions) THEN 'CASCADED'::text ELSE NULL END AS checkoption, ");
	else
		appendPQExpBufferStr(query,
						  "c.reloptions, NULL AS checkoption, ");

	if (fout->remoteVersion >= 80400)
		appendPQExpBufferStr(query,
						  "tc.reloptions AS toast_reloptions, ");
	else
		appendPQExpBufferStr(query,
						  "NULL AS toast_reloptions, ");

	if (fout->remoteVersion >= 90000)
		appendPQExpBufferStr(query,
						  "CASE WHEN c.reloftype <> 0 THEN c.reloftype::pg_catalog.regtype ELSE NULL END AS reloftype ");
	else
		appendPQExpBufferStr(query,
						  "NULL AS reloftype ");

	/*
	 * Left join to pg_depend to pick up dependency info linking sequences to
	 * their owning column, if any (note this dependency is AUTO as of 8.2).
	 * Also join to pg_tablespace to collect the spcname.
	 */
	appendPQExpBufferStr(query,
						  "\nFROM pg_class c\n"
						  "LEFT JOIN pg_depend d ON "
						  "(c.relkind = " CppAsString2(RELKIND_SEQUENCE) " AND "
						  "d.classid = c.tableoid AND d.objid = c.oid AND "
						  "d.objsubid = 0 AND "
						  "d.refclassid = c.tableoid AND d.deptype IN ('a', 'i'))\n"
						  "LEFT JOIN pg_tablespace tsp ON (tsp.oid = c.reltablespace)\n");

	/*
	 * We purposefully ignore toast OIDs for partitioned tables; the reason is
	 * that versions 10 and 11 have them, but later versions do not, so
	 * emitting them causes the upgrade to fail.
	 */

	appendPQExpBufferStr(query,
						  "LEFT JOIN pg_class tc ON (c.reltoastrelid = tc.oid"
						  " AND tc.relkind = " CppAsString2(RELKIND_TOASTVALUE) ")\n"
						  "LEFT JOIN pg_partition_rule pr ON c.oid = pr.parchildrelid\n"
						  "LEFT JOIN pg_partition p ON pr.paroid = p.oid\n"
						  "LEFT JOIN pg_partition pl ON (c.oid = pl.parrelid AND pl.parlevel = 0)\n");

	if (binary_upgrade)
		appendPQExpBuffer(query,
						"LEFT JOIN pg_catalog.pg_index i ON (c.reltoastrelid = i.indrelid AND i.indisvalid)\n"
					  "LEFT JOIN pg_catalog.pg_class ti ON (i.indexrelid = ti.oid)\n");

	/*
	 * Restrict to interesting relkinds (in particular, not indexes).  Not all
	 * relkinds are possible in older servers, but it's not worth the trouble
	 * to emit a version-dependent list.
	 *
	 * Composite-type table entries won't be dumped as such, but we have to
	 * make a DumpableObject for them so that we can track dependencies of the
	 * composite type (pg_depend entries for columns of the composite type
	 * link to the pg_class entry not the pg_type entry).
	 */
	appendPQExpBufferStr(query,
						  "WHERE c.relkind IN ("
						  CppAsString2(RELKIND_RELATION) ", "
						  CppAsString2(RELKIND_SEQUENCE) ", "
						  CppAsString2(RELKIND_VIEW) ", "
						  CppAsString2(RELKIND_COMPOSITE_TYPE) ", "
						  CppAsString2(RELKIND_MATVIEW) ", "
						  CppAsString2(RELKIND_FOREIGN_TABLE) ")\n");

  if (fout->remoteVersion >= 80400)
		appendPQExpBufferStr(query,
						  "AND c.relnamespace <> 7012\n"); /* BM_BITMAPINDEX_NAMESPACE */
  else
		appendPQExpBufferStr(query,
						  "AND c.relnamespace <> 3012\n"); /* BM_BITMAPINDEX_NAMESPACE in GPDB 5 and below */

	appendPQExpBufferStr(query,
							"ORDER BY c.oid");
	
	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numTables = ntups;

	/*
	 * Extract data from result and lock dumpable tables.  We do the locking
	 * before anything else, to minimize the window wherein a table could
	 * disappear under us.
	 *
	 * Note that we have to save info about all tables here, even when dumping
	 * only one, because we don't yet know which tables might be inheritance
	 * ancestors of the target table.
	 */
	tblinfo = (TableInfo *) pg_malloc0(ntups * sizeof(TableInfo));

	i_reltableoid = PQfnumber(res, "tableoid");
	i_reloid = PQfnumber(res, "oid");
	i_relname = PQfnumber(res, "relname");
	i_relnamespace = PQfnumber(res, "relnamespace");
	i_relacl = PQfnumber(res, "relacl");
	i_relkind = PQfnumber(res, "relkind");
	i_relowner = PQfnumber(res, "relowner");
	i_relchecks = PQfnumber(res, "relchecks");
	i_relhasindex = PQfnumber(res, "relhasindex");
	i_relhasrules = PQfnumber(res, "relhasrules");
	i_relpages = PQfnumber(res, "relpages");
	i_owning_tab = PQfnumber(res, "owning_tab");
	i_owning_col = PQfnumber(res, "owning_col");
	i_reltablespace = PQfnumber(res, "reltablespace");
	i_relhasoids = PQfnumber(res, "relhasoids");
	i_relhastriggers = PQfnumber(res, "relhastriggers");
	i_relpersistence = PQfnumber(res, "relpersistence");
	i_relispopulated = PQfnumber(res, "relispopulated");
	i_relreplident = PQfnumber(res, "relreplident");
	i_relfrozenxid = PQfnumber(res, "relfrozenxid");
	i_toastfrozenxid = PQfnumber(res, "tfrozenxid");
	i_toastoid = PQfnumber(res, "toid");
	i_relminmxid = PQfnumber(res, "relminmxid");
	i_toastminmxid = PQfnumber(res, "tminmxid");
	i_reloptions = PQfnumber(res, "reloptions");
	i_checkoption = PQfnumber(res, "checkoption");
	i_toastreloptions = PQfnumber(res, "toast_reloptions");
	i_reloftype = PQfnumber(res, "reloftype");
	i_relstorage = PQfnumber(res, "relstorage");
	i_parrelid = PQfnumber(res, "parrelid");
	i_parlevel = PQfnumber(res, "parlevel");
	i_toast_type_oid = PQfnumber(res, "toast_type_oid");
	i_toast_index_oid = PQfnumber(res, "toast_index_oid");
	i_reltype = PQfnumber(res, "reltype");
	i_distclause = PQfnumber(res, "distclause");

	if (lockWaitTimeout)
	{
		/*
		 * Arrange to fail instead of waiting forever for a table lock.
		 *
		 * NB: this coding assumes that the only queries issued within the
		 * following loop are LOCK TABLEs; else the timeout may be undesirably
		 * applied to other things too.
		 */
		resetPQExpBuffer(query);
		appendPQExpBufferStr(query, "SET statement_timeout = ");
		appendStringLiteralConn(query, lockWaitTimeout, GetConnection(fout));
		ExecuteSqlStatement(fout, query->data);
	}

	resetPQExpBuffer(query);

	for (i = 0; i < ntups; i++)
	{
		tblinfo[i].dobj.objType = DO_TABLE;
		tblinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_reltableoid));
		tblinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_reloid));
		AssignDumpId(&tblinfo[i].dobj);
		tblinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_relname));
		tblinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_relnamespace)),
						  tblinfo[i].dobj.catId.oid);
		tblinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_relowner));
		tblinfo[i].relacl = pg_strdup(PQgetvalue(res, i, i_relacl));
		tblinfo[i].relkind = *(PQgetvalue(res, i, i_relkind));
		tblinfo[i].reltype = atooid(PQgetvalue(res, i, i_reltype));
		tblinfo[i].relstorage = *(PQgetvalue(res, i, i_relstorage));
		tblinfo[i].relpersistence = *(PQgetvalue(res, i, i_relpersistence));
		tblinfo[i].hasindex = (strcmp(PQgetvalue(res, i, i_relhasindex), "t") == 0);
		tblinfo[i].hasrules = (strcmp(PQgetvalue(res, i, i_relhasrules), "t") == 0);
		tblinfo[i].hastriggers = (strcmp(PQgetvalue(res, i, i_relhastriggers), "t") == 0);
		tblinfo[i].hasoids = (strcmp(PQgetvalue(res, i, i_relhasoids), "t") == 0);
		tblinfo[i].relispopulated = (strcmp(PQgetvalue(res, i, i_relispopulated), "t") == 0);
		tblinfo[i].relreplident = *(PQgetvalue(res, i, i_relreplident));
		tblinfo[i].relpages = atoi(PQgetvalue(res, i, i_relpages));
		if (PQgetisnull(res, i, i_owning_tab))
		{
			tblinfo[i].owning_tab = InvalidOid;
			tblinfo[i].owning_col = 0;
		}
		else
		{
			tblinfo[i].owning_tab = atooid(PQgetvalue(res, i, i_owning_tab));
			tblinfo[i].owning_col = atoi(PQgetvalue(res, i, i_owning_col));
		}
		tblinfo[i].reltablespace = pg_strdup(PQgetvalue(res, i, i_reltablespace));
		tblinfo[i].hasoids = (strcmp(PQgetvalue(res, i, i_relhasoids), "t") == 0);
		tblinfo[i].hastriggers = (strcmp(PQgetvalue(res, i, i_relhastriggers), "t") == 0);
		tblinfo[i].relpersistence = *(PQgetvalue(res, i, i_relpersistence));
		tblinfo[i].relispopulated = (strcmp(PQgetvalue(res, i, i_relispopulated), "t") == 0);
		tblinfo[i].relreplident = *(PQgetvalue(res, i, i_relreplident));
		tblinfo[i].frozenxid = atooid(PQgetvalue(res, i, i_relfrozenxid));
		tblinfo[i].toast_frozenxid = atooid(PQgetvalue(res, i, i_toastfrozenxid));
		tblinfo[i].toast_oid = atooid(PQgetvalue(res, i, i_toastoid));
		tblinfo[i].minmxid = atooid(PQgetvalue(res, i, i_relminmxid));
		tblinfo[i].toast_minmxid = atooid(PQgetvalue(res, i, i_toastminmxid));
		tblinfo[i].reloptions = pg_strdup(PQgetvalue(res, i, i_reloptions));
		if (PQgetisnull(res, i, i_checkoption))
			tblinfo[i].checkoption = NULL;
		else
			tblinfo[i].checkoption = pg_strdup(PQgetvalue(res, i, i_checkoption));
		tblinfo[i].toast_reloptions = pg_strdup(PQgetvalue(res, i, i_toastreloptions));
		tblinfo[i].parrelid = atooid(PQgetvalue(res, i, i_parrelid));
		if (PQgetisnull(res, i, i_parlevel))
			tblinfo[i].parlevel = -1;
		else
			tblinfo[i].parlevel = atoi(PQgetvalue(res, i, i_parlevel));

		if (tblinfo[i].parlevel == 0)
			tblinfo[i].parparent = true;
		else
			tblinfo[i].parparent = false;


		if (tblinfo[i].parrelid != 0 && tblinfo[i].relstorage == 'x')
		{
			/*
			 * Length of tmpStr is bigger than the sum of NAMEDATALEN
			 * and the length of EXT_PARTITION_NAME_POSTFIX
			 */
			char tmpStr[500];
			snprintf(tmpStr, sizeof(tmpStr), "%s%s", tblinfo[i].dobj.name, EXT_PARTITION_NAME_POSTFIX);
			tblinfo[i].dobj.name = pg_strdup(tmpStr);
		}
	
		tblinfo[i].distclause = pg_strdup(PQgetvalue(res, i, i_distclause));

		if (binary_upgrade)
		{
			/* AO table metadata will be set in getAOTableInfo() */
			tblinfo[i].aotbl = NULL;
			tblinfo[i].toast_index = atooid(PQgetvalue(res, i, i_toast_index_oid));
			tblinfo[i].toast_type = atooid(PQgetvalue(res, i, i_toast_type_oid));
		}

		/* other fields were zeroed above */

		/*
		 * Decide whether we want to dump this table.
		 * GPDB: To gather binary upgrade information for partition children,
		 * they need to exist in the tblinfo array for findTableByOid.
		 * This requires the getTable query to also collect all
		 * partition children so they can be referenced, but we do not want
		 * to dump the partition children as their DDL will be handled by the parent.
		 * We do need to dump external partition DDL however, so ensure partition children
		 * with external storage have their dump flag set.
		 */
		if (tblinfo[i].relkind == RELKIND_COMPOSITE_TYPE ||
			(tblinfo[i].parrelid != 0 && tblinfo[i].relstorage != 'x'))
			tblinfo[i].dobj.dump = false;
		else
			selectDumpableTable(&tblinfo[i]);
		tblinfo[i].interesting = tblinfo[i].dobj.dump;
		tblinfo[i].dummy_view = false;	/* might get set during sort */
		tblinfo[i].postponed_def = false;		/* might get set during sort */

		/*
		 * Read-lock target tables to make sure they aren't DROPPED or altered
		 * in schema before we get around to dumping them.
		 *
		 * Note that we don't explicitly lock parents of the target tables; we
		 * assume our lock on the child is enough to prevent schema
		 * alterations to parent tables.
		 *
		 * NOTE: it'd be kinda nice to lock other relations too, not only
		 * plain tables, but the backend doesn't presently allow that.
		 * 
		 * GPDB: Build a single LOCK TABLE statement to lock all interesting tables.
		 * This is more performant than issuing a separate LOCK TABLE statement for each table,
		 * with considerable savings in FE/BE overhead. It does come at the cost of some increased
		 * memory usage in both FE and BE, which we will be able to tolerate.
		 */
		if (tblinfo[i].dobj.dump && tblinfo[i].relkind == RELKIND_RELATION && tblinfo[i].parrelid == 0)
		{
			if (!lockTableDumped)
				appendPQExpBuffer(query,
						"LOCK TABLE %s ",
						fmtQualifiedDumpable(&tblinfo[i]));
			else
				appendPQExpBuffer(query,
						",%s ",
						fmtQualifiedDumpable(&tblinfo[i]));
			lockTableDumped = true;
		}
	}
	/* Are there any tables to lock? */
	if (lockTableDumped)
	{
	appendPQExpBuffer(query,
			"IN ACCESS SHARE MODE");
	ExecuteSqlStatement(fout, query->data);
	}

	if (lockWaitTimeout)
	{
		ExecuteSqlStatement(fout, "SET statement_timeout = 0");
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return tblinfo;
}

void
getAOTableInfo(Archive *fout)
{
	PQExpBuffer query = createPQExpBuffer();
	PGresult	*res;
	int			ntups;
	AOTableInfo *aotblinfo;
	int     i_oid;
	int			i_columnstore;
	int			i_segrelid;
	int			i_segreltype;
	int			i_blkdirrelid;
	int			i_blkdirreltype;
	int			i_blkdiridxid;
	int			i_visimaprelid;
	int			i_visimapreltype;
	int			i_visimapidxid;

	resetPQExpBuffer(query);

	appendPQExpBufferStr(query,
					  "SELECT "
						"ao.relid,"
						"ao.columnstore,"
						"ao.segrelid, t1.reltype as segreltype, "
						"ao.blkdirrelid, t3.reltype as blkdirreltype, "
						"ao.blkdiridxid, "
						"ao.visimaprelid, t2.reltype as visimapreltype, "
						"ao.visimapidxid "
						"\nFROM pg_catalog.pg_appendonly ao\n"
						"LEFT JOIN pg_class c ON (c.oid=ao.relid)\n"
						"LEFT JOIN pg_class t1 ON (t1.oid=ao.segrelid)\n"
						"LEFT JOIN pg_class t2 ON (t2.oid=ao.visimaprelid)\n"
						"LEFT JOIN pg_class t3 ON (t3.oid=ao.blkdirrelid and ao.blkdirrelid <> 0)\n"
						"ORDER BY 1");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_oid = PQfnumber(res, "relid");
	i_columnstore =  PQfnumber(res, "columnstore");
	i_segrelid = PQfnumber(res, "segrelid");
	i_segreltype = PQfnumber(res, "segreltype");
	i_blkdirrelid = PQfnumber(res, "blkdirrelid");
	i_blkdirreltype = PQfnumber(res, "blkdirreltype");
	i_blkdiridxid = PQfnumber(res, "blkdiridxid");
	i_visimaprelid = PQfnumber(res, "visimaprelid");
	i_visimapreltype = PQfnumber(res, "visimapreltype");
	i_visimapidxid = PQfnumber(res, "visimapidxid");

	for (int i = 0; i < ntups; i++)
	{
		TableInfo *tbinfo = findTableByOid(atooid(PQgetvalue(res, i, i_oid)));
		if (tbinfo)
		{
			aotblinfo = (AOTableInfo *) pg_malloc(sizeof(AOTableInfo));
			aotblinfo->columnstore = (strcmp(PQgetvalue(res, i, i_columnstore), "t") == 0);
			aotblinfo->segrelid = atooid(PQgetvalue(res, i, i_segrelid));
			aotblinfo->segreltype = atooid(PQgetvalue(res, i, i_segreltype));
			aotblinfo->blkdirrelid = atooid(PQgetvalue(res, i, i_blkdirrelid));
			aotblinfo->blkdirreltype = atooid(PQgetvalue(res, i, i_blkdirreltype));
			aotblinfo->blkdiridxid = atooid(PQgetvalue(res, i, i_blkdiridxid));
			aotblinfo->visimaprelid = atooid(PQgetvalue(res, i, i_visimaprelid));
			aotblinfo->visimapreltype = atooid(PQgetvalue(res, i, i_visimapreltype));
			aotblinfo->visimapidxid = atooid(PQgetvalue(res, i, i_visimapidxid));
			tbinfo->aotbl = aotblinfo;
		}
	}
	PQclear(res);

	destroyPQExpBuffer(query);
}

void
getBMIndxInfo(Archive *fout)
{
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int		ntups;
	BMIndxInfo *bmindxinfo;
	int		i_indexrelid;
	int		i_bmrelid;
	int		i_bmreltype;
	int		i_bmidxid;
	Oid bitmap_index_namespace;

	/* On GPDB5 pg_bitmapindex OID is 3012 */
	bitmap_index_namespace = fout->remoteVersion >= 90400 ? PG_BITMAPINDEX_NAMESPACE : 3012;

	resetPQExpBuffer(query);

	appendPQExpBuffer(query,
					  "SELECT "
						"bm.indexrelid, bm.bmrelid, bm.bmreltype, bmidx.bmidxid\n"
						"FROM (SELECT substring(relname FROM \'\\d+\')::oid as indexrelid, "
						"			oid as bmrelid, reltype as bmreltype\n "
						"			FROM pg_class\n"
						"			WHERE relnamespace='%u' AND relkind='r') as bm\n"
						"JOIN (SELECT substring(relname FROM \'\\d+\')::oid as indexrelid, "
						"			oid as bmidxid\n"
						"			FROM pg_class\n"
						"			WHERE relnamespace='%u' AND relkind='i') as bmidx "
						"ON (bmidx.indexrelid=bm.indexrelid)\n"
						"ORDER BY 1",
						bitmap_index_namespace, bitmap_index_namespace);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_indexrelid = PQfnumber(res, "indexrelid");
	i_bmrelid =  PQfnumber(res, "bmrelid");
	i_bmreltype = PQfnumber(res, "bmreltype");
	i_bmidxid = PQfnumber(res, "bmidxid");

	for (int i = 0; i < ntups; i++)
	{
		IndxInfo *indxinfo = findIndexByOid(atooid(PQgetvalue(res, i, i_indexrelid)));
		if (indxinfo)
		{
			bmindxinfo = (BMIndxInfo *) pg_malloc(sizeof(BMIndxInfo));
			bmindxinfo->bmrelid = atooid(PQgetvalue(res, i, i_bmrelid));
			bmindxinfo->bmreltype = atooid(PQgetvalue(res, i, i_bmreltype));
			bmindxinfo->bmidxid = atooid(PQgetvalue(res, i, i_bmidxid));
			indxinfo->bmidx = bmindxinfo;
		}
	}
	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * getOwnedSeqs
 *	  identify owned sequences and mark them as dumpable if owning table is
 *
 * We used to do this in getTables(), but it's better to do it after the
 * index used by findTableByOid() has been set up.
 */
void
getOwnedSeqs(Archive *fout, TableInfo tblinfo[], int numTables)
{
	int			i;

	/*
	 * Force sequences that are "owned" by table columns to be dumped whenever
	 * their owning table is being dumped.
	 */
	for (i = 0; i < numTables; i++)
	{
		TableInfo  *seqinfo = &tblinfo[i];
		TableInfo  *owning_tab;

		if (!OidIsValid(seqinfo->owning_tab))
			continue;			/* not an owned sequence */
		if (seqinfo->dobj.dump)
			continue;			/* no need to search */
		owning_tab = findTableByOid(seqinfo->owning_tab);
		if (owning_tab && owning_tab->dobj.dump)
		{
			seqinfo->interesting = true;
			seqinfo->dobj.dump = true;
		}
	}
}

/*
 * getInherits
 *	  read all the inheritance information
 * from the system catalogs return them in the InhInfo* structure
 *
 * numInherits is set to the number of pairs read in
 */
InhInfo *
getInherits(Archive *fout, int *numInherits)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	InhInfo    *inhinfo;
	int			i_inhrelid;
	int			i_inhparent;

	/* find all the inheritance information */

	appendPQExpBufferStr(query, "SELECT inhrelid, inhparent FROM pg_inherits");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numInherits = ntups;

	inhinfo = (InhInfo *) pg_malloc(ntups * sizeof(InhInfo));

	i_inhrelid = PQfnumber(res, "inhrelid");
	i_inhparent = PQfnumber(res, "inhparent");

	for (i = 0; i < ntups; i++)
	{
		inhinfo[i].inhrelid = atooid(PQgetvalue(res, i, i_inhrelid));
		inhinfo[i].inhparent = atooid(PQgetvalue(res, i, i_inhparent));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return inhinfo;
}

/*
 * getIndexes
 *	  get information about every index on a dumpable table
 *
 * Note: index data is not returned directly to the caller, but it
 * does get entered into the DumpableObject tables.
 */
void
getIndexes(Archive *fout, TableInfo tblinfo[], int numTables)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer tbloids = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			curtblindx;
	IndxInfo   *indxinfo;
	int			i_tableoid,
				i_oid,
				i_indrelid,
				i_indexname,
				i_indexdef,
				i_indnkeys,
				i_indkey,
				i_indisclustered,
				i_indisreplident,
				i_contype,
				i_conname,
				i_condeferrable,
				i_condeferred,
				i_contableoid,
				i_conoid,
				i_condef,
				i_tablespace,
				i_indreloptions,
				i_relpages;

	/*
	 * We want to perform just one query against pg_index.  However, we
	 * mustn't try to select every row of the catalog and then sort it out on
	 * the client side, because some of the server-side functions we need
	 * would be unsafe to apply to tables we don't have lock on.  Hence, we
	 * build an array of the OIDs of tables we care about (and now have lock
	 * on!), and use a WHERE clause to constrain which rows are selected.
	 */
	appendPQExpBufferChar(tbloids, '{');
	for (int i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		if (!tbinfo->hasindex)
			continue;

		/*
		 * We can ignore indexes of uninteresting tables.
		 */
		if (!tbinfo->interesting)
			continue;

		/* OK, we need info for this table */
		if (tbloids->len > 1)	/* do we have more than the '{'? */
			appendPQExpBufferChar(tbloids, ',');
		appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);
	}
	appendPQExpBufferChar(tbloids, '}');

	resetPQExpBuffer(query);

	appendPQExpBuffer(query,
					  "SELECT t.tableoid, t.oid, i.indrelid, "
					  "t.relname AS indexname, "
					  "pg_catalog.pg_get_indexdef(i.indexrelid) AS indexdef, "
					  "t.relnatts AS indnkeys, "
					  "i.indkey, i.indisclustered, "
					  "c.contype, c.conname, "
					  "c.condeferrable, c.condeferred, "
					  "c.tableoid AS contableoid, "
					  "c.oid AS conoid, t.relpages, "
					  "pg_catalog.pg_get_constraintdef(c.oid, false) AS condef, "
					  "(SELECT spcname FROM pg_catalog.pg_tablespace s WHERE s.oid = t.reltablespace) AS tablespace, "
					  "t.reloptions AS indreloptions, ");

	if (fout->remoteVersion >= 90400)
		appendPQExpBuffer(query,
						  "i.indisreplident ");
	else
		appendPQExpBuffer(query,
						  "false AS indisreplident ");

	appendPQExpBuffer(query,
							"FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
							"JOIN pg_catalog.pg_index i ON (src.tbloid = i.indrelid) "
							"JOIN pg_catalog.pg_class t ON (t.oid = i.indexrelid) ",
							tbloids->data);
	/*
	 * The point of the messy-looking outer join is to find a constraint that
	 * is related by an internal dependency link to the index. If we find one,
	 * create a CONSTRAINT entry linked to the INDEX entry.  We assume an
	 * index won't have more than one internal dependency.
	 *
	 * As of 9.0 we don't need to look at pg_depend but can check for a match
	 * to pg_constraint.conindid.  The check on conrelid is redundant but
	 * useful because that column is indexed while conindid is not.
	 */

	if (fout->remoteVersion >= 90400)
	{
		/*
		 * the test on indisready is necessary in 9.2, and harmless in
		 * earlier/later versions
		 */
		appendPQExpBuffer(query,
						  "LEFT JOIN pg_catalog.pg_constraint c "
						  "ON (i.indrelid = c.conrelid AND "
						  "i.indexrelid = c.conindid AND "
						  "c.contype IN ('p','u','x')) "
						  "WHERE i.indisvalid AND i.indisready "
						  "ORDER BY i.indrelid, indexname");
	}
	else
	{
		appendPQExpBuffer(query,
						  "LEFT JOIN pg_catalog.pg_depend d "
						  "ON (d.classid = t.tableoid "
						  "AND d.objid = t.oid "
						  "AND d.deptype = 'i') "
						  "LEFT JOIN pg_catalog.pg_constraint c "
						  "ON (d.refclassid = c.tableoid "
						  "AND d.refobjid = c.oid) "
						  "WHERE i.indisvalid "
						  "ORDER BY i.indrelid, indexname");
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_indrelid = PQfnumber(res, "indrelid");
	i_indexname = PQfnumber(res, "indexname");
	i_indexdef = PQfnumber(res, "indexdef");
	i_indnkeys = PQfnumber(res, "indnkeys");
	i_indkey = PQfnumber(res, "indkey");
	i_indisclustered = PQfnumber(res, "indisclustered");
	i_indisreplident = PQfnumber(res, "indisreplident");
	i_relpages = PQfnumber(res, "relpages");
	i_contype = PQfnumber(res, "contype");
	i_conname = PQfnumber(res, "conname");
	i_condeferrable = PQfnumber(res, "condeferrable");
	i_condeferred = PQfnumber(res, "condeferred");
	i_contableoid = PQfnumber(res, "contableoid");
	i_conoid = PQfnumber(res, "conoid");
	i_condef = PQfnumber(res, "condef");
	i_tablespace = PQfnumber(res, "tablespace");
	i_indreloptions = PQfnumber(res, "indreloptions");

	indxinfo = (IndxInfo *) pg_malloc(ntups * sizeof(IndxInfo));

	/*
	 * Outer loop iterates once per table, not once per row.  Incrementing of
	 * j is handled by the inner loop.
	 */
	curtblindx = -1;
	for (int j = 0; j < ntups;)
	{
		Oid			indrelid = atooid(PQgetvalue(res, j, i_indrelid));
		TableInfo  *tbinfo = NULL;
		int			numinds;

		/* Count rows for this table */
		for (numinds = 1; numinds < ntups - j; numinds++)
			if (atooid(PQgetvalue(res, j + numinds, i_indrelid)) != indrelid)
				break;

		/*
		 * Locate the associated TableInfo; we rely on tblinfo[] being in OID
		 * order.
		 */
		while (++curtblindx < numTables)
		{
			tbinfo = &tblinfo[curtblindx];
			if (tbinfo->dobj.catId.oid == indrelid)
				break;
		}
		if (curtblindx >= numTables)
			exit_horribly(NULL, "unrecognized table OID %u\n", indrelid);
		/* cross-check that we only got requested tables */
		if (!tbinfo->hasindex ||
			!tbinfo->interesting)
			exit_horribly(NULL, "unexpected index data for table \"%s\"\n",
				  tbinfo->dobj.name);

		for (int c = 0; c < numinds; c++, j++)
		{
			char		contype;

			indxinfo[j].dobj.objType = DO_INDEX;
			indxinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
			indxinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
			AssignDumpId(&indxinfo[j].dobj);
			indxinfo[j].dobj.dump = tbinfo->dobj.dump;
			indxinfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_indexname));
			indxinfo[j].dobj.namespace = tbinfo->dobj.namespace;
			indxinfo[j].indextable = tbinfo;
			indxinfo[j].indexdef = pg_strdup(PQgetvalue(res, j, i_indexdef));
			indxinfo[j].indnkeys = atoi(PQgetvalue(res, j, i_indnkeys));
			indxinfo[j].tablespace = pg_strdup(PQgetvalue(res, j, i_tablespace));
			indxinfo[j].indreloptions = pg_strdup(PQgetvalue(res, j, i_indreloptions));

			/*
			 * In pre-7.4 releases, indkeys may contain more entries than
			 * indnkeys says (since indnkeys will be 1 for a functional
			 * index).  We don't actually care about this case since we don't
			 * examine indkeys except for indexes associated with PRIMARY and
			 * UNIQUE constraints, which are never functional indexes. But we
			 * have to allocate enough space to keep parseOidArray from
			 * complaining.
			 */
			indxinfo[j].indkeys = (Oid *) pg_malloc(INDEX_MAX_KEYS * sizeof(Oid));
			parseOidArray(PQgetvalue(res, j, i_indkey),
						  indxinfo[j].indkeys, INDEX_MAX_KEYS);
			indxinfo[j].indisclustered = (PQgetvalue(res, j, i_indisclustered)[0] == 't');
			indxinfo[j].indisreplident = (PQgetvalue(res, j, i_indisreplident)[0] == 't');
			indxinfo[j].relpages = atoi(PQgetvalue(res, j, i_relpages));
			contype = *(PQgetvalue(res, j, i_contype));

			if (contype == 'p' || contype == 'u' || contype == 'x')
			{
				/*
				 * If we found a constraint matching the index, create an
				 * entry for it.
				 */
				ConstraintInfo *constrinfo;

				constrinfo = (ConstraintInfo *) pg_malloc(sizeof(ConstraintInfo));
				constrinfo->dobj.objType = DO_CONSTRAINT;
				constrinfo->dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
				constrinfo->dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
				AssignDumpId(&constrinfo->dobj);
				constrinfo->dobj.dump = tbinfo->dobj.dump;
				constrinfo->dobj.name = pg_strdup(PQgetvalue(res, j, i_conname));
				constrinfo->dobj.namespace = tbinfo->dobj.namespace;
				constrinfo->contable = tbinfo;
				constrinfo->condomain = NULL;
				constrinfo->contype = contype;
				if (contype == 'x')
					constrinfo->condef = pg_strdup(PQgetvalue(res, j, i_condef));
				else
					constrinfo->condef = NULL;
				constrinfo->confrelid = InvalidOid;
				constrinfo->conindex = indxinfo[j].dobj.dumpId;
				constrinfo->condeferrable = *(PQgetvalue(res, j, i_condeferrable)) == 't';
				constrinfo->condeferred = *(PQgetvalue(res, j, i_condeferred)) == 't';
				constrinfo->conislocal = true;
				constrinfo->separate = true;

				indxinfo[j].indexconstraint = constrinfo->dobj.dumpId;
			}
			else
			{
				/* Plain secondary index */
				indxinfo[j].indexconstraint = 0;
			}
			/* Bitmap index metadata is collected in getBMIndxInfo */
			indxinfo[j].bmidx = NULL;
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(tbloids);
}

/*
 * getConstraints
 *
 * Get info about constraints on dumpable tables.
 *
 * Currently handles foreign keys only.
 * Unique and primary key constraints are handled with indexes,
 * while check constraints are processed in getTableAttrs().
 */
void
getConstraints(Archive *fout, TableInfo tblinfo[], int numTables)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer tbloids = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			curtblindx;
	TableInfo  *tbinfo = NULL;
	ConstraintInfo *constrinfo;
	int			i_contableoid,
				i_conoid,
				i_conrelid,
				i_conname,
				i_confrelid,
				i_condef;

	/*
	 * We want to perform just one query against pg_constraint.  However, we
	 * mustn't try to select every row of the catalog and then sort it out on
	 * the client side, because some of the server-side functions we need
	 * would be unsafe to apply to tables we don't have lock on.  Hence, we
	 * build an array of the OIDs of tables we care about (and now have lock
	 * on!), and use a WHERE clause to constrain which rows are selected.
	 */
	appendPQExpBufferChar(tbloids, '{');
	for (int i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		if (!tbinfo->hastriggers || !tbinfo->dobj.dump)
			continue;

		/* OK, we need info for this table */
		if (tbloids->len > 1)	/* do we have more than the '{'? */
			appendPQExpBufferChar(tbloids, ',');
		appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);
	}
	appendPQExpBufferChar(tbloids, '}');

	appendPQExpBuffer(query,
						"SELECT c.tableoid, c.oid, c.conrelid, conname, confrelid, "
						"pg_catalog.pg_get_constraintdef(c.oid) AS condef "
					  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
					  "JOIN pg_catalog.pg_constraint c ON (src.tbloid = c.conrelid)\n"
						"AND contype = 'f'",
						tbloids->data);

	appendPQExpBufferStr(query,
						 "ORDER BY conrelid, conname");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_contableoid = PQfnumber(res, "tableoid");
	i_conoid = PQfnumber(res, "oid");
	i_conrelid = PQfnumber(res, "conrelid");
	i_conname = PQfnumber(res, "conname");
	i_confrelid = PQfnumber(res, "confrelid");
	i_condef = PQfnumber(res, "condef");

	constrinfo = (ConstraintInfo *) pg_malloc(ntups * sizeof(ConstraintInfo));

	curtblindx = -1;
	for (int j = 0; j < ntups; j++)
	{
		Oid			conrelid = atooid(PQgetvalue(res, j, i_conrelid));
		/*
		 * Locate the associated TableInfo; we rely on tblinfo[] being in OID
		 * order.
		 */
		if (tbinfo == NULL || tbinfo->dobj.catId.oid != conrelid)
		{
			while (++curtblindx < numTables)
			{
				tbinfo = &tblinfo[curtblindx];
				if (tbinfo->dobj.catId.oid == conrelid)
					break;
			}
			if (curtblindx >= numTables)
				exit_horribly(NULL, "unrecognized table OID %u\n", conrelid);
		}

		constrinfo[j].dobj.objType = DO_FK_CONSTRAINT;
		constrinfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_contableoid));
		constrinfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_conoid));
		AssignDumpId(&constrinfo[j].dobj);
		constrinfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_conname));
		constrinfo[j].dobj.namespace = tbinfo->dobj.namespace;
		constrinfo[j].contable = tbinfo;
		constrinfo[j].condomain = NULL;
		constrinfo[j].contype = 'f';
		constrinfo[j].condef = pg_strdup(PQgetvalue(res, j, i_condef));
		constrinfo[j].confrelid = atooid(PQgetvalue(res, j, i_confrelid));
		constrinfo[j].conindex = 0;
		constrinfo[j].condeferrable = false;
		constrinfo[j].condeferred = false;
		constrinfo[j].conislocal = true;
		constrinfo[j].separate = true;
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(tbloids);
}

/*
 * getDomainConstraints
 *
 * Get info about constraints on a domain.
 */
static void
getDomainConstraints(Archive *fout, TypeInfo *tyinfo)
{
	int			i;
	ConstraintInfo *constrinfo;
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			i_tableoid,
				i_oid,
				i_conname,
				i_consrc;
	int			ntups;

	if (!fout->is_prepared[PREPQUERY_GETDOMAINCONSTRAINTS])
	{
		/* Set up query for constraint-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE getDomainConstraints(pg_catalog.oid) AS\n");

		if (fout->remoteVersion >= 90100)
			appendPQExpBufferStr(query, "SELECT tableoid, oid, conname, "
								 "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
								 "convalidated "
								 "FROM pg_catalog.pg_constraint "
								 "WHERE contypid = $1 "
								 "ORDER BY conname");
		else
			appendPQExpBufferStr(query, "SELECT tableoid, oid, conname, "
								 "pg_catalog.pg_get_constraintdef(oid) AS consrc, "
								 "true as convalidated "
								 "FROM pg_catalog.pg_constraint "
								 "WHERE contypid = $1 "
								 "ORDER BY conname");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_GETDOMAINCONSTRAINTS] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE getDomainConstraints('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_conname = PQfnumber(res, "conname");
	i_consrc = PQfnumber(res, "consrc");

	constrinfo = (ConstraintInfo *) pg_malloc(ntups * sizeof(ConstraintInfo));

	tyinfo->nDomChecks = ntups;
	tyinfo->domChecks = constrinfo;

	for (i = 0; i < ntups; i++)
	{
		bool		validated = PQgetvalue(res, i, 4)[0] == 't';

		constrinfo[i].dobj.objType = DO_CONSTRAINT;
		constrinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		constrinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&constrinfo[i].dobj);
		constrinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_conname));
		constrinfo[i].dobj.namespace = tyinfo->dobj.namespace;
		constrinfo[i].contable = NULL;
		constrinfo[i].condomain = tyinfo;
		constrinfo[i].contype = 'c';
		constrinfo[i].condef = pg_strdup(PQgetvalue(res, i, i_consrc));
		constrinfo[i].confrelid = InvalidOid;
		constrinfo[i].conindex = 0;
		constrinfo[i].condeferrable = false;
		constrinfo[i].condeferred = false;
		constrinfo[i].conislocal = true;

		constrinfo[i].separate = !validated;

		/*
		 * Make the domain depend on the constraint, ensuring it won't be
		 * output till any constraint dependencies are OK.  If the constraint
		 * has not been validated, it's going to be dumped after the domain
		 * anyway, so this doesn't matter.
		 */
		if (validated)
			addObjectDependency(&tyinfo->dobj,
								constrinfo[i].dobj.dumpId);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * getRules
 *	  get basic information about every rule in the system
 *
 * numRules is set to the number of rules read in
 */
RuleInfo *
getRules(Archive *fout, int *numRules)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	RuleInfo   *ruleinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_rulename;
	int			i_ruletable;
	int			i_ev_type;
	int			i_is_instead;
	int			i_ev_enabled;

	appendPQExpBufferStr(query, "SELECT "
							"tableoid, oid, rulename, "
							"ev_class AS ruletable, ev_type, is_instead, "
							"ev_enabled "
							"FROM pg_rewrite "
							"ORDER BY oid");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numRules = ntups;

	ruleinfo = (RuleInfo *) pg_malloc(ntups * sizeof(RuleInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_rulename = PQfnumber(res, "rulename");
	i_ruletable = PQfnumber(res, "ruletable");
	i_ev_type = PQfnumber(res, "ev_type");
	i_is_instead = PQfnumber(res, "is_instead");
	i_ev_enabled = PQfnumber(res, "ev_enabled");

	for (i = 0; i < ntups; i++)
	{
		Oid			ruletableoid;

		ruleinfo[i].dobj.objType = DO_RULE;
		ruleinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		ruleinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&ruleinfo[i].dobj);
		ruleinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_rulename));
		ruletableoid = atooid(PQgetvalue(res, i, i_ruletable));
		ruleinfo[i].ruletable = findTableByOid(ruletableoid);
		if (ruleinfo[i].ruletable == NULL)
			exit_horribly(NULL, "failed sanity check, parent table OID %u of pg_rewrite entry OID %u not found\n",
						  ruletableoid, ruleinfo[i].dobj.catId.oid);
		ruleinfo[i].dobj.namespace = ruleinfo[i].ruletable->dobj.namespace;
		ruleinfo[i].dobj.dump = ruleinfo[i].ruletable->dobj.dump;
		ruleinfo[i].ev_type = *(PQgetvalue(res, i, i_ev_type));
		ruleinfo[i].is_instead = *(PQgetvalue(res, i, i_is_instead)) == 't';
		ruleinfo[i].ev_enabled = *(PQgetvalue(res, i, i_ev_enabled));
		if (ruleinfo[i].ruletable)
		{
			/*
			 * If the table is a view or materialized view, force its ON
			 * SELECT rule to be sorted before the view itself --- this
			 * ensures that any dependencies for the rule affect the table's
			 * positioning. Other rules are forced to appear after their
			 * table.
			 */
			if ((ruleinfo[i].ruletable->relkind == RELKIND_VIEW ||
				 ruleinfo[i].ruletable->relkind == RELKIND_MATVIEW) &&
				ruleinfo[i].ev_type == '1' && ruleinfo[i].is_instead)
			{
				addObjectDependency(&ruleinfo[i].ruletable->dobj,
									ruleinfo[i].dobj.dumpId);
				/* We'll merge the rule into CREATE VIEW, if possible */
				ruleinfo[i].separate = false;
			}
			else
			{
				addObjectDependency(&ruleinfo[i].dobj,
									ruleinfo[i].ruletable->dobj.dumpId);
				ruleinfo[i].separate = true;
			}
		}
		else
			ruleinfo[i].separate = true;
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return ruleinfo;
}

/*
 * getTriggers
 *	  get information about every trigger on a dumpable table
 *
 * Note: trigger data is not returned directly to the caller, but it
 * does get entered into the DumpableObject tables.
 */
void
getTriggers(Archive *fout, TableInfo tblinfo[], int numTables)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer tbloids = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			curtblindx;
	TriggerInfo *tginfo;
	int			i_tableoid,
				i_oid,
				i_tgrelid,
				i_tgname,
				i_tgfname,
				i_tgtype,
				i_tgnargs,
				i_tgargs,
				i_tgisconstraint,
				i_tgconstrname,
				i_tgconstrrelid,
				i_tgconstrrelname,
				i_tgenabled,
				i_tgdeferrable,
				i_tginitdeferred,
				i_tgdef;

	/*
	 * We want to perform just one query against pg_trigger.  However, we
	 * mustn't try to select every row of the catalog and then sort it out on
	 * the client side, because some of the server-side functions we need
	 * would be unsafe to apply to tables we don't have lock on.  Hence, we
	 * build an array of the OIDs of tables we care about (and now have lock
	 * on!), and use a WHERE clause to constrain which rows are selected.
	 */
	appendPQExpBufferChar(tbloids, '{');
	for (int i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		if (!tbinfo->hastriggers || !tbinfo->dobj.dump)
			continue;

		/* OK, we need info for this table */
		if (tbloids->len > 1)	/* do we have more than the '{'? */
			appendPQExpBufferChar(tbloids, ',');
		appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);
	}
	appendPQExpBufferChar(tbloids, '}');


	resetPQExpBuffer(query);
  if (fout->remoteVersion >= 90000)
	{
		/* See above about pretty=true in pg_get_triggerdef */
		appendPQExpBuffer(query,
						  "SELECT t.tgrelid, t.tgname, "
						  "t.tgfoid::pg_catalog.regproc AS tgfname, "
						  "pg_catalog.pg_get_triggerdef(t.oid, false) AS tgdef, "
						  "t.tgenabled, t.tableoid, t.oid "
						  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
						  "JOIN pg_catalog.pg_trigger t ON (src.tbloid = t.tgrelid) "
						  "WHERE NOT t.tgisinternal "
						  "ORDER BY t.tgrelid, t.tgname",
						  tbloids->data);
	}
	else
	{
		/*
		 * We ignore triggers that are tied to a foreign-key constraint
		 */
		appendPQExpBuffer(query,
						  "SELECT t.tgrelid, tgname, "
						  "tgfoid::pg_catalog.regproc AS tgfname, "
						  "tgtype, tgnargs, tgargs, tgenabled, "
						  "false as tgisinternal, "
						  "tgisconstraint, tgconstrname, tgdeferrable, "
						  "tgconstrrelid, tginitdeferred, t.tableoid, t.oid, "
						  "tgconstrrelid::pg_catalog.regclass AS tgconstrrelname "
						  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
						  "JOIN pg_catalog.pg_trigger t ON (src.tbloid = t.tgrelid) "
						  "WHERE tgconstraint = 0 "
						  "ORDER BY t.tgrelid, t.tgname",
							tbloids->data);
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_tgrelid = PQfnumber(res, "tgrelid");
	i_tgname = PQfnumber(res, "tgname");
	i_tgfname = PQfnumber(res, "tgfname");
	i_tgtype = PQfnumber(res, "tgtype");
	i_tgnargs = PQfnumber(res, "tgnargs");
	i_tgargs = PQfnumber(res, "tgargs");
	i_tgisconstraint = PQfnumber(res, "tgisconstraint");
	i_tgconstrname = PQfnumber(res, "tgconstrname");
	i_tgconstrrelid = PQfnumber(res, "tgconstrrelid");
	i_tgconstrrelname = PQfnumber(res, "tgconstrrelname");
	i_tgenabled = PQfnumber(res, "tgenabled");
	i_tgdeferrable = PQfnumber(res, "tgdeferrable");
	i_tginitdeferred = PQfnumber(res, "tginitdeferred");
	i_tgdef = PQfnumber(res, "tgdef");

	tginfo = (TriggerInfo *) pg_malloc(ntups * sizeof(TriggerInfo));

	/*
	 * Outer loop iterates once per table, not once per row.  Incrementing of
	 * j is handled by the inner loop.
	 */
	curtblindx = -1;
	for (int j = 0; j < ntups;)
	{
		Oid			tgrelid = atooid(PQgetvalue(res, j, i_tgrelid));
		TableInfo  *tbinfo = NULL;
		int			numtrigs;

		/* Count rows for this table */
		for (numtrigs = 1; numtrigs < ntups - j; numtrigs++)
			if (atooid(PQgetvalue(res, j + numtrigs, i_tgrelid)) != tgrelid)
				break;

		/*
		 * Locate the associated TableInfo; we rely on tblinfo[] being in OID
		 * order.
		 */
		while (++curtblindx < numTables)
		{
			tbinfo = &tblinfo[curtblindx];
			if (tbinfo->dobj.catId.oid == tgrelid)
				break;
		}
		if (curtblindx >= numTables)
			exit_horribly(NULL, "unrecognized table OID %u\n", tgrelid);

		/* Save data for this table */
		tbinfo->triggers = tginfo + j;
		tbinfo->numTriggers = numtrigs;

		for (int c = 0; c < numtrigs; c++, j++)
		{
			tginfo[j].dobj.objType = DO_TRIGGER;
			tginfo[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
			tginfo[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
			AssignDumpId(&tginfo[j].dobj);
			tginfo[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_tgname));
			tginfo[j].dobj.namespace = tbinfo->dobj.namespace;
			tginfo[j].tgtable = tbinfo;
			tginfo[j].tgenabled = *(PQgetvalue(res, j, i_tgenabled));
			if (i_tgdef >= 0)
			{
				tginfo[j].tgdef = pg_strdup(PQgetvalue(res, j, i_tgdef));

				/* remaining fields are not valid if we have tgdef */
				tginfo[j].tgfname = NULL;
				tginfo[j].tgtype = 0;
				tginfo[j].tgnargs = 0;
				tginfo[j].tgargs = NULL;
				tginfo[j].tgisconstraint = false;
				tginfo[j].tgdeferrable = false;
				tginfo[j].tginitdeferred = false;
				tginfo[j].tgconstrname = NULL;
				tginfo[j].tgconstrrelid = InvalidOid;
				tginfo[j].tgconstrrelname = NULL;
			}
			else
			{
				tginfo[j].tgdef = NULL;

				tginfo[j].tgfname = pg_strdup(PQgetvalue(res, j, i_tgfname));
				tginfo[j].tgtype = atoi(PQgetvalue(res, j, i_tgtype));
				tginfo[j].tgnargs = atoi(PQgetvalue(res, j, i_tgnargs));
				tginfo[j].tgargs = pg_strdup(PQgetvalue(res, j, i_tgargs));
				tginfo[j].tgisconstraint = *(PQgetvalue(res, j, i_tgisconstraint)) == 't';
				tginfo[j].tgdeferrable = *(PQgetvalue(res, j, i_tgdeferrable)) == 't';
				tginfo[j].tginitdeferred = *(PQgetvalue(res, j, i_tginitdeferred)) == 't';

				if (tginfo[j].tgisconstraint)
				{
					tginfo[j].tgconstrname = pg_strdup(PQgetvalue(res, j, i_tgconstrname));
					tginfo[j].tgconstrrelid = atooid(PQgetvalue(res, j, i_tgconstrrelid));
					if (OidIsValid(tginfo[j].tgconstrrelid))
					{
						if (PQgetisnull(res, j, i_tgconstrrelname))
							exit_horribly(NULL, "query produced null referenced table name for foreign key trigger \"%s\" on table \"%s\" (OID of table: %u)\n",
								  tginfo[j].dobj.name,
								  tbinfo->dobj.name,
								  tginfo[j].tgconstrrelid);
						tginfo[j].tgconstrrelname = pg_strdup(PQgetvalue(res, j, i_tgconstrrelname));
					}
					else
						tginfo[j].tgconstrrelname = NULL;
				}
				else
				{
					tginfo[j].tgconstrname = NULL;
					tginfo[j].tgconstrrelid = InvalidOid;
					tginfo[j].tgconstrrelname = NULL;
				}
			}
		}
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(tbloids);
}

/*
 * getEventTriggers
 *	  get information about event triggers
 */
EventTriggerInfo *
getEventTriggers(Archive *fout, int *numEventTriggers)
{
	int			i;
	PQExpBuffer query;
	PGresult   *res;
	EventTriggerInfo *evtinfo;
	int			i_tableoid,
				i_oid,
				i_evtname,
				i_evtevent,
				i_evtowner,
				i_evttags,
				i_evtfname,
				i_evtenabled;
	int			ntups;

	/* Before 9.3, there are no event triggers */
	if (fout->remoteVersion < 90300)
	{
		*numEventTriggers = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	appendPQExpBuffer(query,
					  "SELECT e.tableoid, e.oid, evtname, evtenabled, "
					  "evtevent, evtowner, "
					  "array_to_string(array("
					  "select quote_literal(x) "
					  " from unnest(evttags) as t(x)), ', ') as evttags, "
					  "e.evtfoid::regproc as evtfname "
					  "FROM pg_event_trigger e "
					  "ORDER BY e.oid");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numEventTriggers = ntups;

	evtinfo = (EventTriggerInfo *) pg_malloc(ntups * sizeof(EventTriggerInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_evtname = PQfnumber(res, "evtname");
	i_evtevent = PQfnumber(res, "evtevent");
	i_evtowner = PQfnumber(res, "evtowner");
	i_evttags = PQfnumber(res, "evttags");
	i_evtfname = PQfnumber(res, "evtfname");
	i_evtenabled = PQfnumber(res, "evtenabled");

	for (i = 0; i < ntups; i++)
	{
		evtinfo[i].dobj.objType = DO_EVENT_TRIGGER;
		evtinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		evtinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&evtinfo[i].dobj);
		evtinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_evtname));
		evtinfo[i].evtname = pg_strdup(PQgetvalue(res, i, i_evtname));
		evtinfo[i].evtevent = pg_strdup(PQgetvalue(res, i, i_evtevent));
		evtinfo[i].evtowner = getRoleName(PQgetvalue(res, i, i_evtowner));
		evtinfo[i].evttags = pg_strdup(PQgetvalue(res, i, i_evttags));
		evtinfo[i].evtfname = pg_strdup(PQgetvalue(res, i, i_evtfname));
		evtinfo[i].evtenabled = *(PQgetvalue(res, i, i_evtenabled));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(evtinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return evtinfo;
}

/*
 * getProcLangs
 *	  get basic information about every procedural language in the system
 *
 * numProcLangs is set to the number of langs read in
 *
 * NB: this must run after getFuncs() because we assume we can do
 * findFuncByOid().
 */
ProcLangInfo *
getProcLangs(Archive *fout, int *numProcLangs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	ProcLangInfo *planginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_lanname;
	int			i_lanpltrusted;
	int			i_lanplcallfoid;
	int			i_laninline;
	int			i_lanvalidator;
	int			i_lanacl;
	int			i_lanowner;

	/*
	 * The laninline column was added in upstream 90000 but was backported to
	 * Greenplum 5, so the check needs to go further back than 90000.
	 */
	if (fout->remoteVersion >= 90000)
	{
		/* pg_language has a laninline column */
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "lanname, lanpltrusted, lanplcallfoid, "
						  "laninline, lanvalidator, lanacl, "
						  "lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid");
	}
	else
	{
		/* pg_language has a laninline column */
		/* pg_language has a lanowner column */
		appendPQExpBuffer(query, "SELECT tableoid, oid, "
						  "lanname, lanpltrusted, lanplcallfoid, "
						  "laninline, lanvalidator, lanacl, "
						  "lanowner "
						  "FROM pg_language "
						  "WHERE lanispl "
						  "ORDER BY oid");
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numProcLangs = ntups;

	planginfo = (ProcLangInfo *) pg_malloc(ntups * sizeof(ProcLangInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_lanname = PQfnumber(res, "lanname");
	i_lanpltrusted = PQfnumber(res, "lanpltrusted");
	i_lanplcallfoid = PQfnumber(res, "lanplcallfoid");
	i_laninline = PQfnumber(res, "laninline");
	i_lanvalidator = PQfnumber(res, "lanvalidator");
	i_lanacl = PQfnumber(res, "lanacl");
	i_lanowner = PQfnumber(res, "lanowner");

	for (i = 0; i < ntups; i++)
	{
		planginfo[i].dobj.objType = DO_PROCLANG;
		planginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		planginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&planginfo[i].dobj);

		planginfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_lanname));
		planginfo[i].lanpltrusted = *(PQgetvalue(res, i, i_lanpltrusted)) == 't';
		planginfo[i].lanplcallfoid = atooid(PQgetvalue(res, i, i_lanplcallfoid));
		planginfo[i].laninline = atooid(PQgetvalue(res, i, i_laninline));
		planginfo[i].lanvalidator = atooid(PQgetvalue(res, i, i_lanvalidator));
		planginfo[i].lanacl = pg_strdup(PQgetvalue(res, i, i_lanacl));
		planginfo[i].lanowner = getRoleName(PQgetvalue(res, i, i_lanowner));

		/* Decide whether we want to dump it */
		selectDumpableProcLang(&(planginfo[i]));

		/* Decide whether we want to dump it */
		selectDumpableProcLang(&(planginfo[i]));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return planginfo;
}

/*
 * getCasts
 *	  get basic information about every cast in the system
 *
 * numCasts is set to the number of casts read in
 */
CastInfo *
getCasts(Archive *fout, int *numCasts)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query = createPQExpBuffer();
	CastInfo   *castinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_castsource;
	int			i_casttarget;
	int			i_castfunc;
	int			i_castcontext;
	int			i_castmethod;

	if (fout->remoteVersion >= 80400)
	{
		appendPQExpBufferStr(query, "SELECT tableoid, oid, "
							 "castsource, casttarget, castfunc, castcontext, "
							 "castmethod "
							 "FROM pg_cast ORDER BY 3,4");
	}
	else
	{
		appendPQExpBufferStr(query, "SELECT tableoid, oid, "
							 "castsource, casttarget, castfunc, castcontext, "
				"CASE WHEN castfunc = 0 THEN 'b' ELSE 'f' END AS castmethod "
							 "FROM pg_cast ORDER BY 3,4");
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	*numCasts = ntups;

	castinfo = (CastInfo *) pg_malloc(ntups * sizeof(CastInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_castsource = PQfnumber(res, "castsource");
	i_casttarget = PQfnumber(res, "casttarget");
	i_castfunc = PQfnumber(res, "castfunc");
	i_castcontext = PQfnumber(res, "castcontext");
	i_castmethod = PQfnumber(res, "castmethod");

	for (i = 0; i < ntups; i++)
	{
		PQExpBufferData namebuf;
		TypeInfo   *sTypeInfo;
		TypeInfo   *tTypeInfo;

		castinfo[i].dobj.objType = DO_CAST;
		castinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		castinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&castinfo[i].dobj);
		castinfo[i].castsource = atooid(PQgetvalue(res, i, i_castsource));
		castinfo[i].casttarget = atooid(PQgetvalue(res, i, i_casttarget));
		castinfo[i].castfunc = atooid(PQgetvalue(res, i, i_castfunc));
		castinfo[i].castcontext = *(PQgetvalue(res, i, i_castcontext));
		castinfo[i].castmethod = *(PQgetvalue(res, i, i_castmethod));

		/*
		 * Try to name cast as concatenation of typnames.  This is only used
		 * for purposes of sorting.  If we fail to find either type, the name
		 * will be an empty string.
		 */
		initPQExpBuffer(&namebuf);
		sTypeInfo = findTypeByOid(castinfo[i].castsource);
		tTypeInfo = findTypeByOid(castinfo[i].casttarget);
		if (sTypeInfo && tTypeInfo)
			appendPQExpBuffer(&namebuf, "%s %s",
							  sTypeInfo->dobj.name, tTypeInfo->dobj.name);
		castinfo[i].dobj.name = namebuf.data;

		/* Decide whether we want to dump it */
		selectDumpableCast(&(castinfo[i]));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return castinfo;
}

/*
 * getTableAttrs -
 *	  for each interesting table, read info about its attributes
 *	  (names, types, default values, CHECK constraints, etc)
 *
 * This is implemented in a very inefficient way right now, looping
 * through the tblinfo and doing a join per table to find the attrs and their
 * types.  However, because we want type names and so forth to be named
 * relative to the schema of each table, we couldn't do it in just one
 * query.  (Maybe one query per schema?)
 *
 *	modifies tblinfo
 */
void
getTableAttrs(Archive *fout, TableInfo *tblinfo, int numTables)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer tbloids = createPQExpBuffer();
	PQExpBuffer checkoids = createPQExpBuffer();
	PGresult   *res;
	int			ntups;
	int			curtblindx;
	int			i_attrelid;
	int			i_attnum;
	int			i_attname;
	int			i_atttypname;
	int			i_atttypmod;
	int			i_attstattarget;
	int			i_attstorage;
	int			i_typstorage;
	int			i_attisdropped;
	int			i_attlen;
	int			i_attalign;
	int			i_attislocal;
	int			i_attnotnull;
	int			i_attoptions;
	int			i_attcollation;
	int			i_attfdwoptions;
	int			i_atthasdef;
	int			i_attencoding;

	/*
	 * We want to perform just one query against pg_attribute, and then just
	 * one against pg_attrdef (for DEFAULTs) and one against pg_constraint
	 * (for CHECK constraints).  However, we mustn't try to select every row
	 * of those catalogs and then sort it out on the client side, because some
	 * of the server-side functions we need would be unsafe to apply to tables
	 * we don't have lock on.  Hence, we build an array of the OIDs of tables
	 * we care about (and now have lock on!), and use a WHERE clause to
	 * constrain which rows are selected.
	 */
	appendPQExpBufferChar(tbloids, '{');
	appendPQExpBufferChar(checkoids, '{');
	for (int i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &tblinfo[i];

		/* Don't bother to collect info for sequences */
		if (tbinfo->relkind == RELKIND_SEQUENCE)
			continue;

		/* Don't bother with uninteresting tables, either */
		if (!tbinfo->interesting)
			continue;

		/* OK, we need info for this table */
		if (tbloids->len > 1)	/* do we have more than the '{'? */
			appendPQExpBufferChar(tbloids, ',');
		appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);

		if (tbinfo->ncheck > 0)
		{
			/* Also make a list of the ones with check constraints */
			if (checkoids->len > 1) /* do we have more than the '{'? */
				appendPQExpBufferChar(checkoids, ',');
			appendPQExpBuffer(checkoids, "%u", tbinfo->dobj.catId.oid);
		}
	}
	appendPQExpBufferChar(tbloids, '}');
	appendPQExpBufferChar(checkoids, '}');


	/* find all the user attributes and their types */
	appendPQExpBufferStr(q,
						 "SELECT\n"
						 "a.attrelid,\n"
						 "a.attnum,\n"
						 "a.attname,\n"
						 "a.atttypmod,\n"
						 "a.attstattarget,\n"
						 "a.attstorage,\n"
						 "t.typstorage,\n"
						 "a.attnotnull,\n"
						 "a.atthasdef,\n"
						 "a.attisdropped,\n"
						 "a.attlen,\n"
						 "a.attalign,\n"
						 "a.attislocal,\n"
						 "pg_catalog.format_type(t.oid, a.atttypmod) AS atttypname,\n"
						 "pg_catalog.array_to_string(e.attoptions, ',') AS attencoding,\n");

	if (fout->remoteVersion >= 90000)
		appendPQExpBufferStr(q,
							 "array_to_string(a.attoptions, ', ') AS attoptions,\n");
	else
		appendPQExpBufferStr(q,
							 "'' AS attoptions,\n");

	if (fout->remoteVersion >= 90100)
	{
		/*
		 * Since we only want to dump COLLATE clauses for attributes whose
		 * collation is different from their type's default, we use a CASE
		 * here to suppress uninteresting attcollations cheaply.
		 */
		appendPQExpBufferStr(q,
							 "CASE WHEN a.attcollation <> t.typcollation "
							 "THEN a.attcollation ELSE 0 END AS attcollation,\n");
	}
	else
		appendPQExpBufferStr(q,
							 "0 AS attcollation,\n");

	if (fout->remoteVersion >= 90200)
		appendPQExpBufferStr(q,
							 "pg_catalog.array_to_string(ARRAY("
							 "SELECT pg_catalog.quote_ident(option_name) || "
							 "' ' || pg_catalog.quote_literal(option_value) "
							 "FROM pg_catalog.pg_options_to_table(attfdwoptions) "
							 "ORDER BY option_name"
							 "), E',\n    ') AS attfdwoptions\n");
	else
		appendPQExpBufferStr(q,
							 "'' AS attfdwoptions\n");

	/* need left join to pg_type to not fail on dropped columns ... */
	appendPQExpBuffer(q,
					  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
					  "JOIN pg_catalog.pg_attribute a ON (src.tbloid = a.attrelid) "
					  "LEFT JOIN pg_catalog.pg_type t "
					  "ON (a.atttypid = t.oid)\n"
						"LEFT OUTER JOIN pg_catalog.pg_attribute_encoding e "
						"ON e.attrelid = a.attrelid AND e.attnum = a.attnum \n"
					  "WHERE a.attnum > 0::pg_catalog.int2\n"
					  "ORDER BY a.attrelid, a.attnum",
					  tbloids->data);

	res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_attrelid = PQfnumber(res, "attrelid");
	i_attnum = PQfnumber(res, "attnum");
	i_attname = PQfnumber(res, "attname");
	i_atttypname = PQfnumber(res, "atttypname");
	i_atttypmod = PQfnumber(res, "atttypmod");
	i_attstattarget = PQfnumber(res, "attstattarget");
	i_attstorage = PQfnumber(res, "attstorage");
	i_typstorage = PQfnumber(res, "typstorage");
	i_attisdropped = PQfnumber(res, "attisdropped");
	i_attlen = PQfnumber(res, "attlen");
	i_attalign = PQfnumber(res, "attalign");
	i_attislocal = PQfnumber(res, "attislocal");
	i_attnotnull = PQfnumber(res, "attnotnull");
	i_attoptions = PQfnumber(res, "attoptions");
	i_attcollation = PQfnumber(res, "attcollation");
	i_attfdwoptions = PQfnumber(res, "attfdwoptions");
	i_atthasdef = PQfnumber(res, "atthasdef");
	i_attencoding = PQfnumber(res, "attencoding");

	/* Within the next loop, we'll accumulate OIDs of tables with defaults */
	resetPQExpBuffer(tbloids);
	appendPQExpBufferChar(tbloids, '{');

	/*
	 * Outer loop iterates once per table, not once per row.  Incrementing of
	 * r is handled by the inner loop.
	 */
	curtblindx = -1;
	for (int r = 0; r < ntups;)
	{
		Oid			attrelid = atooid(PQgetvalue(res, r, i_attrelid));
		TableInfo  *tbinfo = NULL;
		int			numatts;
		bool		hasdefaults;
		bool		rootPartHasDroppedAttr = false;

		/* Count rows for this table */
		for (numatts = 1; numatts < ntups - r; numatts++)
			if (atooid(PQgetvalue(res, r + numatts, i_attrelid)) != attrelid)
				break;

		/*
		 * Locate the associated TableInfo; we rely on tblinfo[] being in OID
		 * order.
		 */
		while (++curtblindx < numTables)
		{
			tbinfo = &tblinfo[curtblindx];
			if (tbinfo->dobj.catId.oid == attrelid)
				break;
		}
		if (curtblindx >= numTables)
			exit_horribly(NULL, "unrecognized table OID %u\n", attrelid);
		/* cross-check that we only got requested tables */
		if (tbinfo->relkind == RELKIND_SEQUENCE ||
			!tbinfo->interesting)
			exit_horribly(NULL, "unexpected column data for table \"%s\"\n",
				  tbinfo->dobj.name);

		/* Save data for this table */
		tbinfo->numatts = numatts;
		tbinfo->attnames = (char **) pg_malloc(numatts * sizeof(char *));
		tbinfo->atttypnames = (char **) pg_malloc(numatts * sizeof(char *));
		tbinfo->atttypmod = (int *) pg_malloc(numatts * sizeof(int));
		tbinfo->attstattarget = (int *) pg_malloc(numatts * sizeof(int));
		tbinfo->attstorage = (char *) pg_malloc(numatts * sizeof(char));
		tbinfo->typstorage = (char *) pg_malloc(numatts * sizeof(char));
		tbinfo->attisdropped = (bool *) pg_malloc(numatts * sizeof(bool));
		tbinfo->attlen = (int *) pg_malloc(numatts * sizeof(int));
		tbinfo->attalign = (char *) pg_malloc(numatts * sizeof(char));
		tbinfo->attislocal = (bool *) pg_malloc(numatts * sizeof(bool));
		tbinfo->attoptions = (char **) pg_malloc(numatts * sizeof(char *));
		tbinfo->attcollation = (Oid *) pg_malloc(numatts * sizeof(Oid));
		tbinfo->attfdwoptions = (char **) pg_malloc(numatts * sizeof(char *));
		tbinfo->notnull = (bool *) pg_malloc(numatts * sizeof(bool));
		tbinfo->inhNotNull = (bool *) pg_malloc(numatts * sizeof(bool));
		tbinfo->attrdefs = (AttrDefInfo **) pg_malloc(numatts * sizeof(AttrDefInfo *));
		tbinfo->attencoding = (char **) pg_malloc(numatts * sizeof(char*));
		hasdefaults = false;

		for (int j = 0; j < numatts; j++, r++)
		{
			if (j + 1 != atoi(PQgetvalue(res, r, i_attnum)))
				exit_horribly(NULL, "invalid column numbering in table \"%s\"\n",
					  tbinfo->dobj.name);
			tbinfo->attnames[j] = pg_strdup(PQgetvalue(res, r, i_attname));
			tbinfo->atttypnames[j] = pg_strdup(PQgetvalue(res, r, i_atttypname));
			tbinfo->atttypmod[j] = atoi(PQgetvalue(res, r, i_atttypmod));
			tbinfo->attstattarget[j] = atoi(PQgetvalue(res, r, i_attstattarget));
			tbinfo->attstorage[j] = *(PQgetvalue(res, r, i_attstorage));
			tbinfo->typstorage[j] = *(PQgetvalue(res, r, i_typstorage));
			tbinfo->attisdropped[j] = (PQgetvalue(res, r, i_attisdropped)[0] == 't');
			tbinfo->attlen[j] = atoi(PQgetvalue(res, r, i_attlen));
			tbinfo->attalign[j] = *(PQgetvalue(res, r, i_attalign));
			tbinfo->attislocal[j] = (PQgetvalue(res, r, i_attislocal)[0] == 't');
			tbinfo->notnull[j] = (PQgetvalue(res, r, i_attnotnull)[0] == 't');
			tbinfo->attoptions[j] = pg_strdup(PQgetvalue(res, r, i_attoptions));
			tbinfo->attcollation[j] = atooid(PQgetvalue(res, r, i_attcollation));
			tbinfo->attfdwoptions[j] = pg_strdup(PQgetvalue(res, r, i_attfdwoptions));
			tbinfo->attrdefs[j] = NULL; /* fix below */
			if (PQgetvalue(res, r, i_atthasdef)[0] == 't')
				hasdefaults = true;
			/* these flags will be set in flagInhAttrs() */
			tbinfo->inhNotNull[j] = false;

			/* column storage attributes */
			if (!PQgetisnull(res, r, PQfnumber(res, "attencoding")))
				tbinfo->attencoding[j] = pg_strdup(PQgetvalue(res, r, PQfnumber(res, "attencoding")));
			else
				tbinfo->attencoding[j] = NULL;

			/*
			 * External table doesn't support inheritance so ensure that all
			 * attributes are marked as local.  Applicable to partitioned
			 * tables where a partition is exchanged for an external table.
			 */
			if (tbinfo->relstorage == RELSTORAGE_EXTERNAL && tbinfo->attislocal[j])
				tbinfo->attislocal[j] = false;

			/*
			 * GPDB: If root partition table has dropped column, we must do an
			 * extra check later.
			 */
			if (binary_upgrade && tbinfo->parparent && tbinfo->attisdropped[j])
				rootPartHasDroppedAttr = true;
		}

		if (hasdefaults)
		{
			/* Collect OIDs of interesting tables that have defaults */
			if (tbloids->len > 1)	/* do we have more than the '{'? */
				appendPQExpBufferChar(tbloids, ',');
			appendPQExpBuffer(tbloids, "%u", tbinfo->dobj.catId.oid);
		}

		/*
		 * GPDB: If root partition has a dropped attribute, check if its child
		 * partitions do too. If all the child partitions have the same
		 * dropped attribute then continue as normal. If none of the child
		 * partitions have a dropped attribute, we will suppress the dropped
		 * attribute from being dumped later in the CREATE TABLE PARTITION BY
		 * DDL along with the respective ALTER TABLE DROP COLUMN. Child and
		 * subroot partitions do not have their own DDL; they are completely
		 * delegated by the root partition DDL.
		 *
		 * Note: This assumes that the dropped column reference is the same
		 * between the root partition and its child partitions which should
		 * have been confirmed by `pg_upgrade --check` heterogeneous partition
		 * table check.
		 */
		if (binary_upgrade && rootPartHasDroppedAttr)
		{
			int numDistinctNatts;
			int childPartNumNatts;
			PGresult   *attsRes;

			if (g_verbose)
				write_msg(NULL, "checking if root partition table \"%s\" with dropped column(s) is synchronized with its child partitions.\n",
						  tbinfo->dobj.name);

			resetPQExpBuffer(q);
			appendPQExpBuffer(q, "SELECT DISTINCT relnatts "
							  "FROM pg_catalog.pg_class "
							  "WHERE NOT relhassubclass AND "
							  "oid IN (SELECT parchildrelid "
							  "    FROM pg_catalog.pg_partition par "
							  "    JOIN pg_catalog.pg_partition_rule rule ON par.oid=rule.paroid "
							  "        AND NOT par.paristemplate "
							  "        AND par.parrelid = '%u'::pg_catalog.oid)",
							  tbinfo->dobj.catId.oid);

			attsRes = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);
			numDistinctNatts = PQntuples(attsRes);
			Assert(numDistinctNatts > 0);

			/*
			 * We encountered a heterogeneous partition table where all the
			 * child partitions are not synchronized with the number of
			 * attributes (e.g. one has a dropped column while the others do
			 * not). This should have been fixed manually by the user before
			 * running pg_dump --binary-upgrade (most likely as part of
			 * addressing issues reported by `pg_upgrade --check`).
			 */
			if (numDistinctNatts != 1)
			{
				write_msg(NULL, "invalid heterogeneous partition table detected with root partition table \"%s\".\n",
						  tbinfo->dobj.name);
				exit_nicely(1);
			}

			/*
			 * If the number of attributes from the child partitions match the
			 * root partition then keep the dropped column reference. If they
			 * do not match, then ignore the dropped column reference when
			 * dumping the partition table DDL.
			 */
			childPartNumNatts = atoi(PQgetvalue(attsRes, 0, 0));
			if (childPartNumNatts != numatts)
			{
				if (g_verbose)
					write_msg(NULL, "suppressing dropped column(s) for root partition table \"%s\".\n",
							  tbinfo->dobj.name);
				tbinfo->ignoreRootPartDroppedAttr = true;
			}
			PQclear(attsRes);
		}
	}

	PQclear(res);

	/*
	 * Now get info about column defaults.  This is skipped for a data-only
	 * dump, as it is only needed for table schemas.
	 */
	if (!dataOnly && tbloids->len > 1)
	{
		AttrDefInfo *attrdefs;
		int			numDefaults;
		TableInfo  *tbinfo = NULL;
		
		if (g_verbose)
			write_msg(NULL, "finding table default expressions\n");

		appendPQExpBufferChar(tbloids, '}');

		printfPQExpBuffer(q, "SELECT a.tableoid, a.oid, adrelid, adnum, "
						  "pg_catalog.pg_get_expr(adbin, adrelid) AS adsrc\n"
						  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
						  "JOIN pg_catalog.pg_attrdef a ON (src.tbloid = a.adrelid)\n"
						  "ORDER BY a.adrelid, a.adnum",
						  tbloids->data);

		res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

		numDefaults = PQntuples(res);
		attrdefs = (AttrDefInfo *) pg_malloc(numDefaults * sizeof(AttrDefInfo));

		curtblindx = -1;
		for (int j = 0; j < numDefaults; j++)
		{
			Oid			adtableoid = atooid(PQgetvalue(res, j, 0));
			Oid			adoid = atooid(PQgetvalue(res, j, 1));
			Oid			adrelid = atooid(PQgetvalue(res, j, 2));
			int			adnum = atoi(PQgetvalue(res, j, 3));
			char	   *adsrc = PQgetvalue(res, j, 4);

			/*
			 * Locate the associated TableInfo; we rely on tblinfo[] being in
			 * OID order.
			 */
			if (tbinfo == NULL || tbinfo->dobj.catId.oid != adrelid)
			{
				while (++curtblindx < numTables)
				{
					tbinfo = &tblinfo[curtblindx];
					if (tbinfo->dobj.catId.oid == adrelid)
						break;
				}
				if (curtblindx >= numTables)
					exit_horribly(NULL, "unrecognized table OID %u\n", adrelid);
			}

			if (adnum <= 0 || adnum > tbinfo->numatts)
				exit_horribly(NULL, "invalid adnum value %d for table \"%s\"\n",
					  adnum, tbinfo->dobj.name);

			/*
			 * dropped columns shouldn't have defaults, but just in case,
			 * ignore 'em
			 */
			if (tbinfo->attisdropped[adnum - 1])
				continue;

			attrdefs[j].dobj.objType = DO_ATTRDEF;
			attrdefs[j].dobj.catId.tableoid = adtableoid;
			attrdefs[j].dobj.catId.oid = adoid;
			AssignDumpId(&attrdefs[j].dobj);
			attrdefs[j].adtable = tbinfo;
			attrdefs[j].adnum = adnum;
			attrdefs[j].adef_expr = pg_strdup(adsrc);

			attrdefs[j].dobj.name = pg_strdup(tbinfo->dobj.name);
			attrdefs[j].dobj.namespace = tbinfo->dobj.namespace;

			attrdefs[j].dobj.dump = tbinfo->dobj.dump;

			if (tbinfo->relkind == RELKIND_VIEW)
			{
				/*
				 * Defaults on a VIEW must always be dumped as separate ALTER
				 * TABLE commands.
				 */
				attrdefs[j].separate = true;
			}
			else if (!shouldPrintColumn(tbinfo, adnum - 1))
			{
				/* column will be suppressed, print default separately */
				attrdefs[j].separate = true;
			}
			else
			{
				attrdefs[j].separate = false;
			}

			if (!attrdefs[j].separate)
			{
				/*
				 * Mark the default as needing to appear before the table, so
				 * that any dependencies it has must be emitted before the
				 * CREATE TABLE.  If this is not possible, we'll change to
				 * "separate" mode while sorting dependencies.
				 */
				addObjectDependency(&tbinfo->dobj,
									attrdefs[j].dobj.dumpId);
			}

			tbinfo->attrdefs[adnum - 1] = &attrdefs[j];
		}

		PQclear(res);
	}

	/*
	 * Get info about table CHECK constraints.  This is skipped for a
	 * data-only dump, as it is only needed for table schemas.
	 */
	if (!dataOnly && checkoids->len > 2)
	{
		ConstraintInfo *constrs;
		int			numConstrs;
		int			i_tableoid;
		int			i_oid;
		int			i_conrelid;
		int			i_conname;
		int			i_consrc;
		int			i_conislocal;
		int			i_convalidated;

		write_msg(NULL, "finding table check constraints\n");

		resetPQExpBuffer(q);
		appendPQExpBufferStr(q,
							 "SELECT c.tableoid, c.oid, conrelid, conname, "
							 "pg_catalog.pg_get_constraintdef(c.oid) AS consrc, ");
		if (fout->remoteVersion >= 90200)
		{
			/*
			 * convalidated is new in 9.2 (actually, it is there in 9.1, but
			 * it wasn't ever false for check constraints until 9.2).
			 */
			appendPQExpBufferStr(q,
								 "conislocal, convalidated ");
		}
		else if (fout->remoteVersion >= 80400)
		{
			/* conislocal is new in 8.4 */
			appendPQExpBufferStr(q,
								 "conislocal, true AS convalidated ");
		}
		else
		{
			appendPQExpBufferStr(q,
								 "true AS conislocal, true AS convalidated ");
		}
		appendPQExpBuffer(q,
						  "FROM unnest('%s'::pg_catalog.oid[]) AS src(tbloid)\n"
						  "JOIN pg_catalog.pg_constraint c ON (src.tbloid = c.conrelid)\n"
						  "WHERE contype = 'c' "
						  "ORDER BY c.conrelid, c.conname",
						  checkoids->data);

		res = ExecuteSqlQuery(fout, q->data, PGRES_TUPLES_OK);

		numConstrs = PQntuples(res);
		constrs = (ConstraintInfo *) pg_malloc(numConstrs * sizeof(ConstraintInfo));

		i_tableoid = PQfnumber(res, "tableoid");
		i_oid = PQfnumber(res, "oid");
		i_conrelid = PQfnumber(res, "conrelid");
		i_conname = PQfnumber(res, "conname");
		i_consrc = PQfnumber(res, "consrc");
		i_conislocal = PQfnumber(res, "conislocal");
		i_convalidated = PQfnumber(res, "convalidated");
	
		/* As above, this loop iterates once per table, not once per row */
		curtblindx = -1;
		for (int j = 0; j < numConstrs;)
		{
			Oid			conrelid = atooid(PQgetvalue(res, j, i_conrelid));
			TableInfo  *tbinfo = NULL;
			int			numcons;

			/* Count rows for this table */
			for (numcons = 1; numcons < numConstrs - j; numcons++)
				if (atooid(PQgetvalue(res, j + numcons, i_conrelid)) != conrelid)
					break;

			/*
			 * Locate the associated TableInfo; we rely on tblinfo[] being in
			 * OID order.
			 */
			while (++curtblindx < numTables)
			{
				tbinfo = &tblinfo[curtblindx];
				if (tbinfo->dobj.catId.oid == conrelid)
					break;
			}
			if (curtblindx >= numTables)
				exit_horribly(NULL, "unrecognized table OID %u\n", conrelid);

			if (numcons != tbinfo->ncheck)
			{
				write_msg(NULL, ngettext("expected %d check constraint on table \"%s\" but found %d\n",
									  "expected %d check constraints on table \"%s\" but found %d\n",
									  tbinfo->ncheck),
							 tbinfo->ncheck, tbinfo->dobj.name, numcons);
				write_msg(NULL, "(The system catalogs might be corrupted.)\n");
				exit_nicely(1);
			}

			tbinfo->checkexprs = constrs + j;

			for (int c = 0; c < numcons; c++, j++)
			{
				bool		validated = PQgetvalue(res, j, i_convalidated)[0] == 't';

				constrs[j].dobj.objType = DO_CONSTRAINT;
				constrs[j].dobj.catId.tableoid = atooid(PQgetvalue(res, j, i_tableoid));
				constrs[j].dobj.catId.oid = atooid(PQgetvalue(res, j, i_oid));
				AssignDumpId(&constrs[j].dobj);
				constrs[j].dobj.name = pg_strdup(PQgetvalue(res, j, i_conname));
				constrs[j].dobj.namespace = tbinfo->dobj.namespace;
				constrs[j].contable = tbinfo;
				constrs[j].condomain = NULL;
				constrs[j].contype = 'c';
				constrs[j].condef = pg_strdup(PQgetvalue(res, j, i_consrc));
				constrs[j].confrelid = InvalidOid;
				constrs[j].conindex = 0;
				constrs[j].condeferrable = false;
				constrs[j].condeferred = false;
				constrs[j].conislocal = (PQgetvalue(res, j, i_conislocal)[0] == 't');

				/*
				 * An unvalidated constraint needs to be dumped separately, so
				 * that potentially-violating existing data is loaded before
				 * the constraint.
				 */
				constrs[j].separate = !validated;

				constrs[j].dobj.dump = tbinfo->dobj.dump;

				/*
				 * Mark the constraint as needing to appear before the table
				 * --- this is so that any other dependencies of the
				 * constraint will be emitted before we try to create the
				 * table.  If the constraint is to be dumped separately, it
				 * will be dumped after data is loaded anyway, so don't do it.
				 * (There's an automatic dependency in the opposite direction
				 * anyway, so don't need to add one manually here.)
				 */
				if (!constrs[j].separate)
					addObjectDependency(&tbinfo->dobj,
										constrs[j].dobj.dumpId);

				/*
				 * If the constraint is inherited, this will be detected later
				 * (in pre-8.4 databases).  We also detect later if the
				 * constraint must be split out from the table definition.
				 */
			}
		}

		PQclear(res);
	}

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(tbloids);
	destroyPQExpBuffer(checkoids);
}

/*
 * Test whether a column should be printed as part of table's CREATE TABLE.
 * Column number is zero-based.
 *
 * Normally this is always true, but it's false for dropped columns, as well
 * as those that were inherited without any local definition.  (If we print
 * such a column it will mistakenly get pg_attribute.attislocal set to true.)
 * However, in binary_upgrade mode, we must print all such columns anyway and
 * fix the attislocal/attisdropped state later, so as to keep control of the
 * physical column order.
 *
 * This function exists because there are scattered nonobvious places that
 * must be kept in sync with this decision.
 *
 * GPDB: For GPDB partition tables during binary_upgrade mode, dropped columns
 * may or may not be printed depending on whether all the child partitions
 * have the dropped column reference OR all the child partitions do not have
 * the dropped column reference.
 */
bool
shouldPrintColumn(TableInfo *tbinfo, int colno)
{
	if (binary_upgrade && !tbinfo->ignoreRootPartDroppedAttr)
		return true;
	return ((tbinfo->attislocal[colno] || tbinfo->relstorage == RELSTORAGE_EXTERNAL) &&
	        !tbinfo->attisdropped[colno]);
}


/*
 * getTSParsers:
 *	  read all text search parsers in the system catalogs and return them
 *	  in the TSParserInfo* structure
 *
 *	numTSParsers is set to the number of parsers read in
 */
TSParserInfo *
getTSParsers(Archive *fout, int *numTSParsers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	TSParserInfo *prsinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_prsname;
	int			i_prsnamespace;
	int			i_prsstart;
	int			i_prstoken;
	int			i_prsend;
	int			i_prsheadline;
	int			i_prslextype;

	query = createPQExpBuffer();

	/*
	 * find all text search objects, including builtin ones; we filter out
	 * system-defined objects at dump-out time.
	 */

	appendPQExpBufferStr(query, "SELECT tableoid, oid, prsname, prsnamespace, "
						 "prsstart::oid, prstoken::oid, "
						 "prsend::oid, prsheadline::oid, prslextype::oid "
						 "FROM pg_ts_parser");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSParsers = ntups;

	prsinfo = (TSParserInfo *) pg_malloc(ntups * sizeof(TSParserInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_prsname = PQfnumber(res, "prsname");
	i_prsnamespace = PQfnumber(res, "prsnamespace");
	i_prsstart = PQfnumber(res, "prsstart");
	i_prstoken = PQfnumber(res, "prstoken");
	i_prsend = PQfnumber(res, "prsend");
	i_prsheadline = PQfnumber(res, "prsheadline");
	i_prslextype = PQfnumber(res, "prslextype");

	for (i = 0; i < ntups; i++)
	{
		prsinfo[i].dobj.objType = DO_TSPARSER;
		prsinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		prsinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&prsinfo[i].dobj);
		prsinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_prsname));
		prsinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_prsnamespace)),
						  prsinfo[i].dobj.catId.oid);
		prsinfo[i].prsstart = atooid(PQgetvalue(res, i, i_prsstart));
		prsinfo[i].prstoken = atooid(PQgetvalue(res, i, i_prstoken));
		prsinfo[i].prsend = atooid(PQgetvalue(res, i, i_prsend));
		prsinfo[i].prsheadline = atooid(PQgetvalue(res, i, i_prsheadline));
		prsinfo[i].prslextype = atooid(PQgetvalue(res, i, i_prslextype));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(prsinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return prsinfo;
}

/*
 * getTSDictionaries:
 *	  read all text search dictionaries in the system catalogs and return them
 *	  in the TSDictInfo* structure
 *
 *	numTSDicts is set to the number of dictionaries read in
 */
TSDictInfo *
getTSDictionaries(Archive *fout, int *numTSDicts)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	TSDictInfo *dictinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_dictname;
	int			i_dictnamespace;
	int			i_dictowner;
	int			i_dicttemplate;
	int			i_dictinitoption;

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT tableoid, oid, dictname, "
					  "dictnamespace, dictowner, "
					  "dicttemplate, dictinitoption "
					  "FROM pg_ts_dict");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSDicts = ntups;

	dictinfo = (TSDictInfo *) pg_malloc(ntups * sizeof(TSDictInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_dictname = PQfnumber(res, "dictname");
	i_dictnamespace = PQfnumber(res, "dictnamespace");
	i_dictowner = PQfnumber(res, "dictowner");
	i_dictinitoption = PQfnumber(res, "dictinitoption");
	i_dicttemplate = PQfnumber(res, "dicttemplate");

	for (i = 0; i < ntups; i++)
	{
		dictinfo[i].dobj.objType = DO_TSDICT;
		dictinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		dictinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&dictinfo[i].dobj);
		dictinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_dictname));
		dictinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_dictnamespace)),
						  dictinfo[i].dobj.catId.oid);
		dictinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_dictowner));
		dictinfo[i].dicttemplate = atooid(PQgetvalue(res, i, i_dicttemplate));
		if (PQgetisnull(res, i, i_dictinitoption))
			dictinfo[i].dictinitoption = NULL;
		else
			dictinfo[i].dictinitoption = pg_strdup(PQgetvalue(res, i, i_dictinitoption));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(dictinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return dictinfo;
}

/*
 * getTSTemplates:
 *	  read all text search templates in the system catalogs and return them
 *	  in the TSTemplateInfo* structure
 *
 *	numTSTemplates is set to the number of templates read in
 */
TSTemplateInfo *
getTSTemplates(Archive *fout, int *numTSTemplates)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	TSTemplateInfo *tmplinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_tmplname;
	int			i_tmplnamespace;
	int			i_tmplinit;
	int			i_tmpllexize;

	query = createPQExpBuffer();

	appendPQExpBufferStr(query, "SELECT tableoid, oid, tmplname, "
						 "tmplnamespace, tmplinit::oid, tmpllexize::oid "
						 "FROM pg_ts_template");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSTemplates = ntups;

	tmplinfo = (TSTemplateInfo *) pg_malloc(ntups * sizeof(TSTemplateInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_tmplname = PQfnumber(res, "tmplname");
	i_tmplnamespace = PQfnumber(res, "tmplnamespace");
	i_tmplinit = PQfnumber(res, "tmplinit");
	i_tmpllexize = PQfnumber(res, "tmpllexize");

	for (i = 0; i < ntups; i++)
	{
		tmplinfo[i].dobj.objType = DO_TSTEMPLATE;
		tmplinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		tmplinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&tmplinfo[i].dobj);
		tmplinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_tmplname));
		tmplinfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_tmplnamespace)),
						  tmplinfo[i].dobj.catId.oid);
		tmplinfo[i].tmplinit = atooid(PQgetvalue(res, i, i_tmplinit));
		tmplinfo[i].tmpllexize = atooid(PQgetvalue(res, i, i_tmpllexize));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(tmplinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return tmplinfo;
}

/*
 * getTSConfigurations:
 *	  read all text search configurations in the system catalogs and return
 *	  them in the TSConfigInfo* structure
 *
 *	numTSConfigs is set to the number of configurations read in
 */
TSConfigInfo *
getTSConfigurations(Archive *fout, int *numTSConfigs)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	TSConfigInfo *cfginfo;
	int			i_tableoid;
	int			i_oid;
	int			i_cfgname;
	int			i_cfgnamespace;
	int			i_cfgowner;
	int			i_cfgparser;

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT tableoid, oid, cfgname, "
					  "cfgnamespace, cfgowner, cfgparser "
					  "FROM pg_ts_config");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numTSConfigs = ntups;

	cfginfo = (TSConfigInfo *) pg_malloc(ntups * sizeof(TSConfigInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_cfgname = PQfnumber(res, "cfgname");
	i_cfgnamespace = PQfnumber(res, "cfgnamespace");
	i_cfgowner = PQfnumber(res, "cfgowner");
	i_cfgparser = PQfnumber(res, "cfgparser");

	for (i = 0; i < ntups; i++)
	{
		cfginfo[i].dobj.objType = DO_TSCONFIG;
		cfginfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		cfginfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&cfginfo[i].dobj);
		cfginfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_cfgname));
		cfginfo[i].dobj.namespace =
			findNamespace(fout,
						  atooid(PQgetvalue(res, i, i_cfgnamespace)),
						  cfginfo[i].dobj.catId.oid);
		cfginfo[i].rolname = getRoleName(PQgetvalue(res, i, i_cfgowner));
		cfginfo[i].cfgparser = atooid(PQgetvalue(res, i, i_cfgparser));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(cfginfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return cfginfo;
}

/*
 * getForeignDataWrappers:
 *	  read all foreign-data wrappers in the system catalogs and return
 *	  them in the FdwInfo* structure
 *
 *	numForeignDataWrappers is set to the number of fdws read in
 */
FdwInfo *
getForeignDataWrappers(Archive *fout, int *numForeignDataWrappers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	FdwInfo    *fdwinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_fdwname;
	int			i_fdwowner;
	int			i_fdwhandler;
	int			i_fdwvalidator;
	int			i_fdwacl;
	int			i_fdwoptions;

	/* Before 8.4, there are no foreign-data wrappers */
	if (fout->remoteVersion < 80400)
	{
		*numForeignDataWrappers = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	if (fout->remoteVersion >= 90100)
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, fdwname, "
						  "fdwowner, "
						  "fdwhandler::pg_catalog.regproc, "
						  "fdwvalidator::pg_catalog.regproc, fdwacl, "
						  "array_to_string(ARRAY("
						  "SELECT quote_ident(option_name) || ' ' || "
						  "quote_literal(option_value) "
						  "FROM pg_options_to_table(fdwoptions) "
						  "ORDER BY option_name"
						  "), E',\n    ') AS fdwoptions "
						  "FROM pg_foreign_data_wrapper");
	}
	else
	{
		appendPQExpBuffer(query, "SELECT tableoid, oid, fdwname, "
						  "fdwowner, "
						  "'-' AS fdwhandler, "
						  "fdwvalidator::pg_catalog.regproc, fdwacl, "
						  "array_to_string(ARRAY("
						  "SELECT quote_ident(option_name) || ' ' || "
						  "quote_literal(option_value) "
						  "FROM pg_options_to_table(fdwoptions) "
						  "ORDER BY option_name"
						  "), E',\n    ') AS fdwoptions "
						  "FROM pg_foreign_data_wrapper");
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numForeignDataWrappers = ntups;

	fdwinfo = (FdwInfo *) pg_malloc(ntups * sizeof(FdwInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_fdwname = PQfnumber(res, "fdwname");
	i_fdwowner = PQfnumber(res, "fdwowner");
	i_fdwhandler = PQfnumber(res, "fdwhandler");
	i_fdwvalidator = PQfnumber(res, "fdwvalidator");
	i_fdwacl = PQfnumber(res, "fdwacl");
	i_fdwoptions = PQfnumber(res, "fdwoptions");

	for (i = 0; i < ntups; i++)
	{
		fdwinfo[i].dobj.objType = DO_FDW;
		fdwinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		fdwinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&fdwinfo[i].dobj);
		fdwinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_fdwname));
		fdwinfo[i].dobj.namespace = NULL;
		fdwinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_fdwowner));
		fdwinfo[i].fdwhandler = pg_strdup(PQgetvalue(res, i, i_fdwhandler));
		fdwinfo[i].fdwvalidator = pg_strdup(PQgetvalue(res, i, i_fdwvalidator));
		fdwinfo[i].fdwoptions = pg_strdup(PQgetvalue(res, i, i_fdwoptions));
		fdwinfo[i].fdwacl = pg_strdup(PQgetvalue(res, i, i_fdwacl));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(fdwinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return fdwinfo;
}

/*
 * getForeignServers:
 *	  read all foreign servers in the system catalogs and return
 *	  them in the ForeignServerInfo * structure
 *
 *	numForeignServers is set to the number of servers read in
 */
ForeignServerInfo *
getForeignServers(Archive *fout, int *numForeignServers)
{
	PGresult   *res;
	int			ntups;
	int			i;
	PQExpBuffer query;
	ForeignServerInfo *srvinfo;
	int			i_tableoid;
	int			i_oid;
	int			i_srvname;
	int			i_srvowner;
	int			i_srvfdw;
	int			i_srvtype;
	int			i_srvversion;
	int			i_srvacl;
	int			i_srvoptions;

	/* Before 8.4, there are no foreign servers */
	if (fout->remoteVersion < 80400)
	{
		*numForeignServers = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT tableoid, oid, srvname, "
					  "srvowner, "
					  "srvfdw, srvtype, srvversion, srvacl,"
					  "array_to_string(ARRAY("
					  "SELECT quote_ident(option_name) || ' ' || "
					  "quote_literal(option_value) "
					  "FROM pg_options_to_table(srvoptions) "
					  "ORDER BY option_name"
					  "), E',\n    ') AS srvoptions "
					  "FROM pg_foreign_server");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numForeignServers = ntups;

	srvinfo = (ForeignServerInfo *) pg_malloc(ntups * sizeof(ForeignServerInfo));

	i_tableoid = PQfnumber(res, "tableoid");
	i_oid = PQfnumber(res, "oid");
	i_srvname = PQfnumber(res, "srvname");
	i_srvowner = PQfnumber(res, "srvowner");
	i_srvfdw = PQfnumber(res, "srvfdw");
	i_srvtype = PQfnumber(res, "srvtype");
	i_srvversion = PQfnumber(res, "srvversion");
	i_srvacl = PQfnumber(res, "srvacl");
	i_srvoptions = PQfnumber(res, "srvoptions");

	for (i = 0; i < ntups; i++)
	{
		srvinfo[i].dobj.objType = DO_FOREIGN_SERVER;
		srvinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		srvinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&srvinfo[i].dobj);
		srvinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_srvname));
		srvinfo[i].dobj.namespace = NULL;
		srvinfo[i].rolname = getRoleName(PQgetvalue(res, i, i_srvowner));
		srvinfo[i].srvfdw = atooid(PQgetvalue(res, i, i_srvfdw));
		srvinfo[i].srvtype = pg_strdup(PQgetvalue(res, i, i_srvtype));
		srvinfo[i].srvversion = pg_strdup(PQgetvalue(res, i, i_srvversion));
		srvinfo[i].srvoptions = pg_strdup(PQgetvalue(res, i, i_srvoptions));
		srvinfo[i].srvacl = pg_strdup(PQgetvalue(res, i, i_srvacl));

		/* Decide whether we want to dump it */
		selectDumpableObject(&(srvinfo[i].dobj));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return srvinfo;
}

/*
 * getDefaultACLs:
 *	  read all default ACL information in the system catalogs and return
 *	  them in the DefaultACLInfo structure
 *
 *	numDefaultACLs is set to the number of ACLs read in
 */
DefaultACLInfo *
getDefaultACLs(Archive *fout, int *numDefaultACLs)
{
	DefaultACLInfo *daclinfo;
	PQExpBuffer query;
	PGresult   *res;
	int			i_oid;
	int			i_tableoid;
	int			i_defaclrole;
	int			i_defaclnamespace;
	int			i_defaclobjtype;
	int			i_defaclacl;
	int			i,
				ntups;

	if (fout->remoteVersion < 90000)
	{
		*numDefaultACLs = 0;
		return NULL;
	}

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT oid, tableoid, "
					  "defaclrole, "
					  "defaclnamespace, "
					  "defaclobjtype, "
					  "defaclacl "
					  "FROM pg_default_acl");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	*numDefaultACLs = ntups;

	daclinfo = (DefaultACLInfo *) pg_malloc(ntups * sizeof(DefaultACLInfo));

	i_oid = PQfnumber(res, "oid");
	i_tableoid = PQfnumber(res, "tableoid");
	i_defaclrole = PQfnumber(res, "defaclrole");
	i_defaclnamespace = PQfnumber(res, "defaclnamespace");
	i_defaclobjtype = PQfnumber(res, "defaclobjtype");
	i_defaclacl = PQfnumber(res, "defaclacl");

	for (i = 0; i < ntups; i++)
	{
		Oid			nspid = atooid(PQgetvalue(res, i, i_defaclnamespace));

		daclinfo[i].dobj.objType = DO_DEFAULT_ACL;
		daclinfo[i].dobj.catId.tableoid = atooid(PQgetvalue(res, i, i_tableoid));
		daclinfo[i].dobj.catId.oid = atooid(PQgetvalue(res, i, i_oid));
		AssignDumpId(&daclinfo[i].dobj);
		/* cheesy ... is it worth coming up with a better object name? */
		daclinfo[i].dobj.name = pg_strdup(PQgetvalue(res, i, i_defaclobjtype));

		if (nspid != InvalidOid)
			daclinfo[i].dobj.namespace = findNamespace(fout, nspid,
												 daclinfo[i].dobj.catId.oid);
		else
			daclinfo[i].dobj.namespace = NULL;

		daclinfo[i].defaclrole = getRoleName(PQgetvalue(res, i, i_defaclrole));
		daclinfo[i].defaclobjtype = *(PQgetvalue(res, i, i_defaclobjtype));
		daclinfo[i].defaclacl = pg_strdup(PQgetvalue(res, i, i_defaclacl));

		/* Decide whether we want to dump it */
		selectDumpableDefaultACL(&(daclinfo[i]));
	}

	PQclear(res);

	destroyPQExpBuffer(query);

	return daclinfo;
}

/*
 * getRoleName -- look up the name of a role, given its OID
 *
 * In current usage, we don't expect failures, so error out for a bad OID.
 */
static const char *
getRoleName(const char *roleoid_str)
{
	Oid			roleoid = atooid(roleoid_str);

	/*
	 * Do binary search to find the appropriate item.
	 */
	if (nrolenames > 0)
	{
		RoleNameItem *low = &rolenames[0];
		RoleNameItem *high = &rolenames[nrolenames - 1];

		while (low <= high)
		{
			RoleNameItem *middle = low + (high - low) / 2;

			if (roleoid < middle->roleoid)
				high = middle - 1;
			else if (roleoid > middle->roleoid)
				low = middle + 1;
			else
				return middle->rolename;	/* found a match */
		}
	}

	exit_horribly(NULL, "role with OID %u does not exist\n", roleoid);
	return NULL;				/* keep compiler quiet */
}

/*
 * collectRoleNames --
 *
 * Construct a table of all known roles.
 * The table is sorted by OID for speed in lookup.
 */
static void
collectRoleNames(Archive *fout)
{
	PGresult   *res;
	const char *query;
	int			i;

	query = "SELECT oid, rolname FROM pg_catalog.pg_roles ORDER BY 1";

	res = ExecuteSqlQuery(fout, query, PGRES_TUPLES_OK);

	nrolenames = PQntuples(res);

	rolenames = (RoleNameItem *) pg_malloc(nrolenames * sizeof(RoleNameItem));

	for (i = 0; i < nrolenames; i++)
	{
		rolenames[i].roleoid = atooid(PQgetvalue(res, i, 0));
		rolenames[i].rolename = pg_strdup(PQgetvalue(res, i, 1));
	}

	PQclear(res);
}

/*
 * dumpComment --
 *
 * This routine is used to dump any comments associated with the
 * object handed to this routine. The routine takes the object type
 * and object name (ready to print, except for schema decoration), plus
 * the namespace and owner of the object (for labeling the ArchiveEntry),
 * plus catalog ID and subid which are the lookup key for pg_description,
 * plus the dump ID for the object (for setting a dependency).
 * If a matching pg_description entry is found, it is dumped.
 *
 * Note: in some cases, such as comments for triggers and rules, the "type"
 * string really looks like, e.g., "TRIGGER name ON".  This is a bit of a hack
 * but it doesn't seem worth complicating the API for all callers to make
 * it cleaner.
 *
 * Note: although this routine takes a dumpId for dependency purposes,
 * that purpose is just to mark the dependency in the emitted dump file
 * for possible future use by pg_restore.  We do NOT use it for determining
 * ordering of the comment in the dump file, because this routine is called
 * after dependency sorting occurs.  This routine should be called just after
 * calling ArchiveEntry() for the specified object.
 */
static void
dumpComment(Archive *fout, const char *type, const char *name,
			const char *namespace, const char *owner,
			CatalogId catalogId, int subid, DumpId dumpId)
{
	CommentItem *comments;
	int			ncomments;

	/* Comments are schema not data ... except blob comments are data */
	if (strcmp(type, "LARGE OBJECT") != 0)
	{
		if (dataOnly)
			return;
	}
	else
	{
		/* We do dump blob comments in binary-upgrade mode */
		if (schemaOnly && !binary_upgrade)
			return;
	}

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
		PQExpBuffer tag = createPQExpBuffer();

		appendPQExpBuffer(query, "COMMENT ON %s ", type);
		if (namespace && *namespace)
			appendPQExpBuffer(query, "%s.", fmtId(namespace));
		appendPQExpBuffer(query, "%s IS ", name);
		appendStringLiteralAH(query, comments->descr, fout);
		appendPQExpBufferStr(query, ";\n");

		appendPQExpBuffer(tag, "%s %s", type, name);

		/*
		 * We mark comments as SECTION_NONE because they really belong in the
		 * same section as their parent, whether that is pre-data or
		 * post-data.
		 */
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag->data, namespace, NULL, owner,
					 false, "COMMENT", SECTION_NONE,
					 query->data, "", NULL,
					 &(dumpId), 1,
					 NULL, NULL);

		destroyPQExpBuffer(query);
		destroyPQExpBuffer(tag);
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
	PQExpBuffer tag;

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
	tag = createPQExpBuffer();

	while (ncomments > 0)
	{
		const char *descr = comments->descr;
		int			objsubid = comments->objsubid;

		if (objsubid == 0)
		{
			resetPQExpBuffer(tag);
			if (strcmp(reltypename, "EXTERNAL TABLE") == 0)
				reltypename = "TABLE";
			appendPQExpBuffer(tag, "%s %s.", reltypename,
							  fmtId(tbinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(tag, "%s ", fmtId(tbinfo->dobj.name));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON %s %s IS ", reltypename,
							  fmtQualifiedDumpable(tbinfo));
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBufferStr(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 tag->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL, tbinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}
		else if (objsubid > 0 && objsubid <= tbinfo->numatts)
		{
			resetPQExpBuffer(tag);
			appendPQExpBuffer(tag, "COLUMN %s.",
							  fmtId(tbinfo->dobj.name));
			appendPQExpBufferStr(tag, fmtId(tbinfo->attnames[objsubid - 1]));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON COLUMN %s.",
							  fmtQualifiedDumpable(tbinfo));
			appendPQExpBuffer(query, "%s IS ",
							  fmtId(tbinfo->attnames[objsubid - 1]));
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBufferStr(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 tag->data,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL, tbinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}

		comments++;
		ncomments--;
	}

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(tag);
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

	query = createPQExpBuffer();

	appendPQExpBufferStr(query, "SELECT description, classoid, objoid, objsubid "
							"FROM pg_catalog.pg_description "
							"ORDER BY classoid, objoid, objsubid");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	/* Construct lookup table containing OIDs in numeric form */

	i_description = PQfnumber(res, "description");
	i_classoid = PQfnumber(res, "classoid");
	i_objoid = PQfnumber(res, "objoid");
	i_objsubid = PQfnumber(res, "objsubid");

	ntups = PQntuples(res);

	comments = (CommentItem *) pg_malloc(ntups * sizeof(CommentItem));

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
	switch (dobj->objType)
	{
		case DO_NAMESPACE:
			dumpNamespace(fout, (NamespaceInfo *) dobj);
			break;
		case DO_EXTENSION:
			dumpExtension(fout, (ExtensionInfo *) dobj);
			break;
		case DO_TYPE:
			dumpType(fout, (TypeInfo *) dobj);
			break;
		case DO_SHELL_TYPE:
			dumpShellType(fout, (ShellTypeInfo *) dobj);
			break;
		case DO_FUNC:
			dumpFunc(fout, (FuncInfo *) dobj);
			break;
		case DO_AGG:
			dumpAgg(fout, (AggInfo *) dobj);
			break;
		case DO_EXTPROTOCOL:
			dumpExtProtocol(fout, (ExtProtInfo *) dobj);
			break;
		case DO_OPERATOR:
			dumpOpr(fout, (OprInfo *) dobj);
			break;
		case DO_OPCLASS:
			dumpOpclass(fout, (OpclassInfo *) dobj);
			break;
		case DO_OPFAMILY:
			dumpOpfamily(fout, (OpfamilyInfo *) dobj);
			break;
		case DO_COLLATION:
			dumpCollation(fout, (CollInfo *) dobj);
			break;
		case DO_CONVERSION:
			dumpConversion(fout, (ConvInfo *) dobj);
			break;
		case DO_TABLE:
			dumpTable(fout, (TableInfo *) dobj);
			break;
		case DO_ATTRDEF:
			dumpAttrDef(fout, (AttrDefInfo *) dobj);
			break;
		case DO_INDEX:
			dumpIndex(fout, (IndxInfo *) dobj);
			break;
		case DO_REFRESH_MATVIEW:
			refreshMatViewData(fout, (TableDataInfo *) dobj);
			break;
		case DO_RULE:
			dumpRule(fout, (RuleInfo *) dobj);
			break;
		case DO_TRIGGER:
			dumpTrigger(fout, (TriggerInfo *) dobj);
			break;
		case DO_EVENT_TRIGGER:
			dumpEventTrigger(fout, (EventTriggerInfo *) dobj);
			break;
		case DO_CONSTRAINT:
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_FK_CONSTRAINT:
			dumpConstraint(fout, (ConstraintInfo *) dobj);
			break;
		case DO_PROCLANG:
			dumpProcLang(fout, (ProcLangInfo *) dobj);
			break;
		case DO_CAST:
			dumpCast(fout, (CastInfo *) dobj);
			break;
		case DO_TABLE_DATA:
			if (((TableDataInfo *) dobj)->tdtable->relkind == RELKIND_SEQUENCE)
				dumpSequenceData(fout, (TableDataInfo *) dobj);
			else
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
		case DO_FDW:
			dumpForeignDataWrapper(fout, (FdwInfo *) dobj);
			break;
		case DO_FOREIGN_SERVER:
			dumpForeignServer(fout, (ForeignServerInfo *) dobj);
			break;
		case DO_DEFAULT_ACL:
			dumpDefaultACL(fout, (DefaultACLInfo *) dobj);
			break;
		case DO_BLOB:
			dumpBlob(fout, (BlobInfo *) dobj);
			break;
		case DO_BLOB_DATA:
			ArchiveEntry(fout, dobj->catId, dobj->dumpId,
						 dobj->name, NULL, NULL, "",
						 false, "BLOBS", SECTION_DATA,
						 "", "", NULL,
						 NULL, 0,
						 dumpBlobs, NULL);
			break;
		case DO_PRE_DATA_BOUNDARY:
		case DO_POST_DATA_BOUNDARY:
			/* never dumped, nothing to do */
			break;

		case DO_BINARY_UPGRADE:
			dumpPreassignedOidArchiveEntry(fout, (BinaryUpgradeInfo *) dobj);
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

	if (binary_upgrade)
		binary_upgrade_set_namespace_oid(fout, q, nspinfo->dobj.catId.oid);

	appendPQExpBuffer(q, "CREATE SCHEMA %s;\n", qnspname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &nspinfo->dobj,
										"SCHEMA", qnspname, NULL);

	ArchiveEntry(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId,
				 nspinfo->dobj.name,
				 NULL, NULL,
				 nspinfo->rolname,
				 false, "SCHEMA", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 &(binary_upgrade_dumpid), 1,
				 NULL, NULL);

	/* Dump Schema Comments and Security Labels */
	dumpComment(fout, "SCHEMA", qnspname,
				NULL, nspinfo->rolname,
				nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);
	dumpSecLabel(fout, "SCHEMA", qnspname,
				 NULL, nspinfo->rolname,
				 nspinfo->dobj.catId, 0, nspinfo->dobj.dumpId);

	dumpACL(fout, nspinfo->dobj.catId, nspinfo->dobj.dumpId, "SCHEMA",
			qnspname, NULL, NULL,
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
	char	   *qextname;

	/* Skip if not to be dumped */
	if (!extinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qextname = pg_strdup(fmtId(extinfo->dobj.name));

	appendPQExpBuffer(delq, "DROP EXTENSION %s;\n", qextname);

	if (!binary_upgrade)
	{
		/*
		 * In a regular dump, we use IF NOT EXISTS so that there isn't a
		 * problem if the extension already exists in the target database;
		 * this is essential for installed-by-default extensions such as
		 * plpgsql.
		 *
		 * In binary-upgrade mode, that doesn't work well, so instead we skip
		 * built-in extensions based on their OIDs; see
		 * selectDumpableExtension.
		 */
		appendPQExpBuffer(q, "CREATE EXTENSION IF NOT EXISTS %s WITH SCHEMA %s;\n",
						  qextname, fmtId(extinfo->namespace));
	}
	else
	{
		int			i;
		int			n;

		appendPQExpBufferStr(q, "-- For binary upgrade, create an empty extension and insert objects into it\n");

		/*
		 * We unconditionally create the extension, so we must drop it if it
		 * exists.  This could happen if the user deleted 'plpgsql' and then
		 * readded it, causing its oid to be greater than g_last_builtin_oid.
		 * The g_last_builtin_oid test was kept to avoid repeatedly dropping
		 * and recreating extensions like 'plpgsql'.
		 */
		appendPQExpBuffer(q, "DROP EXTENSION IF EXISTS %s;\n", qextname);

		appendPQExpBufferStr(q,
							 "SELECT binary_upgrade.create_empty_extension(");
		appendStringLiteralAH(q, extinfo->dobj.name, fout);
		appendPQExpBufferStr(q, ", ");
		appendStringLiteralAH(q, extinfo->namespace, fout);
		appendPQExpBufferStr(q, ", ");
		appendPQExpBuffer(q, "%s, ", extinfo->relocatable ? "true" : "false");
		appendStringLiteralAH(q, extinfo->extversion, fout);
		appendPQExpBufferStr(q, ", ");

		/*
		 * Note that we're pushing extconfig (an OID array) back into
		 * pg_extension exactly as-is.  This is OK because pg_class OIDs are
		 * preserved in binary upgrade.
		 */
		if (strlen(extinfo->extconfig) > 2)
			appendStringLiteralAH(q, extinfo->extconfig, fout);
		else
			appendPQExpBufferStr(q, "NULL");
		appendPQExpBufferStr(q, ", ");
		if (strlen(extinfo->extcondition) > 2)
			appendStringLiteralAH(q, extinfo->extcondition, fout);
		else
			appendPQExpBufferStr(q, "NULL");
		appendPQExpBufferStr(q, ", ");
		appendPQExpBufferStr(q, "ARRAY[");
		n = 0;
		for (i = 0; i < extinfo->dobj.nDeps; i++)
		{
			DumpableObject *extobj;

			extobj = findObjectByDumpId(extinfo->dobj.dependencies[i]);
			if (extobj && extobj->objType == DO_EXTENSION)
			{
				if (n++ > 0)
					appendPQExpBufferChar(q, ',');
				appendStringLiteralAH(q, extobj->name, fout);
			}
		}
		appendPQExpBufferStr(q, "]::pg_catalog.text[]");
		appendPQExpBufferStr(q, ");\n");
	}

	ArchiveEntry(fout, extinfo->dobj.catId, extinfo->dobj.dumpId,
				 extinfo->dobj.name,
				 NULL, NULL,
				 "",
				 false, "EXTENSION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Extension Comments and Security Labels */
	dumpComment(fout, "EXTENSION", qextname,
				NULL, "",
				extinfo->dobj.catId, 0, extinfo->dobj.dumpId);
	dumpSecLabel(fout, "EXTENSION", qextname,
				 NULL, "",
				 extinfo->dobj.catId, 0, extinfo->dobj.dumpId);

	free(qextname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpType
 *	  writes out to fout the queries to recreate a user-defined type
 */
static void
dumpType(Archive *fout, TypeInfo *tyinfo)
{
	/* Skip if not to be dumped */
	if (!tyinfo->dobj.dump || dataOnly)
		return;

	/* Dump out in proper style */
	if (tyinfo->typtype == TYPTYPE_BASE)
		dumpBaseType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_DOMAIN)
		dumpDomain(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_COMPOSITE)
		dumpCompositeType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_ENUM)
		dumpEnumType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_RANGE)
		dumpRangeType(fout, tyinfo);
	else if (tyinfo->typtype == TYPTYPE_PSEUDO && !tyinfo->isDefined)
		dumpUndefinedType(fout, tyinfo);
	else
		write_msg(NULL, "typtype of data type \"%s\" appears to be invalid",
					   tyinfo->dobj.name);

	if (tyinfo->typstorage && *tyinfo->typstorage != '\0')
		dumpTypeStorageOptions(fout, tyinfo);

}

/*
 * dumpEnumType
 *	  writes out to fout the queries to recreate a user-defined enum type
 */
static void
dumpEnumType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			num,
				i;
	Oid			enum_oid;
	char	   *qtypname;
	char	   *qualtypname;
	char	   *label;
	int			i_enumlabel;
	int			i_oid;

	if (!fout->is_prepared[PREPQUERY_DUMPENUMTYPE])
	{
		/* Set up query for enum-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpEnumType(pg_catalog.oid) AS\n");

		if (fout->remoteVersion >= 90100)
			appendPQExpBufferStr(query, "SELECT oid, enumlabel "
								 "FROM pg_catalog.pg_enum "
								 "WHERE enumtypid = $1 "
								 "ORDER BY enumsortorder");
		else
			appendPQExpBufferStr(query, "SELECT oid, enumlabel "
								 "FROM pg_catalog.pg_enum "
								 "WHERE enumtypid = $1 "
								 "ORDER BY oid");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPENUMTYPE] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpEnumType('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	num = PQntuples(res);

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	/*
	 * CASCADE shouldn't be required here as for normal types since the I/O
	 * functions are generic and do not get dropped.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo);

	appendPQExpBuffer(q, "CREATE TYPE %s AS ENUM (",
					  qualtypname);

	if (!binary_upgrade)
	{
		i_enumlabel = PQfnumber(res, "enumlabel");

		/* Labels with server-assigned oids */
		for (i = 0; i < num; i++)
		{
			label = PQgetvalue(res, i, i_enumlabel);
			if (i > 0)
				appendPQExpBufferChar(q, ',');
			appendPQExpBufferStr(q, "\n    ");
			appendStringLiteralAH(q, label, fout);
		}
	}

	appendPQExpBufferStr(q, "\n);\n");

	if (binary_upgrade)
	{
		i_oid = PQfnumber(res, "oid");
		i_enumlabel = PQfnumber(res, "enumlabel");

		/* Labels with dump-assigned (preserved) oids */
		for (i = 0; i < num; i++)
		{
			enum_oid = atooid(PQgetvalue(res, i, i_oid));
			label = PQgetvalue(res, i, i_enumlabel);

			if (i == 0)
				appendPQExpBufferStr(q, "\n-- For binary upgrade, must preserve pg_enum oids\n");
			appendPQExpBuffer(q,
							  "SELECT binary_upgrade.set_next_pg_enum_oid('%u'::pg_catalog.oid, '%u'::pg_catalog.oid, $$%s$$::text);\n",
							  enum_oid, tyinfo->dobj.catId.oid, label);

			appendPQExpBuffer(q, "ALTER TYPE %s.",
							  fmtId(tyinfo->dobj.namespace->dobj.name));
			appendPQExpBuffer(q, "%s ADD VALUE ",
							  qtypname);
			appendStringLiteralAH(q, label, fout);
			appendPQExpBufferStr(q, ";\n\n");
		}
	}

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"TYPE", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Type Comments and Security Labels */
	dumpComment(fout, "TYPE", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "TYPE", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qtypname);
	free(qualtypname);
}

/*
 * dumpRangeType
 *	  writes out to fout the queries to recreate a user-defined range type
 */
static void
dumpRangeType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	Oid			collationOid;
	char	   *qtypname;
	char	   *qualtypname;
	char	   *procname;

	if (!fout->is_prepared[PREPQUERY_DUMPRANGETYPE])
	{
		/* Set up query for range-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpRangeType(pg_catalog.oid) AS\n");

		appendPQExpBuffer(query,
				"SELECT pg_catalog.format_type(rngsubtype, NULL) AS rngsubtype, "
							"opc.opcname AS opcname, "
							"(SELECT nspname FROM pg_catalog.pg_namespace nsp "
							"  WHERE nsp.oid = opc.opcnamespace) AS opcnsp, "
							"opc.opcdefault, "
							"CASE WHEN rngcollation = st.typcollation THEN 0 "
							"     ELSE rngcollation END AS collation, "
							"rngcanonical, rngsubdiff "
							"FROM pg_catalog.pg_range r, pg_catalog.pg_type st, "
							"     pg_catalog.pg_opclass opc "
							"WHERE st.oid = rngsubtype AND opc.oid = rngsubopc AND "
							 "rngtypid = $1");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPRANGETYPE] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpRangeType('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	/*
	 * CASCADE shouldn't be required here as for normal types since the I/O
	 * functions are generic and do not get dropped.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo);

	appendPQExpBuffer(q, "CREATE TYPE %s AS RANGE (",
					  qualtypname);

	appendPQExpBuffer(q, "\n    subtype = %s",
					  PQgetvalue(res, 0, PQfnumber(res, "rngsubtype")));

	/* print subtype_opclass only if not default for subtype */
	if (PQgetvalue(res, 0, PQfnumber(res, "opcdefault"))[0] != 't')
	{
		char	   *opcname = PQgetvalue(res, 0, PQfnumber(res, "opcname"));
		char	   *nspname = PQgetvalue(res, 0, PQfnumber(res, "opcnsp"));

		appendPQExpBuffer(q, ",\n    subtype_opclass = %s.",
						  fmtId(nspname));
		appendPQExpBufferStr(q, fmtId(opcname));
	}

	collationOid = atooid(PQgetvalue(res, 0, PQfnumber(res, "collation")));
	if (OidIsValid(collationOid))
	{
		CollInfo   *coll = findCollationByOid(collationOid);

		if (coll)
			appendPQExpBuffer(q, ",\n    collation = %s",
							  fmtQualifiedDumpable(coll));
	}

	procname = PQgetvalue(res, 0, PQfnumber(res, "rngcanonical"));
	if (strcmp(procname, "-") != 0)
		appendPQExpBuffer(q, ",\n    canonical = %s", procname);

	procname = PQgetvalue(res, 0, PQfnumber(res, "rngsubdiff"));
	if (strcmp(procname, "-") != 0)
		appendPQExpBuffer(q, ",\n    subtype_diff = %s", procname);

	appendPQExpBufferStr(q, "\n);\n");

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"TYPE", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Type Comments and Security Labels */
	dumpComment(fout, "TYPE", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "TYPE", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qtypname);
	free(qualtypname);
}

/*
 * dumpUndefinedType
 *	  writes out to fout the queries to recreate a !typisdefined type
 *
 * This is a shell type, but we use different terminology to distinguish
 * this case from where we have to emit a shell type definition to break
 * circular dependencies.  An undefined type shouldn't ever have anything
 * depending on it.
 */
static void
dumpUndefinedType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	char	   *qtypname;
	char	   *qualtypname;

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout,
												 q, tyinfo);

	appendPQExpBuffer(q, "CREATE TYPE %s;\n",
					  qualtypname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"TYPE", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Type Comments and Security Labels */
	dumpComment(fout, "TYPE", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "TYPE", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qtypname);
	free(qualtypname);
}

/*
 * dumpBaseType
 *	  writes out to fout the queries to recreate a user-defined base type
 */
static void
dumpBaseType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	char	   *qtypname;
	char	   *qualtypname;
	char	   *typlen;
	char	   *typinput;
	char	   *typoutput;
	char	   *typreceive;
	char	   *typsend;
	char	   *typmodin;
	char	   *typmodout;
	char	   *typanalyze;
	Oid			typreceiveoid;
	Oid			typsendoid;
	Oid			typmodinoid;
	Oid			typmodoutoid;
	Oid			typanalyzeoid;
	char	   *typcategory;
	char	   *typispreferred;
	char	   *typdelim;
	char	   *typbyval;
	char	   *typalign;
	char	   *typstorage;
	char	   *typcollatable;
	char	   *typdefault;
	bool		typdefault_is_literal = false;

	if (!fout->is_prepared[PREPQUERY_DUMPBASETYPE])
	{
		/* Set up query for type-specific details */
		appendPQExpBufferStr(query,
							"PREPARE dumpBaseType(pg_catalog.oid) AS\n");

		appendPQExpBufferStr(query, "SELECT typlen, "
							"typinput, typoutput, typreceive, typsend, "
							"typreceive::pg_catalog.oid AS typreceiveoid, "
							"typsend::pg_catalog.oid AS typsendoid, "
							"typanalyze, "
							"typanalyze::pg_catalog.oid AS typanalyzeoid, "
							"typdelim, typbyval, typalign, typstorage, ");

		if (fout->remoteVersion >= 80300)
			appendPQExpBufferStr(query,
								"typmodin, typmodout, "
								"typmodin::pg_catalog.oid AS typmodinoid, "
								"typmodout::pg_catalog.oid AS typmodoutoid, ");
		else
			appendPQExpBufferStr(query,
								"'-' AS typmodin, '-' AS typmodout, "
								"0 AS typmodinoid, 0 AS typmodoutoid, ");

		if (fout->remoteVersion >= 80400)
			appendPQExpBufferStr(query,
								"typcategory, typispreferred, ");
		else
			appendPQExpBufferStr(query,
								"'U' AS typcategory, false AS typispreferred, ");

		if (fout->remoteVersion >= 90100)
			appendPQExpBufferStr(query, "(typcollation <> 0) AS typcollatable, ");
		else
			appendPQExpBufferStr(query, "false AS typcollatable, ");

		/* Before 8.4, pg_get_expr does not allow 0 for its second arg */
		if (fout->remoteVersion >= 80400)
			appendPQExpBufferStr(query,
								"pg_catalog.pg_get_expr(typdefaultbin, 0) AS typdefaultbin, typdefault ");
		else
			appendPQExpBufferStr(query,
								"pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, typdefault ");

		appendPQExpBuffer(query, "FROM pg_catalog.pg_type "
							"WHERE oid = $1");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPBASETYPE] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpBaseType('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	typlen = PQgetvalue(res, 0, PQfnumber(res, "typlen"));
	typinput = PQgetvalue(res, 0, PQfnumber(res, "typinput"));
	typoutput = PQgetvalue(res, 0, PQfnumber(res, "typoutput"));
	typreceive = PQgetvalue(res, 0, PQfnumber(res, "typreceive"));
	typsend = PQgetvalue(res, 0, PQfnumber(res, "typsend"));
	typmodin = PQgetvalue(res, 0, PQfnumber(res, "typmodin"));
	typmodout = PQgetvalue(res, 0, PQfnumber(res, "typmodout"));
	typanalyze = PQgetvalue(res, 0, PQfnumber(res, "typanalyze"));
	typreceiveoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typreceiveoid")));
	typsendoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typsendoid")));
	typmodinoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodinoid")));
	typmodoutoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typmodoutoid")));
	typanalyzeoid = atooid(PQgetvalue(res, 0, PQfnumber(res, "typanalyzeoid")));
	typcategory = PQgetvalue(res, 0, PQfnumber(res, "typcategory"));
	typispreferred = PQgetvalue(res, 0, PQfnumber(res, "typispreferred"));
	typdelim = PQgetvalue(res, 0, PQfnumber(res, "typdelim"));
	typbyval = PQgetvalue(res, 0, PQfnumber(res, "typbyval"));
	typalign = PQgetvalue(res, 0, PQfnumber(res, "typalign"));
	typstorage = PQgetvalue(res, 0, PQfnumber(res, "typstorage"));
	typcollatable = PQgetvalue(res, 0, PQfnumber(res, "typcollatable"));
	if (!PQgetisnull(res, 0, PQfnumber(res, "typdefaultbin")))
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefaultbin"));
	else if (!PQgetisnull(res, 0, PQfnumber(res, "typdefault")))
	{
		typdefault = PQgetvalue(res, 0, PQfnumber(res, "typdefault"));
		typdefault_is_literal = true;	/* it needs quotes */
	}
	else
		typdefault = NULL;

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	/*
	 * The reason we include CASCADE is that the circular dependency between
	 * the type and its I/O functions makes it impossible to drop the type any
	 * other way.
	 */
	appendPQExpBuffer(delq, "DROP TYPE %s CASCADE;\n", qualtypname);

	/* We might already have a shell type, but setting pg_type_oid is harmless */
	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo);

	appendPQExpBuffer(q,
					  "CREATE TYPE %s (\n"
					  "    INTERNALLENGTH = %s",
					  qualtypname,
					  (strcmp(typlen, "-1") == 0) ? "variable" : typlen);

	/* regproc result is correctly quoted as of 7.3 */
	appendPQExpBuffer(q, ",\n    INPUT = %s", typinput);
	appendPQExpBuffer(q, ",\n    OUTPUT = %s", typoutput);
	if (OidIsValid(typreceiveoid))
		appendPQExpBuffer(q, ",\n    RECEIVE = %s", typreceive);
	if (OidIsValid(typsendoid))
		appendPQExpBuffer(q, ",\n    SEND = %s", typsend);
	if (OidIsValid(typmodinoid))
		appendPQExpBuffer(q, ",\n    TYPMOD_IN = %s", typmodin);
	if (OidIsValid(typmodoutoid))
		appendPQExpBuffer(q, ",\n    TYPMOD_OUT = %s", typmodout);
	if (OidIsValid(typanalyzeoid))
		appendPQExpBuffer(q, ",\n    ANALYZE = %s", typanalyze);

	if (strcmp(typcollatable, "t") == 0)
		appendPQExpBufferStr(q, ",\n    COLLATABLE = true");

	if (typdefault != NULL)
	{
		appendPQExpBufferStr(q, ",\n    DEFAULT = ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	if (OidIsValid(tyinfo->typelem))
	{
		char	   *elemType;

		elemType = getFormattedTypeName(fout, tyinfo->typelem, zeroAsOpaque);
		appendPQExpBuffer(q, ",\n    ELEMENT = %s", elemType);
		free(elemType);
	}

	if (strcmp(typcategory, "U") != 0)
	{
		appendPQExpBufferStr(q, ",\n    CATEGORY = ");
		appendStringLiteralAH(q, typcategory, fout);
	}

	if (strcmp(typispreferred, "t") == 0)
		appendPQExpBufferStr(q, ",\n    PREFERRED = true");

	if (typdelim && strcmp(typdelim, ",") != 0)
	{
		appendPQExpBufferStr(q, ",\n    DELIMITER = ");
		appendStringLiteralAH(q, typdelim, fout);
	}

	if (strcmp(typalign, "c") == 0)
		appendPQExpBufferStr(q, ",\n    ALIGNMENT = char");
	else if (strcmp(typalign, "s") == 0)
		appendPQExpBufferStr(q, ",\n    ALIGNMENT = int2");
	else if (strcmp(typalign, "i") == 0)
		appendPQExpBufferStr(q, ",\n    ALIGNMENT = int4");
	else if (strcmp(typalign, "d") == 0)
		appendPQExpBufferStr(q, ",\n    ALIGNMENT = double");

	if (strcmp(typstorage, "p") == 0)
		appendPQExpBufferStr(q, ",\n    STORAGE = plain");
	else if (strcmp(typstorage, "e") == 0)
		appendPQExpBufferStr(q, ",\n    STORAGE = external");
	else if (strcmp(typstorage, "x") == 0)
		appendPQExpBufferStr(q, ",\n    STORAGE = extended");
	else if (strcmp(typstorage, "m") == 0)
		appendPQExpBufferStr(q, ",\n    STORAGE = main");

	if (strcmp(typbyval, "t") == 0)
		appendPQExpBufferStr(q, ",\n    PASSEDBYVALUE");

	appendPQExpBufferStr(q, "\n);\n");

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"TYPE", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Type Comments and Security Labels */
	dumpComment(fout, "TYPE", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "TYPE", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qtypname);
	free(qualtypname);
}

/*
 * dumpTypeStorageOptions
 *     writes out to fout the ALTER TYPE queries to set default storage options for type
 */
static void
dumpTypeStorageOptions(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q;
	q = createPQExpBuffer();

	appendPQExpBuffer(q, "ALTER TYPE %s.",
					  fmtId(tyinfo->dobj.namespace->dobj.name));
	appendPQExpBuffer(q, "%s SET DEFAULT ENCODING (%s);\n",
					  fmtId(tyinfo->dobj.name),
					  tyinfo->typstorage);

	ArchiveEntry(	fout
	            , tyinfo->dobj.catId                 	         /* catalog ID  */
	            , tyinfo->dobj.dumpId                          /* dump ID     */
	            , tyinfo->dobj.name                            /* type name   */
	            , tyinfo->dobj.namespace->dobj.name            /* name space  */
	            , NULL                                        /* table space */
	            , tyinfo->rolname                             /* owner name  */
	            , false                                       /* with oids   */
	            , "TYPE STORAGE OPTIONS"                      /* Desc        */
				, SECTION_PRE_DATA
	            , q->data                                     /* ALTER...    */
	            , ""                                          /* Del         */
	            , NULL                                        /* Copy        */
	            , NULL                                        /* Deps        */
	            , 0                                           /* num Deps    */
	            , NULL                                        /* Dumper      */
	            , NULL                                        /* Dumper Arg  */
	            );

	destroyPQExpBuffer(q);

}  /* end dumpTypeStorageOptions */

/*
 * dumpDomain
 *	  writes out to fout the queries to recreate a user-defined domain
 */
static void
dumpDomain(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	int			i;
	char	   *qtypname;
	char	   *qualtypname;
	char	   *typnotnull;
	char	   *typdefn;
	char	   *typdefault;
	Oid			typcollation;
	bool		typdefault_is_literal = false;

	if (!fout->is_prepared[PREPQUERY_DUMPDOMAIN])
	{
		/* Set up query for domain-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpDomain(pg_catalog.oid) AS\n");

		if (fout->remoteVersion >= 90100)
		{
			/* typcollation is new in 9.1 */
			appendPQExpBufferStr(query, "SELECT t.typnotnull, "
								 "pg_catalog.format_type(t.typbasetype, t.typtypmod) AS typdefn, "
								 "pg_catalog.pg_get_expr(t.typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, "
								 "t.typdefault, "
								 "CASE WHEN t.typcollation <> u.typcollation "
								 "THEN t.typcollation ELSE 0 END AS typcollation "
								 "FROM pg_catalog.pg_type t "
								 "LEFT JOIN pg_catalog.pg_type u ON (t.typbasetype = u.oid) "
								 "WHERE t.oid = $1");
		}
		else
		{
			appendPQExpBufferStr(query, "SELECT typnotnull, "
								 "pg_catalog.format_type(typbasetype, typtypmod) AS typdefn, "
								 "pg_catalog.pg_get_expr(typdefaultbin, 'pg_catalog.pg_type'::pg_catalog.regclass) AS typdefaultbin, "
								 "typdefault, 0 AS typcollation "
								 "FROM pg_catalog.pg_type "
								 "WHERE oid = $1");
		}

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPDOMAIN] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpDomain('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

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
	typcollation = atooid(PQgetvalue(res, 0, PQfnumber(res, "typcollation")));

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo);

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	appendPQExpBuffer(q,
					  "CREATE DOMAIN %s AS %s",
					  qualtypname,
					  typdefn);

	/* Print collation only if different from base type's collation */
	if (OidIsValid(typcollation))
	{
		CollInfo   *coll;

		coll = findCollationByOid(typcollation);
		if (coll)
			appendPQExpBuffer(q, " COLLATE %s", fmtQualifiedDumpable(coll));
	}

	if (typnotnull[0] == 't')
		appendPQExpBufferStr(q, " NOT NULL");

	if (typdefault != NULL)
	{
		appendPQExpBufferStr(q, " DEFAULT ");
		if (typdefault_is_literal)
			appendStringLiteralAH(q, typdefault, fout);
		else
			appendPQExpBufferStr(q, typdefault);
	}

	PQclear(res);

	/*
	 * Add any CHECK constraints for the domain
	 */
	for (i = 0; i < tyinfo->nDomChecks; i++)
	{
		ConstraintInfo *domcheck = &(tyinfo->domChecks[i]);

		if (!domcheck->separate)
			appendPQExpBuffer(q, "\n\tCONSTRAINT %s %s",
							  fmtId(domcheck->dobj.name), domcheck->condef);
	}

	appendPQExpBufferStr(q, ";\n");

	appendPQExpBuffer(delq, "DROP DOMAIN %s;\n", qualtypname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"DOMAIN", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "DOMAIN", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Domain Comments and Security Labels */
	dumpComment(fout, "DOMAIN", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "DOMAIN", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qtypname);
	free(qualtypname);
}

/*
 * dumpCompositeType
 *	  writes out to fout the queries to recreate a user-defined stand-alone
 *	  composite type
 */
static void
dumpCompositeType(Archive *fout, TypeInfo *tyinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer dropped = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	PQExpBuffer query = createPQExpBuffer();
	PGresult   *res;
	char	   *qtypname;
	char	   *qualtypname;
	int			ntups;
	int			i_attname;
	int			i_atttypdefn;
	int			i_attlen;
	int			i_attalign;
	int			i_attisdropped;
	int			i_attcollation;
	int			i;
	int			actual_atts;

	if (!fout->is_prepared[PREPQUERY_DUMPCOMPOSITETYPE])
	{
		/* Set up query for type-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpCompositeType(pg_catalog.oid) AS\n");

		if (fout->remoteVersion >= 90100)
		{
			/*
			 * attcollation is new in 9.1.  Since we only want to dump COLLATE
			 * clauses for attributes whose collation is different from their
			 * type's default, we use a CASE here to suppress uninteresting
			 * attcollations cheaply.  atttypid will be 0 for dropped columns;
			 * collation does not matter for those.
			 */
			appendPQExpBufferStr(query, "SELECT a.attname, "
								 "pg_catalog.format_type(a.atttypid, a.atttypmod) AS atttypdefn, "
								 "a.attlen, a.attalign, a.attisdropped, "
								 "CASE WHEN a.attcollation <> at.typcollation "
								 "THEN a.attcollation ELSE 0 END AS attcollation "
								 "FROM pg_catalog.pg_type ct "
								 "JOIN pg_catalog.pg_attribute a ON a.attrelid = ct.typrelid "
								 "LEFT JOIN pg_catalog.pg_type at ON at.oid = a.atttypid "
								 "WHERE ct.oid = $1 "
								 "ORDER BY a.attnum");
		}
		else
		{
			/*
			 * Since ALTER TYPE could not drop columns until 9.1, attisdropped
			 * should always be false.
			 */
			appendPQExpBufferStr(query, "SELECT a.attname, "
								 "pg_catalog.format_type(a.atttypid, a.atttypmod) AS atttypdefn, "
								 "a.attlen, a.attalign, a.attisdropped, "
								 "0 AS attcollation "
								 "FROM pg_catalog.pg_type ct, pg_catalog.pg_attribute a "
								 "WHERE ct.oid = $1 "
								 "AND a.attrelid = ct.typrelid "
								 "ORDER BY a.attnum");
		}

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPCOMPOSITETYPE] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpCompositeType('%u')",
					  tyinfo->dobj.catId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_attname = PQfnumber(res, "attname");
	i_atttypdefn = PQfnumber(res, "atttypdefn");
	i_attlen = PQfnumber(res, "attlen");
	i_attalign = PQfnumber(res, "attalign");
	i_attisdropped = PQfnumber(res, "attisdropped");
	i_attcollation = PQfnumber(res, "attcollation");

	if (binary_upgrade)
	{
		binary_upgrade_set_type_oids_by_type_oid(fout, q, tyinfo);
		binary_upgrade_set_pg_class_oids(fout, q, tyinfo->typrelid, false);
	}

	qtypname = pg_strdup(fmtId(tyinfo->dobj.name));
	qualtypname = pg_strdup(fmtQualifiedDumpable(tyinfo));

	appendPQExpBuffer(q, "CREATE TYPE %s AS (",
					  qualtypname);

	actual_atts = 0;
	for (i = 0; i < ntups; i++)
	{
		char	   *attname;
		char	   *atttypdefn;
		char	   *attlen;
		char	   *attalign;
		bool		attisdropped;
		Oid			attcollation;

		attname = PQgetvalue(res, i, i_attname);
		atttypdefn = PQgetvalue(res, i, i_atttypdefn);
		attlen = PQgetvalue(res, i, i_attlen);
		attalign = PQgetvalue(res, i, i_attalign);
		attisdropped = (PQgetvalue(res, i, i_attisdropped)[0] == 't');
		attcollation = atooid(PQgetvalue(res, i, i_attcollation));

		if (attisdropped && !binary_upgrade)
			continue;

		/* Format properly if not first attr */
		if (actual_atts++ > 0)
			appendPQExpBufferChar(q, ',');
		appendPQExpBufferStr(q, "\n\t");

		if (!attisdropped)
		{
			appendPQExpBuffer(q, "%s %s", fmtId(attname), atttypdefn);

			/* Add collation if not default for the column type */
			if (OidIsValid(attcollation))
			{
				CollInfo   *coll;

				coll = findCollationByOid(attcollation);
				if (coll)
					appendPQExpBuffer(q, " COLLATE %s",
									  fmtQualifiedDumpable(coll));
			}
		}
		else
		{
			/*
			 * This is a dropped attribute and we're in binary_upgrade mode.
			 * Insert a placeholder for it in the CREATE TYPE command, and set
			 * length and alignment with direct UPDATE to the catalogs
			 * afterwards. See similar code in dumpTableSchema().
			 */
			appendPQExpBuffer(q, "%s INTEGER /* dummy */", fmtId(attname));

			/* stash separately for insertion after the CREATE TYPE */
			appendPQExpBufferStr(dropped,
					  "\n-- For binary upgrade, recreate dropped column.\n");
			appendPQExpBuffer(dropped, "SET allow_system_table_mods = true;\n");
			appendPQExpBuffer(dropped, "UPDATE pg_catalog.pg_attribute\n"
							  "SET attlen = %s, "
							  "attalign = '%s', attbyval = false\n"
							  "WHERE attname = ", attlen, attalign);
			appendStringLiteralAH(dropped, attname, fout);
			appendPQExpBufferStr(dropped, "\n  AND attrelid = ");
			appendStringLiteralAH(dropped, qualtypname, fout);
			appendPQExpBufferStr(dropped, "::pg_catalog.regclass;\n");

			appendPQExpBuffer(dropped, "ALTER TYPE %s ",
							  qualtypname);
			appendPQExpBuffer(dropped, "DROP ATTRIBUTE %s;\n",
							  fmtId(attname));
			appendPQExpBuffer(dropped, "RESET allow_system_table_mods;\n");
		}
	}
	appendPQExpBufferStr(q, "\n);\n");
	appendPQExpBufferStr(q, dropped->data);

	appendPQExpBuffer(delq, "DROP TYPE %s;\n", qualtypname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tyinfo->dobj,
										"TYPE", qtypname,
										tyinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId,
				 tyinfo->dobj.name,
				 tyinfo->dobj.namespace->dobj.name,
				 NULL,
				 tyinfo->rolname, false,
				 "TYPE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);


	/* Dump Type Comments and Security Labels */
	dumpComment(fout, "TYPE", qtypname,
				tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);
	dumpSecLabel(fout, "TYPE", qtypname,
				 tyinfo->dobj.namespace->dobj.name, tyinfo->rolname,
				 tyinfo->dobj.catId, 0, tyinfo->dobj.dumpId);

	dumpACL(fout, tyinfo->dobj.catId, tyinfo->dobj.dumpId, "TYPE",
			qtypname, NULL,
			tyinfo->dobj.namespace->dobj.name,
			tyinfo->rolname, tyinfo->typacl);

	PQclear(res);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(dropped);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qtypname);
	free(qualtypname);

	/* Dump any per-column comments */
	dumpCompositeTypeColComments(fout, tyinfo);
}

/*
 * dumpCompositeTypeColComments
 *	  writes out to fout the queries to recreate comments on the columns of
 *	  a user-defined stand-alone composite type
 */
static void
dumpCompositeTypeColComments(Archive *fout, TypeInfo *tyinfo)
{
	CommentItem *comments;
	int			ncomments;
	PGresult   *res;
	PQExpBuffer query;
	PQExpBuffer target;
	Oid			pgClassOid;
	int			i;
	int			ntups;
	int			i_attname;
	int			i_attnum;

	query = createPQExpBuffer();

	/* We assume here that remoteVersion must be at least 70300 */
	appendPQExpBuffer(query,
					  "SELECT c.tableoid, a.attname, a.attnum "
					  "FROM pg_catalog.pg_class c, pg_catalog.pg_attribute a "
					  "WHERE c.oid = '%u' AND c.oid = a.attrelid "
					  "  AND NOT a.attisdropped "
					  "ORDER BY a.attnum ",
					  tyinfo->typrelid);

	/* Fetch column attnames */
	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	if (ntups < 1)
	{
		PQclear(res);
		destroyPQExpBuffer(query);
		return;
	}

	pgClassOid = atooid(PQgetvalue(res, 0, PQfnumber(res, "tableoid")));

	/* Search for comments associated with type's pg_class OID */
	ncomments = findComments(fout,
							 pgClassOid,
							 tyinfo->typrelid,
							 &comments);

	/* If no comments exist, we're done */
	if (ncomments <= 0)
	{
		PQclear(res);
		destroyPQExpBuffer(query);
		return;
	}

	/* Build COMMENT ON statements */
	target = createPQExpBuffer();

	i_attnum = PQfnumber(res, "attnum");
	i_attname = PQfnumber(res, "attname");
	while (ncomments > 0)
	{
		const char *attname;

		attname = NULL;
		for (i = 0; i < ntups; i++)
		{
			if (atoi(PQgetvalue(res, i, i_attnum)) == comments->objsubid)
			{
				attname = PQgetvalue(res, i, i_attname);
				break;
			}
		}
		if (attname)			/* just in case we don't find it */
		{
			const char *descr = comments->descr;

			resetPQExpBuffer(target);
			appendPQExpBuffer(target, "COLUMN %s.",
							  fmtId(tyinfo->dobj.name));
			appendPQExpBufferStr(target, fmtId(attname));

			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "COMMENT ON COLUMN %s.",
							  fmtQualifiedDumpable(tyinfo));
			appendPQExpBuffer(query, "%s IS ", fmtId(attname));
			appendStringLiteralAH(query, descr, fout);
			appendPQExpBufferStr(query, ";\n");

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 target->data,
						 tyinfo->dobj.namespace->dobj.name,
						 NULL, tyinfo->rolname,
						 false, "COMMENT", SECTION_NONE,
						 query->data, "", NULL,
						 &(tyinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}

		comments++;
		ncomments--;
	}

	PQclear(res);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(target);
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
	 * is driven off the base type entry, instead.  This interacts with
	 * _printTocEntry()'s use of the presence of a DROP command to decide
	 * whether an entry needs an ALTER OWNER command.  We don't want to alter
	 * the shell type's owner immediately on creation; that should happen only
	 * after it's filled in, otherwise the backend complains.
	 */

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_type_oid(fout, q, stinfo->baseType);

	appendPQExpBuffer(q, "CREATE TYPE %s;\n",
					  fmtQualifiedDumpable(stinfo));

	ArchiveEntry(fout, stinfo->dobj.catId, stinfo->dobj.dumpId,
				 stinfo->dobj.name,
				 stinfo->dobj.namespace->dobj.name,
				 NULL,
				 stinfo->baseType->rolname, false,
				 "SHELL TYPE", SECTION_PRE_DATA,
				 q->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(q);
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
	FuncInfo   *funcInfo;
	FuncInfo   *inlineInfo = NULL;
	FuncInfo   *validatorInfo = NULL;

	/* Skip if not to be dumped */
	if (!plang->dobj.dump || dataOnly)
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

	if (OidIsValid(plang->laninline))
	{
		inlineInfo = findFuncByOid(plang->laninline);
		if (inlineInfo != NULL && !inlineInfo->dobj.dump)
			inlineInfo = NULL;
	}

	if (OidIsValid(plang->lanvalidator))
	{
		validatorInfo = findFuncByOid(plang->lanvalidator);
		if (validatorInfo != NULL && !validatorInfo->dobj.dump)
			validatorInfo = NULL;
	}

	/*
	 * If the functions are dumpable then emit a traditional CREATE LANGUAGE
	 * with parameters.  Otherwise, we'll write a parameterless command, which
	 * will rely on data from pg_pltemplate.
	 */
	useParams = (funcInfo != NULL &&
				 (inlineInfo != NULL || !OidIsValid(plang->laninline)) &&
				 (validatorInfo != NULL || !OidIsValid(plang->lanvalidator)));

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();

	qlanname = pg_strdup(fmtId(plang->dobj.name));

	appendPQExpBuffer(delqry, "DROP PROCEDURAL LANGUAGE %s;\n",
					  qlanname);

	if (useParams)
	{
		appendPQExpBuffer(defqry, "CREATE %sPROCEDURAL LANGUAGE %s",
						  plang->lanpltrusted ? "TRUSTED " : "",
						  qlanname);
		appendPQExpBuffer(defqry, " HANDLER %s",
						  fmtQualifiedDumpable(funcInfo));
		if (OidIsValid(plang->laninline))
			appendPQExpBuffer(defqry, " INLINE %s",
							  fmtQualifiedDumpable(inlineInfo));
		if (OidIsValid(plang->lanvalidator))
			appendPQExpBuffer(defqry, " VALIDATOR %s",
							  fmtQualifiedDumpable(validatorInfo));
	}
	else
	{
		/*
		 * If not dumping parameters, then use CREATE OR REPLACE so that the
		 * command will not fail if the language is preinstalled in the target
		 * database.  We restrict the use of REPLACE to this case so as to
		 * eliminate the risk of replacing a language with incompatible
		 * parameter settings: this command will only succeed at all if there
		 * is a pg_pltemplate entry, and if there is one, the existing entry
		 * must match it too.
		 */
		appendPQExpBuffer(defqry, "CREATE OR REPLACE PROCEDURAL LANGUAGE %s",
						  qlanname);
	}
	appendPQExpBufferStr(defqry, ";\n");

	if (binary_upgrade)
		binary_upgrade_extension_member(defqry, &plang->dobj,
										"LANGUAGE", qlanname, NULL);

	ArchiveEntry(fout, plang->dobj.catId, plang->dobj.dumpId,
				 plang->dobj.name,
				 NULL, NULL, plang->lanowner,
				 false, "PROCEDURAL LANGUAGE", SECTION_PRE_DATA,
				 defqry->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Proc Lang Comments and Security Labels */
	dumpComment(fout, "LANGUAGE", qlanname,
				NULL, plang->lanowner,
				plang->dobj.catId, 0, plang->dobj.dumpId);
	dumpSecLabel(fout, "LANGUAGE", qlanname,
				 NULL, plang->lanowner,
				 plang->dobj.catId, 0, plang->dobj.dumpId);

	if (plang->lanpltrusted)
		dumpACL(fout, plang->dobj.catId, plang->dobj.dumpId, "LANGUAGE",
				qlanname, NULL, NULL,
				plang->lanowner, plang->lanacl);

	free(qlanname);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
}

/*
 * format_function_arguments: generate function name and argument list
 *
 * This is used when we can rely on pg_get_function_arguments to format
 * the argument list.  Note, however, that pg_get_function_arguments
 * does not special-case zero-argument aggregates.
 */
static char *
format_function_arguments(FuncInfo *finfo, char *funcargs, bool is_agg)
{
	PQExpBufferData fn;

	initPQExpBuffer(&fn);
	appendPQExpBufferStr(&fn, fmtId(finfo->dobj.name));
	if (is_agg && finfo->nargs == 0)
		appendPQExpBufferStr(&fn, "(*)");
	else
		appendPQExpBuffer(&fn, "(%s)", funcargs);
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
format_table_function_columns(Archive *fout, FuncInfo *finfo, int nallargs,
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
			typname = getFormattedTypeName(fout, typid, zeroAsOpaque);

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
 * Generates a minimal list of input argument types; this is sufficient to
 * reference the function, but not to define it.
 *
 * If honor_quotes is false then the function name is never quoted.
 * This is appropriate for use in TOC tags, but not in SQL commands.
 */
static char *
format_function_signature(Archive *fout, FuncInfo *finfo, bool honor_quotes)
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

		if (j > 0)
			appendPQExpBufferStr(&fn, ", ");

		typname = getFormattedTypeName(fout, finfo->argtypes[j],
									   zeroAsOpaque);
		appendPQExpBufferStr(&fn, typname);
		free(typname);
	}
	appendPQExpBufferChar(&fn, ')');
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
	char	   *funcsig;		/* identity signature */
	char	   *funcfullsig = NULL;		/* full signature */
	char	   *funcsig_tag;
	char	   *qual_funcsig;
	char	   *proretset;
	char	   *prosrc;
	char	   *probin;
	char	   *funcargs;
	char	   *funciargs;
	char	   *funcresult;
	char	   *proiswindow;
	char	   *provolatile;
	char	   *proisstrict;
	char	   *prosecdef;
	char	   *proleakproof;
	char	   *proconfig;
	char	   *procost;
	char	   *prorows;
	char	   *lanname;
	char	   *callbackfunc;
	char	   *prodataaccess;
	char	   *proexeclocation;
	char	   *rettypename;
	int			nallargs;
	char	  **allargtypes = NULL;
	char	  **argmodes = NULL;
	char	  **argnames = NULL;
	char	  **configitems = NULL;
	int			nconfigitems = 0;
	int			i;

	/* Skip if not to be dumped */
	if (!finfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delqry = createPQExpBuffer();
	asPart = createPQExpBuffer();

	if (!fout->is_prepared[PREPQUERY_DUMPFUNC])
	{
		/* Set up query for function-specific details */
		appendPQExpBufferStr(query, "PREPARE dumpFunc(pg_catalog.oid) AS\n");
		/* Fetch function-specific details */
		appendPQExpBuffer(query,
							"SELECT\n"
							"proretset,\n"
							"prosrc,\n"
							"probin,\n"
							"provolatile,\n"
							"proisstrict,\n"
							"prosecdef,\n"
							"(SELECT lanname FROM pg_catalog.pg_language WHERE oid = prolang) AS lanname,\n "
							"proconfig,\n"
							"procost,\n"
							"prorows,\n"
							"prodataaccess,\n"
							"pg_catalog.pg_get_function_arguments(oid) AS funcargs,\n"
							"pg_catalog.pg_get_function_identity_arguments(oid) AS funciargs,\n"
							"pg_catalog.pg_get_function_result(oid) AS funcresult,\n"
							"(SELECT procallback FROM pg_catalog.pg_proc_callback WHERE profnoid::pg_catalog.oid = oid) as callbackfunc,\n");

		if (fout->remoteVersion >= 90200)
			appendPQExpBuffer(query,
								"proleakproof,\n");
		else
			appendPQExpBuffer(query,
								"false AS proleakproof,\n");

		if (fout->remoteVersion >= 80400)
				appendPQExpBuffer(query,
								"proiswindow,\n");
		else
				appendPQExpBuffer(query,
								"proiswin as proiswindow,\n");

		/* GPDB6 added proexeclocation */
		if (fout->remoteVersion >= 90400)
				appendPQExpBuffer(query,
								"proexeclocation\n");
		else
				appendPQExpBuffer(query,
								"'a' as proexeclocation\n");

		appendPQExpBuffer(query,
							"FROM pg_catalog.pg_proc "
							"WHERE oid = $1");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPFUNC] = true;

	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpFunc('%u')",
					  finfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	proretset = PQgetvalue(res, 0, PQfnumber(res, "proretset"));
	prosrc = PQgetvalue(res, 0, PQfnumber(res, "prosrc"));
	probin = PQgetvalue(res, 0, PQfnumber(res, "probin"));
	funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
	funciargs = PQgetvalue(res, 0, PQfnumber(res, "funciargs"));
	funcresult = PQgetvalue(res, 0, PQfnumber(res, "funcresult"));
	proiswindow = PQgetvalue(res, 0, PQfnumber(res, "proiswindow"));
	provolatile = PQgetvalue(res, 0, PQfnumber(res, "provolatile"));
	proisstrict = PQgetvalue(res, 0, PQfnumber(res, "proisstrict"));
	prosecdef = PQgetvalue(res, 0, PQfnumber(res, "prosecdef"));
	proleakproof = PQgetvalue(res, 0, PQfnumber(res, "proleakproof"));
	proconfig = PQgetvalue(res, 0, PQfnumber(res, "proconfig"));
	procost = PQgetvalue(res, 0, PQfnumber(res, "procost"));
	prorows = PQgetvalue(res, 0, PQfnumber(res, "prorows"));
	lanname = PQgetvalue(res, 0, PQfnumber(res, "lanname"));
	callbackfunc = PQgetvalue(res, 0, PQfnumber(res, "callbackfunc"));
	prodataaccess = PQgetvalue(res, 0, PQfnumber(res, "prodataaccess"));
	proexeclocation = PQgetvalue(res, 0, PQfnumber(res, "proexeclocation"));

	/*
	 * See backend/commands/define.c for details of how the 'AS' clause is
	 * used. In GPDB Paris and up, an unused probin is NULL (here ""); previous8bc709b37411ba7ad0fd0f1f79c354714424af3d
	 * versions would set it to "-".  There are no known cases in which prosrc
	 * is unused, so the tests below for "-" are probably useless.
	 */
	if (probin[0] != '\0' && strcmp(probin, "-") != 0)
	{
		appendPQExpBufferStr(asPart, "AS ");
		appendStringLiteralAH(asPart, probin, fout);
		if (strcmp(prosrc, "-") != 0)
		{
			appendPQExpBufferStr(asPart, ", ");

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
			appendPQExpBufferStr(asPart, "AS ");
			/* with no bin, dollar quote src unconditionally if allowed */
			if (disable_dollar_quoting)
				appendStringLiteralAH(asPart, prosrc, fout);
			else
				appendStringLiteralDQ(asPart, prosrc, NULL);
		}
	}

	nallargs = finfo->nargs;	/* unless we learn different from allargs */
	funcfullsig = format_function_arguments(finfo, funcargs, false);
	funcsig = format_function_arguments(finfo, funciargs, false);

	funcsig_tag = format_function_signature(fout, finfo, false);

	if (proconfig && *proconfig)
	{
		if (!parsePGArray(proconfig, &configitems, &nconfigitems))
		{
			write_msg(NULL, "WARNING: could not parse proconfig array\n");
			if (configitems)
				free(configitems);
			configitems = NULL;
			nconfigitems = 0;
		}
	}

	qual_funcsig = psprintf("%s.%s",
							fmtId(finfo->dobj.namespace->dobj.name),
							funcsig);

	appendPQExpBuffer(delqry, "DROP FUNCTION %s;\n", qual_funcsig);

	appendPQExpBuffer(q, "CREATE FUNCTION %s.%s ",
					  fmtId(finfo->dobj.namespace->dobj.name),
					  funcfullsig ? funcfullsig :
					  funcsig);

	if (funcresult)
		appendPQExpBuffer(q, "RETURNS %s", funcresult);
	else
	{
		/* switch between RETURNS SETOF RECORD and RETURNS TABLE functions */
		if (!is_returns_table_function(nallargs, argmodes))
		{
			rettypename = getFormattedTypeName(fout, finfo->prorettype,
											   zeroAsOpaque);
			appendPQExpBuffer(q, "RETURNS %s%s",
							  (proretset[0] == 't') ? "SETOF " : "",
							  rettypename);
			free(rettypename);
		}
		else
		{
			char	   *func_cols;
			func_cols = format_table_function_columns(fout, finfo, nallargs, allargtypes,
													  argmodes, argnames);
			appendPQExpBuffer(q, "RETURNS TABLE %s", func_cols);
			free(func_cols);
		}
	}

	appendPQExpBuffer(q, "\n    LANGUAGE %s", fmtId(lanname));

	if (proiswindow[0] == 't')
		appendPQExpBufferStr(q, " WINDOW");

	if (provolatile[0] != PROVOLATILE_VOLATILE)
	{
		if (provolatile[0] == PROVOLATILE_IMMUTABLE)
			appendPQExpBufferStr(q, " IMMUTABLE");
		else if (provolatile[0] == PROVOLATILE_STABLE)
			appendPQExpBufferStr(q, " STABLE");
		else if (provolatile[0] != PROVOLATILE_VOLATILE)
			exit_horribly(NULL, "unrecognized provolatile value for function \"%s\"\n",
						  finfo->dobj.name);
	}

	if (proisstrict[0] == 't')
		appendPQExpBufferStr(q, " STRICT");

	if (prosecdef[0] == 't')
		appendPQExpBufferStr(q, " SECURITY DEFINER");

	if (proleakproof[0] == 't')
		appendPQExpBufferStr(q, " LEAKPROOF");

	/*
	 * COST and ROWS are emitted only if present and not default, so as not to
	 * break backwards-compatibility of the dump without need.  Keep this code
	 * in sync with the defaults in functioncmds.c.
	 */
	if (strcmp(procost, "0") != 0)
	{
		if (strcmp(lanname, "internal") == 0 || strcmp(lanname, "c") == 0)
		{
			/* default cost is 1 */
			if (strcmp(procost, "1") != 0)
				appendPQExpBuffer(q, " COST %s", procost);
		}
		else
		{
			/* default cost is 100 */
			if (strcmp(procost, "100") != 0)
				appendPQExpBuffer(q, " COST %s", procost);
		}
	}
	if (proretset[0] == 't' &&
		strcmp(prorows, "0") != 0 && strcmp(prorows, "1000") != 0)
		appendPQExpBuffer(q, " ROWS %s", prorows);

	if (prodataaccess[0] == PRODATAACCESS_NONE)
		appendPQExpBuffer(q, " NO SQL");
	else if (prodataaccess[0] == PRODATAACCESS_CONTAINS)
		appendPQExpBuffer(q, " CONTAINS SQL");
	else if (prodataaccess[0] == PRODATAACCESS_READS)
		appendPQExpBuffer(q, " READS SQL DATA");
	else if (prodataaccess[0] == PRODATAACCESS_MODIFIES)
		appendPQExpBuffer(q, " MODIFIES SQL DATA");

	if (proexeclocation[0] == PROEXECLOCATION_ANY)
	{
		/* the default, omit */
	}
	else if (proexeclocation[0] == PROEXECLOCATION_MASTER)
		appendPQExpBuffer(q, " EXECUTE ON MASTER");
	else if (proexeclocation[0] == PROEXECLOCATION_ALL_SEGMENTS)
		appendPQExpBuffer(q, " EXECUTE ON ALL SEGMENTS");
	else if (proexeclocation[0] == PROEXECLOCATION_INITPLAN)
		appendPQExpBuffer(q, " EXECUTE ON INITPLAN");
	else
	{
		write_msg(NULL, "unrecognized proexeclocation value: %c\n", proexeclocation[0]);
		exit_nicely(1);
	}

	for (i = 0; i < nconfigitems; i++)
	{
		/* we feel free to scribble on configitems[] here */
		char	   *configitem = configitems[i];
		char	   *pos;

		pos = strchr(configitem, '=');
		if (pos == NULL)
			continue;
		*pos++ = '\0';
		appendPQExpBuffer(q, "\n    SET %s TO ", fmtId(configitem));

		/*
		 * Variables that are marked GUC_LIST_QUOTE were already fully quoted
		 * by flatten_set_variable_args() before they were put into the
		 * proconfig array.  However, because the quoting rules used there
		 * aren't exactly like SQL's, we have to break the list value apart
		 * and then quote the elements as string literals.  (The elements may
		 * be double-quoted as-is, but we can't just feed them to the SQL
		 * parser; it would do the wrong thing with elements that are
		 * zero-length or longer than NAMEDATALEN.)
		 *
		 * Variables that are not so marked should just be emitted as simple
		 * string literals.  If the variable is not known to
		 * variable_is_guc_list_quote(), we'll do that; this makes it unsafe
		 * to use GUC_LIST_QUOTE for extension variables.
		 */
		if (variable_is_guc_list_quote(configitem))
		{
			char	  **namelist;
			char	  **nameptr;

			/* Parse string into list of identifiers */
			/* this shouldn't fail really */
			if (SplitGUCList(pos, ',', &namelist))
			{
				for (nameptr = namelist; *nameptr; nameptr++)
				{
					if (nameptr != namelist)
						appendPQExpBufferStr(q, ", ");
					appendStringLiteralAH(q, *nameptr, fout);
				}
			}
			pg_free(namelist);
		}
		else
			appendStringLiteralAH(q, pos, fout);
	}

	appendPQExpBuffer(q, "\n    %s", asPart->data);

	/* Append callback function */
	if (callbackfunc && callbackfunc[0] != '\0')
		appendPQExpBuffer(q, "\n    WITH (describe = %s)", callbackfunc);

	appendPQExpBuffer(q, ";\n");

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &finfo->dobj,
										"FUNCTION", funcsig,
										finfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, finfo->dobj.catId, finfo->dobj.dumpId,
				 funcsig_tag,
				 finfo->dobj.namespace->dobj.name,
				 NULL,
				 finfo->rolname, false,
				 "FUNCTION", SECTION_PRE_DATA,
				 q->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Function Comments and Security Labels */
	dumpComment(fout, "FUNCTION", funcsig,
				finfo->dobj.namespace->dobj.name, finfo->rolname,
				finfo->dobj.catId, 0, finfo->dobj.dumpId);
	dumpSecLabel(fout, "FUNCTION", funcsig,
				 finfo->dobj.namespace->dobj.name, finfo->rolname,
				 finfo->dobj.catId, 0, finfo->dobj.dumpId);

	dumpACL(fout, finfo->dobj.catId, finfo->dobj.dumpId, "FUNCTION",
			funcsig, NULL,
			finfo->dobj.namespace->dobj.name,
			finfo->rolname, finfo->proacl);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(asPart);
	free(funcsig);
	if (funcfullsig)
		free(funcfullsig);
	free(funcsig_tag);
	free(qual_funcsig);
	if (allargtypes)
		free(allargtypes);
	if (argmodes)
		free(argmodes);
	if (argnames)
		free(argnames);
	if (configitems)
		free(configitems);
}


/*
 * Dump a user-defined cast
 */
static void
dumpCast(Archive *fout, CastInfo *cast)
{
	PQExpBuffer defqry;
	PQExpBuffer delqry;
	PQExpBuffer labelq;
	PQExpBuffer castargs;
	FuncInfo   *funcInfo = NULL;
	char	   *sourceType;
	char	   *targetType;

	/* Skip if not to be dumped */
	if (!cast->dobj.dump || dataOnly)
		return;

	/* Cannot dump if we don't have the cast function's info */
	if (OidIsValid(cast->castfunc))
	{
		funcInfo = findFuncByOid(cast->castfunc);
		if (funcInfo == NULL)
			exit_horribly(NULL, "could not find function definition for function with OID %u\n",
						  cast->castfunc);
	}

	defqry = createPQExpBuffer();
	delqry = createPQExpBuffer();
	labelq = createPQExpBuffer();
	castargs = createPQExpBuffer();

	sourceType = getFormattedTypeName(fout, cast->castsource, zeroAsNone);
	targetType = getFormattedTypeName(fout, cast->casttarget, zeroAsNone);
	appendPQExpBuffer(delqry, "DROP CAST (%s AS %s);\n",
					  sourceType, targetType);

	appendPQExpBuffer(defqry, "CREATE CAST (%s AS %s) ",
					  sourceType, targetType);

	switch (cast->castmethod)
	{
		case COERCION_METHOD_BINARY:
			appendPQExpBufferStr(defqry, "WITHOUT FUNCTION");
			break;
		case COERCION_METHOD_INOUT:
			appendPQExpBufferStr(defqry, "WITH INOUT");
			break;
		case COERCION_METHOD_FUNCTION:
			if (funcInfo)
			{
				char	   *fsig = format_function_signature(fout, funcInfo, true);

				/*
				 * Always qualify the function name (format_function_signature
				 * won't qualify it).
				 */
				appendPQExpBuffer(defqry, "WITH FUNCTION %s.%s",
						   fmtId(funcInfo->dobj.namespace->dobj.name), fsig);
				free(fsig);
			}
			else
				write_msg(NULL, "WARNING: bogus value in pg_cast.castfunc or pg_cast.castmethod field\n");
			break;
		default:
			write_msg(NULL, "WARNING: bogus value in pg_cast.castmethod field\n");
	}

	if (cast->castcontext == 'a')
		appendPQExpBufferStr(defqry, " AS ASSIGNMENT");
	else if (cast->castcontext == 'i')
		appendPQExpBufferStr(defqry, " AS IMPLICIT");
	appendPQExpBufferStr(defqry, ";\n");

	appendPQExpBuffer(labelq, "CAST (%s AS %s)",
					  sourceType, targetType);

	appendPQExpBuffer(castargs, "(%s AS %s)",
					  sourceType, targetType);

	if (binary_upgrade)
		binary_upgrade_extension_member(defqry, &cast->dobj,
										"CAST", castargs->data, NULL);

	ArchiveEntry(fout, cast->dobj.catId, cast->dobj.dumpId,
				 labelq->data,
				 NULL, NULL, "",
				 false, "CAST", SECTION_PRE_DATA,
				 defqry->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Cast Comments */
	dumpComment(fout, "CAST", castargs->data,
				NULL, "",
				cast->dobj.catId, 0, cast->dobj.dumpId);

	free(sourceType);
	free(targetType);

	destroyPQExpBuffer(defqry);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(labelq);
	destroyPQExpBuffer(castargs);
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
	char	   *oprregproc;
	char	   *oprref;

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

	if (!fout->is_prepared[PREPQUERY_DUMPOPR])
	{
		/* Set up query for operator-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpOpr(pg_catalog.oid) AS\n");

		appendPQExpBuffer(query, "SELECT oprkind, "
							"oprcode::pg_catalog.regprocedure, "
							"oprleft::pg_catalog.regtype, "
							"oprright::pg_catalog.regtype, "
							"oprcom, "
							"oprnegate, "
							"oprrest::pg_catalog.regprocedure, "
							"oprjoin::pg_catalog.regprocedure, "
							"oprcanmerge, oprcanhash "
							"FROM pg_catalog.pg_operator "
							"WHERE oid = $1");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPOPR] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpOpr('%u')",
					  oprinfo->dobj.catId.oid);


	res = ExecuteSqlQueryForSingleRow(fout, query->data);

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

	oprregproc = convertRegProcReference(fout, oprcode);
	if (oprregproc)
	{
		appendPQExpBuffer(details, "    PROCEDURE = %s", oprregproc);
		free(oprregproc);
	}

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
		appendPQExpBufferStr(oprid, name);
	}
	else
		appendPQExpBufferStr(oprid, "NONE");

	if (strcmp(oprkind, "l") == 0 ||
		strcmp(oprkind, "b") == 0)
	{
		name = oprright;

		appendPQExpBuffer(details, ",\n    RIGHTARG = %s", name);
		appendPQExpBuffer(oprid, ", %s)", name);
	}
	else
		appendPQExpBufferStr(oprid, ", NONE)");

	oprref = getFormattedOperatorName(fout, oprcom);
	if (oprref)
	{
		appendPQExpBuffer(details, ",\n    COMMUTATOR = %s", oprref);
		free(oprref);
	}

	oprref = getFormattedOperatorName(fout, oprnegate);
	if (oprref)
	{
		appendPQExpBuffer(details, ",\n    NEGATOR = %s", oprref);
		free(oprref);
	}

	if (strcmp(oprcanmerge, "t") == 0)
		appendPQExpBufferStr(details, ",\n    MERGES");

	if (strcmp(oprcanhash, "t") == 0)
		appendPQExpBufferStr(details, ",\n    HASHES");

	oprregproc = convertRegProcReference(fout, oprrest);
	if (oprregproc)
	{
		appendPQExpBuffer(details, ",\n    RESTRICT = %s", oprregproc);
		free(oprregproc);
	}

	oprregproc = convertRegProcReference(fout, oprjoin);
	if (oprregproc)
	{
		appendPQExpBuffer(details, ",\n    JOIN = %s", oprregproc);
		free(oprregproc);
	}

	appendPQExpBuffer(delq, "DROP OPERATOR %s.%s;\n",
					  fmtId(oprinfo->dobj.namespace->dobj.name),
					  oprid->data);

	appendPQExpBuffer(q, "CREATE OPERATOR %s.%s (\n%s\n);\n",
					  fmtId(oprinfo->dobj.namespace->dobj.name),
					  oprinfo->dobj.name, details->data);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &oprinfo->dobj,
										"OPERATOR", oprid->data,
										oprinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, oprinfo->dobj.catId, oprinfo->dobj.dumpId,
				 oprinfo->dobj.name,
				 oprinfo->dobj.namespace->dobj.name,
				 NULL,
				 oprinfo->rolname,
				 false, "OPERATOR", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Operator Comments */
	dumpComment(fout, "OPERATOR", oprid->data,
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
 * Returns allocated string of what to print, or NULL if function references
 * is InvalidOid. Returned string is expected to be free'd by the caller.
 *
 * In 7.3 the input is a REGPROCEDURE display; we have to strip the
 * argument-types part.  In prior versions, the input is a REGPROC display.
 */
static char *
convertRegProcReference(Archive *fout, const char *proc)
{
	/* In all cases "-" means a null reference */
	if (strcmp(proc, "-") == 0)
		return NULL;

	char	   *name;
	char	   *paren;
	bool		inquote;

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
 * getFormattedOperatorName - retrieve the operator name for the
 * given operator OID (presented in string form).
 *
 * Returns an allocated string, or NULL if the given OID is InvalidOid.
 * Caller is responsible for free'ing result string.
 *
 * What we produce has the format "OPERATOR(schema.oprname)".  This is only
 * useful in commands where the operator's argument types can be inferred from
 * context.  We always schema-qualify the name, though.  The predecessor to
 * this code tried to skip the schema qualification if possible, but that led
 * to wrong results in corner cases, such as if an operator and its negator
 * are in different schemas.
 */
static char *
getFormattedOperatorName(Archive *fout, const char *oproid)
{
	OprInfo    *oprInfo;

	/* In all cases "0" means a null reference */
	if (strcmp(oproid, "0") == 0)
		return NULL;

	oprInfo = findOprByOid(atooid(oproid));
	if (oprInfo == NULL)
	{
		write_msg(NULL, "WARNING: could not find operator with OID %s\n",
					oproid);
		return NULL;
	}

	return psprintf("OPERATOR(%s.%s)",
					fmtId(oprInfo->dobj.namespace->dobj.name),
					oprInfo->dobj.name);
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
convertTSFunction(Archive *fout, Oid funcOid)
{
	char	   *result;
	char		query[128];
	PGresult   *res;

	snprintf(query, sizeof(query),
			 "SELECT '%u'::pg_catalog.regproc", funcOid);
	res = ExecuteSqlQueryForSingleRow(fout, query);

	result = pg_strdup(PQgetvalue(res, 0, 0));

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
	PQExpBuffer nameusing;
	PGresult   *res;
	int			ntups;
	int			i_opcintype;
	int			i_opckeytype;
	int			i_opcdefault;
	int			i_opcfamily;
	int			i_opcfamilyname;
	int			i_opcfamilynsp;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_sortfamily;
	int			i_sortfamilynsp;
	int			i_amprocnum;
	int			i_amproc;
	int			i_amproclefttype;
	int			i_amprocrighttype;
	char	   *opcintype;
	char	   *opckeytype;
	char	   *opcdefault;
	char	   *opcfamily;
	char	   *opcfamilyname;
	char	   *opcfamilynsp;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *sortfamily;
	char	   *sortfamilynsp;
	char	   *amprocnum;
	char	   *amproc;
	char	   *amproclefttype;
	char	   *amprocrighttype;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opcinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	nameusing = createPQExpBuffer();

	/* Get additional fields from the pg_opclass row */
	appendPQExpBuffer(query, "SELECT opcintype::pg_catalog.regtype, "
						"opckeytype::pg_catalog.regtype, "
						"opcdefault, opcfamily, "
						"opfname AS opcfamilyname, "
						"nspname AS opcfamilynsp, "
						"(SELECT amname FROM pg_catalog.pg_am WHERE oid = opcmethod) AS amname "
						"FROM pg_catalog.pg_opclass c "
					"LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = opcfamily "
				"LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
						"WHERE c.oid = '%u'::pg_catalog.oid",
						opcinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	i_opcintype = PQfnumber(res, "opcintype");
	i_opckeytype = PQfnumber(res, "opckeytype");
	i_opcdefault = PQfnumber(res, "opcdefault");
	i_opcfamily = PQfnumber(res, "opcfamily");
	i_opcfamilyname = PQfnumber(res, "opcfamilyname");
	i_opcfamilynsp = PQfnumber(res, "opcfamilynsp");
	i_amname = PQfnumber(res, "amname");

	/* opcintype may still be needed after we PQclear res */
	opcintype = pg_strdup(PQgetvalue(res, 0, i_opcintype));
	opckeytype = PQgetvalue(res, 0, i_opckeytype);
	opcdefault = PQgetvalue(res, 0, i_opcdefault);
	/* opcfamily will still be needed after we PQclear res */
	opcfamily = pg_strdup(PQgetvalue(res, 0, i_opcfamily));
	opcfamilyname = PQgetvalue(res, 0, i_opcfamilyname);
	opcfamilynsp = PQgetvalue(res, 0, i_opcfamilynsp);
	/* amname will still be needed after we PQclear res */
	amname = pg_strdup(PQgetvalue(res, 0, i_amname));

	appendPQExpBuffer(delq, "DROP OPERATOR CLASS %s",
					  fmtQualifiedDumpable(opcinfo));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR CLASS %s\n    ",
					  fmtQualifiedDumpable(opcinfo));
	if (strcmp(opcdefault, "t") == 0)
		appendPQExpBufferStr(q, "DEFAULT ");
	appendPQExpBuffer(q, "FOR TYPE %s USING %s",
					  opcintype,
					  fmtId(amname));
	if (strlen(opcfamilyname) > 0)
	{
		appendPQExpBufferStr(q, " FAMILY ");
		appendPQExpBuffer(q, "%s.", fmtId(opcfamilynsp));
		appendPQExpBuffer(q, "%s", fmtId(opcfamilyname));
	}
	appendPQExpBufferStr(q, " AS\n    ");

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
	 *
	 * Print only those opfamily members that are tied to the opclass by
	 * pg_depend entries.
	 *
	 * XXX RECHECK is gone as of 8.4, but we'll still print it if dumping an
	 * older server's opclass in which it is used.  This is to avoid
	 * hard-to-detect breakage if a newer pg_dump is used to dump from an
	 * older server and then reload into that old version.  This can go away
	 * once 8.3 is so old as to not be of interest to anyone.
	 */
	resetPQExpBuffer(query);

	if (fout->remoteVersion >= 90100)
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "opfname AS sortfamily, "
						  "nspname AS sortfamilynsp "
				   "FROM pg_catalog.pg_amop ao JOIN pg_catalog.pg_depend ON "
						  "(classid = 'pg_catalog.pg_amop'::pg_catalog.regclass AND objid = ao.oid) "
			  "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = amopsortfamily "
			   "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
						  "AND amopfamily = '%s'::pg_catalog.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid,
						  opcfamily);
	}
	else if (fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "NULL AS sortfamily, "
						  "NULL AS sortfamilynsp "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "NULL AS sortfamily, "
						  "NULL AS sortfamilynsp "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		   "WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opcinfo->dobj.catId.oid);
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amopstrategy = PQfnumber(res, "amopstrategy");
	i_amopreqcheck = PQfnumber(res, "amopreqcheck");
	i_amopopr = PQfnumber(res, "amopopr");
	i_sortfamily = PQfnumber(res, "sortfamily");
	i_sortfamilynsp = PQfnumber(res, "sortfamilynsp");

	for (i = 0; i < ntups; i++)
	{
		amopstrategy = PQgetvalue(res, i, i_amopstrategy);
		amopreqcheck = PQgetvalue(res, i, i_amopreqcheck);
		amopopr = PQgetvalue(res, i, i_amopopr);
		sortfamily = PQgetvalue(res, i, i_sortfamily);
		sortfamilynsp = PQgetvalue(res, i, i_sortfamilynsp);

		if (needComma)
			appendPQExpBufferStr(q, " ,\n    ");

		appendPQExpBuffer(q, "OPERATOR %s %s",
						  amopstrategy, amopopr);

		if (strlen(sortfamily) > 0)
		{
			appendPQExpBufferStr(q, " FOR ORDER BY ");
			appendPQExpBuffer(q, "%s.", fmtId(sortfamilynsp));
			appendPQExpBufferStr(q, fmtId(sortfamily));
		}

		if (strcmp(amopreqcheck, "t") == 0)
			appendPQExpBufferStr(q, " RECHECK");

		needComma = true;
	}

	PQclear(res);

	/*
	 * Now fetch and print the FUNCTION entries (pg_amproc rows).
	 *
	 * Print only those opfamily members that are tied to the opclass by
	 * pg_depend entries.
	 *
	 * We print the amproclefttype/amprocrighttype even though in most cases
	 * the backend could deduce the right values, because of the corner case
	 * of a btree sort support function for a cross-type comparison.  That's
	 * only allowed in 9.2 and later, but for simplicity print them in all
	 * versions that have the columns.
	 */
	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT amprocnum, "
						"amproc::pg_catalog.regprocedure, "
						"amproclefttype::pg_catalog.regtype, "
						"amprocrighttype::pg_catalog.regtype "
					"FROM pg_catalog.pg_amproc ap, pg_catalog.pg_depend "
			"WHERE refclassid = 'pg_catalog.pg_opclass'::pg_catalog.regclass "
						"AND refobjid = '%u'::pg_catalog.oid "
				"AND classid = 'pg_catalog.pg_amproc'::pg_catalog.regclass "
						"AND objid = ap.oid "
						"ORDER BY amprocnum",
						opcinfo->dobj.catId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_amprocnum = PQfnumber(res, "amprocnum");
	i_amproc = PQfnumber(res, "amproc");
	i_amproclefttype = PQfnumber(res, "amproclefttype");
	i_amprocrighttype = PQfnumber(res, "amprocrighttype");

	for (i = 0; i < ntups; i++)
	{
		amprocnum = PQgetvalue(res, i, i_amprocnum);
		amproc = PQgetvalue(res, i, i_amproc);
		amproclefttype = PQgetvalue(res, i, i_amproclefttype);
		amprocrighttype = PQgetvalue(res, i, i_amprocrighttype);

		if (needComma)
			appendPQExpBufferStr(q, " ,\n    ");

		appendPQExpBuffer(q, "FUNCTION %s", amprocnum);

		if (*amproclefttype && *amprocrighttype)
			appendPQExpBuffer(q, " (%s, %s)", amproclefttype, amprocrighttype);

		appendPQExpBuffer(q, " %s", amproc);

		needComma = true;
	}

	PQclear(res);

	/*
	 * If needComma is still false it means we haven't added anything after
	 * the AS keyword.  To avoid printing broken SQL, append a dummy STORAGE
	 * clause with the same datatype.  This isn't sanctioned by the
	 * documentation, but actually DefineOpClass will treat it as a no-op.
	 */
	if (!needComma)
		appendPQExpBuffer(q, "STORAGE %s", opcintype);

	appendPQExpBufferStr(q, ";\n");

	appendPQExpBufferStr(nameusing, fmtId(opcinfo->dobj.name));
	appendPQExpBuffer(nameusing, " USING %s",
					  fmtId(amname));

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &opcinfo->dobj,
										"OPERATOR CLASS", nameusing->data,
										opcinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, opcinfo->dobj.catId, opcinfo->dobj.dumpId,
				 opcinfo->dobj.name,
				 opcinfo->dobj.namespace->dobj.name,
				 NULL,
				 opcinfo->rolname,
				 false, "OPERATOR CLASS", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Operator Class Comments */
	dumpComment(fout, "OPERATOR CLASS", nameusing->data,
				opcinfo->dobj.namespace->dobj.name, opcinfo->rolname,
				opcinfo->dobj.catId, 0, opcinfo->dobj.dumpId);

	free(opcintype);
	free(opcfamily);
	free(amname);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(nameusing);
}

/*
 * dumpOpfamily
 *	  write out a single operator family definition
 *
 * Note: this also dumps any "loose" operator members that aren't bound to a
 * specific opclass within the opfamily.
 */
static void
dumpOpfamily(Archive *fout, OpfamilyInfo *opfinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer nameusing;
	PGresult   *res;
	PGresult   *res_ops;
	PGresult   *res_procs;
	int			ntups;
	int			i_amname;
	int			i_amopstrategy;
	int			i_amopreqcheck;
	int			i_amopopr;
	int			i_sortfamily;
	int			i_sortfamilynsp;
	int			i_amprocnum;
	int			i_amproc;
	int			i_amproclefttype;
	int			i_amprocrighttype;
	char	   *amname;
	char	   *amopstrategy;
	char	   *amopreqcheck;
	char	   *amopopr;
	char	   *sortfamily;
	char	   *sortfamilynsp;
	char	   *amprocnum;
	char	   *amproc;
	char	   *amproclefttype;
	char	   *amprocrighttype;
	bool		needComma;
	int			i;

	/* Skip if not to be dumped */
	if (!opfinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	nameusing = createPQExpBuffer();

	/*
	 * Fetch only those opfamily members that are tied directly to the
	 * opfamily by pg_depend entries.
	 *
	 * XXX RECHECK is gone as of 8.4, but we'll still print it if dumping an
	 * older server's opclass in which it is used.  This is to avoid
	 * hard-to-detect breakage if a newer pg_dump is used to dump from an
	 * older server and then reload into that old version.  This can go away
	 * once 8.3 is so old as to not be of interest to anyone.
	 */
	if (fout->remoteVersion >= 90100)
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "opfname AS sortfamily, "
						  "nspname AS sortfamilynsp "
				   "FROM pg_catalog.pg_amop ao JOIN pg_catalog.pg_depend ON "
						  "(classid = 'pg_catalog.pg_amop'::pg_catalog.regclass AND objid = ao.oid) "
			  "LEFT JOIN pg_catalog.pg_opfamily f ON f.oid = amopsortfamily "
			   "LEFT JOIN pg_catalog.pg_namespace n ON n.oid = opfnamespace "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
						  "AND amopfamily = '%u'::pg_catalog.oid "
						  "ORDER BY amopstrategy",
						  opfinfo->dobj.catId.oid,
						  opfinfo->dobj.catId.oid);
	}
	else if (fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, false AS amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "NULL AS sortfamily, "
						  "NULL AS sortfamilynsp "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opfinfo->dobj.catId.oid);
	}
	else
	{
		appendPQExpBuffer(query, "SELECT amopstrategy, amopreqcheck, "
						  "amopopr::pg_catalog.regoperator, "
						  "NULL AS sortfamily, "
						  "NULL AS sortfamilynsp "
						  "FROM pg_catalog.pg_amop ao, pg_catalog.pg_depend "
		  "WHERE refclassid = 'pg_catalog.pg_opfamily'::pg_catalog.regclass "
						  "AND refobjid = '%u'::pg_catalog.oid "
				   "AND classid = 'pg_catalog.pg_amop'::pg_catalog.regclass "
						  "AND objid = ao.oid "
						  "ORDER BY amopstrategy",
						  opfinfo->dobj.catId.oid);
	}

	res_ops = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

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

	res_procs = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	/* Get additional fields from the pg_opfamily row */
	resetPQExpBuffer(query);

	appendPQExpBuffer(query, "SELECT "
	 "(SELECT amname FROM pg_catalog.pg_am WHERE oid = opfmethod) AS amname "
					  "FROM pg_catalog.pg_opfamily "
					  "WHERE oid = '%u'::pg_catalog.oid",
					  opfinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	i_amname = PQfnumber(res, "amname");

	/* amname will still be needed after we PQclear res */
	amname = pg_strdup(PQgetvalue(res, 0, i_amname));

	appendPQExpBuffer(delq, "DROP OPERATOR FAMILY %s",
					  fmtQualifiedDumpable(opfinfo));
	appendPQExpBuffer(delq, " USING %s;\n",
					  fmtId(amname));

	/* Build the fixed portion of the CREATE command */
	appendPQExpBuffer(q, "CREATE OPERATOR FAMILY %s",
					  fmtQualifiedDumpable(opfinfo));
	appendPQExpBuffer(q, " USING %s;\n",
					  fmtId(amname));

	PQclear(res);

	/* Do we need an ALTER to add loose members? */
	if (PQntuples(res_ops) > 0 || PQntuples(res_procs) > 0)
	{
		appendPQExpBuffer(q, "ALTER OPERATOR FAMILY %s",
						  fmtQualifiedDumpable(opfinfo));
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
		i_sortfamily = PQfnumber(res_ops, "sortfamily");
		i_sortfamilynsp = PQfnumber(res_ops, "sortfamilynsp");

		for (i = 0; i < ntups; i++)
		{
			amopstrategy = PQgetvalue(res_ops, i, i_amopstrategy);
			amopreqcheck = PQgetvalue(res_ops, i, i_amopreqcheck);
			amopopr = PQgetvalue(res_ops, i, i_amopopr);
			sortfamily = PQgetvalue(res_ops, i, i_sortfamily);
			sortfamilynsp = PQgetvalue(res_ops, i, i_sortfamilynsp);

			if (needComma)
				appendPQExpBufferStr(q, " ,\n    ");

			appendPQExpBuffer(q, "OPERATOR %s %s",
							  amopstrategy, amopopr);

			if (strlen(sortfamily) > 0)
			{
				appendPQExpBufferStr(q, " FOR ORDER BY ");
				appendPQExpBuffer(q, "%s.", fmtId(sortfamilynsp));
				appendPQExpBufferStr(q, fmtId(sortfamily));
			}

			if (strcmp(amopreqcheck, "t") == 0)
				appendPQExpBufferStr(q, " RECHECK");

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
				appendPQExpBufferStr(q, " ,\n    ");

			appendPQExpBuffer(q, "FUNCTION %s (%s, %s) %s",
							  amprocnum, amproclefttype, amprocrighttype,
							  amproc);

			needComma = true;
		}

		appendPQExpBufferStr(q, ";\n");
	}

	appendPQExpBufferStr(nameusing, fmtId(opfinfo->dobj.name));
	appendPQExpBuffer(nameusing, " USING %s",
					  fmtId(amname));

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &opfinfo->dobj,
										"OPERATOR FAMILY", nameusing->data,
										opfinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, opfinfo->dobj.catId, opfinfo->dobj.dumpId,
				 opfinfo->dobj.name,
				 opfinfo->dobj.namespace->dobj.name,
				 NULL,
				 opfinfo->rolname,
				 false, "OPERATOR FAMILY", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Operator Family Comments */
	dumpComment(fout, "OPERATOR FAMILY", nameusing->data,
				opfinfo->dobj.namespace->dobj.name, opfinfo->rolname,
				opfinfo->dobj.catId, 0, opfinfo->dobj.dumpId);

	free(amname);
	PQclear(res_ops);
	PQclear(res_procs);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(nameusing);
}

/*
 * dumpCollation
 *	  write out a single collation definition
 */
static void
dumpCollation(Archive *fout, CollInfo *collinfo)
{
	PQExpBuffer query;
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *qcollname;
	PGresult   *res;
	int			i_collcollate;
	int			i_collctype;
	const char *collcollate;
	const char *collctype;

	/* Skip if not to be dumped */
	if (!collinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qcollname = pg_strdup(fmtId(collinfo->dobj.name));

	/* Get conversion-specific details */
	appendPQExpBuffer(query, "SELECT "
					  "collcollate, "
					  "collctype "
					  "FROM pg_catalog.pg_collation c "
					  "WHERE c.oid = '%u'::pg_catalog.oid",
					  collinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	i_collcollate = PQfnumber(res, "collcollate");
	i_collctype = PQfnumber(res, "collctype");

	collcollate = PQgetvalue(res, 0, i_collcollate);
	collctype = PQgetvalue(res, 0, i_collctype);

	appendPQExpBuffer(delq, "DROP COLLATION %s;\n",
					  fmtQualifiedDumpable(collinfo));

	appendPQExpBuffer(q, "CREATE COLLATION %s (lc_collate = ",
					  fmtQualifiedDumpable(collinfo));
	appendStringLiteralAH(q, collcollate, fout);
	appendPQExpBufferStr(q, ", lc_ctype = ");
	appendStringLiteralAH(q, collctype, fout);
	appendPQExpBufferStr(q, ");\n");

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &collinfo->dobj,
										"COLLATION", qcollname,
										collinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, collinfo->dobj.catId, collinfo->dobj.dumpId,
				 collinfo->dobj.name,
				 collinfo->dobj.namespace->dobj.name,
				 NULL,
				 collinfo->rolname,
				 false, "COLLATION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Collation Comments */
	dumpComment(fout, "COLLATION", qcollname,
				collinfo->dobj.namespace->dobj.name, collinfo->rolname,
				collinfo->dobj.catId, 0, collinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qcollname);
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
	char	   *qconvname;
	PGresult   *res;
	int			i_conforencoding;
	int			i_contoencoding;
	int			i_conproc;
	int			i_condefault;
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

	qconvname = pg_strdup(fmtId(convinfo->dobj.name));

	/* Get conversion-specific details */
	appendPQExpBuffer(query, "SELECT "
		 "pg_catalog.pg_encoding_to_char(conforencoding) AS conforencoding, "
		   "pg_catalog.pg_encoding_to_char(contoencoding) AS contoencoding, "
					  "conproc, condefault "
					  "FROM pg_catalog.pg_conversion c "
					  "WHERE c.oid = '%u'::pg_catalog.oid",
					  convinfo->dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	i_conforencoding = PQfnumber(res, "conforencoding");
	i_contoencoding = PQfnumber(res, "contoencoding");
	i_conproc = PQfnumber(res, "conproc");
	i_condefault = PQfnumber(res, "condefault");

	conforencoding = PQgetvalue(res, 0, i_conforencoding);
	contoencoding = PQgetvalue(res, 0, i_contoencoding);
	conproc = PQgetvalue(res, 0, i_conproc);
	condefault = (PQgetvalue(res, 0, i_condefault)[0] == 't');

	appendPQExpBuffer(delq, "DROP CONVERSION %s;\n",
					  fmtQualifiedDumpable(convinfo));

	appendPQExpBuffer(q, "CREATE %sCONVERSION %s FOR ",
					  (condefault) ? "DEFAULT " : "",
					  fmtQualifiedDumpable(convinfo));
	appendStringLiteralAH(q, conforencoding, fout);
	appendPQExpBufferStr(q, " TO ");
	appendStringLiteralAH(q, contoencoding, fout);
	/* regproc is automatically quoted in 7.3 and above */
	appendPQExpBuffer(q, " FROM %s;\n", conproc);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &convinfo->dobj,
										"CONVERSION", qconvname,
										convinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, convinfo->dobj.catId, convinfo->dobj.dumpId,
				 convinfo->dobj.name,
				 convinfo->dobj.namespace->dobj.name,
				 NULL,
				 convinfo->rolname,
				 false, "CONVERSION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Conversion Comments */
	dumpComment(fout, "CONVERSION", qconvname,
				convinfo->dobj.namespace->dobj.name, convinfo->rolname,
				convinfo->dobj.catId, 0, convinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qconvname);
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
		appendPQExpBufferStr(&buf, fmtId(agginfo->aggfn.dobj.name));
	else
		appendPQExpBufferStr(&buf, agginfo->aggfn.dobj.name);

	if (agginfo->aggfn.nargs == 0)
		appendPQExpBuffer(&buf, "(*)");
	else
	{
		appendPQExpBufferChar(&buf, '(');
		for (j = 0; j < agginfo->aggfn.nargs; j++)
		{
			char	   *typname;

			typname = getFormattedTypeName(fout, agginfo->aggfn.argtypes[j],
										   zeroAsOpaque);

			appendPQExpBuffer(&buf, "%s%s",
							  (j > 0) ? ", " : "",
							  typname);
			free(typname);
		}
		appendPQExpBufferChar(&buf, ')');
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
	char	   *aggsig;			/* identity signature */
	char	   *aggfullsig = NULL;		/* full signature */
	char	   *aggsig_tag;
	PGresult   *res;
	int			i_agginitval;
	int			i_aggminitval;
	const char *aggtransfn;
	const char *aggfinalfn;
	const char *aggcombinefn;
	const char *aggserialfn;
	const char *aggdeserialfn;
	const char *aggmtransfn;
	const char *aggminvtransfn;
	const char *aggmfinalfn;
	bool		aggfinalextra;
	bool		aggmfinalextra;
	const char *aggsortop;
	char	   *aggsortconvop;
	bool		hypothetical;
	const char *aggtranstype;
	const char *aggtransspace;
	const char *aggmtranstype;
	const char *aggmtransspace;
	const char *agginitval;
	const char *aggminitval;

	/* Skip if not to be dumped */
	if (!agginfo->aggfn.dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	details = createPQExpBuffer();

	if (!fout->is_prepared[PREPQUERY_DUMPAGG])
	{
		/* Set up query for aggregate-specific details */
		appendPQExpBufferStr(query,
							 "PREPARE dumpAgg(pg_catalog.oid) AS\n");

		appendPQExpBuffer(query,
							"SELECT\n"
							"aggtransfn,\n"
							"aggfinalfn,\n"
							"aggtranstype::pg_catalog.regtype,\n"
							"agginitval,\n"
							"aggsortop,\n"
							"pg_catalog.pg_get_function_arguments(p.oid) AS funcargs,\n"
							"pg_catalog.pg_get_function_identity_arguments(p.oid) AS funciargs,\n");

		if (fout->remoteVersion >= 90400)
			appendPQExpBuffer(query,
								"aggcombinefn,\n"
								"aggserialfn,\n"
								"aggdeserialfn,\n"
								"aggmtransfn,\n"
								"aggminvtransfn,\n"
								"aggmfinalfn,\n"
								"aggmtranstype::pg_catalog.regtype,\n"
								"aggfinalextra,\n"
								"aggmfinalextra,\n"
								"(aggkind = 'h') AS hypothetical,\n"
								"aggtransspace,\n"
								"aggmtransspace,\n"
								"aggminitval\n");
		else
			appendPQExpBuffer(query,
								"aggprelimfn AS aggcombinefn,\n"
								"'-' AS aggserialfn,\n"
								"'-' AS aggdeserialfn,\n"
								"'-' AS aggmtransfn,\n"
								"'-' AS aggminvtransfn,\n"
								"'-' AS aggmfinalfn,\n"
								"0 AS aggmtranstype,\n"
								"false AS aggfinalextra,\n"
								"false AS aggmfinalextra,\n"
								"false AS hypothetical,\n"
								"0 AS aggtransspace,\n"
								"0 AS aggmtransspace,\n"
								"NULL AS aggminitval\n");

		appendPQExpBufferStr(query,
							"FROM pg_catalog.pg_aggregate a, pg_catalog.pg_proc p "
							"WHERE a.aggfnoid = p.oid "
							"AND p.oid = $1");

		ExecuteSqlStatement(fout, query->data);

		fout->is_prepared[PREPQUERY_DUMPAGG] = true;
	}

	printfPQExpBuffer(query,
					  "EXECUTE dumpAgg('%u')",
					  agginfo->aggfn.dobj.catId.oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);

	i_agginitval = PQfnumber(res, "agginitval");
	i_aggminitval = PQfnumber(res, "aggminitval");

	aggtransfn = PQgetvalue(res, 0, PQfnumber(res, "aggtransfn"));
	aggfinalfn = PQgetvalue(res, 0, PQfnumber(res, "aggfinalfn"));
	aggcombinefn = PQgetvalue(res, 0, PQfnumber(res,"aggcombinefn"));
	aggserialfn = PQgetvalue(res, 0, PQfnumber(res,"aggserialfn"));
	aggdeserialfn = PQgetvalue(res, 0, PQfnumber(res,"aggdeserialfn"));
	aggmtransfn = PQgetvalue(res, 0, PQfnumber(res, "aggmtransfn"));
	aggminvtransfn = PQgetvalue(res, 0, PQfnumber(res, "aggminvtransfn"));
	aggmfinalfn = PQgetvalue(res, 0, PQfnumber(res, "aggmfinalfn"));
	aggfinalextra = (PQgetvalue(res, 0, PQfnumber(res, "aggfinalextra"))[0] == 't');
	aggmfinalextra = (PQgetvalue(res, 0, PQfnumber(res, "aggmfinalextra"))[0] == 't');
	aggsortop = PQgetvalue(res, 0, PQfnumber(res, "aggsortop"));
	hypothetical = (PQgetvalue(res, 0, PQfnumber(res, "hypothetical"))[0] == 't');
	aggtranstype = PQgetvalue(res, 0, PQfnumber(res, "aggtranstype"));
	aggtransspace = PQgetvalue(res, 0, PQfnumber(res, "aggtransspace"));
	aggmtranstype = PQgetvalue(res, 0, PQfnumber(res, "aggmtranstype"));
	aggmtransspace = PQgetvalue(res, 0, PQfnumber(res, "aggmtransspace"));
	agginitval = PQgetvalue(res, 0, i_agginitval);
	aggminitval = PQgetvalue(res, 0, i_aggminitval);

	if (fout->remoteVersion >= 80400)
	{
		/* 8.4 or later; we rely on server-side code for most of the work */
		char	   *funcargs;
		char	   *funciargs;

		funcargs = PQgetvalue(res, 0, PQfnumber(res, "funcargs"));
		funciargs = PQgetvalue(res, 0, PQfnumber(res, "funciargs"));
		aggfullsig = format_function_arguments(&agginfo->aggfn, funcargs, true);
		aggsig = format_function_arguments(&agginfo->aggfn, funciargs, true);
	}
	else
		/* pre-8.4, do it ourselves */
		aggsig = format_aggregate_signature(agginfo, fout, true);

	aggsig_tag = format_aggregate_signature(agginfo, fout, false);

	appendPQExpBuffer(details, "    SFUNC = %s,\n    STYPE = %s",
						aggtransfn,
						aggtranstype);

	if (strcmp(aggtransspace, "0") != 0)
	{
		appendPQExpBuffer(details, ",\n    SSPACE = %s",
						  aggtransspace);
	}

	if (strcmp(aggtransspace, "0") != 0)
	{
		appendPQExpBuffer(details, ",\n    SSPACE = %s",
						  aggtransspace);
	}

	if (!PQgetisnull(res, 0, i_agginitval))
	{
		appendPQExpBufferStr(details, ",\n    INITCOND = ");
		appendStringLiteralAH(details, agginitval, fout);
	}

	if (strcmp(aggfinalfn, "-") != 0)
	{
		appendPQExpBuffer(details, ",\n    FINALFUNC = %s",
						  aggfinalfn);
		if (aggfinalextra)
			appendPQExpBufferStr(details, ",\n    FINALFUNC_EXTRA");
	}

	if (strcmp(aggcombinefn, "-") != 0)
		appendPQExpBuffer(details, ",\n    COMBINEFUNC = %s",	aggcombinefn);

	if (strcmp(aggserialfn, "-") != 0)
		appendPQExpBuffer(details, ",\n    SERIALFUNC = %s", aggserialfn);

	if (strcmp(aggdeserialfn, "-") != 0)
		appendPQExpBuffer(details, ",\n    DESERIALFUNC = %s", aggdeserialfn);

	if (strcmp(aggmtransfn, "-") != 0)
	{
		appendPQExpBuffer(details, ",\n    MSFUNC = %s,\n    MINVFUNC = %s,\n    MSTYPE = %s",
						  aggmtransfn,
						  aggminvtransfn,
						  aggmtranstype);
	}

	if (strcmp(aggmtransspace, "0") != 0)
	{
		appendPQExpBuffer(details, ",\n    MSSPACE = %s",
						  aggmtransspace);
	}

	if (!PQgetisnull(res, 0, i_aggminitval))
	{
		appendPQExpBufferStr(details, ",\n    MINITCOND = ");
		appendStringLiteralAH(details, aggminitval, fout);
	}

	if (strcmp(aggmfinalfn, "-") != 0)
	{
		appendPQExpBuffer(details, ",\n    MFINALFUNC = %s",
						  aggmfinalfn);
		if (aggmfinalextra)
			appendPQExpBufferStr(details, ",\n    MFINALFUNC_EXTRA");
	}

	aggsortconvop = getFormattedOperatorName(fout, aggsortop);
	if (aggsortconvop)
	{
		appendPQExpBuffer(details, ",\n    SORTOP = %s",
						  aggsortconvop);
		free(aggsortconvop);
	}

	if (hypothetical)
		appendPQExpBufferStr(details, ",\n    HYPOTHETICAL");

	appendPQExpBuffer(delq, "DROP AGGREGATE %s.%s;\n",
					  fmtId(agginfo->aggfn.dobj.namespace->dobj.name),
					  aggsig);

	appendPQExpBuffer(q, "CREATE AGGREGATE %s.%s (\n%s\n);\n",
					  fmtId(agginfo->aggfn.dobj.namespace->dobj.name),
					  aggfullsig ? aggfullsig : aggsig, details->data);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &agginfo->aggfn.dobj,
										"AGGREGATE", aggsig,
								   agginfo->aggfn.dobj.namespace->dobj.name);

	ArchiveEntry(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
				 aggsig_tag,
				 agginfo->aggfn.dobj.namespace->dobj.name,
				 NULL,
				 agginfo->aggfn.rolname,
				 false, "AGGREGATE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Aggregate Comments */
	dumpComment(fout, "AGGREGATE", aggsig,
			agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname,
				agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);
	dumpSecLabel(fout, "AGGREGATE", aggsig,
			agginfo->aggfn.dobj.namespace->dobj.name, agginfo->aggfn.rolname,
				 agginfo->aggfn.dobj.catId, 0, agginfo->aggfn.dobj.dumpId);

	/*
	 * Since there is no GRANT ON AGGREGATE syntax, we have to make the ACL
	 * command look like a function's GRANT; in particular this affects the
	 * syntax for zero-argument aggregates and ordered-set aggregates.
	 */
	free(aggsig);

	aggsig = format_function_signature(fout, &agginfo->aggfn, true);

	dumpACL(fout, agginfo->aggfn.dobj.catId, agginfo->aggfn.dobj.dumpId,
			"FUNCTION", aggsig, NULL,
			agginfo->aggfn.dobj.namespace->dobj.name,
			agginfo->aggfn.rolname, agginfo->aggfn.proacl);

	free(aggsig);
	if (aggfullsig)
		free(aggfullsig);
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
getFunctionName(Archive *fout, Oid oid)
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

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	/* Expecting a single result only */
	ntups = PQntuples(res);
	if (ntups != 1)
	{
		write_msg(NULL, "query yielded %d rows instead of one: %s\n",
				  ntups, query->data);
		exit_nicely(1);
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
	PQExpBuffer	nsq;
	char	   *prev_ns;
	char	   *namecopy;
	int			i;
	bool		has_internal = false;

	/* Skip if not to be dumped */
	if (!ptcinfo->dobj.dump || dataOnly)
		return;

	/* init and fill the protoFuncs array */
	memset(protoFuncs, 0, sizeof(protoFuncs));
	protoFuncs[READFN_IDX].oid = ptcinfo->ptcreadid;
	protoFuncs[WRITEFN_IDX].oid = ptcinfo->ptcwriteid;
	protoFuncs[VALIDFN_IDX].oid = ptcinfo->ptcvalidid;

	for (i = 0; i < FCOUNT; i++)
	{
		if (protoFuncs[i].oid == InvalidOid)
		{
			protoFuncs[i].dumpable = false;
			protoFuncs[i].internal = true;
			/*
			 * We have at least one internal function, signal that we need the
			 * public schema in the search_path
			 */
			has_internal = true;
		}
		else
		{
			protoFuncs[i].pfuncinfo = findFuncByOid(protoFuncs[i].oid);
			if (protoFuncs[i].pfuncinfo != NULL)
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
	if (protoFuncs[READFN_IDX].internal && protoFuncs[WRITEFN_IDX].internal
			&& protoFuncs[VALIDFN_IDX].internal)
		return;

	/* obtain the function name for internal functions (if any) */
	for (i = 0; i < FCOUNT; i++)
	{
		if (protoFuncs[i].internal && protoFuncs[i].oid)
		{
			protoFuncs[i].name = getFunctionName(fout, protoFuncs[i].oid);
			if (protoFuncs[i].name)
				protoFuncs[i].dumpable = true;
		}
	}

	nsq = createPQExpBuffer();
	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	/*
	 * Since the function parameters to the external protocol cannot be fully
	 * qualified with namespace, we must ensure that we have the search_path
	 * set with the namespaces of the referenced functions. We only need the
	 * dump file to have the search_path so inject a SET search_path = .. ;
	 * into the output stream instead of calling selectSourceSchema().
	 */
	prev_ns = NULL;
	for (i = 0; i < FCOUNT; i++)
	{
		if (!protoFuncs[i].pfuncinfo || protoFuncs[i].internal)
			continue;

		if (prev_ns && strcmp(prev_ns, protoFuncs[i].pfuncinfo->dobj.namespace->dobj.name) == 0)
			continue;

		appendPQExpBuffer(nsq, "%s%s", (prev_ns ? "," : ""), protoFuncs[i].pfuncinfo->dobj.namespace->dobj.name);
		prev_ns = protoFuncs[i].pfuncinfo->dobj.namespace->dobj.name;

		/*
		 * If we are adding public to the search_path, then we don't need to do
		 * so again for any internal functions
		 */
		if (strcmp(prev_ns, "public") == 0)
			has_internal = false;
	}

	if (prev_ns)
	{
		appendPQExpBufferStr(q, "-- Set the search_path required to look up the functions\n");
		appendPQExpBuffer(q, "SET search_path = %s%s;\n\n",
						  nsq->data, (has_internal ? ", public" : ""));
	}
	destroyPQExpBuffer(nsq);

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
	appendPQExpBufferStr(q, ");\n");

	appendPQExpBuffer(delq, "DROP PROTOCOL %s;\n",
					  fmtId(ptcinfo->dobj.name));

	ArchiveEntry(fout, ptcinfo->dobj.catId, ptcinfo->dobj.dumpId,
				 ptcinfo->dobj.name,
				 NULL,
				 NULL,
				 ptcinfo->rolname,
				 false, "PROTOCOL", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 ptcinfo->dobj.dependencies, ptcinfo->dobj.nDeps,
				 NULL, NULL);

	/* Handle the ACL */
	namecopy = pg_strdup(fmtId(ptcinfo->dobj.name));
	dumpACL(fout, ptcinfo->dobj.catId, ptcinfo->dobj.dumpId,
			"PROTOCOL",
			namecopy, NULL,
			NULL, ptcinfo->rolname,
			ptcinfo->ptcacl);
	free(namecopy);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);

	for (i = 0; i < FCOUNT; i++)
	{
		if (protoFuncs[i].name)
			free(protoFuncs[i].name);
	}
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
	char	   *qprsname;

	/* Skip if not to be dumped */
	if (!prsinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qprsname = pg_strdup(fmtId(prsinfo->dobj.name));

	appendPQExpBuffer(q, "CREATE TEXT SEARCH PARSER %s (\n",
					  fmtQualifiedDumpable(prsinfo));

	appendPQExpBuffer(q, "    START = %s,\n",
					  convertTSFunction(fout, prsinfo->prsstart));
	appendPQExpBuffer(q, "    GETTOKEN = %s,\n",
					  convertTSFunction(fout, prsinfo->prstoken));
	appendPQExpBuffer(q, "    END = %s,\n",
					  convertTSFunction(fout, prsinfo->prsend));
	if (prsinfo->prsheadline != InvalidOid)
		appendPQExpBuffer(q, "    HEADLINE = %s,\n",
						  convertTSFunction(fout, prsinfo->prsheadline));
	appendPQExpBuffer(q, "    LEXTYPES = %s );\n",
					  convertTSFunction(fout, prsinfo->prslextype));

	appendPQExpBuffer(delq, "DROP TEXT SEARCH PARSER %s;\n",
					  fmtQualifiedDumpable(prsinfo));

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &prsinfo->dobj,
										"TEXT SEARCH PARSER", qprsname,
										prsinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, prsinfo->dobj.catId, prsinfo->dobj.dumpId,
				 prsinfo->dobj.name,
				 prsinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH PARSER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Parser Comments */
	dumpComment(fout, "TEXT SEARCH PARSER", qprsname,
				prsinfo->dobj.namespace->dobj.name, "",
				prsinfo->dobj.catId, 0, prsinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qprsname);
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
	char	   *qdictname;
	PGresult   *res;
	char	   *nspname;
	char	   *tmplname;

	/* Skip if not to be dumped */
	if (!dictinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	qdictname = pg_strdup(fmtId(dictinfo->dobj.name));

	/* Fetch name and namespace of the dictionary's template */
	appendPQExpBuffer(query, "SELECT nspname, tmplname "
					  "FROM pg_ts_template p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = tmplnamespace",
					  dictinfo->dicttemplate);
	res = ExecuteSqlQueryForSingleRow(fout, query->data);
	nspname = PQgetvalue(res, 0, 0);
	tmplname = PQgetvalue(res, 0, 1);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH DICTIONARY %s (\n",
					  fmtQualifiedDumpable(dictinfo));

	appendPQExpBufferStr(q, "    TEMPLATE = ");
	appendPQExpBuffer(q, "%s.", fmtId(nspname));
	appendPQExpBufferStr(q, fmtId(tmplname));

	PQclear(res);

	/* the dictinitoption can be dumped straight into the command */
	if (dictinfo->dictinitoption)
		appendPQExpBuffer(q, ",\n    %s", dictinfo->dictinitoption);

	appendPQExpBufferStr(q, " );\n");

	appendPQExpBuffer(delq, "DROP TEXT SEARCH DICTIONARY %s;\n",
					  fmtQualifiedDumpable(dictinfo));
	ArchiveEntry(fout, dictinfo->dobj.catId, dictinfo->dobj.dumpId,
				 dictinfo->dobj.name,
				 dictinfo->dobj.namespace->dobj.name,
				 NULL,
				 dictinfo->rolname,
				 false, "TEXT SEARCH DICTIONARY", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Dictionary Comments */
	dumpComment(fout, "TEXT SEARCH DICTIONARY", qdictname,
				dictinfo->dobj.namespace->dobj.name, dictinfo->rolname,
				dictinfo->dobj.catId, 0, dictinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qdictname);
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
	char	   *qtmplname;

	/* Skip if not to be dumped */
	if (!tmplinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qtmplname = pg_strdup(fmtId(tmplinfo->dobj.name));

	appendPQExpBuffer(q, "CREATE TEXT SEARCH TEMPLATE %s (\n",
					  fmtQualifiedDumpable(tmplinfo));

	if (tmplinfo->tmplinit != InvalidOid)
		appendPQExpBuffer(q, "    INIT = %s,\n",
						  convertTSFunction(fout, tmplinfo->tmplinit));
	appendPQExpBuffer(q, "    LEXIZE = %s );\n",
					  convertTSFunction(fout, tmplinfo->tmpllexize));

	appendPQExpBuffer(delq, "DROP TEXT SEARCH TEMPLATE %s;\n",
					  fmtQualifiedDumpable(tmplinfo));

	ArchiveEntry(fout, tmplinfo->dobj.catId, tmplinfo->dobj.dumpId,
				 tmplinfo->dobj.name,
				 tmplinfo->dobj.namespace->dobj.name,
				 NULL,
				 "",
				 false, "TEXT SEARCH TEMPLATE", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Template Comments */
	dumpComment(fout, "TEXT SEARCH TEMPLATE", qtmplname,
				tmplinfo->dobj.namespace->dobj.name, "",
				tmplinfo->dobj.catId, 0, tmplinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qtmplname);
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
	char	   *qcfgname;
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

	qcfgname = pg_strdup(fmtId(cfginfo->dobj.name));

	/* Fetch name and namespace of the config's parser */
	appendPQExpBuffer(query, "SELECT nspname, prsname "
					  "FROM pg_ts_parser p, pg_namespace n "
					  "WHERE p.oid = '%u' AND n.oid = prsnamespace",
					  cfginfo->cfgparser);
	res = ExecuteSqlQueryForSingleRow(fout, query->data);
	nspname = PQgetvalue(res, 0, 0);
	prsname = PQgetvalue(res, 0, 1);

	appendPQExpBuffer(q, "CREATE TEXT SEARCH CONFIGURATION %s (\n",
					  fmtQualifiedDumpable(cfginfo));

	appendPQExpBuffer(q, "    PARSER = %s.", fmtId(nspname));
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

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
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
				appendPQExpBufferStr(q, ";\n");
			appendPQExpBuffer(q, "\nALTER TEXT SEARCH CONFIGURATION %s\n",
							  fmtQualifiedDumpable(cfginfo));
			/* tokenname needs quoting, dictname does NOT */
			appendPQExpBuffer(q, "    ADD MAPPING FOR %s WITH %s",
							  fmtId(tokenname), dictname);
		}
		else
			appendPQExpBuffer(q, ", %s", dictname);
	}

	if (ntups > 0)
		appendPQExpBufferStr(q, ";\n");

	PQclear(res);

	appendPQExpBuffer(delq, "DROP TEXT SEARCH CONFIGURATION %s;\n",
					  fmtQualifiedDumpable(cfginfo));

	ArchiveEntry(fout, cfginfo->dobj.catId, cfginfo->dobj.dumpId,
				 cfginfo->dobj.name,
				 cfginfo->dobj.namespace->dobj.name,
				 NULL,
				 cfginfo->rolname,
				 false, "TEXT SEARCH CONFIGURATION", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump Configuration Comments */
	dumpComment(fout, "TEXT SEARCH CONFIGURATION", qcfgname,
				cfginfo->dobj.namespace->dobj.name, cfginfo->rolname,
				cfginfo->dobj.catId, 0, cfginfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
	free(qcfgname);
}

/*
 * dumpForeignDataWrapper
 *	  write out a single foreign-data wrapper definition
 */
static void
dumpForeignDataWrapper(Archive *fout, FdwInfo *fdwinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *qfdwname;

	/* Skip if not to be dumped */
	if (!fdwinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qfdwname = pg_strdup(fmtId(fdwinfo->dobj.name));

	appendPQExpBuffer(q, "CREATE FOREIGN DATA WRAPPER %s",
					  qfdwname);

	if (strcmp(fdwinfo->fdwhandler, "-") != 0)
		appendPQExpBuffer(q, " HANDLER %s", fdwinfo->fdwhandler);

	if (strcmp(fdwinfo->fdwvalidator, "-") != 0)
		appendPQExpBuffer(q, " VALIDATOR %s", fdwinfo->fdwvalidator);

	if (strlen(fdwinfo->fdwoptions) > 0)
		appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", fdwinfo->fdwoptions);

	appendPQExpBufferStr(q, ";\n");

	appendPQExpBuffer(delq, "DROP FOREIGN DATA WRAPPER %s;\n",
					  qfdwname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &fdwinfo->dobj,
										"FOREIGN DATA WRAPPER", qfdwname,
										NULL);

	ArchiveEntry(fout, fdwinfo->dobj.catId, fdwinfo->dobj.dumpId,
				 fdwinfo->dobj.name,
				 NULL,
				 NULL,
				 fdwinfo->rolname,
				 false, "FOREIGN DATA WRAPPER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Handle the ACL */
	dumpACL(fout, fdwinfo->dobj.catId, fdwinfo->dobj.dumpId,
			"FOREIGN DATA WRAPPER", qfdwname, NULL,
			NULL, fdwinfo->rolname,
			fdwinfo->fdwacl);

	/* Dump Foreign Data Wrapper Comments */
	dumpComment(fout, "FOREIGN DATA WRAPPER", qfdwname,
				NULL, fdwinfo->rolname,
				fdwinfo->dobj.catId, 0, fdwinfo->dobj.dumpId);

	free(qfdwname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
}

/*
 * dumpForeignServer
 *	  write out a foreign server definition
 */
static void
dumpForeignServer(Archive *fout, ForeignServerInfo *srvinfo)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PGresult   *res;
	char	   *qsrvname;
	char	   *fdwname;

	/* Skip if not to be dumped */
	if (!srvinfo->dobj.dump || dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	qsrvname = pg_strdup(fmtId(srvinfo->dobj.name));

	/* look up the foreign-data wrapper */
	appendPQExpBuffer(query, "SELECT fdwname "
					  "FROM pg_foreign_data_wrapper w "
					  "WHERE w.oid = '%u'",
					  srvinfo->srvfdw);
	res = ExecuteSqlQueryForSingleRow(fout, query->data);
	fdwname = PQgetvalue(res, 0, 0);

	appendPQExpBuffer(q, "CREATE SERVER %s", qsrvname);
	if (srvinfo->srvtype && strlen(srvinfo->srvtype) > 0)
	{
		appendPQExpBufferStr(q, " TYPE ");
		appendStringLiteralAH(q, srvinfo->srvtype, fout);
	}
	if (srvinfo->srvversion && strlen(srvinfo->srvversion) > 0)
	{
		appendPQExpBufferStr(q, " VERSION ");
		appendStringLiteralAH(q, srvinfo->srvversion, fout);
	}

	appendPQExpBufferStr(q, " FOREIGN DATA WRAPPER ");
	appendPQExpBufferStr(q, fmtId(fdwname));

	if (srvinfo->srvoptions && strlen(srvinfo->srvoptions) > 0)
		appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", srvinfo->srvoptions);

	appendPQExpBufferStr(q, ";\n");

	appendPQExpBuffer(delq, "DROP SERVER %s;\n",
					  qsrvname);

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &srvinfo->dobj,
										"SERVER", qsrvname, NULL);

	ArchiveEntry(fout, srvinfo->dobj.catId, srvinfo->dobj.dumpId,
				 srvinfo->dobj.name,
				 NULL,
				 NULL,
				 srvinfo->rolname,
				 false, "SERVER", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Handle the ACL */
	dumpACL(fout, srvinfo->dobj.catId, srvinfo->dobj.dumpId,
			"FOREIGN SERVER", qsrvname, NULL,
			NULL, srvinfo->rolname,
			srvinfo->srvacl);

	/* Dump user mappings */
	dumpUserMappings(fout,
					 srvinfo->dobj.name, NULL,
					 srvinfo->rolname,
					 srvinfo->dobj.catId, srvinfo->dobj.dumpId);

	/* Dump Foreign Server Comments */
	dumpComment(fout, "SERVER", qsrvname,
				NULL, srvinfo->rolname,
				srvinfo->dobj.catId, 0, srvinfo->dobj.dumpId);

	PQclear(res);

	free(qsrvname);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(query);
}

/*
 * dumpUserMappings
 *
 * This routine is used to dump any user mappings associated with the
 * server handed to this routine. Should be called after ArchiveEntry()
 * for the server.
 */
static void
dumpUserMappings(Archive *fout,
				 const char *servername, const char *namespace,
				 const char *owner,
				 CatalogId catalogId, DumpId dumpId)
{
	PQExpBuffer q;
	PQExpBuffer delq;
	PQExpBuffer query;
	PQExpBuffer tag;
	PGresult   *res;
	int			ntups;
	int			i_usename;
	int			i_umoptions;
	int			i;

	q = createPQExpBuffer();
	tag = createPQExpBuffer();
	delq = createPQExpBuffer();
	query = createPQExpBuffer();

	/*
	 * We read from the publicly accessible view pg_user_mappings, so as not
	 * to fail if run by a non-superuser.  Note that the view will show
	 * umoptions as null if the user hasn't got privileges for the associated
	 * server; this means that pg_dump will dump such a mapping, but with no
	 * OPTIONS clause.  A possible alternative is to skip such mappings
	 * altogether, but it's not clear that that's an improvement.
	 */
	appendPQExpBuffer(query,
					  "SELECT usename, "
					  "array_to_string(ARRAY("
					  "SELECT quote_ident(option_name) || ' ' || "
					  "quote_literal(option_value) "
					  "FROM pg_options_to_table(umoptions) "
					  "ORDER BY option_name"
					  "), E',\n    ') AS umoptions "
					  "FROM pg_user_mappings "
					  "WHERE srvid = '%u' "
					  "ORDER BY usename",
					  catalogId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);
	i_usename = PQfnumber(res, "usename");
	i_umoptions = PQfnumber(res, "umoptions");

	for (i = 0; i < ntups; i++)
	{
		char	   *usename;
		char	   *umoptions;

		usename = PQgetvalue(res, i, i_usename);
		umoptions = PQgetvalue(res, i, i_umoptions);

		resetPQExpBuffer(q);
		appendPQExpBuffer(q, "CREATE USER MAPPING FOR %s", fmtId(usename));
		appendPQExpBuffer(q, " SERVER %s", fmtId(servername));

		if (umoptions && strlen(umoptions) > 0)
			appendPQExpBuffer(q, " OPTIONS (\n    %s\n)", umoptions);

		appendPQExpBufferStr(q, ";\n");

		resetPQExpBuffer(delq);
		appendPQExpBuffer(delq, "DROP USER MAPPING FOR %s", fmtId(usename));
		appendPQExpBuffer(delq, " SERVER %s;\n", fmtId(servername));

		resetPQExpBuffer(tag);
		appendPQExpBuffer(tag, "USER MAPPING %s SERVER %s",
						  usename, servername);

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag->data,
					 namespace,
					 NULL,
					 owner, false,
					 "USER MAPPING", SECTION_PRE_DATA,
					 q->data, delq->data, NULL,
					 &dumpId, 1,
					 NULL, NULL);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delq);
	destroyPQExpBuffer(tag);
	destroyPQExpBuffer(q);
}

/*
 * Write out default privileges information
 */
static void
dumpDefaultACL(Archive *fout, DefaultACLInfo *daclinfo)
{
	PQExpBuffer q;
	PQExpBuffer tag;
	const char *type;

	/* Skip if not to be dumped */
	if (!daclinfo->dobj.dump || dataOnly || aclsSkip)
		return;

	q = createPQExpBuffer();
	tag = createPQExpBuffer();

	switch (daclinfo->defaclobjtype)
	{
		case DEFACLOBJ_RELATION:
			type = "TABLES";
			break;
		case DEFACLOBJ_SEQUENCE:
			type = "SEQUENCES";
			break;
		case DEFACLOBJ_FUNCTION:
			type = "FUNCTIONS";
			break;
		case DEFACLOBJ_TYPE:
			type = "TYPES";
			break;
		default:
			/* shouldn't get here */
			exit_horribly(NULL,
					  "unrecognized object type in default privileges: %d\n",
						  (int) daclinfo->defaclobjtype);
			type = "";			/* keep compiler quiet */
	}

	appendPQExpBuffer(tag, "DEFAULT PRIVILEGES FOR %s", type);

	/* build the actual command(s) for this tuple */
	if (!buildDefaultACLCommands(type,
								 daclinfo->dobj.namespace != NULL ?
								 daclinfo->dobj.namespace->dobj.name : NULL,
								 daclinfo->defaclacl,
								 daclinfo->defaclrole,
								 fout->remoteVersion,
								 q))
		exit_horribly(NULL, "could not parse default ACL list (%s)\n",
					  daclinfo->defaclacl);

	ArchiveEntry(fout, daclinfo->dobj.catId, daclinfo->dobj.dumpId,
				 tag->data,
	   daclinfo->dobj.namespace ? daclinfo->dobj.namespace->dobj.name : NULL,
				 NULL,
				 daclinfo->defaclrole,
				 false, "DEFAULT ACL", SECTION_POST_DATA,
				 q->data, "", NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(tag);
	destroyPQExpBuffer(q);
}

/*----------
 * Write out grant/revoke information
 *
 * 'objCatId' is the catalog ID of the underlying object.
 * 'objDumpId' is the dump ID of the underlying object.
 * 'type' must be one of
 *		TABLE, SEQUENCE, FUNCTION, LANGUAGE, SCHEMA, DATABASE, TABLESPACE,
 *		FOREIGN DATA WRAPPER, SERVER, or LARGE OBJECT.
 * 'name' is the formatted name of the object.  Must be quoted etc. already.
 * 'subname' is the formatted name of the sub-object, if any.  Must be quoted.
 *		(Currently we assume that subname is only provided for table columns.)
 * 'nspname' is the namespace the object is in (NULL if none).
 * 'owner' is the owner, NULL if there is no owner (for languages).
 * 'acls' is the string read out of the fooacl system catalog field;
 *		it will be parsed here.
 *----------
 */
static void
dumpACL(Archive *fout, CatalogId objCatId, DumpId objDumpId,
		const char *type, const char *name, const char *subname,
		const char *nspname, const char *owner,
		const char *acls)
{
	PQExpBuffer sql;

	/* Do nothing if ACL dump is not enabled */
	if (aclsSkip)
		return;

	/* --data-only skips ACLs *except* BLOB ACLs */
	if (dataOnly && strcmp(type, "LARGE OBJECT") != 0)
		return;

	sql = createPQExpBuffer();

	if (!buildACLCommands(name, subname, nspname, type, acls, owner,
						  "", fout->remoteVersion, sql))
		exit_horribly(NULL,
					"could not parse ACL list (%s) for object \"%s\" (%s)\n",
					  acls, name, type);

	if (sql->len > 0)
	{
		PQExpBuffer tag = createPQExpBuffer();

		if (subname)
			appendPQExpBuffer(tag, "COLUMN %s.%s", name, subname);
		else
			appendPQExpBuffer(tag, "%s %s", type, name);

		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag->data, nspname,
					 NULL,
					 owner ? owner : "",
					 false, "ACL", SECTION_NONE,
					 sql->data, "", NULL,
					 &(objDumpId), 1,
					 NULL, NULL);
		destroyPQExpBuffer(tag);
	}

	destroyPQExpBuffer(sql);
}

/*
 * dumpSecLabel
 *
 * This routine is used to dump any security labels associated with the
 * object handed to this routine. The routine takes the object type
 * and object name (ready to print, except for schema decoration), plus
 * the namespace and owner of the object (for labeling the ArchiveEntry),
 * plus catalog ID and subid which are the lookup key for pg_seclabel,
 * plus the dump ID for the object (for setting a dependency).
 * If a matching pg_seclabel entry is found, it is dumped.
 *
 * Note: although this routine takes a dumpId for dependency purposes,
 * that purpose is just to mark the dependency in the emitted dump file
 * for possible future use by pg_restore.  We do NOT use it for determining
 * ordering of the label in the dump file, because this routine is called
 * after dependency sorting occurs.  This routine should be called just after
 * calling ArchiveEntry() for the specified object.
 */
static void
dumpSecLabel(Archive *fout, const char *type, const char *name,
			 const char *namespace, const char *owner,
			 CatalogId catalogId, int subid, DumpId dumpId)
{
	SecLabelItem *labels;
	int			nlabels;
	int			i;
	PQExpBuffer query;

	/* do nothing, if --no-security-labels is supplied */
	if (no_security_labels)
		return;

	/* Security labels are schema not data ... except blob labels are data */
	if (strcmp(type, "LARGE OBJECT") != 0)
	{
		if (dataOnly)
			return;
	}
	else
	{
		/* We do dump blob security labels in binary-upgrade mode */
		if (schemaOnly && !binary_upgrade)
			return;
	}

	/* Search for security labels associated with catalogId, using table */
	nlabels = findSecLabels(fout, catalogId.tableoid, catalogId.oid, &labels);

	query = createPQExpBuffer();

	for (i = 0; i < nlabels; i++)
	{
		/*
		 * Ignore label entries for which the subid doesn't match.
		 */
		if (labels[i].objsubid != subid)
			continue;

		appendPQExpBuffer(query,
						  "SECURITY LABEL FOR %s ON %s ",
						  fmtId(labels[i].provider), type);
		if (namespace && *namespace)
			appendPQExpBuffer(query, "%s.", fmtId(namespace));
		appendPQExpBuffer(query, "%s IS ", name);
		appendStringLiteralAH(query, labels[i].label, fout);
		appendPQExpBufferStr(query, ";\n");
	}

	if (query->len > 0)
	{
		PQExpBuffer tag = createPQExpBuffer();

		appendPQExpBuffer(tag, "%s %s", type, name);
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 tag->data, namespace, NULL, owner,
					 false, "SECURITY LABEL", SECTION_NONE,
					 query->data, "", NULL,
					 &(dumpId), 1,
					 NULL, NULL);
		destroyPQExpBuffer(tag);
	}

	destroyPQExpBuffer(query);
}

/*
 * dumpTableSecLabel
 *
 * As above, but dump security label for both the specified table (or view)
 * and its columns.
 */
static void
dumpTableSecLabel(Archive *fout, TableInfo *tbinfo, const char *reltypename)
{
	SecLabelItem *labels;
	int			nlabels;
	int			i;
	PQExpBuffer query;
	PQExpBuffer target;

	/* do nothing, if --no-security-labels is supplied */
	if (no_security_labels)
		return;

	/* SecLabel are SCHEMA not data */
	if (dataOnly)
		return;

	/* Search for comments associated with relation, using table */
	nlabels = findSecLabels(fout,
							tbinfo->dobj.catId.tableoid,
							tbinfo->dobj.catId.oid,
							&labels);

	/* If security labels exist, build SECURITY LABEL statements */
	if (nlabels <= 0)
		return;

	query = createPQExpBuffer();
	target = createPQExpBuffer();

	for (i = 0; i < nlabels; i++)
	{
		const char *colname;
		const char *provider = labels[i].provider;
		const char *label = labels[i].label;
		int			objsubid = labels[i].objsubid;

		resetPQExpBuffer(target);
		if (objsubid == 0)
		{
			appendPQExpBuffer(target, "%s %s", reltypename,
							  fmtQualifiedDumpable(tbinfo));
		}
		else
		{
			colname = getAttrName(objsubid, tbinfo);
			/* first fmtXXX result must be consumed before calling again */
			appendPQExpBuffer(target, "COLUMN %s",
							  fmtQualifiedDumpable(tbinfo));
			appendPQExpBuffer(target, ".%s", fmtId(colname));
		}
		appendPQExpBuffer(query, "SECURITY LABEL FOR %s ON %s IS ",
						  fmtId(provider), target->data);
		appendStringLiteralAH(query, label, fout);
		appendPQExpBufferStr(query, ";\n");
	}
	if (query->len > 0)
	{
		resetPQExpBuffer(target);
		appendPQExpBuffer(target, "%s %s", reltypename,
						  fmtId(tbinfo->dobj.name));
		ArchiveEntry(fout, nilCatalogId, createDumpId(),
					 target->data,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL, tbinfo->rolname,
					 false, "SECURITY LABEL", SECTION_NONE,
					 query->data, "", NULL,
					 &(tbinfo->dobj.dumpId), 1,
					 NULL, NULL);
	}
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(target);
}

/*
 * findSecLabels
 *
 * Find the security label(s), if any, associated with the given object.
 * All the objsubid values associated with the given classoid/objoid are
 * found with one search.
 */
static int
findSecLabels(Archive *fout, Oid classoid, Oid objoid, SecLabelItem **items)
{
	/* static storage for table of security labels */
	static SecLabelItem *labels = NULL;
	static int	nlabels = -1;

	SecLabelItem *middle = NULL;
	SecLabelItem *low;
	SecLabelItem *high;
	int			nmatch;

	/* Get security labels if we didn't already */
	if (nlabels < 0)
		nlabels = collectSecLabels(fout, &labels);

	if (nlabels <= 0)			/* no labels, so no match is possible */
	{
		*items = NULL;
		return 0;
	}

	/*
	 * Do binary search to find some item matching the object.
	 */
	low = &labels[0];
	high = &labels[nlabels - 1];
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
 * collectSecLabels
 *
 * Construct a table of all security labels available for database objects.
 * It's much faster to pull them all at once.
 *
 * The table is sorted by classoid/objid/objsubid for speed in lookup.
 */
static int
collectSecLabels(Archive *fout, SecLabelItem **items)
{
	PGresult   *res;
	PQExpBuffer query;
	int			i_label;
	int			i_provider;
	int			i_classoid;
	int			i_objoid;
	int			i_objsubid;
	int			ntups;
	int			i;
	SecLabelItem *labels;

	query = createPQExpBuffer();

	appendPQExpBufferStr(query,
						 "SELECT label, provider, classoid, objoid, objsubid "
						 "FROM pg_catalog.pg_seclabel "
						 "ORDER BY classoid, objoid, objsubid");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	/* Construct lookup table containing OIDs in numeric form */
	i_label = PQfnumber(res, "label");
	i_provider = PQfnumber(res, "provider");
	i_classoid = PQfnumber(res, "classoid");
	i_objoid = PQfnumber(res, "objoid");
	i_objsubid = PQfnumber(res, "objsubid");

	ntups = PQntuples(res);

	labels = (SecLabelItem *) pg_malloc(ntups * sizeof(SecLabelItem));

	for (i = 0; i < ntups; i++)
	{
		labels[i].label = PQgetvalue(res, i, i_label);
		labels[i].provider = PQgetvalue(res, i, i_provider);
		labels[i].classoid = atooid(PQgetvalue(res, i, i_classoid));
		labels[i].objoid = atooid(PQgetvalue(res, i, i_objoid));
		labels[i].objsubid = atoi(PQgetvalue(res, i, i_objsubid));
	}

	/* Do NOT free the PGresult since we are keeping pointers into it */
	destroyPQExpBuffer(query);

	*items = labels;
	return ntups;
}

/*
 * dumpTable
 *	  write out to fout the declarations (not data) of a user-defined table
 */
static void
dumpTable(Archive *fout, TableInfo *tbinfo)
{
	if (tbinfo->dobj.dump && !dataOnly)
	{
		char	   *namecopy;
		const char *objtype;

		if (tbinfo->relkind == RELKIND_SEQUENCE)
			dumpSequence(fout, tbinfo);
		else
			dumpTableSchema(fout, tbinfo);

		/* Handle the ACL here */
		namecopy = pg_strdup(fmtId(tbinfo->dobj.name));
		objtype = (tbinfo->relkind == RELKIND_SEQUENCE) ? "SEQUENCE" : "TABLE";
		dumpACL(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				objtype, namecopy, NULL,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tbinfo->relacl);

		/*
		 * Handle column ACLs, if any.  Note: we pull these with a separate
		 * query rather than trying to fetch them during getTableAttrs, so
		 * that we won't miss ACLs on system columns.
		 */
		if (fout->remoteVersion >= 80400)
		{
			PQExpBuffer query = createPQExpBuffer();
			PGresult   *res;
			int			i;

			if (!fout->is_prepared[PREPQUERY_GETCOLUMNACLS])
			{
				/* Set up query for column ACLs */
				appendPQExpBufferStr(query,
									"PREPARE getColumnACLs(pg_catalog.oid) AS\n");

				appendPQExpBuffer(query,
							"SELECT attname, attacl FROM pg_catalog.pg_attribute "
									"WHERE attrelid = $1 AND NOT attisdropped AND attacl IS NOT NULL "
									"ORDER BY attnum");

				ExecuteSqlStatement(fout, query->data);

				fout->is_prepared[PREPQUERY_GETCOLUMNACLS] = true;
			}

			printfPQExpBuffer(query,
								"EXECUTE getColumnACLs('%u')",
								tbinfo->dobj.catId.oid);

			res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

			for (i = 0; i < PQntuples(res); i++)
			{
				char	   *attname = PQgetvalue(res, i, 0);
				char	   *attacl = PQgetvalue(res, i, 1);
				char	   *attnamecopy;

				attnamecopy = pg_strdup(fmtId(attname));
				/* Column's GRANT type is always TABLE */
				dumpACL(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
						"TABLE", namecopy, attnamecopy,
						tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
						attacl);
				free(attnamecopy);
			}
			PQclear(res);
			destroyPQExpBuffer(query);
		}

		free(namecopy);
	}
}

static void
dumpExternal(Archive *fout, TableInfo *tbinfo, PQExpBuffer q, PQExpBuffer delq)
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
		char	   *extencoding;
		char	   *writable = NULL;
		char	   *tmpstring = NULL;
		char 	   *tabfmt = NULL;
		char	   *customfmt = NULL;
		bool		isweb = false;
		bool		iswritable = false;
		char	   *options;
		bool		gpdb6OrLater = isGPDB6000OrLater(fout);
		char	   *logerrors = NULL;
		char	   *on_clause;
		char	   *qualrelname;
		bool		log_errors_persistently = false;
		PQExpBuffer query = createPQExpBuffer();

		qualrelname = pg_strdup(fmtQualifiedDumpable(tbinfo));

		/*
		 * DROP must be fully qualified in case same name appears in
		 * pg_catalog
		 */
		appendPQExpBuffer(delq, "DROP EXTERNAL TABLE %s.",
						  qualrelname);

		/* Now get required information from pg_exttable */
		if (gpdb6OrLater)
		{
			appendPQExpBuffer(query,
					"SELECT x.urilocation, x.execlocation, x.fmttype, x.fmtopts, x.command, "
						   "x.rejectlimit, x.rejectlimittype, "
						   "CASE WHEN x.logerrors THEN true ELSE null END AS logerrors, "
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
		else
		{
			appendPQExpBuffer(query,
					"SELECT x.urilocation, x.execlocation, x.fmttype, x.fmtopts, x.command, "
						   "x.rejectlimit, x.rejectlimittype, "
						   "x.fmterrtbl AS logerrors, "
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

		res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

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
			exit_nicely(1);

		}


		urilocations = PQgetvalue(res, 0, 0);
		execlocations = PQgetvalue(res, 0, 1);
		fmttype = PQgetvalue(res, 0, 2);
		fmtopts = PQgetvalue(res, 0, 3);
		command = PQgetvalue(res, 0, 4);
		rejlim = PQgetvalue(res, 0, 5);
		rejlimtype = PQgetvalue(res, 0, 6);
		logerrors = PQgetvalue(res, 0, 7);
		extencoding = PQgetvalue(res, 0, 8);
		writable = PQgetvalue(res, 0, 9);
		options = PQgetvalue(res, 0, 10);

		on_clause = execlocations;

		if ((command && strlen(command) > 0) ||
			(strncmp(urilocations + 1, "http", strlen("http")) == 0))
			isweb = true;

		if (writable && writable[0] == 't')
			iswritable = true;

		if (binary_upgrade)
			binary_upgrade_set_pg_class_oids(fout, q, tbinfo->dobj.catId.oid, false);

		appendPQExpBuffer(q, "CREATE %sEXTERNAL %sTABLE %s (",
						  (iswritable ? "WRITABLE " : ""),
						  (isweb ? "WEB " : ""),
						  qualrelname);

		int actual_atts = 0;
		int j;
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/* Is the attribute not dropped? */
			if (shouldPrintColumn(tbinfo, j))
			{
				/* Format properly if not first attr */
				if (actual_atts > 0)
					appendPQExpBufferChar(q, ',');
				appendPQExpBufferStr(q, "\n    ");

				/* Attribute name */
				appendPQExpBuffer(q, "%s ", fmtId(tbinfo->attnames[j]));

				/* Attribute type */
				if (tbinfo->attisdropped[j])
				{
					/*
					 * ALTER TABLE DROP COLUMN clears
					 * pg_attribute.atttypid, so we will not have gotten a
					 * valid type name; insert INTEGER as a stopgap. We'll
					 * clean things up later.
					 */
					appendPQExpBufferStr(q, " INTEGER /* dummy */");
				}
				else
				{
					appendPQExpBufferStr(q, tbinfo->atttypnames[j]);
				}

				actual_atts++;
			}
		}

		appendPQExpBufferStr(q, "\n)");

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
			appendPQExpBufferStr(q, "\n) ");
		}

		/*
		 * Add ON clause (unless WRITABLE table, which doesn't allow ON).
		 * ON clauses were up until 5.0 supported only on EXECUTE, in 5.0
		 * and thereafter they are allowed on all external tables.
		 */
		if (!iswritable)
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
				exit_nicely(1);
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

		if (options && options[0] != '\0')
		{
			char *error_log_persistent = "error_log_persistent 'true'";
			char *pos = strstr(options, error_log_persistent);
			int error_log_len = strlen(error_log_persistent);
			if (pos)
			{
				log_errors_persistently = true;
				if (*(pos + error_log_len) == ',')
						error_log_len += 6;
				if (strlen(options) - error_log_len != 0)
				{
					char *opts = pg_malloc(sizeof(char) * (strlen(options) - error_log_len + 1));
					int prev_len = pos - options;
					if (prev_len > 0)
						strncpy(opts, options, prev_len);
					StrNCpy(opts + prev_len, pos + error_log_len,
							strlen(options) - prev_len - error_log_len + 1 /* for \0 */);
					appendPQExpBuffer(q, "OPTIONS (\n %s\n )\n", opts);
					free(opts);
				}
			}
			else
				appendPQExpBuffer(q, "OPTIONS (\n %s\n )\n", options);
		}

		/* add ENCODING clause */
		appendPQExpBuffer(q, "ENCODING '%s'", extencoding);

		/* add Single Row Error Handling clause (if any) */
		if (rejlim && strlen(rejlim) > 0)
		{
			appendPQExpBufferChar(q, '\n');

			/*
				* Error tables were removed and replaced with file error
				* logging.
				*/
			if (logerrors && strlen(logerrors) > 0)
			{
				appendPQExpBufferStr(q, "LOG ERRORS ");
				if (log_errors_persistently)
					appendPQExpBufferStr(q, "PERSISTENTLY ");
			}

			/* reject limit */
			appendPQExpBuffer(q, "SEGMENT REJECT LIMIT %s", rejlim);

			/* reject limit type */
			if (rejlimtype[0] == 'r')
				appendPQExpBufferStr(q, " ROWS");
			else
				appendPQExpBufferStr(q, " PERCENT");
		}

		/* DISTRIBUTED BY clause (if WRITABLE table) */
		if (iswritable)
			addDistributedBy(fout, q, tbinfo, actual_atts);

		appendPQExpBufferStr(q, ";\n");

		PQclear(res);


		destroyPQExpBuffer(query);
}

/*
 * Create the AS clause for a view or materialized view. The semicolon is
 * stripped because a materialized view must add a WITH NO DATA clause.
 *
 * This returns a new buffer which must be freed by the caller.
 */
static PQExpBuffer
createViewAsClause(Archive *fout, TableInfo *tbinfo)
{
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer result = createPQExpBuffer();
	PGresult   *res;
	int			len;

	/* Fetch the view definition */
	appendPQExpBuffer(query,
		"SELECT pg_catalog.pg_get_viewdef('%u'::pg_catalog.oid) AS viewdef",
						tbinfo->dobj.catId.oid);

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		if (PQntuples(res) < 1)
			exit_horribly(NULL, "query to obtain definition of view \"%s\" returned no data\n",
						  tbinfo->dobj.name);
		else
			exit_horribly(NULL, "query to obtain definition of view \"%s\" returned more than one definition\n",
						  tbinfo->dobj.name);
	}

	len = PQgetlength(res, 0, 0);

	if (len == 0)
		exit_horribly(NULL, "definition of view \"%s\" appears to be empty (length zero)\n",
					  tbinfo->dobj.name);

	/* Strip off the trailing semicolon so that other things may follow. */
	Assert(PQgetvalue(res, 0, 0)[len - 1] == ';');
	appendBinaryPQExpBuffer(result, PQgetvalue(res, 0, 0), len - 1);

	PQclear(res);
	destroyPQExpBuffer(query);

	return result;
}

/*
 * Create a dummy AS clause for a view.  This is used when the real view
 * definition has to be postponed because of circular dependencies.
 * We must duplicate the view's external properties -- column names and types
 * (including collation) -- so that it works for subsequent references.
 *
 * This returns a new buffer which must be freed by the caller.
 */
static PQExpBuffer
createDummyViewAsClause(Archive *fout, TableInfo *tbinfo)
{
	PQExpBuffer result = createPQExpBuffer();
	int			j;

	appendPQExpBufferStr(result, "SELECT");

	for (j = 0; j < tbinfo->numatts; j++)
	{
		if (j > 0)
			appendPQExpBufferChar(result, ',');
		appendPQExpBufferStr(result, "\n    ");

		appendPQExpBuffer(result, "NULL::%s", tbinfo->atttypnames[j]);

		/*
		 * Must add collation if not default for the type, because CREATE OR
		 * REPLACE VIEW won't change it
		 */
		if (OidIsValid(tbinfo->attcollation[j]))
		{
			CollInfo   *coll;

			coll = findCollationByOid(tbinfo->attcollation[j]);
			if (coll)
				appendPQExpBuffer(result, " COLLATE %s",
								  fmtQualifiedDumpable(coll));
		}

		appendPQExpBuffer(result, " AS %s", fmtId(tbinfo->attnames[j]));
	}

	return result;
}

/*
 * dumpTableSchema
 *	  write the declaration (not data) of one user-defined table or view
 */
static void
dumpTableSchema(Archive *fout, TableInfo *tbinfo)
{
	PQExpBuffer q = createPQExpBuffer();
	PQExpBuffer delq = createPQExpBuffer();
	char	   *qrelname;
	char	   *qualrelname;
	int			numParents;
	TableInfo **parents;
	int			actual_atts;	/* number of attrs in this CREATE statement */
	const char *reltypename;
	char	   *storage;
	char	   *srvname;
	char	   *ftoptions = NULL;
	int			j,
				k;
	bool		hasExternalPartitions = false;

	qrelname = pg_strdup(fmtId(tbinfo->dobj.name));
	qualrelname = pg_strdup(fmtQualifiedDumpable(tbinfo));

	if (binary_upgrade)
		binary_upgrade_set_type_oids_by_rel(fout, q, tbinfo);

	/* Is it a table or a view? */
	if (tbinfo->relkind == RELKIND_VIEW)
	{
		PQExpBuffer result;

		/*
		 * Note: keep this code in sync with the is_view case in dumpRule()
		 */

		reltypename = "VIEW";

		appendPQExpBuffer(delq, "DROP VIEW %s;\n", qualrelname);

		if (binary_upgrade)
			binary_upgrade_set_pg_class_oids(fout, q,
											 tbinfo->dobj.catId.oid, false);

		appendPQExpBuffer(q, "CREATE VIEW %s", qualrelname);

		if (tbinfo->dummy_view)
			result = createDummyViewAsClause(fout, tbinfo);
		else
		{
			if (nonemptyReloptions(tbinfo->reloptions))
			{
				appendPQExpBufferStr(q, " WITH (");
				fmtReloptionsArray(fout, q, tbinfo->reloptions, "");
				appendPQExpBufferChar(q, ')');
			}
			result = createViewAsClause(fout, tbinfo);
		}
		appendPQExpBuffer(q, " AS\n%s", result->data);
		destroyPQExpBuffer(result);

		if (tbinfo->checkoption != NULL && !tbinfo->dummy_view)
			appendPQExpBuffer(q, "\n  WITH %s CHECK OPTION", tbinfo->checkoption);
		appendPQExpBufferStr(q, ";\n");
	}
	/* START MPP ADDITION */
	else if (tbinfo->relstorage == RELSTORAGE_EXTERNAL)
	{
		reltypename = "EXTERNAL TABLE";
		dumpExternal(fout, tbinfo, q, delq);
	}
	/* END MPP ADDITION */
	else
	{
		switch (tbinfo->relkind)
		{
			case (RELKIND_FOREIGN_TABLE):
				{
					PQExpBuffer query = createPQExpBuffer();
					PGresult   *res;
					int			i_srvname;
					int			i_ftoptions;

					reltypename = "FOREIGN TABLE";

					/* retrieve name of foreign server and generic options */
					appendPQExpBuffer(query,
									  "SELECT fs.srvname, "
									  "pg_catalog.array_to_string(ARRAY("
							 "SELECT pg_catalog.quote_ident(option_name) || "
							 "' ' || pg_catalog.quote_literal(option_value) "
							"FROM pg_catalog.pg_options_to_table(ftoptions) "
									  "ORDER BY option_name"
									  "), E',\n    ') AS ftoptions "
									  "FROM pg_catalog.pg_foreign_table ft "
									  "JOIN pg_catalog.pg_foreign_server fs "
									  "ON (fs.oid = ft.ftserver) "
									  "WHERE ft.ftrelid = '%u'",
									  tbinfo->dobj.catId.oid);
					res = ExecuteSqlQueryForSingleRow(fout, query->data);
					i_srvname = PQfnumber(res, "srvname");
					i_ftoptions = PQfnumber(res, "ftoptions");
					srvname = pg_strdup(PQgetvalue(res, 0, i_srvname));
					ftoptions = pg_strdup(PQgetvalue(res, 0, i_ftoptions));
					PQclear(res);
					destroyPQExpBuffer(query);
					break;
				}
			case (RELKIND_MATVIEW):
				reltypename = "MATERIALIZED VIEW";
				srvname = NULL;
				ftoptions = NULL;
				break;
			default:
				reltypename = "TABLE";
				srvname = NULL;
				ftoptions = NULL;
		}

		numParents = tbinfo->numParents;
		parents = tbinfo->parents;

		appendPQExpBuffer(delq, "DROP %s %s;\n", reltypename, qualrelname);

		if (binary_upgrade)
		{
			binary_upgrade_set_pg_class_oids(fout, q,
											 tbinfo->dobj.catId.oid, false);

			/*
			 * If this is a partition hierarchy parent, dump the Oid
			 * preassignments for each partition member individually.
			 */
			if (tbinfo->parparent)
			{
				PQExpBuffer 	partquery = createPQExpBuffer();
				PGresult	   *partres;

				appendPQExpBuffer(partquery, "SELECT DISTINCT(child.oid) "
											 "FROM pg_catalog.pg_partition part, "
											 "     pg_catalog.pg_partition_rule rule, "
											 "     pg_catalog.pg_class child "
											 "WHERE part.parrelid = '%u'::pg_catalog.oid "
											 "  AND rule.paroid = part.oid "
											 "  AND child.oid = rule.parchildrelid",
											 tbinfo->dobj.catId.oid);
				partres = ExecuteSqlQuery(fout, partquery->data, PGRES_TUPLES_OK);

				/* It really should..  */
				if (PQntuples(partres) > 0)
				{
					int			i;

					for (i = 0; i < PQntuples(partres); i++)
					{
						Oid part_oid = atooid(PQgetvalue(partres, i, 0));
						TableInfo *tbinfo = findTableByOid(part_oid);

						binary_upgrade_set_pg_class_oids(fout, q, part_oid, false);
						binary_upgrade_set_type_oids_by_rel(fout, q, tbinfo);
					}
				}

				PQclear(partres);
				destroyPQExpBuffer(partquery);
			}
		}

		appendPQExpBuffer(q, "CREATE %s%s %s",
						  tbinfo->relpersistence == RELPERSISTENCE_UNLOGGED ?
						  "UNLOGGED " : "",
						  reltypename,
						  qualrelname);

		/*
		 * Attach to type, if reloftype; except in case of a binary upgrade,
		 * we dump the table normally and attach it to the type afterward.
		 */
		if (tbinfo->reloftype && !binary_upgrade)
			appendPQExpBuffer(q, " OF %s", tbinfo->reloftype);

		if (tbinfo->relkind != RELKIND_MATVIEW)
		{
			/* Dump the attributes */
			actual_atts = 0;
			for (j = 0; j < tbinfo->numatts; j++)
			{
				/*
				 * Normally, dump if it's locally defined in this table, and
				 * not dropped.  But for binary upgrade, we'll dump all the
				 * columns, and then fix up the dropped and nonlocal cases
				 * below.
				 */
				if (shouldPrintColumn(tbinfo, j))
				{
					/*
					 * Default value --- suppress if to be printed separately.
					 */
					bool		has_default = (tbinfo->attrdefs[j] != NULL &&
											 !tbinfo->attrdefs[j]->separate);

					/*
					 * Not Null constraint --- suppress if inherited, except
					 * in binary-upgrade case where that won't work.
					 */
					bool		has_notnull = (tbinfo->notnull[j] &&
											   (!tbinfo->inhNotNull[j] ||
												binary_upgrade));

					/* Skip column if fully defined by reloftype */
					if (tbinfo->reloftype &&
						!has_default && !has_notnull && !binary_upgrade)
						continue;

					/* Format properly if not first attr */
					if (actual_atts == 0)
						appendPQExpBufferStr(q, " (");
					else
						appendPQExpBufferStr(q, ",");
					appendPQExpBufferStr(q, "\n    ");
					actual_atts++;

					/* Attribute name */
					appendPQExpBufferStr(q, fmtId(tbinfo->attnames[j]));

					if (tbinfo->attisdropped[j])
					{
						/*
						 * ALTER TABLE DROP COLUMN clears
						 * pg_attribute.atttypid, so we will not have gotten a
						 * valid type name; insert INTEGER as a stopgap. We'll
						 * clean things up later.
						 */
						appendPQExpBufferStr(q, " INTEGER /* dummy */");
						/* Skip all the rest, too */
						continue;
					}

					/* Attribute type */
					if (tbinfo->reloftype && !binary_upgrade)
					{
						appendPQExpBufferStr(q, " WITH OPTIONS");
					}
					else
					{
						appendPQExpBuffer(q, " %s",
										  tbinfo->atttypnames[j]);
					}

					/* Add collation if not default for the type */
					if (OidIsValid(tbinfo->attcollation[j]))
					{
						CollInfo   *coll;

						coll = findCollationByOid(tbinfo->attcollation[j]);
						if (coll)
							appendPQExpBuffer(q, " COLLATE %s",
											  fmtQualifiedDumpable(coll));
					}

					if (has_default)
						appendPQExpBuffer(q, " DEFAULT %s",
										  tbinfo->attrdefs[j]->adef_expr);

					if (has_notnull)
						appendPQExpBufferStr(q, " NOT NULL");

					/* Column Storage attributes */
					if (tbinfo->attencoding[j] != NULL)
						appendPQExpBuffer(q, " ENCODING (%s)",
										  tbinfo->attencoding[j]);
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

				if (actual_atts == 0)
					appendPQExpBufferStr(q, " (\n    ");
				else
					appendPQExpBufferStr(q, ",\n    ");

				appendPQExpBuffer(q, "CONSTRAINT %s ",
								  fmtId(constr->dobj.name));
				appendPQExpBufferStr(q, constr->condef);

				actual_atts++;
			}

			if (actual_atts)
				appendPQExpBufferStr(q, "\n)");
			else if (!(tbinfo->reloftype && !binary_upgrade))
			{
				/*
				 * We must have a parenthesized attribute list, even though
				 * empty, when not using the OF TYPE syntax.
				 */
				appendPQExpBufferStr(q, " (\n)");
			}

			if (numParents > 0 && !binary_upgrade)
			{
				appendPQExpBufferStr(q, "\nINHERITS (");
				for (k = 0; k < numParents; k++)
				{
					TableInfo  *parentRel = parents[k];

					if (k > 0)
						appendPQExpBufferStr(q, ", ");
					appendPQExpBufferStr(q, fmtQualifiedDumpable(parentRel));
				}
				appendPQExpBufferChar(q, ')');
			}

			if (tbinfo->relkind == RELKIND_FOREIGN_TABLE)
				appendPQExpBuffer(q, "\nSERVER %s", fmtId(srvname));
		}
		else
			actual_atts = 0;

		if (nonemptyReloptions(tbinfo->reloptions) ||
			nonemptyReloptions(tbinfo->toast_reloptions))
		{
			bool		addcomma = false;

			appendPQExpBufferStr(q, "\nWITH (");
			if (nonemptyReloptions(tbinfo->reloptions))
			{
				addcomma = true;
				fmtReloptionsArray(fout, q, tbinfo->reloptions, "");
			}
			if (nonemptyReloptions(tbinfo->toast_reloptions))
			{
				if (addcomma)
					appendPQExpBufferStr(q, ", ");
				fmtReloptionsArray(fout, q, tbinfo->toast_reloptions, "toast.");
			}
			appendPQExpBufferChar(q, ')');
		}


		/*
		 * For materialized views, create the AS clause just like a view. At
		 * this point, we always mark the view as not populated.
		 */
		if (tbinfo->relkind == RELKIND_MATVIEW)
		{
			PQExpBuffer result;

			result = createViewAsClause(fout, tbinfo);
			appendPQExpBuffer(q, " AS\n%s\n  WITH NO DATA\n",
							  result->data);
			destroyPQExpBuffer(result);
		}
		else
			appendPQExpBufferStr(q, "\n");

		/* START MPP ADDITION */

		/*
		 * Dump distributed by clause.
		 */
		if (dumpPolicy && tbinfo->relkind != RELKIND_FOREIGN_TABLE && !tbinfo->parents)
			addDistributedBy(fout, q, tbinfo, actual_atts);

		/*
		 * Add the partitioning constraints to the table definition. This
		 * check used to look for existence of pg_partition table to make the
		 * decision, instead seems better to decide based on version. GPDB6
		 * and below have pg_get_partition_def functions.
		 */
		if (fout->remoteVersion <= 90426 && tbinfo->parparent)
		{
			PQExpBuffer query = createPQExpBuffer();
			PGresult   *res;

			resetPQExpBuffer(query);
			/* MPP-6297: dump by tablename */
			appendPQExpBuffer(query, "SELECT "
					"pg_get_partition_def('%u'::pg_catalog.oid, true, true) ",
								tbinfo->dobj.catId.oid);

			res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

			if (PQntuples(res) != 1)
			{
				if (PQntuples(res) < 1)
					write_msg(NULL, "query to obtain definition of table \"%s\" returned no data\n",
							  tbinfo->dobj.name);
				else
					write_msg(NULL, "query to obtain definition of table \"%s\" returned more than one definition\n",
							  tbinfo->dobj.name);
				exit_nicely(1);
			}

			appendPQExpBuffer(q, " %s", PQgetvalue(res, 0, 0));
			PQclear(res);

			/*
			 * MPP-6095: dump ALTER TABLE statements for subpartition
			 * templates
			 */
			resetPQExpBuffer(query);

			appendPQExpBuffer(
								query, "SELECT "
								"pg_get_partition_template_def('%u'::pg_catalog.oid, true, true) ",
								tbinfo->dobj.catId.oid);

			res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

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
				exit_nicely(1);
			}

			/*
				* MPP-9537: terminate (with semicolon) the previous
				* statement, and dump the template definitions
				*/
			if (!PQgetisnull(res, 0, 0) &&
				PQgetlength(res, 0, 0))
				appendPQExpBuffer(q, ";\n %s", PQgetvalue(res, 0, 0));

			PQclear(res);

			/* Find out if there are any external partitions. */
			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "SELECT EXISTS (SELECT 1 "
										"  FROM pg_class part "
										"  JOIN pg_partition_rule pr ON (part.oid = pr.parchildrelid) "
										"  JOIN pg_partition p ON (pr.paroid = p.oid) "
										"WHERE p.parrelid = '%u'::pg_catalog.oid "
										"  AND part.relstorage = '%c') "
										"AS has_external_partitions;",
								tbinfo->dobj.catId.oid, RELSTORAGE_EXTERNAL);

			res = ExecuteSqlQueryForSingleRow(fout, query->data);
			hasExternalPartitions = (PQgetvalue(res, 0, 0)[0] == 't');
			PQclear(res);

			destroyPQExpBuffer(query);
		}

		/* END MPP ADDITION */

		/* Dump generic options if any */
		if (ftoptions && ftoptions[0])
			appendPQExpBuffer(q, "\nOPTIONS (\n    %s\n)", ftoptions);

		appendPQExpBufferStr(q, ";\n");

		/*
		 * Exchange external partitions. This is an expensive process, so only
		 * run it if we've found evidence of external partitions up above.
		 */
		if (hasExternalPartitions)
		{
			int i = 0;
			int ntups = 0;
			char *relname = NULL;
			int i_relname = 0;
			int i_parname = 0;
			int i_partitionrank = 0;
			PQExpBuffer query = createPQExpBuffer();
			PGresult   *res;

			/*
			 * The multiple JOINs below trigger an apparent planner bug which
			 * may effectively hang the backend. This bug is present in both
			 * released versions of GPDB and the current development tip at time
			 * of writing. Disable nestloops temporarily as a workaround.
			 *
			 * TODO: when this bug is fixed, version-gate this code so that we
			 * don't run it on well-behaved backends.
			 */
			ExecuteSqlStatement(fout, "SET enable_nestloop TO off");

			appendPQExpBuffer(query, "SELECT DISTINCT cc.relname, ps.partitionrank, pp.parname "
					"FROM pg_partition p "
					"JOIN pg_class c on (p.parrelid = c.oid) "
					"JOIN pg_partitions ps on (c.relname = ps.tablename) "
					"JOIN pg_class cc on (ps.partitiontablename = cc.relname) "
					"JOIN pg_partition_rule pp on (cc.oid = pp.parchildrelid) "
					"WHERE p.parrelid = %u AND cc.relstorage = '%c';",
					tbinfo->dobj.catId.oid, RELSTORAGE_EXTERNAL);

			res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

			ntups = PQntuples(res);
			i_relname = PQfnumber(res, "relname");
			i_parname = PQfnumber(res, "parname");
			i_partitionrank = PQfnumber(res, "partitionrank");

			/* FIXME: does this code handle external SUBPARTITIONs correctly? */
			for (i = 0; i < ntups; i++)
			{
				char tmpExtTable[500] = {0};
				relname = pg_strdup(PQgetvalue(res, i, i_relname));
				snprintf(tmpExtTable, sizeof(tmpExtTable), "%s%s", relname, EXT_PARTITION_NAME_POSTFIX);
				char *qualTmpExtTable = pg_strdup(fmtQualifiedId(fout->remoteVersion,
																 tbinfo->dobj.namespace->dobj.name,
																 tmpExtTable));

				appendPQExpBuffer(q, "ALTER TABLE %s ", qualrelname);
				/*
				 * If it is an anonymous range partition we must exchange for
				 * the rank rather than the parname.
				 */
				if (PQgetisnull(res, i, i_parname) || !strlen(PQgetvalue(res, i, i_parname)))
				{
					appendPQExpBuffer(q, "EXCHANGE PARTITION FOR (RANK(%s)) ",
									  PQgetvalue(res, i, i_partitionrank));
				}
				else
				{
					appendPQExpBuffer(q, "EXCHANGE PARTITION %s ",
									  fmtId(PQgetvalue(res, i, i_parname)));
				}
				appendPQExpBuffer(q, "WITH TABLE %s WITHOUT VALIDATION; ", qualTmpExtTable);

				appendPQExpBuffer(q, "\n");

				appendPQExpBuffer(q, "DROP TABLE %s; ", qualTmpExtTable);

				appendPQExpBuffer(q, "\n");
				free(relname);
				free(qualTmpExtTable);
			}

			PQclear(res);
			destroyPQExpBuffer(query);

			/* TODO: version-gate this when the planner bug is fixed; see above. */
			ExecuteSqlStatement(fout, "SET enable_nestloop TO on");
		}

		/*
		 * To create binary-compatible heap files, we have to ensure the same
		 * physical column order, including dropped columns, as in the
		 * original.  Therefore, we create dropped columns above and drop them
		 * here, also updating their attlen/attalign values so that the
		 * dropped column can be skipped properly.  (We do not bother with
		 * restoring the original attbyval setting.)  Also, inheritance
		 * relationships are set up by doing ALTER INHERIT rather than using
		 * an INHERITS clause --- the latter would possibly mess up the column
		 * order.  That also means we have to take care about setting
		 * attislocal correctly, plus fix up any inherited CHECK constraints.
		 * Analogously, we set up typed tables using ALTER TABLE / OF here.
		 *
		 * We process foreign tables here, even though they lack heap storage,
		 * because they can participate in inheritance relationships and we
		 * want this stuff to be consistent across the inheritance tree.  We
		 * exclude indexes, toast tables, sequences and matviews, even though
		 * they have storage, because we don't support altering or dropping
		 * columns in them, nor can they be part of inheritance trees.
		 *
		 * GPDB: We ignore dropped columns for partition table DDL. We assume
		 * here that all child partitions will NOT have dropped columns
		 * (manual user intervention should have been done after heterogeneous
		 * partition tables were flagged via pg_upgrade --check).
		 */
		if (binary_upgrade && (tbinfo->relkind == RELKIND_RELATION ||
							   tbinfo->relkind == RELKIND_FOREIGN_TABLE))
		{
			/*
			 * Greenplum doesn't allow altering system catalogs without
			 * setting the allow_system_table_mods GUC first.
			 */
			appendPQExpBuffer(q, "SET allow_system_table_mods = true;\n");

			for (j = 0; j < tbinfo->numatts; j++)
			{
				if (tbinfo->attisdropped[j] && !tbinfo->ignoreRootPartDroppedAttr)
				{
					appendPQExpBufferStr(q, "\n-- For binary upgrade, recreate dropped column.\n");
					appendPQExpBuffer(q, "UPDATE pg_catalog.pg_attribute\n"
									  "SET attlen = %d, "
									  "attalign = '%c', attbyval = false\n"
									  "WHERE attname = ",
									  tbinfo->attlen[j],
									  tbinfo->attalign[j]);
					appendStringLiteralAH(q, tbinfo->attnames[j], fout);

					/*
						* Do for all descendants of a partition table.
						* No hurt if this is not a table with partitions.
						*/
					appendPQExpBufferStr(q, "\n  AND attrelid IN (SELECT ");
					appendStringLiteralAH(q, qualrelname, fout);
					appendPQExpBufferStr(q, "::pg_catalog.regclass ");
					appendPQExpBufferStr(q, "UNION SELECT pr.parchildrelid FROM "
										"pg_catalog.pg_partition_rule pr, "
										"pg_catalog.pg_partition p WHERE "
										"pr.parchildrelid != 0 AND "
										"pr.paroid = p.oid AND p.parrelid = ");
					appendStringLiteralAH(q, qualrelname, fout);
					appendPQExpBufferStr(q, "::pg_catalog.regclass);\n");

					/*
					 * GPDB: Upstream uses ALTER TABLE ONLY below. Because we
					 * need to cascade the DROP down into partitions as well,
					 * we use ALTER TABLE instead.
					 *
					 * At the moment, we believe this does not cause problems
					 * for vanilla inherited tables, because the tables aren't
					 * plugged into the inheritance hierarchy until after this
					 * code is run (see the ALTER TABLE ... INHERIT below), and
					 * therefore ALTER TABLE and ALTER TABLE ONLY are
					 * effectively the same. If that changes, this will need to
					 * be revisited.
					 */
					if (tbinfo->relkind == RELKIND_RELATION)
						appendPQExpBuffer(q, "ALTER TABLE %s ",
										  qualrelname);
					else
						appendPQExpBuffer(q, "ALTER FOREIGN TABLE %s ",
										  qualrelname);

					appendPQExpBuffer(q, "DROP COLUMN %s;\n",
									  fmtId(tbinfo->attnames[j]));
				}
				else if (!tbinfo->attislocal[j])
				{
					Assert(tbinfo->relkind != RELKIND_FOREIGN_TABLE);
					appendPQExpBufferStr(q, "\n-- For binary upgrade, recreate inherited column.\n");
					appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_attribute\n"
										 "SET attislocal = false\n"
										 "WHERE attname = ");
					appendStringLiteralAH(q, tbinfo->attnames[j], fout);
					appendPQExpBufferStr(q, "\n  AND attrelid = ");
					appendStringLiteralAH(q, qualrelname, fout);
					appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");
				}
			}

			for (k = 0; k < tbinfo->ncheck; k++)
			{
				ConstraintInfo *constr = &(tbinfo->checkexprs[k]);

				if (constr->separate || constr->conislocal)
					continue;

				appendPQExpBufferStr(q, "\n-- For binary upgrade, set up inherited constraint.\n");
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  qualrelname);
				appendPQExpBuffer(q, " ADD CONSTRAINT %s ",
								  fmtId(constr->dobj.name));
				appendPQExpBuffer(q, "%s;\n", constr->condef);
				appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_constraint\n"
									 "SET conislocal = false\n"
									 "WHERE contype = 'c' AND conname = ");
				appendStringLiteralAH(q, constr->dobj.name, fout);
				appendPQExpBufferStr(q, "\n  AND conrelid = ");
				appendStringLiteralAH(q, qualrelname, fout);
				appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");
			}

			if (numParents > 0)
			{
				appendPQExpBufferStr(q, "\n-- For binary upgrade, set up inheritance this way.\n");
				for (k = 0; k < numParents; k++)
				{
					TableInfo  *parentRel = parents[k];

					appendPQExpBuffer(q, "ALTER TABLE ONLY %s INHERIT ",
									  qualrelname);
					appendPQExpBuffer(q, "%s;\n",
									  fmtQualifiedDumpable(parentRel));
				}
			}

			if (tbinfo->reloftype)
			{
				appendPQExpBufferStr(q, "\n-- For binary upgrade, set up typed tables this way.\n");
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s OF %s;\n",
								  qualrelname,
								  tbinfo->reloftype);
			}
			appendPQExpBuffer(q, "RESET allow_system_table_mods;\n");
		}

		/*
		 * In binary_upgrade mode, arrange to restore the old relfrozenxid and
		 * relminmxid of all vacuumable relations.  (While vacuum.c processes
		 * TOAST tables semi-independently, here we see them only as children
		 * of other relations; so this "if" lacks RELKIND_TOASTVALUE, and the
		 * child toast table is handled below.)
		 */
		if (binary_upgrade &&
			(tbinfo->relkind == RELKIND_RELATION ||
			 tbinfo->relkind == RELKIND_MATVIEW))
		{
			appendPQExpBuffer(q, "SET allow_system_table_mods = true;\n");

			appendPQExpBufferStr(q, "\n-- For binary upgrade, set heap's relfrozenxid and relminmxid\n");
			appendPQExpBuffer(q, "UPDATE pg_catalog.pg_class\n"
							  "SET relfrozenxid = '%u', relminmxid = '%u'\n"
							  "WHERE oid = ",
							  tbinfo->frozenxid, tbinfo->minmxid);
			appendStringLiteralAH(q, qualrelname, fout);
			appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");

			if (tbinfo->toast_oid)
			{
				/*
				 * The toast table will have the same OID at restore, so we
				 * can safely target it by OID.
				 */
				appendPQExpBufferStr(q, "\n-- For binary upgrade, set toast's relfrozenxid and relminmxid\n");
				appendPQExpBuffer(q, "UPDATE pg_catalog.pg_class\n"
								  "SET relfrozenxid = '%u', relminmxid = '%u'\n"
								  "WHERE oid = '%u';\n",
								  tbinfo->toast_frozenxid,
								  tbinfo->toast_minmxid, tbinfo->toast_oid);
			}

			/*
			 * We have probably bumped allow_system_table_mods to true in the
			 * above processing, but even we didn't let's just reset it here
			 * since it doesn't to do any harm to.
			 */
			appendPQExpBuffer(q, "RESET allow_system_table_mods;\n");
		}

		/*
		 * In binary_upgrade mode, restore matviews' populated status by
		 * poking pg_class directly.  This is pretty ugly, but we can't use
		 * REFRESH MATERIALIZED VIEW since it's possible that some underlying
		 * matview is not populated even though this matview is; in any case,
		 * we want to transfer the matview's heap storage, not run REFRESH.
		 */
		if (binary_upgrade && tbinfo->relkind == RELKIND_MATVIEW &&
			tbinfo->relispopulated)
		{
			appendPQExpBuffer(q, "SET allow_system_table_mods = true;\n");

			appendPQExpBufferStr(q, "\n-- For binary upgrade, mark materialized view as populated\n");
			appendPQExpBufferStr(q, "UPDATE pg_catalog.pg_class\n"
								 "SET relispopulated = 't'\n"
								 "WHERE oid = ");
			appendStringLiteralAH(q, qualrelname, fout);
			appendPQExpBufferStr(q, "::pg_catalog.regclass;\n");

			appendPQExpBuffer(q, "RESET allow_system_table_mods;\n");
		}

		/*
		 * Dump additional per-column properties that we can't handle in the
		 * main CREATE TABLE command.
		 */
		for (j = 0; j < tbinfo->numatts; j++)
		{
			/* None of this applies to dropped columns */
			if (tbinfo->attisdropped[j])
				continue;

			/*
			 * If we didn't dump the column definition explicitly above, and
			 * it is NOT NULL and did not inherit that property from a parent,
			 * we have to mark it separately.
			 */
			if (!shouldPrintColumn(tbinfo, j) &&
				tbinfo->notnull[j] && !tbinfo->inhNotNull[j])
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  qualrelname);
				appendPQExpBuffer(q, "ALTER COLUMN %s SET NOT NULL;\n",
								  fmtId(tbinfo->attnames[j]));
			}

			/*
			 * Dump per-column statistics information. We only issue an ALTER
			 * TABLE statement if the attstattarget entry for this column is
			 * non-negative (i.e. it's not the default value)
			 */
			if (tbinfo->attstattarget[j] >= 0)
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  qualrelname);
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
			if (tbinfo->attstorage[j] != tbinfo->typstorage[j])
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
									  qualrelname);
					appendPQExpBuffer(q, "ALTER COLUMN %s ",
									  fmtId(tbinfo->attnames[j]));
					appendPQExpBuffer(q, "SET STORAGE %s;\n",
									  storage);
				}
			}

			/*
			 * Dump per-column attributes.
			 */
			if (tbinfo->attoptions[j][0] != '\0')
			{
				appendPQExpBuffer(q, "ALTER TABLE ONLY %s ",
								  qualrelname);
				appendPQExpBuffer(q, "ALTER COLUMN %s ",
								  fmtId(tbinfo->attnames[j]));
				appendPQExpBuffer(q, "SET (%s);\n",
								  tbinfo->attoptions[j]);
			}

			/*
			 * Dump per-column fdw options.
			 */
			if (tbinfo->relkind == RELKIND_FOREIGN_TABLE &&
				tbinfo->attfdwoptions[j][0] != '\0')
			{
				appendPQExpBuffer(q, "ALTER FOREIGN TABLE %s ",
								  qualrelname);
				appendPQExpBuffer(q, "ALTER COLUMN %s ",
								  fmtId(tbinfo->attnames[j]));
				appendPQExpBuffer(q, "OPTIONS (\n    %s\n);\n",
								  tbinfo->attfdwoptions[j]);
			}
		}


		/* MPP-1890 */

		/*
		 * An inherited constraint may be dropped from a child table.  While
		 * this arguably severs the inheritance contract between the child and
		 * the parent, the current pg_constraint content doesn't track
		 * inherited/shared/disjoint constraints of a child.
		 * the INHERITS clause is used on a CREATE
		 * TABLE statement to re-establish the inheritance relationship and
		 * "recovers" the dropped constraint(s).
		 */
		if (numParents > 0)
			DetectChildConstraintDropped(tbinfo, q);
	}

	/*
	 * dump properties we only have ALTER TABLE syntax for
	 */
	if ((tbinfo->relkind == RELKIND_RELATION || tbinfo->relkind == RELKIND_MATVIEW) &&
		tbinfo->relreplident != REPLICA_IDENTITY_DEFAULT)
	{
		if (tbinfo->relreplident == REPLICA_IDENTITY_INDEX)
		{
			/* nothing to do, will be set when the index is dumped */
		}
		else if (tbinfo->relreplident == REPLICA_IDENTITY_NOTHING)
		{
			appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY NOTHING;\n",
							  qualrelname);
		}
		else if (tbinfo->relreplident == REPLICA_IDENTITY_FULL)
		{
			appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY FULL;\n",
							  qualrelname);
		}
	}

	if (binary_upgrade)
		binary_upgrade_extension_member(q, &tbinfo->dobj,
										reltypename, qrelname,
										tbinfo->dobj.namespace->dobj.name);

	/*
	 * GPDB_94_MERGE_FIXME: Why gpdb doesn't pass conditionally
	 * SECTION_PRE_DATA or SECTION_POST_DATA based on tbinfo->postponed_def
	 * similar to upstream.
	 */
	ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
			(tbinfo->relkind == RELKIND_VIEW) ? NULL : tbinfo->reltablespace,
				 tbinfo->rolname,
				 (strcmp(reltypename, "TABLE") == 0 ||
				  strcmp(reltypename, "EXTERNAL TABLE") == 0
					 ) ? tbinfo->hasoids : false,
				 reltypename, SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);


	/* Dump Table Comments */
	dumpTableComment(fout, tbinfo, reltypename);

	/* Dump Table Security Labels */
	dumpTableSecLabel(fout, tbinfo, reltypename);

	/* Dump comments on inlined table constraints */
	for (j = 0; j < tbinfo->ncheck; j++)
	{
		ConstraintInfo *constr = &(tbinfo->checkexprs[j]);

		if (constr->separate || !constr->conislocal)
			continue;

		dumpTableConstraintComment(fout, constr);
	}

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qrelname);
	free(qualrelname);
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
	char	   *qualrelname;

	/* Skip if table definition not to be dumped */
	if (!tbinfo->dobj.dump || dataOnly)
		return;

	/* Skip if not "separate"; it was dumped in the table's definition */
	if (!adinfo->separate)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qualrelname = pg_strdup(fmtQualifiedDumpable(tbinfo));

	/*
	 * If the table is the parent of a partitioning hierarchy, the default
	 * constraint must be applied to all children as well.
	 */
	appendPQExpBuffer(q, "ALTER TABLE %s %s ",
					  tbinfo->parparent ? "" : "ONLY",
					  qualrelname);
	appendPQExpBuffer(q, "ALTER COLUMN %s SET DEFAULT %s;\n",
					  fmtId(tbinfo->attnames[adnum - 1]),
					  adinfo->adef_expr);

	appendPQExpBuffer(delq, "ALTER TABLE %s ",
					  qualrelname);
	appendPQExpBuffer(delq, "ALTER COLUMN %s DROP DEFAULT;\n",
					  fmtId(tbinfo->attnames[adnum - 1]));

	ArchiveEntry(fout, adinfo->dobj.catId, adinfo->dobj.dumpId,
				 tbinfo->attnames[adnum - 1],
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname,
				 false, "DEFAULT", SECTION_PRE_DATA,
				 q->data, delq->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qualrelname);
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
	exit_horribly(NULL, "invalid column number %d for table \"%s\"\n",
				  attrnum, tblInfo->dobj.name);
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
	bool		is_constraint = (indxinfo->indexconstraint != 0);
	PQExpBuffer q;
	PQExpBuffer delq;
	char	   *qindxname;

	if (dataOnly)
		return;

	q = createPQExpBuffer();
	delq = createPQExpBuffer();

	qindxname = pg_strdup(fmtId(indxinfo->dobj.name));

	/*
	 * If there's an associated constraint, don't dump the index per se, but
	 * do dump any comment for it.  (This is safe because dependency ordering
	 * will have ensured the constraint is emitted first.)	Note that the
	 * emitted comment has to be shown as depending on the constraint, not the
	 * index, in such cases.
	 */
	if (!is_constraint)
	{
		if (binary_upgrade)
			binary_upgrade_set_pg_class_oids(fout, q, indxinfo->dobj.catId.oid, true);

		/* Plain secondary index */
		appendPQExpBuffer(q, "%s;\n", indxinfo->indexdef);

		/*
		 * Append ALTER TABLE commands as needed to set properties that we
		 * only have ALTER TABLE syntax for.  Keep this in sync with the
		 * similar code in dumpConstraint!
		 */

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtQualifiedDumpable(tbinfo));
			/* index name is not qualified in this syntax */
			appendPQExpBuffer(q, " ON %s;\n",
							  qindxname);
		}

		/* If the index defines identity, we need to record that. */
		if (indxinfo->indisreplident)
		{
			appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY USING",
							  fmtQualifiedDumpable(tbinfo));
			/* index name is not qualified in this syntax */
			appendPQExpBuffer(q, " INDEX %s;\n",
							  qindxname);
		}

		appendPQExpBuffer(delq, "DROP INDEX %s;\n",
						  fmtQualifiedDumpable(indxinfo));

		ArchiveEntry(fout, indxinfo->dobj.catId, indxinfo->dobj.dumpId,
					 indxinfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "INDEX", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 NULL, 0,
					 NULL, NULL);
	}

	/* Dump Index Comments */
	dumpComment(fout, "INDEX", qindxname,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				indxinfo->dobj.catId, 0,
				is_constraint ? indxinfo->indexconstraint :
				indxinfo->dobj.dumpId);

	destroyPQExpBuffer(q);
	destroyPQExpBuffer(delq);
	free(qindxname);
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

	if (coninfo->contype == 'p' ||
		coninfo->contype == 'u' ||
		coninfo->contype == 'x')
	{
		/* Index-related constraint */
		IndxInfo   *indxinfo;
		int			k;

		indxinfo = (IndxInfo *) findObjectByDumpId(coninfo->conindex);

		if (indxinfo == NULL)
			exit_horribly(NULL, "missing index for constraint \"%s\"\n",
						  coninfo->dobj.name);

		if (binary_upgrade)
			binary_upgrade_set_pg_class_oids(fout, q, indxinfo->dobj.catId.oid, true);

		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtQualifiedDumpable(tbinfo));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s ",
						  fmtId(coninfo->dobj.name));

		if (coninfo->condef)
		{
			/* pg_get_constraintdef should have provided everything */
			appendPQExpBuffer(q, "%s;\n", coninfo->condef);
		}
		else
		{
			appendPQExpBuffer(q, "%s (",
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

			appendPQExpBufferChar(q, ')');

			if (nonemptyReloptions(indxinfo->indreloptions))
			{
				appendPQExpBufferStr(q, " WITH (");
				fmtReloptionsArray(fout, q, indxinfo->indreloptions, "");
				appendPQExpBufferChar(q, ')');
			}

			if (coninfo->condeferrable)
			{
				appendPQExpBufferStr(q, " DEFERRABLE");
				if (coninfo->condeferred)
					appendPQExpBufferStr(q, " INITIALLY DEFERRED");
			}

			appendPQExpBufferStr(q, ";\n");
		}

		/*
		 * Append ALTER TABLE commands as needed to set properties that we
		 * only have ALTER TABLE syntax for.  Keep this in sync with the
		 * similar code in dumpIndex!
		 */

		/* If the index is clustered, we need to record that. */
		if (indxinfo->indisclustered)
		{
			appendPQExpBuffer(q, "\nALTER TABLE %s CLUSTER",
							  fmtQualifiedDumpable(tbinfo));
			/* index name is not qualified in this syntax */
			appendPQExpBuffer(q, " ON %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		/* If the index defines identity, we need to record that. */
		if (indxinfo->indisreplident)
		{
			appendPQExpBuffer(q, "\nALTER TABLE ONLY %s REPLICA IDENTITY USING",
							  fmtQualifiedDumpable(tbinfo));
			/* index name is not qualified in this syntax */
			appendPQExpBuffer(q, " INDEX %s;\n",
							  fmtId(indxinfo->dobj.name));
		}

		appendPQExpBuffer(delq, "ALTER TABLE ONLY %s ",
						  fmtQualifiedDumpable(tbinfo));
		appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 indxinfo->tablespace,
					 tbinfo->rolname, false,
					 "CONSTRAINT", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 NULL, 0,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'f')
	{
		/*
		 * XXX Potentially wrap in a 'SET CONSTRAINTS OFF' block so that the
		 * current table data is not processed
		 */
		appendPQExpBuffer(q, "ALTER TABLE ONLY %s\n",
						  fmtQualifiedDumpable(tbinfo));
		appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
						  fmtId(coninfo->dobj.name),
						  coninfo->condef);

		appendPQExpBuffer(delq, "ALTER TABLE ONLY %s ",
						  fmtQualifiedDumpable(tbinfo));
		appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
						  fmtId(coninfo->dobj.name));

		ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
					 coninfo->dobj.name,
					 tbinfo->dobj.namespace->dobj.name,
					 NULL,
					 tbinfo->rolname, false,
					 "FK CONSTRAINT", SECTION_POST_DATA,
					 q->data, delq->data, NULL,
					 NULL, 0,
					 NULL, NULL);
	}
	else if (coninfo->contype == 'c' && tbinfo)
	{
		/* CHECK constraint on a table */

		/* Ignore if not to be dumped separately, or if it was inherited */
		if (coninfo->separate && coninfo->conislocal)
		{
			/* not ONLY since we want it to propagate to children */
			appendPQExpBuffer(q, "ALTER TABLE %s\n",
							  fmtQualifiedDumpable(tbinfo));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			appendPQExpBuffer(delq, "ALTER TABLE %s ",
							  fmtQualifiedDumpable(tbinfo));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname, false,
						 "CHECK CONSTRAINT", SECTION_POST_DATA,
						 q->data, delq->data, NULL,
						 NULL, 0,
						 NULL, NULL);
		}
	}
	else if (coninfo->contype == 'c' && tbinfo == NULL)
	{
		/* CHECK constraint on a domain */
		TypeInfo   *tyinfo = coninfo->condomain;

		/* Ignore if not to be dumped separately */
		if (coninfo->separate)
		{
			appendPQExpBuffer(q, "ALTER DOMAIN %s\n",
							  fmtQualifiedDumpable(tyinfo));
			appendPQExpBuffer(q, "    ADD CONSTRAINT %s %s;\n",
							  fmtId(coninfo->dobj.name),
							  coninfo->condef);

			appendPQExpBuffer(delq, "ALTER DOMAIN %s ",
							  fmtQualifiedDumpable(tyinfo));
			appendPQExpBuffer(delq, "DROP CONSTRAINT %s;\n",
							  fmtId(coninfo->dobj.name));

			ArchiveEntry(fout, coninfo->dobj.catId, coninfo->dobj.dumpId,
						 coninfo->dobj.name,
						 tyinfo->dobj.namespace->dobj.name,
						 NULL,
						 tyinfo->rolname, false,
						 "CHECK CONSTRAINT", SECTION_POST_DATA,
						 q->data, delq->data, NULL,
						 NULL, 0,
						 NULL, NULL);
		}
	}
	else
	{
		exit_horribly(NULL, "unrecognized constraint type: %c\n",
					  coninfo->contype);
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
	PQExpBuffer conprefix = createPQExpBuffer();
	char	   *qtabname;

	qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

	appendPQExpBuffer(conprefix, "CONSTRAINT %s ON",
					  fmtId(coninfo->dobj.name));

	dumpComment(fout, conprefix->data, qtabname,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				coninfo->dobj.catId, 0,
			 coninfo->separate ? coninfo->dobj.dumpId : tbinfo->dobj.dumpId);

	destroyPQExpBuffer(conprefix);
	free(qtabname);
}

/*
 * dumpSequence
 *	  write the declaration (not data) of one user-defined sequence
 */
static void
dumpSequence(Archive *fout, TableInfo *tbinfo)
{
	PGresult   *res;
	char	   *startv,
			   *incby,
			   *maxv = NULL,
			   *minv = NULL,
			   *cache;
	char		bufm[100],
				bufx[100];
	bool		cycled;
	PQExpBuffer query = createPQExpBuffer();
	PQExpBuffer delqry = createPQExpBuffer();
	char	   *qseqname;

	qseqname = pg_strdup(fmtId(tbinfo->dobj.name));

	snprintf(bufm, sizeof(bufm), INT64_FORMAT, SEQ_MINVALUE);
	snprintf(bufx, sizeof(bufx), INT64_FORMAT, SEQ_MAXVALUE);

	if (fout->remoteVersion >= 80400)
	{
		appendPQExpBuffer(query,
						  "SELECT sequence_name, "
						  "start_value, increment_by, "
				   "CASE WHEN increment_by > 0 AND max_value = %s THEN NULL "
				   "     WHEN increment_by < 0 AND max_value = -1 THEN NULL "
						  "     ELSE max_value "
						  "END AS max_value, "
					"CASE WHEN increment_by > 0 AND min_value = 1 THEN NULL "
				   "     WHEN increment_by < 0 AND min_value = %s THEN NULL "
						  "     ELSE min_value "
						  "END AS min_value, "
						  "cache_value, is_cycled FROM %s",
						  bufx, bufm,
						  fmtQualifiedDumpable(tbinfo));
	}
	else
	{
		appendPQExpBuffer(query,
						  "SELECT sequence_name, "
						  "0 AS start_value, increment_by, "
				   "CASE WHEN increment_by > 0 AND max_value = %s THEN NULL "
				   "     WHEN increment_by < 0 AND max_value = -1 THEN NULL "
						  "     ELSE max_value "
						  "END AS max_value, "
					"CASE WHEN increment_by > 0 AND min_value = 1 THEN NULL "
				   "     WHEN increment_by < 0 AND min_value = %s THEN NULL "
						  "     ELSE min_value "
						  "END AS min_value, "
						  "cache_value, is_cycled FROM %s",
						  bufx, bufm,
						  fmtQualifiedDumpable(tbinfo));
	}

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		write_msg(NULL, ngettext("query to get data of sequence \"%s\" returned %d row (expected 1)\n",
								 "query to get data of sequence \"%s\" returned %d rows (expected 1)\n",
								 PQntuples(res)),
				  tbinfo->dobj.name, PQntuples(res));
		exit_nicely(1);
	}

	/* Disable this check: it fails if sequence has been renamed */
#ifdef NOT_USED
	if (strcmp(PQgetvalue(res, 0, 0), tbinfo->dobj.name) != 0)
	{
		write_msg(NULL, "query to get data of sequence \"%s\" returned name \"%s\"\n",
				  tbinfo->dobj.name, PQgetvalue(res, 0, 0));
		exit_nicely(1);
	}
#endif

	startv = PQgetvalue(res, 0, 1);
	incby = PQgetvalue(res, 0, 2);
	if (!PQgetisnull(res, 0, 3))
		maxv = PQgetvalue(res, 0, 3);
	if (!PQgetisnull(res, 0, 4))
		minv = PQgetvalue(res, 0, 4);
	cache = PQgetvalue(res, 0, 5);
	cycled = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);

	appendPQExpBuffer(delqry, "DROP SEQUENCE %s;\n",
					  fmtQualifiedDumpable(tbinfo));

	resetPQExpBuffer(query);

	if (binary_upgrade)
	{
		binary_upgrade_set_pg_class_oids(fout, query,
										 tbinfo->dobj.catId.oid, false);
		binary_upgrade_set_type_oids_by_rel(fout, query,
												tbinfo);
	}

	appendPQExpBuffer(query,
					  "CREATE SEQUENCE %s\n",
					  fmtQualifiedDumpable(tbinfo));

	if (fout->remoteVersion >= 80400)
		appendPQExpBuffer(query, "    START WITH %s\n", startv);

	appendPQExpBuffer(query, "    INCREMENT BY %s\n", incby);

	if (minv)
		appendPQExpBuffer(query, "    MINVALUE %s\n", minv);
	else
		appendPQExpBufferStr(query, "    NO MINVALUE\n");

	if (maxv)
		appendPQExpBuffer(query, "    MAXVALUE %s\n", maxv);
	else
		appendPQExpBufferStr(query, "    NO MAXVALUE\n");

	appendPQExpBuffer(query,
					  "    CACHE %s%s",
					  cache, (cycled ? "\n    CYCLE" : ""));

	appendPQExpBufferStr(query, ";\n");

	/* binary_upgrade:	no need to clear TOAST table oid */

	if (binary_upgrade)
		binary_upgrade_extension_member(query, &tbinfo->dobj,
										"SEQUENCE", qseqname,
										tbinfo->dobj.namespace->dobj.name);

	ArchiveEntry(fout, tbinfo->dobj.catId, tbinfo->dobj.dumpId,
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname,
				 false, "SEQUENCE", SECTION_PRE_DATA,
				 query->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/*
	 * If the sequence is owned by a table column, emit the ALTER for it as a
	 * separate TOC entry immediately following the sequence's own entry. It's
	 * OK to do this rather than using full sorting logic, because the
	 * dependency that tells us it's owned will have forced the table to be
	 * created first.  We can't just include the ALTER in the TOC entry
	 * because it will fail if we haven't reassigned the sequence owner to
	 * match the table's owner.
	 *
	 * We need not schema-qualify the table reference because both sequence
	 * and table must be in the same schema.
	 */
	if (OidIsValid(tbinfo->owning_tab))
	{
		TableInfo  *owning_tab = findTableByOid(tbinfo->owning_tab);

		if (owning_tab && owning_tab->dobj.dump)
		{
			resetPQExpBuffer(query);
			appendPQExpBuffer(query, "ALTER SEQUENCE %s",
							  fmtQualifiedDumpable(tbinfo));
			appendPQExpBuffer(query, " OWNED BY %s",
							  fmtQualifiedDumpable(owning_tab));
			appendPQExpBuffer(query, ".%s;\n",
						fmtId(owning_tab->attnames[tbinfo->owning_col - 1]));

			ArchiveEntry(fout, nilCatalogId, createDumpId(),
						 tbinfo->dobj.name,
						 tbinfo->dobj.namespace->dobj.name,
						 NULL,
						 tbinfo->rolname,
						 false, "SEQUENCE OWNED BY", SECTION_PRE_DATA,
						 query->data, "", NULL,
						 &(tbinfo->dobj.dumpId), 1,
						 NULL, NULL);
		}
	}

	/* Dump Sequence Comments and Security Labels */
	dumpComment(fout, "SEQUENCE", qseqname,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);
	dumpSecLabel(fout, "SEQUENCE", qseqname,
				 tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				 tbinfo->dobj.catId, 0, tbinfo->dobj.dumpId);

	PQclear(res);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
	free(qseqname);
}

/*
 * dumpSequenceData
 *	  write the data of one user-defined sequence
 */
static void
dumpSequenceData(Archive *fout, TableDataInfo *tdinfo)
{
	TableInfo  *tbinfo = tdinfo->tdtable;
	PGresult   *res;
	char	   *last;
	bool		called;
	PQExpBuffer query = createPQExpBuffer();

	appendPQExpBuffer(query,
					  "SELECT last_value, is_called FROM %s",
					  fmtQualifiedDumpable(tbinfo));

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	if (PQntuples(res) != 1)
	{
		write_msg(NULL, ngettext("query to get data of sequence \"%s\" returned %d row (expected 1)\n",
								 "query to get data of sequence \"%s\" returned %d rows (expected 1)\n",
								 PQntuples(res)),
				  tbinfo->dobj.name, PQntuples(res));
		exit_nicely(1);
	}

	last = PQgetvalue(res, 0, 0);
	called = (strcmp(PQgetvalue(res, 0, 1), "t") == 0);

	resetPQExpBuffer(query);
	appendPQExpBufferStr(query, "SELECT pg_catalog.setval(");
	appendStringLiteralAH(query, fmtQualifiedDumpable(tbinfo), fout);
	appendPQExpBuffer(query, ", %s, %s);\n",
					  last, (called ? "true" : "false"));

	ArchiveEntry(fout, nilCatalogId, createDumpId(),
				 tbinfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname,
				 false, "SEQUENCE SET", SECTION_DATA,
				 query->data, "", NULL,
				 &(tbinfo->dobj.dumpId), 1,
				 NULL, NULL);

	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * dumpTrigger
 *	  write the declaration of one user-defined table trigger
 */
static void
dumpTrigger(Archive *fout, TriggerInfo *tginfo)
{
	TableInfo  *tbinfo = tginfo->tgtable;
	PQExpBuffer query;
	PQExpBuffer delqry;
	PQExpBuffer trigprefix;
	char	   *qtabname;
	char	   *tgargs;
	size_t		lentgargs;
	const char *p;
	int			findx;

	/*
	 * we needn't check dobj.dump because TriggerInfo wouldn't have been
	 * created in the first place for non-dumpable triggers
	 */
	if (dataOnly)
		return;

	query = createPQExpBuffer();
	delqry = createPQExpBuffer();
	trigprefix = createPQExpBuffer();

	qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

	appendPQExpBuffer(delqry, "DROP TRIGGER %s ",
					  fmtId(tginfo->dobj.name));
	appendPQExpBuffer(delqry, "ON %s;\n",
					  fmtQualifiedDumpable(tbinfo));

	if (tginfo->tgdef)
	{
		appendPQExpBuffer(query, "%s;\n", tginfo->tgdef);
	}
	else
	{
		if (tginfo->tgisconstraint)
		{
			appendPQExpBufferStr(query, "CREATE CONSTRAINT TRIGGER ");
			appendPQExpBufferStr(query, fmtId(tginfo->tgconstrname));
		}
		else
		{
			appendPQExpBufferStr(query, "CREATE TRIGGER ");
			appendPQExpBufferStr(query, fmtId(tginfo->dobj.name));
		}
		appendPQExpBufferStr(query, "\n    ");

		/* Trigger type */
		if (TRIGGER_FOR_BEFORE(tginfo->tgtype))
			appendPQExpBufferStr(query, "BEFORE");
		else if (TRIGGER_FOR_AFTER(tginfo->tgtype))
			appendPQExpBufferStr(query, "AFTER");
		else if (TRIGGER_FOR_INSTEAD(tginfo->tgtype))
			appendPQExpBufferStr(query, "INSTEAD OF");
		else
		{
			write_msg(NULL, "unexpected tgtype value: %d\n", tginfo->tgtype);
			exit_nicely(1);
		}

		findx = 0;
		if (TRIGGER_FOR_INSERT(tginfo->tgtype))
		{
			appendPQExpBufferStr(query, " INSERT");
			findx++;
		}
		if (TRIGGER_FOR_DELETE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBufferStr(query, " OR DELETE");
			else
				appendPQExpBufferStr(query, " DELETE");
			findx++;
		}
		if (TRIGGER_FOR_UPDATE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBufferStr(query, " OR UPDATE");
			else
				appendPQExpBufferStr(query, " UPDATE");
			findx++;
		}
		if (TRIGGER_FOR_TRUNCATE(tginfo->tgtype))
		{
			if (findx > 0)
				appendPQExpBufferStr(query, " OR TRUNCATE");
			else
				appendPQExpBufferStr(query, " TRUNCATE");
			findx++;
		}
		appendPQExpBuffer(query, " ON %s\n",
						  fmtQualifiedDumpable(tbinfo));

		if (tginfo->tgisconstraint)
		{
			if (OidIsValid(tginfo->tgconstrrelid))
			{
				appendPQExpBuffer(query, "    FROM %s\n    ",
									tginfo->tgconstrrelname);
			}
			if (!tginfo->tgdeferrable)
				appendPQExpBufferStr(query, "NOT ");
			appendPQExpBufferStr(query, "DEFERRABLE INITIALLY ");
			if (tginfo->tginitdeferred)
				appendPQExpBufferStr(query, "DEFERRED\n");
			else
				appendPQExpBufferStr(query, "IMMEDIATE\n");
		}

		if (TRIGGER_FOR_ROW(tginfo->tgtype))
			appendPQExpBufferStr(query, "    FOR EACH ROW\n    ");
		else
			appendPQExpBufferStr(query, "    FOR EACH STATEMENT\n    ");

		appendPQExpBuffer(query, "EXECUTE PROCEDURE %s(",
							tginfo->tgfname);

		tgargs = (char *) PQunescapeBytea((unsigned char *) tginfo->tgargs,
										  &lentgargs);
		p = tgargs;
		for (findx = 0; findx < tginfo->tgnargs; findx++)
		{
			/* find the embedded null that terminates this trigger argument */
			size_t		tlen = strlen(p);

			if (p + tlen >= tgargs + lentgargs)
			{
				/* hm, not found before end of bytea value... */
				write_msg(NULL, "invalid argument string (%s) for trigger \"%s\" on table \"%s\"\n",
						  tginfo->tgargs,
						  tginfo->dobj.name,
						  tbinfo->dobj.name);
				exit_nicely(1);
			}

			if (findx > 0)
				appendPQExpBufferStr(query, ", ");
			appendStringLiteralAH(query, p, fout);
			p += tlen + 1;
		}
		free(tgargs);
		appendPQExpBufferStr(query, ");\n");
	}

	if (tginfo->tgenabled != 't' && tginfo->tgenabled != 'O')
	{
		appendPQExpBuffer(query, "\nALTER TABLE %s ",
						  fmtQualifiedDumpable(tbinfo));
		switch (tginfo->tgenabled)
		{
			case 'D':
			case 'f':
				appendPQExpBufferStr(query, "DISABLE");
				break;
			case 'A':
				appendPQExpBufferStr(query, "ENABLE ALWAYS");
				break;
			case 'R':
				appendPQExpBufferStr(query, "ENABLE REPLICA");
				break;
			default:
				appendPQExpBufferStr(query, "ENABLE");
				break;
		}
		appendPQExpBuffer(query, " TRIGGER %s;\n",
						  fmtId(tginfo->dobj.name));
	}

	appendPQExpBuffer(trigprefix, "TRIGGER %s ON",
					  fmtId(tginfo->dobj.name));

	ArchiveEntry(fout, tginfo->dobj.catId, tginfo->dobj.dumpId,
				 tginfo->dobj.name,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "TRIGGER", SECTION_POST_DATA,
				 query->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	dumpComment(fout, trigprefix->data, qtabname,
				tbinfo->dobj.namespace->dobj.name, tbinfo->rolname,
				tginfo->dobj.catId, 0, tginfo->dobj.dumpId);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
	destroyPQExpBuffer(trigprefix);
	free(qtabname);
}

/*
 * dumpEventTrigger
 *	  write the declaration of one user-defined event trigger
 */
static void
dumpEventTrigger(Archive *fout, EventTriggerInfo *evtinfo)
{
	PQExpBuffer query;
	PQExpBuffer delqry;
	char	   *qevtname;

	/* Skip if not to be dumped */
	if (!evtinfo->dobj.dump || dataOnly)
		return;

	query = createPQExpBuffer();
	delqry = createPQExpBuffer();

	qevtname = pg_strdup(fmtId(evtinfo->dobj.name));

	appendPQExpBufferStr(query, "CREATE EVENT TRIGGER ");
	appendPQExpBufferStr(query, qevtname);
	appendPQExpBufferStr(query, " ON ");
	appendPQExpBufferStr(query, fmtId(evtinfo->evtevent));

	if (strcmp("", evtinfo->evttags) != 0)
	{
		appendPQExpBufferStr(query, "\n         WHEN TAG IN (");
		appendPQExpBufferStr(query, evtinfo->evttags);
		appendPQExpBufferChar(query, ')');
	}

	appendPQExpBufferStr(query, "\n   EXECUTE PROCEDURE ");
	appendPQExpBufferStr(query, evtinfo->evtfname);
	appendPQExpBufferStr(query, "();\n");

	if (evtinfo->evtenabled != 'O')
	{
		appendPQExpBuffer(query, "\nALTER EVENT TRIGGER %s ",
						  qevtname);
		switch (evtinfo->evtenabled)
		{
			case 'D':
				appendPQExpBufferStr(query, "DISABLE");
				break;
			case 'A':
				appendPQExpBufferStr(query, "ENABLE ALWAYS");
				break;
			case 'R':
				appendPQExpBufferStr(query, "ENABLE REPLICA");
				break;
			default:
				appendPQExpBufferStr(query, "ENABLE");
				break;
		}
		appendPQExpBufferStr(query, ";\n");
	}

	appendPQExpBuffer(delqry, "DROP EVENT TRIGGER %s;\n",
					  qevtname);

	if (binary_upgrade)
		binary_upgrade_extension_member(query, &evtinfo->dobj,
										"EVENT TRIGGER", qevtname, NULL);

	ArchiveEntry(fout, evtinfo->dobj.catId, evtinfo->dobj.dumpId,
				 evtinfo->dobj.name, NULL, NULL,
				 evtinfo->evtowner, false,
				 "EVENT TRIGGER", SECTION_POST_DATA,
				 query->data, delqry->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	dumpComment(fout, "EVENT TRIGGER", qevtname,
				NULL, evtinfo->evtowner,
				evtinfo->dobj.catId, 0, evtinfo->dobj.dumpId);

	destroyPQExpBuffer(query);
	destroyPQExpBuffer(delqry);
	free(qevtname);
}

/*
 * dumpRule
 *		Dump a rule
 */
static void
dumpRule(Archive *fout, RuleInfo *rinfo)
{
	TableInfo  *tbinfo = rinfo->ruletable;
	bool		is_view;
	PQExpBuffer query;
	PQExpBuffer cmd;
	PQExpBuffer delcmd;
	PQExpBuffer ruleprefix;
	char	   *qtabname;
	PGresult   *res;
	char	   *tag;

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
	 * If it's an ON SELECT rule, we want to print it as a view definition,
	 * instead of a rule.
	 */
	is_view = (rinfo->ev_type == '1' && rinfo->is_instead);

	query = createPQExpBuffer();
	cmd = createPQExpBuffer();
	delcmd = createPQExpBuffer();
	ruleprefix = createPQExpBuffer();

	qtabname = pg_strdup(fmtId(tbinfo->dobj.name));

	if (is_view)
	{
		PQExpBuffer result;

		/*
		 * We need OR REPLACE here because we'll be replacing a dummy view.
		 * Otherwise this should look largely like the regular view dump code.
		 */
		appendPQExpBuffer(cmd, "CREATE OR REPLACE VIEW %s",
						  fmtQualifiedDumpable(tbinfo));
		if (nonemptyReloptions(tbinfo->reloptions))
		{
			appendPQExpBufferStr(cmd, " WITH (");
			fmtReloptionsArray(fout, cmd, tbinfo->reloptions, "");
			appendPQExpBufferChar(cmd, ')');
		}
		result = createViewAsClause(fout, tbinfo);
		appendPQExpBuffer(cmd, " AS\n%s", result->data);
		destroyPQExpBuffer(result);
		if (tbinfo->checkoption != NULL)
			appendPQExpBuffer(cmd, "\n  WITH %s CHECK OPTION",
							  tbinfo->checkoption);
		appendPQExpBufferStr(cmd, ";\n");
	}
	else
	{
		appendPQExpBuffer(query,
							"SELECT pg_catalog.pg_get_ruledef('%u'::pg_catalog.oid) AS definition",
							rinfo->dobj.catId.oid);

		res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

		if (PQntuples(res) != 1)
		{
			write_msg(NULL, "query to get rule \"%s\" for table \"%s\" failed: wrong number of rows returned\n",
					  rinfo->dobj.name, tbinfo->dobj.name);
			exit_nicely(1);
		}

		printfPQExpBuffer(cmd, "%s\n", PQgetvalue(res, 0, 0));

		PQclear(res);
	}

	/*
	 * Add the command to alter the rules replication firing semantics if it
	 * differs from the default.
	 */
	if (rinfo->ev_enabled != 'O')
	{
		appendPQExpBuffer(cmd, "ALTER TABLE %s ", fmtQualifiedDumpable(tbinfo));
		switch (rinfo->ev_enabled)
		{
			case 'A':
				appendPQExpBuffer(cmd, "ENABLE ALWAYS RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
			case 'R':
				appendPQExpBuffer(cmd, "ENABLE REPLICA RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
			case 'D':
				appendPQExpBuffer(cmd, "DISABLE RULE %s;\n",
								  fmtId(rinfo->dobj.name));
				break;
		}
	}

	if (is_view)
	{
		/*
		 * We can't DROP a view's ON SELECT rule.  Instead, use CREATE OR
		 * REPLACE VIEW to replace the rule with something with minimal
		 * dependencies.
		 */
		PQExpBuffer result;

		appendPQExpBuffer(delcmd, "CREATE OR REPLACE VIEW %s",
						  fmtQualifiedDumpable(tbinfo));
		result = createDummyViewAsClause(fout, tbinfo);
		appendPQExpBuffer(delcmd, " AS\n%s;\n", result->data);
		destroyPQExpBuffer(result);
	}
	else
	{
		appendPQExpBuffer(delcmd, "DROP RULE %s ",
						  fmtId(rinfo->dobj.name));
		appendPQExpBuffer(delcmd, "ON %s;\n",
						  fmtQualifiedDumpable(tbinfo));
	}

	appendPQExpBuffer(ruleprefix, "RULE %s ON",
					  fmtId(rinfo->dobj.name));

	tag = psprintf("%s %s", tbinfo->dobj.name, rinfo->dobj.name);

	ArchiveEntry(fout, rinfo->dobj.catId, rinfo->dobj.dumpId,
				 tag,
				 tbinfo->dobj.namespace->dobj.name,
				 NULL,
				 tbinfo->rolname, false,
				 "RULE", SECTION_POST_DATA,
				 cmd->data, delcmd->data, NULL,
				 NULL, 0,
				 NULL, NULL);

	/* Dump rule comments */
	dumpComment(fout, ruleprefix->data, qtabname,
				tbinfo->dobj.namespace->dobj.name,
				tbinfo->rolname,
				rinfo->dobj.catId, 0, rinfo->dobj.dumpId);

	free(tag);
	destroyPQExpBuffer(query);
	destroyPQExpBuffer(cmd);
	destroyPQExpBuffer(delcmd);
	destroyPQExpBuffer(ruleprefix);
	free(qtabname);
}

/*
 * getExtensionMembership --- obtain extension membership data
 *
 * We need to identify objects that are extension members as soon as they're
 * loaded, so that we can correctly determine whether they need to be dumped.
 * Generally speaking, extension member objects will get marked as *not* to
 * be dumped, as they will be recreated by the single CREATE EXTENSION
 * command.  However, in binary upgrade mode we still need to dump the members
 * individually.
 */
void
getExtensionMembership(Archive *fout, ExtensionInfo extinfo[],
					   int numExtensions)
{
	PQExpBuffer query;
	PGresult   *res;
	int			ntups,
				i;
	int			i_classid,
				i_objid,
				i_refobjid;
	ExtensionInfo *ext;

	/* Nothing to do if no extensions */
	if (numExtensions == 0)
		return;

	query = createPQExpBuffer();

	/* refclassid constraint is redundant but may speed the search */
	appendPQExpBufferStr(query, "SELECT "
						 "classid, objid, refobjid "
						 "FROM pg_depend "
						 "WHERE refclassid = 'pg_extension'::regclass "
						 "AND deptype = 'e' "
						 "ORDER BY 3");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

	ntups = PQntuples(res);

	i_classid = PQfnumber(res, "classid");
	i_objid = PQfnumber(res, "objid");
	i_refobjid = PQfnumber(res, "refobjid");

	/*
	 * Since we ordered the SELECT by referenced ID, we can expect that
	 * multiple entries for the same extension will appear together; this
	 * saves on searches.
	 */
	ext = NULL;

	for (i = 0; i < ntups; i++)
	{
		CatalogId	objId;
		Oid			extId;

		objId.tableoid = atooid(PQgetvalue(res, i, i_classid));
		objId.oid = atooid(PQgetvalue(res, i, i_objid));
		extId = atooid(PQgetvalue(res, i, i_refobjid));

		if (ext == NULL ||
			ext->dobj.catId.oid != extId)
			ext = findExtensionByOid(extId);

		if (ext == NULL)
		{
			/* shouldn't happen */
			fprintf(stderr, "could not find referenced extension %u\n", extId);
			continue;
		}

		recordExtensionMembership(objId, ext);
	}

	PQclear(res);

	destroyPQExpBuffer(query);
}

/*
 * processExtensionTables --- deal with extension configuration tables
 *
 * There are two parts to this process:
 *
 * 1. Identify and create dump records for extension configuration tables.
 *
 *	  Extensions can mark tables as "configuration", which means that the user
 *	  is able and expected to modify those tables after the extension has been
 *	  loaded.  For these tables, we dump out only the data- the structure is
 *	  expected to be handled at CREATE EXTENSION time, including any indexes or
 *	  foreign keys, which brings us to-
 *
 * 2. Record FK dependencies between configuration tables.
 *
 *	  Due to the FKs being created at CREATE EXTENSION time and therefore before
 *	  the data is loaded, we have to work out what the best order for reloading
 *	  the data is, to avoid FK violations when the tables are restored.  This is
 *	  not perfect- we can't handle circular dependencies and if any exist they
 *	  will cause an invalid dump to be produced (though at least all of the data
 *	  is included for a user to manually restore).  This is currently documented
 *	  but perhaps we can provide a better solution in the future.
 */
void
processExtensionTables(Archive *fout, ExtensionInfo extinfo[],
					   int numExtensions)
{
	PQExpBuffer query;
	PGresult   *res;
	int			ntups,
				i;
	int			i_conrelid,
				i_confrelid;

	/* Nothing to do if no extensions */
	if (numExtensions == 0)
		return;

	/*
	 * Identify extension configuration tables and create TableDataInfo
	 * objects for them, ensuring their data will be dumped even though the
	 * tables themselves won't be.
	 *
	 * Note that we create TableDataInfo objects even in schemaOnly mode, ie,
	 * user data in a configuration table is treated like schema data. This
	 * seems appropriate since system data in a config table would get
	 * reloaded by CREATE EXTENSION.
	 */
	for (i = 0; i < numExtensions; i++)
	{
		ExtensionInfo *curext = &(extinfo[i]);
		char	   *extconfig = curext->extconfig;
		char	   *extcondition = curext->extcondition;
		char	  **extconfigarray = NULL;
		char	  **extconditionarray = NULL;
		int			nconfigitems;
		int			nconditionitems;

		if (parsePGArray(extconfig, &extconfigarray, &nconfigitems) &&
			parsePGArray(extcondition, &extconditionarray, &nconditionitems) &&
			nconfigitems == nconditionitems)
		{
			int			j;

			for (j = 0; j < nconfigitems; j++)
			{
				TableInfo  *configtbl;
				Oid			configtbloid = atooid(extconfigarray[j]);
				bool		dumpobj = curext->dobj.dump;

				configtbl = findTableByOid(configtbloid);
				if (configtbl == NULL)
					continue;
				/*
				 * Tables of not-to-be-dumped extensions shouldn't be dumped
				 * unless the table or its schema is explicitly included
				 */
				if (!curext->dobj.dump)
				{
					/* check table explicitly requested */
					if (table_include_oids.head != NULL &&
						simple_oid_list_member(&table_include_oids,
											   configtbloid))
						dumpobj = true;

					/* check table's schema explicitly requested */
					if (configtbl->dobj.namespace->dobj.dump)
						dumpobj = true;
				}

				/* check table excluded by an exclusion switch */
				if (table_exclude_oids.head != NULL &&
					simple_oid_list_member(&table_exclude_oids,
										   configtbloid))
					dumpobj = false;

				/* check schema excluded by an exclusion switch */
				if (simple_oid_list_member(&schema_exclude_oids,
								  configtbl->dobj.namespace->dobj.catId.oid))
					dumpobj = false;

				if (dumpobj)
				{
					/*
					 * Note: config tables are dumped without OIDs regardless
					 * of the --oids setting.  This is because row filtering
					 * conditions aren't compatible with dumping OIDs.
					 */
					makeTableDataInfo(configtbl, false);
					if (configtbl->dataObj != NULL)
					{
						if (strlen(extconditionarray[j]) > 0)
							configtbl->dataObj->filtercond = pg_strdup(extconditionarray[j]);
					}
				}
			}
		}
		if (extconfigarray)
			free(extconfigarray);
		if (extconditionarray)
			free(extconditionarray);
	}

	/*
	 * Now that all the TableInfoData objects have been created for all the
	 * extensions, check their FK dependencies and register them to try and
	 * dump the data out in an order that they can be restored in.
	 *
	 * Note that this is not a problem for user tables as their FKs are
	 * recreated after the data has been loaded.
	 */

	query = createPQExpBuffer();

	printfPQExpBuffer(query,
			"SELECT conrelid, confrelid "
			"FROM pg_constraint "
				"JOIN pg_depend ON (objid = confrelid) "
			"WHERE contype = 'f' "
			"AND refclassid = 'pg_extension'::regclass "
			"AND classid = 'pg_class'::regclass;");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);
	ntups = PQntuples(res);

	i_conrelid = PQfnumber(res, "conrelid");
	i_confrelid = PQfnumber(res, "confrelid");

	/* Now get the dependencies and register them */
	for (i = 0; i < ntups; i++)
	{
		Oid			conrelid, confrelid;
		TableInfo  *reftable, *contable;

		conrelid = atooid(PQgetvalue(res, i, i_conrelid));
		confrelid = atooid(PQgetvalue(res, i, i_confrelid));
		contable = findTableByOid(conrelid);
		reftable = findTableByOid(confrelid);

		if (reftable == NULL ||
			reftable->dataObj == NULL ||
			contable == NULL ||
			contable->dataObj == NULL)
			continue;

		/*
		 * Make referencing TABLE_DATA object depend on the
		 * referenced table's TABLE_DATA object.
		 */
		addObjectDependency(&contable->dataObj->dobj,
							reftable->dataObj->dobj.dumpId);
	}
	PQclear(res);
	destroyPQExpBuffer(query);
}

/*
 * setExtPartDependency -
 */
static void
setExtPartDependency(TableInfo *tblinfo, int numTables)
{
	int			i;

	for (i = 0; i < numTables; i++)
	{
		TableInfo  *tbinfo = &(tblinfo[i]);
		TableInfo  *parent;
		Oid parrelid = tbinfo->parrelid;

		if (parrelid == 0)
			continue;

		parent = findTableByOid(parrelid);
		if (!parent)
		{
			write_msg(NULL, "parent table (OID %u) of partition \"%s\" (OID %u) not found\n",
					  parrelid, tbinfo->dobj.name, tbinfo->dobj.catId.oid);
			exit_nicely(1);
		}

		addObjectDependency(&parent->dobj, tbinfo->dobj.dumpId);
		removeObjectDependency(&tbinfo->dobj, parent->dobj.dumpId);
	}
}

/*
 * getDependencies --- obtain available dependency data
 */
static void
getDependencies(Archive *fout)
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
		write_msg(NULL, "reading dependency data\n");

	query = createPQExpBuffer();

	/*
	 * Messy query to collect the dependency data we need.  Note that we
	 * ignore the sub-object column, so that dependencies of or on a column
	 * look the same as dependencies of or on a whole table.
	 *
	 * PIN dependencies aren't interesting, and EXTENSION dependencies were
	 * already processed by getExtensionMembership.
	 */
	appendPQExpBufferStr(query, "SELECT "
						 "classid, objid, refclassid, refobjid, deptype "
						 "FROM pg_depend "
						 "WHERE deptype != 'p' AND deptype != 'e'\n");

	/*
	 * Since we don't treat pg_amop entries as separate DumpableObjects, we
	 * have to translate their dependencies into dependencies of their parent
	 * opfamily.  Ignore internal dependencies though, as those will point to
	 * their parent opclass, which we needn't consider here (and if we did,
	 * it'd just result in circular dependencies).  Also, "loose" opfamily
	 * entries will have dependencies on their parent opfamily, which we
	 * should drop since they'd likewise become useless self-dependencies.
	 * (But be sure to keep deps on *other* opfamilies; see amopsortfamily.)
	 */
	appendPQExpBufferStr(query, "UNION ALL\n"
						 "SELECT 'pg_opfamily'::regclass AS classid, amopfamily AS objid, refclassid, refobjid, deptype "
						 "FROM pg_depend d, pg_amop o "
						 "WHERE deptype NOT IN ('p', 'e', 'i') AND "
						 "classid = 'pg_amop'::regclass AND objid = o.oid "
						 "AND NOT (refclassid = 'pg_opfamily'::regclass AND amopfamily = refobjid)\n");

	/* Likewise for pg_amproc entries */
	appendPQExpBufferStr(query, "UNION ALL\n"
						 "SELECT 'pg_opfamily'::regclass AS classid, amprocfamily AS objid, refclassid, refobjid, deptype "
						 "FROM pg_depend d, pg_amproc p "
						 "WHERE deptype NOT IN ('p', 'e', 'i') AND "
						 "classid = 'pg_amproc'::regclass AND objid = p.oid "
						 "AND NOT (refclassid = 'pg_opfamily'::regclass AND amprocfamily = refobjid)\n");

	/* Sort the output for efficiency below */
	appendPQExpBufferStr(query, "ORDER BY 1,2");

	res = ExecuteSqlQuery(fout, query->data, PGRES_TUPLES_OK);

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
		 * tables.  However, for a composite type the implicit dependency goes
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
 * createBoundaryObjects - create dummy DumpableObjects to represent
 * dump section boundaries.
 */
static DumpableObject *
createBoundaryObjects(void)
{
	DumpableObject *dobjs;

	dobjs = (DumpableObject *) pg_malloc(2 * sizeof(DumpableObject));

	dobjs[0].objType = DO_PRE_DATA_BOUNDARY;
	dobjs[0].catId = nilCatalogId;
	AssignDumpId(dobjs + 0);
	dobjs[0].name = pg_strdup("PRE-DATA BOUNDARY");

	dobjs[1].objType = DO_POST_DATA_BOUNDARY;
	dobjs[1].catId = nilCatalogId;
	AssignDumpId(dobjs + 1);
	dobjs[1].name = pg_strdup("POST-DATA BOUNDARY");

	return dobjs;
}

/*
 * addBoundaryDependencies - add dependencies as needed to enforce the dump
 * section boundaries.
 */
static void
addBoundaryDependencies(DumpableObject **dobjs, int numObjs,
						DumpableObject *boundaryObjs)
{
	DumpableObject *preDataBound = boundaryObjs + 0;
	DumpableObject *postDataBound = boundaryObjs + 1;
	int			i;

	for (i = 0; i < numObjs; i++)
	{
		DumpableObject *dobj = dobjs[i];

		/*
		 * The classification of object types here must match the SECTION_xxx
		 * values assigned during subsequent ArchiveEntry calls!
		 */
		switch (dobj->objType)
		{
			case DO_NAMESPACE:
			case DO_EXTENSION:
			case DO_TYPE:
			case DO_SHELL_TYPE:
			case DO_FUNC:
			case DO_AGG:
			case DO_OPERATOR:
			case DO_OPCLASS:
			case DO_OPFAMILY:
			case DO_COLLATION:
			case DO_CONVERSION:
			case DO_TABLE:
			case DO_ATTRDEF:
			case DO_PROCLANG:
			case DO_CAST:
			case DO_DUMMY_TYPE:
			case DO_TSPARSER:
			case DO_TSDICT:
			case DO_TSTEMPLATE:
			case DO_TSCONFIG:
			case DO_FDW:
			case DO_FOREIGN_SERVER:
			case DO_BLOB:
			case DO_EXTPROTOCOL:
			case DO_BINARY_UPGRADE:
				/* Pre-data objects: must come before the pre-data boundary */
				addObjectDependency(preDataBound, dobj->dumpId);
				break;
			case DO_TABLE_DATA:
			case DO_BLOB_DATA:
				/* Data objects: must come between the boundaries */
				addObjectDependency(dobj, preDataBound->dumpId);
				addObjectDependency(postDataBound, dobj->dumpId);
				break;
			case DO_INDEX:
			case DO_REFRESH_MATVIEW:
			case DO_TRIGGER:
			case DO_EVENT_TRIGGER:
			case DO_DEFAULT_ACL:
				/* Post-data objects: must come after the post-data boundary */
				addObjectDependency(dobj, postDataBound->dumpId);
				break;
			case DO_RULE:
				/* Rules are post-data, but only if dumped separately */
				if (((RuleInfo *) dobj)->separate)
					addObjectDependency(dobj, postDataBound->dumpId);
				break;
			case DO_CONSTRAINT:
			case DO_FK_CONSTRAINT:
				/* Constraints are post-data, but only if dumped separately */
				if (((ConstraintInfo *) dobj)->separate)
					addObjectDependency(dobj, postDataBound->dumpId);
				break;
			case DO_PRE_DATA_BOUNDARY:
				/* nothing to do */
				break;
			case DO_POST_DATA_BOUNDARY:
				/* must come after the pre-data boundary */
				addObjectDependency(dobj, preDataBound->dumpId);
				break;
		}
	}
}


/*
 * BuildArchiveDependencies - create dependency data for archive TOC entries
 *
 * The raw dependency data obtained by getDependencies() is not terribly
 * useful in an archive dump, because in many cases there are dependency
 * chains linking through objects that don't appear explicitly in the dump.
 * For example, a view will depend on its _RETURN rule while the _RETURN rule
 * will depend on other objects --- but the rule will not appear as a separate
 * object in the dump.  We need to adjust the view's dependencies to include
 * whatever the rule depends on that is included in the dump.
 *
 * Just to make things more complicated, there are also "special" dependencies
 * such as the dependency of a TABLE DATA item on its TABLE, which we must
 * not rearrange because pg_restore knows that TABLE DATA only depends on
 * its table.  In these cases we must leave the dependencies strictly as-is
 * even if they refer to not-to-be-dumped objects.
 *
 * To handle this, the convention is that "special" dependencies are created
 * during ArchiveEntry calls, and an archive TOC item that has any such
 * entries will not be touched here.  Otherwise, we recursively search the
 * DumpableObject data structures to build the correct dependencies for each
 * archive TOC item.
 */
static void
BuildArchiveDependencies(Archive *fout)
{
	ArchiveHandle *AH = (ArchiveHandle *) fout;
	TocEntry   *te;

	/* Scan all TOC entries in the archive */
	for (te = AH->toc->next; te != AH->toc; te = te->next)
	{
		DumpableObject *dobj;
		DumpId	   *dependencies;
		int			nDeps;
		int			allocDeps;

		/* No need to process entries that will not be dumped */
		if (te->reqs == 0)
			continue;
		/* Ignore entries that already have "special" dependencies */
		if (te->nDeps > 0)
			continue;
		/* Otherwise, look up the item's original DumpableObject, if any */
		dobj = findObjectByDumpId(te->dumpId);
		if (dobj == NULL)
			continue;
		/* No work if it has no dependencies */
		if (dobj->nDeps <= 0)
			continue;
		/* Set up work array */
		allocDeps = 64;
		dependencies = (DumpId *) pg_malloc(allocDeps * sizeof(DumpId));
		nDeps = 0;
		/* Recursively find all dumpable dependencies */
		findDumpableDependencies(AH, dobj,
								 &dependencies, &nDeps, &allocDeps);
		/* And save 'em ... */
		if (nDeps > 0)
		{
			dependencies = (DumpId *) pg_realloc(dependencies,
												 nDeps * sizeof(DumpId));
			te->dependencies = dependencies;
			te->nDeps = nDeps;
		}
		else
			free(dependencies);
	}
}

/* Recursive search subroutine for BuildArchiveDependencies */
static void
findDumpableDependencies(ArchiveHandle *AH, DumpableObject *dobj,
						 DumpId **dependencies, int *nDeps, int *allocDeps)
{
	int			i;

	/*
	 * Ignore section boundary objects: if we search through them, we'll
	 * report lots of bogus dependencies.
	 */
	if (dobj->objType == DO_PRE_DATA_BOUNDARY ||
		dobj->objType == DO_POST_DATA_BOUNDARY)
		return;

	for (i = 0; i < dobj->nDeps; i++)
	{
		DumpId		depid = dobj->dependencies[i];

		if (TocIDRequired(AH, depid) != 0)
		{
			/* Object will be dumped, so just reference it as a dependency */
			if (*nDeps >= *allocDeps)
			{
				*allocDeps *= 2;
				*dependencies = (DumpId *) pg_realloc(*dependencies,
												*allocDeps * sizeof(DumpId));
			}
			(*dependencies)[*nDeps] = depid;
			(*nDeps)++;
		}
		else
		{
			/*
			 * Object will not be dumped, so recursively consider its deps. We
			 * rely on the assumption that sortDumpableObjects already broke
			 * any dependency loops, else we might recurse infinitely.
			 */
			DumpableObject *otherdobj = findObjectByDumpId(depid);

			if (otherdobj)
				findDumpableDependencies(AH, otherdobj,
										 dependencies, nDeps, allocDeps);
		}
	}
}

/*
 * isGPbackend - returns true if the connected backend is a GreenPlum DB backend.
 */
static bool
testGPbackend(Archive *fout)
{
	PQExpBuffer query;
	PGresult   *res;
	bool		isGPbackend;

	query = createPQExpBuffer();

	appendPQExpBuffer(query, "SELECT current_setting('gp_role');");
	ArchiveHandle *AH = (ArchiveHandle *) fout;
	res = PQexec(AH->connection, query->data);

	isGPbackend = (PQresultStatus(res) == PGRES_TUPLES_OK);

	PQclear(res);
	destroyPQExpBuffer(query);

	return isGPbackend;
}

/*
 *	addDistributedBy
 *
 *	find the distribution policy of the passed in relation and append the
 *	DISTRIBUTED BY clause to the passed in dump buffer (q).
 */
static void
addDistributedBy(Archive *fout, PQExpBuffer q, TableInfo *tbinfo, int actual_atts)
{
	if (isGPDB6000OrLater(fout))
	{
		if (strcmp(tbinfo->distclause, "") != 0)
			appendPQExpBuffer(q, " %s", tbinfo->distclause);
	}
	else
		addDistributedByOld(fout, q, tbinfo, actual_atts);
}

/*
 * This is used with GPDB 5 and older, where pg_get_table_distributedby()
 * backend function is not available.
 */
static void
addDistributedByOld(Archive *fout, PQExpBuffer q, TableInfo *tbinfo, int actual_atts)
{
	char	   *policydef = tbinfo->distclause;
	char	   *policycol;

	if (strcmp(policydef, "f") == 0)
	{
		/*
		 * There is no entry in the policy table for this table. Report an
		 * error unless this is a zero attribute table (actual_atts == 0).
		 *
		 * In binary_upgrade mode, we run directly against segments, and there
		 * are no gp_distribution_policy rows in segments.
		 */
		if (actual_atts > 0 && !binary_upgrade)
		{
			/* if this is a catalog table we allow dumping it, skip the error */
			if (strncmp(tbinfo->dobj.namespace->dobj.name, "pg_", 3) != 0)
			{
				write_msg(NULL, "query to obtain distribution policy of table \"%s\" returned no data\n",
							   tbinfo->dobj.name);
				exit_nicely(1);
			}
		}
	}
	else
	{
		/*
		 * There is exactly 1 policy entry for this table (either a concrete
		 * one or NULL).
		 */
		policydef = tbinfo->distclause;

		if (strlen(policydef) > 0)
		{
			bool		isfirst = true;
			char	   *separator;

			appendPQExpBuffer(q, " DISTRIBUTED BY (");

			/* policy indicates one or more columns to distribute on */
			policydef[strlen(policydef) - 1] = '\0';
			policydef++;
			separator = ",";

			while ((policycol = nextToken(&policydef, ",")) != NULL)
			{
				if (!isfirst)
					appendPQExpBuffer(q, ", ");
				isfirst = false;

				appendPQExpBuffer(q, "%s",
								  fmtId(tbinfo->attnames[atoi(policycol) - 1]));
			}
			appendPQExpBufferChar(q, ')');
		}
		else
		{
			/* policy has an empty policy - distribute randomly */
			appendPQExpBufferStr(q, " DISTRIBUTED RANDOMLY");
		}
	}
}

/*
 * getFormattedTypeName - retrieve a nicely-formatted type name for the
 * given type OID.
 *
 * This does not guarantee to schema-qualify the output, so it should not
 * be used to create the target object name for CREATE or ALTER commands.
 */
static char *
getFormattedTypeName(Archive *fout, Oid oid, OidOptions opts)
{
	TypeInfo   *typeInfo;
	char	   *result;
	PQExpBuffer query;
	PGresult   *res;

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

	/* see if we have the result cached in the type's TypeInfo record */
	typeInfo = findTypeByOid(oid);
	if (typeInfo && typeInfo->ftypname)
		return pg_strdup(typeInfo->ftypname);

	query = createPQExpBuffer();
	appendPQExpBuffer(query, "SELECT pg_catalog.format_type('%u'::pg_catalog.oid, NULL)",
						oid);

	res = ExecuteSqlQueryForSingleRow(fout, query->data);
	result = pg_strdup(PQgetvalue(res, 0, 0));

	PQclear(res);
	destroyPQExpBuffer(query);

	/* cache a copy for later requests */
	if (typeInfo)
		typeInfo->ftypname = pg_strdup(result);

	return result;
}

/*
 * Return a column list clause for the given relation.
 *
 * Special case: if there are no undropped columns in the relation, return
 * "", not an invalid "()" column list.
 */
static const char *
fmtCopyColumnList(const TableInfo *ti, PQExpBuffer buffer)
{
	int			numatts = ti->numatts;
	char	  **attnames = ti->attnames;
	bool	   *attisdropped = ti->attisdropped;
	bool		needComma;
	int			i;

	appendPQExpBufferChar(buffer, '(');
	needComma = false;
	for (i = 0; i < numatts; i++)
	{
		if (attisdropped[i])
			continue;
		if (needComma)
			appendPQExpBufferStr(buffer, ", ");
		appendPQExpBufferStr(buffer, fmtId(attnames[i]));
		needComma = true;
	}

	if (!needComma)
		return "";				/* no undropped columns */

	appendPQExpBufferChar(buffer, ')');
	return buffer->data;
}

/*
 * Check if a reloptions array is nonempty.
 */
static bool
nonemptyReloptions(const char *reloptions)
{
	/* Don't want to print it if it's just "{}" */
	return (reloptions != NULL && strlen(reloptions) > 2);
}

/*
 * Format a reloptions array and append it to the given buffer.
 *
 * "prefix" is prepended to the option names; typically it's "" or "toast.".
 *
 * Note: this logic should generally match the backend's flatten_reloptions()
 * (in adt/ruleutils.c).
 */
static void
fmtReloptionsArray(Archive *fout, PQExpBuffer buffer, const char *reloptions,
				   const char *prefix)
{
	char	  **options;
	int			noptions;
	int			i;

	if (!parsePGArray(reloptions, &options, &noptions))
	{
		write_msg(NULL, "WARNING: could not parse reloptions array\n");
		if (options)
			free(options);
		return;
	}

	for (i = 0; i < noptions; i++)
	{
		char	   *option = options[i];
		char	   *name;
		char	   *separator;
		char	   *value;

		/*
		 * Each array element should have the form name=value.  If the "=" is
		 * missing for some reason, treat it like an empty value.
		 */
		name = option;
		separator = strchr(option, '=');
		if (separator)
		{
			*separator = '\0';
			value = separator + 1;
		}
		else
			value = "";

		if (i > 0)
			appendPQExpBufferStr(buffer, ", ");
		appendPQExpBuffer(buffer, "%s%s=", prefix, fmtId(name));

		/*
		 * In general we need to quote the value; but to avoid unnecessary
		 * clutter, do not quote if it is an identifier that would not need
		 * quoting.  (We could also allow numbers, but that is a bit trickier
		 * than it looks --- for example, are leading zeroes significant?  We
		 * don't want to assume very much here about what custom reloptions
		 * might mean.)
		 */
		if (strcmp(fmtId(value), value) == 0)
			appendPQExpBufferStr(buffer, value);
		else
			appendStringLiteralAH(buffer, value, fout);
	}

	if (options)
		free(options);
}

/* START MPP ADDITION */
/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
														 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
static char *
nextToken(register char **stringp, register const char *delim)
{
	register char *s;
	register const char *spanp;
	register int c,
				sc;
	char	   *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;)
	{
		c = *s++;
		spanp = delim;
		do
		{
			if ((sc = *spanp++) == c)
			{
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

/* END MPP ADDITION */
