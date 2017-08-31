/*-------------------------------------------------------------------------
 *
 * debugbreak.c
 *	  Debugging facilities
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/error/debugbreak.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/debugbreak.h"
#include "miscadmin.h"

#include <unistd.h>
/* How to use debug_break
 *
 * Greenplum DB will spawn many processes to execute a query in parallel.
 * In order to debug, one may need to first run a query that will successfully
 * create enough process (slice), attach gdb to these slices, and set break
 * points in all GDB sessions.  This is quite tedious.
 *
 * Alternatively, you can put a sleep somewhere in the code, so the process
 * running the interesting code will sleep and you will have enough time to
 * attach gdb. The problem is that it is a tedious process to "unfreeze" the
 * sleeped process, and, you cannot disable the "sleep point".
 *
 * Here is the more convinient debugging facilities.
 * Suppose you want to inspecting some code in slice 3 (from logging, or crash,
 * etc). At the point you want to break in, add the follwing
 * 
 * 		if(currentSliceId == 3) 
 * 			debug_break();
 * 			
 * Run the query, slice 3 will sleep here.  You can find the process id from 
 * log and attach debugger.  In order to "unfreeze" the process, you go to the
 * frame of debug_break, and in gdb, do,
 *
 * 		gdb> p debug_break_flag = 0
 *		gdb> c
 *
 * this will unfreeze the process.  If you just want the process keep running
 * and do not hit any debug_break any more, do
 *
 * 		gdb> p debug_break_flag = 2
 *		gdb> c
 *		
 * Now debug_break will not hit again.
 *
 * Two very good place to put debug break are,
 * 		1. ExceptionalCondition, to catch assert.
 *		2. errfinish, before PG_RE_THROW, to catch exceptions
 */

/*
 * XXX: NOTE: all the operations really should be "interlocked" ops, but
 * so far I did not run into any problem (postgres is not threaded)
 */
#ifdef USE_DEBUG_BREAK

void
debug_break()
{
	static volatile int debug_break_loop = 0; 

	++debug_break_loop; 

	if(debug_break_loop == 1)
		elog(LOG, "Debug break");

	while(debug_break_loop == 1)
    {
        CHECK_FOR_INTERRUPTS();
		sleep(1);
    }
}

static bool hit_once = false;

/*
 * debug_break_timed
 *
 * debug_break_timed inserts a sec seconds break in the execution code.
 * If singleton is true, it will only fire up once, the first time it is
 * encoutered, otherwise it will break every time.
 */
void
debug_break_timed(int sec, bool singleton)
{
	volatile int debug_break_loop;

	if (singleton && hit_once)
	{
		return;
	}

	hit_once = true;
	debug_break_loop = 0;

	++debug_break_loop;

	if(debug_break_loop == 1)
		elog(LOG, "Debug break timed");

	while(debug_break_loop++ < sec )
    {
        CHECK_FOR_INTERRUPTS();
		sleep(1);
    }
}

#define DEBUG_BREAK_N_MAX (sizeof(int) * 8)

/* by default, only enable debug_break_point 1, (Assert) */
static volatile int debug_break_n_flags = 1; 

/*
 * debug_break_n
 *
 * debug_break will turn on/off all debug_breaks, the _n version will turn
 * on/off a debug_break point controlled by number n.
 */
void
debug_break_n(int n)
{
	int checkbit = 1;
	static volatile int debug_break_n_loop = 0;

	Assert(n >= 1 && n<= DEBUG_BREAK_N_MAX);
	
	checkbit <<= (n-1);
	++debug_break_n_loop;

	if((debug_break_n_flags & checkbit) && debug_break_n_loop == 1)
		elog(LOG, "Debug break n: %d", n);

	while((debug_break_n_flags & checkbit) && debug_break_n_loop == 1)
    {
        CHECK_FOR_INTERRUPTS();
		sleep(1);
    }
}

void
enable_debug_break_n(int n)
{
	if(n == 0)
		debug_break_n_flags = ~0;
	else
	{
		int bit = 1;
		Assert(n >= 1 && n <= DEBUG_BREAK_N_MAX);
		bit <<= (n-1);
		debug_break_n_flags |= bit;
	}
}

void
disable_debug_break_n(int n)
{
	if(n == 0)
		debug_break_n_flags = 0;
	else
	{
		int bit = 1;
		Assert(n >= 1 && n <= DEBUG_BREAK_N_MAX);
		bit <<= (n-1);
		debug_break_n_flags &= ~bit;
	}
}

#endif

