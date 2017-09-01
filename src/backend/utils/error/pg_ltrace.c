/*-------------------------------------------------------------------------
 *
 * pg_ltrace.c
 *	  Poor linux's tracing facility.
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/error/pg_ltrace.c
 *-------------------------------------------------------------------------
 */

#include <pg_config.h>
#ifdef ENABLE_LTRACE
#include <sys/types.h>

#include <unistd.h>
#include <pg_trace.h>
#include <stdio.h>

extern int gp_ltrace_flag;

struct pg_trace_data_t
{
	long val[9];
};

/* 
 * Poor linux tracing facility.
 *
 * Unlike dtrace, linux tracing (SystemTap) at this moment
 * does not support user space tracing.  We use the following
 * HACK to get some tracing.
 *
 * The write is strange.  Postgres will redirect fd 0 (stdin) to 
 * read from /dev/null.  We are writing to stdin, a string of 0 bytes.
 * it will fail, as stdin is readonly, but, it is still a system call.
 * In system tap, we can catch at syscall.write and test $fd == 0 and 
 * $count == 0.  The constructed buffer can be accessed by $buf.
 *
 * Note that the $fd = 0 and $count = 0 hack is kind of "Needed".  
 * If the fd is not valid, then actually the systemtap syscalls tapset
 * will not catch it.  Count set to 0 is safe.
 *
 * NOTE: At this moment, SystemTap cannot even display user stack.
 * if we really want the stack, we can construct the stack using backtrace()
 * and put the string as the str arg.
 * 
 * filename and funcname can be used a quick dirty depth 1 stack.
 *
 * The stp script need to match the t.val layout.  Right now, 
 * 	0 	family
 *	1 	filename
 *	2 	funcname
 *	3 	line number
 *	4-9 	5 args
 */

#if 0 
---------------------  BEGIN EXAMPLE STP ---------------------------------
#! /usr/bin/env stap

/* 
 * Example stap.  The stp must be run as root because it uses embeded C.
 *
 * sudo stap -g my.stp
 */

function pgtrace_decode_val:long(pgtrace_data:long, fieldnum:long)
%{
	struct pg_trace_data_t {
		long iv[9];
	};

	struct pg_trace_data_t *p = (struct pg_trace_data_t *) (long) THIS->pgtrace_data;
	if(p && THIS->fieldnum >= 0 && THIS->fieldnum < 9) 
		THIS->__retvalue = kread(&(p->iv[THIS->fieldnum]));
	else
		THIS->__retvalue = 0;

	CATCH_DEREF_FAULT();
%}

probe syscall.write {
	/* We have $fd, $count, and $buf.  Need to access $buf with user_string($buf) */
	if($fd == 0 && $count == 0)
	{
		printf("PGTrace family %d, file %s, func %s line %d: args: %d, %d, %d, %d, %d\n", 
				pgtrace_decode_val($buf, 0),
				user_string(pgtrace_decode_val($buf, 1)),
				user_string(pgtrace_decode_val($buf, 2)),
				pgtrace_decode_val($buf, 3),
				pgtrace_decode_val($buf, 4),
				pgtrace_decode_val($buf, 5),
				pgtrace_decode_val($buf, 6),
				pgtrace_decode_val($buf, 7),
				pgtrace_decode_val($buf, 8)
		      )
	}
}	
--------------------- END EXAMPLE STP -----------------------------------------
#endif

void LTRACE_PROBE_FIRE(long family, 
		const char *filename, const char *funcname, long line,
		long i1, long i2, long i3, long i4, long i5
		)
{
	if(gp_ltrace_flag)
	{
		struct pg_trace_data_t t;
		t.val[0] = family;
		t.val[1] = (long) filename;
		t.val[2] = (long) funcname;
		t.val[3] = line;
		t.val[4] = i1;
		t.val[5] = i2;
		t.val[6] = i3;
		t.val[7] = i4;
		t.val[8] = i5;

		write(0, (char *) &t, 0);
	}
}
#endif
