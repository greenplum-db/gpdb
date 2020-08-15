/*
 *	reporting.c
 *
 *	runtime reporting functions
 *
 *	Copyright (c) 2017-Present, Pivotal Software Inc.
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

	fprintf(progress_file, "%lu;%s;%s;%s;\n",
			epoch_us(), CLUSTER_NAME(cluster), opname(op), message);
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

void
duration(instr_time duration, char *buf, size_t len)
{
	int seconds = INSTR_TIME_GET_DOUBLE(duration);
	// convert total seconds to hour, minute and seconds
	int h = (seconds / 3600);
	int m = (seconds - (3600 * h)) / 60;
	int s = (seconds - (3600 * h) - (60 * m));
	if (h > 0) {
		snprintf(buf, len, "%dh%dm%ds", h, m, s);
	} else if (m > 0) {
		snprintf(buf, len, "%dm%ds", m, s);
	} else if (s > 0){
		snprintf(buf, len, "%ds", s);
	} else {
		snprintf(buf, len, "%.3fms", INSTR_TIME_GET_MILLISEC(duration));
	}
}

void
log_with_timing(step_timer *st, const char *msg)
{
	size_t len=20;
	char elapsed_time[len];
	Assert(st->start_time.tv_sec != 0);
	INSTR_TIME_SET_CURRENT(st->end_time);
	INSTR_TIME_SUBTRACT(st->end_time, st->start_time);
	duration(st->end_time, elapsed_time, len);

	report_status(PG_REPORT, "%s %s", msg, elapsed_time);
	fflush(stdout);

	INSTR_TIME_SET_ZERO(st->start_time);
	INSTR_TIME_SET_ZERO(st->end_time);
}
