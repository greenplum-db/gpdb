/*-------------------------------------------------------------------------
 *
 * idle_resource_cleaner.h
 *	  Functions used to clean up resources on idle sessions.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/tcop/idle_resource_cleaner.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef IDLE_RESOURCE_CLEANER_H
#define IDLE_RESOURCE_CLEANER_H


void		StartIdleResourceCleanupTimers(void);

void		DoIdleResourceCleanup(void);

extern int	IdleSessionGangTimeout;

extern int	(*get_idle_session_timeout_hook) (void);
extern void (*idle_session_timeout_action_hook) (void);

#endif /* IDLE_RESOURCE_CLEANER_H */
