/*-------------------------------------------------------------------------
 *
 * pg_dumpall.c
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
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

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#include "getopt_long.h"

#include "dumputils.h"
#include "pg_backup.h"
#include "fe_utils/connect.h"
#include "catalog/gp_segment_config.h"

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
static void dumpCreateDB(PGconn *conn);
static void dumpDatabaseConfig(PGconn *conn, const char *dbname);
static void dumpUserConfig(PGconn *conn, const char *username);
static void dumpDbRoleConfig(PGconn *conn);
static void makeAlterConfigCommand(PGconn *conn, const char *arrayitem,
					   const char *type, const char *name, const char *type2,
					   const char *name2);
static void dumpDatabases(PGconn *conn);
static void dumpTimestamp(char *msg);

static int	runPgDump(const char *dbname);
static void buildShSecLabels(PGconn *conn,
				 const char *catalog_name, Oid objectId,
				 const char *objtype, const char *objname,
				 PQExpBuffer buffer);
static PGconn *connectDatabase(const char *dbname, const char *connstr, const char *pghost, const char *pgport,
	  const char *pguser, enum trivalue prompt_password, bool fail_on_error);
static char *constructConnStr(const char **keywords, const char **values);
static PGresult *executeQuery(PGconn *conn, const char *query);
static void executeCommand(PGconn *conn, const char *query);

static char pg_dump_bin[MAXPGPATH];
static const char *progname;
static PQExpBuffer pgdumpopts;
static char *connstr = "";
static bool skip_acls = false;
static bool verbose = false;

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
static int	no_security_labels = 0;
static int	no_unlogged_table_data = 0;
static int	server_version;


static FILE *OPF;
static char *filename = NULL;

#define exit_nicely(code) exit(code)

int
main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"data-only", no_argument, NULL, 'a'},
		{"clean", no_argument, NULL, 'c'},
		{"file", required_argument, NULL, 'f'},
		{"globals-only", no_argument, NULL, 'g'},
		{"host", required_argument, NULL, 'h'},
		{"ignore-version", no_argument, NULL, 'i'},
		{"dbname", required_argument, NULL, 'd'},
		{"database", required_argument, NULL, 'l'},
		{"oids", no_argument, NULL, 'o'},
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
		{"if-exists", no_argument, &if_exists, 1},
		{"inserts", no_argument, &inserts, 1},
		{"resource-queues", no_argument, &resource_queues, 1},
		{"resource-groups", no_argument, &resource_groups, 1},
		{"roles-only", no_argument, NULL, 999},
		{"lock-wait-timeout", required_argument, NULL, 2},
		{"no-tablespaces", no_argument, &no_tablespaces, 1},
		{"quote-all-identifiers", no_argument, &quote_all_identifiers, 1},
		{"role", required_argument, NULL, 3},
		{"use-set-session-authorization", no_argument, &use_setsessauth, 1},
		{"no-security-labels", no_argument, &no_security_labels, 1},
		{"no-unlogged-table-data", no_argument, &no_unlogged_table_data, 1},

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
	enum trivalue prompt_password = TRI_DEFAULT;
	bool		data_only = false;
	bool		globals_only = false;
	bool		output_clean = false;
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
			fprintf(stderr,
					_("The program \"pg_dump\" is needed by %s "
					  "but was not found in the\n"
					  "same directory as \"%s\".\n"
					  "Check your installation.\n"),
					progname, full_path);
		else
			fprintf(stderr,
					_("The program \"pg_dump\" was found by \"%s\"\n"
					  "but was not the same version as %s.\n"
					  "Check your installation.\n"),
					full_path, progname);
		exit_nicely(1);
	}

	pgdumpopts = createPQExpBuffer();

	while ((c = getopt_long(argc, argv, "acd:f:gh:il:oOp:rsS:tU:vwWx", long_options, &optindex)) != -1)
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

			case 'i':
				/* ignored, deprecated option */
				break;

			case 'l':
				pgdb = pg_strdup(optarg);
				break;

			case 'o':
				appendPQExpBufferStr(pgdumpopts, " -o");
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
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	/* Make sure the user hasn't specified a mix of globals-only options */
	if (globals_only && roles_only)
	{
		fprintf(stderr, _("%s: options -g/--globals-only and -r/--roles-only cannot be used together\n"),
				progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (globals_only && tablespaces_only)
	{
		fprintf(stderr, _("%s: options -g/--globals-only and -t/--tablespaces-only cannot be used together\n"),
				progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (if_exists && !output_clean)
	{
		fprintf(stderr, _("%s: option --if-exists requires option -c/--clean\n"),
				progname);
		exit_nicely(1);
	}

	if (roles_only && tablespaces_only)
	{
		fprintf(stderr, _("%s: options -r/--roles-only and -t/--tablespaces-only cannot be used together\n"),
				progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit_nicely(1);
	}

	if (gp_syntax && no_gp_syntax)
	{
		fprintf(stderr, _("%s: options \"--gp-syntax\" and \"--no-gp-syntax\" cannot be used together\n"),
				progname);
		exit(1);
	}

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
	if (use_setsessauth)
		appendPQExpBufferStr(pgdumpopts, " --use-set-session-authorization");
	if (no_security_labels)
		appendPQExpBufferStr(pgdumpopts, " --no-security-labels");
	if (no_unlogged_table_data)
		appendPQExpBufferStr(pgdumpopts, " --no-unlogged-table-data");
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
			fprintf(stderr, _("%s: could not connect to database \"%s\"\n"),
					progname, pgdb);
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
			fprintf(stderr, _("%s: could not connect to databases \"postgres\" or \"template1\"\n"
							  "Please specify an alternative database.\n"),
					progname);
			fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
					progname);
			exit_nicely(1);
		}
	}

	/*
	 * Open the output file if required, otherwise use stdout
	 */
	if (filename)
	{
		OPF = fopen(filename, PG_BINARY_W);
		if (!OPF)
		{
			fprintf(stderr, _("%s: could not open the output file \"%s\": %s\n"),
					progname, filename, strerror(errno));
			exit_nicely(1);
		}
	}
	else
		OPF = stdout;

	/*
	 * Get the active encoding and the standard_conforming_strings setting, so
	 * we know how to escape strings.
	 */
	encoding = PQclientEncoding(conn);
	std_strings = PQparameterStatus(conn, "standard_conforming_strings");
	if (!std_strings)
		std_strings = "off";

	/* Set the role if requested */
	if (use_role)
	{
		PQExpBuffer query = createPQExpBuffer();

		appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
		executeCommand(conn, query->data);
		destroyPQExpBuffer(query);
	}

	/* Force quoting of all identifiers if requested. */
	if (quote_all_identifiers)
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
			{
				dropTablespaces(conn);
			}

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

		if (!roles_only && !no_tablespaces)
		{
			/* Dump tablespaces */
			dumpTablespaces(conn);
		}

		/* Dump CREATE DATABASE commands */
		if (binary_upgrade || (!globals_only && !roles_only && !tablespaces_only))
			dumpCreateDB(conn);

		/* Dump role/database settings */
		if (!tablespaces_only && !roles_only)
		{
			if (server_version >= 90000)
				dumpDbRoleConfig(conn);
		}
	}

	if (!globals_only && !roles_only && !tablespaces_only)
		dumpDatabases(conn);

	PQfinish(conn);

	if (verbose)
		dumpTimestamp("Completed on");
	fprintf(OPF, "--\n-- PostgreSQL database cluster dump complete\n--\n\n");

	if (filename)
		fclose(OPF);

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
	printf(_("  -g, --globals-only           dump only global objects, no databases\n"));
	printf(_("  -o, --oids                   include OIDs in dump\n"));
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
	printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
	printf(_("  --inserts                    dump data as INSERT commands, rather than COPY\n"));
	printf(_("  --no-security-labels         do not dump security label assignments\n"));
	printf(_("  --no-tablespaces             do not dump tablespace assignments\n"));
	printf(_("  --no-unlogged-table-data     do not dump unlogged table data\n"));
	printf(_("  --quote-all-identifiers      quote all identifiers, even if not key words\n"));
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
					  "LEFT JOIN pg_resgroupcapability t7 ON g.oid = t7.resgroupid AND t7.reslimittype = 7;");

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
	fprintf(OPF, "ALTER RESOURCE GROUP \"admin_group\" SET cpu_rate_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP \"default_group\" SET cpu_rate_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP \"admin_group\" SET memory_limit 1;\n");
	fprintf(OPF, "ALTER RESOURCE GROUP \"default_group\" SET memory_limit 1;\n");

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

		groupname = PQgetvalue(res, i, i_groupname);
		cpu_rate_limit = PQgetvalue(res, i, i_cpu_rate_limit);
		concurrency = PQgetvalue(res, i, i_concurrency);
		memory_limit = PQgetvalue(res, i, i_memory_limit);
		memory_shared_quota = PQgetvalue(res, i, i_memory_shared_quota);
		memory_spill_ratio = PQgetvalue(res, i, i_memory_spill_ratio);
		memory_auditor = PQgetvalue(res, i, i_memory_auditor);
		cpuset = PQgetvalue(res, i, i_cpuset);

		resetPQExpBuffer(buf);

		if (0 == strcmp(groupname, "default_group") || 0 == strcmp(groupname, "admin_group"))
		{
			/*
			 * We can't emit CREATE statements for the built-in groups as they
			 * will already exist in the target cluster. So emit ALTER
			 * statements instead.
			 *
			 * Default resource groups must have memory_auditor == "vmtracker",
			 * no need to ALTER it, and we do not support ALTER memory_auditor
			 * at all.
			 */
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET concurrency %s;\n",
							  fmtId(groupname), concurrency);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_limit %s;\n",
							  fmtId(groupname), memory_limit);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_shared_quota %s;\n",
							  fmtId(groupname), memory_shared_quota);
			appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET memory_spill_ratio %s;\n",
							  fmtId(groupname), memory_spill_ratio);
			if (atoi(cpu_rate_limit) >= 0)
				appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET cpu_rate_limit %s;\n",
								  fmtId(groupname), cpu_rate_limit);
			else
				appendPQExpBuffer(buf, "ALTER RESOURCE GROUP %s SET cpuset '%s';\n",
								  fmtId(groupname), cpuset);
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
							  fmtId(groupname), concurrency, cpu_prop, cpu_setting,
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
					  "rsqignorecostlimit::text as ressetting, "
					  "4 as ord FROM pg_resqueue "
					   "UNION "
					   "SELECT rq.oid, rq.rsqname, rt.resname, rc.ressetting, "
					   "rt.restypid as ord FROM "
					   "pg_resqueue rq,  pg_resourcetype rt, "
					   "pg_resqueuecapability rc WHERE "
					   "rq.oid=rc.resqueueid and rc.restypid = rt.restypid "
					  "order by rsqname,  ord");

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
	PGresult   *res;
	int			i_rolname;
	int			i;

	res = executeQuery(conn,
							"SELECT rolname "
							"FROM pg_authid "
							"ORDER BY 1");

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
				i_rolcomment,
				i_rolqueuename = -1,	/* keep compiler quiet */
				i_rolgroupname = -1,	/* keep compiler quiet */
				i_rolcreaterextgpfd,
				i_rolcreaterexthttp,
				i_rolcreatewextgpfd,
				i_rolcreaterexthdfs,
				i_rolcreatewexthdfs,
				i_is_current_user;
	int			i;

	char	   *resq_col = resource_queues ? "(SELECT rsqname FROM pg_resqueue WHERE "
	"  pg_resqueue.oid = rolresqueue) AS rolqueuename, " : "";
	char	   *resgroup_col = resource_groups ? "(SELECT rsgname FROM pg_resgroup WHERE "
	"  pg_resgroup.oid = rolresgroup) AS rolgroupname, " : "";

	/*
	 * Query to select role info get resqueue if version support it get
	 * external table auth on gpfdist, gpfdists and http if version support it get
	 *
	 * Support for gphdfs was removed in Greenplum 6
	 */

	/* note: rolconfig is dumped later */
	if (server_version >= 90400)
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, rolcatupdate, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, rolreplication, "
						  "pg_catalog.shobj_description(oid, 'pg_authid') as rolcomment, "
						  "rolname = current_user AS is_current_user, "
						  "%s %s"
							"'f' AS rolcreaterexthdfs, 'f' AS rolcreatewexthdfs, "
						  "rolcreaterextgpfd, rolcreaterexthttp, rolcreatewextgpfd "
						  "FROM pg_authid "
						  "ORDER BY 2",
						  resq_col, resgroup_col);
	else
		printfPQExpBuffer(buf,
						  "SELECT oid, rolname, rolsuper, rolinherit, "
						  "rolcreaterole, rolcreatedb, rolcatupdate, "
						  "rolcanlogin, rolconnlimit, rolpassword, "
						  "rolvaliduntil, false as rolreplication, "
						  "pg_catalog.shobj_description(oid, 'pg_authid') as rolcomment, "
						  "rolname = current_user AS is_current_user, "
						  "%s %s"
						  "rolcreaterexthdfs, rolcreatewexthdfs, "
						  "rolcreaterextgpfd, rolcreaterexthttp, rolcreatewextgpfd "
						  "FROM pg_authid "
						  "ORDER BY 2",
						  resq_col, resgroup_col);

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
	i_rolcomment = PQfnumber(res, "rolcomment");
	i_is_current_user = PQfnumber(res, "is_current_user");

	if (resource_queues)
		i_rolqueuename = PQfnumber(res, "rolqueuename");

	if (resource_groups)
		i_rolgroupname = PQfnumber(res, "rolgroupname");

	i_rolcreaterextgpfd = PQfnumber(res, "rolcreaterextgpfd");
	i_rolcreaterexthttp = PQfnumber(res, "rolcreaterexthttp");
	i_rolcreatewextgpfd = PQfnumber(res, "rolcreatewextgpfd");

	i_rolcreaterexthdfs = PQfnumber(res, "rolcreaterexthdfs");
	i_rolcreatewexthdfs = PQfnumber(res, "rolcreatewexthdfs");


	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Roles\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		const char *rolename;
		Oid			auth_oid;

		auth_oid = atooid(PQgetvalue(res, i, i_oid));
		rolename = PQgetvalue(res, i, i_rolname);

		resetPQExpBuffer(buf);

		if (binary_upgrade)
		{
			appendPQExpBufferStr(buf, "\n-- For binary upgrade, must preserve pg_authid.oid\n");
			appendPQExpBuffer(buf,
							  "SELECT binary_upgrade.set_next_pg_authid_oid('%u'::pg_catalog.oid, '%s'::text);\n\n",
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

		if (strcmp(PQgetvalue(res, i, i_rolconnlimit), "-1") != 0)
			appendPQExpBuffer(buf, " CONNECTION LIMIT %s",
							  PQgetvalue(res, i, i_rolconnlimit));

		if (!PQgetisnull(res, i, i_rolpassword))
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

		if (!PQgetisnull(res, i, i_rolcreaterexthdfs) &&
			strcmp(PQgetvalue(res, i, i_rolcreaterexthdfs), "t") == 0)
			appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gphdfs', type='readable')");

		if (!PQgetisnull(res, i, i_rolcreatewexthdfs) &&
			strcmp(PQgetvalue(res, i, i_rolcreatewexthdfs), "t") == 0)
			appendPQExpBufferStr(buf, " CREATEEXTTABLE (protocol='gphdfs', type='writable')");

		appendPQExpBufferStr(buf, ";\n");

		if (!PQgetisnull(res, i, i_rolcomment))
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
	PGresult   *res;
	int			i;

	res = executeQuery(conn, "SELECT ur.rolname AS roleid, "
					   "um.rolname AS member, "
					   "a.admin_option, "
					   "ug.rolname AS grantor "
					   "FROM pg_auth_members a "
					   "LEFT JOIN pg_authid ur on ur.oid = a.roleid "
					   "LEFT JOIN pg_authid um on um.oid = a.member "
					   "LEFT JOIN pg_authid ug on ug.oid = a.grantor "
					   "ORDER BY 1,2,3");

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

static void
append_preassign_tablespace_oid(PQExpBuffer buffer, Oid tablespace_oid, char *tablespace_name)
{
	appendPQExpBuffer(buffer,
	                  "select binary_upgrade.set_next_preassigned_tablespace_oid(%u, '%s');\n",
	                  tablespace_oid,
	                  tablespace_name);
}

/*
 * Dump tablespaces.
 */
static void
dumpTablespaces(PGconn *conn)
{
	PGresult   *res;
	int			i;
	bool        filespace_to_tablespace = false;

	/*
	 * Get all tablespaces execpt built-in ones (named pg_xxx)
	 *
	 * [FIXME] the queries need to be slightly different if the backend isn't
	 * Greenplum, and the dump format should vary depending on if the dump is
	 * --gp-syntax or --no-gp-syntax.
	 */
	if (server_version == GPDB5_MAJOR_PGVERSION)
	{
		filespace_to_tablespace = true;
		/*
		 * Filespaces imply that there will be multiple rows in the result set
		 * that will represent a single tablespace. This is due to the fact that
		 * a filespace spans over multiple "locations". Each row for a single
		 * tablespace records segment specific information, which would include
		 * the segment specific location. Although filespaces can have different
		 * values for location for the primary and mirror of a primary-mirror
		 * pair, GP6+ onwards, we restrict primary-mirror pairs to have the same
		 * value for location. This is why we filter the result set with role =
		 * primary. We also sort the result set such that each tablespace is
		 * represented by contiguous rows.
		 */
		res = executeQuery(conn,
				"SELECT tsi.*, "
				       "sc.content "
				"FROM   gp_segment_configuration AS sc "
				       "join (SELECT ts.oid                                  AS oid, "
				                    "ts.spcname                              AS spcname, "
				                    "pg_catalog.Pg_get_userbyid(ts.spcowner) AS spcowner, "
									"fs.fselocation || '/' || ts.oid         AS location, "
				                    "ts.spcacl                               AS spcacl, "
				                    "NULL                                    AS spcoptions, "
				                    "pg_catalog.Shobj_description(ts.oid, 'pg_tablespace'), "
				                    "fs.fsedbid                              AS dbid "
				             "FROM   pg_tablespace AS ts "
				                    "join pg_filespace_entry AS fs "
				                      "ON ts.spcfsoid = fs.fsefsoid "
				             "WHERE  ts.spcname !~ '^pg_') AS tsi "
				         "ON tsi.dbid = sc.dbid "
				            "AND sc.ROLE = 'p' "
				"ORDER  BY oid, content ASC;");
	}
	else if (server_version >= 90200)
		res = executeQuery(conn, "SELECT oid, spcname, "
						 "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "pg_catalog.pg_tablespace_location(oid), spcacl, "
						   "array_to_string(spcoptions, ', '),"
						"pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else if (server_version >= 90000)
		res = executeQuery(conn, "SELECT oid, spcname, "
						 "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "pg_catalog.pg_tablespace_location(oid), spcacl, "
						   "array_to_string(spcoptions, ', '),"
						"pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");
	else
		res = executeQuery(conn, "SELECT oid, spcname, "
						 "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
						   "spclocation, spcacl, null, "
						"pg_catalog.shobj_description(oid, 'pg_tablespace') "
						   "FROM pg_catalog.pg_tablespace "
						   "WHERE spcname !~ '^pg_' "
						   "ORDER BY 1");

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
		char	   *spcoptions = PQgetvalue(res, i, 5);
		char	   *spccomment = PQgetvalue(res, i, 6);
		char	   *fspcname;

		append_preassign_tablespace_oid(buf, spcoid, spcname);

		/* needed for buildACLCommands() */
		fspcname = pg_strdup(fmtId(spcname));

		appendPQExpBuffer(buf, "CREATE TABLESPACE %s", spcname);
		appendPQExpBuffer(buf, " OWNER %s", fmtId(spcowner));

		appendPQExpBufferStr(buf, " LOCATION ");
		appendStringLiteralConn(buf, spclocation, conn);

		if (filespace_to_tablespace)
		{
			int j;
			for (j = i + 1; j < PQntuples(res) && atooid(PQgetvalue(res, j, 0)) == spcoid; j++)
			{
				if (j == i + 1)
				{
					if (!(atoi(PQgetvalue(res, i, 8)) == MASTER_CONTENT_ID))
					{
						fprintf(stderr, _("%s: master location is unavailable for tablespace \"%s\"\n"),
								progname, spcname);
						PQfinish(conn);
						exit_nicely(1);
					}
					appendPQExpBufferStr(buf, " WITH (");
				}
				else
					appendPQExpBufferStr(buf, ", ");

				appendPQExpBuffer(buf, "content%s=", PQgetvalue(res, j, 8));
				appendStringLiteralConn(buf, PQgetvalue(res, j, 3), conn);
			}
			if (j > i + 1)
			{
				appendPQExpBufferStr(buf, ")");
				i = j - 1;
			}
		}

		appendPQExpBufferStr(buf, ";\n");

		if (spcoptions && spcoptions[0] != '\0')
			appendPQExpBuffer(buf, "ALTER TABLESPACE %s SET (%s);\n",
							  fspcname, spcoptions);

		if (!skip_acls &&
			!buildACLCommands(fspcname, NULL, NULL, "TABLESPACE",
							  spcacl, spcowner,
							  "", server_version, buf))
		{
			fprintf(stderr, _("%s: could not parse ACL list (%s) for tablespace \"%s\"\n"),
					progname, spcacl, spcname);
			PQfinish(conn);
			exit_nicely(1);
		}

		/* Set comments */
		if (spccomment && strlen(spccomment))
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
 *
 * This should match the set of databases targeted by dumpCreateDB().
 */
static void
dropDBs(PGconn *conn)
{
	PGresult   *res;
	int			i;

	res = executeQuery(conn,
							"SELECT datname "
							"FROM pg_database d "
							"WHERE datallowconn ORDER BY 1");

	if (PQntuples(res) > 0)
		fprintf(OPF, "--\n-- Drop databases\n--\n\n");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *dbname = PQgetvalue(res, i, 0);

		/*
		 * Skip "template1" and "postgres"; the restore script is almost
		 * certainly going to be run in one or the other, and we don't know
		 * which.  This must agree with dumpCreateDB's choices!
		 */
		if (strcmp(dbname, "template1") != 0 &&
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
 * Dump commands to create each database.
 *
 * To minimize the number of reconnections (and possibly ensuing
 * password prompts) required by the output script, we emit all CREATE
 * DATABASE commands during the initial phase of the script, and then
 * run pg_dump for each database to dump the contents of that
 * database.  We skip databases marked not datallowconn, since we'd be
 * unable to connect to them anyway (and besides, we don't want to
 * dump template0).
 */
static void
dumpCreateDB(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	char	   *default_encoding = NULL;
	char	   *default_collate = NULL;
	char	   *default_ctype = NULL;
	PGresult   *res;
	int			i;

	fprintf(OPF, "--\n-- Database creation\n--\n\n");

	/*
	 * First, get the installation's default encoding and locale information.
	 * We will dump encoding and locale specifications in the CREATE DATABASE
	 * commands for just those databases with values different from defaults.
	 *
	 * We consider template0's encoding and locale (or, pre-7.1, template1's)
	 * to define the installation default.  Pre-8.4 installations do not have
	 * per-database locale settings; for them, every database must necessarily
	 * be using the installation default, so there's no need to do anything
	 * (which is good, since in very old versions there is no good way to find
	 * out what the installation locale is anyway...)
	 */
	if (server_version >= 80400)
		res = executeQuery(conn,
						   "SELECT pg_encoding_to_char(encoding), "
						   "datcollate, datctype "
						   "FROM pg_database "
						   "WHERE datname = 'template0'");
	else
		res = executeQuery(conn,
						   "SELECT pg_encoding_to_char(encoding), "
						   "null::text AS datcollate, null::text AS datctype "
						   "FROM pg_database "
						   "WHERE datname = 'template0'");

	/* If for some reason the template DB isn't there, treat as unknown */
	if (PQntuples(res) > 0)
	{
		if (!PQgetisnull(res, 0, 0))
			default_encoding = pg_strdup(PQgetvalue(res, 0, 0));
		if (!PQgetisnull(res, 0, 1))
			default_collate = pg_strdup(PQgetvalue(res, 0, 1));
		if (!PQgetisnull(res, 0, 2))
			default_ctype = pg_strdup(PQgetvalue(res, 0, 2));
	}

	PQclear(res);

	/* Now collect all the information about databases to dump */
	if (server_version >= 90300)
		res = executeQuery(conn,
						   "SELECT datname, "
						   "coalesce(rolname, (select rolname from pg_authid where oid=(select datdba from pg_database where datname='template0'))), "
						   "pg_encoding_to_char(d.encoding), "
						   "datcollate, datctype, datfrozenxid, datminmxid, "
						   "datistemplate, datacl, datconnlimit, "
						   "(SELECT spcname FROM pg_tablespace t WHERE t.oid = d.dattablespace) AS dattablespace "
			  "FROM pg_database d LEFT JOIN pg_authid u ON (datdba = u.oid) "
						   "WHERE datallowconn ORDER BY 1");
	else if (server_version >= 80400)
		res = executeQuery(conn,
						   "SELECT datname, "
						   "coalesce(rolname, (select rolname from pg_authid where oid=(select datdba from pg_database where datname='template0'))), "
						   "pg_encoding_to_char(d.encoding), "
						   "datcollate, datctype, datfrozenxid, 0 AS datminmxid, "
						   "datistemplate, datacl, datconnlimit, "
						   "(SELECT spcname FROM pg_tablespace t WHERE t.oid = d.dattablespace) AS dattablespace "
			  "FROM pg_database d LEFT JOIN pg_authid u ON (datdba = u.oid) "
						   "WHERE datallowconn ORDER BY 1");
	else
		res = executeQuery(conn,
						   "SELECT datname, "
						   "coalesce(rolname, (select rolname from pg_authid where oid=(select datdba from pg_database where datname='template0'))), "
						   "pg_encoding_to_char(d.encoding), "
		   "null::text AS datcollate, null::text AS datctype, datfrozenxid, 0 AS datminmxid, "
						   "datistemplate, datacl, datconnlimit, "
						   "(SELECT spcname FROM pg_tablespace t WHERE t.oid = d.dattablespace) AS dattablespace "
			  "FROM pg_database d LEFT JOIN pg_authid u ON (datdba = u.oid) "
						   "WHERE datallowconn ORDER BY 1");

	for (i = 0; i < PQntuples(res); i++)
	{
		char	   *dbname = PQgetvalue(res, i, 0);
		char	   *dbowner = PQgetvalue(res, i, 1);
		char	   *dbencoding = PQgetvalue(res, i, 2);
		char	   *dbcollate = PQgetvalue(res, i, 3);
		char	   *dbctype = PQgetvalue(res, i, 4);
		uint32		dbfrozenxid = atooid(PQgetvalue(res, i, 5));
		uint32		dbminmxid = atooid(PQgetvalue(res, i, 6));
		char	   *dbistemplate = PQgetvalue(res, i, 7);
		char	   *dbacl = PQgetvalue(res, i, 8);
		char	   *dbconnlimit = PQgetvalue(res, i, 9);
		char	   *dbtablespace = PQgetvalue(res, i, 10);
		char	   *fdbname;

		fdbname = pg_strdup(fmtId(dbname));

		resetPQExpBuffer(buf);

		/*
		 * Skip the CREATE DATABASE commands for "template1" and "postgres",
		 * since they are presumably already there in the destination cluster.
		 * We do want to emit their ACLs and config options if any, however.
		 */
		appendPQExpBuffer(buf, "SET allow_system_table_mods = true;\n");

		if (strcmp(dbname, "template1") != 0 &&
			strcmp(dbname, "postgres") != 0)
		{
			appendPQExpBuffer(buf, "CREATE DATABASE %s", fdbname);

			appendPQExpBufferStr(buf, " WITH TEMPLATE = template0");

			if (strlen(dbowner) != 0)
				appendPQExpBuffer(buf, " OWNER = %s", fmtId(dbowner));

			if (default_encoding && strcmp(dbencoding, default_encoding) != 0)
			{
				appendPQExpBufferStr(buf, " ENCODING = ");
				appendStringLiteralConn(buf, dbencoding, conn);
			}

			if (default_collate && strcmp(dbcollate, default_collate) != 0)
			{
				appendPQExpBufferStr(buf, " LC_COLLATE = ");
				appendStringLiteralConn(buf, dbcollate, conn);
			}

			if (default_ctype && strcmp(dbctype, default_ctype) != 0)
			{
				appendPQExpBufferStr(buf, " LC_CTYPE = ");
				appendStringLiteralConn(buf, dbctype, conn);
			}

			/*
			 * Output tablespace if it isn't the default.  For default, it
			 * uses the default from the template database.  If tablespace is
			 * specified and tablespace creation failed earlier, (e.g. no such
			 * directory), the database creation will fail too.  One solution
			 * would be to use 'SET default_tablespace' like we do in pg_dump
			 * for setting non-default database locations.
			 */
			if (strcmp(dbtablespace, "pg_default") != 0 && !no_tablespaces)
				appendPQExpBuffer(buf, " TABLESPACE = %s",
								  fmtId(dbtablespace));

			if (strcmp(dbconnlimit, "-1") != 0)
				appendPQExpBuffer(buf, " CONNECTION LIMIT = %s",
								  dbconnlimit);

			appendPQExpBufferStr(buf, ";\n");

			if (strcmp(dbistemplate, "t") == 0)
			{
				appendPQExpBufferStr(buf, "UPDATE pg_catalog.pg_database SET datistemplate = 't' WHERE datname = ");
				appendStringLiteralConn(buf, dbname, conn);
				appendPQExpBufferStr(buf, ";\n");
			}
		}
		else if (strcmp(dbtablespace, "pg_default") != 0 && !no_tablespaces)
		{
			/*
			 * Cannot change tablespace of the database we're connected to,
			 * so to move "postgres" to another tablespace, we connect to
			 * "template1", and vice versa.
			 */
			if (strcmp(dbname, "postgres") == 0)
				appendPQExpBuffer(buf, "\\connect template1\n");
			else
				appendPQExpBuffer(buf, "\\connect postgres\n");

			appendPQExpBuffer(buf, "ALTER DATABASE %s SET TABLESPACE %s;\n",
							  fdbname, fmtId(dbtablespace));

			/* connect to original database */
			appendPsqlMetaConnect(buf, dbname);
		}

		if (binary_upgrade)
		{
			appendPQExpBufferStr(buf, "-- For binary upgrade, set datfrozenxid and datminmxid.\n");
			appendPQExpBuffer(buf, "UPDATE pg_catalog.pg_database "
							  "SET datfrozenxid = '%u', datminmxid = '%u' "
							  "WHERE datname = ",
							  dbfrozenxid, dbminmxid);
			appendStringLiteralConn(buf, dbname, conn);
			appendPQExpBufferStr(buf, ";\n");
		}
		appendPQExpBuffer(buf, "RESET allow_system_table_mods;\n");

		if (!skip_acls &&
			!buildACLCommands(fdbname, NULL, NULL, "DATABASE", dbacl, dbowner,
							  "", server_version, buf))
		{
			fprintf(stderr, _("%s: could not parse ACL list (%s) for database \"%s\"\n"),
					progname, dbacl, fdbname);
			PQfinish(conn);
			exit_nicely(1);
		}

		fprintf(OPF, "%s", buf->data);

		dumpDatabaseConfig(conn, dbname);

		free(fdbname);
	}

	if (default_encoding)
		free(default_encoding);
	if (default_collate)
		free(default_collate);
	if (default_ctype)
		free(default_ctype);

	PQclear(res);
	destroyPQExpBuffer(buf);

	fprintf(OPF, "\n\n");
}


/*
 * Dump database-specific configuration
 */
static void
dumpDatabaseConfig(PGconn *conn, const char *dbname)
{
	PQExpBuffer buf = createPQExpBuffer();
	int			count = 1;

	for (;;)
	{
		PGresult   *res;

		if (server_version >= 90000)
			printfPQExpBuffer(buf, "SELECT setconfig[%d] FROM pg_db_role_setting WHERE "
							  "setrole = 0 AND setdatabase = (SELECT oid FROM pg_database WHERE datname = ", count);
		else
			printfPQExpBuffer(buf, "SELECT datconfig[%d] FROM pg_database WHERE datname = ", count);
		appendStringLiteralConn(buf, dbname, conn);

		if (server_version >= 90000)
			appendPQExpBuffer(buf, ")");

		res = executeQuery(conn, buf->data);
		if (PQntuples(res) == 1 &&
			!PQgetisnull(res, 0, 0))
		{
			makeAlterConfigCommand(conn, PQgetvalue(res, 0, 0),
								   "DATABASE", dbname, NULL, NULL);
			PQclear(res);
			count++;
		}
		else
		{
			PQclear(res);
			break;
		}
	}

	/*
	 * If we're upgrading from GPDB 5, use the legacy hash ops.
	 */
	if (binary_upgrade && server_version < GPDB6_MAJOR_PGVERSION)
	{
		makeAlterConfigCommand(conn, "gp_use_legacy_hashops=on",
							   "DATABASE", dbname, NULL, NULL);
	}

	destroyPQExpBuffer(buf);
}



/*
 * Dump user-specific configuration
 */
static void
dumpUserConfig(PGconn *conn, const char *username)
{
	PQExpBuffer buf = createPQExpBuffer();
	int			count = 1;

	for (;;)
	{
		PGresult   *res;

		if (server_version >= 90000)
			printfPQExpBuffer(buf, "SELECT setconfig[%d] FROM pg_db_role_setting WHERE "
							  "setdatabase = 0 AND setrole = "
					   "(SELECT oid FROM pg_authid WHERE rolname = ", count);
		else
			printfPQExpBuffer(buf, "SELECT rolconfig[%d] FROM pg_authid WHERE rolname = ", count);

		appendStringLiteralConn(buf, username, conn);
		if (server_version >= 90000)
			appendPQExpBufferChar(buf, ')');

		res = executeQuery(conn, buf->data);
		if (PQntuples(res) == 1 &&
			!PQgetisnull(res, 0, 0))
		{
			makeAlterConfigCommand(conn, PQgetvalue(res, 0, 0),
								   "ROLE", username, NULL, NULL);
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
 * Dump user-and-database-specific configuration
 */
static void
dumpDbRoleConfig(PGconn *conn)
{
	PQExpBuffer buf = createPQExpBuffer();
	PGresult   *res;
	int			i;

	/* Need "ORDER BY" here to keep order consistency cross pg_dump call */
	printfPQExpBuffer(buf, "SELECT rolname, datname, unnest(setconfig) "
					  "FROM pg_db_role_setting, pg_authid, pg_database "
		  "WHERE setrole = pg_authid.oid AND setdatabase = pg_database.oid ORDER BY 1,2,3");
	res = executeQuery(conn, buf->data);

	if (PQntuples(res) > 0)
	{
		fprintf(OPF, "--\n-- Per-Database Role Settings \n--\n\n");

		for (i = 0; i < PQntuples(res); i++)
		{
			makeAlterConfigCommand(conn, PQgetvalue(res, i, 2),
								   "ROLE", PQgetvalue(res, i, 0),
								   "DATABASE", PQgetvalue(res, i, 1));
		}

		fprintf(OPF, "\n\n");
	}

	PQclear(res);
	destroyPQExpBuffer(buf);
}


/*
 * Helper function for dumpXXXConfig().
 */
static void
makeAlterConfigCommand(PGconn *conn, const char *arrayitem,
					   const char *type, const char *name,
					   const char *type2, const char *name2)
{
	char	   *pos;
	char	   *mine;
	PQExpBuffer buf;

	mine = pg_strdup(arrayitem);
	pos = strchr(mine, '=');
	if (pos == NULL)
	{
		free(mine);
		return;
	}

	buf = createPQExpBuffer();

	*pos = 0;
	appendPQExpBuffer(buf, "ALTER %s %s ", type, fmtId(name));
	if (type2 != NULL && name2 != NULL)
		appendPQExpBuffer(buf, "IN %s %s ", type2, fmtId(name2));
	appendPQExpBuffer(buf, "SET %s TO ", fmtId(mine));

	/*
	 * Variables that are marked GUC_LIST_QUOTE were already fully quoted by
	 * flatten_set_variable_args() before they were put into the setconfig
	 * array.  However, because the quoting rules used there aren't exactly
	 * like SQL's, we have to break the list value apart and then quote the
	 * elements as string literals.  (The elements may be double-quoted as-is,
	 * but we can't just feed them to the SQL parser; it would do the wrong
	 * thing with elements that are zero-length or longer than NAMEDATALEN.)
	 *
	 * Variables that are not so marked should just be emitted as simple
	 * string literals.  If the variable is not known to
	 * variable_is_guc_list_quote(), we'll do that; this makes it unsafe to
	 * use GUC_LIST_QUOTE for extension variables.
	 */
	if (variable_is_guc_list_quote(mine))
	{
		char	  **namelist;
		char	  **nameptr;

		/* Parse string into list of identifiers */
		/* this shouldn't fail really */
		if (SplitGUCList(pos + 1, ',', &namelist))
		{
			for (nameptr = namelist; *nameptr; nameptr++)
			{
				if (nameptr != namelist)
					appendPQExpBufferStr(buf, ", ");
				appendStringLiteralConn(buf, *nameptr, conn);
			}
		}
		pg_free(namelist);
	}
	else
		appendStringLiteralConn(buf, pos + 1, conn);
	appendPQExpBufferStr(buf, ";\n");

	fprintf(OPF, "%s", buf->data);
	destroyPQExpBuffer(buf);
	free(mine);
}



/*
 * Dump contents of databases.
 */
static void
dumpDatabases(PGconn *conn)
{
	PGresult   *res;
	int			i;

	res = executeQuery(conn, "SELECT datname FROM pg_database WHERE datallowconn ORDER BY 1");

	for (i = 0; i < PQntuples(res); i++)
	{
		int			ret;

		char	   *dbname = PQgetvalue(res, i, 0);
		PQExpBufferData connectbuf;

		if (verbose)
			fprintf(stderr, _("%s: dumping database \"%s\"...\n"), progname, dbname);

		initPQExpBuffer(&connectbuf);
		appendPsqlMetaConnect(&connectbuf, dbname);
		fprintf(OPF, "%s\n", connectbuf.data);
		termPQExpBuffer(&connectbuf);

		/*
		 * Restore will need to write to the target cluster.  This connection
		 * setting is emitted for pg_dumpall rather than in the code also used
		 * by pg_dump, so that a cluster with databases or users which have
		 * this flag turned on can still be replicated through pg_dumpall
		 * without editing the file or stream.  With pg_dump there are many
		 * other ways to allow the file to be used, and leaving it out allows
		 * users to protect databases from being accidental restore targets.
		 */
		fprintf(OPF, "SET default_transaction_read_only = off;\n\n");

		if (filename)
			fclose(OPF);

		ret = runPgDump(dbname);
		if (ret != 0)
		{
			fprintf(stderr, _("%s: pg_dump failed on database \"%s\", exiting\n"), progname, dbname);
			exit_nicely(1);
		}

		if (filename)
		{
			OPF = fopen(filename, PG_BINARY_A);
			if (!OPF)
			{
				fprintf(stderr, _("%s: could not re-open the output file \"%s\": %s\n"),
						progname, filename, strerror(errno));
				exit_nicely(1);
			}
		}

	}

	PQclear(res);
}



/*
 * Run pg_dump on dbname.
 */
static int
runPgDump(const char *dbname)
{
	PQExpBuffer connstrbuf = createPQExpBuffer();
	PQExpBuffer cmd = createPQExpBuffer();
	int			ret;

	appendPQExpBuffer(cmd, "\"%s\" %s", pg_dump_bin,
					  pgdumpopts->data);

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

	if (verbose)
		fprintf(stderr, _("%s: running \"%s\"\n"), progname, cmd->data);

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
 * Build SECURITY LABEL command(s) for an shared object
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
				enum trivalue prompt_password, bool fail_on_error)
{
	PGconn	   *conn;
	bool		new_pass;
	const char *remoteversion_str;
	int			my_version;
	static char *password = NULL;
	const char **keywords = NULL;
	const char **values = NULL;
	PQconninfoOption *conn_opts = NULL;

	if (prompt_password == TRI_YES && !password)
		password = simple_prompt("Password: ", 100, false);

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
				fprintf(stderr, "%s: %s", progname, err_msg);
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
		if (password)
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
			fprintf(stderr, _("%s: could not connect to database \"%s\"\n"),
					progname, dbname);
			exit_nicely(1);
		}

		if (PQstatus(conn) == CONNECTION_BAD &&
			PQconnectionNeedsPassword(conn) &&
			password == NULL &&
			prompt_password != TRI_NO)
		{
			PQfinish(conn);
			password = simple_prompt("Password: ", 100, false);
			new_pass = true;
		}
	} while (new_pass);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		if (fail_on_error)
		{
			fprintf(stderr,
					_("%s: could not connect to database \"%s\": %s"),
					progname, dbname, PQerrorMessage(conn));
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
		fprintf(stderr, _("%s: could not get server version\n"), progname);
		exit_nicely(1);
	}
	server_version = PQserverVersion(conn);
	if (server_version == 0)
	{
		fprintf(stderr, _("%s: could not parse server version \"%s\"\n"),
				progname, remoteversion_str);
		exit_nicely(1);
	}

	my_version = PG_VERSION_NUM;

	/*
	 * We allow the server to be back to 8.3, and up to any minor release of
	 * our own major version.  (See also version check in pg_dump.c.)
	 */
	if (my_version != server_version
		&& (server_version < GPDB5_MAJOR_PGVERSION ||		/* we can handle back to 8.3 */
			(server_version / 100) > (my_version / 100)))
	{
		fprintf(stderr, _("server version: %s; %s version: %s\n"),
				remoteversion_str, progname, PG_VERSION);
		fprintf(stderr, _("aborting because of server version mismatch\n"));
		exit_nicely(1);
	}

	/*
	 * On 7.3 and later, make sure we are not fooled by non-system schemas in
	 * the search path.
	 */
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

	if (verbose)
		fprintf(stderr, _("%s: executing %s\n"), progname, query);

	res = PQexec(conn, query);
	if (!res ||
		PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		fprintf(stderr, _("%s: query failed: %s"),
				progname, PQerrorMessage(conn));
		fprintf(stderr, _("%s: query was: %s\n"),
				progname, query);
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

	if (verbose)
		fprintf(stderr, _("%s: executing %s\n"), progname, query);

	res = PQexec(conn, query);
	if (!res ||
		PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, _("%s: query failed: %s"),
				progname, PQerrorMessage(conn));
		fprintf(stderr, _("%s: query was: %s\n"),
				progname, query);
		PQfinish(conn);
		exit_nicely(1);
	}

	PQclear(res);
}


/*
 * dumpTimestamp
 */
static void
dumpTimestamp(char *msg)
{
	char		buf[256];
	time_t		now = time(NULL);

	/*
	 * We don't print the timezone on Win32, because the names are long and
	 * localized, which means they may contain characters in various random
	 * encodings; this has been seen to cause encoding errors when reading the
	 * dump script.
	 */
	if (strftime(buf, sizeof(buf),
#ifndef WIN32
				 "%Y-%m-%d %H:%M:%S %Z",
#else
				 "%Y-%m-%d %H:%M:%S",
#endif
				 localtime(&now)) != 0)
		fprintf(OPF, "-- %s %s\n\n", msg, buf);
}
