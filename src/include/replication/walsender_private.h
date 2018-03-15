/*-------------------------------------------------------------------------
 *
 * walsender_private.h
 *	  Private definitions from replication/walsender.c.
 *
 * Portions Copyright (c) 2010-2012, PostgreSQL Global Development Group
 *
 * src/include/replication/walsender_private.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _WALSENDER_PRIVATE_H
#define _WALSENDER_PRIVATE_H

#include "access/xlog.h"
#include "nodes/nodes.h"
#include "replication/syncrep.h"
#include "storage/latch.h"
#include "storage/shmem.h"
#include "storage/spin.h"

typedef enum WalSndState
{
	WALSNDSTATE_STARTUP = 0,
	WALSNDSTATE_BACKUP,
	WALSNDSTATE_CATCHUP,
	WALSNDSTATE_STREAMING
} WalSndState;

/*
 * Each walsender has a WalSnd struct in shared memory.
 */
typedef struct WalSnd
{
	pid_t		pid;			/* this walsender's process id, or 0 */
	WalSndState state;			/* this walsender's state */
	XLogRecPtr	sentPtr;		/* WAL has been sent up to this point */
	bool		needreload;		/* does currently-open file need to be
								 * reloaded? */
	bool		sendKeepalive;	/* do we send keepalives on this connection? */

	/*
	 * The xlog locations that have been written, flushed, and applied by
	 * standby-side. These may be invalid if the standby-side has not offered
	 * values yet.
	 */
	XLogRecPtr	write;
	XLogRecPtr	flush;
	XLogRecPtr	apply;

	/*
	 * This boolean indicates if this WAL sender has caught up within the
	 * range defined by user (guc). This helps the backends to decide if they
	 * should wait in the sync rep queue, should they see a live WAL sender
	 * but that is not yet in streaming state.
	 */
	bool		caughtup_within_range;

	/*
	 * xlog location upto which xlog seg file cleanup for this walsender
	 * is allowed.
	 * In case of backup mode, it is the starting xlog ptr and
	 * in case of actual xlog replication to a standby it is the
	 * either the write/flush xlog ptr
	 *
	 * Note:- Valid only when this walsender is alive
	 */
	XLogRecPtr	xlogCleanUpTo;

	/* Protects shared variables shown above. */
	slock_t		mutex;

	/*
	 * Latch used by backends to wake up this walsender when it has work to
	 * do.
	 */
	Latch		latch;

	/*
	 * The priority order of the standby managed by this WALSender, as listed
	 * in synchronous_standby_names, or 0 if not-listed. Protected by
	 * SyncRepLock.
	 */
	int			sync_standby_priority;

	bool		synchronous;

	/*
	 * Records time when PID was set to 0, either during initialization or due
	 * to disconnection. This helps to detect time passed since mirror didn't
	 * connect.
	 */
	pg_time_t   marked_pid_zero_at_time;
} WalSnd;

extern WalSnd *MyWalSnd;

typedef enum
{
	WALSNDERROR_NONE = 0,
	WALSNDERROR_WALREAD
} WalSndError;

/* There is one WalSndCtl struct for the whole database cluster */
typedef struct
{
	/*
	 * Synchronous replication queue with one queue per request type.
	 * Protected by SyncRepLock.
	 */
	SHM_QUEUE	SyncRepQueue[NUM_SYNC_REP_WAIT_MODE];

	/*
	 * Current location of the head of the queue. All waiters should have a
	 * waitLSN that follows this value. Protected by SyncRepLock.
	 */
	XLogRecPtr	lsn[NUM_SYNC_REP_WAIT_MODE];

	/*
	 * Are any sync standbys defined?  Waiting backends can't reload the
	 * config file safely, so checkpointer updates this value as needed.
	 * Protected by SyncRepLock.
	 */
	bool		sync_standbys_defined;

	/*
	 * xlog location upto which xlog seg file cleanup is allowed.
	 * Checkpoint creation cleans old non-required xlog files. We have to
	 * preserve old files in case where the backup dump is large and the
	 * old xlog seg files are not yet dumped out OR in case the walsender
	 * has just commenced but hasn't replicated all the old xlog seg file contents.
	 *
	 * This location is obtained by comparing 'xlogCleanUpTo'
	 * set by each active walsender.
	 *
	 * Note:- Valid only when atleast one walsender is alive
	 */
	XLogRecPtr	walsnd_xlogCleanUpTo;

	/*
	 * Indicate error state of WalSender, for example, missing XLOG for mirror
	 * to stream.
	 *
	 * Note: If we want to support multiple mirrors, this data structure
	 * need to be redesigned (e.g. using WalSndError[]). We cannot store this
	 * field in the walsnds[] array below, because the walsnds[] only
	 * tracks the live wal senders. Hence, if the wal sender goes away
	 * with certain error, the error state will go away with it.
	 *
	 */
	WalSndError error;

	WalSnd		walsnds[1];		/* VARIABLE LENGTH ARRAY */
} WalSndCtlData;

extern WalSndCtlData *WalSndCtl;


extern void WalSndSetState(WalSndState state);
extern void XLogRead(char *buf, XLogRecPtr startptr, Size count);

/*
 * Internal functions for parsing the replication grammar, in repl_gram.y and
 * repl_scanner.l
 */
extern int	replication_yyparse(void);
extern int	replication_yylex(void);
extern void replication_yyerror(const char *str);
extern void replication_scanner_init(const char *query_string);
extern void replication_scanner_finish(void);

extern Node *replication_parse_result;

#endif   /* _WALSENDER_PRIVATE_H */
