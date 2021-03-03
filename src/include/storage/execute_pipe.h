
/*-------------------------------------------------------------------------
 *
 * execute_pipe.c
 *	   popen() replacement with stderr
 *
 * Portions Copyright (C) 2020-Present VMware, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/storage/execute_pipe.h
 *
 *-------------------------------------------------------------------------
 */

#include "lib/stringinfo.h"

extern int popen_with_stderr(int *rwepipe, const char *exe, bool forwrite);
extern int pclose_with_stderr(int pid, int *rwepipe, StringInfo sinfo);
