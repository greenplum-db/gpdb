/*-------------------------------------------------------------------------
 *
 * primary_mirror_transition_client.h
 *	  Exports from primary_mirror_transition_client.c.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/primary_mirror_transition_client.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PRIMARY_MIRROR_TRANSITION_CLIENT_H
#define _PRIMARY_MIRROR_TRANSITION_CLIENT_H

/** These codes can be returned by sendTransitionMessage.  Note that some of these error codes are only used
 *   by the external client program for sending transition messages -- gpmirrortransition.c */
 
/* NOTE!  These codes also exist in python code -- gp.py */
#define TRANS_ERRCODE_SUCCESS                             0
#define TRANS_ERRCODE_ERROR_UNSPECIFIED                   1
#define TRANS_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA    10
#define TRANS_ERRCODE_ERROR_PROTOCOL_VIOLATED             11
#define TRANS_ERRCODE_ERROR_HOST_LOOKUP_FAILED            12
#define TRANS_ERRCODE_ERROR_INVALID_ARGUMENT              13
#define TRANS_ERRCODE_ERROR_READING_INPUT                 14
#define TRANS_ERRCODE_ERROR_SOCKET                        15

/* this code is for when transitionCheckShouldExitFunction returns true and we exit because of that */
#define TRANS_ERRCODE_INTERRUPT_REQUESTED                 16
/* NOTE!  These codes above also exist in python code -- gp.py */

/*
 * The timeout occurs when requesting a segment state transition.
 * Only used in gpmirrortransition.c.
 */
#define TRANS_ERRCODE_ERROR_TIMEOUT                       17

typedef void (*transitionErrorLoggingFunction)(char *str);
typedef void (*transitionReceivedDataFunction)(char *buf);
typedef bool (*transitionCheckShouldExitFunction)(void);

typedef struct
{
	transitionErrorLoggingFunction errorLogFn;

	/**
	 * This function will be called with the data received (regardless of whether it's a Success: result or not).
	 *
	 * The function will not be called if there is a socket or other error during the sending of the message.
	 *
	 * The buffer passed to the function should NOT be stored -- copy the data out if you need to keep it around
	 */
	transitionReceivedDataFunction receivedDataCallbackFn;


	/**
	 * This function will be called before entering any system call to see if we should exit
	 *    the transition attempt instead.
	 * If the function returns true, the transition will be exited as soon as possible
	 */
	transitionCheckShouldExitFunction checkForNeedToExitFn;
} PrimaryMirrorTransitionClientInfo;

extern int sendTransitionMessage(PrimaryMirrorTransitionClientInfo *client, struct addrinfo *addr,
	void *data, int dataLength, int maxRetries, int retrySleepTimeSeconds);


#endif // _PRIMARY_MIRROR_TRANSITION_CLIENT_H
