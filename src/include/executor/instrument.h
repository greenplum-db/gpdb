/*-------------------------------------------------------------------------
 *
 * instrument.h
 *	  definitions for run-time statistics collection
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 * src/include/executor/instrument.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "nodes/plannodes.h"
#include "portability/instr_time.h"
#include "utils/resowner.h"
#include "storage/s_lock.h"

struct CdbExplain_NodeSummary;          /* private def in cdb/cdbexplain.c */

typedef struct BufferUsage
{
	long		shared_blks_hit;	/* # of shared buffer hits */
	long		shared_blks_read;	/* # of shared disk blocks read */
	long		shared_blks_dirtied;	/* # of shared blocks dirtied */
	long		shared_blks_written;	/* # of shared disk blocks written */
	long		local_blks_hit; /* # of local buffer hits */
	long		local_blks_read;	/* # of local disk blocks read */
	long		local_blks_dirtied; /* # of shared blocks dirtied */
	long		local_blks_written; /* # of local disk blocks written */
	long		temp_blks_read; /* # of temp blocks read */
	long		temp_blks_written;	/* # of temp blocks written */
	instr_time	blk_read_time;	/* time spent reading */
	instr_time	blk_write_time; /* time spent writing */
} BufferUsage;

/* Flag bits included in InstrAlloc's instrument_options bitmask */
typedef enum InstrumentOption
{
	INSTRUMENT_NONE = 0,
	INSTRUMENT_TIMER = 1 << 0,	/* needs timer (and row counts) */
	INSTRUMENT_BUFFERS = 1 << 1,	/* needs buffer usage (not implemented yet) */
	INSTRUMENT_ROWS = 1 << 2,	/* needs row count */
	INSTRUMENT_MEMORY_DETAIL = 0x20000000,	/* needs detailed memory accounting */
	INSTRUMENT_CDB = 0x40000000,	/* needs cdb statistics */
	INSTRUMENT_ALL = PG_INT32_MAX
} InstrumentOption;

typedef struct Instrumentation
{
	/* Parameters set at node creation: */
	bool		need_timer;		/* true if we need timer data */
	bool		need_cdb;		/* true if we need cdb statistics */
	bool		need_bufusage;	/* true if we need buffer usage data */
	/* Info about current plan cycle: */
	bool		running;		/* true if we've completed first tuple */
	instr_time	starttime;		/* Start time of current iteration of node */
	instr_time	counter;		/* Accumulated runtime for this node */
	double		firsttuple;		/* Time for first tuple of this cycle */
	uint64		tuplecount;		/* Tuples emitted so far this cycle */
	BufferUsage	bufusage_start;	/* Buffer usage at start */
	/* Accumulated statistics across all completed cycles: */
	double		startup;		/* Total startup time (in seconds) */
	double		total;			/* Total total time (in seconds) */
	uint64		ntuples;		/* Total tuples produced */
	// GPDB_12_MERGE_FIXME: we had changed 'ntuples' and 'nloops' to uint64. Do the same
	// for 'ntuples2', or?
	double		ntuples2;		/* Secondary node-specific tuple counter */
	uint64		nloops;			/* # of run cycles for this node */
	double		nfiltered1;		/* # tuples removed by scanqual or joinqual */
	double		nfiltered2;		/* # tuples removed by "other" quals */
	BufferUsage	bufusage;		/* Total buffer usage */

	double		execmemused;	/* CDB: executor memory used (bytes) */
	double		workmemused;	/* CDB: work_mem actually used (bytes) */
	double		workmemwanted;	/* CDB: work_mem to avoid scratch i/o (bytes) */
	instr_time	firststart;		/* CDB: Start time of first iteration of node */
	bool		workfileCreated;	/* TRUE if workfiles are created in this
									 * node */
	int			numPartScanned; /* Number of part tables scanned */
	const char *sortMethod;		/* CDB: Type of sort */
	const char *sortSpaceType;	/* CDB: Sort space type (Memory / Disk) */
	long		sortSpaceUsed;	/* CDB: Memory / Disk used by sort(KBytes) */
	struct CdbExplain_NodeSummary *cdbNodeSummary;	/* stats from all qExecs */
} Instrumentation;

typedef struct WorkerInstrumentation
{
	int			num_workers;	/* # of structures that follow */
	Instrumentation instrument[FLEXIBLE_ARRAY_MEMBER];
} WorkerInstrumentation;

extern PGDLLIMPORT BufferUsage pgBufferUsage;

extern Instrumentation *InstrAlloc(int n, int instrument_options);
extern void InstrInit(Instrumentation *instr, int instrument_options);
extern void InstrStartNode(Instrumentation *instr);
extern void InstrStopNode(Instrumentation *instr, uint64 nTuples);
extern void InstrEndLoop(Instrumentation *instr);
extern void InstrAggNode(Instrumentation *dst, Instrumentation *add);
extern void InstrStartParallelQuery(void);
extern void InstrEndParallelQuery(BufferUsage *result);
extern void InstrAccumParallelQuery(BufferUsage *result);

#define GP_INSTRUMENT_OPTS (gp_enable_query_metrics ? INSTRUMENT_ROWS : INSTRUMENT_NONE)

/* Greenplum query metrics */
typedef struct InstrumentationHeader
{
	void	   *head;
	int			free;
	slock_t		lock;
} InstrumentationHeader;

typedef struct InstrumentationSlot
{
	Instrumentation data;
	int32		pid;			/* process id */
	int32		tmid;			/* transaction time */
	int32		ssid;			/* session id */
	int32		ccnt;			/* command count */
	int16		segid;			/* segment id */
	int16		nid;			/* node id */
} InstrumentationSlot;

/*
 * To guarantee the slot recycled properly,
 * record the slot with its resource owner when picked
 */
typedef struct InstrumentationResownerSet
{
	InstrumentationSlot *slot;
	ResourceOwner owner;
	struct InstrumentationResownerSet *next;
} InstrumentationResownerSet;

extern InstrumentationHeader *InstrumentGlobal;
extern Size InstrShmemNumSlots(void);
extern Size InstrShmemSize(void);
extern void InstrShmemInit(void);
extern Instrumentation *GpInstrAlloc(const Plan *node, int instrument_options);

/*
 * For each free slot in shmem, fill it with specific pattern
 * Use this pattern to detect the slot has been recycled.
 * Also protect writes outside the allocated shmem buffer.
 */
#define PATTERN 0xd5
#define LONG_PATTERN 0xd5d5d5d5d5d5d5d5

/*
 * Empty if first 8 bytes of slot filled with pattern.
 */
#define SlotIsEmpty(slot) ((*((int64 *)(slot)) ^ LONG_PATTERN) == 0)

/*
 * The last 8 bytes of slot points to next free slot.
 */
#define GetInstrumentNext(slot) (*((InstrumentationSlot **)((slot) + 1) - 1))

/*
 * Limit the maximum scan node's instr per query in shmem
 */
#define MAX_SCAN_ON_SHMEM 300

#endif							/* INSTRUMENT_H */
