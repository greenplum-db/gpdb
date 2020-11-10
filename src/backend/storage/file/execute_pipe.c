/*-------------------------------------------------------------------------
 *
 * execute_pipe.c
 *	   popen() replacement with stderr
 *
 * NOTES:
 *
 * COPY and external table can execute arbitary command. Upstream uses
 * OpenPipeStream(), which uses popen to create the new process. But
 * popen can't catch stderr, so when something goes wrong only exit code
 * is reported. This is a replacement that also catches stderr as well,
 * so we can get detail messages from child process if provided.
 *
 * Portions Copyright (C) 2020-Present VMware, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/storage/file/execute_pipe.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "postgres.h"
#include "storage/execute_pipe.h"
#include "cdb/cdbvars.h"

#define EXEC_DATA_P 0 /* index to data pipe */
#define EXEC_ERR_P 1 /* index to error pipe  */

static void read_err_msg(int fid, StringInfo sinfo);

/*
 * popen_with_stderr
 *
 * standard popen doesn't redirect stderr from the child process.
 * we need stderr in order to display the error that child process
 * encountered and show it to the user. This is, therefore, a wrapper
 * around a set of file descriptor redirections and a fork.
 *
 * if 'forwrite' is set then we set the data pipe write side on the
 * parent. otherwise, we set the read side on the parent.
 *
 * This function is used to replace OpenPipeStream in COPY TO/FROM
 * PROGRAM command. With OpenPipeStream, COPY command can only report
 * exit code.
 */
int
popen_with_stderr(int *pipes, const char *exe, bool forwrite)
{
	int data[2];	/* pipe to send data child <--> parent */
	int err[2];		/* pipe to send errors child --> parent */
	int pid = -1;

	const int READ = 0;
	const int WRITE = 1;

	if (pipe(data) < 0)
		return -1;

	if (pipe(err) < 0)
	{
		close(data[READ]);
		close(data[WRITE]);
		return -1;
	}
#ifndef WIN32

	pid = fork();

	if (pid > 0) /* parent */
	{

		if (forwrite)
		{
			/* parent writes to child */
			close(data[READ]);
			pipes[EXEC_DATA_P] = data[WRITE];
		}
		else
		{
			/* parent reads from child */
			close(data[WRITE]);
			pipes[EXEC_DATA_P] = data[READ];
		}

		close(err[WRITE]);
		pipes[EXEC_ERR_P] = err[READ];

		return pid;
	}
	else if (pid == 0) /* child */
	{

		/*
		 * set up the data pipe
		 */
		if (forwrite)
		{
			close(data[WRITE]);
			close(fileno(stdin));

			/* assign pipes to parent to stdin */
			if (dup2(data[READ], fileno(stdin)) < 0)
			{
				perror("dup2 error");
				exit(EXIT_FAILURE);
			}

			/* no longer needed after the duplication */
			close(data[READ]);
		}
		else
		{
			close(data[READ]);
			close(fileno(stdout));

			/* assign pipes to parent to stdout */
			if (dup2(data[WRITE], fileno(stdout)) < 0)
			{
				perror("dup2 error");
				exit(EXIT_FAILURE);
			}

			/* no longer needed after the duplication */
			close(data[WRITE]);
		}

		/*
		 * now set up the error pipe
		 */
		close(err[READ]);
		close(fileno(stderr));

		if (dup2(err[WRITE], fileno(stderr)) < 0)
		{
			if(forwrite)
				close(data[WRITE]);
			else
				close(data[READ]);

			perror("dup2 error");
			exit(EXIT_FAILURE);
		}

		close(err[WRITE]);

		/* go ahead and execute the user command */
		execl("/bin/sh", "sh", "-c", exe, NULL);

		/* if we're here an error occurred */
		exit(EXIT_FAILURE);
	}
	else
	{
		if(forwrite)
		{
			close(data[WRITE]);
			close(data[READ]);
		}
		else
		{
			close(data[READ]);
			close(data[WRITE]);
		}
		close(err[READ]);
		close(err[WRITE]);

		return -1;
	}
#endif

	return pid;
}

/*
 * pclose_with_stderr
 *
 * close our data and error pipes and return the child process
 * termination status. if child terminated with error, 'buf' will
 * point to the error string retrieved from child's stderr.
 */
int
pclose_with_stderr(int pid, int *pipes, StringInfo sinfo)
{
	int status = 0;

	/* close the data pipe. we can now read from error pipe without being blocked */
	close(pipes[EXEC_DATA_P]);

	read_err_msg(pipes[EXEC_ERR_P], sinfo);

	close(pipes[EXEC_ERR_P]);

	if (kill(pid, 0) == 0) /* process exists */
	{
	#ifndef WIN32
		waitpid(pid, &status, 0);
	#else
		status = -1;
	#endif
	}

	return status;
}


/*
 * read err msg from err pipe
 */
static void
read_err_msg(int fid, StringInfo sinfo)
{
	char ebuf[512];
	int ebuflen = 512;

	while (true)
	{
		int nread = read(fid, ebuf, ebuflen);

		if(nread == 0)
		{
			break;
		}
		else if(nread > 0)
		{
			appendBinaryStringInfo(sinfo, ebuf, nread);
		}
		else
		{
			appendStringInfoString(sinfo, "error string unavailable due to read error");
			break;
		}
	}

	if (sinfo->len > 0){
		write_log("read err msg from pipe, len:%d msg:%s", sinfo->len, sinfo->data);
	}
}
