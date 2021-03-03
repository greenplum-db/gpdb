/*-------------------------------------------------------------------------
 *
 * url_execute.c
 *	  Core support for opening external relations via a URL execute
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 *	  src/backend/access/external/url_execute.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "access/url.h"
#include "storage/execute_pipe.h"
#include "cdb/cdbtimer.h"
#include "cdb/cdbvars.h"
#include "libpq/pqsignal.h"
#include "utils/resowner.h"

#define EXEC_DATA_P 0 /* index to data pipe */
#define EXEC_ERR_P 1 /* index to error pipe  */

/*
 * This struct encapsulates the resources that need to be explicitly cleaned up
 * on error. We use the resource owner mechanism to make sure
 * these are not leaked. When a ResourceOwner is released, our hook will
 * walk the list of open curlhandles, and releases any that were owned by
 * the released resource owner.
 *
 * On abort, we need to close the pipe FDs, and wait for the subprocess to
 * exit.
 */
typedef struct execute_handle_t
{
	/*
	 * PID of the open sub-process, and pipe FDs to communicate with it.
	 */
	int			pid;
	int			pipes[2];		/* only out and err needed */

	ResourceOwner owner;	/* owner of this handle */
	struct execute_handle_t *next;
	struct execute_handle_t *prev;
} execute_handle_t;

/*
 * Private state for an EXECUTE external table.
 */
typedef struct URL_EXECUTE_FILE
{
	URL_FILE	common;

	char	   *shexec;			/* shell command-line */

	execute_handle_t *handle;	/* ResourceOwner-tracked stuff */
} URL_EXECUTE_FILE;

static void pclose_without_stderr(int *rwepipe);
static char *interpretError(int exitCode, char *buf, size_t buflen, char *err, size_t errlen);
static const char *getSignalNameFromCode(int signo);
static void cleanup_execute_handle(execute_handle_t *h);


/*
 * Linked list of open "handles". These are allocated in TopMemoryContext,
 * and tracked by resource owners.
 */
static execute_handle_t *open_execute_handles;

static bool execute_resowner_callback_registered;

static execute_handle_t *
create_execute_handle(void)
{
	execute_handle_t *h;

	h = MemoryContextAlloc(TopMemoryContext, sizeof(execute_handle_t));
	h->pid = -1;
	h->pipes[EXEC_DATA_P] = -1;
	h->pipes[EXEC_ERR_P] = -1;

	h->owner = CurrentResourceOwner;
	h->next = open_execute_handles;
	h->prev = NULL;
	if (open_execute_handles)
		open_execute_handles->prev = h;
	open_execute_handles = h;

	return h;
}

/*
 * Close any open handles on abort.
 */
static void
destroy_execute_handle(execute_handle_t *h)
{
	int hpid = h->pid;

	cleanup_execute_handle(h);
	if (h->pid != -1)
	{
#ifndef WIN32
		int			status;
		waitpid(hpid, &status, 0);
#endif
	}

}

/*
 * Cleanup open handles.
 */
static void
cleanup_execute_handle(execute_handle_t *h)
{
	/* unlink from linked list first */
	if (h->prev)
		h->prev->next = h->next;
	else
		open_execute_handles = h->next;
	if (h->next)
		h->next->prev = h->prev;

	if (h->pipes[EXEC_DATA_P] != -1)
		close(h->pipes[EXEC_DATA_P]);

	/* We don't bother reading possible error message from the pipe */
	if (h->pipes[EXEC_ERR_P] != -1)
		close(h->pipes[EXEC_ERR_P]);

	pfree(h);
}

/*
 * Close any open handles on abort.
 */
static void
execute_abort_callback(ResourceReleasePhase phase,
					   bool isCommit,
					   bool isTopLevel,
					   void *arg)
{
	execute_handle_t *curr;
	execute_handle_t *next;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	next = open_execute_handles;
	while (next)
	{
		curr = next;
		next = curr->next;

		if (curr->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(WARNING, "execute-type external table reference leak: %p still referenced", curr);

			destroy_execute_handle(curr);
		}
	}
}


static void
make_export(char *name, const char *value, StringInfo buf)
{
	char		ch;

	/*
	 * Shell-quote the value so that we don't need to escape other special char
	 * except single quote and backslash. (We assume the variable name doesn't contain
	 * funny characters.
	 *
	 * Every single-quote is replaced with '\''. For example, value
	 * foo'bar becomes 'foo'\''bar'.
	 *
	 * Don't need to escape backslash, although using echo will behave differently on
	 * different platforms. It's better to write as: /usr/bin/env bash -c 'echo -E "$VAR"'.
	 */
	appendStringInfo(buf, "%s='", name);

	for ( ; 0 != (ch = *value); value++)
	{
		if(ch == '\'')
		{
			appendStringInfo(buf, "\'\\\'");
		}

		appendStringInfoChar(buf, ch);
	}

	appendStringInfo(buf, "' && export %s && ", name);
}


char *
make_command(const char *cmd, extvar_t *ev)
{
	StringInfoData buf;

	initStringInfo(&buf);

	make_export("GP_MASTER_HOST", ev->GP_MASTER_HOST, &buf);
	make_export("GP_MASTER_PORT", ev->GP_MASTER_PORT, &buf);
	make_export("GP_SEG_PG_CONF", ev->GP_SEG_PG_CONF, &buf);
	make_export("GP_SEG_DATADIR", ev->GP_SEG_DATADIR, &buf);
	make_export("GP_DATABASE", ev->GP_DATABASE, &buf);
	make_export("GP_USER", ev->GP_USER, &buf);
	make_export("GP_DATE", ev->GP_DATE, &buf);
	make_export("GP_TIME", ev->GP_TIME, &buf);
	make_export("GP_XID", ev->GP_XID, &buf);
	make_export("GP_CID", ev->GP_CID, &buf);
	make_export("GP_SN", ev->GP_SN, &buf);
	make_export("GP_SEGMENT_ID", ev->GP_SEGMENT_ID, &buf);
	make_export("GP_SEG_PORT", ev->GP_SEG_PORT, &buf);
	make_export("GP_SESSION_ID", ev->GP_SESSION_ID, &buf);
	make_export("GP_SEGMENT_COUNT", ev->GP_SEGMENT_COUNT, &buf);
	make_export("GP_QUERY_STRING", ev->GP_QUERY_STRING, &buf);

	appendStringInfoString(&buf, cmd);

	return buf.data;
}

/**
 * execute_fopen()
 *
 * refactor the fopen code for execute into this routine
 */
URL_FILE *
url_execute_fopen(char *url, bool forwrite, extvar_t *ev, CopyState pstate)
{
	URL_EXECUTE_FILE *file;
	int			save_errno;
	struct itimers savetimers;
	pqsigfunc	save_SIGPIPE;
	char	   *cmd;

	/* Execute command */
	Assert(strncmp(url, EXEC_URL_PREFIX, strlen(EXEC_URL_PREFIX)) == 0);
	cmd  = url + strlen(EXEC_URL_PREFIX);

	file = palloc0(sizeof(URL_EXECUTE_FILE));
	file->common.type = CFTYPE_EXEC;	/* marked as a EXEC */
	file->common.url = pstrdup(url);
	file->shexec = make_command(cmd, ev);		/* Execute command */

	/* Clear process interval timers */
	resetTimers(&savetimers);

	if (!execute_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(execute_abort_callback, NULL);
		execute_resowner_callback_registered = true;
	}

	file->handle = create_execute_handle();

	/*
	 * Preserve the SIGPIPE handler and set to default handling.  This
	 * allows "normal" SIGPIPE handling in the command pipeline.  Normal
	 * for PG is to *ignore* SIGPIPE.
	 */
	save_SIGPIPE = pqsignal(SIGPIPE, SIG_DFL);

	/* execute the user command */
	file->handle->pid = popen_with_stderr(file->handle->pipes,
										  file->shexec,
										  forwrite);
	save_errno = errno;

	/* Restore the SIGPIPE handler */
	pqsignal(SIGPIPE, save_SIGPIPE);

	/* Restore process interval timers */
	restoreTimers(&savetimers);

	elog(DEBUG5, "EXTERNAL TABLE EXECUTE Command: %s", file->shexec);
	if (file->handle->pid == -1)
	{
		errno = save_errno;
		ereport(ERROR,
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
						errmsg("cannot start external table command: %m"),
						errdetail("Command: %s", cmd)));

	}

	return (URL_FILE *) file;
}

void
url_execute_fclose(URL_FILE *file, bool failOnError, const char *relname)
{
	URL_EXECUTE_FILE *efile = (URL_EXECUTE_FILE *) file;
	StringInfoData sinfo;
	char	   *url;
	int			ret=0;

	initStringInfo(&sinfo);

	/* close the child process and related pipes */
	if(failOnError)
		ret = pclose_with_stderr(efile->handle->pid, efile->handle->pipes, &sinfo);
	else
		pclose_without_stderr(efile->handle->pipes);

	cleanup_execute_handle(efile->handle);
	efile->handle = NULL;

	url = pstrdup(file->url);
	if (ret == 0)
	{
		/* pclose() ended successfully; no errors to reflect */
		;
	}
	else if (ret == -1)
	{
		/* pclose()/wait4() ended with an error; errno should be valid */
		if (failOnError)
			pfree(file);
		ereport((failOnError ? ERROR : LOG),
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("cannot close external table %s command: %m",
						(relname ? relname : "")),
				 errdetail("command: %s", url)));
	}
	else
	{
		/*
		 * pclose() returned the process termination state.  The interpretExitCode() function
		 * generates a descriptive message from the exit code.
		 */
		char buf[512];

		if (failOnError)
			pfree(file);
		ereport((failOnError ? ERROR : LOG),
				(errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
				 errmsg("external table %s command ended with %s",
						(relname ? relname : ""),
						interpretError(ret, buf, sizeof(buf)/sizeof(char), sinfo.data, sinfo.len)),
				 errdetail("Command: %s", url)));
	}
	pfree(url);
	pfree(sinfo.data);

	pfree(file);
}

bool
url_execute_feof(URL_FILE *file, int bytesread)
{
	return (bytesread == 0);
}

bool
url_execute_ferror(URL_FILE *file, int bytesread, char *ebuf, int ebuflen)
{
	URL_EXECUTE_FILE *efile = (URL_EXECUTE_FILE *) file;
	int			ret;
	int			nread;

	ret = (bytesread == -1);
	if(ret == true && ebuflen > 0 && ebuf != NULL)
	{
		/*
		 * Read one byte less than the maximum size to ensure zero
		 * termination of the buffer.
		 */
reread:
		nread = read(efile->handle->pipes[EXEC_ERR_P], ebuf, ebuflen -1);
		if(nread == -1 && errno == EINTR)
		{
			CHECK_FOR_INTERRUPTS();
			goto reread;
		}

		if(nread != -1)
			ebuf[nread] = 0;
		else
			strncpy(ebuf,"error string unavailable due to read error",ebuflen-1);
	}

	return ret;
}

size_t
url_execute_fread(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
	URL_EXECUTE_FILE *efile = (URL_EXECUTE_FILE *) file;
	ssize_t		n;
	bool        rerun;

	do {
		n = read(efile->handle->pipes[EXEC_DATA_P], ptr, size);

		if (n == -1 && errno == EINTR)
		{
			CHECK_FOR_INTERRUPTS();
			rerun = true;
		}
		else
			rerun = false;
	} while (rerun);

	return n;
}

size_t
url_execute_fwrite(void *ptr, size_t size, URL_FILE *file, CopyState pstate)
{
    URL_EXECUTE_FILE *efile = (URL_EXECUTE_FILE *) file;
    int fd = efile->handle->pipes[EXEC_DATA_P];
    size_t offset = 0;
    const char* p = (const char* ) ptr;

    size_t n;
    /* ensure all data in buffer is send out to pipe*/
    while(size > offset)
    {
        n = write(fd,p,size - offset);

		if (n == -1)
		{
			if (errno == EINTR)
			{
				CHECK_FOR_INTERRUPTS();
				continue;
			}
			return -1;
		}

        if(n == 0) break;

        offset += n;
        p = (const char*)ptr + offset;
    }

    if(offset < size) elog(WARNING,"partial write, expected %lu, written %lu", size, offset);

    return offset;
}

/*
 * interpretError - formats a brief message and/or the exit code from pclose()
 * 		(or wait4()).
 */
static char *
interpretError(int rc, char *buf, size_t buflen, char *err, size_t errlen)
{
	if (WIFEXITED(rc))
	{
		int exitCode = WEXITSTATUS(rc);
		if (exitCode >= 128)
		{
			/*
			 * If the exit code has the 128-bit set, the exit code represents
			 * a shell exited by signal where the signal number is exitCode - 128.
			 */
			exitCode -= 128;
			snprintf(buf, buflen, "SHELL TERMINATED by signal %s (%d)", getSignalNameFromCode(exitCode), exitCode);
		}
		else if (exitCode == 0)
		{
			snprintf(buf, buflen, "EXITED; rc=%d", exitCode);
		}
		else
		{
			/* Exit codes from commands rarely map to strerror() strings. In here
			 * we show the error string returned from pclose, and omit the non
			 * friendly exit code interpretation */
			snprintf(buf, buflen, "error. %s", err);
		}
	}
	else if (WIFSIGNALED(rc))
	{
		int signalCode = WTERMSIG(rc);
		snprintf(buf, buflen, "TERMINATED by signal %s (%d)", getSignalNameFromCode(signalCode), signalCode);
	}
#ifndef WIN32
	else if (WIFSTOPPED(rc))
	{
		int signalCode = WSTOPSIG(rc);
		snprintf(buf, buflen, "STOPPED by signal %s (%d)", getSignalNameFromCode(signalCode), signalCode);
	}
#endif
	else
	{
		snprintf(buf, buflen, "UNRECOGNIZED termination; rc=%#x", rc);
	}

	return buf;
}


struct signalDef {
	const int			signalCode;
	const char		   *signalName;
};

/*
 * Table mapping signal numbers to signal identifiers (names).
 */
struct signalDef signals[] = {
#ifdef SIGHUP
		{ SIGHUP,    "SIGHUP" },
#endif
#ifdef SIGINT
		{ SIGINT,    "SIGINT" },
#endif
#ifdef SIGQUIT
		{ SIGQUIT,   "SIGQUIT" },
#endif
#ifdef SIGILL
		{ SIGILL,    "SIGILL" },
#endif
#ifdef SIGTRAP
		{ SIGTRAP,   "SIGTRAP" },
#endif
#ifdef SIGABRT
		{ SIGABRT,   "SIGABRT" },
#endif
#ifdef SIGEMT
		{ SIGEMT,    "SIGEMT" },
#endif
#ifdef SIGFPE
		{ SIGFPE,    "SIGFPE" },
#endif
#ifdef SIGKILL
		{ SIGKILL,   "SIGKILL" },
#endif
#ifdef SIGBUS
		{ SIGBUS,    "SIGBUS" },
#endif
#ifdef SIGSEGV
		{ SIGSEGV,   "SIGSEGV" },
#endif
#ifdef SIGSYS
		{ SIGSYS,    "SIGSYS" },
#endif
#ifdef SIGPIPE
		{ SIGPIPE,   "SIGPIPE" },
#endif
#ifdef SIGALRM
		{ SIGALRM,   "SIGALRM" },
#endif
#ifdef SIGTERM
		{ SIGTERM,   "SIGTERM" },
#endif
#ifdef SIGURG
		{ SIGURG,    "SIGURG" },
#endif
#ifdef SIGSTOP
		{ SIGSTOP,   "SIGSTOP" },
#endif
#ifdef SIGTSTP
		{ SIGTSTP,   "SIGTSTP" },
#endif
#ifdef SIGCONT
		{ SIGCONT,   "SIGCONT" },
#endif
#ifdef SIGCHLD
		{ SIGCHLD,   "SIGCHLD" },
#endif
#ifdef SIGTTIN
		{ SIGTTIN,   "SIGTTIN" },
#endif
#ifdef SIGTTOU
		{ SIGTTOU,   "SIGTTOU" },
#endif
#ifdef SIGIO
		{ SIGIO,     "SIGIO" },
#endif
#ifdef SIGXCPU
		{ SIGXCPU,   "SIGXCPU" },
#endif
#ifdef SIGXFSZ
		{ SIGXFSZ,   "SIGXFSZ" },
#endif
#ifdef SIGVTALRM
		{ SIGVTALRM, "SIGVTALRM" },
#endif
#ifdef SIGPROF
		{ SIGPROF,   "SIGPROF" },
#endif
#ifdef SIGWINCH
		{ SIGWINCH,  "SIGWINCH" },
#endif
#ifdef SIGINFO
		{ SIGINFO,   "SIGINFO" },
#endif
#ifdef SIGUSR1
		{ SIGUSR1,   "SIGUSR1" },
#endif
#ifdef SIGUSR2
		{ SIGUSR2,   "SIGUSR2" },
#endif
		{ -1, "" }
};


/*
 * getSignalNameFromCode - gets the signal name given the signal number.
 */
static const char *
getSignalNameFromCode(int signo)
{
	int i;
	for (i = 0; signals[i].signalCode != -1; i++)
	{
		if (signals[i].signalCode == signo)
			return signals[i].signalName;
	}

	return "UNRECOGNIZED";
}


/*
 * pclose_without_stderr
 *
 * close our data and error pipes
 * we don't probe for any error message or suspend the current process.
 * this function is meant for scenarios when the current slice doesn't
 * need to wait for the error message available at the completion of
 * the child process.
 */
static void
pclose_without_stderr(int *pipes)
{
	close(pipes[EXEC_DATA_P]);
	close(pipes[EXEC_ERR_P]);
}
