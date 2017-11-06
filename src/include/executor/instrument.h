/*-------------------------------------------------------------------------
 *
 * instrument.h
 *	  definitions for run-time statistics collection
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Copyright (c) 2001-2009, PostgreSQL Global Development Group
 *
 * $PostgreSQL: pgsql/src/include/executor/instrument.h,v 1.18 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "executor/executor.h"
#include "nodes/plannodes.h"
#include "portability/instr_time.h"
#include "utils/resowner.h"

struct CdbExplain_NodeSummary;          /* private def in cdb/cdbexplain.c */

/* Flag bits included in InstrAlloc's instrument_options bitmask */
typedef enum InstrumentOption
{
	INSTRUMENT_TIMER = 1 << 0,	/* needs timer (and row counts) */
	INSTRUMENT_BUFFERS = 1 << 1,	/* needs buffer usage (not implemented yet) */
	INSTRUMENT_ROWS = 1 << 2,	/* needs row count */
	INSTRUMENT_CDB = 1 << 3,	/* needs cdb statistics */
	INSTRUMENT_ALL = PG_INT32_MAX
} InstrumentOption;

typedef struct Instrumentation
{
	/* Parameters set at node creation: */
	bool		need_timer;	    /* TRUE if we need timer data */
	bool		need_cdb;	    /* TRUE if we need cdb statistics */
	bool		in_shmem;		/* TRUE if this instrument in alloced in shmem, used for recycle */
	/* Info about current plan cycle: */
	bool		running;		/* TRUE if we've completed first tuple */
	instr_time	starttime;		/* Start time of current iteration of node */
	instr_time	counter;		/* Accumulated runtime for this node */
	double		firsttuple;		/* Time for first tuple of this cycle */
	uint64		tuplecount;		/* Tuples emitted so far this cycle */
	/* Accumulated statistics across all completed cycles: */
	double		startup;		/* Total startup time (in seconds) */
	double		total;			/* Total total time (in seconds) */
	uint64		ntuples;		/* Total tuples produced */
	uint64		nloops;			/* # of run cycles for this node */
    double		execmemused;    /* CDB: executor memory used (bytes) */
    double		workmemused;    /* CDB: work_mem actually used (bytes) */
    double		workmemwanted;  /* CDB: work_mem to avoid scratch i/o (bytes) */
	instr_time	firststart;		/* CDB: Start time of first iteration of node */
	bool		workfileCreated;/* TRUE if workfiles are created in this node */
	int		numPartScanned; /* Number of part tables scanned */
	const char* sortMethod;	/* CDB: Type of sort */
	const char* sortSpaceType; /*CDB: Sort space type (Memory / Disk) */
	long			  sortSpaceUsed; /* CDB: Memory / Disk used by sort(KBytes) */
    struct CdbExplain_NodeSummary  *cdbNodeSummary; /* stats from all qExecs */
} Instrumentation;

extern Instrumentation *InstrAlloc(int n, int instrument_options);
extern void InstrStartNode(Instrumentation *instr);
extern void InstrStopNode(Instrumentation *instr, uint64 nTuples);
extern void InstrEndLoop(Instrumentation *instr);

#define INSTR_START_NODE(instr) do {											\
	if ((instr)->need_timer) {													\
		if (INSTR_TIME_IS_ZERO((instr)->starttime))								\
			INSTR_TIME_SET_CURRENT((instr)->starttime);							\
		else																	\
			elog(DEBUG2, "INSTR_START_NODE called twice in a row");				\
	}																			\
} while(0)

#define INSTR_STOP_NODE(instr, nTuples) do {									\
	(instr)->tuplecount += (nTuples);											\
	if ((instr)->need_timer)													\
	{																			\
		instr_time endtime;														\
		if (INSTR_TIME_IS_ZERO((instr)->starttime))								\
		{																		\
			elog(DEBUG2, "INSTR_STOP_NODE called without start");				\
			break;																\
		}																		\
		INSTR_TIME_SET_CURRENT(endtime);										\
		INSTR_TIME_ACCUM_DIFF((instr)->counter, endtime, (instr)->starttime);	\
		INSTR_TIME_SET_ZERO((instr)->starttime);								\
	}																			\
	if (!(instr)->running)														\
	{																			\
		(instr)->running = true;												\
		(instr)->firsttuple = INSTR_TIME_GET_DOUBLE((instr)->counter);			\
		(instr)->firststart = (instr)->starttime;								\
	}																			\
} while(0)

/* GPDB query metrics */
typedef struct InstrumentationHeader
{
	void *head;
	int used;
	int free;
	slock_t	lock;
} InstrumentationHeader;

typedef struct InstrumentationSlot
{
	Instrumentation data;
	int32 eflags;			/* executor flags */
	int32 pid;				/* process id */
	int32 tmid;				/* transaction time */
	int32 ssid; 			/* session id */
	int16 ccnt;				/* command count */
	int16 segid;			/* segment id */
	int16 nid;				/* node id */
} InstrumentationSlot;

extern InstrumentationHeader *InstrumentGlobal;
extern int scan_node_counter;
extern Size InstrShmemSize(void);
extern void InstrShmemInit(void);
extern Instrumentation *InstrShmemPick(Plan *plan, int eflags, int instrument_options);
extern void InstrShmemRecycleCallback(ResourceReleasePhase phase, bool isCommit, bool isTopLevel, void *arg);
#define GetInstrumentNext(ptr) (*((InstrumentationSlot **)(ptr+1) - 1))
#define MAX_SCAN_ON_SHMEM 300

#endif   /* INSTRUMENT_H */
