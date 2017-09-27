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

#include "portability/instr_time.h"

struct CdbExplain_NodeSummary;          /* private def in cdb/cdbexplain.c */

/* Flag bits included in InstrAlloc's instrument_options bitmask */
typedef enum InstrumentOption
{
	INSTRUMENT_TIMER = 1 << 0,	/* needs timer (and row counts) */
	INSTRUMENT_BUFFERS = 1 << 1,	/* needs buffer usage (not implemented yet) */
	INSTRUMENT_ROWS = 1 << 2,	/* needs row count */
	INSTRUMENT_ALL = PG_INT32_MAX
} InstrumentOption;

typedef struct Instrumentation
{
	/* Parameters set at node creation: */
	bool		need_timer;	    /* TRUE if we need timer data */
	/* Info about current plan cycle: */
	bool		running;		/* TRUE if we've completed first tuple */
	instr_time	starttime;		/* Start time of current iteration of node */
	instr_time	counter;		/* Accumulated runtime for this node */
	double		firsttuple;		/* Time for first tuple of this cycle */
	double		tuplecount;		/* Tuples emitted so far this cycle */
	/* Accumulated statistics across all completed cycles: */
	double		startup;		/* Total startup time (in seconds) */
	double		total;			/* Total total time (in seconds) */
	double		ntuples;		/* Total tuples produced */
	double		nloops;			/* # of run cycles for this node */
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
extern void InstrStopNode(Instrumentation *instr, double nTuples);
extern void InstrEndLoop(Instrumentation *instr);

#endif   /* INSTRUMENT_H */
