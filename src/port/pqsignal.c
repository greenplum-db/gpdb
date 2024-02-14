/*-------------------------------------------------------------------------
 *
 * pqsignal.c
 *	  reliable BSD-style signal(2) routine stolen from RWW who stole it
 *	  from Stevens...
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/port/pqsignal.c
 *
 *	A NOTE ABOUT SIGNAL HANDLING ACROSS THE VARIOUS PLATFORMS.
 *
 *	pg_config.h defines the macro HAVE_POSIX_SIGNALS for some platforms and
 *	not for others.  We use that here to decide how to handle signalling.
 *
 *	Ultrix and SunOS provide BSD signal(2) semantics by default.
 *
 *	SVID2 and POSIX signal(2) semantics differ from BSD signal(2)
 *	semantics.  We can use the POSIX sigaction(2) on systems that
 *	allow us to request restartable signals (SA_RESTART).
 *
 *	Some systems don't allow restartable signals at all unless we
 *	link to a special BSD library.
 *
 *	We devoutly hope that there aren't any Unix-oid systems that provide
 *	neither POSIX signals nor BSD signals.  The alternative is to do
 *	signal-handler reinstallation, which doesn't work well at all.
 *
 *	Windows, of course, is resolutely in a class by itself.  In the backend,
 *	we don't use this file at all; src/backend/port/win32/signal.c provides
 *	pqsignal() for the backend environment.  Frontend programs can use
 *	this version of pqsignal() if they wish, but beware that Windows
 *	requires signal-handler reinstallation, because indeed it provides
 *	neither POSIX signals nor BSD signals :-(
 * ------------------------------------------------------------------------
 */

#include "c.h"

#include <signal.h>
#ifndef FRONTEND
#include <unistd.h>
#endif


#ifdef PG_SIGNAL_COUNT			/* Windows */
#define PG_NSIG (PG_SIGNAL_COUNT)
#elif defined(NSIG)
#define PG_NSIG (NSIG)
#else
#define PG_NSIG (64)			/* XXX: wild guess */
#endif

#ifndef FRONTEND
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#endif

#include "utils/faultinjector.h"
#include "postgres.h"

static volatile pqsigfunc pqsignal_handlers[PG_NSIG];

/*
 * Except when called with SIG_IGN or SIG_DFL, pqsignal() sets up this function
 * as the handler for all signals.  This wrapper handler function checks that
 * it is called within a process that the server knows about (i.e., any process
 * that has called InitProcessGlobals(), such as a client backend), and not a
 * child process forked by system(3), etc.  This check ensures that such child
 * processes do not modify shared memory, which is often detrimental.  If the
 * check succeeds, the function originally provided to pqsignal() is called.
 * Otherwise, the default signal handler is installed and then called.
 */
static void
wrapper_handler(SIGNAL_ARGS)
{
#ifndef FRONTEND

	/*
	 * We expect processes to set MyProcPid before calling pqsignal() or
	 * before accepting signals.
	 */
	Assert(MyProcPid);
	Assert(MyProcPid != PostmasterPid || !IsUnderPostmaster);

	if (unlikely(MyProcPid != (int) getpid()))
	{
		pqsignal(postgres_signal_arg, SIG_DFL);
		raise(postgres_signal_arg);
		return;
	}
#endif

	(*pqsignal_handlers[postgres_signal_arg]) (postgres_signal_arg);
}

/*
 * Set up a signal handler for signal "signo"
 *
 * Returns the previous handler.
 *
 * NB: If called within a signal handler, race conditions may lead to bogus
 * return values.  You should either avoid calling this within signal handlers
 * or ignore the return value.
 *
 * XXX: Since no in-tree callers use the return value, and there is little
 * reason to do so, it would be nice if we could convert this to a void
 * function instead of providing potentially-bogus return values.
 * Unfortunately, that requires modifying the pqsignal() in legacy-pqsignal.c,
 * which in turn requires an SONAME bump, which is probably not worth it.
 */
pqsigfunc
pqsignal(int signo, pqsigfunc func)
{
	pqsigfunc	orig_func = pqsignal_handlers[signo];	/* assumed atomic */
#if !(defined(WIN32) && defined(FRONTEND))
	struct sigaction act,
				oact;
#else
	pqsigfunc	ret;
#endif

	Assert(signo < PG_NSIG);

	if (func != SIG_IGN && func != SIG_DFL)
	{
		pqsignal_handlers[signo] = func;	/* assumed atomic */
		func = wrapper_handler;
	}

#if !(defined(WIN32) && defined(FRONTEND))
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
#ifdef SA_NOCLDSTOP
	if (signo == SIGCHLD)
		act.sa_flags |= SA_NOCLDSTOP;
#endif
	if (sigaction(signo, &act, &oact) < 0)
		return SIG_ERR;
	else if (oact.sa_handler == wrapper_handler)
		return orig_func;
	else
		return oact.sa_handler;
#else
	/* Forward to Windows native signal system. */
	if ((ret = signal(signo, func)) == wrapper_handler)
		return orig_func;
	else
		return ret;
#endif
}
