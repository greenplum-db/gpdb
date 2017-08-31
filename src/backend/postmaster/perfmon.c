/*-------------------------------------------------------------------------
 *
 * perfmon.c
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * src/backend/postmaster/perfmon.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include <unistd.h>

#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "postmaster/fork_process.h"
#include "postmaster/perfmon.h"
#include "storage/ipc.h"
#include "utils/ps_status.h"
#include "cdb/cdbvars.h"

#include "gpmon/gpmon.h"

/*
 * FUNCTION PROTOTYPES
 */
#ifdef EXEC_BACKEND
static pid_t perfmon_forkexec(void);
#endif /* EXEC_BACKEND */
NON_EXEC_STATIC void PerfmonMain(int argc, char *argv[]);

/*
 * Main entry point for perfmon process.
 */
int
perfmon_start(void)
{
	pid_t		PerfmonPID;

#ifdef EXEC_BACKEND
	switch ((PerfmonPID = perfmon_forkexec()))
#else
	switch ((PerfmonPID = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork perfmon process: %m")));
			return 0;
#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			PerfmonMain(0, NULL);
			break;
#endif
		default:
			return (int)PerfmonPID;
	}

	/* shouldn't get here */
	Assert(false);

	return 0;
}


#ifdef EXEC_BACKEND
/*
 * perfmon_forkexec()
 *
 * Format up the arglist for the perfmon process, then fork and exec.
 */
static pid_t
perfmon_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--forkperfmon";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}
#endif   /* EXEC_BACKEND */


NON_EXEC_STATIC void
PerfmonMain(int argc, char *argv[])
{
	char		gpmmon_bin[MAXPGPATH] = {'\0'};
	char		gpmmon_cfg_file[MAXPGPATH] = {'\0'};
	char	   *av[10] = {NULL};
	char		port[6] = {'\0'};
	int			ac = 0;
	int			ret = 0;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/* Find gpmmon executable */
	if ((ret = find_other_exec(my_exec_path, "gpmmon",
			GPMMON_PACKET_VERSION_STRING, gpmmon_bin)) < 0)
	{
		elog(FATAL,"Failed to find gpmmon executable: %s (%s)", gpmmon_bin,
				GPMMON_PACKET_VERSION_STRING);
		proc_exit(0);
	}

	snprintf(gpmmon_cfg_file, MAXPGPATH, "%s/gpperfmon/conf/gpperfmon.conf",
				data_directory);

	snprintf(port, 6, "%d", PostPortNumber);

	av[ac++] = gpmmon_bin;
	av[ac++] = "-D";
	av[ac++] = gpmmon_cfg_file;
	av[ac++] = "-p";
	av[ac++] = port;
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	/* exec gpmmon now */
	if (execv(gpmmon_bin, av) < 0)
	{
		elog(FATAL, "could not execute server process \"%s\"", gpmmon_bin);
		proc_exit(0);
	}
}
