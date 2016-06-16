/* $PostgreSQL: pgsql/src/include/port/solaris.h,v 1.17 2007/11/15 21:14:44 momjian Exp $ */

/*
 * Sort this out for all operating systems some time.  The __xxx
 * symbols are defined on both GCC and Solaris CC, although GCC
 * doesn't document them.  The __xxx__ symbols are only on GCC.
 */
#if defined(__i386) && !defined(__i386__)
#define __i386__
#endif

#if defined(__amd64) && !defined(__amd64__)
#define __amd64__
#endif

#if defined(__x86_64) && !defined(__x86_64__)
#define __x86_64__
#endif

#if defined(__sparc) && !defined(__sparc__)
#define __sparc__
#endif

#if defined(__i386__)
#include <sys/isa_defs.h>
#endif

/*
 * Many versions of Solaris have broken strtod() --- see bug #4751182.
 * This has been fixed in current versions of Solaris:
 *
 * http://sunsolve.sun.com/search/document.do?assetkey=1-21-108993-62-1&searchclause=108993-62
 * http://sunsolve.sun.com/search/document.do?assetkey=1-21-112874-34-1&searchclause=112874-34
 *
 * However, many people might not have patched versions, so
 * still use our own fix for the buggy version.
 */
#define HAVE_BUGGY_SOLARIS_STRTOD


/*
 * Make sure that we have an easy way to check that we're actually on
 * Solaris in the olden days this was built into gcc -- but it appears
 * that it isn't there any longer.
 */
#ifndef pg_on_solaris
#if defined(__sun__) && defined(__unix__) && defined(__svr4__)
#define pg_on_solaris 1
#endif
#endif

/* this is defined by pg_config.h, which is included by the time we get here. */
#ifdef ENABLE_THREAD_SAFETY
/*
 * MPP-2105: Make sure that multithreaded code can get the right errno.
 * (any such code also needs to explicitly include /usr/include/errno.h)
 */
#define _REENTRANT 1
#include <errno.h>
#endif
