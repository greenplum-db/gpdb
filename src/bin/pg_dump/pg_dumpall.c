/*-------------------------------------------------------------------------
 *
 * pg_dumpall.c
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * pg_dumpall forces all pg_dump output to be text, since it also outputs
 * text into the same output stream.
 *
 * src/bin/pg_dump/pg_dumpall.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <time.h>
#include <unistd.h>

#include "getopt_long.h"

#include "dumputils.h"
#include "pg_backup.h"
#include "common/file_utils.h"
#include "common/logging.h"
#include "fe_utils/connect.h"
#include "fe_utils/string_utils.h"

/* version string we expect back from pg_dump */
#define PGDUMP_VERSIONSTR "pg_dump (PostgreSQL) " PG_VERSION "\n"


static void help(void);

static void dumpResQueues(PGconn *conn);
static void dumpResGroups(PGconn *conn);
static void dumpRoleConstraints(PGconn *conn);

static void dropRoles(PGconn *conn);
static void dumpRoles(PGconn *conn);
static void dumpRoleMembership(PGconn *conn);
static void dropTablespaces(PGconn *conn);
static void dumpTablespaces(PGconn *conn);
static void dropDBs(PGconn *conn);
static void dumpUserConfig(PGconn *conn, const char *username);
static void dumpDatabases(PGconn *conn);
static void dumpTimestamp(const char *msg);
static int	runPgDump(const char *dbname, const char *create_opts);
static void buildShSecLabels(PGconn *conn,
							 const char *catalog_name, Oid objectId,
							 const char *objtype, const char *objname,
							 PQExpBuffer buffer);
static PGconn *connectDatabase(const char *dbname, const char *connstr, const char *pghost, const char *pgport,
							   const char *pguser, trivalue prompt_password, bool fail_on_error);
static char *constructConnStr(const char **keywords, const char **values);
static PGresult *executeQuery(PGconn *conn, const char *query);
static void executeCommand(PGconn *conn, const char *query);
static void expand_dbname_patterns(PGconn *conn, SimpleStringList *patterns,
								   SimpleStringList *names);

static void error_unsupported_server_version(PGconn *conn) pg_attribute_noreturn();

static char pg_dump_bin[MAXPGPATH];
static const char *progname;
static PQExpBuffer pgdumpopts;
static char *connstr = "";
static bool output_clean = false;
static bool skip_acls = false;
static bool verbose = false;
static bool dosync = true;

static int	resource_queues = 0;
static int	resource_groups = 0;

static int	binary_upgrade = 0;
static int	column_inserts = 0;
static int	disable_dollar_quoting = 0;
static int	disable_triggers = 0;
static int	if_exists = 0;
static int	inserts = 0;
static int	no_tablespaces = 0;
static int	use_setsessauth = 0;
static int	no_comments = 0;
static int	no_publications = 0;
static int	no_security_labels = 0;
static int	no_subscriptions = 0;
static int	no_unlogged_table_data = 0;
static int	no_role_passwords = 0;
static int	server_version;
static int	load_via_partition_root = 0;
static int	on_conflict_do_nothing = 0;

static char role_catalog[10];
#define PG_AUTHID "pg_authid"
#define PG_ROLES  "pg_roles "

static FILE *OPF;
static char *filename = NULL;

static SimpleStringList database_exclude_patterns = {NULL, NULL};
static SimpleStringList database_exclude_names = {NULL, NULL};

#define exit_nicely(code) exit(code)

int
main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"data-only", no_argument, NULL, 'a'},
		{"clean", no_argument, NULL, 'c'},
		{"encoding", required_argument, NULL, 'E'},
		{"file", required_argument, NULL, 'f'},
		{"globals-only", no_argument, NULL, 'g'},
		{"host", required_argument, NULL, 'h'},
		{"dbname", required_argument, NULL, 'd'},
		{"database", required_argument, NULL, 'l'},
		{"no-owner", no_argument, NULL, 'O'},
		{"port", required_argument, NULL, 'p'},
		{"schema-only", no_argument, NULL, 's'},
		{"superuser", required_argument, NULL, 'S'},
		{"tablespaces-only", no_argument, NULL, 't'},
		{"username", required_argument, NULL, 'U'},
		{"verbose", no_argument, NULL, 'v'},
		{"no-password", no_argument, NULL, 'w'},
		{"password", no_argument, NULL, 'W'},
		{"no-privileges", no_argument, NULL, 'x'},
		{"no-acl", no_argument, NULL, 'x'},

		/*
		 * the following options don't have an equivalent short option letter
		 */
		{"attribute-inserts", no_argument, &column_inserts, 1},
		{"binary-upgrade", no_argument, &binary_upgrade, 1},
		{"column-inserts", no_argument, &column_inserts, 1},
		{"disable-dollar-quoting", no_argument, &disable_dollar_quoting, 1},
		{"disable-triggers", no_argument, &disable_triggers, 1},
		{"exclude-database", required_argument, NULL, 6},
		{"extra-float-digits", required_argument, NULL, 5},
		{"if-exists", no_argument, &if_exists, 1},
		{"inserts", no_argument, &inserts, 1},
		{"resource-queues", no_argument, &resource_queues, 1},
		{"resource-groups", no_argument, &resource_groups, 1},
		{"roles-only", no_argument, NULL, 999},
		{"lock-wait-timeout", required_argument, NULL, 2},
		{"no-tablespaces", no_argument, &no_tablespaces, 1},
		{"quote-all-identifiers", no_argument, &quote_all_identifiers, 1},
		{"load-via-partition-root", no_argument, &load_via_partition_root, 1},
		{"role", required_argument, NULL, 3},
		{"use-set-session-authorization", no_argument, &use_setsessauth, 1},
		{"no-comments", no_argument, &no_comments, 1},
		{"no-publications", no_argument, &no_publications, 1},
		{"no-role-passwords", no_argument, &no_role_passwords, 1},
		{"no-security-labels", no_argument, &no_security_labels, 1},
		{"no-subscriptions", no_argument, &no_subscriptions, 1},
		{"no-sync", no_argument, NULL, 4},
		{"no-unlogged-table-data", no_argument, &no_unlogged_table_data, 1},
		{"on-conflict-do-nothing", no_argument, &on_conflict_do_nothing, 1},
		{"rows-per-insert", required_argument, NULL, 7},

		/* START MPP ADDITION */
		{"gp-syntax", no_argument, NULL, 1000},
		{"no-gp-syntax", no_argument, NULL, 1001},
		/* END MPP ADDITION */

		{NULL, 0, NULL, 0}
	};

	char	   *pghost = NULL;
	char	   *pgport = NULL;
	char	   *pguser = NULL;
	char	   *pgdb = NULL;
	char	   *use_role = NULL;
	const char *dumpencoding = NULL;
	trivalue	prompt_password = TRI_DEFAULT;
	bool		data_only = false;
	bool		globals_only = false;
	bool		roles_only = false;
	bool		tablespaces_only = false;
	PGconn	   *conn;
	int			encoding;
	const char *std_strings;
	int			c,
				ret;
	int			optindex;
	bool		gp_syntax = false;
	bool		no_gp_syntax = false;

	pg_logging_init(argv[0]);
	pg_logging_set_level(PG_LOG_WARNING);
	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));
	progname = get_progname(argv[0]);

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			help();
			exit_nicely(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_dumpall (PostgreSQL) " PG_VERSION);
			exit_nicely(0);
		}
	}

	if ((ret = find_other_exec(argv[0], "pg_dump", PGDUMP_VERSIONSTR,
							   pg_dump_bin)) < 0)
	{
		char		full_path[MAXPGPATH];

		if (find_my_exec(argv[0], full_path) < 0)
			strlcpy(full_path, progname, sizeof(full_path));

		if (ret == -1)
			pg_log_error("The program \"pg_dump\" is needed by %s but was not found in the\n"
						 "same directory as \"%s\".\n"
						 "Check your installation.",
						 progname, full_path);
		else
			pg_log_error("The program \"pg_dump\" was found by \"%s\"\n"
						 "but was not the same version as %s.\n"
						 "Check your installation.",
						 full_path, progname);
		exit_nicely(1);
	}

	pgdumpopts = createPQExpBuffer();

	while ((c = getopt_long(argc, argv, "acd:E:f:gh:l:Op:rsS:tU:vwWx", long_options, &optindex)) != -1)
	{
		switch (c)
		{
			case 'a':
				data_only = true;
				appendPQExpBufferStr(pgdumpopts, " -a");
				break;

			case 'c':
				output_clean = true;
				break;

			case 'd':
				connstr = pg_strdup(optarg);
				break;

			case 'E':
				dumpencoding = pg_strdup(optarg);
				appendPQExpBufferStr(pgdumpopts, " -E ");
				appendShellString(pgdumpopts, optarg);
				break;

			case 'f':
				filename = pg_strdup(optarg);
				appendPQExpBufferStr(pgdumpopts, " -f ");
				appendShellString(pgdumpopts, filename);
				break;

			case 'g':
				globals_only = true;
				break;

			case 'h':
				pghost = pg_strdup(optarg);
				break;

			case 'l':
				pgdb = pg_strdup(optarg);
				break;

			case 'O':
				appendPQExpBufferStr(pgdumpopts, " -O");
				break;

			case 'p':
				pgport = pg_strdup(optarg);
				break;

			/*
			 * Both Greenplum and PostgreSQL have used -r but for different
			 * options, disallow the short option entirely to avoid confusion
			 * and require the use of long options for the conflicting pair.
			 */
			case 'r':
				fprintf(stderr, _("-r option is not supported. Did you mean --roles-only or --resource-queues?\n"));
				exit(1);
				break;

			case 999:	/* --roles-only */
				roles_only = true;
				break;

			case 's':
				appendPQExpBufferStr(pgdumpopts, " -s");
				break;

			case 'S':
				appendPQExpBufferStr(pgdumpopts, " -S ");
				appendShellString(pgdumpopts, optarg);
				break;

			case 't':
				tablespaces_only = true;
				break;

			case 'U':
				pguser = pg_strdup(optarg);
				break;

			case 'v':
				verbose = true;
				pg_logging_set_level(PG_LOG_INFO);
				appendPQExpBufferStr(pgdumpopts, " -v");
				break;

			case 'w':
				prompt_password = TRI_NO;
				appendPQExpBufferStr(pgdumpopts, " -w");
				break;

			case 'W':
				prompt_password = TRI_YES;
				appendPQExpBufferStr(pgdumpopts, " -W");
				break;

			case 'x':
				skip_acls = true;
				appendPQExpBufferStr(pgdumpopts, " -x");
				break;

			case 0:
				break;

			case 2:
				appendPQExpBufferStr(pgdumpopts, " --lock-wait-timeout ");
				appendShellString(pgdumpopts, optarg);
				break;

			case 3:
				use_role = pg_strdup(optarg);
				appendPQExpBufferStr(pgdumpopts, " --role ");
				appendShellString(pgdumpopts, use_role);
				break;

			case 4:
				dosync = false;
				appendPQExpBufferStr(pgdumpopts, " --no-sync");
				break;

			case 5:
				appendPQExpBufferStr(pgdumpopts, " --extra-float-digits ");
				appendShellString(pgdumpopts, optarg);
				break;

			case 6:
				simple_string_list_append(&database_exclude_patterns, optarg);
				break;

			case 7:
				appendPQExpBufferStr(pgdumpopts, " --rows-per-insert ");
				appendShellString(pgdumpopts, optarg);
				break;

				/* START MPP ADDITION */
			case 1000:
				/* gp-format */
				appendPQExpBuffer(pgdumpopts, " --gp-syntax");
				gp_syntax = true;
				resource_queues = 1; /* --resource-queues is implied by --gp-syntax */
				resource_groups = 1; /* --resource-groups is implied by --gp-syntax */
				break;
			case 1001:
				/* no-gp-format */
				appendPQExpBuffer(pgdumpopts, " --no-gp-syntax");
				no_gp_syntax = true;
				break;

				/* END MPP ADDITION */

			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit_nicely(1);
		}
	}

	/* Complain if any arguments remain */
	if (optind < argc)
	{
		pg_log_error("too many command-line arguments (first is \"%s\")",
					 argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (database_exclude_patterns.head != NULL &&
		(globals_only || roles_only || tablespaces_only))
	{
		pg_log_error("option --exclude-database cannot be used together with -g/--globals-only, -r/--roles-only, or -t/--tablespaces-only");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	/* Make sure the user hasn't specified a mix of globals-only options */
	if (globals_only && roles_only)
	{
		pg_log_error("options -g/--globals-only and -r/--roles-only cannot be used together");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (globals_only && tablespaces_only)
	{
		pg_log_error("options -g/--globals-only and -t/--tablespaces-only cannot be used together");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (if_exists && !output_clean)
	{
		pg_log_error("option --if-exists requires option -c/--clean");
		exit_nicely(1);
	}

	if (roles_only && tablespaces_only)
	{
		pg_log_error("options -r/--roles-only and -t/--tablespaces-only cannot be used together");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (gp_syntax && no_gp_syntax)
	{
		pg_log_error("options --gp-syntax and --no-gp-syntax cannot be used together");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	/*
	 * If password values are not required in the dump, switch to using
	 * pg_roles which is equally useful, just more likely to have unrestricted
	 * access than pg_authid.
	 */
	if (no_role_passwords)
		sprintf(role_catalog, "%s", PG_ROLES);
	else
		sprintf(role_catalog, "%s", PG_AUTHID);

	/* Add long options to the pg_dump argument list */
	if (binary_upgrade)
		appendPQExpBufferStr(pgdumpopts, " --binary-upgrade");
	if (column_inserts)
		appendPQExpBufferStr(pgdumpopts, " --column-inserts");
	if (disable_dollar_quoting)
		appendPQExpBufferStr(pgdumpopts, " --disable-dollar-quoting");
	if (disable_triggers)
		appendPQExpBufferStr(pgdumpopts, " --disable-triggers");
	if (inserts)
		appendPQExpBufferStr(pgdumpopts, " --inserts");
	if (no_tablespaces)
		appendPQExpBufferStr(pgdumpopts, " --no-tablespaces");
	if (quote_all_identifiers)
		appendPQExpBufferStr(pgdumpopts, " --quote-all-identifiers");
	if (load_via_partition_root)
		appendPQExpBufferStr(pgdumpopts, " --load-via-partition-root");
	if (use_setsessauth)
		appendPQExpBufferStr(pgdumpopts, " --use-set-session-authorization");
	if (no_comments)
		appendPQExpBufferStr(pgdumpopts, " --no-comments");
	if (no_publications)
		appendPQExpBufferStr(pgdumpopts, " --no-publications");
	if (no_security_labels)
		appendPQExpBufferStr(pgdumpopts, " --no-security-labels");
	if (no_subscriptions)
		appendPQExpBufferStr(pgdumpopts, " --no-subscriptions");
	if (no_unlogged_table_data)
		appendPQExpBufferStr(pgdumpopts, " --no-unlogged-table-data");
	if (on_conflict_do_nothing)
		appendPQExpBufferStr(pgdumpopts, " --on-conflict-do-nothing");
	if (roles_only)
		appendPQExpBufferStr(pgdumpopts, " --roles-only");

	/*
	 * If there was a database specified on the command line, use that,
	 * otherwise try to connect to database "postgres", and failing that
	 * "template1".  "postgres" is the preferred choice for 8.1 and later
	 * servers, but it usually will not exist on older ones.
	 */
	if (pgdb)
	{
		conn = connectDatabase(pgdb, connstr, pghost, pgport, pguser,
							   prompt_password, false);

		if (!conn)
		{
			pg_log_error("could not connect to database \"%s\"", pgdb);
			exit_nicely(1);
		}
	}
	else
	{
		conn = connectDatabase("postgres", connstr, pghost, pgport, pguser,
							   prompt_password, false);
		if (!conn)
			conn = connectDatabase("template1", connstr, pghost, pgport, pguser,
								   prompt_password, true);

		if (!conn)
		{
			pg_log_error("could not connect to databases \"postgres\" or \"template1\"\n"
						 "Please specify an alternative database.");
			fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
					progname);
			exit_nicely(1);
		}
	}

	/*
	 * Get a list of database names that match the exclude patterns
	 */
	expand_dbname_patterns(conn, &database_exclude_patterns,
						   &database_exclude_names);

	/*
	 * Open the output file if required, otherwise use stdout
	 */
	if (filename)
	{
		OPF = fopen(filename, PG_BINARY_W);
		if (!OPF)
		{
			pg_log_error("could not open the output file \"%s\": %m",
						 filename);
			exit_nicely(1);
		}
	}
	else
		OPF = stdout;

	/*
	 * Set the client encoding if requested.
	 */
	if (dumpencoding)
	{
		if (PQsetClientEncoding(conn, dumpencoding) < 0)
		{
			pg_log_error("invalid client encoding \"%s\" specified",
						 dumpencoding);
			exit_nicely(1);
		}
	}

	/*
	 * Get the active encoding and the standard_conforming_strings setting, so
	 * we know how to escape strings.
	 */
	encoding = PQclientEncoding(conn);
	std_strings = PQparameterStatus(conn, "standard_conforming_strings");
	if (!std_strings)
		std_strings = "off";

	/* Set the role if requested */
	if (use_role && server_version >= 80100)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
		executeCommand(conn, query->data);
		destroyPQExpBuffer(query);
	}

	/* Force quoting of all identifiers if requested. */
	if (quote_all_identifiers && server_version >= 90100)
		executeCommand(conn, "SET quote_all_identifiers = true");

	fprintf(OPF,"--\n-- Greenplum Database cluster dump\n--\n\n");
	if (verbose)
		dumpTimestamp("Started on");

	/*
	 * We used to emit \connect postgres here, but that served no purpose
	 * other than to break things for installations without a postgres
	 * database.  Everything we're restoring here is a global, so whichever
	 * database we're connected to at the moment is fine.
	 */

	/* Restore will need to write to the target cluster */
	fprintf(OPF, "SET default_transaction_read_only = off;\n\n");

	/* Replicate encoding and std_strings in output */
	fprintf(OPF, "SET client_encoding = '%s';\n",
			pg_encoding_to_char(encoding));
	fprintf(OPF, "SET standard_conforming_strings = %s;\n", std_strings);
	if (strcmp(std_strings, "off") == 0)
		fprintf(OPF, "SET escape_string_warning = off;\n");
	fprintf(OPF, "\n");

	if (binary_upgrade)
	{
		/*
		 * Greenplum doesn't allow altering system catalogs without
		 * setting the allow_system_table_mods GUC first.
		 */
		fprintf(OPF, "SET allow_system_table_mods = true;\n");
		fprintf(OPF, "\n");
	}

	if (!data_only)
	{
		/*
		 * If asked to --clean, do that first.  We can avoid detailed
		 * dependency analysis because databases never depend on each other,
		 * and tablespaces never depend on each other.  Roles could have
		 * grants to each other, but DROP ROLE will clean those up silently.
		 */
		if (output_clean)
		{
			if (!globals_only && !roles_only && !tablespaces_only)
				dropDBs(conn);

			if (!roles_only && !no_tablespaces)
				dropTablespaces(conn);

			if (!tablespaces_only)
				dropRoles(conn);
		}

		/*
		 * Now create objects as requested.  Be careful that option logic here
		 * is the same as for drops above.
		 */
		if (!tablespaces_only)
		{
			/* Dump Resource Queues */
			if (resource_queues)
				dumpResQueues(conn);

			/* Dump Resource Groups */
			if (resource_groups)
				dumpResGroups(conn);

			/* Dump roles (users) */
			dumpRoles(conn);

			/* Dump role memberships */
			dumpRoleMembership(conn);

			/* Dump role constraints */
			dumpRoleConstraints(conn);
		}

		/* Dump tablespaces */
		if (!roles_only && !no_tablespaces)
			/* Dump tablespaces */
			dumpTablespaces(conn);
	}

	if (!globals_only && !roles_only && !tablespaces_only)
		dumpDatabases(conn);

	PQfinish(conn);

	if (verbose)
		dumpTimestamp("Completed on");
	fprintf(OPF, "--\n-- PostgreSQL database cluster dump complete\n--\n\n");

	if (filename)
	{
		fclose(OPF);

		/* sync the resulting file, errors are not fatal */
		if (dosync)
			(void) fsync_fname(filename, false);
	}

	exit_nicely(0);
}


static void
help(void)
{
	printf(_("%s extracts a PostgreSQL database cluster into an SQL script file.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]...\n"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  -f, --file=FILENAME          output file name\n"));
	printf(_("  -v, --verbose                verbose mode\n"));
	printf(_("  -V, --version                output version information, then exit\n"));
	printf(_("  --lock-wait-timeout=TIMEOUT  fail after waiting TIMEOUT for a table lock\n"));
	printf(_("  -?, --help                   show this help, then exit\n"));
	printf(_("\nOptions controlling the output content:\n"));
	printf(_("  -a, --data-only              dump only the data, not the schema\n"));
	printf(_("  -c, --clean                  clean (drop) databases before recreating\n"));
	printf(_("  -E, --encoding=ENCODING      dump the data in encoding ENCODING\n"));
	printf(_("  -g, --globals-only           dump only global objects, no databases\n"));
	printf(_("  -O, --no-owner               skip restoration of object ownership\n"));
	printf(_("      --roles-only             dump only roles, no databases or tablespaces\n"));
	printf(_("  -s, --schema-only            dump only the schema, no data\n"));
	printf(_("  -S, --superuser=NAME         superuser user name to use in the dump\n"));
	printf(_("  -t, --tablespaces-only       dump only tablespaces, no databases or roles\n"));
	printf(_("  -x, --no-privileges          do not dump privileges (grant/revoke)\n"));
	printf(_("  --resource-queues            dump resource queue data\n"));
	printf(_("  --resource-groups            dump resource group data\n"));
	printf(_("  --binary-upgrade             for use by upgrade utilities only\n"));
	printf(_("  --column-inserts             dump data as INSERT commands with column names\n"));
	printf(_("  --disable-dollar-quoting     disable dollar quoting, use SQL standard quoting\n"));
	printf(_("  --disable-triggers           disable triggers during data-only restore\n"));
	printf(_("  --exclude-database=PATTERN   exclude databases whose name matches PATTERN\n"));
	printf(_("  --extra-float-digits=NUM     override default setting for extra_float_digits\n"));
	printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
	printf(_("  --inserts                    dump data as INSERT commands, rather than COPY\n"));
	printf(_("  --load-via-partition-root    load partitions via the root table\n"));
	printf(_("  --no-comments                do not dump comments\n"));
	printf(_("  --no-publications            do not dump publications\n"));
	printf(_("  --no-role-passwords          do not dump passwords for roles\n"));
	printf(_("  --no-security-labels         do not dump security label assignments\n"));
	printf(_("  --no-subscriptions           do not dump subscriptions\n"));
	printf(_("  --no-sync                    do not wait for changes to be written safely to disk\n"));
	printf(_("  --no-tablespaces             do not dump tablespace assignments\n"));
	printf(_("  --no-unlogged-table-data     do not dump unlogged table data\n"));
	printf(_("  --on-conflict-do-nothing     add ON CONFLICT DO NOTHING to INSERT commands\n"));
	printf(_("  --quote-all-identifiers      quote all identifiers, even if not key words\n"));
	printf(_("  --rows-per-insert=NROWS      number of rows per INSERT; implies --inserts\n"));
	printf(_("  --use-set-session-authorization\n"
			 "                               use SET SESSION AUTHORIZATION commands instead of\n"
			 "                               ALTER OWNER commands to set ownership\n"));
	printf(_("  --gp-syntax                  dump with Greenplum Database syntax (default if gpdb)\n"));
	printf(_("  --no-gp-syntax               dump without Greenplum Database syntax (default if postgresql)\n"));

	printf(_("\nConnection options:\n"));
	printf(_("  -d, --dbname=CONNSTR     connect using connection string\n"));
	printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
	printf(_("  -l, --database=DBNAME    alternative default database\n"));
	printf(_("  -p, --port=PORT          database server port number\n"));
	printf(_("  -U, --username=NAME      connect as specified database user\n"));
	printf(_("  -w, --no-password        never prompt for password\n"));
	printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
	printf(_("  --role=ROLENAME          do SET ROLE before dump\n"));

	printf(_("\nIf -f/--file is not used, then the SQL script will be written to the standard\n"
			 "output.\n\n"));
	printf(_("Report bugs to <bugs@greenplum.org>.\n"));
}


/*
 * Build the WITH clause for resource queue dump
 */
static void 
buildWithClause(const char *resname, const char *ressetting, PQExpBuffer buf)
{
	if (0 == strncmp("memory_limit", resname, 12) && (strncmp(ressetting, "-1", 2) != 0))
        	appendPQExpBuffer(buf, " %s='%s'", resname, ressetting);
	else
		appendPQExpBuffer(buf, " %s=%s", resname, ressetting);
}

/*
 * Dump resource group
 */
static void
dumpResGroups(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int		i;
	int		i_groupname,
			i_cpu_rate_limit,
			i_concurrency,
			i_memory_limit,
			i_memory_shared_quota,
			i_memory_spill_ratio,
			i_memory_auditor,
			i_cpuset;

	printfPQExpBuffer(buf, "SELECT g.rsgname AS groupname, "
					  "t1.value AS concurrency, "
					  "t2.value AS cpu_rate_limit, "
					  "t3.value AS memory_limit, "
					  "t4.value AS memory_shared_quota, "
					  "t5.value AS memory_spill_ratio, "
					  "t6.value AS memory_auditor, "
					  "t7.value AS cpuset "
					  "FROM pg_resgroup g "
					  "     JOIN pg_resgroupcapability t1 ON g.oid = t1.resgroupid AND t1.reslimittype = 1 "
					  "     JOIN pg_resgroupcapability t2 ON g.oid = t2.resgroupid AND t2.reslimittype = 2 "
					  "     JOIN pg_resgroupcapability t3 ON g.oid = t3.resgroupid AND t3.reslimittype = 3 "
					  "     JOIN pg_resgroupcapability t4 ON g.oid = t4.resgroupid AND t4.reslimittype = 4 "
					  "     JOIN pg_resgroupcapability t5 ON g.oid = t5.resgroupid AND t5.reslimittype = 5 "
					  "LEFT JOIN pg_resgroupcapability t6 ON g.oid = t6.resgroupid AND t6.reslimittype = 6 "
					  "LEFT JOIN pg_resgroupcapability t7 ON g.oid = t7.resgroupid AND t7.reslimittype = 7 "
					  ";");

	res = executeQuery(conn, buf->data);

	i_groupname = PQfnumber(res, "groupname");
	i_cpu_rate_limit = PQfnumber(res, "cpu_rate_limit");
	i_concurrency = PQfnumber(res, "concurrency");
	i_memory_limit = PQfnumber(res, "memory_limit");
	i_memory_shared_quota = PQfnumber(res, "memory_shared_quota");
	i_memory_spill_ratio = PQfnumber(res, "memory_spill_ratio");
	i_memory_auditor = PQfnumber(res, "memory_auditor");
	i_cpuset = PQfnumber(res, "cpuset");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Resource Group\n--\n\n");

	/*
	 * total cpu_rate_limit and memory_limit should less than 100, so clean
	 * them before we seting new memory_limit and cpu_rate_limit.
	 */
	fprintf(OPF, "ALTER RESOURCE GROUP admin_group SET cpu_rate_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP default_group SET cpu_rate_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP admin_group SET memory_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP default_group SET memory_limit 1;\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *groupname;
		const char *cpu_rate_limit;
		const char *concurrency;
		const char *memory_limit;
		const char *memory_shared_quota;
		const char *memory_spill_ratio;
		const char *memory_auditor;
		const char *cpuset;

		groupname = fmtId(PQgetvalue(res, i, i_groupname));
		cpu_rate_limit = PQgetvalue(res, i, i_cpu_rate_limit);
		concurrency = PQgetvalue(res, i, i_concurrency);
		memory_limit = PQgetvalue(res, i, i_memory_limit);
		memory_shared_quota = PQgetvalue(res, i, i_memory_shared_quota);
		memory_spill_ratio = PQgetvalue(res, i, i_memory_spill_ratio);
		memory_auditor = PQgetvalue(res, i, i_memory_auditor);
		cpuset = PQgetvalue(res, i, i_cpuset);

		resetPQExpBuffer(buf);

		/* DROP or CREATE default group, so ALTER it  */
		if (0 == strcmp(groupname, "default_group") || 0 == strcmp(groupname, "admin_group"))
		{
			/*
			 * Default resource groups must have memory_auditor == "vmtracker",
			 * no need to ALTER it, and we do not support ALTER memory_auditor
			 * at all.
			 */
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET concurrency %s;\n",
							  groupname, concurrency);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_limit %s;\n",
							  groupname, memory_limit);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_shared_quota %s;\n",
							  groupname, memory_shared_quota);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_spill_ratio %s;\n",
							  groupname, memory_spill_ratio);
			if (atoi(cpu_rate_limit) >= 0)
				appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET cpu_rate_limit %s;\n",
								  groupname, cpu_rate_limit);
			else
				appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET cpuset '%s';\n",
								  groupname, cpuset);
		}
		else
		{
			const char *memory_auditor_name;
			const char *cpu_prop;
			char cpu_setting[1024];

			/*
			 * Possible values of memory_auditor:
			 * - "1": cgroup;
			 * - "0": vmtracker;
			 * - "": not set, e.g. created on an older version which does not
			 *   support memory_auditor yet, consider it as vmtracker;
			 */
			if (strcmp(memory_auditor, "1") == 0)
				memory_auditor_name = "cgroup";
			else
				memory_auditor_name = "vmtracker";

			if (atoi(cpu_rate_limit) >= 0)
			{
				cpu_prop = "cpu_rate_limit";
				snprintf(cpu_setting, sizeof(cpu_setting), "%s", cpu_rate_limit);
			}
			else
			{
				cpu_prop = "cpuset";
				snprintf(cpu_setting, sizeof(cpu_setting), "'%s'", cpuset);
			}

			printfPQExpBuffer(buf, "CREATE RESOURCE GROUP %s WITH ("
							  "concurrency=%s, %s=%s, "
							  "memory_limit=%s, memory_shared_quota=%s, "
							  "memory_spill_ratio=%s, memory_auditor=%s);\n",
							  groupname, concurrency, cpu_prop, cpu_setting,
							  memory_limit, memory_shared_quota,
							  memory_spill_ratio, memory_auditor_name);
		}

		fprintf(OPF, "%s", buf->data);
	}

	PQclear(res);

	destroyPQExpBuffer(buf);

	fprintf(OPF, "\n\n");
}

/*
 * Dump resource queues
 */
static void
dumpResQueues(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int			i_rsqname,
				i_resname,
				i_ressetting,
				i_rqoid;
	int			i;
	char	   *prev_rsqname = NULL;
	bool		bWith = false;

	printfPQExpBuffer(buf,
					  "SELECT oid, rsqname, 'activelimit' as resname, "
					  "rsqcountlimit::text as ressetting, "
					  "1 as ord FROM pg_resqueue "
					  "UNION "
					  "SELECT oid, rsqname, 'costlimit' as resname, "
					  "rsqcostlimit::text as ressetting, "
					  "2 as ord FROM pg_resqueue "
					  "UNION "
					  "SELECT oid, rsqname, 'overcommit' as resname, "
					  "case when rsqovercommit then '1' "
					  "else '0' end as ressetting, "
					  "3 as ord FROM pg_resqueue "
					  "UNION "
					  "SELECT oid, rsqname, 'ignorecostlimit' as resname, "
					  "%s as ressetting, "
					  "4 as ord FROM pg_resqueue "
					  "%s"
					  "order by rsqname,  ord",
					  (server_version >= 80205 ?
					   "rsqignorecostlimit::text"
					   : "0 AS"),
					  (server_version >= 80214 ?
					   "UNION "
					   "SELECT rq.oid, rq.rsqname, rt.resname, rc.ressetting, "
					   "rt.restypid as ord FROM "
					   "pg_resqueue rq,  pg_resourcetype rt, "
					   "pg_resqueuecapability rc WHERE "
					   "rq.oid=rc.resqueueid and rc.restypid = rt.restypid "
					   : "")
		);

	res = executeQuery(conn, buf->data);

	i_rqoid = PQfnumber(res, "oid");
	i_rsqname = PQfnumber(res, "rsqname");
	i_resname = PQfnumber(res, "resname");
	i_ressetting = PQfnumber(res, "ressetting");

	if (PQntuples(res) > 0)
	    fprintf(OPF, "--\n-- Resource Queues\n--\n\n");


	/*
	 * settings for resource queue are spread over multiple rows, but sorted
	 * by queue name (and ranked in order of resname ) eg:
	 *
	 * rsqname	  |		resname		| ressetting | ord
	 * -----------+-----------------+------------+----- pg_default |
	 * activelimit	   | 20			|	1 pg_default | costlimit	   | -1 |
	 * 2 pg_default | overcommit	  | 0		   |   3 pg_default |
	 * ignorecostlimit | 0			|	4
	 *
	 * This format lets us support an arbitrary number of resqueuecapability
	 * entries.  So watch for change of rsqname to switch to next CREATE
	 * statement.
	 *
	 */

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *rsqname;
		const char *resname;
		const char *ressetting;

		rsqname = PQgetvalue(res, i, i_rsqname);
		resname = PQgetvalue(res, i, i_resname);
		ressetting = PQgetvalue(res, i, i_ressetting);

		/* if first CREATE statement, or name changed... */
		if (!prev_rsqname || (0 != strcmp(rsqname, prev_rsqname)))
		{
			if (prev_rsqname)
			{
				/* terminate the WITH if necessary */
				if (bWith)
					appendPQExpBuffer(buf, ") ");

				appendPQExpBuffer(buf, ";\n");

				fprintf(OPF, "%s", buf->data);

				free(prev_rsqname);
			}

			bWith = false;

			/* save the name */
			prev_rsqname = strdup(rsqname);

			resetPQExpBuffer(buf);

			/* MPP-6926: cannot DROP or CREATE default queue, so ALTER it  */
			if (0 == strcmp(rsqname, "pg_default"))
				appendPQExpBuffer(buf, "ALTER RESOURCE QUEUE %s", fmtId(rsqname));
			else
				appendPQExpBuffer(buf, "CREATE RESOURCE QUEUE %s", fmtId(rsqname));
		}

		/* NOTE: currently 3.3-style, but will switch to one WITH clause... */

		if (0 == strcmp("activelimit", resname))
		{
			if (strcmp(ressetting, "-1") != 0)
				appendPQExpBuffer(buf, " ACTIVE THRESHOLD %s",
								  ressetting);
		}
		else if (0 == strcmp("costlimit", resname))
		{
			if (strcmp(ressetting, "-1") != 0)
				appendPQExpBuffer(buf, " COST THRESHOLD %.2f",
								  atof(ressetting));
		}
		else if (0 == strcmp("ignorecostlimit", resname))
		{
			if (!((strcmp(ressetting, "-1") == 0)
				  || (strcmp(ressetting, "0") == 0)))
				appendPQExpBuffer(buf, " IGNORE THRESHOLD %.2f",
								  atof(ressetting));
		}
		else if (0 == strcmp("overcommit", resname))
		{
			if (strcmp(ressetting, "1") == 0)
				appendPQExpBuffer(buf, " OVERCOMMIT");
			else
				appendPQExpBuffer(buf, " NOOVERCOMMIT");
		}
		else
		{
			/* build the WITH clause */
			if (bWith)
				appendPQExpBuffer(buf, ",\n");
			else
			{
				bWith = true;
				appendPQExpBuffer(buf, "\n WITH (");
			}

			buildWithClause(resname, ressetting, buf);

		}

	}							/* end for */

	/* need to write out last statement */
	if (prev_rsqname)
	{
		/* terminate the WITH if necessary */
		if (bWith)
			appendPQExpBuffer(buf, ") ");

		appendPQExpBuffer(buf, ";\n");

		fprintf(OPF, "%s", buf->data);

		free(prev_rsqname);
	}

	PQclear(res);

	destroyPQExpBuffer(buf);

	fprintf(OPF, "\n\n");
}

/*
 * Drop roles
 */
static void
dropRoles(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int			i_rolname;
	int			i;

	if (server_version >= 90600)
		printfPQExpBuffer(buf,
						  "SELECT rolname "
						  "FROM %s "
						  "WHERE rolname !~ '^pg_' "
						  "ORDER BY 1", role_catalog);
	else if (server_version >= 80100)
		printfPQExpBuffer(buf,
						  "SELECT rolname "
						  "FROM %s "
						  "ORDER BY 1", role_catalog);
	else
		printfPQExpBuffer(buf,
						  "SELECT usename as rolname "
						  "FROM pg_shadow "
						  "UNION "
						  "SELECT groname as rolname "
						  "FROM pg_group "
						  "ORDER BY 1");

	res = executeQuery(conn, buf->data);

	i_rolname = PQfnumber(res, "rolname");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Drop roles\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *rolename;

		rolename = PQgetvalue(res, i, i_rolname);

		fprintf(OPF, "DROP ROLE %s%s;\n",
				if_exists ? "IF EXISTS " : "",
				fmtId(rolename));
	}

	PQclear(res);
	destroyPQExpBuffer(buf);

	fprintf(OPF, "\n\n");
}

/*
 * Dump roles
 */
static void
dumpRoles(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int			i_oid,
				i_rolname,
				i_rolsuper,
				i_rolinherit,
				i_rolcreaterole,
				i_rolcreatedb,
				i_rolcanlogin,
				i_rolconnlimit,
				i_rolpassword,
				i_rolvaliduntil,
				i_rolreplication,
				i_rolbypassrls,
				i_rolcomment,
				i_rolqueuename = -1,	/* keep compiler quiet */
				i_rolgroupname = -1,	/* keep compiler quiet */
				i_rolcreaterextgpfd = -1,
				i_rolcreaterexthttp = -1,
				i_rolcreatewextgpfd = -1,
				i_rolcreaterexthdfs = -1,
				i_rolcreatewexthdfs = -1,
				i_is_current_user;
	int			i;
	bool		exttab_auth = (server_version >= 80214);
	/*
	 * Support for gphdfs was removed in Greenplum 6
	 */
	bool		hdfs_auth = (server_version >= 80215 && server_version < 80400);
	char	   *resq_col = resource_queues ? ", (SELECT rsqname FROM pg_resqueue WHERE "
	"  pg_resqueue.oid = rolresqueue) AS rolqueuename " : "";
	char	   *resgroup_col = resource_groups ? ", (SELECT rsgname FROM pg_resgroup WHERE "
	"  pg_resgroup.oid = rolresgroup) AS rolgroupname " : "";
	char	   *extauth_col = exttab_auth ? ", rolcreaterextgpfd, rolcreaterexthttp, rolcreatewextgpfd" : "";
	char	   *hdfs_col = hdfs_auth ? ", rolcreaterexthdfs, rolcreatewexthdfs " : "";

	/*
	 * Query to select role info get resqueue if version support it get
	 * external table auth on gpfdist, gpfdists and http if version support it get
	 */

	/* note: rolconfig is dumped later */
	if (server_version >= 90600)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, rolreplication, rolbypassrls, "
						  "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
						  "rolname = current_user AS is_current_user "
						  " %s %s %s %s"
						  "FROM %s "
						  "WHERE rolname !~ '^pg_' "
						  "ORDER BY 2", role_catalog, resq_col, resgroup_col, extauth_col, hdfs_col, role_catalog);
	else if (server_version >= 90500)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, rolreplication, rolbypassrls, "
						  "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
						  "rolname = current_user AS is_current_user "
						  " %s %s %s %s"
						  "FROM %s "
						  "ORDER BY 2", role_catalog, resq_col, resgroup_col, extauth_col, hdfs_col, role_catalog);
	else if (server_version >= 90100)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, rolcatupdate, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, rolreplication, "
						  "false as rolbypassrls, "
						  "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
						  "rolname = current_user AS is_current_user "
						  " %s %s %s %s"
						  "FROM %s "
						  "ORDER BY 2", role_catalog, resq_col, resgroup_col, extauth_col, hdfs_col, role_catalog);
	else if (server_version >= 80200)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, rolcatupdate, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, false as rolreplication, "
						  "false as rolbypassrls, "
						  "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
						  "rolname = current_user AS is_current_user "
						  " %s %s %s %s"
						  "FROM %s "
						  "ORDER BY 2", role_catalog, resq_col, resgroup_col, extauth_col, hdfs_col, role_catalog);
	else if (server_version >= 80100)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, false as rolreplication, "
						  "false as rolbypassrls, "
						  "null as rolcomment, "
						  "rolname = current_user AS is_current_user "
						  "FROM %s "
						  "ORDER BY 2", role_catalog);
	else
		printfPQExpBuffer(buf,
						  "SELECT 0 as oid, usename as rolname, "
						  "usesuper as rolsuper, "
						  "true as rolinherit, "
						  "usesuper as rolcreaterole, "
						  "usecreatedb as rolcreatedb, "
						  "true as rolcanlogin, "
						  "-1 as rolconnlimit, "
						  "passwd as rolpassword, "
						  "valuntil as rolvaliduntil, "
						  "false as rolreplication, "
						  "false as rolbypassrls, "
						  "null as rolcomment, "
						  "usename = current_user AS is_current_user "
						  "FROM pg_shadow "
						  "UNION ALL "
						  "SELECT 0 as oid, groname as rolname, "
						  "false as rolsuper, "
						  "true as rolinherit, "
						  "false as rolcreaterole, "
						  "false as rolcreatedb, "
						  "false as rolcanlogin, "
						  "-1 as rolconnlimit, "
						  "null::text as rolpassword, "
						  "null::timestamptz as rolvaliduntil, "
						  "false as rolreplication, "
						  "false as rolbypassrls, "
						  "null as rolcomment, "
						  "false AS is_current_user "
						  "FROM pg_group "
						  "WHERE NOT EXISTS (SELECT 1 FROM pg_shadow "
						  " WHERE usename = groname) "
						  "ORDER BY 2");

	res = executeQuery(conn, buf->data);

	i_oid = PQfnumber(res, "oid");
	i_rolname = PQfnumber(res, "rolname");
	i_rolsuper = PQfnumber(res, "rolsuper");
	i_rolinherit = PQfnumber(res, "rolinherit");
	i_rolcreaterole = PQfnumber(res, "rolcreaterole");
	i_rolcreatedb = PQfnumber(res, "rolcreatedb");
	i_rolcanlogin = PQfnumber(res, "rolcanlogin");
	i_rolconnlimit = PQfnumber(res, "rolconnlimit");
	i_rolpassword = PQfnumber(res, "rolpassword");
	i_rolvaliduntil = PQfnumber(res, "rolvaliduntil");
	i_rolreplication = PQfnumber(res, "rolreplication");
	i_rolbypassrls = PQfnumber(res, "rolbypassrls");
	i_rolcomment = PQfnumber(res, "rolcomment");
	i_is_current_user = PQfnumber(res, "is_current_user");

	if (resource_queues)
		i_rolqueuename = PQfnumber(res, "rolqueuename");

	if (resource_groups)
		i_rolgroupname = PQfnumber(res, "rolgroupname");

	if (exttab_auth)
	{
		i_rolcreaterextgpfd = PQfnumber(res, "rolcreaterextgpfd");
		i_rolcreaterexthttp = PQfnumber(res, "rolcreaterexthttp");
		i_rolcreatewextgpfd = PQfnumber(res, "rolcreatewextgpfd");
		if (hdfs_auth)
		{
			i_rolcreaterexthdfs = PQfnumber(res, "rolcreaterexthdfs");
			i_rolcreatewexthdfs = PQfnumber(res, "rolcreatewexthdfs");
		}
	}

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Roles\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *rolename;
		Oid			auth_oid;

		auth_oid = atooid(PQgetvalue(res, i, i_oid));
		rolename = PQgetvalue(res, i, i_rolname);

		if (strncmp(rolename, "pg_", 3) == 0)
		{
			pg_log_warning("role name starting with \"pg_\" skipped (%s)",
						   rolename);
			continue;
		}

		resetPQExpBuffer(buf);

		if (binary_upgrade)
		{
			appendPQExpBufferStr(buf, "\n-- For binary upgrade, must preserve pg_authid.oid\n");
			appendPQExpBuffer(buf,
							  "SELECT pg_catalog.binary_upgrade_set_next_pg_authid_oid('%u'::pg_catalog.oid, '%s'::text);\n\n",
							  auth_oid, rolename);
		}

		/*
		 * We dump CREATE ROLE followed by ALTER ROLE to ensure that the role
		 * will acquire the right properties even if it already exists (ie, it
		 * won't hurt for the CREATE to fail).  This is particularly important
		 * for the role we are connected as, since even with --clean we will
		 * have failed to drop it.  binary_upgrade cannot generate any errors,
		 * so we assume the current role is already created.
		 */
		if (!binary_upgrade ||
			strcmp(PQgetvalue(res, i, i_is_current_user), "f") == 0)
			appendPQExpBuffer(buf, "CREATE ROLE %s;\n", fmtId(rolename));
		appendPQExpBuffer(buf, "ALTER ROLE %s WITH", fmtId(rolename));

		if (strcmp(PQgetvalue(res, i, i_rolsuper), "t") == 0)
			appendPQExpBufferStr(buf, " SUPERUSER");
		else
			appendPQExpBufferStr(buf, " NOSUPERUSER");

		if (strcmp(PQgetvalue(res, i, i_rolinherit), "t") == 0)
			appendPQExpBufferStr(buf, " INHERIT");
		else
			appendPQExpBufferStr(buf, " NOINHERIT");

		if (strcmp(PQgetvalue(res, i, i_rolcreaterole), "t") == 0)
			appendPQExpBufferStr(buf, " CREATEROLE");
		else
			appendPQExpBufferStr(buf, " NOCREATEROLE");

		if (strcmp(PQgetvalue(res, i, i_rolcreatedb), "t") == 0)
			appendPQExpBufferStr(buf, " CREATEDB");
		else
			appendPQExpBufferStr(buf, " NOCREATEDB");

		if (strcmp(PQgetvalue(res, i, i_rolcanlogin), "t") == 0)
			appendPQExpBufferStr(buf, " LOGIN");
		else
			appendPQExpBufferStr(buf, " NOLOGIN");

		if (strcmp(PQgetvalue(res, i, i_rolreplication), "t") == 0)
			appendPQExpBufferStr(buf, " REPLICATION");
		else
			appendPQExpBufferStr(buf, " NOREPLICATION");

		if (strcmp(PQgetvalue(res, i, i_rolbypassrls), "t") == 0)
			appendPQExpBufferStr(buf, " BYPASSRLS");
		else
			appendPQExpBufferStr(buf, " NOBYPASSRLS");

		if (strcmp(PQgetvalue(res, i, i_rolconnlimit), "-1") != 0)
			appendPQExpBuffer(buf, " CONNECTION LIMIT %s",
							  PQgetvalue(res, i, i_rolconnlimit));


		if (!PQgetisnull(res, i, i_rolpassword) && !no_role_passwords)
		{
			appendPQExpBufferStr(buf, " PASSWORD ");
			appendStringLiteralConn(buf, PQgetvalue(res, i, i_rolpassword), conn);
		}

		if (!PQgetisnull(res, i, i_rolvaliduntil))
			appendPQExpBuffer(buf, " VALID UNTIL '%s'",
							  PQgetvalue(res, i, i_rolvaliduntil));

		if (resource_queues)
		{
			if (!PQgetisnull(res, i, i_rolqueuename))
				appendPQExpBuffer(buf, " RESOURCE QUEUE %s",
								  PQgetvalue(res, i, i_rolqueuename));
		}

		if (resource_groups)
		{
			if (!PQgetisnull(res, i, i_rolgroupname))
				appendPQExpBuffer(buf, " RESOURCE GROUP %s",
								  PQgetvalue(res, i, i_rolgroupname));
		}

		if (exttab_auth)
		{
			/* we use the same privilege for gpfdist and gpfdists */
			if (!PQgetisnull(res, i, i_rolcreaterextgpfd) &&
				strcmp(PQgetvalue(res, i, i_rolcreaterextgpfd), "t") == 0)
				appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gpfdist', type='readable')");

			if (!PQgetisnull(res, i, i_rolcreatewextgpfd) &&
				strcmp(PQgetvalue(res, i, i_rolcreatewextgpfd), "t") == 0)
				appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gpfdist', type='writable')");

			if (!PQgetisnull(res, i, i_rolcreaterexthttp) &&
				strcmp(PQgetvalue(res, i, i_rolcreaterexthttp), "t") == 0)
				appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='http')");

			if (hdfs_auth)
			{
				if (!PQgetisnull(res, i, i_rolcreaterexthdfs) &&
					strcmp(PQgetvalue(res, i, i_rolcreaterexthdfs), "t") == 0)
					appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gphdfs', type='readable')");

				if (!PQgetisnull(res, i, i_rolcreatewexthdfs) &&
					strcmp(PQgetvalue(res, i, i_rolcreatewexthdfs), "t") == 0)
					appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gphdfs', type='writable')");
			}
		}

		appendPQExpBufferStr(buf, ";\n");

		if (!no_comments && !PQgetisnull(res, i, i_rolcomment))
		{
			appendPQExpBuffer(buf, "COMMENT ON ROLE %s IS ", fmtId(rolename));
			appendStringLiteralConn(buf, PQgetvalue(res, i, i_rolcomment), conn);
			appendPQExpBufferStr(buf, ";\n");
		}

		if (!no_security_labels && server_version >= 90200)
			buildShSecLabels(conn, "pg_authid", auth_oid,
							 "ROLE", rolename,
							 buf);

		fprintf(OPF, "%s", buf->data);
	}

	/*
	 * Dump configuration settings for roles after all roles have been dumped.
	 * We do it this way because config settings for roles could mention the
	 * names of other roles.
	 */
	for (i = 0; i < PQntuples(res); i++)
		dumpUserConfig(conn, PQgetvalue(res, i, i_rolname));

	PQclear(res);

	fprintf(OPF, "\n\n");

	destroyPQExpBuffer(buf);
}


/*
 * Dump role memberships.  This code is used for 8.1 and later servers.
 *
 * Note: we expect dumpRoles already created all the roles, but there is
 * no membership yet.
 */
static void
dumpRoleMembership(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int			i;

	printfPQExpBuffer(buf, "SELECT ur.rolname AS roleid, "
					  "um.rolname AS member, "
					  "a.admin_option, "
					  "ug.rolname AS grantor "
					  "FROM pg_auth_members a "
					  "LEFT JOIN %s ur on ur.oid = a.roleid "
					  "LEFT JOIN %s um on um.oid = a.member "
					  "LEFT JOIN %s ug on ug.oid = a.grantor "
					  "WHERE NOT (ur.rolname ~ '^pg_' AND um.rolname ~ '^pg_')"
					  "ORDER BY 1,2,3", role_catalog, role_catalog, role_catalog);
	res = executeQuery(conn, buf->data);

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Role memberships\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *roleid = PQgetvalue(res, i, 0);
		char	   *member = PQgetvalue(res, i, 1);
		char	   *option = PQgetvalue(res, i, 2);

		fprintf(OPF, "GRANT %s", fmtId(roleid));
		fprintf(OPF, " TO %s", fmtId(member));
		if (*option == 't')
			fprintf(OPF, " WITH ADMIN OPTION");

		/*
		 * We don't track the grantor very carefully in the backend, so cope
		 * with the possibility that it has been dropped.
		 */
		if (!PQgetisnull(res, i, 3))
		{
			char	   *grantor = PQgetvalue(res, i, 3);

			fprintf(OPF, " GRANTED BY %s", fmtId(grantor));
		}
		fprintf(OPF, ";\n");
	}

	PQclear(res);
	destroyPQExpBuffer(buf);

	fprintf(OPF, "\n\n");
}

/*
 * Dump role time constraints. 
 *
 * Note: we expect dumpRoles already created all the roles, but there are
 * no time constraints yet.
 */
static void
dumpRoleConstraints(PGconn *conn)
{
	PGresult   *res;
	int 		i;

	res = executeQuery(conn, "SELECT a.rolname, c.start_day, c.start_time, c.end_day, c.end_time "
							 "FROM pg_authid a, pg_auth_time_constraint c "
							 "WHERE a.oid = c.authid "
							 "ORDER BY 1");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Role time constraints\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char		*rolname 	= PQgetvalue(res, i, 0);
		char		*start_day 	= PQgetvalue(res, i, 1);
		char 		*start_time = PQgetvalue(res, i, 2);
		char		*end_day 	= PQgetvalue(res, i, 3);
		char 		*end_time 	= PQgetvalue(res, i, 4);

		fprintf(OPF, "ALTER ROLE %s DENY BETWEEN DAY %s TIME '%s' AND DAY %s TIME '%s';\n", 
				fmtId(rolname), start_day, start_time, end_day, end_time);
	}

	PQclear(res);

	fprintf(OPF, "\n\n");
}


/*
 * Drop tablespaces.
 */
static void
dropTablespaces(PGconn *conn)
{
	PGresult   *res;
	int			i;

	/*
	 * Get all tablespaces except built-in ones (which we assume are named
	 * pg_xxx)
	 */
	res = executeQuery(conn, "SELECT spcname "
					   "FROM pg_catalog.pg_tablespace "
					   "WHERE spcname !~ '^pg_' "
					   "ORDER BY 1");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Drop tablespaces\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *spcname = PQgetvalue(res, i, 0);

		fprintf(OPF, "DROP TABLESPACE %s%s;\n",
				if_exists ? "IF EXISTS " : "",
				fmtId(spcname));
	}

	PQclear(res);

	fprintf(OPF, "\n\n");
}

/*
 * Dump tablespaces.
 */
static void
dumpTablespaces(PGconn *conn)
{
	PGresult   *res;
	int			i;

	// WALREP_FIXME: filespaces are gone. How do we deal with that here?
	
	/*
	 * Get all tablespaces except built-in ones (which we assume are named
	 * pg_xxx)
	 *
	 * [FIXME] the queries need to be slightly different if the backend isn't
	 * Greenplum, and the dump format should vary depending on if the dump is
	 * --gp-syntax or --no-gp-syntax.
	 *
	 * For the tablespace ACLs, as of 9.6, we extract both the positive (as
	 * spcacl) and negative (as rspcacl) ACLs, relative to the default ACL for
	 * tablespaces, which are then passed to buildACLCommands() below.
	 *
	 * See buildACLQueries() and buildACLCommands().
	 *
	 * The order in which privileges are in the ACL string (the order they
	 * have been GRANT'd in, which the backend maintains) must be preserved to
	 * ensure that GRANTs WITH GRANT OPTION and subsequent GRANTs based on
	 * those are dumped in the correct order.
	 *
	 * Note that we do not support initial privileges (pg_init_privs) on
	 * tablespaces, so this logic cannot make use of buildACLQueries().
	 */
	if (server_version < 80214)
	{
		/* Filespaces were introduced in GP 4.0 (server_version 8.2.14) */
		return;
	}

	if (server_version >= 90600)
		res = executeQuery(conn, "SELECT oid, spcname, "
						   "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "pg_catalog.pg_tablespace_location(oid), "
						   "(SELECT array_agg(acl ORDER BY row_n) FROM "
						   "  (SELECT acl, row_n FROM "
						   "     unnest(coalesce(spcacl,acldefault('t',spcowner))) "
						   "     WITH ORDINALITY AS perm(acl,row_n) "
						   "   WHERE NOT EXISTS ( "
						   "     SELECT 1 "
						   "     FROM unnest(acldefault('t',spcowner)) "
						   "       AS init(init_acl) "
						   "     WHERE acl = init_acl)) AS spcacls) "
						   " AS spcacl, "
						   "(SELECT array_agg(acl ORDER BY row_n) FROM "
						   "  (SELECT acl, row_n FROM "
						   "     unnest(acldefault('t',spcowner)) "
						   "     WITH ORDINALITY AS initp(acl,row_n) "
						   "   WHERE NOT EXISTS ( "
						   "     SELECT 1 "
						   "     FROM unnest(coalesce(spcacl,acldefault('t',spcowner))) "
						   "       AS permp(orig_acl) "
						   "     WHERE acl = orig_acl)) AS rspcacls) "
						   " AS rspcacl, "
						   "array_to_string(spcoptions, ', '),"
						   "pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else if (server_version >= 90200)
		res = executeQuery(conn, "SELECT oid, spcname, "
						   "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "pg_catalog.pg_tablespace_location(oid), "
						   "spcacl, '' as rspcacl, "
						   "array_to_string(spcoptions, ', '),"
						   "pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else if (server_version >= 90000)
		res = executeQuery(conn, "SELECT oid, spcname, "
						   "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "spclocation, spcacl, '' as rspcacl, "
						   "array_to_string(spcoptions, ', '),"
						   "pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else if (server_version >= 80200)
		res = executeQuery(conn, "SELECT oid, spcname, "
						   "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "spclocation, spcacl, '' as rspcacl, null, "
						   "pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else
	{
		error_unsupported_server_version(conn);
		res = executeQuery(conn, "SELECT oid, spcname, "
						   "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "spclocation, spcacl, '' as rspcacl, "
						   "null, null "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	}

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Tablespaces\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		PQExpBuffer buf = createPQExpBuffer();
		Oid			spcoid = atooid(PQgetvalue(res, i, 0));
		char	   *spcname = PQgetvalue(res, i, 1);
		char	   *spcowner = PQgetvalue(res, i, 2);
		char	   *spclocation = PQgetvalue(res, i, 3);
		char	   *spcacl = PQgetvalue(res, i, 4);
		char	   *rspcacl = PQgetvalue(res, i, 5);
		char	   *spcoptions = PQgetvalue(res, i, 6);
		char	   *spccomment = PQgetvalue(res, i, 7);
		char	   *fspcname;

		/* needed for buildACLCommands() */
		fspcname = pg_strdup(fmtId(spcname));

		appendPQExpBuffer(buf, "CREATE TABLESPACE %s", spcname);
		appendPQExpBuffer(buf, " OWNER %s", fmtId(spcowner));

		appendPQExpBufferStr(buf, " LOCATION ");
		appendStringLiteralConn(buf, spclocation, conn);
		appendPQExpBufferStr(buf, ";\n");

		if (spcoptions && spcoptions[0] != '\0')
			appendPQExpBuffer(buf, "ALTER TABLESPACE %s SET (%s);\n",
							  fspcname, spcoptions);

		if (!skip_acls &&
			!buildACLCommands(fspcname, NULL, NULL, "TABLESPACE",
							  spcacl, rspcacl,
							  spcowner, "", server_version, buf))
		{
			pg_log_error("could not parse ACL list (%s) for tablespace \"%s\"",
						 spcacl, spcname);
			PQfinish(conn);
			exit_nicely(1);
		}

		if (!no_comments && spccomment && spccomment[0] != '\0')
		{
			appendPQExpBuffer(buf, "COMMENT ON TABLESPACE %s IS ", fspcname);
			appendStringLiteralConn(buf, spccomment, conn);
			appendPQExpBufferStr(buf, ";\n");
		}

		if (!no_security_labels && server_version >= 90200)
			buildShSecLabels(conn, "pg_tablespace", spcoid,
							 "TABLESPACE", spcname,
							 buf);

		fprintf(OPF, "%s", buf->data);

		free(fspcname);
		destroyPQExpBuffer(buf);
	}

	PQclear(res);
	fprintf(OPF, "\n\n");
}


/*
 * Dump commands to drop each database.
 */
static void
dropDBs(PGconn *conn)
{
	PGresult   *res;
	int			i;

	/*
	 * Skip databases marked not datallowconn, since we'd be unable to connect
	 * to them anyway.  This must agree with dumpDatabases().
	 */
	res = executeQuery(conn,
					   "SELECT datname "
					   "FROM pg_database d "
					   "WHERE datallowconn "
					   "ORDER BY datname");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Drop databases (except postgres and template1)\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *dbname = PQgetvalue(res, i, 0);

		/*
		 * Skip "postgres" and "template1"; dumpDatabases() will deal with
		 * them specially.  Also, be sure to skip "template0", even if for
		 * some reason it's not marked !datallowconn.
		 */
		if (strcmp(dbname, "template1") != 0 &&
			strcmp(dbname, "template0") != 0 &&
			strcmp(dbname, "postgres") != 0)
		{
			fprintf(OPF, "DROP DATABASE %s%s;\n",
					if_exists ? "IF EXISTS " : "",
					fmtId(dbname));
		}
	}

	PQclear(res);

	fprintf(OPF, "\n\n");
}


/*
 * Dump user-specific configuration
 */
static void
dumpUserConfig(PGconn *conn, const char *username)
{
	PQExpBuffer buf = createPQExpBuffer();
	int			count = 1;
	bool		first = true;

	for (;;)
	{
		PGresult   *res;

		if (server_version >= 90000)
			printfPQExpBuffer(buf, "SELECT setconfig[%d] FROM pg_db_role_setting WHERE "
							  "setdatabase = 0 AND setrole = "
							  "(SELECT oid FROM %s WHERE rolname = ", count, role_catalog);
		else if (server_version >= 80100)
			printfPQExpBuffer(buf, "SELECT rolconfig[%d] FROM %s WHERE rolname = ", count, role_catalog);
		else
			printfPQExpBuffer(buf, "SELECT useconfig[%d] FROM pg_shadow WHERE usename = ", count);
		appendStringLiteralConn(buf, username, conn);
		if (server_version >= 90000)
			appendPQExpBufferChar(buf, ')');

		res = executeQuery(conn, buf->data);
		if (PQntuples(res) == 1 &&
			!PQgetisnull(res, 0, 0))
		{
			/* comment at section start, only if needed */
			if (first)
			{
				fprintf(OPF, "--\n-- User Configurations\n--\n\n");
				first = false;
			}

			fprintf(OPF, "--\n-- User Config \"%s\"\n--\n\n", username);
			resetPQExpBuffer(buf);
			makeAlterConfigCommand(conn, PQgetvalue(res, 0, 0),
								   "ROLE", username, NULL, NULL,
								   buf);
			fprintf(OPF, "%s", buf->data);
			PQclear(res);
			count++;
		}
		else
		{
			PQclear(res);
			break;
		}
	}

	destroyPQExpBuffer(buf);
}

/*
 * Find a list of database names that match the given patterns.
 * See also expand_table_name_patterns() in pg_dump.c
 */
static void
expand_dbname_patterns(PGconn *conn,
					   SimpleStringList *patterns,
					   SimpleStringList *names)
{
	PQExpBuffer query;
	PGresult   *res;

	if (patterns->head == NULL)
		return;					/* nothing to do */

	query = createPQExpBuffer();

	/*
	 * The loop below runs multiple SELECTs, which might sometimes result in
	 * duplicate entries in the name list, but we don't care, since all we're
	 * going to do is test membership of the list.
	 */

	for (SimpleStringListCell *cell = patterns->head; cell; cell = cell->next)
	{
		appendPQExpBuffer(query,
						  "SELECT datname FROM pg_catalog.pg_database n\n");
		processSQLNamePattern(conn, query, cell->val, false,
							  false, NULL, "datname", NULL, NULL);

		res = executeQuery(conn, query->data);
		for (int i = 0; i < PQntuples(res); i++)
		{
			simple_string_list_append(names, PQgetvalue(res, i, 0));
		}

		PQclear(res);
		resetPQExpBuffer(query);
	}

	destroyPQExpBuffer(query);
}

/*
 * Dump contents of databases.
 */
static void
dumpDatabases(PGconn *conn)
{
	PGresult   *res;
	int			i;

	/*
	 * Skip databases marked not datallowconn, since we'd be unable to connect
	 * to them anyway.  This must agree with dropDBs().
	 *
	 * We arrange for template1 to be processed first, then we process other
	 * DBs in alphabetical order.  If we just did them all alphabetically, we
	 * might find ourselves trying to drop the "postgres" database while still
	 * connected to it.  This makes trying to run the restore script while
	 * connected to "template1" a bad idea, but there's no fixed order that
	 * doesn't have some failure mode with --clean.
	 */
	res = executeQuery(conn,
					   "SELECT datname "
					   "FROM pg_database d "
					   "WHERE datallowconn "
					   "ORDER BY (datname <> 'template1'), datname");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Databases\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *dbname = PQgetvalue(res, i, 0);
		const char *create_opts;
		int			ret;

		/* Skip template0, even if it's not marked !datallowconn. */
		if (strcmp(dbname, "template0") == 0)
			continue;

		/* Skip any explicitly excluded database */
		if (simple_string_list_member(&database_exclude_names, dbname))
		{
			pg_log_info("excluding database \"%s\"...", dbname);
			continue;
		}

		pg_log_info("dumping database \"%s\"...", dbname);

		fprintf(OPF, "--\n-- Database \"%s\" dump\n--\n\n", dbname);

		/*
		 * We assume that "template1" and "postgres" already exist in the
		 * target installation.  dropDBs() won't have removed them, for fear
		 * of removing the DB the restore script is initially connected to. If
		 * --clean was specified, tell pg_dump to drop and recreate them;
		 * otherwise we'll merely restore their contents.  Other databases
		 * should simply be created.
		 */
		if (strcmp(dbname, "template1") == 0 || strcmp(dbname, "postgres") == 0)
		{
			if (output_clean)
				create_opts = "--clean --create";
			else
			{
				create_opts = "";
				/* Since pg_dump won't emit a \connect command, we must */
				fprintf(OPF, "\\connect %s\n\n", dbname);
			}
		}
		else
			create_opts = "--create";

		if (filename)
			fclose(OPF);

		ret = runPgDump(dbname, create_opts);
		if (ret != 0)
		{
			pg_log_error("pg_dump failed on database \"%s\", exiting", dbname);
			exit_nicely(1);
		}

		if (filename)
		{
			OPF = fopen(filename, PG_BINARY_A);
			if (!OPF)
			{
				pg_log_error("could not re-open the output file \"%s\": %m",
							 filename);
				exit_nicely(1);
			}
		}

	}

	PQclear(res);
}



/*
 * Run pg_dump on dbname, with specified options.
 */
static int
runPgDump(const char *dbname, const char *create_opts)
{
	PQExpBuffer connstrbuf = createPQExpBuffer();
	PQExpBuffer cmd = createPQExpBuffer();
	int			ret;

	appendPQExpBuffer(cmd, "\"%s\" %s %s", pg_dump_bin,
					  pgdumpopts->data, create_opts);

	/*
	 * If we have a filename, use the undocumented plain-append pg_dump
	 * format.
	 */
	if (filename)
		appendPQExpBufferStr(cmd, " -Fa ");
	else
		appendPQExpBufferStr(cmd, " -Fp ");

	/*
	 * Append the database name to the already-constructed stem of connection
	 * string.
	 */
	appendPQExpBuffer(connstrbuf, "%s dbname=", connstr);
	appendConnStrVal(connstrbuf, dbname);

	appendShellString(cmd, connstrbuf->data);

	pg_log_info("running \"%s\"", cmd->data);

	fflush(stdout);
	fflush(stderr);

	ret = system(cmd->data);

	destroyPQExpBuffer(cmd);
	destroyPQExpBuffer(connstrbuf);

	return ret;
}

/*
 * buildShSecLabels
 *
 * Build SECURITY LABEL command(s) for a shared object
 *
 * The caller has to provide object type and identity in two separate formats:
 * catalog_name (e.g., "pg_database") and object OID, as well as
 * type name (e.g., "DATABASE") and object name (not pre-quoted).
 *
 * The command(s) are appended to "buffer".
 */
static void
buildShSecLabels(PGconn *conn, const char *catalog_name, Oid objectId,
				 const char *objtype, const char *objname,
				 PQExpBuffer buffer)
{
	PQExpBuffer sql = createPQExpBuffer();
	PGresult   *res;

	buildShSecLabelQuery(conn, catalog_name, objectId, sql);
	res = executeQuery(conn, sql->data);
	emitShSecLabels(conn, res, buffer, objtype, objname);

	PQclear(res);
	destroyPQExpBuffer(sql);
}

/*
 * Make a database connection with the given parameters.  An
 * interactive password prompt is automatically issued if required.
 *
 * If fail_on_error is false, we return NULL without printing any message
 * on failure, but preserve any prompted password for the next try.
 *
 * On success, the global variable 'connstr' is set to a connection string
 * containing the options used.
 */
static PGconn *
connectDatabase(const char *dbname, const char *connection_string,
				const char *pghost, const char *pgport, const char *pguser,
				trivalue prompt_password, bool fail_on_error)
{
	PGconn	   *conn;
	bool		new_pass;
	const char *remoteversion_str;
	int			my_version;
	const char **keywords = NULL;
	const char **values = NULL;
	PQconninfoOption *conn_opts = NULL;
	static bool have_password = false;
	static char password[100];

	if (prompt_password == TRI_YES && !have_password)
	{
		simple_prompt("Password: ", password, sizeof(password), false);
		have_password = true;
	}

	/*
	 * Start the connection.  Loop until we have a password if requested by
	 * backend.
	 */
	do
	{
		int			argcount = 6;
		PQconninfoOption *conn_opt;
		char	   *err_msg = NULL;
		int			i = 0;

		if (keywords)
			free(keywords);
		if (values)
			free(values);
		if (conn_opts)
			PQconninfoFree(conn_opts);

		/*
		 * Merge the connection info inputs given in form of connection string
		 * and other options.  Explicitly discard any dbname value in the
		 * connection string; otherwise, PQconnectdbParams() would interpret
		 * that value as being itself a connection string.
		 */
		if (connection_string)
		{
			conn_opts = PQconninfoParse(connection_string, &err_msg);
			if (conn_opts == NULL)
			{
				pg_log_error("%s", err_msg);
				exit_nicely(1);
			}

			for (conn_opt = conn_opts; conn_opt->keyword != NULL; conn_opt++)
			{
				if (conn_opt->val != NULL && conn_opt->val[0] != '\0' &&
					strcmp(conn_opt->keyword, "dbname") != 0)
					argcount++;
			}

			keywords = pg_malloc0((argcount + 1) * sizeof(*keywords));
			values = pg_malloc0((argcount + 1) * sizeof(*values));

			for (conn_opt = conn_opts; conn_opt->keyword != NULL; conn_opt++)
			{
				if (conn_opt->val != NULL && conn_opt->val[0] != '\0' &&
					strcmp(conn_opt->keyword, "dbname") != 0)
				{
					keywords[i] = conn_opt->keyword;
					values[i] = conn_opt->val;
					i++;
				}
			}
		}
		else
		{
			keywords = pg_malloc0((argcount + 1) * sizeof(*keywords));
			values = pg_malloc0((argcount + 1) * sizeof(*values));
		}

		if (pghost)
		{
			keywords[i] = "host";
			values[i] = pghost;
			i++;
		}
		if (pgport)
		{
			keywords[i] = "port";
			values[i] = pgport;
			i++;
		}
		if (pguser)
		{
			keywords[i] = "user";
			values[i] = pguser;
			i++;
		}
		if (have_password)
		{
			keywords[i] = "password";
			values[i] = password;
			i++;
		}
		if (dbname)
		{
			keywords[i] = "dbname";
			values[i] = dbname;
			i++;
		}
		keywords[i] = "fallback_application_name";
		values[i] = progname;
		i++;

		new_pass = false;
		conn = PQconnectdbParams(keywords, values, true);

		if (!conn)
		{
			pg_log_error("could not connect to database \"%s\"", dbname);
			exit_nicely(1);
		}

		if (PQstatus(conn) == CONNECTION_BAD &&
			PQconnectionNeedsPassword(conn) &&
			!have_password &&
			prompt_password != TRI_NO)
		{
			PQfinish(conn);
			simple_prompt("Password: ", password, sizeof(password), false);
			have_password = true;
			new_pass = true;
		}
	} while (new_pass);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		if (fail_on_error)
		{
			pg_log_error("could not connect to database \"%s\": %s",
						 dbname, PQerrorMessage(conn));
			exit_nicely(1);
		}
		else
		{
			PQfinish(conn);

			free(keywords);
			free(values);
			PQconninfoFree(conn_opts);

			return NULL;
		}
	}

	/*
	 * Ok, connected successfully. Remember the options used, in the form of a
	 * connection string.
	 */
	connstr = constructConnStr(keywords, values);

	free(keywords);
	free(values);
	PQconninfoFree(conn_opts);

	/* Check version */
	remoteversion_str = PQparameterStatus(conn, "server_version");
	if (!remoteversion_str)
	{
		pg_log_error("could not get server version");
		exit_nicely(1);
	}
	server_version = PQserverVersion(conn);
	if (server_version == 0)
	{
		pg_log_error("could not parse server version \"%s\"",
					 remoteversion_str);
		exit_nicely(1);
	}

	my_version = PG_VERSION_NUM;

	/*
	 * We allow the server to be back to 8.0, and up to any minor release of
	 * our own major version.  (See also version check in pg_dump.c.)
	 */
	if (my_version != server_version
		&& (server_version < 80200 ||		/* we can handle back to 8.2 */
			(server_version / 100) > (my_version / 100)))
	{
		pg_log_error("server version: %s; %s version: %s",
					 remoteversion_str, progname, PG_VERSION);
		pg_log_error("aborting because of server version mismatch");
		exit_nicely(1);
	}

	PQclear(executeQuery(conn, ALWAYS_SECURE_SEARCH_PATH_SQL));

	return conn;
}

/* ----------
 * Construct a connection string from the given keyword/value pairs. It is
 * used to pass the connection options to the pg_dump subprocess.
 *
 * The following parameters are excluded:
 *	dbname		- varies in each pg_dump invocation
 *	password	- it's not secure to pass a password on the command line
 *	fallback_application_name - we'll let pg_dump set it
 * ----------
 */
static char *
constructConnStr(const char **keywords, const char **values)
{
	PQExpBuffer buf = createPQExpBuffer();
	char	   *connstr;
	int			i;
	bool		firstkeyword = true;

	/* Construct a new connection string in key='value' format. */
	for (i = 0; keywords[i] != NULL; i++)
	{
		if (strcmp(keywords[i], "dbname") == 0 ||
			strcmp(keywords[i], "password") == 0 ||
			strcmp(keywords[i], "fallback_application_name") == 0)
			continue;

		if (!firstkeyword)
			appendPQExpBufferChar(buf, ' ');
		firstkeyword = false;
		appendPQExpBuffer(buf, "%s=", keywords[i]);
		appendConnStrVal(buf, values[i]);
	}

	connstr = pg_strdup(buf->data);
	destroyPQExpBuffer(buf);
	return connstr;
}

/*
 * Run a query, return the results, exit program on failure.
 */
static PGresult *
executeQuery(PGconn *conn, const char *query)
{
	PGresult   *res;

	pg_log_info("executing %s", query);

	res = PQexec(conn, query);
	if (!res ||
		PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		pg_log_error("query failed: %s", PQerrorMessage(conn));
		pg_log_error("query was: %s", query);
		PQfinish(conn);
		exit_nicely(1);
	}

	return res;
}

/*
 * As above for a SQL command (which returns nothing).
 */
static void
executeCommand(PGconn *conn, const char *query)
{
	PGresult   *res;

	pg_log_info("executing %s", query);

	res = PQexec(conn, query);
	if (!res ||
		PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		pg_log_error("query failed: %s", PQerrorMessage(conn));
		pg_log_error("query was: %s", query);
		PQfinish(conn);
		exit_nicely(1);
	}

	PQclear(res);
}


/*
 * dumpTimestamp
 */
static void
dumpTimestamp(const char *msg)
{
	char		buf[64];
	time_t		now = time(NULL);

	if (strftime(buf, sizeof(buf), PGDUMP_STRFTIME_FMT, localtime(&now)) != 0)
		fprintf(OPF, "-- %s %s\n\n", msg, buf);
}

/*
 * This GPDB-specific function is copied (in spirit) from pg_dump.c.
 *
 * PostgreSQL's pg_dumpall supports very old server versions, but in GPDB, we
 * only need to go back to 8.2-derived GPDB versions (4.something?). A lot of
 * that code to deal with old versions has been removed. But in order to not
 * change the formatting of the surrounding code, and to make it more clear
 * when reading a diff against the corresponding PostgreSQL version of
 * pg_dumpall, calls to this function has been left in place of the removed
 * code.
 *
 * This function should never actually be used, because check that the server
 * version is new enough at the beginning of pg_dumpall. This is just for
 * documentation purposes, to show were upstream code has been removed, and
 * to avoid those diffs or merge conflicts with upstream.
 */
static void
error_unsupported_server_version(PGconn *conn)
{
	fprintf(stderr, _("unexpected server version %d\n"), server_version);
	PQfinish(conn);
	exit(1);
}
