/* ----------
 *	pg_trace.h
 *
 *	Definitions for the PostgreSQL tracing framework
 *
 *	Copyright (c) 2006-2008, PostgreSQL Global Development Group
 *
 *	$PostgreSQL: pgsql/src/include/pg_trace.h,v 1.3 2008/01/02 02:42:06 momjian Exp $
 * ----------
 */

#ifndef PG_TRACE_H
#define PG_TRACE_H

#ifdef ENABLE_DTRACE
#define PGTRACE_ENABLED

#include <sys/sdt.h>

/*
 * The PG_TRACE macros are mapped to the appropriate macros used by DTrace.
 *
 * Only one DTrace provider called "postgresql" will be used for PostgreSQL,
 * so the name is hard-coded here to avoid having to specify it in the
 * source code.
 */

#define PG_TRACE(name) \
	DTRACE_PROBE(postgresql, name)
#define PG_TRACE1(name, arg1) \
	DTRACE_PROBE1(postgresql, name, arg1)
#define PG_TRACE2(name, arg1, arg2) \
	DTRACE_PROBE2(postgresql, name, arg1, arg2)
#define PG_TRACE3(name, arg1, arg2, arg3) \
	DTRACE_PROBE3(postgresql, name, arg1, arg2, arg3)
#define PG_TRACE4(name, arg1, arg2, arg3, arg4) \
	DTRACE_PROBE4(postgresql, name, arg1, arg2, arg3, arg4)
#define PG_TRACE5(name, arg1, arg2, arg3, arg4, arg5) \
	DTRACE_PROBE5(postgresql, name, arg1, arg2, arg3, arg4, arg5)

#define LTRACE_PROBE(family, fn, fc, l, i1, i2, i3, i4, i5)

#else
#ifdef ENABLE_LTRACE
#define PGTRACE_ENABLED
enum 
{
	/* Postgres dtrace family, 1 to 999,999 */
	/* See src/backend/utils/probes.d */
	transaction__start  = 1, 
	transaction__commit,
	transaction__abort,
	lwlock__acquire,
	lwlock__release,
	lwlock__startwait,
	lwlock__endwait,
	lwlock__condacquire,
	lwlock__condacquire__fail,
	lock__startwait,
	lock__endwait,

	memctxt__alloc,
	memctxt__free,
	memctxt__realloc,

	execprocnode__enter,
	execprocnode__exit,

	tuplesort__begin,
	tuplesort__end,
	tuplesort__perform__sort,
	tuplesort__mergeonerun,
	tuplesort__dumptuples,
	tuplesort__switch__external,

	/* Internal test and debug use */
	LTR_DEBUG_START = 1000000,

	/* Internal test and debug, very verbose */
	LTR_DEBUG_VERBOSE = 2000000,
};

extern int gp_ltrace_flag;
extern void LTRACE_PROBE_FIRE(long family, 
	const char *filen, const char *func, long line,
	long i1, long i2, long i3, long i4, long i5
	);
static inline void LTRACE_PROBE(long family, 
	const char *filen, const char *func, long line,
	long i1, long i2, long i3, long i4, long i5
	)
{
	if(family < gp_ltrace_flag)
		LTRACE_PROBE_FIRE(family, filen, func, line,
				i1, i2, i3, i4, i5);
}

#define PG_TRACE(name) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, 0, 0, 0, 0, 0)
#define PG_TRACE1(name, arg1) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, arg1, 0, 0, 0, 0)
#define PG_TRACE2(name, arg1, arg2) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, arg1, arg2, 0, 0, 0)
#define PG_TRACE3(name, arg1, arg2, arg3) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, arg1, arg2, arg3, 0, 0)
#define PG_TRACE4(name, arg1, arg2, arg3, arg4) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, arg1, arg2, arg3, arg4, 0)
#define PG_TRACE5(name, arg1, arg2, arg3, arg4, arg5) \
	LTRACE_PROBE(name, __FILE__, PG_FUNCNAME_MACRO, __LINE__, arg1, arg2, arg3, arg4, arg5)

#else   /* No tracing */
/*
 * Unless DTrace is explicitly enabled with --enable-dtrace, the PG_TRACE
 * macros will expand to no-ops.
 */

#define PG_TRACE(name)
#define PG_TRACE1(name, arg1)
#define PG_TRACE2(name, arg1, arg2)
#define PG_TRACE3(name, arg1, arg2, arg3)
#define PG_TRACE4(name, arg1, arg2, arg3, arg4)
#define PG_TRACE5(name, arg1, arg2, arg3, arg4, arg5)
#define LTRACE_PROBE(family, fn, fc, l, i1, i2, i3, i4, i5)

#endif   /* not ENABLE_LTRACE */
#endif   /* not ENABLE_DTRACE */
#endif   /* PG_TRACE_H */
