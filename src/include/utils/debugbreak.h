/*-------------------------------------------------------------------------
 *
 * debugbreak.h
 * 		Debugging facilities
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/debugbreak.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef _DEBUG_BREAK_H_
#define _DEBUG_BREAK_H_

#define DEBUG_BREAK_POINT_ASSERT 		(1)
#define DEBUG_BREAK_POINT_EXCEPTION 	(1 << 1)

#ifdef USE_DEBUG_BREAK
extern void debug_break(void);
extern void debug_break_n(int n);
extern void enable_debug_break_n(int n);
extern void disable_debug_break_n(int n);
extern void debug_break_timed(int sec, bool singleton);

#else
#define debug_break()
#define debug_break_n(n)
#define enable_debug_break_n(n)
#define disable_debug_break_n(n)
#define debug_break_timed(sec, singleton);
#endif

#endif /* _DEBUG_BREAK_H_ */


