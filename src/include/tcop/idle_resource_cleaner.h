/*-------------------------------------------------------------------------
 *
 * idle_resource_cleaner.h
 *	  Functions used to clean up resources on idle sessions.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/tcop/idle_resource_cleaner.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef IDLE_RESOURCE_CLEANER_H
#define IDLE_RESOURCE_CLEANER_H

extern void	StartIdleResourceCleanupTimers(void);
extern void	CancelIdleResourceCleanupTimers(void);

extern void IdleGangTimeoutHandler(void);

extern void EnableClientWaitTimeoutInterrupt(void);
extern bool DisableClientWaitTimeoutInterrupt(void);

extern int	IdleSessionGangTimeout;

#endif /* IDLE_RESOURCE_CLEANER_H */
