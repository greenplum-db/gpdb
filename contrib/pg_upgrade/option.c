/*
 *	opt.c
 *
 *	options functions
 *
 *	Copyright (c) 2010, PostgreSQL Global Development Group
 *	$PostgreSQL: pgsql/contrib/pg_upgrade/option.c,v 1.12.2.1 2010/07/13 20:15:51 momjian Exp $
 */

#include "pg_upgrade.h"

#include "getopt_long.h"

#ifdef WIN32
#include <io.h>
#endif


static void usage(migratorContext *ctx);
static void validateDirectoryOption(migratorContext *ctx, char **dirpath,
				   char *envVarName, char *cmdLineOption, char *description);
static void get_pkglibdirs(migratorContext *ctx);
static char *get_pkglibdir(migratorContext *ctx, const char *bindir);


/*
 * parseCommandLine()
 *
 *	Parses the command line (argc, argv[]) into the given migratorContext object
 *	and initializes the rest of the object.
 */
void
parseCommandLine(migratorContext *ctx, int argc, char *argv[])
{
	static struct option long_options[] = {
		{"old-datadir", required_argument, NULL, 'd'},
		{"new-datadir", required_argument, NULL, 'D'},
		{"old-bindir", required_argument, NULL, 'b'},
		{"new-bindir", required_argument, NULL, 'B'},
		{"old-port", required_argument, NULL, 'p'},
		{"new-port", required_argument, NULL, 'P'},

		{"user", required_argument, NULL, 'u'},
		{"check", no_argument, NULL, 'c'},
		{"debug", no_argument, NULL, 'g'},
		{"debugfile", required_argument, NULL, 'G'},
		{"link", no_argument, NULL, 'k'},
		{"logfile", required_argument, NULL, 'l'},
		{"verbose", no_argument, NULL, 'v'},
		{"progress", no_argument, NULL, 'X'},
		{"add-checksum", no_argument, NULL, 'J'},
		{"remove-checksum", no_argument, NULL, 'j'},

		{"dispatcher-mode", no_argument, NULL, 1},

		{NULL, 0, NULL, 0}
	};
	int			option;			/* Command line option */
	int			optindex = 0;	/* used by getopt_long */
	int			user_id;

	if (getenv("PGUSER"))
	{
		pg_free(ctx->user);
		ctx->user = pg_strdup(ctx, getenv("PGUSER"));
	}

	ctx->progname = get_progname(argv[0]);
	ctx->old.port = getenv("PGPORT") ? atoi(getenv("PGPORT")) : DEF_PGPORT;
	ctx->new.port = getenv("PGPORT") ? atoi(getenv("PGPORT")) : DEF_PGPORT;
	/* must save value, getenv()'s pointer is not stable */

	ctx->transfer_mode = TRANSFER_MODE_COPY;

	/* user lookup and 'root' test must be split because of usage() */
	user_id = get_user_info(ctx, &ctx->user);

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
			strcmp(argv[1], "-?") == 0)
		{
			usage(ctx);
			exit_nicely(ctx, false);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			pg_log(ctx, PG_REPORT, "pg_upgrade " PG_VERSION "\n");
			exit_nicely(ctx, false);
		}
	}

	if (user_id == 0)
		pg_log(ctx, PG_FATAL, "%s: cannot be run as root\n", ctx->progname);

	getcwd(ctx->cwd, MAXPGPATH);

	while ((option = getopt_long(argc, argv, "d:D:b:B:cgG:jJkl:p:P:u:v",
								 long_options, &optindex)) != -1)
	{
		switch (option)
		{
			case 'd':
				ctx->old.pgdata = pg_strdup(ctx, optarg);
				break;

			case 'D':
				ctx->new.pgdata = pg_strdup(ctx, optarg);
				break;

			case 'b':
				ctx->old.bindir = pg_strdup(ctx, optarg);
				break;

			case 'B':
				ctx->new.bindir = pg_strdup(ctx, optarg);
				break;

			case 'c':
				ctx->check = true;
				break;

			case 'g':
				pg_log(ctx, PG_REPORT, "Running in debug mode\n");
				ctx->debug = true;
				break;

			case 'G':
				if ((ctx->debug_fd = fopen(optarg, "w")) == NULL)
				{
					pg_log(ctx, PG_FATAL, "cannot open debug file\n");
					exit_nicely(ctx, false);
				}
				break;

			case 'j':
				ctx->checksum_mode = CHECKSUM_REMOVE;
				break;

			case 'J':
				ctx->checksum_mode = CHECKSUM_ADD;
				break;

			case 'k':
				ctx->transfer_mode = TRANSFER_MODE_LINK;
				break;

			case 'l':
				ctx->logfile = pg_strdup(ctx, optarg);
				break;

			case 'p':
				if ((ctx->old.port = atoi(optarg)) <= 0)
				{
					pg_log(ctx, PG_FATAL, "invalid old port number\n");
					exit_nicely(ctx, false);
				}
				break;

			case 'P':
				if ((ctx->new.port = atoi(optarg)) <= 0)
				{
					pg_log(ctx, PG_FATAL, "invalid new port number\n");
					exit_nicely(ctx, false);
				}
				break;

			case 'u':
				pg_free(ctx->user);
				ctx->user = pg_strdup(ctx, optarg);
				break;

			case 'v':
				pg_log(ctx, PG_REPORT, "Running in verbose mode\n");
				ctx->verbose = true;
				break;

			case 'X':
				pg_log(ctx, PG_REPORT, "Running in progress report mode\n");
				ctx->progress = true;
				break;

			case 1:		/* --dispatcher-mode */
				/*
				 * XXX: Ideally, we could tell just by looking at the data
				 * directory, whether it's a QD node, or a QE node. You might
				 * think that we could look at Gp_role or gp_contentid, but
				 * alas, we pass those options on the pg_ctl command line,
				 * they are not stored permanently in the data directory
				 * itself, and we don't want to dig into the auxiliary
				 * config files created by gpinitsystem. So for now, the
				 * caller of pg_upgrade must use the --dispatcher-mode
				 * option, when upgrading the QD node.
				 */
				ctx->dispatcher_mode = true;
				break;

			default:
				pg_log(ctx, PG_FATAL,
					   "Try \"%s --help\" for more information.\n",
					   ctx->progname);
				break;
		}
	}

	if (ctx->logfile != NULL)
	{
		/*
		 * We must use append mode so output generated by child processes via
		 * ">>" will not be overwritten, and we want the file truncated on
		 * start.
		 */
		/* truncate */
		if ((ctx->log_fd = fopen(ctx->logfile, "w")) == NULL)
			pg_log(ctx, PG_FATAL, "Cannot write to log file %s\n", ctx->logfile);
		fclose(ctx->log_fd);
		if ((ctx->log_fd = fopen(ctx->logfile, "a")) == NULL)
			pg_log(ctx, PG_FATAL, "Cannot write to log file %s\n", ctx->logfile);
	}
	else
		ctx->logfile = pg_strdup(ctx, DEVNULL);

	/* if no debug file name, output to the terminal */
	if (ctx->debug && !ctx->debug_fd)
	{
		ctx->debug_fd = fopen(DEVTTY, "w");
		if (!ctx->debug_fd)
			pg_log(ctx, PG_FATAL, "Cannot write to terminal\n");
	}

	/* Ensure we are only adding checksums in copy mode */
	if (ctx->transfer_mode != TRANSFER_MODE_COPY &&
		ctx->checksum_mode != CHECKSUM_NONE)
		pg_log(ctx, PG_FATAL, "Adding and removing checksums only supported in copy mode.\n");

	/* Get values from env if not already set */
	validateDirectoryOption(ctx, &ctx->old.pgdata, "OLDDATADIR", "-d",
							"old cluster data resides");
	validateDirectoryOption(ctx, &ctx->new.pgdata, "NEWDATADIR", "-D",
							"new cluster data resides");
	validateDirectoryOption(ctx, &ctx->old.bindir, "OLDBINDIR", "-b",
							"old cluster binaries reside");
	validateDirectoryOption(ctx, &ctx->new.bindir, "NEWBINDIR", "-B",
							"new cluster binaries reside");

	get_pkglibdirs(ctx);
}


static void
usage(migratorContext *ctx)
{
	printf(_("\nUsage: pg_upgrade [OPTIONS]...\n\
\n\
Options:\n\
 -b, --old-bindir=old_bindir      old cluster executable directory\n\
 -B, --new-bindir=new_bindir      new cluster executable directory\n\
 -c, --check                      check clusters only, don't change any data\n\
 -d, --old-datadir=old_datadir    old cluster data directory\n\
 -D, --new-datadir=new_datadir    new cluster data directory\n\
 -g, --debug                      enable debugging\n\
 -G, --debugfile=debug_filename   output debugging activity to file\n\
 -j, --remove-checksum            remove data checksums when creating new cluster\n\
 -J, --add-checksum               add data checksumming to the new cluster\n\
 -k, --link                       link instead of copying files to new cluster\n\
 -l, --logfile=log_filename       log session activity to file\n\
 -p, --old-port=old_portnum       old cluster port number (default %d)\n\
 -P, --new-port=new_portnum       new cluster port number (default %d)\n\
 -u, --user=username              clusters superuser (default \"%s\")\n\
 -v, --verbose                    enable verbose output\n\
 -X, --progress                   enable progress reporting\n\
 -V, --version                    display version information, then exit\n\
 -h, --help                       show this help, then exit\n\
\n\
Before running pg_upgrade you must:\n\
  create a new database cluster (using the new version of initdb)\n\
  shutdown the postmaster servicing the old cluster\n\
  shutdown the postmaster servicing the new cluster\n\
\n\
When you run pg_upgrade, you must provide the following information:\n\
  the data directory for the old cluster  (-d OLDDATADIR)\n\
  the data directory for the new cluster  (-D NEWDATADIR)\n\
  the 'bin' directory for the old version (-b OLDBINDIR)\n\
  the 'bin' directory for the new version (-B NEWBINDIR)\n\
\n\
For example:\n\
  pg_upgrade -d oldCluster/data -D newCluster/data -b oldCluster/bin -B newCluster/bin\n\
or\n"), ctx->old.port, ctx->new.port, ctx->user);
#ifndef WIN32
	printf(_("\
  $ export OLDDATADIR=oldCluster/data\n\
  $ export NEWDATADIR=newCluster/data\n\
  $ export OLDBINDIR=oldCluster/bin\n\
  $ export NEWBINDIR=newCluster/bin\n\
  $ pg_upgrade\n"));
#else
	printf(_("\
  C:\\> set OLDDATADIR=oldCluster/data\n\
  C:\\> set NEWDATADIR=newCluster/data\n\
  C:\\> set OLDBINDIR=oldCluster/bin\n\
  C:\\> set NEWBINDIR=newCluster/bin\n\
  C:\\> pg_upgrade\n"));
#endif
}


/*
 * validateDirectoryOption()
 *
 * Validates a directory option.
 *	dirpath		  - the directory name supplied on the command line
 *	envVarName	  - the name of an environment variable to get if dirpath is NULL
 *	cmdLineOption - the command line option corresponds to this directory (-o, -O, -n, -N)
 *	description   - a description of this directory option
 *
 * We use the last two arguments to construct a meaningful error message if the
 * user hasn't provided the required directory name.
 */
static void
validateDirectoryOption(migratorContext *ctx, char **dirpath,
					char *envVarName, char *cmdLineOption, char *description)
{
	if (*dirpath == NULL || (strlen(*dirpath) == 0))
	{
		const char *envVar;

		if ((envVar = getenv(envVarName)) && strlen(envVar))
			*dirpath = pg_strdup(ctx, envVar);
		else
		{
			pg_log(ctx, PG_FATAL, "You must identify the directory where the %s\n"
				   "Please use the %s command-line option or the %s environment variable\n",
				   description, cmdLineOption, envVarName);
		}
	}

	/*
	 * Trim off any trailing path separators
	 */
#ifndef WIN32
	if ((*dirpath)[strlen(*dirpath) - 1] == '/')
#else
	if ((*dirpath)[strlen(*dirpath) - 1] == '/' ||
		(*dirpath)[strlen(*dirpath) - 1] == '\\')
#endif
		(*dirpath)[strlen(*dirpath) - 1] = 0;
}


static void
get_pkglibdirs(migratorContext *ctx)
{
	/*
	 * we do not need to know the libpath in the old cluster, and might not
	 * have a working pg_config to ask for it anyway.
	 */
	ctx->old.libpath = NULL;
	ctx->new.libpath = get_pkglibdir(ctx, ctx->new.bindir);
}


static char *
get_pkglibdir(migratorContext *ctx, const char *bindir)
{
	char		cmd[MAXPGPATH];
	char		bufin[MAX_STRING];
	FILE	   *output;
	int			i;

	snprintf(cmd, sizeof(cmd), "\"%s/pg_config\" --pkglibdir", bindir);

	if ((output = popen(cmd, "r")) == NULL)
		pg_log(ctx, PG_FATAL, "Could not get pkglibdir data: %s\n",
			   getErrorText(errno));

	fgets(bufin, sizeof(bufin), output);

	if (output)
		pclose(output);

	/* Remove trailing newline */
	i = strlen(bufin) - 1;

	if (bufin[i] == '\n')
		bufin[i] = '\0';

	return pg_strdup(ctx, bufin);
}
