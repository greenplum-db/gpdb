/*-------------------------------------------------------------------------
 *
 * cdbdisp_async.h
 * routines for asynchronous implementation of dispatching commands
 * to the qExec processes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdisp_async.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_ASYNC_H
#define CDBDISP_ASYNC_H

extern DispatcherInternalFuncs DispatcherAsyncFuncs;

/*
 * Hook for plugins to check permissions in dispatcher
 * One example is to check whether disk quota limit is 
 * exceeded for the table which is loading data.
 */
typedef bool (*DispatcherCheckPerms_hook_type) (void);
extern PGDLLIMPORT DispatcherCheckPerms_hook_type DispatcherCheckPerms_hook;

#endif
