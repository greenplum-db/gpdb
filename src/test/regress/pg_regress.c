/*-------------------------------------------------------------------------
 *
 * pg_regress --- regression test driver
 *
 * This is a C implementation of the previous shell script for running
 * the regression tests, and should be mostly compatible with it.
 * Initial author of C translation: Magnus Hagander
 *
 * This code is released under the terms of the PostgreSQL License.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/test/regress/pg_regress.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#ifdef __linux__
#include <mntent.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "pg_regress.h"

#include "common/logging.h"
#include "common/restricted_token.h"
#include "common/string.h"
#include "common/username.h"
#include "getopt_long.h"
#include "lib/stringinfo.h"
#include "libpq/pqcomm.h"		/* needed for UNIXSOCK_PATH() */
#include "pg_config_paths.h"
#include "portability/instr_time.h"

/* for resultmap we need a list of pairs of strings */
typedef struct _resultmap
{
	char	   *test;
	char	   *type;
	char	   *resultfile;
	struct _resultmap *next;
} _resultmap;

/*
 * Values obtained from Makefile.
 */
char	   *host_platform = HOST_TUPLE;

#ifndef WIN32					/* not used in WIN32 case */
static char *shellprog = SHELLPROG;
#endif

static char gpdiffprog[MAXPGPATH];
static char gpstringsubsprog[MAXPGPATH];

/*
 * On Windows we use -w in diff switches to avoid problems with inconsistent
 * newline representation.  The actual result files will generally have
 * Windows-style newlines, but the comparison files might or might not.
 */
#ifndef WIN32
/* GPDB:  Add stuff to ignore all the extra NOTICE messages we give */
const char *basic_diff_opts = "-I HINT: -I CONTEXT: -I GP_IGNORE:";
const char *pretty_diff_opts = "-I HINT: -I CONTEXT: -I GP_IGNORE: -U3";
#else
const char *basic_diff_opts = "-w";
const char *pretty_diff_opts = "-w -U3";
#endif

_stringlist *setup_tests = NULL;
/* options settable from command line */
_stringlist *dblist = NULL;
bool		debug = false;
char	   *inputdir = ".";
char	   *outputdir = ".";
char	   *tablespacedir = ".";
char	   *exclude_tests_file = "";
char	   *prehook = "";
char	   *bindir = PGBINDIR;
char	   *launcher = NULL;
bool        print_failure_diffs_is_enabled = false;
bool 		optimizer_enabled = false;
bool 		resgroup_enabled = false;
static _stringlist *loadlanguage = NULL;
static _stringlist *loadextension = NULL;
static int	max_connections = 0;
static int	max_concurrent_tests = 0;
static char *encoding = NULL;
static _stringlist *init_file_list = NULL;
static _stringlist *schedulelist = NULL;
static _stringlist *exclude_tests = NULL;
static _stringlist *extra_tests = NULL;
static char *temp_instance = NULL;
static _stringlist *temp_configs = NULL;
static bool nolocale = false;
static bool use_existing = false;
static char *hostname = NULL;
static int	port = -1;
static bool port_specified_by_user = false;
static char *dlpath = PKGLIBDIR;
static char *user = NULL;
static char *sslmode = NULL;
static _stringlist *extraroles = NULL;
static char *config_auth_datadir = NULL;
static bool  ignore_plans = false;

/* internal variables */
static const char *progname;
static char *logfilename;
static FILE *logfile;
static char *difffilename;
static const char *sockdir;
#ifdef HAVE_UNIX_SOCKETS
static const char *temp_sockdir;
static char sockself[MAXPGPATH];
static char socklock[MAXPGPATH];
#endif

static _resultmap *resultmap = NULL;

static PID_TYPE postmaster_pid = INVALID_PID;
static bool postmaster_running = false;

static int	success_count = 0;
static int	fail_count = 0;
static int	fail_ignore_count = 0;

static bool halt_work = false;

static bool directory_exists(const char *dir);
static void make_directory(const char *dir);

static void header(const char *fmt,...) pg_attribute_printf(1, 2);
static void status(const char *fmt,...) pg_attribute_printf(1, 2);
static void psql_command(const char *database, const char *query,...) pg_attribute_printf(2, 3);

static bool detectCgroupMountPoint(char *cgdir, int len);
static bool should_exclude_test(char *test);
static int run_diff(const char *cmd, const char *filename);

static char *content_zero_hostname = NULL;
static char *get_host_name(int16 contentid, char role);
static bool cluster_healthy(void);

/*
 * allow core files if possible.
 */
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_CORE)
static void
unlimit_core_size(void)
{
	struct rlimit lim;

	getrlimit(RLIMIT_CORE, &lim);
	if (lim.rlim_max == 0)
	{
		fprintf(stderr,
				_("%s: could not set core size: disallowed by hard limit\n"),
				progname);
		return;
	}
	else if (lim.rlim_max == RLIM_INFINITY || lim.rlim_cur < lim.rlim_max)
	{
		lim.rlim_cur = lim.rlim_max;
		setrlimit(RLIMIT_CORE, &lim);
	}
}
#endif


/*
 * Add an item at the end of a stringlist.
 */
void
add_stringlist_item(_stringlist **listhead, const char *str)
{
	_stringlist *newentry = pg_malloc(sizeof(_stringlist));
	_stringlist *oldentry;

	newentry->str = pg_strdup(str);
	newentry->next = NULL;
	if (*listhead == NULL)
		*listhead = newentry;
	else
	{
		for (oldentry = *listhead; oldentry->next; oldentry = oldentry->next)
			 /* skip */ ;
		oldentry->next = newentry;
	}
}

/*
 * Free a stringlist.
 */
static void
free_stringlist(_stringlist **listhead)
{
	if (listhead == NULL || *listhead == NULL)
		return;
	if ((*listhead)->next != NULL)
		free_stringlist(&((*listhead)->next));
	free((*listhead)->str);
	free(*listhead);
	*listhead = NULL;
}

/*
 * Split a delimited string into a stringlist
 */
static void
split_to_stringlist(const char *s, const char *delim, _stringlist **listhead)
{
	char	   *sc = pg_strdup(s);
	char	   *token = strtok(sc, delim);

	while (token)
	{
		add_stringlist_item(listhead, token);
		token = strtok(NULL, delim);
	}
	free(sc);
}

static void
load_exclude_tests_file(_stringlist **listhead, const char *exclude_tests_file)
{
	char buf[1024];
	FILE *excludefile;
	int i;
	excludefile = fopen(exclude_tests_file, "r");
	if (!excludefile)
	{
		fprintf(stderr, _("\ncould not open file %s: %s\n"),
				exclude_tests_file, strerror(errno));
		_exit(2);
	}
	while (fgets(buf, sizeof(buf), excludefile))
	{
		i = strlen(buf);
		if (buf[i-1] == '\n')
			buf[i-1] = '\0';
		add_stringlist_item(&exclude_tests, buf);
	}
	if (ferror(excludefile))
	{
		fprintf(stderr, _("\ncould not read file %s: %s\n"),
				exclude_tests_file, strerror(errno));
		_exit(2);
	}
	if (fclose(excludefile))
	{
		fprintf(stderr, _("\ncould not close file %s: %s\n"),
				exclude_tests_file, strerror(errno));
		_exit(2);
	}
}

/*
 * Print a progress banner on stdout.
 */
static void
header(const char *fmt,...)
{
	char		tmp[64];
	va_list		ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	fprintf(stdout, "============== %-38s ==============\n", tmp);
	fflush(stdout);
}

/*
 * Print "doing something ..." --- supplied text should not end with newline
 */
static void
status(const char *fmt,...)
{
	va_list		ap;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	fflush(stdout);
	va_end(ap);

	if (logfile)
	{
		va_start(ap, fmt);
		vfprintf(logfile, fmt, ap);
		va_end(ap);
	}
}

/*
 * Done "doing something ..."
 */
static void
status_end(void)
{
	fprintf(stdout, "\n");
	fflush(stdout);
	if (logfile)
		fprintf(logfile, "\n");
}

/*
 * shut down temp postmaster
 */
static void
stop_postmaster(void)
{
	if (postmaster_running)
	{
		/* We use pg_ctl to issue the kill and wait for stop */
		char		buf[MAXPGPATH * 2];
		int			r;

		/* On Windows, system() seems not to force fflush, so... */
		fflush(stdout);
		fflush(stderr);

		snprintf(buf, sizeof(buf),
				 "\"%s%spg_ctl\" stop -D \"%s/data\" -s",
				 bindir ? bindir : "",
				 bindir ? "/" : "",
				 temp_instance);
		r = system(buf);
		if (r != 0)
		{
			fprintf(stderr, _("\n%s: could not stop postmaster: exit code was %d\n"),
					progname, r);
			_exit(2);			/* not exit(), that could be recursive */
		}

		postmaster_running = false;
	}
}

#ifdef HAVE_UNIX_SOCKETS
/*
 * Remove the socket temporary directory.  pg_regress never waits for a
 * postmaster exit, so it is indeterminate whether the postmaster has yet to
 * unlink the socket and lock file.  Unlink them here so we can proceed to
 * remove the directory.  Ignore errors; leaking a temporary directory is
 * unimportant.  This can run from a signal handler.  The code is not
 * acceptable in a Windows signal handler (see initdb.c:trapsig()), but
 * Windows is not a HAVE_UNIX_SOCKETS platform.
 */
static void
remove_temp(void)
{
	Assert(temp_sockdir);
	unlink(sockself);
	unlink(socklock);
	rmdir(temp_sockdir);
}

/*
 * Signal handler that calls remove_temp() and reraises the signal.
 */
static void
signal_remove_temp(int signum)
{
	remove_temp();

	pqsignal(signum, SIG_DFL);
	raise(signum);
}

/*
 * Create a temporary directory suitable for the server's Unix-domain socket.
 * The directory will have mode 0700 or stricter, so no other OS user can open
 * our socket to exploit our use of trust authentication.  Most systems
 * constrain the length of socket paths well below _POSIX_PATH_MAX, so we
 * place the directory under /tmp rather than relative to the possibly-deep
 * current working directory.
 *
 * Compared to using the compiled-in DEFAULT_PGSOCKET_DIR, this also permits
 * testing to work in builds that relocate it to a directory not writable to
 * the build/test user.
 */
static const char *
make_temp_sockdir(void)
{
	char	   *template = pg_strdup("/tmp/pg_regress-XXXXXX");

	temp_sockdir = mkdtemp(template);
	if (temp_sockdir == NULL)
	{
		fprintf(stderr, _("%s: could not create directory \"%s\": %s\n"),
				progname, template, strerror(errno));
		exit(2);
	}

	/* Stage file names for remove_temp().  Unsafe in a signal handler. */
	UNIXSOCK_PATH(sockself, port, temp_sockdir);
	snprintf(socklock, sizeof(socklock), "%s.lock", sockself);

	/* Remove the directory during clean exit. */
	atexit(remove_temp);

	/*
	 * Remove the directory before dying to the usual signals.  Omit SIGQUIT,
	 * preserving it as a quick, untidy exit.
	 */
	pqsignal(SIGHUP, signal_remove_temp);
	pqsignal(SIGINT, signal_remove_temp);
	pqsignal(SIGPIPE, signal_remove_temp);
	pqsignal(SIGTERM, signal_remove_temp);

	return temp_sockdir;
}
#endif							/* HAVE_UNIX_SOCKETS */

/*
 * Check whether string matches pattern
 *
 * In the original shell script, this function was implemented using expr(1),
 * which provides basic regular expressions restricted to match starting at
 * the string start (in conventional regex terms, there's an implicit "^"
 * at the start of the pattern --- but no implicit "$" at the end).
 *
 * For now, we only support "." and ".*" as non-literal metacharacters,
 * because that's all that anyone has found use for in resultmap.  This
 * code could be extended if more functionality is needed.
 */
static bool
string_matches_pattern(const char *str, const char *pattern)
{
	while (*str && *pattern)
	{
		if (*pattern == '.' && pattern[1] == '*')
		{
			pattern += 2;
			/* Trailing .* matches everything. */
			if (*pattern == '\0')
				return true;

			/*
			 * Otherwise, scan for a text position at which we can match the
			 * rest of the pattern.
			 */
			while (*str)
			{
				/*
				 * Optimization to prevent most recursion: don't recurse
				 * unless first pattern char might match this text char.
				 */
				if (*str == *pattern || *pattern == '.')
				{
					if (string_matches_pattern(str, pattern))
						return true;
				}

				str++;
			}

			/*
			 * End of text with no match.
			 */
			return false;
		}
		else if (*pattern != '.' && *str != *pattern)
		{
			/*
			 * Not the single-character wildcard and no explicit match? Then
			 * time to quit...
			 */
			return false;
		}

		str++;
		pattern++;
	}

	if (*pattern == '\0')
		return true;			/* end of pattern, so declare match */

	/* End of input string.  Do we have matching pattern remaining? */
	while (*pattern == '.' && pattern[1] == '*')
		pattern += 2;
	if (*pattern == '\0')
		return true;			/* end of pattern, so declare match */

	return false;
}

/*
 * Replace all occurrences of "replace" in "string" with "replacement".
 * The StringInfo will be suitably enlarged if necessary.
 *
 * Note: this is optimized on the assumption that most calls will find
 * no more than one occurrence of "replace", and quite likely none.
 */
void
replace_string(StringInfo string, const char *replace, const char *replacement)
{
	int			pos = 0;
	char	   *ptr;

	while ((ptr = strstr(string->data + pos, replace)) != NULL)
	{
		/* Must copy the remainder of the string out of the StringInfo */
		char	   *suffix = pg_strdup(ptr + strlen(replace));

		/* Truncate StringInfo at start of found string ... */
		string->len = ptr - string->data;
		/* ... and append the replacement (this restores the trailing '\0') */
		appendStringInfoString(string, replacement);
		/* Next search should start after the replacement */
		pos = string->len;
		/* Put back the remainder of the string */
		appendStringInfoString(string, suffix);
		free(suffix);
	}
}

typedef struct replacements
{
	char *abs_srcdir;
	char *abs_builddir;
	char *testtablespace;
	char *dlpath;
	char *dlsuffix;
	char *bindir;
	char *amname;
	char *cgroup_mnt_point;
	char *content_zero_hostname;
	const char *username;
} replacements;

/* Internal helper function to detect cgroup mount point at runtime.*/
static bool
detectCgroupMountPoint(char *cgdir, int len)
{
#ifdef __linux__
	struct mntent *me;
	FILE *fp;
	bool ret = false;

	fp = setmntent("/proc/self/mounts", "r");
	if (fp == NULL)
		return ret;

	while ((me = getmntent(fp)))
	{
		char *p;

		if (strcmp(me->mnt_type, "cgroup"))
			continue;

		strncpy(cgdir, me->mnt_dir, len);

		p = strrchr(cgdir, '/');
		if (p != NULL)
		{
			*p = 0;
			ret = true;
		}
		break;
	}

	endmntent(fp);
	return ret;
#else
	return false;
#endif
}

static void
convert_line(StringInfo line, replacements *repls)
{
	replace_string(line, "@cgroup_mnt_point@", repls->cgroup_mnt_point);
	replace_string(line, "@abs_srcdir@", repls->abs_srcdir);
	replace_string(line, "@abs_builddir@", repls->abs_builddir);
	replace_string(line, "@testtablespace@", repls->testtablespace);
	replace_string(line, "@libdir@", repls->dlpath);
	replace_string(line, "@DLSUFFIX@", repls->dlsuffix);
	replace_string(line, "@bindir@", repls->bindir);
	replace_string(line, "@hostname@", repls->content_zero_hostname);
	replace_string(line, "@curusername@", (char *) repls->username);
	if (repls->amname)
	{
		replace_string(line, "@amname@", repls->amname);
		if (strcmp(repls->amname, "ao_row") == 0)
			replace_string(line, "@aoseg@", "aoseg");
		else
			replace_string(line, "@aoseg@", "aocsseg");
	}
}

/*
 * Generate two files for each UAO test case, one for row and the
 * other for column amname.
 */
static int
generate_uao_sourcefiles(const char *src_dir, const char *dest_dir, const char *suffix, replacements *repls)
{
	struct stat st;
	int			ret;
	char	  **name;
	char	  **names;
	int			count = 0;

	/*
	 * Return silently if src_dir or dest_dir is not a directory, in
	 * the same spirit as in convert_sourcefiles_in().
	 */
	ret = stat(src_dir, &st);
	if (ret != 0 || !S_ISDIR(st.st_mode))
		return 0;

	ret = stat(dest_dir, &st);
	if (ret != 0 || !S_ISDIR(st.st_mode))
		return 0;

	names = pgfnames(src_dir);
	if (!names)
		/* Error logged in pgfnames */
		exit(2);

	/* finally loop on each file and generate the files */
	for (name = names; *name; name++)
	{
		char		srcfile[MAXPGPATH];
		char		destfile_row[MAXPGPATH];
		char		destfile_col[MAXPGPATH];
		char		prefix[MAXPGPATH];
		FILE	   *infile,
				   *outfile_row,
				   *outfile_col;
		StringInfoData line;
		StringInfoData line_row;
		bool		has_tokens = false;

		/* reject filenames not finishing in ".source" */
		if (strlen(*name) < 8)
			continue;
		if (strcmp(*name + strlen(*name) - 7, ".source") != 0)
			continue;

		count++;

		/*
		 * Build the full actual paths to open.  Optimizer specific
		 * answer filenames must end with "optimizer".
		 */
		snprintf(srcfile, MAXPGPATH, "%s/%s", src_dir, *name);
		if (strlen(*name) > 17 &&
			strcmp(*name + strlen(*name) - 17, "_optimizer.source") == 0)
		{
			snprintf(prefix, strlen(*name) - 16, "%s", *name);
			snprintf(destfile_row, MAXPGPATH, "%s/%s_row_optimizer.%s",
					 dest_dir, prefix, suffix);
			snprintf(destfile_col, MAXPGPATH, "%s/%s_column_optimizer.%s",
					 dest_dir, prefix, suffix);
		}
		else
		{
			snprintf(prefix, strlen(*name) - 6, "%s", *name);
			snprintf(destfile_row, MAXPGPATH, "%s/%s_row.%s",
					 dest_dir, prefix, suffix);
			snprintf(destfile_col, MAXPGPATH, "%s/%s_column.%s",
					 dest_dir, prefix, suffix);
		}

		infile = fopen(srcfile, "r");
		if (!infile)
		{
			fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
					progname, srcfile, strerror(errno));
			exit(2);
		}
		outfile_row = fopen(destfile_row, "w");
		if (!outfile_row)
		{
			fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
					progname, destfile_row, strerror(errno));
			exit(2);
		}
		outfile_col = fopen(destfile_col, "w");
		if (!outfile_col)
		{
			fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
					progname, destfile_col, strerror(errno));
			exit(2);
		}

		initStringInfo(&line);
		initStringInfo(&line_row);

		while (pg_get_line_buf(infile, &line))
		{
			appendStringInfoString(&line_row, line.data);
			repls->amname = "ao_row";
			convert_line(&line_row, repls);
			repls->amname = "ao_column";
			convert_line(&line, repls);
			fputs(line.data, outfile_col);
			fputs(line_row.data, outfile_row);
			/*
			 * Remember if there are any more tokens that we didn't recognize.
			 * They need to be handled by the gpstringsubs.pl script
			 */
			if (!has_tokens && strstr(line.data, "@gp") != NULL)
				has_tokens = true;

			resetStringInfo(&line_row);
		}

		pfree(line.data);
		pfree(line_row.data);
		fclose(infile);
		fclose(outfile_row);
		fclose(outfile_col);

		if (has_tokens)
		{
			char		cmd[MAXPGPATH * 3];
			snprintf(cmd, sizeof(cmd),
					 "%s %s", gpstringsubsprog, destfile_row);
			if (run_diff(cmd, destfile_row) != 0)
			{
				fprintf(stderr, _("%s: could not convert %s\n"),
						progname, destfile_row);
			}
			snprintf(cmd, sizeof(cmd),
					 "%s %s", gpstringsubsprog, destfile_col);
			if (run_diff(cmd, destfile_col) != 0)
			{
				fprintf(stderr, _("%s: could not convert %s\n"),
						progname, destfile_col);
			}
		}
	}

	pgfnames_cleanup(names);
	return count;
}

/*
 * Convert *.source found in the "source" directory, replacing certain tokens
 * in the file contents with their intended values, and put the resulting files
 * in the "dest" directory, replacing the ".source" prefix in their names with
 * the given suffix.
 */
static int
convert_sourcefiles_in(const char *source_subdir, const char *dest_dir, const char *dest_subdir, const char *suffix)
{
	char		testtablespace[MAXPGPATH];
	char		indir[MAXPGPATH];
	char		cgroup_mnt_point[MAXPGPATH];
	replacements repls;
	struct stat st;
	int			ret;
	char	  **name;
	char	  **names;
	int			count = 0;
	char *errstr;

	snprintf(indir, MAXPGPATH, "%s/%s", inputdir, source_subdir);

	/* Check that indir actually exists and is a directory */
	ret = stat(indir, &st);
	if (ret != 0 || !S_ISDIR(st.st_mode))
	{
		/*
		 * No warning, to avoid noise in tests that do not have these
		 * directories; for example, ecpg, contrib and src/pl.
		 */
		return count;
	}

	names = pgfnames(indir);
	if (!names)
		/* Error logged in pgfnames */
		exit(2);

	/* also create the output directory if not present */
	if (!directory_exists(dest_subdir))
		make_directory(dest_subdir);

	snprintf(testtablespace, MAXPGPATH, "%s/testtablespace", tablespacedir);

#ifdef WIN32

	/*
	 * On Windows only, clean out the test tablespace dir, or create it if it
	 * doesn't exist.  On other platforms we expect the Makefile to take care
	 * of that.  (We don't migrate that functionality in here because it'd be
	 * harder to cope with platform-specific issues such as SELinux.)
	 *
	 * XXX it would be better if pg_regress.c had nothing at all to do with
	 * testtablespace, and this were handled by a .BAT file or similar on
	 * Windows.  See pgsql-hackers discussion of 2008-01-18.
	 */
	if (directory_exists(testtablespace))
		if (!rmtree(testtablespace, true))
		{
			fprintf(stderr, _("\n%s: could not remove test tablespace \"%s\"\n"),
					progname, testtablespace);
			exit(2);
		}
	make_directory(testtablespace);
#endif

	memset(cgroup_mnt_point, 0, sizeof(cgroup_mnt_point));
	if (!detectCgroupMountPoint(cgroup_mnt_point,
								sizeof(cgroup_mnt_point) - 1))
		strcpy(cgroup_mnt_point, "/sys/fs/cgroup");

	memset(&repls, 0, sizeof(repls));
	repls.abs_srcdir = inputdir;
	repls.abs_builddir = outputdir;
	repls.testtablespace = testtablespace;
	repls.dlpath = dlpath;
	repls.dlsuffix = DLSUFFIX;
	repls.bindir = bindir;
	repls.cgroup_mnt_point = cgroup_mnt_point;
	repls.content_zero_hostname = content_zero_hostname;
	repls.username = get_user_name(&errstr);

	if (repls.username == NULL)
	{
		fprintf(stderr, "%s: %s\n", progname, errstr);
		exit(2);
	}

	/* finally loop on each file and do the replacement */
	for (name = names; *name; name++)
	{
		char		srcfile[MAXPGPATH];
		char		destfile[MAXPGPATH];
		char		prefix[MAXPGPATH];
		FILE	   *infile,
				   *outfile;
		StringInfoData line;
		bool		has_tokens = false;
		struct stat fst;

		snprintf(srcfile, MAXPGPATH, "%s/%s",  indir, *name);
		if (stat(srcfile, &fst) < 0)
		{
			fprintf(stderr, _("\n%s: stat failed for \"%s\"\n"),
					progname, srcfile);
			exit(2);
		}

		/* recurse if it's a directory */
		if (S_ISDIR(fst.st_mode))
		{
			char generate_uao_file[MAXPGPATH];
			snprintf(generate_uao_file, MAXPGPATH, "%s/%s",  srcfile, "GENERATE_ROW_AND_COLUMN_FILES");

			snprintf(srcfile, MAXPGPATH, "%s/%s", source_subdir, *name);
			snprintf(destfile, MAXPGPATH, "%s/%s", dest_subdir, *name);

			if (access(generate_uao_file, F_OK) != -1)
				count += generate_uao_sourcefiles(srcfile, destfile, suffix, &repls);
			else
				count += convert_sourcefiles_in(srcfile, dest_dir, destfile, suffix);

			continue;
		}

		/* reject filenames not finishing in ".source" */
		if (strlen(*name) < 8)
			continue;
		if (strcmp(*name + strlen(*name) - 7, ".source") != 0)
			continue;

		count++;

		/* build the full actual paths to open */
		snprintf(prefix, strlen(*name) - 6, "%s", *name);
		snprintf(srcfile, MAXPGPATH, "%s/%s", indir, *name);
		snprintf(destfile, MAXPGPATH, "%s/%s/%s.%s", dest_dir, dest_subdir,
				 prefix, suffix);

		infile = fopen(srcfile, "r");
		if (!infile)
		{
			fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
					progname, srcfile, strerror(errno));
			exit(2);
		}
		outfile = fopen(destfile, "w");
		if (!outfile)
		{
			fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
					progname, destfile, strerror(errno));
			exit(2);
		}

		initStringInfo(&line);

		while (pg_get_line_buf(infile, &line))
		{
			convert_line(&line, &repls);
			fputs(line.data, outfile);

			/*
			 * Remember if there are any more tokens that we didn't recognize.
			 * They need to be handled by the gpstringsubs.pl script
			 */
			if (!has_tokens && strstr(line.data, "@gp") != NULL)
				has_tokens = true;
		}

		pfree(line.data);
		fclose(infile);
		fclose(outfile);

		if (has_tokens)
		{
			char		cmd[MAXPGPATH * 3];
			snprintf(cmd, sizeof(cmd),
					 "%s %s", gpstringsubsprog, destfile);
			if (run_diff(cmd, destfile) != 0)
			{
				fprintf(stderr, _("%s: could not convert %s\n"),
						progname, destfile);
			}
		}

	}

	/*
	 * If we didn't process any files, complain because it probably means
	 * somebody neglected to pass the needed --inputdir argument.
	 */
	if (count <= 0)
	{
		fprintf(stderr, _("%s: no *.source files found in \"%s\"\n"),
				progname, indir);
		exit(2);
	}

	pgfnames_cleanup(names);

	return count;
}

/* Create the .sql, .out and .yml files from the .source files, if any */
static void
convert_sourcefiles(void)
{
	content_zero_hostname = get_host_name(0, 'p');

	convert_sourcefiles_in("input", outputdir, "sql", "sql");
	convert_sourcefiles_in("output", outputdir, "expected", "out");

	convert_sourcefiles_in("yml_in", inputdir, "yml", "yml");
}

/*
 * Scan resultmap file to find which platform-specific expected files to use.
 *
 * The format of each line of the file is
 *		   testname/hostplatformpattern=substitutefile
 * where the hostplatformpattern is evaluated per the rules of expr(1),
 * namely, it is a standard regular expression with an implicit ^ at the start.
 * (We currently support only a very limited subset of regular expressions,
 * see string_matches_pattern() above.)  What hostplatformpattern will be
 * matched against is the config.guess output.  (In the shell-script version,
 * we also provided an indication of whether gcc or another compiler was in
 * use, but that facility isn't used anymore.)
 */
static void
load_resultmap(void)
{
	char		buf[MAXPGPATH];
	FILE	   *f;

	/* scan the file ... */
	snprintf(buf, sizeof(buf), "%s/resultmap", inputdir);
	f = fopen(buf, "r");
	if (!f)
	{
		/* OK if it doesn't exist, else complain */
		if (errno == ENOENT)
			return;
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, buf, strerror(errno));
		exit(2);
	}

	while (fgets(buf, sizeof(buf), f))
	{
		char	   *platform;
		char	   *file_type;
		char	   *expected;
		int			i;

		/* strip trailing whitespace, especially the newline */
		i = strlen(buf);
		while (i > 0 && isspace((unsigned char) buf[i - 1]))
			buf[--i] = '\0';

		/* parse out the line fields */
		file_type = strchr(buf, ':');
		if (!file_type)
		{
			fprintf(stderr, _("incorrectly formatted resultmap entry: %s\n"),
					buf);
			exit(2);
		}
		*file_type++ = '\0';

		platform = strchr(file_type, ':');
		if (!platform)
		{
			fprintf(stderr, _("incorrectly formatted resultmap entry: %s\n"),
					buf);
			exit(2);
		}
		*platform++ = '\0';
		expected = strchr(platform, '=');
		if (!expected)
		{
			fprintf(stderr, _("incorrectly formatted resultmap entry: %s\n"),
					buf);
			exit(2);
		}
		*expected++ = '\0';

		/*
		 * if it's for current platform, save it in resultmap list. Note: by
		 * adding at the front of the list, we ensure that in ambiguous cases,
		 * the last match in the resultmap file is used. This mimics the
		 * behavior of the old shell script.
		 */
		if (string_matches_pattern(host_platform, platform))
		{
			_resultmap *entry = pg_malloc(sizeof(_resultmap));

			entry->test = pg_strdup(buf);
			entry->type = pg_strdup(file_type);
			entry->resultfile = pg_strdup(expected);
			entry->next = resultmap;
			resultmap = entry;
		}
	}
	fclose(f);
}

/*
 * Check in resultmap if we should be looking at a different file
 */
static
const char *
get_expectfile(const char *testname, const char *file, const char *default_expectfile)
{
	char		expectpath[MAXPGPATH];
	char	   *file_type;
	char	   *file_name;
	char		base_file[MAXPGPATH];
	_resultmap *rm;
	char		buf[MAXPGPATH];

	/*
	 * Determine the file type from the file name. This is just what is
	 * following the last dot in the file name.
	 */
	if (!file || !(file_type = strrchr(file, '.')))
		return NULL;

	file_type++;

	/*
	 * Also determine the base file name from the result full path.
	 */
	if (!(file_name = strrchr(file, '/')))
		return NULL;

	file_name ++;

	if (file_type < file_name)
		return NULL;
	strlcpy(base_file, file_name, (file_type) - file_name);

	/*
	 * Find the directory the default expected file is in. That is, everything
	 * up to the last slash.
	 */
	{
		char	   *p = strrchr(default_expectfile, '/');

		if (!p)
			return NULL;

		strlcpy(expectpath, default_expectfile, p - default_expectfile + 1);
	}

	for (rm = resultmap; rm != NULL; rm = rm->next)
	{
		if (strcmp(testname, rm->test) == 0 && strcmp(file_type, rm->type) == 0)
		{
			snprintf(buf, sizeof(buf), "%s/%s", expectpath, rm->resultfile);
			return strdup(buf);
		}
	}

	/* Use ORCA or resgroup expected outputs, if available */
	if  (optimizer_enabled && resgroup_enabled)
	{
		snprintf(buf, sizeof(buf), "%s/%s_optimizer_resgroup.%s", expectpath, base_file, file_type);
		if (file_exists(buf))
			return strdup(buf);
	}
	if  (optimizer_enabled)
	{
		snprintf(buf, sizeof(buf), "%s/%s_optimizer.%s", expectpath, base_file, file_type);
		if (file_exists(buf))
			return strdup(buf);
	}
	if  (resgroup_enabled)
	{
		snprintf(buf, sizeof(buf), "%s/%s_resgroup.%s", expectpath, base_file, file_type);
		if (file_exists(buf))
			return strdup(buf);
	}

	return NULL;
}

/*
 * Handy subroutine for setting an environment variable "var" to "val"
 */
static void
doputenv(const char *var, const char *val)
{
	char	   *s;

	s = psprintf("%s=%s", var, val);
	putenv(s);
}

/*
 * Prepare environment variables for running regression tests
 */
static void
initialize_environment(void)
{
	/*
	 * Set default application_name.  (The test_function may choose to
	 * override this, but if it doesn't, we have something useful in place.)
	 */
	putenv("PGAPPNAME=pg_regress");

	if (nolocale)
	{
		/*
		 * Clear out any non-C locale settings
		 */
		unsetenv("LC_COLLATE");
		unsetenv("LC_CTYPE");
		unsetenv("LC_MONETARY");
		unsetenv("LC_NUMERIC");
		unsetenv("LC_TIME");
		unsetenv("LANG");

		/*
		 * Most platforms have adopted the POSIX locale as their
		 * implementation-defined default locale.  Exceptions include native
		 * Windows, macOS with --enable-nls, and Cygwin with --enable-nls.
		 * (Use of --enable-nls matters because libintl replaces setlocale().)
		 * Also, PostgreSQL does not support macOS with locale environment
		 * variables unset; see PostmasterMain().
		 */
#if defined(WIN32) || defined(__CYGWIN__) || defined(__darwin__)
		putenv("LANG=C");
#endif
	}

	/*
	 * Set translation-related settings to English; otherwise psql will
	 * produce translated messages and produce diffs.  (XXX If we ever support
	 * translation of pg_regress, this needs to be moved elsewhere, where psql
	 * is actually called.)
	 */
	unsetenv("LANGUAGE");
	unsetenv("LC_ALL");
	putenv("LC_MESSAGES=C");

	/*
	 * Set encoding as requested
	 */
	if (encoding)
		doputenv("PGCLIENTENCODING", encoding);
	else
		unsetenv("PGCLIENTENCODING");

	/*
	 * Set timezone and datestyle for datetime-related tests
	 */
	putenv("PGTZ=PST8PDT");
	putenv("PGDATESTYLE=Postgres, MDY");

	/*
	 * Likewise set intervalstyle to ensure consistent results.  This is a bit
	 * more painful because we must use PGOPTIONS, and we want to preserve the
	 * user's ability to set other variables through that.
	 */
	{
		const char *my_pgoptions = "-c intervalstyle=postgres_verbose";
		const char *old_pgoptions = getenv("PGOPTIONS");
		char	   *new_pgoptions;

		if (!old_pgoptions)
			old_pgoptions = "";
		new_pgoptions = psprintf("PGOPTIONS=%s %s",
								 old_pgoptions, my_pgoptions);
		putenv(new_pgoptions);
	}

	if (temp_instance)
	{
		/*
		 * Clear out any environment vars that might cause psql to connect to
		 * the wrong postmaster, or otherwise behave in nondefault ways. (Note
		 * we also use psql's -X switch consistently, so that ~/.psqlrc files
		 * won't mess things up.)  Also, set PGPORT to the temp port, and set
		 * PGHOST depending on whether we are using TCP or Unix sockets.
		 *
		 * This list should be kept in sync with TestLib.pm.
		 */
		/* PGCLIENTENCODING, see above */
		unsetenv("PGCONNECT_TIMEOUT");
		unsetenv("PGDATA");
		unsetenv("PGDATABASE");
		unsetenv("PGGSSENCMODE");
		unsetenv("PGGSSLIB");
		/* PGHOSTADDR, see below */
		unsetenv("PGKRBSRVNAME");
		unsetenv("PGPASSFILE");
		unsetenv("PGPASSWORD");
		unsetenv("PGREQUIREPEER");
		unsetenv("PGREQUIRESSL");
		unsetenv("PGSERVICE");
		unsetenv("PGSERVICEFILE");
		unsetenv("PGSSLCERT");
		unsetenv("PGSSLCRL");
		unsetenv("PGSSLKEY");
		unsetenv("PGSSLMODE");
		unsetenv("PGSSLROOTCERT");
		unsetenv("PGTARGETSESSIONATTRS");
		unsetenv("PGUSER");
		/* PGPORT, see below */
		/* PGHOST, see below */

#ifdef HAVE_UNIX_SOCKETS
		if (hostname != NULL)
			doputenv("PGHOST", hostname);
		else
		{
			sockdir = getenv("PG_REGRESS_SOCK_DIR");
			if (!sockdir)
				sockdir = make_temp_sockdir();
			doputenv("PGHOST", sockdir);
		}
#else
		Assert(hostname != NULL);
		doputenv("PGHOST", hostname);
#endif
		unsetenv("PGHOSTADDR");
		if (port != -1)
		{
			char		s[16];

			sprintf(s, "%d", port);
			doputenv("PGPORT", s);
		}
	}
	else
	{
		const char *pghost;
		const char *pgport;

		/*
		 * When testing an existing install, we honor existing environment
		 * variables, except if they're overridden by command line options.
		 */
		if (hostname != NULL)
		{
			doputenv("PGHOST", hostname);
			unsetenv("PGHOSTADDR");
		}
		if (port != -1)
		{
			char		s[16];

			sprintf(s, "%d", port);
			doputenv("PGPORT", s);
		}
		if (user != NULL)
			doputenv("PGUSER", user);
		if (sslmode != NULL)
			doputenv("PGSSLMODE", sslmode);

		/*
		 * Report what we're connecting to
		 */
		pghost = getenv("PGHOST");
		pgport = getenv("PGPORT");
#ifndef HAVE_UNIX_SOCKETS
		if (!pghost)
			pghost = "localhost";
#endif

		if (pghost && pgport)
			printf(_("(using postmaster on %s, port %s)\n"), pghost, pgport);
		if (pghost && !pgport)
			printf(_("(using postmaster on %s, default port)\n"), pghost);
		if (!pghost && pgport)
			printf(_("(using postmaster on Unix socket, port %s)\n"), pgport);
		if (!pghost && !pgport)
			printf(_("(using postmaster on Unix socket, default port)\n"));
	}

	convert_sourcefiles();
	load_resultmap();
}

#ifdef ENABLE_SSPI

/* support for config_sspi_auth() */
static const char *
fmtHba(const char *raw)
{
	static char *ret;
	const char *rp;
	char	   *wp;

	wp = ret = realloc(ret, 3 + strlen(raw) * 2);

	*wp++ = '"';
	for (rp = raw; *rp; rp++)
	{
		if (*rp == '"')
			*wp++ = '"';
		*wp++ = *rp;
	}
	*wp++ = '"';
	*wp++ = '\0';

	return ret;
}

/*
 * Get account and domain/realm names for the current user.  This is based on
 * pg_SSPI_recvauth().  The returned strings use static storage.
 */
static void
current_windows_user(const char **acct, const char **dom)
{
	static char accountname[MAXPGPATH];
	static char domainname[MAXPGPATH];
	HANDLE		token;
	TOKEN_USER *tokenuser;
	DWORD		retlen;
	DWORD		accountnamesize = sizeof(accountname);
	DWORD		domainnamesize = sizeof(domainname);
	SID_NAME_USE accountnameuse;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &token))
	{
		fprintf(stderr,
				_("%s: could not open process token: error code %lu\n"),
				progname, GetLastError());
		exit(2);
	}

	if (!GetTokenInformation(token, TokenUser, NULL, 0, &retlen) && GetLastError() != 122)
	{
		fprintf(stderr,
				_("%s: could not get token information buffer size: error code %lu\n"),
				progname, GetLastError());
		exit(2);
	}
	tokenuser = pg_malloc(retlen);
	if (!GetTokenInformation(token, TokenUser, tokenuser, retlen, &retlen))
	{
		fprintf(stderr,
				_("%s: could not get token information: error code %lu\n"),
				progname, GetLastError());
		exit(2);
	}

	if (!LookupAccountSid(NULL, tokenuser->User.Sid, accountname, &accountnamesize,
						  domainname, &domainnamesize, &accountnameuse))
	{
		fprintf(stderr,
				_("%s: could not look up account SID: error code %lu\n"),
				progname, GetLastError());
		exit(2);
	}

	free(tokenuser);

	*acct = accountname;
	*dom = domainname;
}

/*
 * Rewrite pg_hba.conf and pg_ident.conf to use SSPI authentication.  Permit
 * the current OS user to authenticate as the bootstrap superuser and as any
 * user named in a --create-role option.
 *
 * In --config-auth mode, the --user switch can be used to specify the
 * bootstrap superuser's name, otherwise we assume it is the default.
 */
static void
config_sspi_auth(const char *pgdata, const char *superuser_name)
{
	const char *accountname,
			   *domainname;
	char	   *errstr;
	bool		have_ipv6;
	char		fname[MAXPGPATH];
	int			res;
	FILE	   *hba,
			   *ident;
	_stringlist *sl;

	/* Find out the name of the current OS user */
	current_windows_user(&accountname, &domainname);

	/* Determine the bootstrap superuser's name */
	if (superuser_name == NULL)
	{
		/*
		 * Compute the default superuser name the same way initdb does.
		 *
		 * It's possible that this result always matches "accountname", the
		 * value SSPI authentication discovers.  But the underlying system
		 * functions do not clearly guarantee that.
		 */
		superuser_name = get_user_name(&errstr);
		if (superuser_name == NULL)
		{
			fprintf(stderr, "%s: %s\n", progname, errstr);
			exit(2);
		}
	}

	/*
	 * Like initdb.c:setup_config(), determine whether the platform recognizes
	 * ::1 (IPv6 loopback) as a numeric host address string.
	 */
	{
		struct addrinfo *gai_result;
		struct addrinfo hints;
		WSADATA		wsaData;

		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = 0;
		hints.ai_protocol = 0;
		hints.ai_addrlen = 0;
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

		have_ipv6 = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0 &&
					 getaddrinfo("::1", NULL, &hints, &gai_result) == 0);
	}

	/* Check a Write outcome and report any error. */
#define CW(cond)	\
	do { \
		if (!(cond)) \
		{ \
			fprintf(stderr, _("%s: could not write to file \"%s\": %s\n"), \
					progname, fname, strerror(errno)); \
			exit(2); \
		} \
	} while (0)

	res = snprintf(fname, sizeof(fname), "%s/pg_hba.conf", pgdata);
	if (res < 0 || res >= sizeof(fname))
	{
		/*
		 * Truncating this name is a fatal error, because we must not fail to
		 * overwrite an original trust-authentication pg_hba.conf.
		 */
		fprintf(stderr, _("%s: directory name too long\n"), progname);
		exit(2);
	}
	hba = fopen(fname, "w");
	if (hba == NULL)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
				progname, fname, strerror(errno));
		exit(2);
	}
	CW(fputs("# Configuration written by config_sspi_auth()\n", hba) >= 0);
	CW(fputs("host all all 127.0.0.1/32  sspi include_realm=1 map=regress\n",
			 hba) >= 0);
	if (have_ipv6)
		CW(fputs("host all all ::1/128  sspi include_realm=1 map=regress\n",
				 hba) >= 0);
	CW(fclose(hba) == 0);

	snprintf(fname, sizeof(fname), "%s/pg_ident.conf", pgdata);
	ident = fopen(fname, "w");
	if (ident == NULL)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
				progname, fname, strerror(errno));
		exit(2);
	}
	CW(fputs("# Configuration written by config_sspi_auth()\n", ident) >= 0);

	/*
	 * Double-quote for the benefit of account names containing whitespace or
	 * '#'.  Windows forbids the double-quote character itself, so don't
	 * bother escaping embedded double-quote characters.
	 */
	CW(fprintf(ident, "regress  \"%s@%s\"  %s\n",
			   accountname, domainname, fmtHba(superuser_name)) >= 0);
	for (sl = extraroles; sl; sl = sl->next)
		CW(fprintf(ident, "regress  \"%s@%s\"  %s\n",
				   accountname, domainname, fmtHba(sl->str)) >= 0);
	CW(fclose(ident) == 0);
}

#endif							/* ENABLE_SSPI */

/*
 * Issue a command via psql, connecting to the specified database
 *
 * Since we use system(), this doesn't return until the operation finishes
 */
static void
psql_command(const char *database, const char *query,...)
{
	char		query_formatted[1024];
	char		query_escaped[2048];
	char		psql_cmd[MAXPGPATH + 2048];
	va_list		args;
	char	   *s;
	char	   *d;

	/* Generate the query with insertion of sprintf arguments */
	va_start(args, query);
	vsnprintf(query_formatted, sizeof(query_formatted), query, args);
	va_end(args);

	/* Now escape any shell double-quote metacharacters */
	d = query_escaped;
	for (s = query_formatted; *s; s++)
	{
		if (strchr("\\\"$`", *s))
			*d++ = '\\';
		*d++ = *s;
	}
	*d = '\0';

	/* And now we can build and execute the shell command */
	snprintf(psql_cmd, sizeof(psql_cmd),
			 "\"%s%spsql\" -X -c \"%s\" \"%s\"",
			 bindir ? bindir : "",
			 bindir ? "/" : "",
			 query_escaped,
			 database);

	if (system(psql_cmd) != 0)
	{
		/* psql probably already reported the error */
		fprintf(stderr, _("command failed: %s\n"), psql_cmd);
		exit(2);
	}
}

/*
 * Spawn a process to execute the given shell command; don't wait for it
 *
 * Returns the process ID (or HANDLE) so we can wait for it later
 */
PID_TYPE
spawn_process(const char *cmdline)
{
#ifndef WIN32
	pid_t		pid;

	/*
	 * Must flush I/O buffers before fork.  Ideally we'd use fflush(NULL) here
	 * ... does anyone still care about systems where that doesn't work?
	 */
	fflush(stdout);
	fflush(stderr);
	if (logfile)
		fflush(logfile);

	pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, _("%s: could not fork: %s\n"),
				progname, strerror(errno));
		exit(2);
	}
	if (pid == 0)
	{
		/*
		 * In child
		 *
		 * Instead of using system(), exec the shell directly, and tell it to
		 * "exec" the command too.  This saves two useless processes per
		 * parallel test case.
		 */
		char	   *cmdline2;

		cmdline2 = psprintf("exec %s", cmdline);
		execl(shellprog, shellprog, "-c", cmdline2, (char *) NULL);
		fprintf(stderr, _("%s: could not exec \"%s\": %s\n"),
				progname, shellprog, strerror(errno));
		_exit(1);				/* not exit() here... */
	}
	/* in parent */
	return pid;
#else
	PROCESS_INFORMATION pi;
	char	   *cmdline2;
	HANDLE		restrictedToken;

	memset(&pi, 0, sizeof(pi));
	cmdline2 = psprintf("cmd /c \"%s\"", cmdline);

	if ((restrictedToken =
		 CreateRestrictedProcess(cmdline2, &pi)) == 0)
		exit(2);

	CloseHandle(pi.hThread);
	return pi.hProcess;
#endif
}

/*
 * Count bytes in file
 */
static long
file_size(const char *file)
{
	long		r;
	FILE	   *f = fopen(file, "r");

	if (!f)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, file, strerror(errno));
		return -1;
	}
	fseek(f, 0, SEEK_END);
	r = ftell(f);
	fclose(f);
	return r;
}

/*
 * Count lines in file
 */
static int
file_line_count(const char *file)
{
	int			c;
	int			l = 0;
	FILE	   *f = fopen(file, "r");

	if (!f)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, file, strerror(errno));
		return -1;
	}
	while ((c = fgetc(f)) != EOF)
	{
		if (c == '\n')
			l++;
	}
	fclose(f);
	return l;
}

static FILE *
open_file_for_reading(const char *filename) {
	FILE *file = fopen(filename, "r");

	if (!file)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, filename, strerror(errno));
		exit(1);
	}

	return file;
}

static void
print_contents_of_file(const char* filename) {
	FILE *file;
	char string[1024];

	file = open_file_for_reading(filename);
	while (fgets(string, sizeof(string), file))
		fprintf(stdout, "%s", string);

	fclose(file);
}

bool
file_exists(const char *file)
{
	FILE	   *f = fopen(file, "r");

	if (!f)
		return false;
	fclose(f);
	return true;
}

static bool
directory_exists(const char *dir)
{
	struct stat st;

	if (stat(dir, &st) != 0)
		return false;
	if (S_ISDIR(st.st_mode))
		return true;
	return false;
}

/* Create a directory */
static void
make_directory(const char *dir)
{
	if (mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
	{
		fprintf(stderr, _("%s: could not create directory \"%s\": %s\n"),
				progname, dir, strerror(errno));
		exit(2);
	}
}

/*
 * In: filename.ext, Return: filename_i.ext, where 0 < i <= 9
 */
static char *
get_alternative_expectfile(const char *expectfile, int i)
{
	char	   *last_dot;
	int			ssize = strlen(expectfile) + 2 + 1;
	char	   *tmp;
	char	   *s;

	if (!(tmp = (char *) malloc(ssize)))
		return NULL;

	if (!(s = (char *) malloc(ssize)))
	{
		free(tmp);
		return NULL;
	}

	strcpy(tmp, expectfile);
	last_dot = strrchr(tmp, '.');
	if (!last_dot)
	{
		free(tmp);
		free(s);
		return NULL;
	}
	*last_dot = '\0';
	snprintf(s, ssize, "%s_%d.%s", tmp, i, last_dot + 1);
	free(tmp);
	return s;
}

/*
 * Run a "diff" command and also check that it didn't crash
 */
static int
run_diff(const char *cmd, const char *filename)
{
	int			r;

	r = system(cmd);
	if (!WIFEXITED(r) || WEXITSTATUS(r) > 1)
	{
		fprintf(stderr, _("diff command failed with status %d: %s\n"), r, cmd);
		exit(2);
	}
#ifdef WIN32

	/*
	 * On WIN32, if the 'diff' command cannot be found, system() returns 1,
	 * but produces nothing to stdout, so we check for that here.
	 */
	if (WEXITSTATUS(r) == 1 && file_size(filename) <= 0)
	{
		fprintf(stderr, _("diff command not found: %s\n"), cmd);
		exit(2);
	}
#endif

	return WEXITSTATUS(r);
}

/*
 * Check the actual result file for the given test against expected results
 *
 * Returns true if different (failure), false if correct match found.
 * In the true case, the diff is appended to the diffs file.
 */
static bool
results_differ(const char *testname, const char *resultsfile, const char *default_expectfile)
{
	char		expectfile[MAXPGPATH];
	char		diff[MAXPGPATH];
	char		cmd[MAXPGPATH * 3];
	char		best_expect_file[MAXPGPATH];
    char		diff_opts[MAXPGPATH];
	char	   *diff_opts_st = diff_opts;
	char	   *diff_opts_en = diff_opts + sizeof(diff_opts);
	char		m_pretty_diff_opts[MAXPGPATH];
	char		generated_initfile[MAXPGPATH];
	char	   *pretty_diff_opts_st = m_pretty_diff_opts;
	char	   *pretty_diff_opts_en = m_pretty_diff_opts + sizeof(m_pretty_diff_opts);
	char		buf[MAXPGPATH];
	FILE	   *difffile;
	int			best_line_count;
	int			i;
	int			l;
	const char *platform_expectfile;
	const char *ignore_plans_opts;
	_stringlist *sl;

	/*
	 * We can pass either the resultsfile or the expectfile, they should have
	 * the same type (filename.type) anyway.
	 */
	platform_expectfile = get_expectfile(testname, resultsfile, default_expectfile);

	if (platform_expectfile)
		strlcpy(expectfile, platform_expectfile, sizeof(expectfile));
	else
		strlcpy(expectfile, default_expectfile, sizeof(expectfile));

	if (ignore_plans)
		ignore_plans_opts = " -gpd_ignore_plans";
	else
		ignore_plans_opts = "";

	/* Name to use for temporary diff file */
	snprintf(diff, sizeof(diff), "%s.diff", resultsfile);
    
	/* Add init file arguments if provided via commandline */
	diff_opts_st += snprintf(diff_opts_st,
							 diff_opts_en - diff_opts_st,
							 "%s%s", basic_diff_opts, ignore_plans_opts);

	pretty_diff_opts_st += snprintf(pretty_diff_opts_st,
									pretty_diff_opts_en - pretty_diff_opts_st,
									"%s%s", pretty_diff_opts, ignore_plans_opts);

	for (sl = init_file_list; sl != NULL; sl = sl->next)
	{
		diff_opts_st += snprintf(diff_opts_st,
								 diff_opts_en - diff_opts_st,
								 " --gpd_init %s", sl->str);

		pretty_diff_opts_st += snprintf(pretty_diff_opts_st,
										pretty_diff_opts_en - pretty_diff_opts_st,
										" --gpd_init %s", sl->str);
	}

	/* Add auto generated init file if it is generated */
	snprintf(buf, sizeof(buf), "%s.initfile", resultsfile);
	if (file_exists(buf))
	{
		snprintf(generated_initfile, sizeof(generated_initfile),
				 "--gpd_init %s", buf);
	}
	else
	{
		memset(generated_initfile, '\0', sizeof(generated_initfile));
	}

	/* OK, run the diff */
	snprintf(cmd, sizeof(cmd),
			 "%s %s %s \"%s\" \"%s\" > \"%s\"",
			 gpdiffprog, diff_opts, generated_initfile, expectfile, resultsfile, diff);

	/* Is the diff file empty? */
	if (run_diff(cmd, diff) == 0)
	{
		unlink(diff);
		return false;
	}

	/* There may be secondary comparison files that match better */
	best_line_count = file_line_count(diff);
	strcpy(best_expect_file, expectfile);

	for (i = 0; i <= 9; i++)
	{
		char	   *alt_expectfile;

		alt_expectfile = get_alternative_expectfile(expectfile, i);
		if (!alt_expectfile)
		{
			fprintf(stderr, _("Unable to check secondary comparison files: %s\n"),
					strerror(errno));
			exit(2);
		}

		if (!file_exists(alt_expectfile))
		{
			free(alt_expectfile);
			continue;
		}

		snprintf(cmd, sizeof(cmd),
				 "%s %s %s \"%s\" \"%s\" > \"%s\"",
				 gpdiffprog, diff_opts, generated_initfile, alt_expectfile, resultsfile, diff);

		if (run_diff(cmd, diff) == 0)
		{
			unlink(diff);
			free(alt_expectfile);
			return false;
		}

		l = file_line_count(diff);
		if (l < best_line_count)
		{
			/* This diff was a better match than the last one */
			best_line_count = l;
			strlcpy(best_expect_file, alt_expectfile, sizeof(best_expect_file));
		}
		free(alt_expectfile);
	}

	/*
	 * fall back on the canonical results file if we haven't tried it yet and
	 * haven't found a complete match yet.
	 *
	 * In GPDB, platform_expectfile is used for determining ORCA/planner/resgroup
	 * expect files, wheras in upstream that is not the case and it is based on
	 * the underlying platform. Thus, it is unnecessary and confusing to compare
	 * against default answer file even when platform_expect file exists. It gets
	 * confusing because the below block chooses the best expect file based on
	 * the number of lines in diff file.
	 */

#if 0
	if (platform_expectfile)
	{
		snprintf(cmd, sizeof(cmd),
				 "%s %s %s \"%s\" \"%s\" > \"%s\"",
				 gpdiffprog, diff_opts, generated_initfile, default_expectfile, resultsfile, diff);

		if (run_diff(cmd, diff) == 0)
		{
			/* No diff = no changes = good */
			unlink(diff);
			return false;
		}

		l = file_line_count(diff);
		if (l < best_line_count)
		{
			/* This diff was a better match than the last one */
			best_line_count = l;
			strlcpy(best_expect_file, default_expectfile, sizeof(best_expect_file));
		}
	}
#endif
	/*
	 * Use the best comparison file to generate the "pretty" diff, which we
	 * append to the diffs summary file.
	 */

	/* Write diff header */
	difffile = fopen(difffilename, "a");
	if (difffile)
	{
		fprintf(difffile,
				"diff %s %s %s\n",
				pretty_diff_opts, best_expect_file, resultsfile);
		fclose(difffile);
	}

	/* Run diff */
	snprintf(cmd, sizeof(cmd),
			 "%s %s %s \"%s\" \"%s\" >> \"%s\"",
			 gpdiffprog, m_pretty_diff_opts, generated_initfile, best_expect_file, resultsfile, difffilename);
	run_diff(cmd, difffilename);

	unlink(diff);
	return true;
}

/*
 * Wait for specified subprocesses to finish, and return their exit
 * statuses into statuses[] and stop times into stoptimes[]
 *
 * If names isn't NULL, print each subprocess's name as it finishes
 *
 * Note: it's OK to scribble on the pids array, but not on the names array
 */
static void
wait_for_tests(PID_TYPE * pids, int *statuses, instr_time *stoptimes,
			   char **names, int num_tests)
{
	int			tests_left;
	int			i;

#ifdef WIN32
	PID_TYPE   *active_pids = pg_malloc(num_tests * sizeof(PID_TYPE));

	memcpy(active_pids, pids, num_tests * sizeof(PID_TYPE));
#endif

	tests_left = num_tests;
	while (tests_left > 0)
	{
		PID_TYPE	p;

#ifndef WIN32
		int			exit_status;

		p = wait(&exit_status);

		if (p == INVALID_PID)
		{
			fprintf(stderr, _("failed to wait for subprocesses: %s\n"),
					strerror(errno));
			exit(2);
		}
#else
		DWORD		exit_status;
		int			r;

		r = WaitForMultipleObjects(tests_left, active_pids, FALSE, INFINITE);
		if (r < WAIT_OBJECT_0 || r >= WAIT_OBJECT_0 + tests_left)
		{
			fprintf(stderr, _("failed to wait for subprocesses: error code %lu\n"),
					GetLastError());
			exit(2);
		}
		p = active_pids[r - WAIT_OBJECT_0];
		/* compact the active_pids array */
		active_pids[r - WAIT_OBJECT_0] = active_pids[tests_left - 1];
#endif							/* WIN32 */

		for (i = 0; i < num_tests; i++)
		{
			if (p == pids[i])
			{
#ifdef WIN32
				GetExitCodeProcess(pids[i], &exit_status);
				CloseHandle(pids[i]);
#endif
				pids[i] = INVALID_PID;
				statuses[i] = (int) exit_status;
				INSTR_TIME_SET_CURRENT(stoptimes[i]);
				if (names)
					status(" %s", names[i]);
				tests_left--;
				break;
			}
		}
	}

#ifdef WIN32
	free(active_pids);
#endif
}

/*
 * report nonzero exit code from a test process
 */
static void
log_child_failure(int exitstatus)
{
	if (WIFEXITED(exitstatus))
		status(_(" (test process exited with exit code %d)"),
			   WEXITSTATUS(exitstatus));
	else if (WIFSIGNALED(exitstatus))
	{
#if defined(WIN32)
		status(_(" (test process was terminated by exception 0x%X)"),
			   WTERMSIG(exitstatus));
#else
		status(_(" (test process was terminated by signal %d: %s)"),
			   WTERMSIG(exitstatus), pg_strsignal(WTERMSIG(exitstatus)));
#endif
	}
	else
		status(_(" (test process exited with unrecognized status %d)"),
			   exitstatus);
}

/*
 * Run all the tests specified in one schedule file
 */
static void
run_schedule(const char *schedule, test_function tfunc)
{
#define MAX_PARALLEL_TESTS 100
	char	   *tests[MAX_PARALLEL_TESTS];
	_stringlist *resultfiles[MAX_PARALLEL_TESTS];
	_stringlist *expectfiles[MAX_PARALLEL_TESTS];
	_stringlist *tags[MAX_PARALLEL_TESTS];
	PID_TYPE	pids[MAX_PARALLEL_TESTS];
	instr_time	starttimes[MAX_PARALLEL_TESTS];
	instr_time	stoptimes[MAX_PARALLEL_TESTS];
	int			statuses[MAX_PARALLEL_TESTS];
	_stringlist *ignorelist = NULL;
	char		scbuf[1024];
	FILE	   *scf;
	int			line_num = 0;

	memset(tests, 0, sizeof(tests));
	memset(resultfiles, 0, sizeof(resultfiles));
	memset(expectfiles, 0, sizeof(expectfiles));
	memset(tags, 0, sizeof(tags));

	scf = fopen(schedule, "r");
	if (!scf)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, schedule, strerror(errno));
		exit(2);
	}

	while (fgets(scbuf, sizeof(scbuf), scf))
	{
		char	   *test = NULL;
		char	   *c;
		int			num_tests;
		int			excluded_tests;
		bool		inword;
		int			i;
		struct timeval start_time;

		line_num++;

		/* strip trailing whitespace, especially the newline */
		i = strlen(scbuf);
		while (i > 0 && isspace((unsigned char) scbuf[i - 1]))
			scbuf[--i] = '\0';

		if (scbuf[0] == '\0' || scbuf[0] == '#')
			continue;
		if (strncmp(scbuf, "test: ", 6) == 0)
			test = scbuf + 6;
		else if (strncmp(scbuf, "ignore: ", 8) == 0)
		{
			c = scbuf + 8;
			while (*c && isspace((unsigned char) *c))
				c++;
			add_stringlist_item(&ignorelist, c);

			/*
			 * Note: ignore: lines do not run the test, they just say that
			 * failure of this test when run later on is to be ignored. A bit
			 * odd but that's how the shell-script version did it.
			 */
			continue;
		}
		else
		{
			fprintf(stderr, _("syntax error in schedule file \"%s\" line %d: %s\n"),
					schedule, line_num, scbuf);
			exit(2);
		}

		num_tests = 0;
		excluded_tests = 0;
		inword = false;
		for (c = test;; c++)
		{
			if (*c == '\0' || isspace((unsigned char) *c))
			{
				if (inword)
				{
					/* Reached end of a test name */
					char		sav;

					if (num_tests >= MAX_PARALLEL_TESTS)
					{
						fprintf(stderr, _("too many parallel tests (more than %d) in schedule file \"%s\" line %d: %s\n"),
								MAX_PARALLEL_TESTS, schedule, line_num, scbuf);
						exit(2);
					}
					sav = *c;
					*c = '\0';
					tests[num_tests] = pg_strdup(test);
					num_tests++;
					*c = sav;
					inword = false;

					/*
					 * GPDB: if this test is in the exclude list, don't add it to the
					 * array, after all.
					 */
					if (should_exclude_test(tests[num_tests - 1]))
					{
						excluded_tests++;
						num_tests--;
					}
				}
				if (*c == '\0')
					break;		/* loop exit is here */
			}
			else if (!inword)
			{
				/* Start of a test name */
				test = c;
				inword = true;
			}
		}

		/* The last test in the line needs to be checked for exclusion */
		if (num_tests - 1 >= 0 && should_exclude_test(tests[num_tests - 1]))
		{
			num_tests--;
			excluded_tests++;
		}

		if (num_tests == 0 && excluded_tests == 0)
		{
			fprintf(stderr, _("syntax error in schedule file \"%s\" line %d: %s\n"),
					schedule, line_num, scbuf);
			exit(2);
		}

		/* All tests in this line are to be excluded, so go to the next line */
		if (num_tests == 0)
			continue;

		if (!cluster_healthy())
			break;

		gettimeofday(&start_time, NULL);
		if (num_tests == 1)
		{
			status(_("test %-28s ... "), tests[0]);
			pids[0] = (tfunc) (tests[0], &resultfiles[0], &expectfiles[0], &tags[0]);
			INSTR_TIME_SET_CURRENT(starttimes[0]);
			wait_for_tests(pids, statuses, stoptimes, NULL, 1);
			/* status line is finished below */
		}
		else if (max_concurrent_tests > 0 && max_concurrent_tests < num_tests)
		{
			fprintf(stderr, _("too many parallel tests (more than %d) in schedule file \"%s\" line %d: %s\n"),
					max_concurrent_tests, schedule, line_num, scbuf);
			exit(2);
		}
		else if (max_connections > 0 && max_connections < num_tests)
		{
			int			oldest = 0;

			status(_("parallel group (%d tests, in groups of %d): "),
				   num_tests, max_connections);
			for (i = 0; i < num_tests; i++)
			{
				if (i - oldest >= max_connections)
				{
					wait_for_tests(pids + oldest, statuses + oldest,
								   stoptimes + oldest,
								   tests + oldest, i - oldest);
					oldest = i;
				}
				pids[i] = (tfunc) (tests[i], &resultfiles[i], &expectfiles[i], &tags[i]);
				INSTR_TIME_SET_CURRENT(starttimes[i]);
			}
			wait_for_tests(pids + oldest, statuses + oldest,
						   stoptimes + oldest,
						   tests + oldest, i - oldest);
			status_end();
		}
		else
		{
			status(_("parallel group (%d tests): "), num_tests);
			for (i = 0; i < num_tests; i++)
			{
				pids[i] = (tfunc) (tests[i], &resultfiles[i], &expectfiles[i], &tags[i]);
				INSTR_TIME_SET_CURRENT(starttimes[i]);
			}
			wait_for_tests(pids, statuses, stoptimes, tests, num_tests);
			status_end();
		}

		/* Check results for all tests */
		for (i = 0; i < num_tests; i++)
		{
			_stringlist *rl,
					   *el,
					   *tl;
			bool		differ = false;
			instr_time diff_start_time;
			instr_time diff_stop_time;

			if (num_tests > 1)
				status(_("     %-28s ... "), tests[i]);

			/*
			 * Advance over all three lists simultaneously.
			 *
			 * Compare resultfiles[j] with expectfiles[j] always. Tags are
			 * optional but if there are tags, the tag list has the same
			 * length as the other two lists.
			 */

			INSTR_TIME_SET_CURRENT(diff_start_time);
			for (rl = resultfiles[i], el = expectfiles[i], tl = tags[i];
				 rl != NULL;	/* rl and el have the same length */
				 rl = rl->next, el = el->next,
				 tl = tl ? tl->next : NULL)
			{
				bool		newdiff;

				newdiff = results_differ(tests[i], rl->str, el->str);
				if (newdiff && tl)
				{
					printf("%s ", tl->str);
				}
				differ |= newdiff;
			}
			INSTR_TIME_SET_CURRENT(diff_stop_time);

			if (differ)
			{
				bool		ignore = false;
				_stringlist *sl;

				for (sl = ignorelist; sl != NULL; sl = sl->next)
				{
					if (strcmp(tests[i], sl->str) == 0)
					{
						ignore = true;
						break;
					}
				}
				if (ignore)
				{
					status(_("failed (ignored)"));
					fail_ignore_count++;
				}
				else
				{
					status(_("FAILED"));
					fail_count++;
				}
			}
			else
			{
				status(_("ok    "));	/* align with FAILED */
				success_count++;
			}

			if (statuses[i] != 0)
				log_child_failure(statuses[i]);

			INSTR_TIME_SUBTRACT(stoptimes[i], starttimes[i]);
			status(_(" %8.0f ms"), INSTR_TIME_GET_MILLISEC(stoptimes[i]));

			INSTR_TIME_SUBTRACT(diff_stop_time, diff_start_time);
			status(_(" (diff %4.0f ms)"), INSTR_TIME_GET_MILLISEC(diff_stop_time));

			status_end();
		}

		for (i = 0; i < num_tests; i++)
		{
			pg_free(tests[i]);
			tests[i] = NULL;
			free_stringlist(&resultfiles[i]);
			free_stringlist(&expectfiles[i]);
			free_stringlist(&tags[i]);
		}
	}

	free_stringlist(&ignorelist);

	fclose(scf);
}

/*
 * Run a single test
 */
static void
run_single_test(const char *test, test_function tfunc)
{
	PID_TYPE	pid;
	instr_time	starttime;
	instr_time	stoptime;
	int			exit_status;
	_stringlist *resultfiles = NULL;
	_stringlist *expectfiles = NULL;
	_stringlist *tags = NULL;
	_stringlist *rl,
			   *el,
			   *tl;
	bool		differ = false;

	if (!cluster_healthy())
		return;

	if (should_exclude_test((char *) test))
		return;

	status(_("test %-28s ... "), test);
	pid = (tfunc) (test, &resultfiles, &expectfiles, &tags);
	INSTR_TIME_SET_CURRENT(starttime);
	wait_for_tests(&pid, &exit_status, &stoptime, NULL, 1);

	/*
	 * Advance over all three lists simultaneously.
	 *
	 * Compare resultfiles[j] with expectfiles[j] always. Tags are optional
	 * but if there are tags, the tag list has the same length as the other
	 * two lists.
	 */
	for (rl = resultfiles, el = expectfiles, tl = tags;
		 rl != NULL;			/* rl and el have the same length */
		 rl = rl->next, el = el->next,
		 tl = tl ? tl->next : NULL)
	{
		bool		newdiff;

		newdiff = results_differ(test, rl->str, el->str);
		if (newdiff && tl)
		{
			printf("%s ", tl->str);
		}
		differ |= newdiff;
	}

	if (differ)
	{
		status(_("FAILED"));
		fail_count++;
	}
	else
	{
		status(_("ok    "));	/* align with FAILED */
		success_count++;
	}

	if (exit_status != 0)
		log_child_failure(exit_status);

	INSTR_TIME_SUBTRACT(stoptime, starttime);
	status(_(" %8.0f ms"), INSTR_TIME_GET_MILLISEC(stoptime));

	status_end();
}

/*
 * Get error message pattern based on return code
 */
static const char *
get_helper_err_pattern(int rc)
{
	if (rc == -2)
	{
		return "The program \"%s\" is needed by %s "
			"has differece in build version (check \"GpTest.pm\" import) with "
			"\"%s\".\nPlease rebuild tests or reconfigure the project.\n";
	}
	/* default error message pattern */
	return "The program \"%s\" is needed by %s "
		"but was not found in the same directory as \"%s\".\n"
		"Please check that file exists (or is it a regular file).\n";
}

/*
 * Find the other binaries that we need. Currently, gpdiff.pl and
 * gpstringsubs.pl.
 */
static void
find_helper_programs(const char *argv0)
{
	int 		rc;
	char		full_path[MAXPGPATH];
	const char 	*msg;

	if ((rc = find_other_exec(argv0, "gpdiff.pl", "gpdiff.pl " GP_VERSION"\n", gpdiffprog)) != 0)
	{
		msg = get_helper_err_pattern(rc);

		if (find_my_exec(argv0, full_path) < 0)
			strlcpy(full_path, progname, sizeof(full_path));

		fprintf(stderr, _(msg), "gpdiff.pl", progname, full_path);
		exit(1);
	}

	if ((rc = find_other_exec(argv0, "gpstringsubs.pl", "gpstringsubs.pl " GP_VERSION"\n", gpstringsubsprog)) != 0)
	{
		msg = get_helper_err_pattern(rc);

		if (find_my_exec(argv0, full_path) < 0)
			strlcpy(full_path, progname, sizeof(full_path));

		fprintf(stderr, _(msg), "gpstringsubs.pl", progname, full_path);
		exit(1);
	}
}
/*
 * Create the summary-output files (making them empty if already existing)
 */
static void
open_result_files(void)
{
	char		file[MAXPGPATH];
	FILE	   *difffile;

	/* create outputdir directory if not present */
	if (!directory_exists(outputdir))
		make_directory(outputdir);

	/* create the log file (copy of running status output) */
	snprintf(file, sizeof(file), "%s/regression.out", outputdir);
	logfilename = pg_strdup(file);
	logfile = fopen(logfilename, "w");
	if (!logfile)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
				progname, logfilename, strerror(errno));
		exit(2);
	}

	/* create the diffs file as empty */
	snprintf(file, sizeof(file), "%s/regression.diffs", outputdir);
	difffilename = pg_strdup(file);
	difffile = fopen(difffilename, "w");
	if (!difffile)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for writing: %s\n"),
				progname, difffilename, strerror(errno));
		exit(2);
	}
	/* we don't keep the diffs file open continuously */
	fclose(difffile);

	/* also create the results directory if not present */
	snprintf(file, sizeof(file), "%s/results", outputdir);
	if (!directory_exists(file))
		make_directory(file);
}

static void
drop_database_if_exists(const char *dbname)
{
	header(_("dropping database \"%s\""), dbname);
	psql_command("postgres", "DROP DATABASE IF EXISTS \"%s\"", dbname);
}

static void
create_database(const char *dbname)
{
	_stringlist *sl;

	/*
	 * We use template0 so that any installation-local cruft in template1 will
	 * not mess up the tests.
	 */
	header(_("creating database \"%s\""), dbname);
	if (encoding)
		psql_command("postgres", "CREATE DATABASE \"%s\" TEMPLATE=template0 ENCODING='%s'", dbname, encoding);
	else
		psql_command("postgres", "CREATE DATABASE \"%s\" TEMPLATE=template0%s", dbname,
					 (nolocale) ? " LC_COLLATE='C' LC_CTYPE='C'" : "");
	psql_command(dbname,
				 "ALTER DATABASE \"%s\" SET lc_messages TO 'C';"
				 "ALTER DATABASE \"%s\" SET lc_monetary TO 'C';"
				 "ALTER DATABASE \"%s\" SET lc_numeric TO 'C';"
				 "ALTER DATABASE \"%s\" SET lc_time TO 'C';"
				 "ALTER DATABASE \"%s\" SET bytea_output TO 'hex';"
				 "ALTER DATABASE \"%s\" SET timezone_abbreviations TO 'Default';",
				 dbname, dbname, dbname, dbname, dbname, dbname);

	/*
	 * Install any requested procedural languages.  We use CREATE OR REPLACE
	 * so that this will work whether or not the language is preinstalled.
	 */
	for (sl = loadlanguage; sl != NULL; sl = sl->next)
	{
		header(_("installing %s"), sl->str);
		psql_command(dbname, "CREATE OR REPLACE LANGUAGE \"%s\"", sl->str);
	}

	/*
	 * Install any requested extensions.  We use CREATE IF NOT EXISTS so that
	 * this will work whether or not the extension is preinstalled.
	 */
	for (sl = loadextension; sl != NULL; sl = sl->next)
	{
		header(_("installing %s"), sl->str);
		psql_command(dbname, "CREATE EXTENSION IF NOT EXISTS \"%s\"", sl->str);
	}

}

static void
drop_role_if_exists(const char *rolename)
{
	header(_("dropping role \"%s\""), rolename);
	psql_command("postgres", "DROP ROLE IF EXISTS \"%s\"", rolename);
}

static void
create_role(const char *rolename, const _stringlist *granted_dbs)
{
	header(_("creating role \"%s\""), rolename);
	psql_command("postgres", "CREATE ROLE \"%s\" WITH LOGIN", rolename);
	for (; granted_dbs != NULL; granted_dbs = granted_dbs->next)
	{
		psql_command("postgres", "GRANT ALL ON DATABASE \"%s\" TO \"%s\"",
					 granted_dbs->str, rolename);
	}
}

static char *
trim_white_space(char *str)
{
	char *end;
	while (isspace((unsigned char)*str))
	{
		str++;
	}

	if (*str == 0)
	{
		return str;
	}

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
	{
		end--;
	}

	*(end+1) = 0;
	return str;
}

/*
 * Should the test be excluded from running
 */
static bool
should_exclude_test(char *test)
{
	_stringlist *sl;
	for (sl = exclude_tests; sl != NULL; sl = sl->next)
	{
		if (strncmp(test, sl->str, strlen(sl->str)) == 0)
			return true;
	}

	return false;
}

/*
 * @brief Check whether a feature (e.g. optimizer) is on or off.
 * If the input feature is optimizer, then set the global
 * variable "optimizer_enabled" accordingly.
 *
 * @param feature_name Name of the feature to be checked (e.g. optimizer)
 * @param feature_value Expected value when the feature is enabled (i.e., on or group)
 * @param on_msg Message to be printed when the feature is enabled
 * @param off_msg Message to be printed when the feature is disabled
 * @return true if the feature is enabled; false otherwise
 */
static bool
check_feature_status(const char *feature_name, const char *feature_value,
					 const char *on_msg, const char *off_msg)
{
	char psql_cmd[MAXPGPATH];
	char statusfilename[MAXPGPATH];
	char line[1024];
	bool isEnabled = false;
	int len;

	header(_("checking %s status"), feature_name);

	snprintf(statusfilename, sizeof(statusfilename), "%s/%s_status.out", outputdir, feature_name);

	len = snprintf(psql_cmd, sizeof(psql_cmd),
			"\"%s%spsql\" -X -t -c \"show %s;\" -o \"%s\" -d \"postgres\"",
			bindir ? bindir : "",
			bindir ? "/" : "",
			feature_name,
			statusfilename);

	if (len >= sizeof(psql_cmd))
		exit(2);

	if (system(psql_cmd) != 0)
		exit(2);

	FILE *statusfile = fopen(statusfilename, "r");
	if (!statusfile)
	{
		fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
				progname, statusfilename, strerror(errno));
		exit(2);
	}

	while (fgets(line, sizeof(line), statusfile))
	{
		char *trimmed = trim_white_space(line);
		if (strcmp(trimmed, feature_value) == 0)
		{
			status(_("%s"), on_msg);
			isEnabled = true;
			break;
		}
	}
	if (!isEnabled)
		status(_("%s"), off_msg);

	status_end();
	fclose(statusfile);
	unlink(statusfilename);
	return isEnabled;
}

static void
help(void)
{
	printf(_("PostgreSQL regression test driver\n"));
	printf(_("\n"));
	printf(_("Usage:\n  %s [OPTION]... [EXTRA-TEST]...\n"), progname);
	printf(_("\n"));
	printf(_("Options:\n"));
	printf(_("      --bindir=BINPATH          use BINPATH for programs that are run;\n"));
	printf(_("                                if empty, use PATH from the environment\n"));
	printf(_("      --config-auth=DATADIR     update authentication settings for DATADIR\n"));
	printf(_("      --create-role=ROLE        create the specified role before testing\n"));
	printf(_("      --dbname=DB               use database DB (default \"regression\")\n"));
	printf(_("      --debug                   turn on debug mode in programs that are run\n"));
	printf(_("      --dlpath=DIR              look for dynamic libraries in DIR\n"));
	printf(_("      --encoding=ENCODING       use ENCODING as the encoding\n"));
	printf(_("  -h, --help                    show this help, then exit\n"));
	printf(_("      --inputdir=DIR            take input files from DIR (default \".\")\n"));
	printf(_("      --launcher=CMD            use CMD as launcher of psql\n"));
	printf(_("      --load-extension=EXT      load the named extension before running the\n"));
	printf(_("                                tests; can appear multiple times\n"));
	printf(_("      --load-language=LANG      load the named language before running the\n"));
	printf(_("                                tests; can appear multiple times\n"));
	printf(_("      --max-connections=N       maximum number of concurrent connections\n"));
	printf(_("                                (default is 0, meaning unlimited)\n"));
	printf(_("      --max-concurrent-tests=N  maximum number of concurrent tests in schedule\n"));
	printf(_("                                (default is 0, meaning unlimited)\n"));
	printf(_("      --outputdir=DIR           place output files in DIR (default \".\")\n"));
	printf(_("      --schedule=FILE           use test ordering schedule from FILE\n"));
	printf(_("                                (can be used multiple times to concatenate)\n"));
	printf(_("      --temp-instance=DIR       create a temporary instance in DIR\n"));
	printf(_("      --use-existing            use an existing installation\n"));
	/* Please put GPDB specific options here, at the end */
	printf(_("      --prehook=NAME            pre-hook name (default \"\")\n"));
	printf(_("      --exclude-tests=TEST      comma or space delimited tests to exclude from running\n"));
	printf(_("      --exclude-file=FILE       file with tests to exclude from running, one test name per line\n"));
    printf(_("      --init-file=GPD_INIT_FILE  init file to be used for gpdiff (could be used multiple times)\n"));
	printf(_("      --ignore-plans            ignore any explain plan diffs\n"));
	printf(_("      --print-failure-diffs     Print the diff file to standard out after a failure\n"));
	printf(_("      --tablespace-dir=DIR      place tablespace files in DIR/testtablespace (default \"./testtablespace\")\n"));
	/* end of GPDB specific options */
	printf(_("  -V, --version                 output version information, then exit\n"));
	printf(_("\n"));
	printf(_("Options for \"temp-instance\" mode:\n"));
	printf(_("      --no-locale               use C locale\n"));
	printf(_("      --port=PORT               start postmaster on PORT\n"));
	printf(_("      --temp-config=FILE        append contents of FILE to temporary config\n"));
	printf(_("\n"));
	printf(_("Options for using an existing installation:\n"));
	printf(_("      --host=HOST               use postmaster running on HOST\n"));
	printf(_("      --port=PORT               use postmaster running at PORT\n"));
	printf(_("      --user=USER               connect as USER\n"));
	printf(_("      --sslmode=SSLMODE         connect with SSLMODE\n"));
	printf(_("\n"));
	printf(_("The exit status is 0 if all tests passed, 1 if some tests failed, and 2\n"));
	printf(_("if the tests could not be run for some reason.\n"));
	printf(_("\n"));
	printf(_("Report bugs to <bugs@greenplum.org>.\n"));
}

int
regression_main(int argc, char *argv[], init_function ifunc, test_function tfunc)
{
	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{"dbname", required_argument, NULL, 1},
		{"debug", no_argument, NULL, 2},
		{"inputdir", required_argument, NULL, 3},
		{"load-language", required_argument, NULL, 4},
		{"max-connections", required_argument, NULL, 5},
		{"encoding", required_argument, NULL, 6},
		{"outputdir", required_argument, NULL, 7},
		{"schedule", required_argument, NULL, 8},
		{"temp-instance", required_argument, NULL, 9},
		{"no-locale", no_argument, NULL, 10},
		{"host", required_argument, NULL, 13},
		{"port", required_argument, NULL, 14},
		{"user", required_argument, NULL, 15},
		{"bindir", required_argument, NULL, 16},
		{"dlpath", required_argument, NULL, 17},
		{"create-role", required_argument, NULL, 18},
		{"temp-config", required_argument, NULL, 19},
		{"use-existing", no_argument, NULL, 20},
		{"launcher", required_argument, NULL, 21},
		{"load-extension", required_argument, NULL, 22},
		{"config-auth", required_argument, NULL, 24},
		{"max-concurrent-tests", required_argument, NULL, 25},
		{"init-file", required_argument, NULL, 80},
		{"exclude-tests", required_argument, NULL, 81},
		{"ignore-plans", no_argument, NULL, 82},
		{"prehook", required_argument, NULL, 83},
		{"print-failure-diffs", no_argument, NULL, 84},
		{"tablespace-dir", required_argument, NULL, 85},
		{"exclude-file", required_argument, NULL, 87}, /* 86 conflicts with 'V' */
		{"sslmode", required_argument, NULL, 88},
		{NULL, 0, NULL, 0}
	};

	_stringlist *sl;
	int			c;
	int			i;
	int			option_index;
	char		buf[MAXPGPATH * 4];
	char		buf2[MAXPGPATH * 4];

	pg_logging_init(argv[0]);
	progname = get_progname(argv[0]);
	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_regress"));

	get_restricted_token();

	atexit(stop_postmaster);

#ifndef HAVE_UNIX_SOCKETS
	/* no unix domain sockets available, so change default */
	hostname = "localhost";
#endif

	/*
	 * We call the initialization function here because that way we can set
	 * default parameters and let them be overwritten by the commandline.
	 */
	ifunc(argc, argv);

	if (getenv("PG_REGRESS_DIFF_OPTS"))
		pretty_diff_opts = getenv("PG_REGRESS_DIFF_OPTS");

	while ((c = getopt_long(argc, argv, "hV", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'h':
				help();
				exit(0);
			case 'V':
				puts("pg_regress (PostgreSQL) " PG_VERSION);
				exit(0);
			case 1:

				/*
				 * If a default database was specified, we need to remove it
				 * before we add the specified one.
				 */
				free_stringlist(&dblist);
				split_to_stringlist(optarg, ",", &dblist);
				break;
			case 2:
				debug = true;
				break;
			case 3:
				inputdir = pg_strdup(optarg);
				break;
			case 4:
				add_stringlist_item(&loadlanguage, optarg);
				break;
			case 5:
				max_connections = atoi(optarg);
				break;
			case 6:
				encoding = pg_strdup(optarg);
				break;
			case 7:
				outputdir = pg_strdup(optarg);
				break;
			case 8:
				add_stringlist_item(&schedulelist, optarg);
				break;
			case 9:
				temp_instance = make_absolute_path(optarg);
				break;
			case 10:
				nolocale = true;
				break;
			case 13:
				hostname = pg_strdup(optarg);
				break;
			case 14:
				port = atoi(optarg);
				port_specified_by_user = true;
				break;
			case 15:
				user = pg_strdup(optarg);
				break;
			case 16:
				/* "--bindir=" means to use PATH */
				if (strlen(optarg))
					bindir = pg_strdup(optarg);
				else
					bindir = NULL;
				break;
			case 17:
				dlpath = pg_strdup(optarg);
				break;
			case 18:
				split_to_stringlist(optarg, ",", &extraroles);
				break;
			case 19:
				add_stringlist_item(&temp_configs, optarg);
				break;
			case 20:
				use_existing = true;
				break;
			case 21:
				launcher = pg_strdup(optarg);
				break;
			case 22:
				add_stringlist_item(&loadextension, optarg);
				break;
			case 24:
				config_auth_datadir = pg_strdup(optarg);
				break;
			case 25:
				max_concurrent_tests = atoi(optarg);
				break;

			/* GPDB-added options */
            case 80:
				add_stringlist_item(&init_file_list, optarg);
                break;
            case 81:
                split_to_stringlist(strdup(optarg), ", ", &exclude_tests);
                break;
			case 82:
				ignore_plans = true;
				break;
			case 83:
				prehook = strdup(optarg);
				break;
			case 84:
				print_failure_diffs_is_enabled = true;
				break;
			case 85:
				tablespacedir = strdup(optarg);
				break;
			case 87:
				exclude_tests_file = strdup(optarg);
				load_exclude_tests_file(&exclude_tests, exclude_tests_file);
				break;
			case 88:
				sslmode = strdup(optarg);
				break;

			default:
				/* getopt_long already emitted a complaint */
				fprintf(stderr, _("\nTry \"%s -h\" for more information.\n"),
						progname);
				exit(2);
		}
	}

	/*
	 * if we still have arguments, they are extra tests to run
	 */
	while (argc - optind >= 1)
	{
		add_stringlist_item(&extra_tests, argv[optind]);
		optind++;
	}

	if (config_auth_datadir)
	{
#ifdef ENABLE_SSPI
		config_sspi_auth(config_auth_datadir, user);
#endif
		exit(0);
	}

	if (temp_instance && !port_specified_by_user)

		/*
		 * To reduce chances of interference with parallel installations, use
		 * a port number starting in the private range (49152-65535)
		 * calculated from the version number.  This aids !HAVE_UNIX_SOCKETS
		 * systems; elsewhere, the use of a private socket directory already
		 * prevents interference.
		 */
		port = 0xC000 | (PG_VERSION_NUM & 0x3FFF);

	inputdir = make_absolute_path(inputdir);
	outputdir = make_absolute_path(outputdir);
	dlpath = make_absolute_path(dlpath);
	tablespacedir = make_absolute_path(tablespacedir);

	/*
	 * Initialization
	 */
	find_helper_programs(argv[0]);
	open_result_files();

	if (prehook[0])
	{
		char *fullname = psprintf("%s/sql/hooks/%s.sql", inputdir, prehook);

		if (!file_exists(fullname))
		{
			convert_sourcefiles_in("input/hooks", outputdir, "sql/hooks", "sql");

			if (!file_exists(fullname))
			{
				fprintf(stderr, _("%s: could not open file \"%s\" for reading: %s\n"),
						progname, fullname, strerror(errno));
				exit(2);
			}
		}
	}

	initialize_environment();

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_CORE)
	unlimit_core_size();
#endif

	if (temp_instance)
	{
		FILE	   *pg_conf;
		const char *env_wait;
		int			wait_seconds;

		/*
		 * Prepare the temp instance
		 */

		if (directory_exists(temp_instance))
		{
			header(_("removing existing temp instance"));
			if (!rmtree(temp_instance, true))
			{
				fprintf(stderr, _("\n%s: could not remove temp instance \"%s\"\n"),
						progname, temp_instance);
				exit(2);
			}
		}

		header(_("creating temporary instance"));

		/* make the temp instance top directory */
		make_directory(temp_instance);

		/* and a directory for log files */
		snprintf(buf, sizeof(buf), "%s/log", outputdir);
		if (!directory_exists(buf))
			make_directory(buf);

		/* initdb */
		header(_("initializing database system"));
		snprintf(buf, sizeof(buf),
				 "\"%s%sinitdb\" -D \"%s/data\" --no-clean --no-sync%s%s > \"%s/log/initdb.log\" 2>&1",
				 bindir ? bindir : "",
				 bindir ? "/" : "",
				 temp_instance,
				 debug ? " --debug" : "",
				 nolocale ? " --no-locale" : "",
				 outputdir);
		if (system(buf))
		{
			fprintf(stderr, _("\n%s: initdb failed\nExamine %s/log/initdb.log for the reason.\nCommand was: %s\n"), progname, outputdir, buf);
			exit(2);
		}

		/*
		 * Adjust the default postgresql.conf for regression testing. The user
		 * can specify a file to be appended; in any case we expand logging
		 * and set max_prepared_transactions to enable testing of prepared
		 * xacts.  (Note: to reduce the probability of unexpected shmmax
		 * failures, don't set max_prepared_transactions any higher than
		 * actually needed by the prepared_xacts regression test.)
		 */
		snprintf(buf, sizeof(buf), "%s/data/postgresql.conf", temp_instance);
		pg_conf = fopen(buf, "a");
		if (pg_conf == NULL)
		{
			fprintf(stderr, _("\n%s: could not open \"%s\" for adding extra config: %s\n"), progname, buf, strerror(errno));
			exit(2);
		}
		fputs("\n# Configuration added by pg_regress\n\n", pg_conf);
		fputs("log_autovacuum_min_duration = 0\n", pg_conf);
		fputs("log_checkpoints = on\n", pg_conf);
		fputs("log_line_prefix = '%m [%p] %q%a '\n", pg_conf);
		fputs("log_lock_waits = on\n", pg_conf);
		fputs("log_temp_files = 128kB\n", pg_conf);
		fputs("max_prepared_transactions = 2\n", pg_conf);

		for (sl = temp_configs; sl != NULL; sl = sl->next)
		{
			char	   *temp_config = sl->str;
			FILE	   *extra_conf;
			char		line_buf[1024];

			extra_conf = fopen(temp_config, "r");
			if (extra_conf == NULL)
			{
				fprintf(stderr, _("\n%s: could not open \"%s\" to read extra config: %s\n"), progname, temp_config, strerror(errno));
				exit(2);
			}
			while (fgets(line_buf, sizeof(line_buf), extra_conf) != NULL)
				fputs(line_buf, pg_conf);
			fclose(extra_conf);
		}

		fclose(pg_conf);

#ifdef ENABLE_SSPI

		/*
		 * Since we successfully used the same buffer for the much-longer
		 * "initdb" command, this can't truncate.
		 */
		snprintf(buf, sizeof(buf), "%s/data", temp_instance);
		config_sspi_auth(buf, NULL);
#elif !defined(HAVE_UNIX_SOCKETS)
#error Platform has no means to secure the test installation.
#endif

		/*
		 * Check if there is a postmaster running already.
		 */
		snprintf(buf2, sizeof(buf2),
				 "\"%s%spsql\" -X postgres <%s 2>%s",
				 bindir ? bindir : "",
				 bindir ? "/" : "",
				 DEVNULL, DEVNULL);

		for (i = 0; i < 16; i++)
		{
			if (system(buf2) == 0)
			{
				char		s[16];

				if (port_specified_by_user || i == 15)
				{
					fprintf(stderr, _("port %d apparently in use\n"), port);
					if (!port_specified_by_user)
						fprintf(stderr, _("%s: could not determine an available port\n"), progname);
					fprintf(stderr, _("Specify an unused port using the --port option or shut down any conflicting PostgreSQL servers.\n"));
					exit(2);
				}

				fprintf(stderr, _("port %d apparently in use, trying %d\n"), port, port + 1);
				port++;
				sprintf(s, "%d", port);
				doputenv("PGPORT", s);
			}
			else
				break;
		}

		/*
		 * Start the temp postmaster
		 */
		header(_("starting postmaster"));
		snprintf(buf, sizeof(buf),
				 "\"%s%spostgres\" -D \"%s/data\" -F%s "
				 "-c \"listen_addresses=%s\" -k \"%s\" "
				 "> \"%s/log/postmaster.log\" 2>&1",
				 bindir ? bindir : "",
				 bindir ? "/" : "",
				 temp_instance, debug ? " -d 5" : "",
				 hostname ? hostname : "", sockdir ? sockdir : "",
				 outputdir);
		postmaster_pid = spawn_process(buf);
		if (postmaster_pid == INVALID_PID)
		{
			fprintf(stderr, _("\n%s: could not spawn postmaster: %s\n"),
					progname, strerror(errno));
			exit(2);
		}

		/*
		 * Wait till postmaster is able to accept connections; normally this
		 * is only a second or so, but Cygwin is reportedly *much* slower, and
		 * test builds using Valgrind or similar tools might be too.  Hence,
		 * allow the default timeout of 60 seconds to be overridden from the
		 * PGCTLTIMEOUT environment variable.
		 */
		env_wait = getenv("PGCTLTIMEOUT");
		if (env_wait != NULL)
		{
			wait_seconds = atoi(env_wait);
			if (wait_seconds <= 0)
				wait_seconds = 60;
		}
		else
			wait_seconds = 60;

		for (i = 0; i < wait_seconds; i++)
		{
			/* Done if psql succeeds */
			if (system(buf2) == 0)
				break;

			/*
			 * Fail immediately if postmaster has exited
			 */
#ifndef WIN32
			if (waitpid(postmaster_pid, NULL, WNOHANG) == postmaster_pid)
#else
			if (WaitForSingleObject(postmaster_pid, 0) == WAIT_OBJECT_0)
#endif
			{
				fprintf(stderr, _("\n%s: postmaster failed\nExamine %s/log/postmaster.log for the reason\n"), progname, outputdir);
				exit(2);
			}

			pg_usleep(1000000L);
		}
		if (i >= wait_seconds)
		{
			fprintf(stderr, _("\n%s: postmaster did not respond within %d seconds\nExamine %s/log/postmaster.log for the reason\n"),
					progname, wait_seconds, outputdir);

			/*
			 * If we get here, the postmaster is probably wedged somewhere in
			 * startup.  Try to kill it ungracefully rather than leaving a
			 * stuck postmaster that might interfere with subsequent test
			 * attempts.
			 */
#ifndef WIN32
			if (kill(postmaster_pid, SIGKILL) != 0 &&
				errno != ESRCH)
				fprintf(stderr, _("\n%s: could not kill failed postmaster: %s\n"),
						progname, strerror(errno));
#else
			if (TerminateProcess(postmaster_pid, 255) == 0)
				fprintf(stderr, _("\n%s: could not kill failed postmaster: error code %lu\n"),
						progname, GetLastError());
#endif

			exit(2);
		}

		postmaster_running = true;

#ifdef _WIN64
/* need a series of two casts to convert HANDLE without compiler warning */
#define ULONGPID(x) (unsigned long) (unsigned long long) (x)
#else
#define ULONGPID(x) (unsigned long) (x)
#endif
		printf(_("running on port %d with PID %lu\n"),
			   port, ULONGPID(postmaster_pid));
	}
	else
	{
		/*
		 * Using an existing installation, so may need to get rid of
		 * pre-existing database(s) and role(s)
		 */
		if (!use_existing)
		{
			for (sl = dblist; sl; sl = sl->next)
				drop_database_if_exists(sl->str);
			for (sl = extraroles; sl; sl = sl->next)
				drop_role_if_exists(sl->str);
		}
	}

#ifdef FAULT_INJECTOR
	header(_("faultinjector enabled"));
#else
	header(_("faultinjector not enabled"));
#endif

	/*
	 * Create the test database(s) and role(s)
	 */
	if (!use_existing)
	{
		for (sl = dblist; sl; sl = sl->next)
			create_database(sl->str);
		for (sl = extraroles; sl; sl = sl->next)
			create_role(sl->str, dblist);
	}

	/*
	 * Find out if optimizer is on or off
	 */
	optimizer_enabled = check_feature_status("optimizer", "on",
			"Optimizer enabled. Using optimizer answer files whenever possible",
			"Optimizer disabled. Using planner answer files");

	/*
	 * Find out if gp_resource_manager is group or not
	 */
	resgroup_enabled = check_feature_status("gp_resource_manager", "group",
			"Resource group enabled. Using resource group answer files whenever possible",
			"Resource group disabled. Using default answer files");

	/*
	 * Ready to run the tests
	 */
	header(_("running regression test queries"));

	for (sl = setup_tests; sl != NULL && !halt_work; sl = sl->next)
	{
		run_single_test(sl->str, tfunc);
	}

	for (sl = schedulelist; sl != NULL && !halt_work; sl = sl->next)
	{
		run_schedule(sl->str, tfunc);
	}

	for (sl = extra_tests; sl != NULL && !halt_work; sl = sl->next)
	{
		run_single_test(sl->str, tfunc);
	}

	/*
	 * Shut down temp installation's postmaster
	 */
	if (temp_instance)
	{
		header(_("shutting down postmaster"));
		stop_postmaster();
	}

	/*
	 * If there were no errors, remove the temp instance immediately to
	 * conserve disk space.  (If there were errors, we leave the instance in
	 * place for possible manual investigation.)
	 */
	if (temp_instance && fail_count == 0 && fail_ignore_count == 0)
	{
		header(_("removing temporary instance"));
		if (!rmtree(temp_instance, true))
			fprintf(stderr, _("\n%s: could not remove temp instance \"%s\"\n"),
					progname, temp_instance);
	}

	fclose(logfile);

	/*
	 * Emit nice-looking summary message
	 */
	if (fail_count == 0 && fail_ignore_count == 0)
		snprintf(buf, sizeof(buf),
				 _(" All %d tests passed. "),
				 success_count);
	else if (fail_count == 0)	/* fail_count=0, fail_ignore_count>0 */
		snprintf(buf, sizeof(buf),
				 _(" %d of %d tests passed, %d failed test(s) ignored. "),
				 success_count,
				 success_count + fail_ignore_count,
				 fail_ignore_count);
	else if (fail_ignore_count == 0)	/* fail_count>0 && fail_ignore_count=0 */
		snprintf(buf, sizeof(buf),
				 _(" %d of %d tests failed. "),
				 fail_count,
				 success_count + fail_count);
	else
		/* fail_count>0 && fail_ignore_count>0 */
		snprintf(buf, sizeof(buf),
				 _(" %d of %d tests failed, %d of these failures ignored. "),
				 fail_count + fail_ignore_count,
				 success_count + fail_count + fail_ignore_count,
				 fail_ignore_count);

	putchar('\n');
	for (i = strlen(buf); i > 0; i--)
		putchar('=');
	printf("\n%s\n", buf);
	for (i = strlen(buf); i > 0; i--)
		putchar('=');
	putchar('\n');
	putchar('\n');

	if (file_size(difffilename) > 0)
	{
		if (print_failure_diffs_is_enabled)
			print_contents_of_file(difffilename);

		printf(_("The differences that caused some tests to fail can be viewed in the\n"
				 "file \"%s\".  A copy of the test summary that you see\n"
				 "above is saved in the file \"%s\".\n\n"),
			   difffilename, logfilename);
	}
	else
	{
		unlink(difffilename);
		unlink(logfilename);
	}

	if (fail_count != 0)
		exit(1);

	return 0;
}

/*
 * Issue a command via psql, connecting to the specified database
 *
 */
static void
psql_command_output(const char *database, char *buffer, int buf_len, const char *query,...)
{
	char		query_formatted[1024];
	char		query_escaped[2048];
	char		psql_cmd[MAXPGPATH + 2048];
	va_list		args;
	char	   *s;
	char	   *d;
	FILE *fp;
	int len;

	/* Generate the query with insertion of sprintf arguments */
	va_start(args, query);
	vsnprintf(query_formatted, sizeof(query_formatted), query, args);
	va_end(args);

	/* Now escape any shell double-quote metacharacters */
	d = query_escaped;
	for (s = query_formatted; *s; s++)
	{
		if (strchr("\\\"$`", *s))
			*d++ = '\\';
		*d++ = *s;
	}
	*d = '\0';

	/* And now we can build and execute the shell command */
	len = snprintf(psql_cmd, sizeof(psql_cmd),
				   "\"%s%spsql\" -X -t -c \"%s\" \"%s\"",
				   bindir ? bindir : "",
				   bindir ? "/" : "",
				   query_escaped,
				   database);

	if (len >= sizeof(psql_cmd))
		exit(2);

	/* Execute the command with pipe and read the standard output. */
	if ((fp = popen(psql_cmd, "r")) == NULL)
	{
		fprintf(stderr, "%s: cannot launch shell command\n", progname);
		exit(2);
	}

	if (fgets(buffer, buf_len, fp) == NULL)
	{
		fprintf(stderr, "%s: cannot read the result\n", progname);
		(void) pclose(fp);
		exit(2);
	}

	if (pclose(fp) < 0)
	{
		fprintf(stderr, "%s: cannot close shell command\n", progname);
		exit(2);
	}
}

static bool
cluster_healthy(void)
{
	char line[1024];
	psql_command_output("postgres", line, 1024,
						"SELECT * FROM gp_segment_configuration WHERE status = 'd' OR preferred_role != role;");

	halt_work = false;
	if (strcmp(line, "\n") != 0)
	{
		fprintf(stderr, _("\n==================================\n"));
		fprintf(stderr, _(" Cluster validation failed:\n%s"), line);
		fprintf(stderr, _("==================================\n"));
		halt_work = true;
	}

	return !halt_work;
}

static char *
get_host_name(int16 contentid, char role)
{
	char line[1024];
	char *hostname = NULL;

	psql_command_output("postgres", line, 1024,
						"SELECT hostname FROM gp_segment_configuration WHERE role=\'%c\' AND content = %d;",
						role,
						contentid);

	hostname = psprintf("%s", trim_white_space(line));

	if (strcmp("", hostname) == 0)
	{
		fprintf(stderr, _("%s: failed to determine hostname for content 0 primary\n"),
				progname);
		exit(2);
	}

	return hostname;
}
