/*-------------------------------------------------------------------------
 *
 * gpmirrortransition.c
 *	  Utility to contact a segment and issue a primary/mirror mode transition
 *
 * Portions Copyright (c) 2009 Greenplum, Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/bin/gpmirrortransition/gpmirrortransition.c
 *
 *-------------------------------------------------------------------------
 */

#include "postmaster/primary_mirror_mode.h"
#include "postmaster/primary_mirror_transition_client.h"
#include <unistd.h>
#include "libpq/pqcomm.h"
#include "libpq/ip.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/* buffer size for message to segment */
#define SEGMENT_MSG_BUF_SIZE     4096

/**
 * gpmirrortransition builds a message from parameters and transmits it to the given server as
 *    mirror transition message.
 */


static bool
gpCheckForNeedToExitFn(void)
{
	return false;
}

static void
gpMirrorErrorLogFunction(char *str)
{
	fprintf(stderr, "%s\n", str);
}

static void
gpMirrorReceivedDataCallbackFunction(char *buf)
{
	fprintf(stderr, "%s\n", buf);
}

/**
 * *addrList will be filled in with the address(es) of the host/port when true is returned
 *
 * host/port may not be NULL
 */
static bool
determineTargetHost( struct addrinfo **addrList, char *host, char *port)
{
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
	if (ret || ! *addrList)
	{
		fprintf(stderr,"could not translate host name \"%s\" to address: %s\n", host, gai_strerror(ret));
		return false;
	}
	return true;
}

static char*
readFully(FILE *f, int *msgLenOut)
{
	int bufSize = 10;
	char *buf = malloc(bufSize * sizeof(char));
	int bufOffset = 0;

	if ( buf == NULL )
	{
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	for ( ;; )
	{
		int numRead;

		errno = 0;
		numRead = fread(buf + bufOffset, sizeof(char), bufSize - bufOffset, f);
		if ( errno != 0 )
		{
			if ( feof(f))
				break;
			fprintf( stderr, "Error reading input.  Error code %d\n", errno);
			return NULL;
		}
		else if ( numRead <= 0 && feof(f))
			break;

		bufOffset += numRead;

		if ( bufOffset == bufSize )
		{
			// increase size!
			bufSize *= 2;
			buf = realloc(buf, bufSize * sizeof(char));
			if ( buf == NULL )
			{
				fprintf(stderr, "Out of memory\n");
				return NULL;
			}
		}
	}

	*msgLenOut = bufOffset;
	return buf;
}


int
main(int argc, char **argv)
{
	struct addrinfo *addrList = NULL;

	char *host = NULL, *port = NULL, *inputFile = NULL;

	char *mode = NULL;
	char *status = NULL;
	char *seg_addr = NULL;
	char *seg_pm_port = NULL;
	char *seg_rep_port = NULL;
	char *peer_addr = NULL;
	char *peer_pm_port = NULL;
	char *peer_rep_port = NULL;

	char *num_retries_str = NULL;
	char *transition_timeout_str = NULL;
	
	int num_retries = 20;
	int transition_timeout = 3600;  /* 1 hour */
	
	int opt;

	char msgBuffer[SEGMENT_MSG_BUF_SIZE];
	char *msg = NULL;
	int msgLen = 0;

	while ((opt = getopt(argc, argv, "m:s:H:P:R:h:p:r:i:n:t:")) != -1)
	{
		switch (opt)
		{
			case 'i':
				inputFile = optarg;
				break;
			case 'm':
				mode = optarg;
				break;
			case 's':
				status = optarg;
				break;
			case 'H':
				seg_addr = optarg;
				break;
			case 'P':
				seg_pm_port = optarg;
				break;
			case 'R':
				seg_rep_port = optarg;
				break;
			case 'h':
				host = peer_addr = optarg;
				break;
			case 'p':
				port = peer_pm_port = optarg;
				break;
			case 'r':
				peer_rep_port = optarg;
				break;
			case 'n':
				num_retries_str = optarg;
				break;
			case 't':
				transition_timeout_str = optarg;
				break;
			case '?':
			default:
				/*
				 * getopt has already emitted an error about unrecognized
				 * parameter so no need to add our own
				 */
				exit(1);
				break;
		}
	}

	if (num_retries_str != NULL)
	{
		num_retries = (int) strtol(num_retries_str, NULL, 10);
		if (num_retries == 0 || errno == ERANGE)
		{
			fprintf(stderr, "Invalid num_retries (-n) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
	}
	
	if (transition_timeout_str != NULL)
	{
		transition_timeout = (int) strtol (transition_timeout_str, NULL, 10);
		if (transition_timeout == 0 || errno == ERANGE)
		{
			fprintf(stderr, "Invalid transition_timeout (-t) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
	}

	/* check if input file parameter is passed */
	if (seg_addr == NULL)
	{
		if ( host == NULL)
		{
			fprintf(stderr, "Missing host (-h) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if ( port == NULL )
		{
			fprintf(stderr, "Missing port (-p) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}

		/* find the target machine */
		if ( ! determineTargetHost(&addrList, host, port))
		{
			return TRANS_ERRCODE_ERROR_HOST_LOOKUP_FAILED;
		}

		/* load the input message into memory */
		if ( inputFile == NULL)
		{
			msg = readFully(stdin, &msgLen);
		}
		else
		{

			FILE *f = fopen(inputFile, "r");
			if ( f == NULL)
			{
				fprintf(stderr, "Unable to open file %s\n", inputFile);
				return TRANS_ERRCODE_ERROR_READING_INPUT;
			}
			msg = readFully(f, &msgLen);
			fclose(f);
		}
	}
	else
	{
		/* build message from passed parameters */

		if (mode == NULL)
		{
			fprintf(stderr, "Missing mode (-m) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (status == NULL)
		{
			fprintf(stderr, "Missing status (-s) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (seg_addr == NULL)
		{
			fprintf(stderr, "Missing segment host (-H) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (seg_pm_port == NULL)
		{
			fprintf(stderr, "Missing segment postmaster port (-P) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (seg_rep_port == NULL)
		{
			fprintf(stderr, "Missing segment replication port (-R) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (peer_addr == NULL)
		{
			fprintf(stderr, "Missing peer host (-h) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (peer_pm_port == NULL)
		{
			fprintf(stderr, "Missing peer postmaster port (-p) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}
		if (peer_rep_port == NULL)
		{
			fprintf(stderr, "Missing peer replication port (-r) argument\n");
			return TRANS_ERRCODE_ERROR_INVALID_ARGUMENT;
		}

		/* build message */
		msgLen = snprintf(
			msgBuffer, sizeof(msgBuffer),
			"%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			mode,
			status,
			seg_addr,
			seg_rep_port,
			peer_addr,
			peer_rep_port,
			peer_pm_port
			);

		msg = msgBuffer;

		/* find the target machine */
		if (!determineTargetHost(&addrList, seg_addr, seg_pm_port))
		{
			return TRANS_ERRCODE_ERROR_HOST_LOOKUP_FAILED;
		}
	}

	 /* check for errors while building the message */
	if (msg == NULL || msgLen >= sizeof(msgBuffer))
	{
		return TRANS_ERRCODE_ERROR_READING_INPUT;
	}

	/* send the message */
	PrimaryMirrorTransitionClientInfo client;
	client.receivedDataCallbackFn = gpMirrorReceivedDataCallbackFunction;
	client.errorLogFn = gpMirrorErrorLogFunction;
	client.checkForNeedToExitFn = gpCheckForNeedToExitFn;
	return sendTransitionMessage(&client, addrList, msg, msgLen, num_retries, transition_timeout);
}
