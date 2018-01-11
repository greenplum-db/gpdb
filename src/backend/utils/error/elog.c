/*-------------------------------------------------------------------------
 *
 * elog.c
 *	  error logging and reporting
 *
 * Some notes about recursion and errors during error processing:
 *
 * We need to be robust about recursive-error scenarios --- for example,
 * if we run out of memory, it's important to be able to report that fact.
 * There are a number of considerations that go into this.
 *
 * First, distinguish between re-entrant use and actual recursion.	It
 * is possible for an error or warning message to be emitted while the
 * parameters for an error message are being computed.	In this case
 * errstart has been called for the outer message, and some field values
 * may have already been saved, but we are not actually recursing.	We handle
 * this by providing a (small) stack of ErrorData records.	The inner message
 * can be computed and sent without disturbing the state of the outer message.
 * (If the inner message is actually an error, this isn't very interesting
 * because control won't come back to the outer message generator ... but
 * if the inner message is only debug or log data, this is critical.)
 *
 * Second, actual recursion will occur if an error is reported by one of
 * the elog.c routines or something they call.	By far the most probable
 * scenario of this sort is "out of memory"; and it's also the nastiest
 * to handle because we'd likely also run out of memory while trying to
 * report this error!  Our escape hatch for this case is to reset the
 * ErrorContext to empty before trying to process the inner error.	Since
 * ErrorContext is guaranteed to have at least 8K of space in it (see mcxt.c),
 * we should be able to process an "out of memory" message successfully.
 * Since we lose the prior error state due to the reset, we won't be able
 * to return to processing the original error, but we wouldn't have anyway.
 * (NOTE: the escape hatch is not used for recursive situations where the
 * inner message is of less than ERROR severity; in that case we just
 * try to process it and return normally.  Usually this will work, but if
 * it ends up in infinite recursion, we will PANIC due to error stack
 * overflow.)
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/error/elog.c,v 1.216 2009/06/25 23:07:15 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

/*
 * OpenSolaris has this header, but Solaris 10 doesn't.
 * Linux and OSX 10.5 have it.
 */
#if !defined(_WIN32) && !defined(_WIN64)
#include <execinfo.h>
#endif

#include "access/transam.h"
#include "access/xact.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"

#include "cdb/cdbvars.h"  /*Gp_segment */
#include "cdb/cdbtm.h"
#include "utils/ps_status.h"    /* get_ps_display_username() */
#include "cdb/cdbselect.h"
#include "pgtime.h"

#include "utils/builtins.h"  /* gp_elog() */

#include "miscadmin.h"

/*
 * dlfcn.h on OSX only has dladdr visible if _DARWIN_C_SOURCE is defined.
 */
#define _DARWIN_C_SOURCE 1
#define _STDBOOL_H_
#include <dlfcn.h>
/*
 * Argh... dlfcn.h on OSX pulls in stdbool.h, changing the type of bool from our
 * definition (char) to gcc's definition (unsigned char if C99, int if not).
 * For some reason the #define _STDBOOL_H_ isn't preventing this.  So, we need to undef bool
 * to get back to our bool type.
 */
#undef bool


#undef _
#define _(x) err_gettext(x)

static const char *err_gettext(const char *str)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format_arg(1)));

#undef _
#define _(x) err_gettext(x)

#undef _
#define _(x) err_gettext(x)

static const char *err_gettext(const char *str)
/* This extension allows gcc to check the format string for consistency with
   the supplied arguments. */
__attribute__((format_arg(1)));

/* Global variables */
ErrorContextCallback *error_context_stack = NULL;

sigjmp_buf *PG_exception_stack = NULL;

extern bool redirection_done;

/* GUC parameters */
int			Log_error_verbosity = PGERROR_VERBOSE;
char	   *Log_line_prefix = NULL;		/* format for extra log line info */
int			Log_destination = LOG_DESTINATION_STDERR;

#ifdef HAVE_SYSLOG

/*
 * Max string length to send to syslog().  Note that this doesn't count the
 * sequence-number prefix we add, and of course it doesn't count the prefix
 * added by syslog itself.	On many implementations it seems that the hard
 * limit is approximately 2K bytes including both those prefixes.
 */
#ifndef PG_SYSLOG_LIMIT
#define PG_SYSLOG_LIMIT 1024
#endif

static bool openlog_done = false;
static char *syslog_ident = NULL;
static int	syslog_facility = LOG_LOCAL0;

static void write_syslog(int level, const char *line);
#endif

#ifdef WIN32
static void write_eventlog(int level, const char *line);
#endif

/* We provide a small stack of ErrorData records for re-entrant cases */
#define ERRORDATA_STACK_SIZE  10

#define CMD_BUFFER_SIZE  1024
#define SYMBOL_SIZE      512
#define ADDRESS_SIZE     20
#define STACK_DEPTH_MAX  100

/*
 * Assembly code, gets the values of the frame pointer.
 * It only works for x86 processors.
 */
#if defined(__i386)
#define ASMFP asm volatile ("movl %%ebp, %0" : "=g" (ulp));
#define GET_PTR_FROM_VALUE(value) ((uint32)value)
#define GET_FRAME_POINTER(x) do { uint64 ulp; ASMFP; x = ulp; } while (0)
#elif defined(__x86_64__)
#define ASMFP asm volatile ("movq %%rbp, %0" : "=g" (ulp));
#define GET_PTR_FROM_VALUE(value) (value)
#define GET_FRAME_POINTER(x) do { uint64 ulp; ASMFP; x = ulp; } while (0)
#else
#define ASMFP
#define GET_PTR_FROM_VALUE(value) (value)
#define GET_FRAME_POINTER(x)
#endif


static ErrorData errordata[ERRORDATA_STACK_SIZE];

static int	errordata_stack_depth = -1; /* index of topmost active frame */

static int	recursion_depth = 0;	/* to detect actual recursion */

/* buffers for formatted timestamps that might be used by both
 * log_line_prefix and csv logs.
 */

#define FORMATTED_TS_LEN 128
static char formatted_start_time[FORMATTED_TS_LEN];
static char formatted_log_time[FORMATTED_TS_LEN];


/* Macro for checking errordata_stack_depth is reasonable */
#define CHECK_STACK_DEPTH() \
	do { \
		if (errordata_stack_depth < 0) \
		{ \
			errordata_stack_depth = -1; \
			ereport(ERROR, (errmsg_internal("errstart was not called"))); \
		} \
	} while (0)


static void cdb_tidy_message(ErrorData *edata);
static void log_line_prefix(StringInfo buf);
static void send_message_to_server_log(ErrorData *edata);
static void send_message_to_frontend(ErrorData *edata);
static char *expand_fmt_string(const char *fmt, ErrorData *edata);
static const char *useful_strerror(int errnum);
static const char *error_severity(int elevel);
static void append_with_tabs(StringInfo buf, const char *str);
static bool is_log_level_output(int elevel, int log_min_level);
static void write_pipe_chunks(char *data, int len, int dest);
static void write_csvlog(ErrorData *edata);
static void elog_debug_linger(ErrorData *edata);


/* verify string is correctly encoded, and escape it if invalid  */
static void verify_and_replace_mbstr(char **str, int len)
{
	Assert(pg_verifymbstr(*str, len, true));

	if (!pg_verifymbstr(*str, len, true))
	{
		pfree(*str);
		*str = pstrdup("Message skipped due to incorrect encoding.");
	}
}

static void setup_formatted_log_time(void);
static void setup_formatted_start_time(void);


/*
 * in_error_recursion_trouble --- are we at risk of infinite error recursion?
 *
 * This function exists to provide common control of various fallback steps
 * that we take if we think we are facing infinite error recursion.  See the
 * callers for details.
 */
bool
in_error_recursion_trouble(void)
{
	/* Pull the plug if recurse more than once */
	return (recursion_depth > 2);
}

/*
 * One of those fallback steps is to stop trying to localize the error
 * message, since there's a significant probability that that's exactly
 * what's causing the recursion.
 */
static inline const char *
err_gettext(const char *str)
{
#ifdef ENABLE_NLS
	if (in_error_recursion_trouble())
		return str;
	else
		return gettext(str);
#else
	return str;
#endif
}


/*
 * elog_internalerror -- report an internal error
 *
 * GPDB only
 *
 * Does not return; exits via longjmp to PG_CATCH error handler.
 */
void
elog_internalerror(const char *filename, int lineno, const char *funcname)
{
	/*
	 * TODO  Why isn't this a FATAL error?
	 *
	 * Also, why aren't we allowing for a specific err message to be passed in?
	 */
    if (errstart(ERROR, filename, lineno, funcname,TEXTDOMAIN))
    {
        errfinish(errcode(ERRCODE_INTERNAL_ERROR),
                  errmsg("Unexpected internal error"));
    }
    /* not reached */
    abort();
}                               /* elog_internalerror */


/*
 * errstart --- begin an error-reporting cycle
 *
 * Create a stack entry and store the given parameters in it.  Subsequently,
 * errmsg() and perhaps other routines will be called to further populate
 * the stack entry.  Finally, errfinish() will be called to actually process
 * the error report.
 *
 * Returns TRUE in normal case.  Returns FALSE to short-circuit the error
 * report (if it's a warning or lower and not to be reported anywhere).
 */
bool
errstart(int elevel, const char *filename, int lineno,
		 const char *funcname, const char *domain)
{
	ErrorData  *edata;
	bool		output_to_server = false;
	bool		output_to_client = false;
	int			i;

	/*
	 * Check some cases in which we want to promote an error into a more
	 * severe error.  None of this logic applies for non-error messages.
	 */
	if (elevel >= ERROR)
	{
		/*
		 * If we are inside a critical section, all errors become PANIC
		 * errors.	See miscadmin.h.
		 */
		if (CritSectionCount > 0)
			elevel = PANIC;

		/*
		 * Check reasons for treating ERROR as FATAL:
		 *
		 * 1. we have no handler to pass the error to (implies we are in the
		 * postmaster or in backend startup).
		 *
		 * 2. ExitOnAnyError mode switch is set (initdb uses this).
		 *
		 * 3. the error occurred after proc_exit has begun to run.	(It's
		 * proc_exit's responsibility to see that this doesn't turn into
		 * infinite recursion!)
		 */
		if (elevel == ERROR)
		{
			if (PG_exception_stack == NULL ||
				ExitOnAnyError ||
				proc_exit_inprogress)
				elevel = FATAL;
		}

		/*
		 * If master process hits FATAL, post PREPARE but before COMMIT / ABORT on segment,
		 * just master process dies silently, leaves dangling prepared xact on segment.
		 * This also introduces inconsistency in the cluster, as xact is commited on master
		 * and some segments and still in-progress on few others.
		 * Hence converting FATAL to PANIC, here to reset master and perform full recovery
		 * instead, which would clean the dangling transaction update to COMMIT / ABORT.
		 */
		if ((elevel == FATAL) && (Gp_role == GP_ROLE_DISPATCH))
		{
			switch (getCurrentDtxState())
			{
				case DTX_STATE_PREPARING:
				case DTX_STATE_PREPARED:
				case DTX_STATE_INSERTING_COMMITTED:
				case DTX_STATE_INSERTED_COMMITTED:
				case DTX_STATE_FORCED_COMMITTED:
				case DTX_STATE_NOTIFYING_COMMIT_PREPARED:
				case DTX_STATE_NOTIFYING_ABORT_SOME_PREPARED:
				case DTX_STATE_NOTIFYING_ABORT_PREPARED:
				case DTX_STATE_RETRY_COMMIT_PREPARED:
				case DTX_STATE_RETRY_ABORT_PREPARED:
					elevel = PANIC;
					break;

				case DTX_STATE_NONE:
				case DTX_STATE_ACTIVE_NOT_DISTRIBUTED:
				case DTX_STATE_ACTIVE_DISTRIBUTED:
				case DTX_STATE_INSERTING_FORGET_COMMITTED:
				case DTX_STATE_INSERTED_FORGET_COMMITTED:
				case DTX_STATE_NOTIFYING_ABORT_NO_PREPARED:
				case DTX_STATE_CRASH_COMMITTED:
					break;
			}
		}

		/*
		 * If the error level is ERROR or more, errfinish is not going to
		 * return to caller; therefore, if there is any stacked error already
		 * in progress it will be lost.  This is more or less okay, except we
		 * do not want to have a FATAL or PANIC error downgraded because the
		 * reporting process was interrupted by a lower-grade error.  So check
		 * the stack and make sure we panic if panic is warranted.
		 */
		for (i = 0; i <= errordata_stack_depth; i++)
			elevel = Max(elevel, errordata[i].elevel);
	}

	/*
	 * Now decide whether we need to process this report at all; if it's
	 * warning or less and not enabled for logging, just return FALSE without
	 * starting up any error logging machinery.
	 */

	/* Determine whether message is enabled for server log output */
	if (IsPostmasterEnvironment)
		output_to_server = is_log_level_output(elevel, log_min_messages);
	else
	{
		/* In bootstrap/standalone case, do not sort LOG out-of-order */
		output_to_server = (elevel >= log_min_messages);
	}

	/* Determine whether message is enabled for client output */
	if (whereToSendOutput == DestRemote && elevel != COMMERROR)
	{
		/*
		 * client_min_messages is honored only after we complete the
		 * authentication handshake.  This is required both for security
		 * reasons and because many clients can't handle NOTICE messages
		 * during authentication.
		 */
		if (ClientAuthInProgress)
			output_to_client = (elevel >= ERROR);
		else
			output_to_client = (elevel >= client_min_messages ||
								elevel == INFO);
	}

	/* Skip processing effort if non-error message will not be output */
	if (elevel < ERROR && !output_to_server && !output_to_client)
		return false;

	/*
	 * Okay, crank up a stack entry to store the info in.
	 */

	if (recursion_depth++ > 0 && elevel >= ERROR)
	{
		/*
		 * Ooops, error during error processing.  Clear ErrorContext as
		 * discussed at top of file.  We will not return to the original
		 * error's reporter or handler, so we don't need it.
		 */
		MemoryContextReset(ErrorContext);

		/*
		 * Infinite error recursion might be due to something broken in a
		 * context traceback routine.  Abandon them too.  We also abandon
		 * attempting to print the error statement (which, if long, could
		 * itself be the source of the recursive failure).
		 */
		if (in_error_recursion_trouble())
		{
			error_context_stack = NULL;
			debug_query_string = NULL;

			/*
			 * If we recurse too many times, this could mean that we have
			 * serious out of memory problems. We bail out immediately here.
			 * See MPP-2440.
			 */
			if (recursion_depth > 2 * ERRORDATA_STACK_SIZE)
			{
				ImmediateInterruptOK = false;
				fflush(stdout);
				fflush(stderr);
				return false;
			}
		}
	}
	if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
	{
		/*
		 * Wups, stack not big enough.	We treat this as a PANIC condition
		 * because it suggests an infinite loop of errors during error
		 * recovery.
		 */
		errordata_stack_depth = -1;		/* make room on stack */
		ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
	}

	/* Initialize data for this error frame */
	edata = &errordata[errordata_stack_depth];
	MemSet(edata, 0, sizeof(ErrorData));
	edata->elevel = elevel;
	edata->output_to_server = output_to_server;
	edata->output_to_client = output_to_client;
	if (filename)
	{
		const char *slash;

		/* keep only base name, useful especially for vpath builds */
		slash = strrchr(filename, '/');
		if (slash)
			filename = slash + 1;
	}
	edata->filename = filename;
	edata->lineno = lineno;
	edata->funcname = funcname;
	/* the default text domain is the backend's */
	edata->domain = domain ? domain : PG_TEXTDOMAIN("postgres");
	edata->omit_location = true;
	/* Select default errcode based on elevel */
	if (elevel >= ERROR)
	{
		edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
		edata->omit_location = false;
	}
	else if (elevel == WARNING)
		edata->sqlerrcode = ERRCODE_WARNING;
	else
		edata->sqlerrcode = ERRCODE_SUCCESSFUL_COMPLETION;
	if (elevel > ERROR)
		edata->send_alert = true;  /* Send alerts for PANIC and FATAL errors */
	/* errno is saved here so that error parameter eval can't change it */
	edata->saved_errno = errno;

#ifndef WIN32
	edata->stacktracesize = backtrace(edata->stacktracearray, 30);
#else
	edata->stacktracesize = 0;
#endif

	recursion_depth--;
	return true;
}

/*
 * errfinish --- end an error-reporting cycle
 *
 * Produce the appropriate error report(s) and pop the error stack.
 *
 * If elevel is ERROR or worse, control does not return to the caller.
 * See elog.h for the error level definitions.
 */
void
errfinish(int dummy __attribute__((unused)),...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	int			elevel = edata->elevel;
	MemoryContext oldcontext;
	ErrorContextCallback *econtext;
	int			saved_errno;            /*CDB*/

	recursion_depth++;
	CHECK_STACK_DEPTH();
	saved_errno = edata->saved_errno;   /*CDB*/

	/*
	 * Do processing in ErrorContext, which we hope has enough reserved space
	 * to report an error.
	 */
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	/*
	 * Call any context callback functions.  Errors occurring in callback
	 * functions will be treated as recursive errors --- this ensures we will
	 * avoid infinite recursion (see errstart).
	 */
	for (econtext = error_context_stack;
		 econtext != NULL;
		 econtext = econtext->previous)
		(*econtext->callback) (econtext->arg);

	/*
	 * If ERROR (not more nor less) we pass it off to the current handler.
	 * Printing it and popping the stack is the responsibility of the handler.
	 */
	if (elevel == ERROR)
	{
		/*
		 * GP: While doing local error try/catch, do not reset all these
		 * important variables!
		 */
		/*
		 * We do some minimal cleanup before longjmp'ing so that handlers can
		 * execute in a reasonably sane state.
		 */

		/* This is just in case the error came while waiting for input */
		ImmediateInterruptOK = false;

		/*
		 * Reset InterruptHoldoffCount in case we ereport'd from inside an
		 * interrupt holdoff section.  (We assume here that no handler will
		 * itself be inside a holdoff section.	If necessary, such a handler
		 * could save and restore InterruptHoldoffCount for itself, but this
		 * should make life easier for most.)
		 */
		InterruptHoldoffCount = 0;

		CritSectionCount = 0;	/* should be unnecessary, but... */

		/*
		 * Note that we leave CurrentMemoryContext set to ErrorContext. The
		 * handler should reset it to something else soon.
		 */

		recursion_depth--;
		PG_RE_THROW();
	}

	/*
	 * If we are doing FATAL or PANIC, abort any old-style COPY OUT in
	 * progress, so that we can report the message before dying.  (Without
	 * this, pq_putmessage will refuse to send the message at all, which is
	 * what we want for NOTICE messages, but not for fatal exits.) This hack
	 * is necessary because of poor design of old-style copy protocol.	Note
	 * we must do this even if client is fool enough to have set
	 * client_min_messages above FATAL, so don't look at output_to_client.
	 */
	if (elevel >= FATAL && whereToSendOutput == DestRemote)
		pq_endcopyout(true);

	/* CDB: If fatal internal error, linger so user can attach a debugger. */
	if (elevel == FATAL &&
		edata->sqlerrcode == ERRCODE_INTERNAL_ERROR &&
		gp_debug_linger > 0)
		elog_debug_linger(edata);

	/* Emit the message to the right places */
	else
		EmitErrorReport();

    /*
     * CDB: Let caller take care of terminating the process, if requested.
     * Used by CdbProgramErrorHandler() to re-raise a signal such as SIGSEGV
     * in order to produce a core file.  We don't want to get involved in
     * platform dependent signal handling here, so let caller do it.
     */
    if (elevel == FATAL &&
        edata->fatal_return)
    {
        fflush(stdout);
        fflush(stderr);
        errno = saved_errno;
        return;
    }

	/* Now free up subsidiary data attached to stack entry, and release it */
	if (edata->message)
		pfree(edata->message);
	if (edata->detail)
		pfree(edata->detail);
	if (edata->detail_log)
		pfree(edata->detail_log);
	if (edata->hint)
		pfree(edata->hint);
	if (edata->context)
		pfree(edata->context);
	if (edata->internalquery)
		pfree(edata->internalquery);

	errordata_stack_depth--;

	/* Exit error-handling context */
	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;

	/*
	 * Perform error recovery action as specified by elevel.
	 */
	if (elevel == FATAL)
	{
		/*
		 * For a FATAL error, we let proc_exit clean up and exit.
		 */
		ImmediateInterruptOK = false;

		/*
		 * If we just reported a startup failure, the client will disconnect
		 * on receiving it, so don't send any more to the client.
		 */
		if (PG_exception_stack == NULL && whereToSendOutput == DestRemote)
			whereToSendOutput = DestNone;

		/*
		 * fflush here is just to improve the odds that we get to see the
		 * error message, in case things are so hosed that proc_exit crashes.
		 * Any other code you might be tempted to add here should probably be
		 * in an on_proc_exit or on_shmem_exit callback instead.
		 */
		fflush(stdout);
		fflush(stderr);

		/*
		 * Do normal process-exit cleanup, then return exit code 1 to indicate
		 * FATAL termination.  The postmaster may or may not consider this
		 * worthy of panic, depending on which subprocess returns it.
		 */
		proc_exit(1);
	}

	if (elevel >= PANIC)
	{
		/*
		 * Serious crash time. Postmaster will observe SIGABRT process exit
		 * status and kill the other backends too.
		 *
		 * XXX: what if we are *in* the postmaster?  abort() won't kill our
		 * children...
		 */
		ImmediateInterruptOK = false;
		fflush(stdout);
		fflush(stderr);
		abort();
	}

	/*
	 * We reach here if elevel <= WARNING. OK to return to caller.
	 *
	 * But check for cancel/die interrupt first --- this is so that the user
	 * can stop a query emitting tons of notice or warning messages, even if
	 * it's in a loop that otherwise fails to check for interrupts.
	 */
	CHECK_FOR_INTERRUPTS();

	errno = saved_errno;                /*CDB*/
}


/*
 * errcode --- add SQLSTATE error code to the current error
 *
 * The code is expected to be represented as per MAKE_SQLSTATE().
 */
int
errcode(int sqlerrcode)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->sqlerrcode = sqlerrcode;

	/* Indicate that we want stack traces etc for internal errors */
	if (sqlerrcode == ERRCODE_INTERNAL_ERROR)
		edata->omit_location = false;
	else
		edata->omit_location = true;

	return 0;					/* return value does not matter */
}


/*
 * errcode_for_file_access --- add SQLSTATE error code to the current error
 *
 * The SQLSTATE code is chosen based on the saved errno value.	We assume
 * that the failing operation was some type of disk file access.
 *
 * NOTE: the primary error message string should generally include %m
 * when this is used.
 */
int
errcode_for_file_access(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	switch (edata->saved_errno)
	{
			/* Permission-denied failures */
		case EPERM:				/* Not super-user */
		case EACCES:			/* Permission denied */
#ifdef EROFS
		case EROFS:				/* Read only file system */
#endif
			edata->sqlerrcode = ERRCODE_INSUFFICIENT_PRIVILEGE;
			break;

			/* File not found */
		case ENOENT:			/* No such file or directory */
			edata->sqlerrcode = ERRCODE_UNDEFINED_FILE;
			break;

			/* Duplicate file */
		case EEXIST:			/* File exists */
			edata->sqlerrcode = ERRCODE_DUPLICATE_FILE;
			break;

			/* Wrong object type or state */
		case ENOTDIR:			/* Not a directory */
		case EISDIR:			/* Is a directory */
#if defined(ENOTEMPTY) && (ENOTEMPTY != EEXIST) /* same code on AIX */
		case ENOTEMPTY: /* Directory not empty */
#endif
			edata->sqlerrcode = ERRCODE_WRONG_OBJECT_TYPE;
			break;

			/* Insufficient resources */
		case ENOSPC:			/* No space left on device */
			edata->sqlerrcode = ERRCODE_DISK_FULL;
			break;

		case ENFILE:			/* File table overflow */
		case EMFILE:			/* Too many open files */
			edata->sqlerrcode = ERRCODE_INSUFFICIENT_RESOURCES;
			break;

			/* Hardware failure */
		case EIO:				/* I/O error */
			edata->sqlerrcode = ERRCODE_IO_ERROR;
			break;

			/* All else is classified as internal errors */
		default:
			edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
			edata->omit_location = false;
			break;
	}

	return 0;					/* return value does not matter */
}

/*
 * errcode_for_socket_access --- add SQLSTATE error code to the current error
 *
 * The SQLSTATE code is chosen based on the saved errno value.	We assume
 * that the failing operation was some type of socket access.
 *
 * NOTE: the primary error message string should generally include %m
 * when this is used.
 */
int
errcode_for_socket_access(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	switch (edata->saved_errno)
	{
			/* Loss of connection */
		case EPIPE:
#ifdef ECONNRESET
		case ECONNRESET:
#endif
			edata->sqlerrcode = ERRCODE_CONNECTION_FAILURE;
			break;

			/* All else is classified as internal errors */
		default:
			edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
			edata->omit_location = false;
			break;
	}

	return 0;					/* return value does not matter */
}

/*
 * Convert compact error code (ERRCODE_xxx) to 5-char SQLSTATE string,
 * and put it into a 6-char buffer provided by caller.
 */
char *
errcode_to_sqlstate(int errcode, char outbuf[6])
{
	int	i;

	for (i = 0; i < 5; ++i)
	{
		outbuf[i] = PGUNSIXBIT(errcode);
		errcode >>= 6;
	}
	outbuf[5] = '\0';
	return &outbuf[5];
}

/*
 * Convert SQLSTATE string to compact error code (ERRCODE_xxx).
 */
int
sqlstate_to_errcode(const char *sqlstate)
{
	return MAKE_SQLSTATE(sqlstate[0], sqlstate[1], sqlstate[2],
						 sqlstate[3], sqlstate[4]);
}

/*
 * This macro handles expansion of a format string and associated parameters;
 * it's common code for errmsg(), errdetail(), etc.  Must be called inside
 * a routine that is declared like "const char *fmt, ..." and has an edata
 * pointer set up.	The message is assigned to edata->targetfield, or
 * appended to it if appendval is true.  The message is subject to translation
 * if translateit is true.
 *
 * Note: we pstrdup the buffer rather than just transferring its storage
 * to the edata field because the buffer might be considerably larger than
 * really necessary.
 */
#define EVALUATE_MESSAGE(targetfield, appendval, translateit)  \
	{ \
		char		   *fmtbuf; \
		StringInfoData	buf; \
		/* Internationalize the error format string */ \
		if (translateit && !in_error_recursion_trouble()) \
			fmt = dgettext(edata->domain, fmt); \
		/* Expand %m in format string */ \
		fmtbuf = expand_fmt_string(fmt, edata); \
		initStringInfo(&buf); \
		if ((appendval) && edata->targetfield) \
			appendStringInfo(&buf, "%s\n", edata->targetfield); \
		/* Generate actual output --- have to use appendStringInfoVA */ \
		for (;;) \
		{ \
			va_list		args; \
			bool		success; \
			va_start(args, fmt); \
			success = appendStringInfoVA(&buf, fmtbuf, args); \
			va_end(args); \
			if (success) \
				break; \
			enlargeStringInfo(&buf, buf.maxlen); \
		} \
		/* Done with expanded fmt */ \
		pfree(fmtbuf); \
		/* Save the completed message into the stack item */ \
		if (edata->targetfield) \
			pfree(edata->targetfield); \
		edata->targetfield = pstrdup(buf.data); \
		pfree(buf.data); \
	}

/*
 * Same as above, except for pluralized error messages.  The calling routine
 * must be declared like "const char *fmt_singular, const char *fmt_plural,
 * unsigned long n, ...".  Translation is assumed always wanted.
 */
#define EVALUATE_MESSAGE_PLURAL(targetfield, appendval)  \
	{ \
		const char	   *fmt; \
		char		   *fmtbuf; \
		StringInfoData	buf; \
		/* Internationalize the error format string */ \
		if (!in_error_recursion_trouble()) \
			fmt = dngettext(edata->domain, fmt_singular, fmt_plural, n); \
		else \
			fmt = (n == 1 ? fmt_singular : fmt_plural); \
		/* Expand %m in format string */ \
		fmtbuf = expand_fmt_string(fmt, edata); \
		initStringInfo(&buf); \
		if ((appendval) && edata->targetfield) \
			appendStringInfo(&buf, "%s\n", edata->targetfield); \
		/* Generate actual output --- have to use appendStringInfoVA */ \
		for (;;) \
		{ \
			va_list		args; \
			bool		success; \
			va_start(args, n); \
			success = appendStringInfoVA(&buf, fmtbuf, args); \
			va_end(args); \
			if (success) \
				break; \
			enlargeStringInfo(&buf, buf.maxlen); \
		} \
		/* Done with expanded fmt */ \
		pfree(fmtbuf); \
		/* Save the completed message into the stack item */ \
		if (edata->targetfield) \
			pfree(edata->targetfield); \
		edata->targetfield = pstrdup(buf.data); \
		pfree(buf.data); \
	}


/*
 * errmsg --- add a primary error message text to the current error
 *
 * In addition to the usual %-escapes recognized by printf, "%m" in
 * fmt is replaced by the error message for the caller's value of errno.
 *
 * Note: no newline is needed at the end of the fmt string, since
 * ereport will provide one for the output methods that need it.
 */
int
errmsg(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(message, false, true);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->message), strlen(edata->message));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errmsg_internal --- add a primary error message text to the current error
 *
 * This is exactly like errmsg() except that strings passed to errmsg_internal
 * are not translated, and are customarily left out of the
 * internationalization message dictionary.  This should be used for "can't
 * happen" cases that are probably not worth spending translation effort on.
 * We also use this for certain cases where we *must* not try to translate
 * the message because the translation would fail and result in infinite
 * error recursion.
 */
int
errmsg_internal(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(message, false, false);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->message), strlen(edata->message));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errmsg_plural --- add a primary error message text to the current error,
 * with support for pluralization of the message text
 */
int
errmsg_plural(const char *fmt_singular, const char *fmt_plural,
			  unsigned long n, ...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE_PLURAL(message, false);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->message), strlen(edata->message));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errdetail --- add a detail error message text to the current error
 */
int
errdetail(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(detail, false, true);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->detail), strlen(edata->detail));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errdetail_plural --- add a detail error message text to the current error,
 * with support for pluralization of the message text
 */
int
errdetail_plural(const char *fmt_singular, const char *fmt_plural,
				 unsigned long n, ...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE_PLURAL(detail, false);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->detail), strlen(edata->detail));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errdetail_log --- add a detail_log error message text to the current error
 */
int
errdetail_log(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(detail_log, false, true);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->detail_log), strlen(edata->detail_log));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errhint --- add a hint error message text to the current error
 */
int
errhint(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(hint, false, true);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->hint), strlen(edata->hint));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errcontext --- add a context error message text to the current error
 *
 * Unlike other cases, multiple calls are allowed to build up a stack of
 * context information.  We assume earlier calls represent more-closely-nested
 * states.
 */
int
errcontext(const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(context, true, true);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->context), strlen(edata->context));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}


/*
 * errhidestmt --- optionally suppress STATEMENT: field of log entry
 *
 * This should be called if the message text already includes the statement.
 */
int
errhidestmt(bool hide_stmt)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->hide_stmt = hide_stmt;

	return 0;					/* return value does not matter */
}


/*
 * errfunction --- add reporting function name to the current error
 *
 * This is used when backwards compatibility demands that the function
 * name appear in messages sent to old-protocol clients.  Note that the
 * passed string is expected to be a non-freeable constant string.
 */
int
errfunction(const char *funcname)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->funcname = funcname;
	edata->show_funcname = true;

	return 0;					/* return value does not matter */
}

/*
 * errposition --- add cursor position to the current error
 */
int
errposition(int cursorpos)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->cursorpos = cursorpos;

	return 0;					/* return value does not matter */
}

/*
 * errprintstack -- force print out stack trace
 */
int
errprintstack(bool printstack)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	edata->printstack = printstack;

	return 0;					/* return value does not matter */
}

/*
 * internalerrposition --- add internal cursor position to the current error
 */
int
internalerrposition(int cursorpos)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->internalpos = cursorpos;

	return 0;					/* return value does not matter */
}

/*
 * internalerrquery --- add internal query text to the current error
 *
 * Can also pass NULL to drop the internal query text entry.  This case
 * is intended for use in error callback subroutines that are editorializing
 * on the layout of the error report.
 */
int
internalerrquery(const char *query)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	if (edata->internalquery)
	{
		pfree(edata->internalquery);
		edata->internalquery = NULL;
	}

	if (query)
		edata->internalquery = MemoryContextStrdup(ErrorContext, query);

	errno = edata->saved_errno; /*CDB*/
	return 0;					/* return value does not matter */
}

/*
 * geterrcode --- return the currently set SQLSTATE error code
 *
 * This is only intended for use in error callback subroutines, since there
 * is no other place outside elog.c where the concept is meaningful.
 */
int
geterrcode(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	return edata->sqlerrcode;
}

/*
 * geterrposition --- return the currently set error position (0 if none)
 *
 * This is only intended for use in error callback subroutines, since there
 * is no other place outside elog.c where the concept is meaningful.
 */
int
geterrposition(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	return edata->cursorpos;
}

/*
 * getinternalerrposition --- same for internal error position
 *
 * This is only intended for use in error callback subroutines, since there
 * is no other place outside elog.c where the concept is meaningful.
 */
int
getinternalerrposition(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	return edata->internalpos;
}

/*
 * GPDB: errSendAlert -- set flag indicating the error should trigger an alert via e-mail or SNMP
 */
int
errSendAlert(bool sendAlert)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->send_alert = sendAlert;

	return 0;					/* return value does not matter */
}

/*
 * CDB: errFatalReturn -- set flag indicating errfinish() should return
 * to the caller instead of calling proc_exit() after reporting a FATAL
 * error.  Allows termination by re-raising a signal in order to obtain
 * a core dump.
 */
int
errFatalReturn(bool fatalReturn)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];

	/* we don't bother incrementing recursion_depth */
	CHECK_STACK_DEPTH();

	edata->fatal_return = fatalReturn;

	return 0;					/* return value does not matter */
}


/*
 * elog_start --- startup for old-style API
 *
 * All that we do here is stash the hidden filename/lineno/funcname
 * arguments into a stack entry.
 *
 * We need this to be separate from elog_finish because there's no other
 * portable way to deal with inserting extra arguments into the elog call.
 * (If macros with variable numbers of arguments were portable, it'd be
 * easy, but they aren't.)
 */
void
elog_start(const char *filename, int lineno, const char *funcname)
{
	ErrorData  *edata;

	if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
	{
		/*
		 * Wups, stack not big enough.	We treat this as a PANIC condition
		 * because it suggests an infinite loop of errors during error
		 * recovery.  Note that the message is intentionally not localized,
		 * else failure to convert it to client encoding could cause further
		 * recursion.
		 */
		errordata_stack_depth = -1;		/* make room on stack */
		ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
	}

	edata = &errordata[errordata_stack_depth];
	if (filename)
	{
		const char *slash;

		/* keep only base name, useful especially for vpath builds */
		slash = strrchr(filename, '/');
		if (slash)
			filename = slash + 1;
	}
	edata->filename = filename;
	edata->lineno = lineno;
	edata->funcname = funcname;
	/* errno is saved now so that error parameter eval can't change it */
	edata->saved_errno = errno;
#ifdef USE_ASSERT_CHECKING
	if (IsUnderPostmaster && mainthread() != 0 && !pthread_equal(main_tid, pthread_self()))
	{
#if defined(__darwin__)
		write_log("elog called from thread (OS-X pthread_sigmask is broken: MPP-4923)\n");
#else
		write_log("elog called from thread (%s:%d)\n", filename, lineno);
#endif
	}
#endif
}

/*
 * elog_finish --- finish up for old-style API
 */
void
elog_finish(int elevel, const char *fmt,...)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	CHECK_STACK_DEPTH();

	/*
	 * Do errstart() to see if we actually want to report the message.
	 */
	errordata_stack_depth--;
	errno = edata->saved_errno;
	if (!errstart(elevel, edata->filename, edata->lineno, edata->funcname, NULL))
		return;					/* nothing to do */

	/*
	 * Format error message just like errmsg_internal().
	 */
	recursion_depth++;
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	EVALUATE_MESSAGE(message, false, false);

	/* enforce correct encoding */
	verify_and_replace_mbstr(&(edata->message), strlen(edata->message));

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;

	/*
	 * And let errfinish() finish up.
	 */
	errfinish(0);
}

/*
 * Actual output of the top-of-stack error message
 *
 * In the ereport(ERROR) case this is called from PostgresMain (or not at all,
 * if the error is caught by somebody).  For all other severity levels this
 * is called by errfinish.
 */
void
EmitErrorReport(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	MemoryContext oldcontext;

	recursion_depth++;
	CHECK_STACK_DEPTH();
	oldcontext = MemoryContextSwitchTo(ErrorContext);

	/* CDB: Tidy up the message */
	/*
	 * TODO Why do we want to do this?  it seems pointless
	 * and makes the error messages harder to read.
	 */
	if (edata->output_to_server ||
		edata->output_to_client)
		cdb_tidy_message(edata);

	/* Send to server log, if enabled */
	if (edata->output_to_server)
		send_message_to_server_log(edata);

	/* Send to client, if enabled */
	if (edata->output_to_client)
		send_message_to_frontend(edata);

	MemoryContextSwitchTo(oldcontext);
	recursion_depth--;
}

/*
 * CopyErrorData --- obtain a copy of the topmost error stack entry
 *
 * This is only for use in error handler code.	The data is copied into the
 * current memory context, so callers should always switch away from
 * ErrorContext first; otherwise it will be lost when FlushErrorState is done.
 */
ErrorData *
CopyErrorData(void)
{
	ErrorData  *edata = &errordata[errordata_stack_depth];
	ErrorData  *newedata;

	/*
	 * we don't increment recursion_depth because out-of-memory here does not
	 * indicate a problem within the error subsystem.
	 */
	CHECK_STACK_DEPTH();

	Assert(CurrentMemoryContext != ErrorContext);

	/* Copy the struct itself */
	newedata = (ErrorData *) palloc(sizeof(ErrorData));
	memcpy(newedata, edata, sizeof(ErrorData));

	/* Make copies of separately-allocated fields */
	if (newedata->message)
		newedata->message = pstrdup(newedata->message);
	if (newedata->detail)
		newedata->detail = pstrdup(newedata->detail);
	if (newedata->detail_log)
		newedata->detail_log = pstrdup(newedata->detail_log);
	if (newedata->hint)
		newedata->hint = pstrdup(newedata->hint);
	if (newedata->context)
		newedata->context = pstrdup(newedata->context);
	if (newedata->internalquery)
		newedata->internalquery = pstrdup(newedata->internalquery);

	return newedata;
}

/*
 * FreeErrorData --- free the structure returned by CopyErrorData.
 *
 * Error handlers should use this in preference to assuming they know all
 * the separately-allocated fields.
 */
void
FreeErrorData(ErrorData *edata)
{
	if (edata->message)
		pfree(edata->message);
	if (edata->detail)
		pfree(edata->detail);
	if (edata->detail_log)
		pfree(edata->detail_log);
	if (edata->hint)
		pfree(edata->hint);
	if (edata->context)
		pfree(edata->context);
	if (edata->internalquery)
		pfree(edata->internalquery);
	pfree(edata);
}

/*
 * FlushErrorState --- flush the error state after error recovery
 *
 * This should be called by an error handler after it's done processing
 * the error; or as soon as it's done CopyErrorData, if it intends to
 * do stuff that is likely to provoke another error.  You are not "out" of
 * the error subsystem until you have done this.
 */
void
FlushErrorState(void)
{
	/*
	 * Reset stack to empty.  The only case where it would be more than one
	 * deep is if we serviced an error that interrupted construction of
	 * another message.  We assume control escaped out of that message
	 * construction and won't ever go back.
	 */
	errordata_stack_depth = -1;
	recursion_depth = 0;
	/* Delete all data in ErrorContext */
	MemoryContextResetAndDeleteChildren(ErrorContext);
}

/*
 * ReThrowError --- re-throw a previously copied error
 *
 * A handler can do CopyErrorData/FlushErrorState to get out of the error
 * subsystem, then do some processing, and finally ReThrowError to re-throw
 * the original error.	This is slower than just PG_RE_THROW() but should
 * be used if the "some processing" is likely to incur another error.
 */
void
ReThrowError(ErrorData *edata)
{
	ErrorData  *newedata;

	Assert(edata->elevel <= ERROR); /* CDB: Ok to rethrow elog_demote'd error */

	/* Push the data back into the error context */
	recursion_depth++;
	MemoryContextSwitchTo(ErrorContext);

	if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
	{
		/*
		 * Wups, stack not big enough.	We treat this as a PANIC condition
		 * because it suggests an infinite loop of errors during error
		 * recovery.  Note that the message is intentionally not localized,
		 * else failure to convert it to client encoding could cause further
		 * recursion.
		 */
		errordata_stack_depth = -1;		/* make room on stack */
		ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
	}

	newedata = &errordata[errordata_stack_depth];
	memcpy(newedata, edata, sizeof(ErrorData));

	/* Make copies of separately-allocated fields */
	if (newedata->message)
		newedata->message = pstrdup(newedata->message);
	if (newedata->detail)
		newedata->detail = pstrdup(newedata->detail);
	if (newedata->detail_log)
		newedata->detail_log = pstrdup(newedata->detail_log);
	if (newedata->hint)
		newedata->hint = pstrdup(newedata->hint);
	if (newedata->context)
		newedata->context = pstrdup(newedata->context);
	if (newedata->internalquery)
		newedata->internalquery = pstrdup(newedata->internalquery);

	recursion_depth--;
	PG_RE_THROW();
}

/*
 * pg_re_throw --- out-of-line implementation of PG_RE_THROW() macro
 */
void
pg_re_throw(void)
{
	/* If possible, throw the error to the next outer setjmp handler */
	if (PG_exception_stack != NULL)
		siglongjmp(*PG_exception_stack, 1);
	else
	{
		/*
		 * If we get here, elog(ERROR) was thrown inside a PG_TRY block, which
		 * we have now exited only to discover that there is no outer setjmp
		 * handler to pass the error to.  Had the error been thrown outside
		 * the block to begin with, we'd have promoted the error to FATAL, so
		 * the correct behavior is to make it FATAL now; that is, emit it and
		 * then call proc_exit.
		 */
		ErrorData  *edata = &errordata[errordata_stack_depth];

		Assert(errordata_stack_depth >= 0);
		Assert(edata->elevel == ERROR);
		edata->elevel = FATAL;

		/*
		 * At least in principle, the increase in severity could have changed
		 * where-to-output decisions, so recalculate.  This should stay in
		 * sync with errstart(), which see for comments.
		 */
		if (IsPostmasterEnvironment)
			edata->output_to_server = is_log_level_output(FATAL,
														  log_min_messages);
		else
			edata->output_to_server = (FATAL >= log_min_messages);
		if (whereToSendOutput == DestRemote)
		{
			if (ClientAuthInProgress)
				edata->output_to_client = true;
			else
				edata->output_to_client = (FATAL >= client_min_messages);
		}

		/*
		 * We can use errfinish() for the rest, but we don't want it to call
		 * any error context routines a second time.  Since we know we are
		 * about to exit, it should be OK to just clear the context stack.
		 */
		error_context_stack = NULL;

		errfinish(0);
	}

	/* We mustn't return... */
	ExceptionalCondition("pg_re_throw tried to return", "FailedAssertion",
						 __FILE__, __LINE__);

	/*
	 * Since ExceptionalCondition isn't declared noreturn because of
	 * TrapMacro(), we need this to keep gcc from complaining.
	 */
	abort();
}


/*
 * CDB: elog_demote
 *
 * A PG_CATCH() handler can call this to downgrade the error that it is
 * currently handling to a level lower than ERROR.  The caller should
 * then do PG_RE_THROW() to proceed to the next error handler.
 *
 * Clients using libpq cannot receive normal output together with an error.
 * The libpq frontend discards any results already buffered when a command
 * completes with an error notification of level ERROR or higher.
 *
 * elog_demote() can be used to reduce the error level reported to the client
 * so that libpq won't suppress normal output, while the backend still frees
 * resources, aborts the transaction, etc, as usual.
 *
 * Returns true if successful, false if the request is disallowed.
 */
bool
elog_demote(int downgrade_to_elevel)
{
	ErrorData  *edata;

	if (errordata_stack_depth < 0 ||
		errordata_stack_depth >= ERRORDATA_STACK_SIZE - 1)
		return false;

	edata = &errordata[errordata_stack_depth];

	if (downgrade_to_elevel >= ERROR ||
		recursion_depth != 0 ||
		edata->elevel > ERROR ||
		edata->elevel < downgrade_to_elevel)
		return false;

	edata->elevel = downgrade_to_elevel;
	return true;
}							   /* elog_demote */


/*
 * CDB: elog_dismiss
 *
 * A PG_CATCH() handler can call this to downgrade the error that it is
 * currently handling to a level lower than ERROR, report it to the log
 * and/or client as appropriate, and purge it from the error system.
 *
 * This shouldn't be attempted unless the caller is certain that the
 * error does not need the services of upper level error handlers to
 * release resources, abort the transaction, etc.
 *
 * Returns true if successful, in which case the error has been expunged
 * and the caller should not do PG_RE_THROW(), but should instead fall or
 * jump out of the PG_CATCH() handler and resume normal execution.
 *
 * Returns false if unsuccessful; then the caller should carry on as
 * PG_CATCH() handlers ordinarily do, and exit via PG_RE_THROW().
 */
bool
elog_dismiss(int downgrade_to_elevel)
{
	ErrorContextCallback   *saveCallbackStack = error_context_stack;
	ErrorData			   *edata;
	bool					shouldEmit = false;

	if (errordata_stack_depth < 0 ||
		errordata_stack_depth >= ERRORDATA_STACK_SIZE - 1)
		return false;

	edata = &errordata[errordata_stack_depth];

	if (downgrade_to_elevel >= ERROR ||
		recursion_depth != 0 ||
		edata->elevel > ERROR)
		return false;

	/*
	 * Context callbacks, if any, were already invoked when this error
	 * first passed through errfinish.  Hide them so they won't be
	 * called redundantly.
	 */
	error_context_stack = NULL;

	/* Use errstart to decide where to send the error report. */
	shouldEmit = errstart(downgrade_to_elevel, NULL, 0, NULL, TEXTDOMAIN);

	/* Send error report to log and/or client. */
	if (shouldEmit)
	{
		ErrorData  *newedata = &errordata[errordata_stack_depth];

		/* errstart has stacked a new ErrorData entry. */
		Insist(newedata == edata + 1);

		/* It tells us where to send the error report for the new elevel. */
		edata->elevel = newedata->elevel;
		edata->output_to_client = newedata->output_to_client;
		edata->output_to_server = newedata->output_to_server;

		/* Pop temp ErrorData entry. Nothing was palloc'ed; no need to pfree. */
		errordata_stack_depth--;
	}

	/* Nobody wants the error report. */
	else
	{
		edata->elevel = downgrade_to_elevel;
		edata->output_to_client = false;
		edata->output_to_server = false;
	}

	/*
	 * Sneak the caller's error through errfinish again (it has been through
	 * once already) to emit the error report (if requested) and clean up.
	 */
	errfinish(0);

	/* Restore the context callback stack. */
	error_context_stack = saveCallbackStack;

	/* Error not pending anymore, so caller should not do PG_RE_THROW(). */
	return true;				/* success */
}							   /* elog_dismiss */


/*
 * CDB: elog_geterrcode
 * Return the SQLSTATE code for the error currently being handled, or 0.
 *
 * This is only intended for use in error handlers.
 */
int
elog_geterrcode(void)
{
	return (errordata_stack_depth < 0)
				? 0
				: errordata[errordata_stack_depth].sqlerrcode;
} /* elog_geterrcode */

int
elog_getelevel(void)
{
	return (errordata_stack_depth < 0)
				? NOTICE
				: errordata[errordata_stack_depth].elevel;
} /* elog_getelevel */

/*
 * Note: A pointer is returned.  Make a copy of the message
 * before re-throwing or flushing the error state.
 */
char*
elog_message(void)
{
	return (errordata_stack_depth < 0)
				? NULL
				: errordata[errordata_stack_depth].message;
}

/*
 * Initialization of error output file
 */
void
DebugFileOpen(void)
{
	int			fd,
				istty;

	if (OutputFileName[0])
	{
		/*
		 * A debug-output file name was given.
		 *
		 * Make sure we can write the file, and find out if it's a tty.
		 */
		if ((fd = open(OutputFileName, O_CREAT | O_APPEND | O_WRONLY,
					   0666)) < 0)
			ereport(FATAL,
					(errcode_for_file_access(),
				  errmsg("could not open file \"%s\": %m", OutputFileName)));
		istty = isatty(fd);
		close(fd);

		/*
		 * Redirect our stderr to the debug output file.
		 */
		if (!freopen(OutputFileName, "a", stderr))
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not reopen file \"%s\" as stderr: %m",
							OutputFileName)));

		/*
		 * If the file is a tty and we're running under the postmaster, try to
		 * send stdout there as well (if it isn't a tty then stderr will block
		 * out stdout, so we may as well let stdout go wherever it was going
		 * before).
		 */
		if (istty && IsUnderPostmaster)
			if (!freopen(OutputFileName, "a", stdout))
				ereport(FATAL,
						(errcode_for_file_access(),
						 errmsg("could not reopen file \"%s\" as stdout: %m",
								OutputFileName)));
	}
}


#ifdef HAVE_SYSLOG

/*
 * Set or update the parameters for syslog logging
 */
void
set_syslog_parameters(const char *ident, int facility)
{
	/*
	 * guc.c is likely to call us repeatedly with same parameters, so don't
	 * thrash the syslog connection unnecessarily.	Also, we do not re-open
	 * the connection until needed, since this routine will get called whether
	 * or not Log_destination actually mentions syslog.
	 *
	 * Note that we make our own copy of the ident string rather than relying
	 * on guc.c's.  This may be overly paranoid, but it ensures that we cannot
	 * accidentally free a string that syslog is still using.
	 */
	if (syslog_ident == NULL || strcmp(syslog_ident, ident) != 0 ||
		syslog_facility != facility)
	{
		if (openlog_done)
		{
			closelog();
			openlog_done = false;
		}
		if (syslog_ident)
			free(syslog_ident);
		syslog_ident = strdup(ident);
		/* if the strdup fails, we will cope in write_syslog() */
		syslog_facility = facility;
	}
}


/*
 * Write a message line to syslog
 */
static void
write_syslog(int level, const char *line)
{
	static unsigned long seq = 0;

	int			len;
	const char *nlpos;

	/* Open syslog connection if not done yet */
	if (!openlog_done)
	{
		openlog(syslog_ident ? syslog_ident : "postgres",
				LOG_PID | LOG_NDELAY | LOG_NOWAIT,
				syslog_facility);
		openlog_done = true;
	}

	/*
	 * We add a sequence number to each log message to suppress "same"
	 * messages.
	 */
	seq++;

	/*
	 * Our problem here is that many syslog implementations don't handle long
	 * messages in an acceptable manner. While this function doesn't help that
	 * fact, it does work around by splitting up messages into smaller pieces.
	 *
	 * We divide into multiple syslog() calls if message is too long or if the
	 * message contains embedded newline(s).
	 */
	len = strlen(line);
	nlpos = strchr(line, '\n');
	if (len > PG_SYSLOG_LIMIT || nlpos != NULL)
	{
		int			chunk_nr = 0;

		while (len > 0)
		{
			char		buf[PG_SYSLOG_LIMIT + 1];
			int			buflen;
			int			i;

			/* if we start at a newline, move ahead one char */
			if (line[0] == '\n')
			{
				line++;
				len--;
				/* we need to recompute the next newline's position, too */
				nlpos = strchr(line, '\n');
				continue;
			}

			/* copy one line, or as much as will fit, to buf */
			if (nlpos != NULL)
				buflen = nlpos - line;
			else
				buflen = len;
			buflen = Min(buflen, PG_SYSLOG_LIMIT);
			memcpy(buf, line, buflen);
			buf[buflen] = '\0';

			/* trim to multibyte letter boundary */
			buflen = pg_mbcliplen(buf, buflen, buflen);
			if (buflen <= 0)
				return;
			buf[buflen] = '\0';

			/* already word boundary? */
			if (line[buflen] != '\0' &&
				!isspace((unsigned char) line[buflen]))
			{
				/* try to divide at word boundary */
				i = buflen - 1;
				while (i > 0 && !isspace((unsigned char) buf[i]))
					i--;

				if (i > 0)		/* else couldn't divide word boundary */
				{
					buflen = i;
					buf[i] = '\0';
				}
			}

			chunk_nr++;

			syslog(level, "[%lu-%d] %s", seq, chunk_nr, buf);
			line += buflen;
			len -= buflen;
		}
	}
	else
	{
		/* message short enough */
		syslog(level, "[%lu] %s", seq, line);
	}
}
#endif   /* HAVE_SYSLOG */

#ifdef WIN32
/*
 * Write a message line to the windows event log
 */
static void
write_eventlog(int level, const char *line)
{
	int			eventlevel = EVENTLOG_ERROR_TYPE;
	static HANDLE evtHandle = INVALID_HANDLE_VALUE;

	if (evtHandle == INVALID_HANDLE_VALUE)
	{
		evtHandle = RegisterEventSource(NULL, "PostgreSQL");
		if (evtHandle == NULL)
		{
			evtHandle = INVALID_HANDLE_VALUE;
			return;
		}
	}

	switch (level)
	{
		case DEBUG5:
		case DEBUG4:
		case DEBUG3:
		case DEBUG2:
		case DEBUG1:
		case LOG:
		case COMMERROR:
		case INFO:
		case NOTICE:
			eventlevel = EVENTLOG_INFORMATION_TYPE;
			break;
		case WARNING:
			eventlevel = EVENTLOG_WARNING_TYPE;
			break;
		case ERROR:
		case FATAL:
		case PANIC:
		default:
			eventlevel = EVENTLOG_ERROR_TYPE;
			break;
	}


	ReportEvent(evtHandle,
				eventlevel,
				0,
				0,				/* All events are Id 0 */
				NULL,
				1,
				0,
				&line,
				NULL);
}
#endif   /* WIN32 */


/*
 * CDB: Tidy up the error message
 */

static void
cdb_strip_trailing_whitespace(char **buf)
{
	if (*buf)
	{
		char   *bp = *buf;
		char   *ep = bp + strlen(bp);

		while (bp < ep &&
			   ep[-1] <= ' ' &&
			   ep[-1] > '\0')
			*--ep = '\0';

		if (bp == ep)
		{
			pfree(*buf);
			*buf = NULL;
		}
	}
}							   /* cdb_strip_trailing_whitespace */

void
cdb_tidy_message(ErrorData *edata)
{
	char	   *bp;
	char	   *cp;
	char	   *ep;
	char	   *tp;
	int			m, n;

	cdb_strip_trailing_whitespace(&edata->hint);
	cdb_strip_trailing_whitespace(&edata->detail);
	cdb_strip_trailing_whitespace(&edata->detail_log);
	cdb_strip_trailing_whitespace(&edata->message);

	/* Look at main error message. */
	if (edata->message)
	{
		bp = edata->message;
		while (*bp <= ' ' &&
			   *bp > '\0')
			bp++;
		ep = bp + strlen(bp);
	}
	else
		ep = bp = "";

	/*
	 * If more than one line, move lines after the first to errdetail.
	 * Make an exception for LOG messages because statement logging would
	 * be uglified.  Skip DEBUG messages too, 'cause users don't see 'em.
	 */
	if (edata->elevel > LOG &&
		0 != (cp = strchr(bp, '\n')))
	{
		char   *dp = cp;

		/* If just one extra line, strip its leading '\n' and whitespace. */
		if (!strchr(dp+1, '\n'))
		{
			while (*dp <= ' ' &&
				   *dp > '\0')
				dp++;
		}

		/* Insert in front of detail message. */
		if (!edata->detail)
			edata->detail = pstrdup(dp);
		else
		{
			m = ep - dp;
			n = strlen(edata->detail) + 1;
			tp = palloc(m + 1 + n);
			memcpy(tp, dp, m);
			tp[m] = '\n';
			memcpy(tp + m + 1, edata->detail, n);

			pfree(edata->detail);
			edata->detail = tp;
		}

		/* Drop from main message. */
		ep = cp;
		while (bp < ep &&
			   ep[-1] <= ' ' &&
			   ep[-1] > '\0')
			ep--;
		*ep = '\0';
	}

	/*
	 * If internal error, append the filename and line number.
	 * (Skip if error came from QE, because QE already added the info.)
	 */
	if (!edata->omit_location &&
		edata->sqlerrcode == ERRCODE_INTERNAL_ERROR &&
		edata->filename)
	{
		char		buf[60];
		const char *bfn;

		/* With some compilers __FILE__ is absolute path.  Strip directory. */
		bfn = edata->filename + strlen(edata->filename);
		while (edata->filename < bfn &&
			   bfn[-1] != '/' &&
			   bfn[-1] != '\\')
			bfn--;

		/* Format the error location. */
		n = snprintf(buf, sizeof(buf)-1, " (%s:%d)", bfn, edata->lineno);

		/* Append to main error message. */
		m = ep - bp;
		tp = palloc(m + n + 1);
		memcpy(tp, bp, m);
		memcpy(tp+m, buf, n);
		tp[m+n] = '\0';

		if (edata->message)
			pfree(edata->message);
		edata->message = tp;
	}
}							   /* cdb_tidy_message */

/*
 * setup formatted_log_time, for consistent times between CSV and regular logs
 */
static void
setup_formatted_log_time(void)
{
	struct timeval tv;
	pg_time_t	stamp_time;
	pg_tz	   *tz;
	char		msbuf[8];

	gettimeofday(&tv, NULL);
	stamp_time = (pg_time_t) tv.tv_sec;

	/*
	 * Normally we print log timestamps in log_timezone, but during startup we
	 * could get here before that's set. If so, fall back to gmt_timezone
	 * (which guc.c ensures is set up before Log_line_prefix can become
	 * nonempty).
	 */
	tz = log_timezone ? log_timezone : gmt_timezone;

	pg_strftime(formatted_log_time, FORMATTED_TS_LEN,
				/* leave room for microseconds... */
				"%Y-%m-%d %H:%M:%S        %Z",
				pg_localtime(&stamp_time, tz));

	/* 'paste' microseconds into place... */
	sprintf(msbuf, ".%06d", (int) (tv.tv_usec));
	strncpy(formatted_log_time + 19, msbuf, 4);
}

/*
 * setup formatted_start_time
 */
static void
setup_formatted_start_time(void)
{
	pg_time_t	stamp_time = (pg_time_t) MyStartTime;
	pg_tz	   *tz;

	/*
	 * Normally we print log timestamps in log_timezone, but during startup we
	 * could get here before that's set. If so, fall back to gmt_timezone
	 * (which guc.c ensures is set up before Log_line_prefix can become
	 * nonempty).
	 */
	tz = log_timezone ? log_timezone : gmt_timezone;

	pg_strftime(formatted_start_time, FORMATTED_TS_LEN,
				"%Y-%m-%d %H:%M:%S %Z",
				pg_localtime(&stamp_time, tz));
}

/*
 * Format tag info for log lines; append to the provided buffer.
 */
static void
log_line_prefix(StringInfo buf)
{
	/* static counter for line numbers */
	static long log_line_number = 0;

	/* has counter been reset in current process? */
	static int	log_my_pid = 0;

	int			format_len;
	int			i;
	int			j;

	/*
	 * This is one of the few places where we'd rather not inherit a static
	 * variable's value from the postmaster.  But since we will, reset it when
	 * MyProcPid changes. MyStartTime also changes when MyProcPid does, so
	 * reset the formatted start timestamp too.
	 */
	if (log_my_pid != MyProcPid)
	{
		log_line_number = 0;
		log_my_pid = MyProcPid;
		formatted_start_time[0] = '\0';
	}
	log_line_number++;

	if (Log_line_prefix == NULL)
		return;					/* in case guc hasn't run yet */

	format_len = strlen(Log_line_prefix);

	for (i = 0; i < format_len; i++)
	{
		if (Log_line_prefix[i] != '%')
		{
			/* literal char, just copy */
			appendStringInfoChar(buf, Log_line_prefix[i]);
			continue;
		}
		/* go to char after '%' */
		i++;
		if (i >= format_len)
			break;				/* format error - ignore it */

		/* process the option */
		switch (Log_line_prefix[i])
		{
			case 'u':
				if (MyProcPort)
				{
					const char *username = MyProcPort->user_name;

					if (username == NULL || *username == '\0')
						username = _("[unknown]");
					appendStringInfo(buf, "%s", username);
				}
				break;
			case 'd':
				if (MyProcPort)
				{
					const char *dbname = MyProcPort->database_name;

					if (dbname == NULL || *dbname == '\0')
						dbname = _("[unknown]");
					appendStringInfo(buf, "%s", dbname);
				}
				break;
			case 'c':
				appendStringInfo(buf, "%lx.%x", (long) (MyStartTime), MyProcPid);
				break;
			case 'p':
				appendStringInfo(buf, "%d", MyProcPid);
				break;
			case 'l':
				appendStringInfo(buf, "%ld", log_line_number);
				break;
			case 'm':
				setup_formatted_log_time();
				appendStringInfoString(buf, formatted_log_time);
				break;
			case 't':
				{
					pg_time_t	stamp_time = (pg_time_t) time(NULL);
					pg_tz	   *tz;
					char		strfbuf[128];

					tz = log_timezone ? log_timezone : gmt_timezone;

					pg_strftime(strfbuf, sizeof(strfbuf),
								"%Y-%m-%d %H:%M:%S %Z",
								pg_localtime(&stamp_time, tz));
					appendStringInfoString(buf, strfbuf);
				}
				break;
			case 's':
				if (formatted_start_time[0] == '\0')
					setup_formatted_start_time();
				appendStringInfoString(buf, formatted_start_time);
				break;
			case 'i':
				if (MyProcPort)
				{
					const char *psdisp;
					int			displen;

					psdisp = get_ps_display(&displen);
					appendBinaryStringInfo(buf, psdisp, displen);
				}
				break;
			case 'r':
				if (MyProcPort && MyProcPort->remote_host)
				{
					appendStringInfo(buf, "%s", MyProcPort->remote_host);
					if (MyProcPort->remote_port &&
						MyProcPort->remote_port[0] != '\0')
						appendStringInfo(buf, "(%s)",
										 MyProcPort->remote_port);
				}
				break;
			case 'h':
				if (MyProcPort && MyProcPort->remote_host)
					appendStringInfo(buf, "%s", MyProcPort->remote_host);
				break;
			case 'q':
				/* in postmaster and friends, stop if %q is seen */
				/* in a backend, just ignore */
				if (MyProcPort == NULL)
					i = format_len;
				break;
			case 'v':
				/* keep VXID format in sync with lockfuncs.c */
				if (MyProc != NULL && MyProc->backendId != InvalidBackendId)
					appendStringInfo(buf, "%d/%u",
									 MyProc->backendId, MyProc->lxid);
				break;
			case 'x':
				appendStringInfo(buf, "%u", GetTopTransactionIdIfAny());
				break;

			/* MPP SPECIFIC OPTIONS. */
			case 'C':
				/* we use -2 to indicate that it hasn't been set yet.  we'll
				 * choose to not write anything for the very early log messages
				 * before GUC variables are set.
				 */
				if( Gp_segment != UNDEF_SEGMENT )
					appendStringInfo(buf, "%d", Gp_segment );
				break;
			case 'I':
				/* prints a succinct description of an MPP process. */
				if (!MyProcPort)
				{
					const char *sp;
					const char *uname = get_ps_display_username();
					if (!uname || !uname[0])
						appendStringInfoString(buf, "postmaster");
					else if ((sp = strstr(uname, " process")) != NULL)
						appendBinaryStringInfo(buf, uname, sp - uname);
					else
						appendStringInfoString(buf, uname);
					break;
				}
				j = buf->len;
				if (gp_session_id > 0)
					appendStringInfo(buf, "con%d ", gp_session_id);
				if (gp_command_count > 0)
					appendStringInfo(buf, "cmd%d ", gp_command_count);
				if (Gp_role == GP_ROLE_EXECUTE)
					appendStringInfo(buf, "seg%d ", Gp_segment);
				if (currentSliceId > 0)
					appendStringInfo(buf, "slice%d ", currentSliceId);
				if (j < buf->len &&
					buf->data[buf->len - 1] == ' ')
					buf->len--;
				break;
			case 'P':
				if( Gp_role == GP_ROLE_EXECUTE )
				{
					appendStringInfoChar(buf, 'P');
				}
				break;
			case 'R':
				if (!MyProcPort)
				{
					const char *uname = get_ps_display_username();
					appendStringInfoString(buf, uname && uname[0] ? uname : "postmaster");
				}
				else
					appendStringInfoString(buf, role_to_string(Gp_role));
				break;
			case 'S':
				if (currentSliceId >= 0)
				{
					appendStringInfo(buf, "%d", currentSliceId );
				}
				break;
			case 'T':
				if (MyProcPort)
				{
					if (Gp_role == GP_ROLE_EXECUTE)
						appendStringInfo(buf, "qe");
					else if (Gp_role == GP_ROLE_DISPATCH)
						appendStringInfo(buf, "qd");
				}
				break;
			case 'X':
				{
					DistributedTransactionId distribXid;
					TransactionId localXid;
					TransactionId subXid;

					GetAllTransactionXids(
									&distribXid,
									&localXid,
									&subXid);

					if (localXid != InvalidTransactionId)
					{
						if (distribXid >= FirstDistributedTransactionId)
							appendStringInfo(buf, "dx%u, ", distribXid);

						appendStringInfo(buf, "x%u", localXid);

						if (subXid >= FirstNormalTransactionId)
							appendStringInfo(buf, ", sx%u, ", subXid);
					}
				}
				break;
			case '%':
				appendStringInfoChar(buf, '%');
				break;
			default:
				/* format error - ignore it */
				break;
		}
	}
}

/*
 * append a CSV'd version of a string to a StringInfo
 * We use the PostgreSQL defaults for CSV, i.e. quote = escape = '"'
 * If it's NULL, append nothing.
 */
static inline void
appendCSVLiteral(StringInfo buf, const char *data)
{
	const char *p = data;
	char		c;

	/* avoid confusing an empty string with NULL */
	if (p == NULL)
		return;

	appendStringInfoCharMacro(buf, '"');
	while ((c = *p++) != '\0')
	{
		if (c == '"')
			appendStringInfoCharMacro(buf, '"');
		appendStringInfoCharMacro(buf, c);
	}
	appendStringInfoCharMacro(buf, '"');
}

/*
 * Constructs the error message, depending on the Errordata it gets, in a CSV
 * format which is described in doc/src/sgml/config.sgml.
 */
static void
write_csvlog(ErrorData *edata)
{
	StringInfoData buf;
	bool		print_stmt = false;

	/* static counter for line numbers */
	static long log_line_number = 0;

	/* has counter been reset in current process? */
	static int	log_my_pid = 0;

	/*
	 * This is one of the few places where we'd rather not inherit a static
	 * variable's value from the postmaster.  But since we will, reset it when
	 * MyProcPid changes.
	 */
	if (log_my_pid != MyProcPid)
	{
		log_line_number = 0;
		log_my_pid = MyProcPid;
		formatted_start_time[0] = '\0';
	}
	log_line_number++;

	initStringInfo(&buf);

	/*
	 * timestamp with milliseconds
	 *
	 * Check if the timestamp is already calculated for the syslog message,
	 * and use it if so.  Otherwise, get the current timestamp.  This is done
	 * to put same timestamp in both syslog and csvlog messages.
	 */
	if (formatted_log_time[0] == '\0')
		setup_formatted_log_time();

	appendStringInfoString(&buf, formatted_log_time);
	appendStringInfoChar(&buf, ',');

	/* username */
	if (MyProcPort)
		appendCSVLiteral(&buf, MyProcPort->user_name);
	appendStringInfoChar(&buf, ',');

	/* database name */
	if (MyProcPort)
		appendCSVLiteral(&buf, MyProcPort->database_name);
	appendStringInfoChar(&buf, ',');

	/* Process id  */
	if (MyProcPid != 0)
		appendStringInfo(&buf, "%d", MyProcPid);
	appendStringInfoChar(&buf, ',');

	/* Remote host and port */
	if (MyProcPort && MyProcPort->remote_host)
	{
		appendStringInfoChar(&buf, '"');
		appendStringInfo(&buf, "%s", MyProcPort->remote_host);
		if (MyProcPort->remote_port && MyProcPort->remote_port[0] != '\0')
			appendStringInfo(&buf, ":%s", MyProcPort->remote_port);
		appendStringInfoChar(&buf, '"');
	}
	appendStringInfoChar(&buf, ',');

	/* session id */
	appendStringInfo(&buf, "%lx.%x", (long) MyStartTime, MyProcPid);
	appendStringInfoChar(&buf, ',');

	/* Line number */
	appendStringInfo(&buf, "%ld", log_line_number);
	appendStringInfoChar(&buf, ',');

	/* PS display */
	if (MyProcPort)
	{
		StringInfoData msgbuf;
		const char *psdisp;
		int			displen;

		initStringInfo(&msgbuf);

		psdisp = get_ps_display(&displen);
		appendBinaryStringInfo(&msgbuf, psdisp, displen);
		appendCSVLiteral(&buf, msgbuf.data);

		pfree(msgbuf.data);
	}
	appendStringInfoChar(&buf, ',');

	/* session start timestamp */
	if (formatted_start_time[0] == '\0')
		setup_formatted_start_time();
	appendStringInfoString(&buf, formatted_start_time);
	appendStringInfoChar(&buf, ',');

	/* Virtual transaction id */
	/* keep VXID format in sync with lockfuncs.c */
	if (MyProc != NULL && MyProc->backendId != InvalidBackendId)
		appendStringInfo(&buf, "%d/%u", MyProc->backendId, MyProc->lxid);
	appendStringInfoChar(&buf, ',');

	/* Transaction id */
	appendStringInfo(&buf, "%u", GetTopTransactionIdIfAny());
	appendStringInfoChar(&buf, ',');

	/* Error severity */
	appendStringInfo(&buf, "%s", error_severity(edata->elevel));
	appendStringInfoChar(&buf, ',');

	/* SQL state code */
	appendStringInfo(&buf, "%s", unpack_sql_state(edata->sqlerrcode));
	appendStringInfoChar(&buf, ',');

	/* errmessage */
	appendCSVLiteral(&buf, edata->message);
	appendStringInfoCharMacro(&buf, ',');

	/* errdetail or errdetail_log */
	if (edata->detail_log)
		appendCSVLiteral(&buf, edata->detail_log);
	else
		appendCSVLiteral(&buf, edata->detail);
	appendStringInfoCharMacro(&buf, ',');

	/* errhint */
	appendCSVLiteral(&buf, edata->hint);
	appendStringInfoCharMacro(&buf, ',');

	/* internal query */
	appendCSVLiteral(&buf, edata->internalquery);
	appendStringInfoCharMacro(&buf, ',');

	/* if printed internal query, print internal pos too */
	if (edata->internalpos > 0 && edata->internalquery != NULL)
		appendStringInfo(&buf, "%d", edata->internalpos);
	appendStringInfoCharMacro(&buf, ',');

	/* errcontext */
	appendCSVLiteral(&buf, edata->context);
	appendStringInfoCharMacro(&buf, ',');

	/* user query --- only reported if not disabled by the caller */
	if (is_log_level_output(edata->elevel, log_min_error_statement) &&
		debug_query_string != NULL &&
		!edata->hide_stmt)
		print_stmt = true;
	if (print_stmt)
		appendCSVLiteral(&buf, debug_query_string);
	appendStringInfoCharMacro(&buf, ',');
	if (print_stmt && edata->cursorpos > 0)
		appendStringInfo(&buf, "%d", edata->cursorpos);
	appendStringInfoCharMacro(&buf, ',');

	/* file error location */
	if (Log_error_verbosity >= PGERROR_VERBOSE)
	{
		StringInfoData msgbuf;

		initStringInfo(&msgbuf);

		if (edata->funcname && edata->filename)
			appendStringInfo(&msgbuf, "%s, %s:%d",
							 edata->funcname, edata->filename,
							 edata->lineno);
		else if (edata->filename)
			appendStringInfo(&msgbuf, "%s:%d",
							 edata->filename, edata->lineno);
		appendCSVLiteral(&buf, msgbuf.data);
		pfree(msgbuf.data);
	}

	appendStringInfoChar(&buf, '\n');

	/* If in the syslogger process, try to write messages direct to file */
	if (am_syslogger)
		write_syslogger_file(buf.data, buf.len, LOG_DESTINATION_CSVLOG);
	else
		write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_CSVLOG);

	pfree(buf.data);
}

/*
 * Unpack MAKE_SQLSTATE code. Note that this returns a pointer to a
 * static buffer.
 */
char *
unpack_sql_state(int sql_state)
{
	static char buf[12];
	int			i;

	for (i = 0; i < 5; i++)
	{
		buf[i] = PGUNSIXBIT(sql_state);
		sql_state >>= 6;
	}

	buf[i] = '\0';
	return buf;
}

#define WRITE_PIPE_CHUNK_TIMEOUT 1000

/*
 * Send the data through the pipe.
 */
static inline void
gp_write_pipe_chunk(const char *buffer, int len)
{
	int			retval;
	fd_set		wfds;
	struct timeval tv;
	int			retry_no;

	/*
	 * Wait until stderr becomes available for write. If it doesn't become
	 * available for WRITE_PIPE_CHUNK_TIMEOUT seconds, give up and ignore the
	 * error message. This could happen e.g. when the logger process crashes.
	 *
	 * We perform the wait in one second intervals, so that interrupts don't
	 * reset the wait.
	 *
	 * XXX: We really should use non-blocking mode here. Currently, it's
	 * possible that the another process writes to the pipe just after we've
	 * determined that it's writeable, and by the time we call write(),
	 * the buffer might be full and we block.
	 */
	for (retry_no = 0; retry_no < WRITE_PIPE_CHUNK_TIMEOUT; retry_no++)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&wfds);
		FD_SET(fileno(stderr), &wfds);

		retval = select(fileno(stderr) + 1, NULL, &wfds, NULL, &tv);

		if (retval == 0 || (retval < 0 && errno == EINTR))
		{
			/* select() timeout or interrupted. Retry */
			continue;
		}
		else
		{
			/*
			 * When the stderr is ready (retval == 1), or errors (other cases),
			 * break out the loop.
			 */
			break;
		}
	}

	Assert((retval == 1) || (retval < 0 && errno != EINTR) || (retry_no == WRITE_PIPE_CHUNK_TIMEOUT));

	if (retval == 1)
	{
		int bytes;

		do
		{
#ifdef USE_ASSERT_CHECKING
			{
				PipeProtoChunk *chunk = (PipeProtoChunk *) buffer;
				Assert(chunk->hdr.zero == 0);
				Assert(chunk->hdr.pid != 0);
				Assert(chunk->hdr.thid != 0);
				Assert(len <= PIPE_CHUNK_SIZE);
			}
#endif

			bytes = write(fileno(stderr), buffer, len);
		}
		while (bytes < 0 && errno == EINTR);
	}
}

/*
 * Append a string (terminated by '\0') to the GpPipeProtoChunk.
 *
 * If GpPipeProtoChunk does not have space for the given string,
 * this function appends enough data to fill the buffer, and
 * sends out the buffer. After that, the payload session of
 * GpPipeProtoChunk is reset and the rest of the given string
 * is appended. If the given string is pretty large, this function
 * may send out multiple chunks.
 */
static inline void
append_string_to_pipe_chunk(PipeProtoChunk *buffer, const char* input)
{
	if(am_syslogger)
		return;

	int len = 0;
	if (input != NULL)
	{
		len = strlen(input);
	}

	/*
	 * If this is a really long message, it does not really
	 * make lots of sense to print them all.  Truncate it.
	 */
	if (len >= PIPE_MAX_PAYLOAD * 20)
	{
		len = PIPE_MAX_PAYLOAD * 20 - 1;
	}

	char *data = buffer->data + buffer->hdr.len;
	int offset = 0;

	while (buffer->hdr.len + len >= PIPE_MAX_PAYLOAD)
	{
		int bytes = PIPE_MAX_PAYLOAD - buffer->hdr.len;
		memcpy(data, input + offset, bytes);

		Assert(bytes + buffer->hdr.len == PIPE_MAX_PAYLOAD);
		buffer->hdr.len = PIPE_MAX_PAYLOAD;
		
		gp_write_pipe_chunk((char *) buffer, PIPE_CHUNK_SIZE);

		buffer->hdr.len = 0;
		buffer->hdr.chunk_no++;
		data = buffer->data;

		len -= bytes;
		offset += bytes;
	}

	/* Copy the remaining data, and add '\0' at the end */
	memcpy(data, input + offset, len);
	data[len] = 0;
	buffer->hdr.len += len+1;

	Assert(buffer->hdr.len > 0 && buffer->hdr.len <= PIPE_MAX_PAYLOAD);
}

/*
 * Append the backtrace to the given PipeProtoChunk or the syslogger file or stderr.
 *
 * We can not use the default backtrace_symbols since it calls malloc, which
 * is not async-safe, to allocate space for symbols. Even though we don't
 * really support async-safe error logging yet, the malloc has caused several
 * deadlock issues in the past, we should avoid using them in our error handler.
 *
 * If buffer is NULL, the stack is written to the syslogger file if amsyslogger is true.
 * Otherwise, write to stderr.
 */
static void
append_stacktrace(PipeProtoChunk *buffer, StringInfo append, void *const *stackarray,
				  int stacksize, bool amsyslogger)
{
#if !defined(WIN32) && !defined(_AIX)
	int stack_no;
	char symbol[SYMBOL_SIZE]; /* a reasonable size for a symbol */
	Dl_info dli;
	int symbol_len;


	FILE * fd;
	bool fd_ok = false;
	char cmd[CMD_BUFFER_SIZE];
	char cmdresult[STACK_DEPTH_MAX][SYMBOL_SIZE];
	char addrtxt[ADDRESS_SIZE];

#if defined(__darwin__)
	const char * prog = "atos -o";
#else
	const char * prog = "addr2line -s -e";
#endif

	static bool in_translate_stacktrace = false;
	bool addr2line_ok = gp_log_stack_trace_lines;

	if (stacksize == 0)
		return;


	if (!in_translate_stacktrace && addr2line_ok)
	{
		/*
		 * Keep a record that we are doing this work, so if we crash during it, we don't
		 * try to do it again when we recurse back here,
		 */
		in_translate_stacktrace = true;

		snprintf(cmd,sizeof(cmd),"%s %s ",prog,my_exec_path);

		for (stack_no = 0; stack_no < stacksize && stack_no < 100; stack_no++)
		{
			cmdresult[stack_no][0] = '\0';   /* clear this array for later */
			snprintf(addrtxt, sizeof(addrtxt),"%p ",stackarray[stack_no]);
			
			Assert(sizeof(cmd) > strlen(cmd));
			strncat(cmd, addrtxt, sizeof(cmd) - strlen(cmd) - 1);
		}

		cmdresult[0][0] = '\0';
		fd = popen(cmd,"r");
		if (fd != NULL)
			fd_ok = true;

		if (fd_ok)
		{
			for (stack_no = 0; stack_no < stacksize && stack_no < STACK_DEPTH_MAX; stack_no++)
			{
				/* initialize the string */
				cmdresult[stack_no][0] = '\0';
				// Get one line of the result from addr2line (or atos)
				if (fgets(cmdresult[stack_no],SYMBOL_SIZE,fd) == NULL)
					break;
				// Force it to be a valid string (in case it was too long)
				cmdresult[stack_no][SYMBOL_SIZE-1] = '\0';
				// Get rid of the newline at the end.
				if (strlen(cmdresult[stack_no]) > 0 &&
					cmdresult[stack_no][strlen(cmdresult[stack_no])-1] == '\n')
					cmdresult[stack_no][strlen(cmdresult[stack_no])-1] = '\0';
			}
		}

		if (fd_ok && strlen(cmdresult[0]) > 1)
		{
			addr2line_ok = true;
		}

		if (fd != NULL)
			pclose(fd);

		in_translate_stacktrace = false;
	}

	for (stack_no = 0; stack_no < stacksize; stack_no++)
	{
		/* check if file/line info is available */
		char *lineInfo = "";
		if (addr2line_ok && stack_no < STACK_DEPTH_MAX)
		{
			lineInfo = cmdresult[stack_no];
		}

		if (dladdr(stackarray[stack_no], &dli) != 0)
		{
			const char *file = dli.dli_fname;
			if (file != NULL &&	file[0] != '\0')
			{
				const char *dir_path = strrchr(file, '/');
				if (strncmp(file, "postgres:", strlen("postgres:")) == 0)
				{
					file = "postgres";
				}
				else if (dir_path != NULL)
				{
					/* don't print path to file */
					file = dir_path + 1;
				}
			}
			else
			{
				file = "";
			}

			const char *function = dli.dli_sname;
			if (function == NULL || function[0] == '\0')
			{
				function = "<symbol not found>";
			}

			// check if lineInfo was retrieved
			// if lineinfo does not contain symbol ':' then the output of cmd contains the input address
			// if lineinfo contains symbol '?' then the filename and line number cannot be determined (the output is ??:0)
			if (strchr(lineInfo, ':') == NULL ||
			    strchr(lineInfo, '?') != NULL)
			{
				/* no line info, print offset in function */
				symbol_len = snprintf(symbol,
									  ARRAY_SIZE(symbol),
									  "%-4d %p %s %s + 0x%x\n",
									  stack_no + 1,
									  stackarray[stack_no],
									  file,
									  function,
									  (int)((char *)(stackarray[stack_no]) - (char *)(dli.dli_saddr)));
			}
			else
			{
				/* keep file:line info; required for atos */
				char *parenth = strrchr(lineInfo, '(');
				if (parenth != NULL)
				{
					lineInfo = parenth + 1;
					parenth = strrchr(lineInfo, ')');
					*parenth = '\0';
				}

				/* line info added, print file and line info */
				symbol_len = snprintf(symbol,
									  ARRAY_SIZE(symbol),
									  "%-4d %p %s %s (%s)\n",
									  stack_no + 1,
									  stackarray[stack_no],
									  file,
									  function,
									  lineInfo);
			}


		}
		else
		{
			if (lineInfo[0] == '\0')
			{
				lineInfo = "<symbol not found>";
			}

			symbol_len = snprintf(symbol,
								  ARRAY_SIZE(symbol),
								  "%-4d %p %s\n",
								  stack_no + 1,
								  stackarray[stack_no],
								  lineInfo);
		}

		if (buffer != NULL)
		{
			append_string_to_pipe_chunk(buffer, symbol);

			if (stack_no != stacksize - 1)
			{
				/* Eliminate the last '\0' */
				buffer->hdr.len --;
			}
		}

		else
		{
			if (append)
			{
				appendStringInfo(append, "%s", symbol);
			}
			else
			{
				if (amsyslogger)
					write_syslogger_file_binary(symbol, symbol_len, LOG_DESTINATION_STDERR);
				else
					write(fileno(stderr), symbol, symbol_len);
			}
		}
	}
#endif
}

/*
 * Directly write a string to the syslogger file or stderr.
 */
static inline void
write_syslogger_file_string(const char *str, bool amsyslogger, bool append_comma)
{
	if (str != NULL && str[0] != '\0')
	{
		if (amsyslogger)
		{
			write_syslogger_file_binary("\"", 1, LOG_DESTINATION_STDERR);
			syslogger_write_str(str, strlen(str), true, true);
			write_syslogger_file_binary("\"", 1, LOG_DESTINATION_STDERR);
		}
		else
		{
			write(fileno(stderr), "\"", 1);
			syslogger_write_str(str, strlen(str), false, true);
			write(fileno(stderr), "\"", 1);
		}
	}

	if (append_comma)
	{
		if (amsyslogger)
			write_syslogger_file_binary(",", 1, LOG_DESTINATION_STDERR);
		else
			write(fileno(stderr), ",", 1);
	}
}


/*
 * Directly write the message in CSV format to the syslogger file or stderr.
 */
static void
write_syslogger_in_csv(ErrorData *edata, bool amsyslogger)
{
	/* timestamp_with_millisecond */
	syslogger_append_current_timestamp(amsyslogger);

	/* username */
	if (MyProcPort != NULL && MyProcPort->user_name != NULL)
		write_syslogger_file_string(MyProcPort->user_name, amsyslogger, true);
	else
		write_syslogger_file_string(NULL, amsyslogger, true);

	/* databasename */
	if (MyProcPort != NULL && MyProcPort->database_name != NULL)
		write_syslogger_file_string(MyProcPort->database_name, amsyslogger, true);
	else
		write_syslogger_file_string(NULL, amsyslogger, true);

	/* Process id, thread id */
	syslogger_write_int32(false, "p", MyProcPid, amsyslogger, true);
	syslogger_write_int32(false, "th", mythread(), amsyslogger, true);

	/* Remote host, remote port */
	if (MyProcPort != NULL && MyProcPort->remote_host != NULL)
		write_syslogger_file_string(MyProcPort->remote_host, amsyslogger, true);
	else
		write_syslogger_file_string(NULL, amsyslogger, true);
	if (MyProcPort != NULL && MyProcPort->remote_port != NULL)
		write_syslogger_file_string(MyProcPort->remote_port, amsyslogger, true);
	else
		write_syslogger_file_string(NULL, amsyslogger, true);

	/* session start timestamp */
	syslogger_append_timestamp((MyProcPort != NULL ?
								(pg_time_t) timestamptz_to_time_t(MyProcPort->SessionStartTime): 0),
							   amsyslogger, true);

	/* transaction id */
	syslogger_write_int32(false, "", GetTopTransactionIdIfAny(), amsyslogger, true);

	/* GPDB specific options */
	syslogger_write_int32(true, "con", gp_session_id, amsyslogger, true);
	syslogger_write_int32(true, "cmd", gp_command_count, amsyslogger, true);
	syslogger_write_int32(false, "seg", Gp_segment, amsyslogger, true);
	syslogger_write_int32(true, "slice", currentSliceId, amsyslogger, true);
	{
		DistributedTransactionId dist_trans_id;
		TransactionId local_trans_id;
		TransactionId subtrans_id;

		GetAllTransactionXids(&dist_trans_id,
							  &local_trans_id,
							  &subtrans_id);

		syslogger_write_int32(true, "dx", dist_trans_id, amsyslogger, true);
		syslogger_write_int32(true, "x", local_trans_id, amsyslogger, true);
		syslogger_write_int32(true, "sx", subtrans_id, amsyslogger, true);
	}

	/* error severity */
	write_syslogger_file_string(error_severity(edata->elevel), amsyslogger, true);

	/* sql state code */
	write_syslogger_file_string(unpack_sql_state(edata->sqlerrcode), amsyslogger, true);

	/* error message */
	write_syslogger_file_string(edata->message, amsyslogger, true);

	/* errdetail */
	if (edata->detail_log)
		write_syslogger_file_string(edata->detail_log, amsyslogger, true);
	else
		write_syslogger_file_string(edata->detail, amsyslogger, true);

	/* errhint */
	write_syslogger_file_string(edata->hint, amsyslogger, true);

	/* internal query */
	write_syslogger_file_string(edata->internalquery, amsyslogger, true);

	/* internal query pos */
	syslogger_write_int32(true, "", edata->internalpos, amsyslogger, true);

	/* error context */
	write_syslogger_file_string(edata->context, amsyslogger, true);

	/* user query */
	write_syslogger_file_string(debug_query_string, amsyslogger, true);

	/* cursor pos */
	syslogger_write_int32(true, "", edata->cursorpos, amsyslogger, true);

	/* func name */
	write_syslogger_file_string(edata->funcname, amsyslogger, true);

	/* file name */
	write_syslogger_file_string(edata->filename, amsyslogger, true);

	/* line number */
	syslogger_write_int32(true, "", edata->lineno, amsyslogger, true);

	/* stack trace */

	if ((edata->printstack ||
			(edata->elevel >= ERROR &&
			(edata->elevel == PANIC || !edata->omit_location))) &&
		edata->stacktracesize > 0)
	{
		append_stacktrace(NULL /*PipeProtoChunk*/, NULL /*StringInfo*/, edata->stacktracearray,
						  edata->stacktracesize, amsyslogger);
	}

	/* EOL */
	if (amsyslogger)
		write_syslogger_file_binary(LOG_EOL, strlen(LOG_EOL), LOG_DESTINATION_STDERR);
	else
		write(fileno(stderr), LOG_EOL, strlen(LOG_EOL));
}

/*
 * Write error report to server's log.
 *
 * This is an equivalent function as send_message_to_server_log, but will write
 * the error report in the format of GpPipeProtoHeader, followed by a serialized
 * format of GpErrorData. The error report is sent over to the syslogger process
 * through the pipe.
 *
 * This function is thread-safe. Here, we assume that sprintf is thread-safe.
 */
void
write_message_to_server_log(int elevel,
							int sqlerrcode,
							const char *message,
							const char *detail,
							const char *hint,
							const char *query_text,
							int cursorpos,
							int internalpos,
							const char *internalquery,
							const char *context,
							const char *funcname,
							bool show_funcname,
							const char *filename,
							int lineno,
							int stacktracesize,
							bool omit_location,
							bool send_alert,
							void* const *stacktracearray,
							bool printstack)
{
	PipeProtoChunk buffer;

	char	   *data = buffer.data;
	GpErrorDataFixFields fix_fields;
	static uint64 log_line_number = 0;

	Assert(!am_syslogger);

	buffer.hdr.zero = 0;
	buffer.hdr.len = 0;
	buffer.hdr.pid = MyProcPid;
	buffer.hdr.thid = mythread();
	buffer.hdr.main_thid = mainthread();
	buffer.hdr.chunk_no = 0;
	buffer.hdr.is_last = 'f';
	buffer.hdr.log_format = 'c';
	buffer.hdr.log_line_number = log_line_number++;
	buffer.hdr.is_segv_msg = 'f';
	buffer.hdr.next = -1;


    /* Serialize edata in the order defined in GpErrorData. */

	fix_fields.session_start_time =
		(MyProcPort == NULL) ? 0 : (pg_time_t) timestamptz_to_time_t(MyProcPort->SessionStartTime);
	fix_fields.send_alert = send_alert ? 't' : 'f';
	fix_fields.omit_location = omit_location ? 't' : 'f';
	fix_fields.gp_is_primary = 't';
	fix_fields.gp_session_id = gp_session_id;
	fix_fields.gp_command_count = gp_command_count;
	fix_fields.gp_segment_id = Gp_segment;
	fix_fields.slice_id = currentSliceId;
	fix_fields.error_cursor_pos = cursorpos;
	fix_fields.internal_query_pos = internalpos;
	fix_fields.error_fileline = lineno;
	fix_fields.top_trans_id = GetTopTransactionIdIfAny();

	GetAllTransactionXids(&(fix_fields.dist_trans_id),
						  &(fix_fields.local_trans_id),
						  &(fix_fields.subtrans_id));

	Assert(buffer.hdr.len + sizeof(GpErrorDataFixFields) <= PIPE_MAX_PAYLOAD);

	memcpy(data, &fix_fields, sizeof(GpErrorDataFixFields));
	buffer.hdr.len += sizeof(GpErrorDataFixFields);

	/* Variable-length fields */

	/* username */
	if (MyProcPort == NULL || MyProcPort->user_name == NULL)
		append_string_to_pipe_chunk(&buffer, NULL);
	else
		append_string_to_pipe_chunk(&buffer, MyProcPort->user_name);

	/* databasename */
	if (MyProcPort == NULL || MyProcPort->database_name == NULL)
		append_string_to_pipe_chunk(&buffer, NULL);
	else
		append_string_to_pipe_chunk(&buffer, MyProcPort->database_name);

	/* remote_host */
	if (MyProcPort == NULL || MyProcPort->remote_host == NULL)
		append_string_to_pipe_chunk(&buffer, NULL);
	else
		append_string_to_pipe_chunk(&buffer, MyProcPort->remote_host);

	/* remote_port */
	if (MyProcPort == NULL || MyProcPort->remote_port == NULL)
		append_string_to_pipe_chunk(&buffer, NULL);
	else
		append_string_to_pipe_chunk(&buffer, MyProcPort->remote_port);

	/* error severity */
	append_string_to_pipe_chunk(&buffer, error_severity(elevel));

	/* sql state */
	append_string_to_pipe_chunk(&buffer, unpack_sql_state(sqlerrcode));

	/* error_message */
	append_string_to_pipe_chunk(&buffer, message);

	/* error_detail */
	append_string_to_pipe_chunk(&buffer, detail);

	/* error_hint */
	append_string_to_pipe_chunk(&buffer, hint);

	/* internal_query */
	append_string_to_pipe_chunk(&buffer, internalquery);

	/* error_context  */
	append_string_to_pipe_chunk(&buffer, context);

	/* debug_query_string */
	append_string_to_pipe_chunk(&buffer, query_text);

	/* error_func_name */
	if (show_funcname)
		append_string_to_pipe_chunk(&buffer, funcname);
	else
		append_string_to_pipe_chunk(&buffer, NULL);

	/* error_filename */
	append_string_to_pipe_chunk(&buffer, filename);

	/* stacktrace */
	if ((printstack ||
		 (elevel >= ERROR &&
		  (elevel == PANIC || !omit_location))) &&
		stacktracesize > 0 &&
		stacktracearray != NULL)
	{
		// move stack trace to new line
		append_string_to_pipe_chunk(&buffer, "Stack trace:\n");
		buffer.hdr.len --;

		append_stacktrace(&buffer, NULL /*StringInfo*/, stacktracearray, stacktracesize,
						  false /*amsyslogger*/);
	}

	/* Send the last chunk */
	buffer.hdr.is_last = 't';
	gp_write_pipe_chunk((char *) &buffer, buffer.hdr.len + PIPE_HEADER_SIZE);
}

/*
 * Write error report to server's log
 */
static void
send_message_to_server_log(ErrorData *edata)
{
	StringInfoData buf;
	StringInfoData prefix;
	int			nc;

	AssertImply(mainthread() != 0, mythread() == mainthread());

	if (Log_destination & LOG_DESTINATION_STDERR)
	{
		if (Logging_collector && gp_log_format == 1)
		{
			if (redirection_done)
			{
				if (!am_syslogger)
					write_message_to_server_log(edata->elevel,
												edata->sqlerrcode,
												edata->message,
												edata->detail_log != NULL ? edata->detail_log : edata->detail,
												edata->hint,
												debug_query_string,
												edata->cursorpos,
												edata->internalpos,
												edata->internalquery,
												edata->context,
												edata->funcname,
												edata->show_funcname,
												edata->filename,
												edata->lineno,
												edata->stacktracesize,
												edata->omit_location,
												edata->send_alert,
												edata->stacktracearray,
												edata->printstack);
				else
					write_syslogger_in_csv(edata, true);
			}
			else
			{
				write_syslogger_in_csv(edata, false);
			}

			return;
		}
	}

	/* Format message prefix. */
	initStringInfo(&buf);

	formatted_log_time[0] = '\0';

	log_line_prefix(&buf);
	nc = buf.len;
	appendStringInfo(&buf, "%s:  ", error_severity(edata->elevel));

	/* Save copy of prefix for subsequent lines of multi-line message. */
	initStringInfo(&prefix);
	appendBinaryStringInfo(&prefix, buf.data, nc);
	nc = 2 + buf.len - prefix.len;
	enlargeStringInfo(&prefix, nc);
	memset(prefix.data+prefix.len, ' ', nc);
	prefix.len += nc;
	prefix.data[prefix.len] = '\0';

	if (Log_error_verbosity >= PGERROR_VERBOSE &&
		edata->sqlerrcode)
	{
		/* unpack MAKE_SQLSTATE code */
		char		tbuf[12];
		int			ssval;
		int			i;

		ssval = edata->sqlerrcode;
		for (i = 0; i < 5; i++)
		{
			tbuf[i] = PGUNSIXBIT(ssval);
			ssval >>= 6;
		}
		tbuf[i] = '\0';
		appendStringInfo(&buf, "(%s) ", tbuf);
	}

	if (edata->message)
	{
		char   *cp = edata->message;

		while (*cp <= ' ' &&
			   *cp > '\0')
			cp++;
		append_with_tabs(&buf, cp);
	}
	else
		append_with_tabs(&buf, _("missing error text"));

	if (edata->cursorpos > 0)
		appendStringInfo(&buf, _(" at character %d"),
						 edata->cursorpos);
	else if (edata->internalpos > 0)
		appendStringInfo(&buf, _(" at character %d"),
						 edata->internalpos);

	appendStringInfoChar(&buf, '\n');

	if (Log_error_verbosity >= PGERROR_DEFAULT)
	{
		if (edata->detail_log)
		{
			log_line_prefix(&buf);
			appendStringInfoString(&buf, _("DETAIL:  "));
			append_with_tabs(&buf, edata->detail_log);
			appendStringInfoChar(&buf, '\n');
		}
		else if (edata->detail)
		{
			appendBinaryStringInfo(&buf, prefix.data, prefix.len);
			appendStringInfoString(&buf, _("DETAIL:  "));
			append_with_tabs(&buf, edata->detail);
			appendStringInfoChar(&buf, '\n');
		}
		if (edata->hint)
		{
			log_line_prefix(&buf);
			appendStringInfoString(&buf, _("HINT:  "));
			append_with_tabs(&buf, edata->hint);
			appendStringInfoChar(&buf, '\n');
		}
		if (edata->internalquery)
		{
			log_line_prefix(&buf);
			appendStringInfoString(&buf, _("QUERY:  "));
			append_with_tabs(&buf, edata->internalquery);
			appendStringInfoChar(&buf, '\n');
		}
		if (edata->context)
		{
			log_line_prefix(&buf);
			appendStringInfoString(&buf, _("CONTEXT:  "));
			append_with_tabs(&buf, edata->context);
			appendStringInfoChar(&buf, '\n');
		}
		if (Log_error_verbosity >= PGERROR_VERBOSE)
		{
			if (edata->elevel == INFO || edata->omit_location)
			{}
			/* assume no newlines in funcname or filename... */
			else if (edata->funcname && edata->filename)
			{
				appendBinaryStringInfo(&buf, prefix.data, prefix.len);
				appendStringInfo(&buf, _("LOCATION:  %s, %s:%d\n"),
								 edata->funcname, edata->filename,
								 edata->lineno);
			}
			else if (edata->filename)
			{
				log_line_prefix(&buf);
				appendStringInfo(&buf, _("LOCATION:  %s:%d\n"),
								 edata->filename, edata->lineno);
			}
		}
	}

	/*
	 * If the user wants the query that generated this error logged, do it.
	 */
	if (is_log_level_output(edata->elevel, log_min_error_statement) &&
		debug_query_string != NULL &&
		!edata->hide_stmt)
	{
		log_line_prefix(&buf);
		appendStringInfoString(&buf, _("STATEMENT:  "));
		append_with_tabs(&buf, debug_query_string);
		appendStringInfoChar(&buf, '\n');
	}

	if (edata->elevel >= ERROR &&
		(edata->elevel == PANIC || !edata->omit_location) &&
		edata->stacktracesize > 0)
	{
#ifndef WIN32
		char	  **strings;
		size_t		i;

		strings = backtrace_symbols(edata->stacktracearray, edata->stacktracesize);
		if (strings != NULL)
		{
			for (i = 0; i < edata->stacktracesize; i++)
			{
				appendBinaryStringInfo(&buf, prefix.data, prefix.len);
				appendStringInfo(&buf, "Traceback %d:  %.200s", (int)i, strings[i]);
				appendStringInfoChar(&buf, '\n');
			}
			free(strings);
		}
#endif
	}


#ifdef HAVE_SYSLOG
	/* Write to syslog, if enabled */
	if (Log_destination & LOG_DESTINATION_SYSLOG)
	{
		int			syslog_level;

		switch (edata->elevel)
		{
			case DEBUG5:
			case DEBUG4:
			case DEBUG3:
			case DEBUG2:
			case DEBUG1:
				syslog_level = LOG_DEBUG;
				break;
			case LOG:
			case COMMERROR:
			case INFO:
				syslog_level = LOG_INFO;
				break;
			case NOTICE:
			case WARNING:
				syslog_level = LOG_NOTICE;
				break;
			case ERROR:
				syslog_level = LOG_WARNING;
				break;
			case FATAL:
				syslog_level = LOG_ERR;
				break;
			case PANIC:
			default:
				syslog_level = LOG_CRIT;
				break;
		}

		write_syslog(syslog_level, buf.data);
	}
#endif   /* HAVE_SYSLOG */

#ifdef WIN32
	/* Write to eventlog, if enabled */
	if (Log_destination & LOG_DESTINATION_EVENTLOG)
	{
		write_eventlog(edata->elevel, buf.data);
	}
#endif   /* WIN32 */

	/* Write to stderr, if enabled */
	if ((Log_destination & LOG_DESTINATION_STDERR) || whereToSendOutput == DestDebug)
	{
		/*
		 * Use the chunking protocol if we know the syslogger should be
		 * catching stderr output, and we are not ourselves the syslogger.
		 * Otherwise, just do a vanilla write to stderr.
		 */
		if (redirection_done && !am_syslogger)
			write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_STDERR);
#ifdef WIN32

		/*
		 * In a win32 service environment, there is no usable stderr. Capture
		 * anything going there and write it to the eventlog instead.
		 *
		 * If stderr redirection is active, it was OK to write to stderr above
		 * because that's really a pipe to the syslogger process.
		 */
		else if (pgwin32_is_service() && (!redirection_done || am_syslogger) )
			write_eventlog(edata->elevel, buf.data);
#endif
			/* only use the chunking protocol if we know the syslogger should
			 * be catching stderr output, and we are not ourselves the
			 * syslogger. Otherwise, go directly to stderr.
			 */
			if (redirection_done && !am_syslogger)
				write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_STDERR);
			else
				write(fileno(stderr), buf.data, buf.len);
	}

	/* If in the syslogger process, try to write messages direct to file */
	if (am_syslogger)
		write_syslogger_file_binary(buf.data, buf.len, LOG_DESTINATION_STDERR);

	pfree(prefix.data);

	/* Write to CSV log if enabled */
	if (Log_destination & LOG_DESTINATION_CSVLOG)
	{
		if (redirection_done || am_syslogger)
		{
			/*
			 * send CSV data if it's safe to do so (syslogger doesn't need the
			 * pipe). First get back the space in the message buffer.
			 */
			pfree(buf.data);
			write_csvlog(edata);
		}
		else
		{
			const char *msg = _("Not safe to send CSV data\n");

			write(fileno(stderr), msg, strlen(msg));
			if (!(Log_destination & LOG_DESTINATION_STDERR) &&
				whereToSendOutput != DestDebug)
			{
				/* write message to stderr unless we just sent it above */
				write(fileno(stderr), buf.data, buf.len);
			}
			pfree(buf.data);
		}
	}
	else
	{
		pfree(buf.data);
	}
}

/*
 * Send data to the syslogger using the chunked protocol
 */
static void
write_pipe_chunks(char *data, int len, int dest)
{
	PipeProtoChunk p;

	int			fd = fileno(stderr);

	Assert(len > 0);

	p.hdr.zero = 0;
	p.hdr.pid = MyProcPid;
	p.hdr.thid = mythread();
	p.hdr.main_thid = mainthread();
	p.hdr.chunk_no = 0;
	p.hdr.log_format = (dest == LOG_DESTINATION_CSVLOG ? 'c' : 't');
	p.hdr.is_segv_msg = 'f';
	p.hdr.next = -1;

	/* write all but the last chunk */
	while (len > PIPE_MAX_PAYLOAD)
	{
		p.hdr.is_last = 'f';
		p.hdr.len = PIPE_MAX_PAYLOAD;
		memcpy(p.data, data, PIPE_MAX_PAYLOAD);

#ifdef USE_ASSERT_CHECKING
				Assert(p.hdr.zero == 0);
				Assert(p.hdr.pid != 0);
				Assert(p.hdr.thid != 0);
#endif
		write(fd, &p, PIPE_CHUNK_SIZE);
		data += PIPE_MAX_PAYLOAD;
		len -= PIPE_MAX_PAYLOAD;

		++p.hdr.chunk_no;
	}

	/* write the last chunk */
	p.hdr.is_last = 't';
	p.hdr.len = len;

#ifdef USE_ASSERT_CHECKING
		Assert(p.hdr.zero == 0);
		Assert(p.hdr.pid != 0);
		Assert(p.hdr.thid != 0);
		Assert(PIPE_HEADER_SIZE + len <= PIPE_CHUNK_SIZE);
#endif
	memcpy(p.data, data, len);
	write(fd, &p, PIPE_HEADER_SIZE + len);
}


/*
 * Append a text string to the error report being built for the client.
 *
 * This is ordinarily identical to pq_sendstring(), but if we are in
 * error recursion trouble we skip encoding conversion, because of the
 * possibility that the problem is a failure in the encoding conversion
 * subsystem itself.  Code elsewhere should ensure that the passed-in
 * strings will be plain 7-bit ASCII, and thus not in need of conversion,
 * in such cases.  (In particular, we disable localization of error messages
 * to help ensure that's true.)
 */
static void
err_sendstring(StringInfo buf, const char *str)
{
	if (in_error_recursion_trouble())
		pq_send_ascii_string(buf, str);
	else
		pq_sendstring(buf, str);
}

/*
 * Write error report to client
 */
static void
send_message_to_frontend(ErrorData *edata)
{
	StringInfoData msgbuf;

	/* 'N' (Notice) is for nonfatal conditions, 'E' is for errors */
	pq_beginmessage(&msgbuf, (edata->elevel < ERROR) ? 'N' : 'E');

	if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
	{
		/* New style with separate fields */
		char		tbuf[12];
		int			ssval;
		int			i;

		pq_sendbyte(&msgbuf, PG_DIAG_SEVERITY);
		err_sendstring(&msgbuf, error_severity(edata->elevel));

		/* unpack MAKE_SQLSTATE code */
		ssval = edata->sqlerrcode;
		for (i = 0; i < 5; i++)
		{
			tbuf[i] = PGUNSIXBIT(ssval);
			ssval >>= 6;
		}
		tbuf[i] = '\0';

		pq_sendbyte(&msgbuf, PG_DIAG_SQLSTATE);
		err_sendstring(&msgbuf, tbuf);

		/* M field is required per protocol, so always send something */
		pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_PRIMARY);
		if (edata->message)
			err_sendstring(&msgbuf, edata->message);
		else
			err_sendstring(&msgbuf, _("missing error text"));

		if (edata->detail)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_DETAIL);
			err_sendstring(&msgbuf, edata->detail);
		}

		/* detail_log is intentionally not used here */

		if (edata->hint)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_HINT);
			err_sendstring(&msgbuf, edata->hint);
		}

		if (edata->context)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_CONTEXT);
			err_sendstring(&msgbuf, edata->context);
		}

		if (edata->cursorpos > 0)
		{
			snprintf(tbuf, sizeof(tbuf), "%d", edata->cursorpos);
			pq_sendbyte(&msgbuf, PG_DIAG_STATEMENT_POSITION);
			err_sendstring(&msgbuf, tbuf);
		}

		if (edata->internalpos > 0)
		{
			snprintf(tbuf, sizeof(tbuf), "%d", edata->internalpos);
			pq_sendbyte(&msgbuf, PG_DIAG_INTERNAL_POSITION);
			err_sendstring(&msgbuf, tbuf);
		}

		if (edata->internalquery)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_INTERNAL_QUERY);
			err_sendstring(&msgbuf, edata->internalquery);
		}

		if (edata->filename)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FILE);
			err_sendstring(&msgbuf, edata->filename);
		}

		if (edata->lineno > 0)
		{
			snprintf(tbuf, sizeof(tbuf), "%d", edata->lineno);
			pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_LINE);
			err_sendstring(&msgbuf, tbuf);
		}

		if (edata->funcname)
		{
			pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FUNCTION);
			err_sendstring(&msgbuf, edata->funcname);
		}

		pq_sendbyte(&msgbuf, '\0');		/* terminator */
	}
	else
	{
		/* Old style --- gin up a backwards-compatible message */
		StringInfoData buf;

		initStringInfo(&buf);

		appendStringInfo(&buf, "%s:  ", error_severity(edata->elevel));

		if (edata->show_funcname && edata->funcname)
			appendStringInfo(&buf, "%s: ", edata->funcname);

		if (edata->message)
			appendStringInfoString(&buf, edata->message);
		else
			appendStringInfoString(&buf, _("missing error text"));

		if (edata->cursorpos > 0)
			appendStringInfo(&buf, _(" at character %d"),
							 edata->cursorpos);
		else if (edata->internalpos > 0)
			appendStringInfo(&buf, _(" at character %d"),
							 edata->internalpos);

		appendStringInfoChar(&buf, '\n');

		err_sendstring(&msgbuf, buf.data);

		pfree(buf.data);
	}

	pq_endmessage(&msgbuf);

	/*
	 * This flush is normally not necessary, since postgres.c will flush out
	 * waiting data when control returns to the main loop. But it seems best
	 * to leave it here, so that the client has some clue what happened if the
	 * backend dies before getting back to the main loop ... error/notice
	 * messages should not be a performance-critical path anyway, so an extra
	 * flush won't hurt much ...
	 */
	pq_flush();
}


/*
 * Support routines for formatting error messages.
 */


/*
 * expand_fmt_string --- process special format codes in a format string
 *
 * We must replace %m with the appropriate strerror string, since vsnprintf
 * won't know what to do with it.
 *
 * The result is a palloc'd string.
 */
static char *
expand_fmt_string(const char *fmt, ErrorData *edata)
{
	StringInfoData buf;
	const char *cp;

	initStringInfo(&buf);

	for (cp = fmt; *cp; cp++)
	{
		if (cp[0] == '%' && cp[1] != '\0')
		{
			cp++;
			if (*cp == 'm')
			{
				/*
				 * Replace %m by system error string.  If there are any %'s in
				 * the string, we'd better double them so that vsnprintf won't
				 * misinterpret.
				 */
				const char *cp2;

				cp2 = useful_strerror(edata->saved_errno);
				for (; *cp2; cp2++)
				{
					if (*cp2 == '%')
						appendStringInfoCharMacro(&buf, '%');
					appendStringInfoCharMacro(&buf, *cp2);
				}
			}
			else
			{
				/* copy % and next char --- this avoids trouble with %%m */
				appendStringInfoCharMacro(&buf, '%');
				appendStringInfoCharMacro(&buf, *cp);
			}
		}
		else
			appendStringInfoCharMacro(&buf, *cp);
	}

	return buf.data;
}


/*
 * A slightly cleaned-up version of strerror()
 */
static const char *
useful_strerror(int errnum)
{
	/* this buffer is only used if errno has a bogus value */
	static char errorstr_buf[48];
	const char *str;

#ifdef WIN32
	/* Winsock error code range, per WinError.h */
	if (errnum >= 10000 && errnum <= 11999)
		return pgwin32_socket_strerror(errnum);
#endif
	str = strerror(errnum);

	/*
	 * Some strerror()s return an empty string for out-of-range errno. This is
	 * ANSI C spec compliant, but not exactly useful.
	 */
	if (str == NULL || *str == '\0')
	{
		snprintf(errorstr_buf, sizeof(errorstr_buf),
		/*------
		  translator: This string will be truncated at 47
		  characters expanded. */
				 _("operating system error %d"), errnum);
		str = errorstr_buf;
	}

	return str;
}


/*
 * error_severity --- get localized string representing elevel
 */
static const char *
error_severity(int elevel)
{
	const char *prefix;

	switch (elevel)
	{
		case DEBUG1:
			prefix = _("DEBUG1");
			break;
		case DEBUG2:
			prefix = _("DEBUG2");
			break;
		case DEBUG3:
			prefix = _("DEBUG3");
			break;
		case DEBUG4:
			prefix = _("DEBUG4");
			break;
		case DEBUG5:
			prefix = _("DEBUG5");
			break;
		case LOG:
		case COMMERROR:
			prefix = _("LOG");
			break;
		case INFO:
			prefix = _("INFO");
			break;
		case NOTICE:
			prefix = _("NOTICE");
			break;
		case WARNING:
			prefix = _("WARNING");
			break;
		case ERROR:
			prefix = _("ERROR");
			break;
		case FATAL:
			prefix = _("FATAL");
			break;
		case PANIC:
			prefix = _("PANIC");
			break;
		default:
			prefix = "???";
			break;
	}

	return prefix;
}


/*
 *	append_with_tabs
 *
 *	Append the string to the StringInfo buffer, inserting a tab after any
 *	newline.
 */
static void
append_with_tabs(StringInfo buf, const char *str)
{
	char		ch;

	while ((ch = *str++) != '\0')
	{
		appendStringInfoCharMacro(buf, ch);
		if (ch == '\n')
			appendStringInfoCharMacro(buf, '\t');
	}
}


/*
 * Write errors to stderr (or by equal means when stderr is
 * not available). Used before ereport/elog can be used
 * safely (memory context, GUC load etc)
 */
void
write_stderr(const char *fmt,...)
{
	va_list		ap;

	fmt = _(fmt);

	va_start(ap, fmt);

	if (Logging_collector && gp_log_format == 1)
	{
		char		errbuf[2048];		/* Arbitrary size? */

		vsnprintf(errbuf, sizeof(errbuf), fmt, ap);

		if (!am_syslogger)
		{
			/* Write the message in the CSV format */
			write_message_to_server_log(LOG,
										0,
										errbuf,
										NULL,
										NULL,
										NULL,
										0,
										0,
										NULL,
										NULL,
										NULL,
										false,
										NULL,
										0,
										0,
										true,
										false,
										NULL,
										false);
		}
		else
		{
			ErrorData edata;
			memset(&edata, 0, sizeof(ErrorData));
			edata.elevel = LOG;
			edata.message = errbuf;
			edata.omit_location = true;
			if (redirection_done)
				write_syslogger_in_csv(&edata, true);
			else
				write_syslogger_in_csv(&edata, false);
		}

		va_end(ap);
		return;
	}

#ifndef WIN32
	/* On Unix, we just fprintf to stderr */
	vfprintf(stderr, fmt, ap);
	fflush(stderr);
#else

	/*
	 * On Win32, we print to stderr if running on a console, or write to
	 * eventlog if running as a service
	 */
	if (pgwin32_is_service())	/* Running as a service */
	{
		char		errbuf[2048];		/* Arbitrary size? */

		vsnprintf(errbuf, sizeof(errbuf), fmt, ap);

		write_eventlog(ERROR, errbuf);
	}
	else
	{
		/* Not running as service, write to stderr */
		vfprintf(stderr, fmt, ap);
		fflush(stderr);
	}
#endif
	va_end(ap);
}


/*
 * is_log_level_output -- is elevel logically >= log_min_level?
 *
 * We use this for tests that should consider LOG to sort out-of-order,
 * between ERROR and FATAL.  Generally this is the right thing for testing
 * whether a message should go to the postmaster log, whereas a simple >=
 * test is correct for testing whether the message should go to the client.
 */
static bool
is_log_level_output(int elevel, int log_min_level)
{
	if (elevel == LOG || elevel == COMMERROR)
	{
		if (log_min_level == LOG || log_min_level <= ERROR)
			return true;
	}
	else if (log_min_level == LOG)
	{
		/* elevel != LOG */
		if (elevel >= FATAL)
			return true;
	}
	/* Neither is LOG */
	else if (elevel >= log_min_level)
		return true;

	return false;
}

/*
 * elog_debug_linger
 */
void
elog_debug_linger(ErrorData *edata)
{
	int			seconds_to_linger = gp_debug_linger;
	int			seconds_lingered = 0;

	/* Don't linger again in the event of another error. */
	gp_debug_linger = 0;

	/* A word of explanation to the user... */
	errhint("%s%sProcess %d will wait for gp_debug_linger=%d seconds before termination.\n"
			"Note that its locks and other resources will not be released until then.",
			edata->hint ? edata->hint : "",
			edata->hint ? "\n" : "",
			MyProcPid,
			seconds_to_linger);

	/* Log the error and notify the client. */
	EmitErrorReport();
	fflush(stdout);
	fflush(stderr);

	/* Terminate the client connection. */
	pq_comm_close_fatal();

	while (seconds_lingered < seconds_to_linger)
	{
		int			seconds_left = seconds_to_linger - seconds_lingered;
		int			minutes_left = seconds_left / 60;
		int			setproctitle_seconds = (minutes_left <= 1) ? 5
									 : (minutes_left <= 5) ? 30
									 : 60;
		int			sleep_seconds;
		char		buf[50];

		/* Update 'ps' display. */
		snprintf(buf, sizeof(buf)-1,
				 "error exit in %dm %ds",
				 minutes_left,
				 seconds_left - minutes_left * 60);
		set_ps_display(buf, true);

		/* Sleep. */
		sleep_seconds = Min(seconds_left, setproctitle_seconds);
		pg_usleep(sleep_seconds * 1000000L);
		seconds_lingered += sleep_seconds;
	}
}							   /* elog_debug_linger */

/*
 * gp_elog
 *
 * This externally callable function allows a user or application to insert records into the log.
 * Only superusers are allowed to call this function due to the potential for abuse.
 *
 * The function takes two arguments, the first is the message to insert into the log (type text).
 * The second is optional, type bool, that specifies if we should send an alert after inserting
 * the message into the log.
 *
 *
 */
Datum
gp_elog(PG_FUNCTION_ARGS)
{
	ErrorData	edata;
	char	   *errormsg;
	const char *save_debug_query;
	bool		sendalert = false;

	/* Validate input arguments */
	if (PG_NARGS() != 1 && PG_NARGS() != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("gp_elog(): called with %hd arguments",
						PG_NARGS())));
	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("gp_elog(): called with null arguments")));

	if (PG_NARGS() == 2 && PG_ARGISNULL(1))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("gp_elog(): called with null arguments")));

	/* Must be super user */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied"),
				 errhint("gp_elog(): requires superuser privileges.")));

	errormsg = text_to_cstring(PG_GETARG_TEXT_PP(0));
	/* text_to_cstring is guaranteed to not return a NULL, so we don't need to check */

	if (PG_NARGS() > 1)
		sendalert = PG_GETARG_BOOL(1);

	memset(&edata, 0, sizeof(edata));
	edata.elevel = LOG;
	edata.output_to_server = true;
	edata.output_to_client = false;
	edata.show_funcname = false;
	edata.omit_location = true;
	edata.hide_stmt = true;
	edata.internalquery = "";
	edata.send_alert = sendalert;
	edata.message = (char *)errormsg;						/* edata.message really should be const char * */
	edata.sqlerrcode = MAKE_SQLSTATE('X','X', '1','0','0'); /* use a special SQLSTATE to make it easy to search the log */

	/* We don't need to log the SQL statement as part of this, it's just noise */
	save_debug_query = debug_query_string;
	debug_query_string = "";

	send_message_to_server_log(&edata);

	debug_query_string = save_debug_query;

	pfree(errormsg);

	PG_RETURN_VOID();
}

void
debug_backtrace(void)
{
#ifndef WIN32
	int 		stacktracesize;
	void	   *stacktracearray[30];

	stacktracesize = backtrace(stacktracearray, 30);

	append_stacktrace(NULL /*PipeProtoChunk*/, NULL /*StringInfo*/, stacktracearray, stacktracesize,
					 false/*amsyslogger*/);
#endif

}

/*
 * Unwind stack up to a given depth and store frame addresses to passed array;
 * return stack depth;
 */
uint32 gp_backtrace(void **stackAddresses, uint32 maxStackDepth)
{
#if defined(__i386) || defined(__x86_64__)

	Assert(stack_base_ptr != NULL);

	/* get base pointer of current frame */
	uint64 framePtrValue = 0;
	GET_FRAME_POINTER(framePtrValue);

	uint32 depth = 0;
	void **pFramePtr = (void**) GET_PTR_FROM_VALUE(framePtrValue);

	/* check if the frame pointer is valid */
	if (pFramePtr != NULL && (void *) &depth < (void *) pFramePtr)
	{
		/* consider the first maxStackDepth frames only, below the stack base pointer */
		for (depth = 0; depth < maxStackDepth; depth++)
		{
			/* check if next frame is within stack */
			if (pFramePtr == NULL ||
				(void *) pFramePtr > *pFramePtr ||
				(void *) stack_base_ptr < *pFramePtr)
			{
				break;
			}

			/* get return address (one above the frame pointer) */
			const uintptr_t *returnAddr = (uintptr_t *)(pFramePtr + 1);

			/* store return address */
			stackAddresses[depth] = (void *) *returnAddr;

			/* move to next frame */
			pFramePtr = (void**)*pFramePtr;
		}
	}
	else
	{
		depth  = backtrace(stackAddresses, maxStackDepth);
	}

	Assert(depth > 0);

	return depth;

#else
	return backtrace(stackAddresses, maxStackDepth);
#endif
}


/*
 * Build stack trace
 */
char *gp_stacktrace(void **stackAddresses, uint32 stackDepth)
{
	StringInfoData append;
	initStringInfo(&append);

#ifdef WIN32
	appendStringInfoString(&append, "stack trace is not available for this platform");
#else
	append_stacktrace(NULL /*PipeProtoChunk*/, &append, stackAddresses, stackDepth,
					 false/*amsyslogger*/);
#endif

	/* we may fail to retrieve stack on opt build */
	if (0 == append.len)
	{
		appendStringInfoString(&append, "failed to retrieve stack");
	}

	return append.data;
}

/*
 * SignalName
 *   Convert a SEGV/BUS/ILL to name.
 */
const char *
SegvBusIllName(int signal)
{
	Assert(signal == SIGILL ||
		   signal == SIGSEGV ||
		   signal == SIGBUS);
	
	switch (signal)
	{
#ifdef SIGILL
		case SIGILL:
			return "SIGILL";
#endif
#ifdef SIGSEGV
		case SIGSEGV:
			return "SIGSEGV";
#endif
#ifdef SIGBUS
		case SIGBUS:
			return "SIGBUS";
#endif
	}

	return NULL;
}

/*
 * StandardHandlerForSigillSigsegvSigbus_OnMainThread
 *   Async-safe signal handler for SEGV/BUS/ILL.
 * This function simple collects the stack addresses and some process information
 * and write them to the pipe.
 */
void
StandardHandlerForSigillSigsegvSigbus_OnMainThread(char *processName, SIGNAL_ARGS)
{
	PG_SETMASK(&BlockSig);

	/* Unblock SEGV/BUS/ILL signals, and set them to their default settings. */
#ifdef SIGILL
	pqsignal(SIGILL, SIG_DFL);
#endif
#ifdef SIGSEGV
	pqsignal(SIGSEGV, SIG_DFL);
#endif
#ifdef SIGBUS
	pqsignal(SIGBUS, SIG_DFL);
#endif

	PipeProtoChunk buffer;
	
	buffer.hdr.zero = 0;
	buffer.hdr.len = 0;
	buffer.hdr.pid = MyProcPid;

	/*
	 * mythread() are not really async-safe, but syslogger requires this value
	 * to be set as part of an identifier of a chunk. We create a fake value here to
	 * satisfy the condition of a valid chunk. But in the syslogger, we reset its
	 * value to 0.
	 */
	buffer.hdr.thid = FIXED_THREAD_ID;
	buffer.hdr.main_thid = mainthread();
	buffer.hdr.chunk_no = 0;
	buffer.hdr.is_last = 't';
	buffer.hdr.log_format = 'c';
	buffer.hdr.is_segv_msg = 't';
	buffer.hdr.log_line_number = 0;
	buffer.hdr.next = -1;

	char *data = buffer.data;
	GpSegvErrorData *errorData = (GpSegvErrorData *)data;
	
	errorData->session_start_time = 0;
	if (MyProcPort)
	{
		errorData->session_start_time =
			(pg_time_t)timestamptz_to_time_t(MyProcPort->SessionStartTime);
	}

	errorData->gp_session_id = gp_session_id;
	errorData->gp_command_count = gp_command_count;
	errorData->gp_segment_id = Gp_segment;
	errorData->slice_id = currentSliceId;
	errorData->signal_num = (int32)postgres_signal_arg;
	errorData->frame_depth = 0;

	/*
	 * Compute how many frame addresses we are able to send in a single chunk.
	 * The total space that is available for frame addresses is
	 * (PIPE_MAX_PAYLOAD - MAXALIGN(sizeof(GpSegvErrorData))).
	 */
	int frameDepth = (PIPE_MAX_PAYLOAD - MAXALIGN(sizeof(GpSegvErrorData))) / sizeof(void *);
	Assert(frameDepth > 0);

	void *stackAddressArray = data + MAXALIGN(sizeof(GpSegvErrorData));
	void **stackAddresses = stackAddressArray;
	errorData->frame_depth = gp_backtrace(stackAddresses, frameDepth);

	buffer.hdr.len =
		MAXALIGN(sizeof(GpSegvErrorData)) +
		errorData->frame_depth * sizeof(void *);

	gp_write_pipe_chunk((char *) &buffer, buffer.hdr.len + PIPE_HEADER_SIZE);

	/* re-raise the signal to OS */
	raise(postgres_signal_arg);
}
