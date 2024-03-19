#include "postgres_fe.h"

#include "pg_upgrade_greenplum.h"
#include "common/file_perm.h"

/*
 * This is a modified version of make_outputdirs.
 *
 * Use make_outputdirs_gp() when the user knows the exact directory to put the
 * files and logs that pg_upgrade generates. This function no longer creates
 * `pg_upgrade_output.d/<timestamp>` within the `pgdata`.
 *
 * Create and assign proper permissions to the set of output directories
 * used to store any data generated internally, filling in log_opts in
 * the process.
 */
void
make_outputdirs_gp(char *pgdata)
{
	FILE	   *fp;
	char	  **filename;
	time_t		run_time = time(NULL);
	char		filename_path[MAXPGPATH];
	int			len;

	log_opts.rootdir = (char *) pg_malloc0(MAXPGPATH);
	len = snprintf(log_opts.rootdir, MAXPGPATH, "%s", pgdata);
	if (len >= MAXPGPATH)
		pg_fatal("directory path for new cluster is too long\n");

	/* keep basedir for upstream code compatibility even though rootdir and
	 * basedir is the same.
	 */
	log_opts.basedir = (char *) pg_malloc0(MAXPGPATH);
	len = snprintf(log_opts.basedir, MAXPGPATH, "%s", log_opts.rootdir);
	if (len >= MAXPGPATH)
		pg_fatal("directory path for new cluster is too long\n");

	/* BASE_OUTPUTDIR/dump/ */
	log_opts.dumpdir = (char *) pg_malloc0(MAXPGPATH);
	len = snprintf(log_opts.dumpdir, MAXPGPATH, "%s/%s", log_opts.rootdir,
				   DUMP_OUTPUTDIR);
	if (len >= MAXPGPATH)
		pg_fatal("directory path for new cluster is too long\n");

	/* BASE_OUTPUTDIR/log/ */
	log_opts.logdir = (char *) pg_malloc0(MAXPGPATH);
	len = snprintf(log_opts.logdir, MAXPGPATH, "%s/%s", log_opts.rootdir,
				   LOG_OUTPUTDIR);
	if (len >= MAXPGPATH)
		pg_fatal("directory path for new cluster is too long\n");

	/*
	 * Ignore the error case where the root path exists, as it is kept the
	 * same across runs.
	 */
	if (mkdir(log_opts.rootdir, pg_dir_create_mode) < 0 && errno != EEXIST)
		pg_fatal("could not create directory \"%s\": %m\n", log_opts.rootdir);
	if (mkdir(log_opts.dumpdir, pg_dir_create_mode) < 0)
		pg_fatal("could not create directory \"%s\": %m\n", log_opts.dumpdir);
	if (mkdir(log_opts.logdir, pg_dir_create_mode) < 0)
		pg_fatal("could not create directory \"%s\": %m\n", log_opts.logdir);

	len = snprintf(filename_path, sizeof(filename_path), "%s/%s",
				   log_opts.logdir, INTERNAL_LOG_FILE);
	if (len >= sizeof(filename_path))
		pg_fatal("directory path for new cluster is too long\n");

	if ((log_opts.internal = fopen_priv(filename_path, "a")) == NULL)
		pg_fatal("could not open log file \"%s\": %m\n", filename_path);

	/* label start of upgrade in logfiles */
	for (filename = output_files; *filename != NULL; filename++)
	{
		len = snprintf(filename_path, sizeof(filename_path), "%s/%s",
					   log_opts.logdir, *filename);
		if (len >= sizeof(filename_path))
			pg_fatal("directory path for new cluster is too long\n");
		if ((fp = fopen_priv(filename_path, "a")) == NULL)
			pg_fatal("could not write to log file \"%s\": %m\n", filename_path);

		fprintf(fp,
				"-----------------------------------------------------------------\n"
				"  pg_upgrade run on %s"
				"-----------------------------------------------------------------\n\n",
				ctime(&run_time));
		fclose(fp);
	}
}
