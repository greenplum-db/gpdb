/*
 *	reporting.c
 *
 *	runtime reporting functions
 *
 *	Copyright (c) 2017-Present, VMware, Inc. or its affiliates.
 */

#include "pg_upgrade_greenplum.h"

#include <time.h>


static FILE			   *progress_file = NULL;
static int				progress_id = 0;
static int				progress_counter = 0;
static unsigned long	progress_prev = 0;

/* Number of operations per progress report file */
#define OP_PER_PROGRESS	25
#define TS_PER_PROGRESS (5 * 1000000)

static char *
opname(progress_type op)
{
	char *ret = "unknown";

	switch(op)
	{
		case CHECK:
			ret = "check";
			break;
		case SCHEMA_DUMP:
			ret = "dump";
			break;
		case SCHEMA_RESTORE:
			ret = "restore";
			break;
		case FILE_MAP:
			ret = "map";
			break;
		case FILE_COPY:
			ret = "copy";
			break;
		case FIXUP:
			ret = "fixup";
			break;
		case ABORT:
			ret = "error";
			break;
		case DONE:
			ret = "done";
			break;
		default:
			break;
	}

	return ret;
}

static unsigned long
epoch_us(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);

	return (tv.tv_sec) * 1000000 + tv.tv_usec;
}

void
report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...)
{
	va_list			args;
	char			message[MAX_STRING];
	char			filename[MAXPGPATH];
	unsigned long	ts;

	if (!is_show_progress_mode())
		return;

	ts = epoch_us();

	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	if (!progress_file)
	{
		snprintf(filename, sizeof(filename), "%d.inprogress", ++progress_id);
		if ((progress_file = fopen(filename, "w")) == NULL)
			pg_log(PG_FATAL, "Could not create progress file:  %s\n",
				   filename);
	}

	/*
	 * GPDB_12_MERGE_FIXME: The CLUSTER_NAME macro was removed, so we've
	 * changed to printing the major version of the cluster instead. This may
	 * well be good enough (or even better), but some more thought should go
	 * into this before calling it done.
	 */
	fprintf(progress_file, "%lu;%s;%s;%s;\n",
			epoch_us(), cluster->major_version_str, opname(op), message);
	progress_counter++;

	/*
	 * Swap the progress report to a new file if we have exceeded the max
	 * number of operations per file as well as the minumum time per report. We
	 * want to avoid too frequent reports while still providing timely feedback
	 * to the user.
	 */
	if ((progress_counter > OP_PER_PROGRESS) && (ts > progress_prev + TS_PER_PROGRESS))
		close_progress();
}

void
close_progress(void)
{
	char	old[MAXPGPATH];
	char	new[MAXPGPATH];

	if (!is_show_progress_mode() || !progress_file)
		return;

	snprintf(old, sizeof(old), "%d.inprogress", progress_id);
	snprintf(new, sizeof(new), "%d.done", progress_id);

	fclose(progress_file);
	progress_file = NULL;

	rename(old, new);
	progress_counter = 0;
	progress_prev = epoch_us();
}
