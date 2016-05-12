/*-------------------------------------------------------------------------
 *
 * cdbdisp_thread.h
 * routines for multi-thread implementation of dispatching commands 
 * to the qExec processes.
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef THREADCDBDISP_H
#define THREADCDBDISP_H

#include "lib/stringinfo.h"         /* StringInfo */
#include <pthread.h>
#include "cdb/cdbdisp.h"

struct SegmentDatabaseDescriptor;   /* #include "cdb/cdbconn.h" */

/*
 * Parameter structure for the DispatchCommand threads
 */
typedef struct DispatchCommandParms
{
	char *query_text;
	int	query_text_len;

	/*
	 * db_count: The number of segdbs that this thread is responsible
	 * for dispatching the command to.
	 * Equals the count of segdbDescPtrArray below.
	 */
	int	db_count;
	

	/*
	 * dispatchResultPtrArray: Array[0..db_count-1] of CdbDispatchResult*
	 * Each CdbDispatchResult object points to a SegmentDatabaseDescriptor
	 * that this thread is responsible for dispatching the command to.
	 */
	struct CdbDispatchResult **dispatchResultPtrArray;

	/*
	 * Depending on this mode, we may send query cancel or query finish
	 * message to QE while we are waiting it to complete.  NONE means
	 * we expect QE to complete without any instruction.
	 */
	volatile DispatchWaitMode waitMode;

	/*
	 * pollfd supports for libpq
	 */
	int	nfds;
	struct pollfd *fds;
	
	/*
	 * The pthread_t thread handle.
	 */
	pthread_t thread;
	bool thread_valid;
	
} DispatchCommandParms;

/*
 * Keeps state of all the dispatch command threads.
 */
typedef struct CdbDispatchCmdThreads
{
	struct DispatchCommandParms *dispatchCommandParmsAr;
	int	dispatchCommandParmsArSize;
	int	threadCount;
}   CdbDispatchCmdThreads;

void
cdbdisp_dispatchToGang_internal(struct CdbDispatcherState *ds,
								struct Gang *gp,
								int sliceIndex,
								CdbDispatchDirectDesc *direct);

void
CdbCheckDispatchResults_internal(struct CdbDispatcherState *ds,
								 struct SegmentDatabaseDescriptor *** failedSegDB,
								 int *numOfFailed,
								 DispatchWaitMode waitMode); 

void cdbdisp_waitThreads(void);

CdbDispatchCmdThreads * cdbdisp_makeDispatchThreads(int maxSlices);

#endif
