/*---------------------------------------------------------------------------
 *	Greenplum Database
 *	Copyright (c) 2004-2015 VMware, Inc. or its affiliates.
 *
 *	@filename:
 *		_api.h
 *
 *	@doc:
 *		GPOS wrapper interface for GPDB.
 *
 *	@owner:
 *
 *	@test:
 *
 *
 *---------------------------------------------------------------------------*/
#ifndef GPOS_api_H
#define GPOS_api_H

#ifndef USE_CMAKE
#include "pg_config.h"
#endif
#include "gpos/base.h"

#ifdef __cplusplus

// lookup given exception type in the given array
gpos::BOOL FoundException(gpos::CException &exc, const gpos::ULONG *exceptions,
						  gpos::ULONG size);

// Check if given exception is an unexpected reason for failing to
// produce a plan
gpos::BOOL IsLoggableFailure(gpos::CException &exc);

// check if given exception should error out
gpos::BOOL ShouldErrorOut(gpos::CException &exc);


extern "C" {
#include <stddef.h>
#endif /* __cplusplus */

/*
 * struct with configuration parameters for task execution;
 * this needs to be in sync with the corresponding structure in optserver.h
 */
struct gpos_exec_params
{
	void *(*func)(void *); /* task function */
	void *arg;			   /* task argument */
	void *result;		   /* task result */
	void *stack_start;	   /* start of current thread's stack */
	char *error_buffer;	   /* buffer used to store error messages */
	int error_buffer_size; /* size of error message buffer */
	bool *abort_requested; /* flag indicating if abort is requested */
};

/* struct containing initialization parameters for gpos */
struct gpos_init_params
{
	bool (*abort_requested)(void); /* callback to report abort requests */
};

/* initialize GPOS memory pool, worker pool and message repository */
void gpos_init(struct gpos_init_params *params);

/*
 * execute function as a GPOS task using current thread;
 * return 0 for successful completion, 1 for error
 */
int gpos_exec(gpos_exec_params *params);

/* shutdown GPOS memory pool, worker pool and message repository */
void gpos_terminate(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !GPOS_api_H */

// EOF
