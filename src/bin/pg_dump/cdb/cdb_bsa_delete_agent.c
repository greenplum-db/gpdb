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

char					*progname = NULL;
char					*netbackupServiceHost = NULL;
char					*netbackupDeleteObject = NULL;
extern char				*optarg;
extern int				optind;

static void help(const char *progname);

int
main(int argc, char *argv[])
{
	int c;

	static struct option long_options[] = {
		{"netbackup-service-host", required_argument, NULL, 1},
		{"netbackup-delete-objects", required_argument, NULL, 2},
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
				netbackupDeleteObject = Safe_strdup(optarg);
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
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "Need to provide --netbackup-service-host mandatory option.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (netbackupDeleteObject == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "Need to provide NetBackup Delete Object.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (initBSARestoreSession(netbackupServiceHost) != 0) {
		mpp_err_msg("ERROR", "gp_bsa_delete_agent", "Failed to initialize the NetBackup BSA session to query BSA object\n");
		exit(1);
	}

	if (deleteBSAObjects(netbackupDeleteObject) != 0) {
		mpp_err_msg("INFO", "gp_bsa_delete_agent", "No objects of the format '%s' found on the NetBackup server or could not delete objecs of the format '%s'\n", netbackupDeleteObject, netbackupDeleteObject);
		exit(1);
	}

	exit(0);
}

static void
help(const char *progname)
{
	printf(_("\n%s queries the given object format to NetBackup server and deletes all matching objects found on the NetBackup server\n\n"), progname);
	printf(_("\n%s This utility should be used carefully since it will delete the specified objects on NetBackup server\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("	%s [OPTION]... [FILENAME]"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  --netbackup-service-host=HOSTNAME  \n"));
	printf(_("  NetBackup master server hostname.\n"));
	printf(_("  NetBackup master server hostname should be provided to delete objects on that server.\n\n\n"));
	printf(_("  --netbackup-delete-objects=OBJREGEX  \n"));
	printf(_("  NetBackup objects to be deleted.\n"));
	printf(_("  NetBackup objects to be deleted can be provided in the form of a regular expression\n\n\n"));
}

#else
int main(int argc, char **argv)
{
	mpp_err_msg("INFO", "gp_bsa_delete_agent", "\ngp_bsa_delete_agent is not supported on this platform\n\n");
	exit(0);
}

#endif
