#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "pg_config.h"
#include "fe-auth.h"
#include "cdb_dump_util.h"
#include "getopt_long.h"

#ifdef USE_NETBACKUP

#include "cdb_bsa_util.h"

char					*progname;
char					*netbackupRestoreFilename = NULL;
char					*netbackupServiceHost = NULL;
char					*netbackupQueryObject = NULL;
extern char				*optarg;
extern int				optind;

static void help(const char *progname);

int
main(int argc, char *argv[])
{
	int c;

	static struct option long_options[] = {
		{"netbackup-service-host", required_argument, NULL, 1},
		{"netbackup-filename", required_argument, NULL, 2},
		{"netbackup-list-dumped-objects", required_argument, NULL, 3},
		{NULL, 0, NULL, 0}
	};

	int optindex;
	progname = (char *)get_progname(argv[0]);

	if (argc > 1)
	{
		if ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-?") == 0))
		{
			help(progname);
			exit(0);
		}
		if ((strcmp(argv[1], "--version") == 0) || (strcmp(argv[1], "-V") == 0))
		{
			puts("pg_dump (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	while((c = getopt_long(argc, argv, "12", long_options, &optindex)) != -1)
	{
		switch(c)
		{
			case 1:
				netbackupServiceHost = Safe_strdup(optarg);
				break;
			case 2:
				netbackupRestoreFilename = Safe_strdup(optarg);
				break;
			case 3:
				netbackupQueryObject = Safe_strdup(optarg);
				break;
			default:
				fprintf(stderr, _("Wrong option entered. Try \"%s --help\" for more information.\n"), progname);
				exit(1);
		}
	}

	if (optind < argc)
	{
		fprintf(stderr, _("%s: Too many command-line arguments (first is \"%s\")\n"), progname, argv[optind + 1]);
		fprintf(stderr, _("Try \"%s --help\" for more information\n"), progname);
		exit(1);
	}

	if (netbackupServiceHost == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Need to provide --netbackup-service-host mandatory option.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if ((netbackupRestoreFilename == NULL) && (netbackupQueryObject == NULL))
	{
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Need to provide NetBackup Dump File Path or NetBackup Query Object.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}
	else if (netbackupRestoreFilename && netbackupQueryObject)
	{
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Cannot provide NetBackup Dump File Path and NetBackup Query Object together.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (initBSARestoreSession(netbackupServiceHost) != 0)
	{
		mpp_err_msg("ERROR", "gp_bsa_query_agent", "Failed to initialize the NetBackup BSA session to query BSA object\n");
		exit(1);
	}

	if (netbackupRestoreFilename)
	{
		if (queryBSAObject(netbackupRestoreFilename) == NULL)
		{
			mpp_err_msg("INFO", "gp_bsa_query_agent", "Query to NetBackup server failed for object: %s\n", netbackupRestoreFilename);
		}
		else
		{
			printf("%s\n", netbackupRestoreFilename);
		}
	}
	else if (netbackupQueryObject)
	{
		if (searchBSAObject(netbackupQueryObject) != 0)
		{
			mpp_err_msg("INFO", "gp_bsa_query_agent", "No objects of the format '%s' found on the NetBackup server\n", netbackupQueryObject);
		}
	}

	exit(0);
}

static void
help(const char *progname)
{
	printf(_("\n%s queries the given object to NetBackup server to determine if it has been backed up using NetBackup\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("	%s [OPTION]... [FILENAME]"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  --netbackup-service-host=HOSTNAME  \n"));
	printf(_("  NetBackup master server hostname.\n"));
	printf(_("  NetBackup master server hostname should be provided to query the NetBackup server.\n\n\n"));
	printf(_("  --netbackup-filename=FILENAME  \n"));
	printf(_("  NetBackup filename to be queried.\n"));
	printf(_("  NetBackup filename should be provided to query the NetBackup server.\n\n\n"));
	printf(_("  --netbackup-list-dumped-objects=OBJREGEX  \n"));
	printf(_("  NetBackup objects to be queried.\n"));
	printf(_("  NetBackup objects to be queried can be provided in the form of a regular expression\n\n\n"));
}

#else
int main(int argc, char **argv)
{
	mpp_err_msg("INFO", "gp_bsa_query_agent", "\ngp_bsa_query_agent is not supported on this platform\n\n");
	exit(0);
}

#endif
