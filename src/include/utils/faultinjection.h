/*-------------------------------------------------------------------------
 *
 * Fault injection utilities 
 * 
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/faultinjection.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GP_FAULT_INJECTION_H
#define GP_FAULT_INJECTION_H

/* Fault group 1: fault from user calling gp_fault_inject(fault_code, arg) */
#define GP_FAULT_USER 1
#define GP_FAULT_USER_SEGV 		1 	/* Seg fault */
#define GP_FAULT_USER_LEAK 		2 	/* Leaking some memory */
#define GP_FAULT_USER_LEAK_TOP          3 	/* Leaking from top context */
#define GP_FAULT_USER_RAISE_ERROR 	4	/* elog a error */
#define GP_FAULT_USER_RAISE_FATAL 	5 	/* elog a fatal */
#define GP_FAULT_USER_RAISE_PANIC 	6 	/* elog a panic */
#define GP_FAULT_USER_PROCEXIT          7   /* Simply call proc_exit  */
#define GP_FAULT_USER_ABORT             8   /* Simply call abort */
#define GP_FAULT_USER_INFINITE_LOOP     9   /* Infinite loop */
#define GP_FAULT_USER_ASSERT_FAILURE   10   /* Assert failure */
#define GP_FAULT_USER_SEGV_CRITICAL    11   /* SEGV inside critical section */
#define GP_FAULT_USER_SEGV_LWLOCK	12		/* SEGV while holding LWLock */
#define GP_FAULT_USER_OPEN_MANY_FILES	13		/* Open very many temporary files */

/* Fault group 2: From User, cause some fault in MP */
#define GP_FAULT_USER_MP_CONFIG 	100	/* Show vmem_max guc */
#define GP_FAULT_USER_MP_ALLOC 	 	101 /* Show allocation */
#define GP_FAULT_USER_MP_HIGHWM 	102 /* Show high water mark */
#define GP_FAULT_SEG_AVAILABLE 		103 /* Show segment available */
#define GP_FAULT_SEG_GET_VMEMMAX 	104 /* Get current vmem max */
#define GP_FAULT_SEG_SET_VMEMMAX 	105 /* Set new vmem max */

/* Fault group 3: Test logging */
#define GP_FAULT_LOG_LONGMSG            200 /* log a long message */
#define GP_FAULT_LOG_3RDPARTY           201 /* directly write to stderr */
#define GP_FAULT_LOG_3RDPARTY_LONGMSG   202 /* directly write to stderr */
#define GP_FAULT_LOG_CRASH              203 /* crash the logger process */

/* Fault group 4: inject failure */
#define GP_FAULT_INJECT_SEGMENT_FAILURE 300 /* inject a segment failure */

#ifdef FAULT_INJECTOR
extern int64 gp_fault_inject_impl(int32 reason, int64 arg); 

extern bool gp_fault_inject_segment_failure;
extern int gp_fault_inject_segment_failure_segment_id;
#endif

#endif

