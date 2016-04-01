/*-------------------------------------------------------------------------
 *
 * meminstrumentation.h
 *	  This file contains declarations for memory instrumentation utility
 *	  functions.
 *
 * Portions Copyright (c) 2013, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/meminstrumentation.h,v 1.00 2013/03/22 14:57:00 rahmaf2 Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMACCOUNTING_H
#define MEMACCOUNTING_H

#include "nodes/nodes.h"
#include "lib/stringinfo.h"             /* StringInfo */

struct MemoryContextData;

/* Macros to define the level of memory accounting to show in EXPLAIN ANALYZE */
#define EXPLAIN_MEMORY_VERBOSITY_SUPPRESS  0 /* Suppress memory reporting in explain analyze */
#define EXPLAIN_MEMORY_VERBOSITY_SUMMARY  1 /* Summary of memory usage for each owner in explain analyze */
#define EXPLAIN_MEMORY_VERBOSITY_DETAIL  2 /* Detail memory accounting tree for each slice in explain analyze */

/*
 * What level of details of the memory accounting information to show during EXPLAIN ANALYZE?
 */
extern int explain_memory_verbosity;

/*
 * Unique run id for memory profiling. May be just a start timestamp for a batch of queries such as TPCH
 */
extern char* memory_profiler_run_id;

/*
 * Dataset ID. Determined by the external script. One example could be, 1: TPCH, 2: TPCDS etc.
 */
extern char* memory_profiler_dataset_id;

/*
 * Which query of the query suite is running currently. E.g., query 21 of TPCH
 */
extern char* memory_profiler_query_id;

/*
 * Scale factor of TPCH/TPCDS etc.
 */
extern int memory_profiler_dataset_size;

/*
 * Should we save the memory usage information before resetting the memory accounting?
 */
extern bool gp_dump_memory_usage;

/*
 * Each memory account can assume one of the following memory
 * owner types
 */
typedef enum MemoryOwnerType
{
	/* Long-living accounts that survive reset */
	MEMORY_OWNER_TYPE_LogicalRoot = 10,
	MEMORY_OWNER_TYPE_SharedChunkHeader = 11,
	MEMORY_OWNER_TYPE_Rollover = 12,
	MEMORY_OWNER_TYPE_MemAccount = 13,
	/* End of long-living accounts */

	/* Short-living accounts */
	MEMORY_OWNER_TYPE_Top = 101,
	MEMORY_OWNER_TYPE_MainEntry = 102,
	MEMORY_OWNER_TYPE_Parser = 103,
	MEMORY_OWNER_TYPE_Planner = 104,
	MEMORY_OWNER_TYPE_Optimizer = 105,
	MEMORY_OWNER_TYPE_Dispatcher = 106,
	MEMORY_OWNER_TYPE_Serializer = 107,
	MEMORY_OWNER_TYPE_Deserializer = 108,

	MEMORY_OWNER_TYPE_EXECUTOR = 1000,
	MEMORY_OWNER_TYPE_Exec_Result = 1001,
	MEMORY_OWNER_TYPE_Exec_Append = 1002,
	MEMORY_OWNER_TYPE_Exec_Sequence = 1003,
	MEMORY_OWNER_TYPE_Exec_BitmapAnd = 1004,
	MEMORY_OWNER_TYPE_Exec_BitmapOr = 1005,
	MEMORY_OWNER_TYPE_Exec_SeqScan = 1006,
	MEMORY_OWNER_TYPE_Exec_ExternalScan = 1007,
	MEMORY_OWNER_TYPE_Exec_AppendOnlyScan = 1008,
	MEMORY_OWNER_TYPE_Exec_AOCSScan = 1009,
	MEMORY_OWNER_TYPE_Exec_TableScan = 1010,
	MEMORY_OWNER_TYPE_Exec_DynamicTableScan = 1011,
	MEMORY_OWNER_TYPE_Exec_IndexScan = 1012,
	MEMORY_OWNER_TYPE_Exec_DynamicIndexScan = 1013,
	MEMORY_OWNER_TYPE_Exec_BitmapIndexScan = 1014,
	MEMORY_OWNER_TYPE_Exec_BitmapHeapScan = 1015,
	MEMORY_OWNER_TYPE_Exec_BitmapAppendOnlyScan = 1016,
	MEMORY_OWNER_TYPE_Exec_TidScan = 1017,
	MEMORY_OWNER_TYPE_Exec_SubqueryScan = 1018,
	MEMORY_OWNER_TYPE_Exec_FunctionScan = 1019,
	MEMORY_OWNER_TYPE_Exec_TableFunctionScan = 1020,
	MEMORY_OWNER_TYPE_Exec_ValuesScan = 1021,
	MEMORY_OWNER_TYPE_Exec_NestLoop = 1022,
	MEMORY_OWNER_TYPE_Exec_MergeJoin = 1023,
	MEMORY_OWNER_TYPE_Exec_HashJoin = 1024,
	MEMORY_OWNER_TYPE_Exec_Material = 1025,
	MEMORY_OWNER_TYPE_Exec_Sort = 1026,
	MEMORY_OWNER_TYPE_Exec_Agg = 1027,
	MEMORY_OWNER_TYPE_Exec_Unique = 1028,
	MEMORY_OWNER_TYPE_Exec_Hash = 1029,
	MEMORY_OWNER_TYPE_Exec_SetOp = 1030,
	MEMORY_OWNER_TYPE_Exec_Limit = 1031,
	MEMORY_OWNER_TYPE_Exec_Motion = 1032,
	MEMORY_OWNER_TYPE_Exec_ShareInputScan = 1033,
	MEMORY_OWNER_TYPE_Exec_Window = 1034,
	MEMORY_OWNER_TYPE_Exec_Repeat = 1035,
	MEMORY_OWNER_TYPE_Exec_DML = 1036,
	MEMORY_OWNER_TYPE_Exec_SplitUpdate = 1037,
	MEMORY_OWNER_TYPE_Exec_RowTrigger = 1038,
	MEMORY_OWNER_TYPE_Exec_AssertOp = 1039,
	MEMORY_OWNER_TYPE_Exec_AlienShared = 1040,
	MEMORY_OWNER_TYPE_Exec_BitmapTableScan = 1041,
	MEMORY_OWNER_TYPE_Exec_PartitionSelector = 1042,
	MEMORY_OWNER_TYPE_Exec_Plan_End, /* No explicit number. Automatically gets the last of executor enumeration */

} MemoryOwnerType;

/****
 * The following are constants to define additional memory stats
 * (in addition to memory accounts) during CSV dump of memory balance
 */
/* vmem reserved from memprot.c */
#define MEMORY_STAT_TYPE_VMEM_RESERVED -1
/* Peak memory observed from inside memory accounting among all allocations */
#define MEMORY_STAT_TYPE_MEMORY_ACCOUNTING_PEAK -2
/***************************************************************************/

struct MemoryAccount;

extern struct MemoryAccount* ActiveMemoryAccount;
extern struct MemoryAccount* RolloverMemoryAccount;
extern struct MemoryAccount* AlienExecutorMemoryAccount;
extern struct MemoryAccount* SharedChunkHeadersMemoryAccount;

extern uint64 MemoryAccountingOutstandingBalance;
extern uint64 MemoryAccountingPeakBalance;

extern uint16 MemoryAccountingCurrentGeneration;

/* MemoryAccount is the fundamental data structure to record memory usage */
typedef struct MemoryAccount {
	NodeTag type;
	MemoryOwnerType ownerType;

	uint64 allocated;
	uint64 freed;
	uint64 peak;
	/*
	 * Maximum targeted allocation for an owner. Peak usage can be tracked to
	 * check if the allocation is overshooting
	 */
	uint64 maxLimit;

	/* If this account requests the privilege to "disown" some of the allocation
	 * that would otherwise be charged to its account, then disownedMemoryAccount
	 * holds a hidden account which will be used for allocation during a "disowned"
	 * state (upon calling "DisownMemoryAccount()").
	 */
	//struct MemoryAccount *disownedMemoryAccount;
	struct MemoryAccount *parentAccount;

	/*
	 * To traverse the tree of MemoryAccount, start from the Top. Read the firstChild and then read the
	 * nextSibling of the firstChild. And continue this process. For each of the nextSibling, you can call
	 * firstChild to descend the tree.
	 */
	struct MemoryAccount* firstChild;
	struct MemoryAccount* nextSibling;
} MemoryAccount;

/*
 * Instead of pointers to construct the tree, the SerializedMemoryAccount
 * uses "serial" of each node and saves parent serial to construct the tree.
 * This is required as we cannot serialize pointers. As an optimization, we
 * can later on try to reuse the pointers themselves and treat them as integer
 * to save the "serial"
 */
typedef struct SerializedMemoryAccount {
	NodeTag type;
	MemoryAccount memoryAccount;

	/*
	 * memoryAccountSerial and parentMemoryAccountSerial are used for serializing
	 * MemoryAccount. Note: we cannot serialize the tree using the pointers.
	 * Instead we serialize these "serial" and "parent serial" and construct the
	 * tree at the destination (e.g., dispatcher or any reporting tool).
	 */
	uint32 memoryAccountSerial;
	/* If memoryAccountSerial == parentMemoryAccountSerial, then the node has NO parent */
	uint32 parentMemoryAccountSerial;
} SerializedMemoryAccount;

/*
 * START_MEMORY_ACCOUNT would switch to the specified newMemoryAccount,
 * saving the oldActiveMemoryAccount. Must be paired with END_MEMORY_ACCOUNT
 */
#define START_MEMORY_ACCOUNT(newMemoryAccount)  \
	do { \
		MemoryAccount *oldActiveMemoryAccount = NULL; \
		Assert(newMemoryAccount != NULL); \
		oldActiveMemoryAccount = ActiveMemoryAccount; \
		ActiveMemoryAccount = newMemoryAccount;\
/*
 * END_MEMORY_ACCOUNT would restore the previous memory account that was
 * active at the time of START_MEMORY_ACCCOUNT call
 */
#define END_MEMORY_ACCOUNT()  \
		ActiveMemoryAccount = oldActiveMemoryAccount;\
	} while (0);

/*
 * CREATE_EXECUTOR_MEMORY_ACCOUNT is a convenience macro to create a new
 * operator specific memory account *if* the operator will be executed in
 * the current slice, i.e., it is not part of some other slice (alien
 * plan node). We assign a shared AlienExecutorMemoryAccount for plan nodes
 * that will not be executed in current slice
 */
#define CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, planNode, NodeType) \
	(isAlienPlanNode ? AlienExecutorMemoryAccount :						\
	 MemoryAccounting_CreateAccount(((Plan*)node)->operatorMemKB == 0 ? \
									work_mem : ((Plan*)node)->operatorMemKB, MEMORY_OWNER_TYPE_Exec_##NodeType))

/*
 * SAVE_EXECUTOR_MEMORY_ACCOUNT saves an operator specific memory account
 * into the PlanState of that operator
 */
#define SAVE_EXECUTOR_MEMORY_ACCOUNT(execState, curMemoryAccount)\
	do { \
		Assert(NULL == ((PlanState *)execState)->memoryAccount || \
			   AlienExecutorMemoryAccount == ((PlanState *)execState)->memoryAccount || \
			   curMemoryAccount == ((PlanState *)execState)->memoryAccount); \
		((PlanState *)execState)->memoryAccount = curMemoryAccount; \
	} while(0)

extern struct MemoryAccount*
MemoryAccounting_CreateAccount(long maxLimit, enum MemoryOwnerType ownerType);

extern MemoryAccount*
MemoryAccounting_SwitchAccount(struct MemoryAccount* desiredAccount);

extern void
MemoryAccounting_Reset(void);

extern void
MemoryAccounting_ResetPeakBalance(void);

extern uint32
MemoryAccounting_Serialize(StringInfoData* buffer);

extern SerializedMemoryAccount*
MemoryAccounting_Deserialize(const void *serializedBits,
		uint32 memoryAccountCount);

extern uint64
MemoryAccounting_GetPeak(MemoryAccount *memoryAccount);

extern uint64
MemoryAccounting_GetBalance(MemoryAccount *memoryAccount);

extern void
MemoryAccounting_ToString(MemoryAccount *root, StringInfoData *str,
		uint32 indentation);

extern void
MemoryAccounting_SaveToFile(int currentSliceId);

extern uint32
MemoryAccounting_SaveToLog(void);

extern const char*
MemoryAccounting_GetAccountName(MemoryAccount *memoryAccount);

extern void
MemoryAccounting_ToCSV(MemoryAccount *root, StringInfoData *str, char *prefix);

extern void
MemoryAccounting_PrettyPrint(void);
/*
 * MemoryAccountIsValid
 *		True iff memory account is valid.
 */
#define MemoryAccountIsValid(memoryAccount) \
	((memoryAccount) != NULL && \
	 ( IsA((memoryAccount), MemoryAccount) ))

#endif   /* MEMACCOUNTING_H */
