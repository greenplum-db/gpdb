/*-------------------------------------------------------------------------
 *
 * memquota.h
 *	  Routines related to memory quota for queries.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/memquota.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMQUOTA_H_
#define MEMQUOTA_H_

#include "nodes/plannodes.h"
#include "cdb/cdbplan.h"

typedef enum ResManagerMemoryPolicy
{
	RESMANAGER_MEMORY_POLICY_NONE,
	RESMANAGER_MEMORY_POLICY_AUTO,
	RESMANAGER_MEMORY_POLICY_EAGER_FREE
} ResManagerMemoryPolicy;

extern ResManagerMemoryPolicy gp_resmanager_memory_policy_default;
extern bool						gp_log_resmanager_memory_default;
extern int						gp_resmanager_memory_policy_auto_fixed_mem_default;
extern bool						gp_resmanager_print_operator_memory_limits_default;

extern ResManagerMemoryPolicy	*gp_resmanager_memory_policy;
extern bool						*gp_log_resmanager_memory;
extern int						*gp_resmanager_memory_policy_auto_fixed_mem;
extern bool						*gp_resmanager_print_operator_memory_limits;

#define GP_RESMANAGER_MEMORY_LOG_LEVEL NOTICE

#define IsResManagerMemoryPolicyNone() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_NONE)
#define IsResManagerMemoryPolicyAuto() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_AUTO)
#define IsResManagerMemoryPolicyEagerFree() (*gp_resmanager_memory_policy == RESMANAGER_MEMORY_POLICY_EAGER_FREE)

#define LogResManagerMemory() (*gp_log_resmanager_memory == true)
#define ResManagerPrintOperatorMemoryLimits() (*gp_resmanager_print_operator_memory_limits == true)

extern void PolicyAutoAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);
extern void PolicyEagerFreeAssignOperatorMemoryKB(PlannedStmt *stmt, uint64 memoryAvailable);

/**
 * Inverse for explain analyze.
 */
extern uint64 PolicyAutoStatementMemForNoSpillKB(PlannedStmt *stmt, uint64 minOperatorMemKB);

/**
 * Is result node memory intensive?
 */
extern bool IsResultMemoryIntesive(Result *res);

/**
 * Is operator memory intensive
 */
extern bool IsMemoryIntensiveOperator(Node *node, PlannedStmt *stmt);

/*
 * Calculate the amount of memory reserved for the query
 */
extern int64 ResourceManagerGetQueryMemoryLimit(PlannedStmt* stmt);

#endif /* MEMQUOTA_H_ */
