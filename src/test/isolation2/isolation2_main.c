/*-------------------------------------------------------------------------
 *
 * isolation2_main --- pg_regress test launcher for Python isolation tests
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/test/isolation2/isolation2_main.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include "pg_regress.h"

#include <sys/stat.h>
#include <sys/types.h>

/*
 * start a Python isolation tester process for specified file (including
 * redirection), and return process ID
 */
static PID_TYPE
isolation_start_test(const char *testname,
					 _stringlist ** resultfiles,
					 _stringlist ** expectfiles,
					 _stringlist ** tags)
{
	PID_TYPE	pid;
	char		infile[MAXPGPATH];
	char		outfile[MAXPGPATH];
	char		expectfile[MAXPGPATH] = "";
	char		psql_cmd[MAXPGPATH * 3];
	size_t		offset = 0;
	char	   *lastslash;

	/*
	 * Look for files in the output dir first, consistent with a vpath search.
	 * This is mainly to create more reasonable error messages if the file is
	 * not found.  It also allows local test overrides when running pg_regress
	 * outside of the source tree.
	 */
	snprintf(infile, sizeof(infile), "%s/sql/%s.sql",
			 outputdir, testname);
	if (!file_exists(infile))
		snprintf(infile, sizeof(infile), "%s/sql/%s.sql",
				 inputdir, testname);

	snprintf(outfile, sizeof(outfile), "%s/results/%s.out",
			 outputdir, testname);

	/*
	 * If the test name contains slashes, create intermediary results
	 * directory.
	 */
	if ((lastslash = strrchr(outfile, '/')) != NULL)
	{
		char		resultdir[MAXPGPATH];

		memcpy(resultdir, outfile, lastslash - outfile);
		resultdir[lastslash - outfile] = '\0';
		if (mkdir(resultdir, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
		{
			if (errno == EEXIST)
			{
				/* exists already, that's OK */
			}
			else
			{
				fprintf(stderr, _("could not create directory \"%s\": %s\n"),
						resultdir, strerror(errno));
				exit(2);
			}
		}
	}

	if (optimizer_enabled)
	{
		snprintf(expectfile, sizeof(expectfile), "%s/expected/%s_optimizer.out",
				 outputdir, testname);
		if (!file_exists(expectfile))
		{
			snprintf(expectfile, sizeof(expectfile), "%s/expected/%s_optimizer.out",
					 inputdir, testname);
		}
	}

	// if optimizer is off or there is no orca-specific answer file, then
	// use the default answer file

	if (!file_exists(expectfile))
	{
		snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out",
				 outputdir, testname);

		if (!file_exists(expectfile))
		{
			snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out",
					 inputdir, testname);
		}
	}

	add_stringlist_item(resultfiles, outfile);
	add_stringlist_item(expectfiles, expectfile);

	if (launcher)
		offset += snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset,
						   "%s ", launcher);

	snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset,
			 "python3 ./sql_isolation_testcase.py --dbname=\"%s\" --initfile_prefix=\"%s\" < \"%s\" > \"%s\" 2>&1",
			 dblist->str,
			 outfile,
			 infile,
			 outfile);

	pid = spawn_process(psql_cmd);

	if (pid == INVALID_PID)
	{
		fprintf(stderr, _("could not start process for test %s\n"),
				testname);
		exit(2);
	}

	return pid;
}

static void
isolation_init(int argc, char **argv)
{
	/* set default regression database name */
	add_stringlist_item(&dblist, "isolation2test");

	/* run setup test as prerequisite for running tests */
	add_stringlist_item(&setup_tests, "setup");
}

int
main(int argc, char *argv[])
{
	return regression_main(argc, argv, isolation_init, isolation_start_test);
}
