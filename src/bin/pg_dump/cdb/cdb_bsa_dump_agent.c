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
char					*netbackupDumpFilename = NULL;
char					*netbackupServiceHost = NULL;
char					*netbackupPolicy = NULL;
char					*netbackupSchedule = NULL;
int						netbackupBlockSize = 512;
char                   	*netbackupKeyword = NULL;
extern char				*optarg;
extern int				optind;

static void help(const char *progname);

int
main(int argc, char *argv[])
{
	int c;

	static struct option long_options[] = {
		{"netbackup-service-host", required_argument, NULL, 1},
		{"netbackup-policy", required_argument, NULL, 2},
		{"netbackup-schedule", required_argument, NULL, 3},
		{"netbackup-filename", required_argument, NULL, 4},
		{"netbackup-block-size", required_argument, NULL, 5},
		{"netbackup-keyword", required_argument, NULL, 6},
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

	while((c = getopt_long(argc, argv, "123456", long_options, &optindex)) != -1)
	{
		switch(c)
		{
			case 1:
				netbackupServiceHost = Safe_strdup(optarg);
				break;
			case 2:
				netbackupPolicy = Safe_strdup(optarg);
				break;
			case 3:
				netbackupSchedule = Safe_strdup(optarg);
				break;
			case 4:
				netbackupDumpFilename = Safe_strdup(optarg);
				break;
			case 5:
				netbackupBlockSize = atoi(optarg);
				break;
			case 6:
				netbackupKeyword = Safe_strdup(optarg);
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

	if (netbackupPolicy == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Need to provide --netbackup-policy mandatory param.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (netbackupSchedule == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Need to provide --netbackup-schedule mandatory param.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if (netbackupDumpFilename == NULL)
	{
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Need to provide NetBackup Dump File Path.\nTry \"%s --help\" for more information\n", progname);
		exit(1);
	}

	if(initBSADumpSession(netbackupServiceHost, netbackupPolicy, netbackupSchedule, netbackupKeyword) != 0){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to initialize NetBackup BSA session for Backup\n");
		exit(1);
	}

	if(createBSADumpObject(netbackupDumpFilename) != 0){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to create NetBackup BSA object to perform Backup\n");
		exit(1);
	}

	if(sendBSADumpData(netbackupBlockSize) != 0){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to send data through NetBackup BSA to perform Backup\n");
		exit(1);
	}

	if(endBSADumpSession() != 0){
		mpp_err_msg("ERROR", "gp_bsa_dump_agent", "Failed to end the NetBackup BSA session after completion of Backup\n");
		exit(1);
	}

	exit(0);
}

static void
help(const char *progname)
{
	printf(_("\n%s utility dumps the given data to NetBackup.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION]... [FILENAME]"), progname);

	printf(_("\nGeneral options:\n"));
	printf(_("  --netbackup-service-host=HOSTNAME  \n"));
	printf(_("  NetBackup master server hostname.\n"));
	printf(_("  NetBackup master server hostname should be provided to perform backup using NetBackup.\n\n\n"));
	printf(_("  --netbackup-policy=POLICY  \n"));
	printf(_("  NetBackup policy to be used for backup.\n"));
	printf(_("  NetBackup policy should be provided to perform backup using NetBackup.\n"));
	printf(_("  NetBackup policy should be of Datastore type to backup GPDB using NetBackup.\n\n\n"));
	printf(_("  --netbackup-schedule=SCHEDULE  \n"));
	printf(_("  NetBackup schedule to be used for backup.\n"));
	printf(_("  NetBackup schedule should be provided to perform backup using NetBackup.\n\n\n"));
	printf(_("  --netbackup-filename=FILENAME  \n"));
	printf(_("  NetBackup dump file path to be used for backup.\n"));
	printf(_("  NetBackup dump file path should be provided to perform backup using NetBackup.\n\n\n"));
	printf(_("  --netbackup-block-size=BLOCKSIZE  \n"));
	printf(_("  NetBackup data transfer block size to be used for backup.\n"));
	printf(_("  This is an optional param. The default data transfer block size is 512.\n\n\n"));
	printf(_("  --netbackup-keyword=KEYWORD  \n"));
	printf(_("  NetBackup Keyword to be used for backup.\n"));
	printf(_("  This is an optional param.\n"));

	printf(_("\nIf no dump file name is provided the utility will fail.\n"));
}

#else
int main(int argc, char *argv[])
{
	mpp_err_msg("INFO", "gp_bsa_dump_agent", "\ngp_bsa_dump_agent is not supported on this platform\n\n");
	exit(0);
}

#endif
