/*-------------------------------------------------------------------------
 *
 * pg_regress_main --- regression test for the main backend
 *
 * This is a C implementation of the previous shell script for running
 * the regression tests, and should be mostly compatible with it.
 * Initial author of C translation: Magnus Hagander
 *
 * This code is released under the terms of the PostgreSQL License.
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/test/regress/pg_regress_main.c
 *
 *-------------------------------------------------------------------------
 */

#include "pg_regress.h"

#include <sys/stat.h>
#include <sys/types.h>

/*
 * start a psql test process for specified file (including redirection),
 * and return process ID
 */
static PID_TYPE
psql_start_test(const char *testname,
				_stringlist **resultfiles,
				_stringlist **expectfiles,
				_stringlist **tags)
{
	PID_TYPE	pid;
	char		infile[MAXPGPATH];
	char		outfile[MAXPGPATH];
	char		expectfile[MAXPGPATH] = "";
	char		psql_cmd[MAXPGPATH * 4];
	size_t		offset = 0;
	char		use_utility_mode = 0;
	char	   *lastslash;

	/* generalise later */
	if (strcmp(testname, "upg2") == 0)
		use_utility_mode = 1;

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
				exit_nicely(2);
			}
		}
	}

	snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out",
			 outputdir, testname);
	if (!file_exists(expectfile))
		snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out",
				 inputdir, testname);

	add_stringlist_item(resultfiles, outfile);
	add_stringlist_item(expectfiles, expectfile);

	if (launcher)
	{
		offset += snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset,
						   "%s ", launcher);
		if (offset >= sizeof(psql_cmd))
		{
			fprintf(stderr, _("command too long\n"));
			exit(2);
		}
	}

	/*
	 * We need to pass multiple input files (prehook and infile) to psql,
	 * to do this a simple way is to execute it like this:
	 *
	 *     cat prehook infile | psql
	 *
	 * However the problem is that cat's pid is returned, although it does not
	 * really matter we prefer to return psql's pid.  We could change the
	 * command like this:
	 *
	 *     psql <(prehook infile)
	 *
	 * However some shells like dash do not support it.  So we have to
	 * execute the command as below:
	 *
	 *     psql <<EOF
	 *     $(cat prehook infile)
	 *     EOF
	 */
	offset += snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset,
					   "%s \"%s%spsql\" -X -a -q -d \"%s\" > \"%s\" 2>&1 <<EOF\n"
					   "$(cat \"%s\" \"%s\")\n"
					   "EOF",
					   use_utility_mode ? "env PGOPTIONS='-c gp_role=utility'" : "",
					   bindir ? bindir : "",
					   bindir ? "/" : "",
					   dblist->str,
					   outfile,
					   prehook[0] ? prehook : "/dev/null",
					   infile);
	if (offset >= sizeof(psql_cmd))
	{
		fprintf(stderr, _("command too long\n"));
		exit(2);
	}

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
psql_init(int argc, char **argv)
{
	/* set default regression database name */
	add_stringlist_item(&dblist, "regression");
}

int
main(int argc, char *argv[])
{
	return regression_main(argc, argv, psql_init, psql_start_test);
}
