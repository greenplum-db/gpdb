/*-------------------------------------------------------------------------
 *
 * cdbfilerepresetpeerprocess.c
 *	  Reset process to be launched during postmaster reset when we want to
 *	  coordinate the reset with the filerep peer
 *
 * FileRepResetPeer_Main does two things:
 *   1) set up general process code
 *   2) run the coordination with the peer
 *
 * The process will exit cleanly with one of two codes:
 *   1: (EXIT_CODE_SHOULD_ENTER_FAULT) if the postmaster should transition to
 *      a filerep fault rather than continue with database/mirror restart
 *   0: (EXIT_CODE_SHOULD_RESTART_SHMEM_CLEANLY) if the postmaster should
 *      continue with shared memory reset followed by startup of the database
 *	    and mirror
 *
 * Since it's hard to set up two-way communication over a single channel, we
 * instead have the peer reset process work by periodically polling the other
 * postmaster, which spawns a process to answer the "question" quickly.
 *
 * Coordinated reset works by:
 *
 *   1) Instruct peer to reset, at the same time receiving a "reset counter"
 *      (an integer, 0-padded to several digits).
 *   2) Poll the peer for status.  If the peer is also in a reset point (and
 *      we are a mirror*) then we can proceed with the restart.  Or, if the
 *      peer is running and the returned counter is >= our counter from
 *      step 1), we can proceed with the restart. TODO: this check can
 *      actually be removed now that primaries and mirrors can be started
 *      in any order
 *
 * Portions Copyright (c) 2009-2010 Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbfilerepresetpeerprocess.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "libpq/ip.h"
#include "libpq/pqsignal.h"

#include "miscadmin.h"
#include "storage/ipc.h"
#include "postmaster/postmaster.h"
#include "postmaster/primary_mirror_mode.h"
#include "postmaster/primary_mirror_transition_client.h"

static bool resetProcessShouldExit = false;

extern void FileRepResetPeer_Main(void);

#define EXIT_CODE_SHOULD_ENTER_FAULT 1
#define EXIT_CODE_SHOULD_RESTART_SHMEM_CLEANLY 0

#define MESSAGE_FROM_PEER_BUF_SIZE 3000

/**
 * The reset "pivot point" is the time when children have been shut down but the new system has not been started
 */
#define RESET_STATUS_IS_IN_RESET_PIVOT_POINT "isInResetPivotPoint"

/**
 * The server is running right now
 */
#define RESET_STATUS_IS_RUNNING "resetStatusServerIsRunning"

 /*
  * The server is starting or stopping right now (perhaps because of a reset)
  */
#define RESET_STATUS_IS_STARTING_OR_STOPPING "resetStatusServerIsStartingOrStopping"

static bool
strequals(char *left, char *right)
{
	return strcmp(left, right) == 0;
}

static bool
determineTargetHost(struct addrinfo **addrList, char *host, char *port)
{
	/*
	 * I should make this function exported by
	 * primary_mirror_transition_client.h ?
	 */
	struct addrinfo hint;
	int			ret;

	*addrList = NULL;

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_UNSPEC;

	/* Using pghost, so we have to look-up the hostname */
	hint.ai_family = AF_UNSPEC;

	/* Use pg_getaddrinfo_all() to resolve the address */
	ret = pg_getaddrinfo_all(host, port, &hint, addrList);
	if (ret || !*addrList)
	{
		elog(WARNING, "could not translate host name \"%s\" to address: %s\n", host, gai_strerror(ret));
		return false;
	}
	return true;
}

/* global variables filled in by callback functions */
static char gResultDataBuf[MESSAGE_FROM_PEER_BUF_SIZE];
static char gErrorLogBuf[MESSAGE_FROM_PEER_BUF_SIZE];
static bool gResponseWasTooLarge = false;

/* callback function needed by code that runs transitions */
static bool
resetPeer_checkForNeedToExitFunction(void)
{
	return resetProcessShouldExit;
}

/* callback function needed by code that runs transitions */
static void
resetPeer_errorLogFunction(char *buf)
{
	if (strlen(buf) + 1 > sizeof(gErrorLogBuf))
	{
		gResponseWasTooLarge = true;
		return;
	}
	snprintf(gErrorLogBuf, sizeof(gErrorLogBuf), "%s", buf);
}

/* callback function needed by code that runs transitions */
static void
resetPeer_receivedDataCallbackFunction(char *buf)
{
	if (strlen(buf) + 1 > sizeof(gResultDataBuf))
	{
		gResponseWasTooLarge = true;
		return;
	}
	snprintf(gResultDataBuf, sizeof(gResultDataBuf), "%s", buf);
}

static void
sendMessageToPeerAndExitIfProblem(struct addrinfo *addrList, char *msgBody,
								  char messageFromPeerOut[MESSAGE_FROM_PEER_BUF_SIZE],
								  char resetNumberFromPeerOut[MESSAGE_FROM_PEER_BUF_SIZE])
{
	elog(DEBUG1, "peer reset: sending message to primary/mirror peer: %s", msgBody);

	/* set up receipt buffers (populated by the callback functions) */
	gResponseWasTooLarge = false;
	gErrorLogBuf[0] = '\0';
	gResultDataBuf[0] = '\0';

	/* make the call and check results */
	PrimaryMirrorTransitionClientInfo client;

	client.receivedDataCallbackFn = resetPeer_receivedDataCallbackFunction;
	client.errorLogFn = resetPeer_errorLogFunction;
	client.checkForNeedToExitFn = resetPeer_checkForNeedToExitFunction;
	int			resultCode = sendTransitionMessage(&client, addrList, msgBody, strlen(msgBody),
												   10 /* numRetries */ , 3600 /* transition_timeout */ );

	if (resultCode != TRANS_ERRCODE_SUCCESS)
	{
		elog(WARNING, "during reset, unable to contact primary/mirror peer to coordinate reset; "
			 "will transition to fault state.  Error code %d and message '%s'",
			 resultCode, gErrorLogBuf);
		proc_exit(EXIT_CODE_SHOULD_ENTER_FAULT);
	}

	/*
	 * extract the two fields into messageFromPeerOut and
	 * resetNumberFromPeerOut, skipping the first Success: line
	 *
	 * is there a way to make this simple string parser easier?
	 *
	 * The result will look like Success:\nLineToKeep1\nLineToKeep2 This pulls
	 * LineToKeep1 and LineToKeep2 out into messageFromPeerOut and
	 * resetNumberFromPeerIndex
	 *
	 * Note that because gResultDataBuf is limited to
	 * MESSAGE_FROM_PEER_BUF_SIZE, we don't technically need to check overflow
	 * here.
	 */
	int			resetNumberFromPeerIndex = 0,
				messageFromPeerIndex = 0,
				whichLine = 0;
	char	   *buf = gResultDataBuf;

	while (*buf)
	{
		if (*buf == '\n')
		{
			whichLine++;
			if (whichLine == 3)
			{
				elog(WARNING, "during reset, invalid message contacting primary/mirror peer to coordinate reset; "
					 "will transition to fault state.  Message received: %s",
					 gResultDataBuf);
				proc_exit(EXIT_CODE_SHOULD_ENTER_FAULT);
			}
		}
		else
		{
			if (whichLine == 1)
			{
				messageFromPeerOut[messageFromPeerIndex] = *buf;
				messageFromPeerIndex++;

				/* see comments above about why this is not strictly needed */
				Insist(messageFromPeerIndex < MESSAGE_FROM_PEER_BUF_SIZE);
			}
			else if (whichLine == 2)
			{
				resetNumberFromPeerOut[resetNumberFromPeerIndex] = *buf;
				resetNumberFromPeerIndex++;

				/* see comments above about why this is not strictly needed */
				Insist(resetNumberFromPeerIndex < MESSAGE_FROM_PEER_BUF_SIZE);
			}
		}
		buf++;
	}

	messageFromPeerOut[messageFromPeerIndex] = '\0';
	resetNumberFromPeerOut[resetNumberFromPeerIndex] = '\0';

	if (whichLine != 2)
	{
		elog(WARNING, "during reset, invalid message contacting primary/mirror peer to coordinate reset; "
			 "will transition to fault state.  Message received: %s",
			 gResultDataBuf);
		proc_exit(EXIT_CODE_SHOULD_ENTER_FAULT);
	}
}

static void
FileRepReset_ShutdownHandler(SIGNAL_ARGS)
{
	resetProcessShouldExit = true;
}

static void
FileRepReset_HandleCrash(SIGNAL_ARGS)
{
	StandardHandlerForSigillSigsegvSigbus_OnMainThread("filerep reset process", PASS_SIGNAL_ARGS);
}

static void
FileRepReset_ConfigureSignals(void)
{
	/* Accept Signals */
	/* shutdowns */
	pqsignal(SIGQUIT, FileRepReset_ShutdownHandler);

#ifdef SIGBUS
	pqsignal(SIGBUS, FileRepReset_HandleCrash);
#endif
#ifdef SIGILL
	pqsignal(SIGILL, FileRepReset_HandleCrash);
#endif
#ifdef SIGSEGV
	pqsignal(SIGSEGV, FileRepReset_HandleCrash);
#endif

	/* Ignore Signals */
	pqsignal(SIGHUP, SIG_IGN);
	pqsignal(SIGCHLD, SIG_IGN);
	pqsignal(SIGUSR1, SIG_IGN);
	pqsignal(SIGTERM, SIG_IGN);
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR2, SIG_IGN);

	/* Use default action */
	pqsignal(SIGINT, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);
}

void
FileRepResetPeer_Main(void)
{
	/* BASIC PROCESS SETUP */

	FileRepReset_ConfigureSignals();

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding and comments
	 * about how the error handling works.
	 */
	sigjmp_buf	local_sigjmp_buf;

	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		HOLD_INTERRUPTS();
		EmitErrorReport();
		proc_exit(EXIT_CODE_SHOULD_ENTER_FAULT);
	}
	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;
	PG_SETMASK(&UnBlockSig);


	/** NOW DO THE ACTUAL WORK */
	char		messageFromPeer[MESSAGE_FROM_PEER_BUF_SIZE];
	char		resetNumberFromPeer[MESSAGE_FROM_PEER_BUF_SIZE];
	char		resetNumberThatIndicatesResetComplete[MESSAGE_FROM_PEER_BUF_SIZE];
	struct addrinfo *addrList = NULL;
	char		portStr[100];

	PrimaryMirrorModeTransitionArguments args = primaryMirrorGetArgumentsFromLocalMemory();

	Assert(args.mode == PMModePrimarySegment || args.mode == PMModeMirrorSegment);

	snprintf(portStr, sizeof(portStr), "%d", args.peerPostmasterPort);
	if (!determineTargetHost(&addrList, args.peerAddress, portStr))
	{
		elog(WARNING, "during reset, unable to look up address for peer host to coordinate reset; "
			 "will transition to fault state.");
		proc_exit(EXIT_CODE_SHOULD_ENTER_FAULT);
	}

	sendMessageToPeerAndExitIfProblem(addrList, "beginPostmasterReset", messageFromPeer,
									  resetNumberThatIndicatesResetComplete);

	for (;;)
	{
		pg_usleep(10 * 1000L);	/* 10 ms */
		sendMessageToPeerAndExitIfProblem(addrList, "getPostmasterResetStatus", messageFromPeer, resetNumberFromPeer);
		if (strequals(messageFromPeer, RESET_STATUS_IS_IN_RESET_PIVOT_POINT))
		{
			if (args.mode == PMModeMirrorSegment)
			{
				/**
				 * peer is in the reset pivot point, we can break out of our checking loop and
				 *   thus exit with a code telling the postmaster to begin the startup sequence again
				 *
				 * this is only done on the mirror as currently the mirror must execute the startup sequence
				 *   before the primary
				 */
				elog(DEBUG1, "peer reset: primary peer has reached reset point");
				break;
			}
		}
		else if (strequals(messageFromPeer, RESET_STATUS_IS_RUNNING))
		{
			/** it's running -- is it >= than the reset number that indicates reset complete one */
			if (strcmp(resetNumberFromPeer, resetNumberThatIndicatesResetComplete) >= 0)
			{
				/** yes, the reset is complete and so we can quit and do a restart */
				elog(DEBUG1, "peer reset: mirror peer reset is complete");
				break;
			}
		}
	}

	proc_exit(EXIT_CODE_SHOULD_RESTART_SHMEM_CLEANLY);
}
