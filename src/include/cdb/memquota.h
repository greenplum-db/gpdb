/*-------------------------------------------------------------------------
 *
 * memquota.h
 * Routines related to memory quota for queries.
 *
 * Copyright (c) 2005-2010, Greenplum inc
 *-------------------------------------------------------------------------*/
#ifndef MEMQUOTA_H_
#define MEMQUOTA_H_

#include "nodes/plannodes.h"
#include "cdb/cdbplan.h"

typedef enum ResQueueMemoryPolicy
{
	RESQUEUE_MEMORY_POLICY_NONE,
	RESQUEUE_MEMORY_POLICY_AUTO,
	RESQUEUE_MEMORY_POLICY_EAGER_FREE
} ResQueueMemoryPolicy;

extern char                		*gp_resqueue_memory_policy_str;
extern ResQueueMemoryPolicy		gp_resqueue_memory_policy;
extern bool						gp_log_resqueue_memory;
extern int						gp_resqueue_memory_policy_auto_fixed_mem;
extern const int				gp_resqueue_memory_log_level;
extern bool						gp_resqueue_print_operator_memory_limits;

extern void PolicyAutoAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);
extern void PolicyEagerFreeAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);

/**
 * What is the memory limit on a queue per the catalog?
 */
extern int64 ResourceQueueGetMemoryLimitInCatalog(Oid queueId);

/**
 * What is the memory limit for a specific queue?
 */
extern int64 ResourceQueueGetMemoryLimit(Oid queueId);

/**
 * What is the memory limit for a query on a specific queue?
 */
extern uint64 ResourceQueueGetQueryMemoryLimit(PlannedStmt *stmt, Oid queueId);

/**
 * What is the memory reservation for superuser queries?
 */
extern uint64 ResourceQueueGetSuperuserQueryMemoryLimit(void);

/**
 * Inverse for explain analyze.
 */
extern uint64 PolicyAutoStatementMemForNoSpillKB(PlannedStmt *stmt, uint64 minOperatorMemKB);

/**
 * Is result node memory intensive?
 */
extern bool IsResultMemoryIntesive(Result *res);

#endif /* MEMQUOTA_H_ */
