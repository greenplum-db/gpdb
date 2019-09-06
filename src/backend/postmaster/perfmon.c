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
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "cdb/cdbvars.h"

#include "gpmon/gpmon.h"

bool
PerfmonStartRule(Datum main_arg)
{
	if (IsUnderMasterDispatchMode() &&
		gp_enable_gpperfmon)
		return true;

	return false;
}

void
PerfmonMain(Datum main_arg)
{
	char		gpmmon_bin[MAXPGPATH] = {'\0'};
	char		gpmmon_cfg_file[MAXPGPATH] = {'\0'};
	char	   *av[10] = {NULL};
	char		port[6] = {'\0'};
	int			ac = 0;
	int			ret = 0;

	/* Find gpmmon executable */
	if ((ret = find_other_exec(my_exec_path, "gpmmon",
			GPMMON_PACKET_VERSION_STRING, gpmmon_bin)) < 0)
	{
		elog(FATAL,"Failed to find gpmmon executable: %s (%s)", gpmmon_bin,
				GPMMON_PACKET_VERSION_STRING);
		proc_exit(0);
	}

	/*
	 * gpperfmon.conf is always in the data directory, so we can reference it
	 * by simple relative paths.
	 */
	snprintf(gpmmon_cfg_file, MAXPGPATH, "./gpperfmon/conf/gpperfmon.conf");

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
