#ifndef GPFXDIST_H
#define GPFXDIST_H

#include <apr.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_IO_H
#include <io.h>
#endif

#include <apr_general.h>
#include <apr_thread_proc.h>
#include <apr_strings.h>

/* 
 * gpfxdist uses this structure to hold additional options and state.
 */
struct gpfxdist_t
{
	char*		cmd;		/* transformation command */
	int			for_write;	/* 1 if writing to subprocess, 0 if reading from subprocess */
	int			pass_paths; /* 1 if subprocess expects filename to contain paths to data files, 0 otherwise */

	apr_pool_t* mp;			/* apache portable runtime memory pool */
	apr_proc_t	proc;		/* apache portable runtime child process structure */
	char*		tempfilename; /* name of temporary file containing file paths, removed at end */
	char*		errfilename; /* name of temporary file containing stderr output, removed at end */
	apr_file_t* errfile;	/* APR handle for errfilename */
};

#endif
