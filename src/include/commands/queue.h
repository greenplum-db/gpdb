/*-------------------------------------------------------------------------
 *
 * queue.h
 *	  Commands for manipulating resource queues.
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/commands/queue.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef QUEUE_H
#define QUEUE_H

#include "nodes/parsenodes.h"


extern List * 
GetResqueueCapabilityEntry(Oid  queueid);
extern void CreateQueue(CreateQueueStmt *stmt);
extern void AlterQueue(AlterQueueStmt *stmt);
extern void DropQueue(DropQueueStmt *stmt);
extern Oid get_resqueue_oid(const char *queuename, bool missing_ok);

#endif   /* QUEUE_H */
