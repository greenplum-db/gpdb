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
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/test/regress/pg_regress_main.c,v 1.6 2009/06/11 14:49:15 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "pg_regress.h"

#include <sys/stat.h>
#include <sys/types.h>

static bool
detect_answer_file(char *name,
				   int namesize,
				   const char *testname,
				   const char *variant)
{
	snprintf(name, namesize, "%s/expected/%s%s.out",
			 outputdir, testname, variant);

	if (file_exists(name))
		return true;

	snprintf(name, namesize, "%s/expected/%s%s.out",
			 inputdir, testname, variant);

	return file_exists(name);
}

/*
 * start a psql test process for specified file (including redirection),
 * and return process ID
 */
static PID_TYPE
psql_start_test(const char *testname,
				_stringlist ** resultfiles,
				_stringlist ** expectfiles,
				_stringlist ** tags)
{
	PID_TYPE	pid;
	char		infile[MAXPGPATH];
	char		outfile[MAXPGPATH];
	char		expectfile[MAXPGPATH] = "";
	char		psql_cmd[MAXPGPATH * 3];
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

	if      (optimizer_enabled && resgroup_enabled &&
			 detect_answer_file(expectfile, sizeof(expectfile), testname, "_optimizer_resgroup")) ;
	else if (optimizer_enabled &&
			 detect_answer_file(expectfile, sizeof(expectfile), testname, "_optimizer")) ;
	else if (resgroup_enabled &&
			 detect_answer_file(expectfile, sizeof(expectfile), testname, "_resgroup")) ;
	else if (detect_answer_file(expectfile, sizeof(expectfile), testname, "")) ;
	else
	{
		fprintf(stderr, _("missing answer file for test \"%s\"\n"), testname);
		exit_nicely(2);
	}

	add_stringlist_item(resultfiles, outfile);
	add_stringlist_item(expectfiles, expectfile);

	snprintf(psql_cmd, sizeof(psql_cmd),
			 "%s " SYSTEMQUOTE "\"%s%spsql\" -X -a -q -d \"%s\" < \"%s\" > \"%s\" 2>&1" SYSTEMQUOTE,
			 use_utility_mode ? "env PGOPTIONS='-c gp_session_role=utility'" : "",
			 psqldir ? psqldir : "",
			 psqldir ? "/" : "",
			 dblist->str,
			 infile,
			 outfile);

	pid = spawn_process(psql_cmd);

	if (pid == INVALID_PID)
	{
		fprintf(stderr, _("could not start process for test %s\n"),
				testname);
		exit_nicely(2);
	}

	return pid;
}

static void
psql_init(void)
{
	/* set default regression database name */
	add_stringlist_item(&dblist, "regression");
}

int
main(int argc, char *argv[])
{
	return regression_main(argc, argv, psql_init, psql_start_test);
}
