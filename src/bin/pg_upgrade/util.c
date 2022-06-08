/*
 *	util.c
 *
 *	utility functions
 *
 *	Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *	src/bin/pg_upgrade/util.c
 */

#include "postgres_fe.h"

#include "common/username.h"
#include "pg_upgrade.h"

#include <signal.h>

#include "greenplum/pg_upgrade_greenplum.h"

LogOpts		log_opts;

static void pg_log_v(eLogType type, const char *fmt, va_list ap) pg_attribute_printf(2, 0);


/*
 * report_status()
 *
 *	Displays the result of an operation (ok, failed, error message,...)
 */
void
report_status(eLogType type, const char *fmt,...)
{
	va_list		args;
	char		message[MAX_STRING];

	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	pg_log(type, "%s\n", message);
}


void
end_progress_output(void)
{
	/*
	 * For output to a tty, erase prior contents of progress line. When either
	 * tty or verbose, indent so that report_status() output will align
	 * nicely.
	 */
	if (log_opts.isatty)
		pg_log(PG_REPORT, "\r%-*s", MESSAGE_WIDTH, "");
	else if (log_opts.verbose)
		pg_log(PG_REPORT, "%-*s", MESSAGE_WIDTH, "");
}

/*
 * Remove any logs generated internally.  To be used once when exiting.
 */
void
cleanup_output_dirs(void)
{
	fclose(log_opts.internal);

	/* Remove dump and log files? */
	if (log_opts.retain)
		return;

	(void) rmtree(log_opts.basedir, true);

	/* Remove pg_upgrade_output.d only if empty */
	switch (pg_check_dir(log_opts.rootdir))
	{
		case 0:					/* non-existent */
		case 3:					/* exists and contains a mount point */
			Assert(false);
			break;

		case 1:					/* exists and empty */
		case 2:					/* exists and contains only dot files */
			(void) rmtree(log_opts.rootdir, true);
			break;

		case 4:					/* exists */

			/*
			 * Keep the root directory as this includes some past log
			 * activity.
			 */
			break;

		default:
			/* different failure, just report it */
			pg_log(PG_WARNING, "could not access directory \"%s\": %m",
				   log_opts.rootdir);
			break;
	}
}

/*
 * prep_status
 *
 *	Displays a message that describes an operation we are about to begin.
 *	We pad the message out to MESSAGE_WIDTH characters so that all of the "ok" and
 *	"failed" indicators line up nicely.
 *
 *	A typical sequence would look like this:
 *		prep_status("about to flarb the next %d files", fileCount );
 *
 *		if(( message = flarbFiles(fileCount)) == NULL)
 *		  report_status(PG_REPORT, "ok" );
 *		else
 *		  pg_log(PG_FATAL, "failed - %s\n", message );
 */
void
prep_status(const char *fmt,...)
{
	va_list		args;
	char		message[MAX_STRING];

	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	/* trim strings */
	pg_log(PG_REPORT, "%-*s", MESSAGE_WIDTH, message);
}

/*
 * prep_status_progress
 *
 *   Like prep_status(), but for potentially longer running operations.
 *   Details about what item is currently being processed can be displayed
 *   with pg_log(PG_STATUS, ...). A typical sequence would look like this:
 *
 *   prep_status_progress("copying files");
 *   for (...)
 *     pg_log(PG_STATUS, "%s", filename);
 *   end_progress_output();
 *   report_status(PG_REPORT, "ok");
 */
void
prep_status_progress(const char *fmt,...)
{
	va_list		args;
	char		message[MAX_STRING];

	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	/*
	 * If outputting to a tty or in verbose, append newline. pg_log_v() will
	 * put the individual progress items onto the next line.
	 */
	if (log_opts.isatty || log_opts.verbose)
		pg_log(PG_REPORT, "%-*s\n", MESSAGE_WIDTH, message);
	else
		pg_log(PG_REPORT, "%-*s", MESSAGE_WIDTH, message);
}

static void
pg_log_v(eLogType type, const char *fmt, va_list ap)
{
	char		message[QUERY_ALLOC];

	vsnprintf(message, sizeof(message), _(fmt), ap);

	/* PG_VERBOSE and PG_STATUS are only output in verbose mode */
	/* fopen() on log_opts.internal might have failed, so check it */
	if (((type != PG_VERBOSE && type != PG_STATUS) || log_opts.verbose) &&
		log_opts.internal != NULL)
	{
		if (type == PG_STATUS)
			/* status messages need two leading spaces and a newline */
			fprintf(log_opts.internal, "  %s\n", message);
		else
			fprintf(log_opts.internal, "%s", message);
		fflush(log_opts.internal);
	}

	switch (type)
	{
		case PG_VERBOSE:
			if (log_opts.verbose)
				printf("%s", message);
			break;

		case PG_STATUS:
			/*
			 * For output to a display, do leading truncation. Append \r so
			 * that the next message is output at the start of the line.
			 *
			 * If going to non-interactive output, only display progress if
			 * verbose is enabled. Otherwise the output gets unreasonably
			 * large by default.
			 */
			if (log_opts.isatty)
				/* -2 because we use a 2-space indent */
				printf("  %s%-*.*s\r",
				/* prefix with "..." if we do leading truncation */
					   strlen(message) <= MESSAGE_WIDTH - 2 ? "" : "...",
					   MESSAGE_WIDTH - 2, MESSAGE_WIDTH - 2,
				/* optional leading truncation */
					   strlen(message) <= MESSAGE_WIDTH - 2 ? message :
					   message + strlen(message) - MESSAGE_WIDTH + 3 + 2);
			else if (log_opts.verbose)
				printf("  %s\n", message);
			break;

		case PG_REPORT:
		case PG_WARNING:
			printf("%s", message);
			break;

		case PG_FATAL:
			printf("\n%s", message);
			printf(_("Failure, exiting\n"));
			exit(1);
			break;

		default:
			break;
	}
	fflush(stdout);
}


void
pg_log(eLogType type, const char *fmt,...)
{
	va_list		args;

	va_start(args, fmt);
	pg_log_v(type, fmt, args);
	va_end(args);
}


void
pg_fatal(const char *fmt,...)
{
	va_list		args;

	va_start(args, fmt);
	pg_log_v(PG_FATAL, fmt, args);
	va_end(args);
	printf(_("Failure, exiting\n"));
	exit(1);
}


void
check_ok(void)
{
	/* all seems well */
	report_status(PG_REPORT, "ok");
	fflush(stdout);
}


/*
 *  Wrapper around pg_fatal to continue check when running in check mode
 *  with --continue-check-on-fatal
 *
 *	Note that there are certain checks that cannot be ignored in spite of
 *  the flag being passed - they may be critical to the check process
 *  itself. One such example is check_proper_datallowconn(), which ensures
 *  that we can connect to user databases to perform the checks themselves.
 *  Certain checks are multi-step, like the check to ensure that only the
 *  install user is the sole user in the target database. If we get the
 *  fatal: "could not determine the number of users", we can't really
 *  proceed with the check. Thus, in these cases the flag will not be
 *  respected.
 */
void
gp_fatal_log(const char *fmt,...)
{
	va_list		args;
	eLogType error_type = PG_FATAL;

	if(is_continue_check_on_fatal())
	{
		set_check_fatal_occured();
		error_type = PG_WARNING;
	}

	va_start(args, fmt);
	pg_log_v(error_type, fmt, args);
	va_end(args);
}


/*
 * quote_identifier()
 *		Properly double-quote a SQL identifier.
 *
 * The result should be pg_free'd, but most callers don't bother because
 * memory leakage is not a big deal in this program.
 */
char *
quote_identifier(const char *s)
{
	char	   *result = pg_malloc(strlen(s) * 2 + 3);
	char	   *r = result;

	*r++ = '"';
	while (*s)
	{
		if (*s == '"')
			*r++ = *s;
		*r++ = *s;
		s++;
	}
	*r++ = '"';
	*r++ = '\0';

	return result;
}


/*
 * Append the given string to the shell command being built in the buffer,
 * with suitable shell-style quoting to create exactly one argument.
 *
 * Forbid LF or CR characters, which have scant practical use beyond designing
 * security breaches.  The Windows command shell is unusable as a conduit for
 * arguments containing LF or CR characters.  A future major release should
 * reject those characters in CREATE ROLE and CREATE DATABASE, because use
 * there eventually leads to errors here.
 */
void
appendShellString(PQExpBuffer buf, const char *str)
{
	const char *p;

#ifndef WIN32
	appendPQExpBufferChar(buf, '\'');
	for (p = str; *p; p++)
	{
		if (*p == '\n' || *p == '\r')
		{
			fprintf(stderr,
					_("shell command argument contains a newline or carriage return: \"%s\"\n"),
					str);
			exit(EXIT_FAILURE);
		}

		if (*p == '\'')
			appendPQExpBufferStr(buf, "'\"'\"'");
		else
			appendPQExpBufferChar(buf, *p);
	}
	appendPQExpBufferChar(buf, '\'');
#else							/* WIN32 */
	int			backslash_run_length = 0;

	/*
	 * A Windows system() argument experiences two layers of interpretation.
	 * First, cmd.exe interprets the string.  Its behavior is undocumented,
	 * but a caret escapes any byte except LF or CR that would otherwise have
	 * special meaning.  Handling of a caret before LF or CR differs between
	 * "cmd.exe /c" and other modes, and it is unusable here.
	 *
	 * Second, the new process parses its command line to construct argv (see
	 * https://msdn.microsoft.com/en-us/library/17w5ykft.aspx).  This treats
	 * backslash-double quote sequences specially.
	 */
	appendPQExpBufferStr(buf, "^\"");
	for (p = str; *p; p++)
	{
		if (*p == '\n' || *p == '\r')
		{
			fprintf(stderr,
					_("shell command argument contains a newline or carriage return: \"%s\"\n"),
					str);
			exit(EXIT_FAILURE);
		}

		/* Change N backslashes before a double quote to 2N+1 backslashes. */
		if (*p == '"')
		{
			while (backslash_run_length)
			{
				appendPQExpBufferStr(buf, "^\\");
				backslash_run_length--;
			}
			appendPQExpBufferStr(buf, "^\\");
		}
		else if (*p == '\\')
			backslash_run_length++;
		else
			backslash_run_length = 0;

		/*
		 * Decline to caret-escape the most mundane characters, to ease
		 * debugging and lest we approach the command length limit.
		 */
		if (!((*p >= 'a' && *p <= 'z') ||
			  (*p >= 'A' && *p <= 'Z') ||
			  (*p >= '0' && *p <= '9')))
			appendPQExpBufferChar(buf, '^');
		appendPQExpBufferChar(buf, *p);
	}

	/*
	 * Change N backslashes at end of argument to 2N backslashes, because they
	 * precede the double quote that terminates the argument.
	 */
	while (backslash_run_length)
	{
		appendPQExpBufferStr(buf, "^\\");
		backslash_run_length--;
	}
	appendPQExpBufferStr(buf, "^\"");
#endif   /* WIN32 */
}


/*
 * Append the given string to the buffer, with suitable quoting for passing
 * the string as a value, in a keyword/pair value in a libpq connection
 * string
 */
void
appendConnStrVal(PQExpBuffer buf, const char *str)
{
	const char *s;
	bool		needquotes;

	/*
	 * If the string is one or more plain ASCII characters, no need to quote
	 * it. This is quite conservative, but better safe than sorry.
	 */
	needquotes = true;
	for (s = str; *s; s++)
	{
		if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
			  (*s >= '0' && *s <= '9') || *s == '_' || *s == '.'))
		{
			needquotes = true;
			break;
		}
		needquotes = false;
	}

	if (needquotes)
	{
		appendPQExpBufferChar(buf, '\'');
		while (*str)
		{
			/* ' and \ must be escaped by to \' and \\ */
			if (*str == '\'' || *str == '\\')
				appendPQExpBufferChar(buf, '\\');

			appendPQExpBufferChar(buf, *str);
			str++;
		}
		appendPQExpBufferChar(buf, '\'');
	}
	else
		appendPQExpBufferStr(buf, str);
}


/*
 * Append a psql meta-command that connects to the given database with the
 * then-current connection's user, host and port.
 */
void
appendPsqlMetaConnect(PQExpBuffer buf, const char *dbname)
{
	const char *s;
	bool		complex;

	/*
	 * If the name is plain ASCII characters, emit a trivial "\connect "foo"".
	 * For other names, even many not technically requiring it, skip to the
	 * general case.  No database has a zero-length name.
	 */
	complex = false;
	for (s = dbname; *s; s++)
	{
		if (*s == '\n' || *s == '\r')
		{
			fprintf(stderr,
					_("database name contains a newline or carriage return: \"%s\"\n"),
					dbname);
			exit(EXIT_FAILURE);
		}

		if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
			  (*s >= '0' && *s <= '9') || *s == '_' || *s == '.'))
		{
			complex = true;
		}
	}

	appendPQExpBufferStr(buf, "\\connect ");
	if (complex)
	{
		PQExpBufferData connstr;

		initPQExpBuffer(&connstr);
		appendPQExpBuffer(&connstr, "dbname=");
		appendConnStrVal(&connstr, dbname);

		appendPQExpBuffer(buf, "-reuse-previous=on ");

		/*
		 * As long as the name does not contain a newline, SQL identifier
		 * quoting satisfies the psql meta-command parser.  Prefer not to
		 * involve psql-interpreted single quotes, which behaved differently
		 * before PostgreSQL 9.2.
		 */
		appendPQExpBufferStr(buf, quote_identifier(connstr.data));

		termPQExpBuffer(&connstr);
	}
	else
		appendPQExpBufferStr(buf, quote_identifier(dbname));
	appendPQExpBufferChar(buf, '\n');
}


/*
 * get_user_info()
 */
int
get_user_info(char **user_name_p)
{
	int			user_id;
	const char *user_name;
	char	   *errstr;

#ifndef WIN32
	user_id = geteuid();
#else
	user_id = 1;
#endif

	user_name = get_user_name(&errstr);
	if (!user_name)
		pg_fatal("%s\n", errstr);

	/* make a copy */
	*user_name_p = pg_strdup(user_name);

	return user_id;
}


/*
 *	str2uint()
 *
 *	convert string to oid
 */
unsigned int
str2uint(const char *str)
{
	return strtoul(str, NULL, 10);
}

uint64
str2uint64(const char *str)
{
	return (uint64) strtoull(str, NULL, 10);
}

/*
 *	pg_putenv()
 *
 *	This is like putenv(), but takes two arguments.
 *	It also does unsetenv() if val is NULL.
 */
void
pg_putenv(const char *var, const char *val)
{
	if (val)
	{
#ifndef WIN32
		char	   *envstr;

		envstr = psprintf("%s=%s", var, val);
		putenv(envstr);

		/*
		 * Do not free envstr because it becomes part of the environment on
		 * some operating systems.  See port/unsetenv.c::unsetenv.
		 */
#else
		SetEnvironmentVariableA(var, val);
#endif
	}
	else
	{
#ifndef WIN32
		unsetenv(var);
#else
		SetEnvironmentVariableA(var, "");
#endif
	}
}
