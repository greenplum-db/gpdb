#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "pg_config.h"
#include "fe-auth.h"
#include "cdb_dump_util.h"
#include "getopt_long.h"

#ifdef USE_NETBACKUP

#include "cdb_bsa_util.h"

char					*progname;
char					*netbackupRestoreFilename = NULL;
char					*netbackupServiceHost = NULL;
int						netbackupBlockSize = 512;
extern char				*optarg;
extern int				optind;

static void help(const char *progname);

int
main(int argc, char *argv[]){

	int c;

	static struct option long_options[] = {
		{"netbackup-service-host", required_argument, NULL, 1},
		{"netbackup-filename", required_argument, NULL, 2},
		{"netbackup-block-size", required_argument, NULL, 3},
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

	while((c = getopt_long(argc, argv, "123", long_options, &optindex)) != -1)
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
				netbackupBlockSize = atoi(optarg);
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
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Need to provide --netbackup-service-host mandatory param.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (netbackupRestoreFilename == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Need to provide NetBackup Dump File Path.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if(initBSARestoreSession(netbackupServiceHost) != 0){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to initialize the NetBackup BSA session to perform Restore\n");
		exit(1);
	}

	if(getBSARestoreObject(netbackupRestoreFilename) != 0){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to get the NetBackup BSA restore object to perform Restore\n");
		exit(1);
	}

	if(getBSARestoreData(netbackupBlockSize) != 0){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to get data from NetBackup in order to perform Restore\n");
		exit(1);
	}

	if(endBSARestoreSession() != 0){
		mpp_err_msg("ERROR", "gp_bsa_restore_agent", "Failed to end the NetBackup BSA session after completion of Restore\n");
		exit(1);
	}

	exit(0);
}

static void
help(const char *progname)
{
	printf(_("\n%s utility restores data from NetBackup\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... [FILENAME]"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  --netbackup-service-host=HOSTNAME  \n"));
	printf(_("  NetBackup master server hostname.\n"));
	printf(_("  NetBackup master server hostname should be provided to perform restore using NetBackup.\n\n\n"));
	printf(_("  --netbackup-filename=FILENAME  \n"));
	printf(_("  NetBackup restore file path.  \n"));
	printf(_("  NetBackup restore file path should be provided to perform restore using NetBackup.\n\n\n"));
	printf(_("  --netbackup-block-size=BLOCKSIZE  \n"));
	printf(_("  NetBackup data transfer block size to be used for restore.\n"));
	printf(_("  This is an optional param. The default data transfer block size is 512.\n\n\n"));

	printf(_("\nIf no restore file name is provided the utility will fail.\n"));
}

#else
int main(int argc, char **argv)
{
	mpp_err_msg("INFO", "gp_bsa_restore_agent", "\ngp_bsa_restore_agent is not supported on this platform\n\n");
	exit(0);
}

#endif
