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

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "c.h"

#include <signal.h>

#if !defined(WIN32) || defined(FRONTEND)

/*
 * Set up a signal handler for signal "signo"
 *
 * Returns the previous handler.
 */
pqsigfunc
pqsignal(int signo, pqsigfunc func)
{
#ifndef HAVE_POSIX_SIGNALS
	return signal(signo, func);
#else
	struct sigaction act,
	                 oact;

	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	if (func == PQ_SIG_DFL) {
		act.sa_handler = SIG_DFL;
	} else if (func == PQ_SIG_IGN) {
		act.sa_handler = SIG_IGN;
	} else {
		act.sa_flags |= SA_SIGINFO;
		act.sa_sigaction = func;
	}

#ifdef SA_NOCLDSTOP
	if (signo == SIGCHLD)
		act.sa_flags |= SA_NOCLDSTOP;
#endif

	if (sigaction(signo, &act, &oact) < 0) {
		return PQ_SIG_ERR;
	}

	if (oact.sa_flags & SA_SIGINFO) {
		return oact.sa_sigaction;
	} else {
		if (oact.sa_handler == SIG_DFL) {
			return PQ_SIG_DFL;
		} else if (oact.sa_handler == SIG_IGN) {
			return PQ_SIG_IGN;
		} else {
#ifndef FRONTEND
			elog(FATAL, "unexpected signal handler");
#else
			printf("unexpected signal handler\n");
#endif
			return PQ_SIG_ERR;
		}
	}
#endif   /* !HAVE_POSIX_SIGNALS */
}

#endif   /* !defined(WIN32) || defined(FRONTEND) */
