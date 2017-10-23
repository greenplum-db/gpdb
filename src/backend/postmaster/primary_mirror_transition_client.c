/**
 * This code is used for clients that need to connect to the postmaster to send a so-called "transition" message
 * Transition messages are special in that they do not require a full database backend to be executed; so they
 *    can run on a mirror segment that does not have a running database.
 */
#include "postmaster/primary_mirror_mode.h"
#include "postmaster/primary_mirror_transition_client.h"
#include "pgtime.h"
#include <unistd.h>
#include "libpq/pqcomm.h"
#include "libpq/ip.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

 /*
  * These macros are needed to let error-handling code be portable between
  * Unix and Windows.  (ugh)
  *
  * macros taken from libpq-int.h
  */
 #ifdef WIN32
 #define SOCK_ERRNO (WSAGetLastError())
 #define SOCK_STRERROR winsock_strerror
 #define SOCK_ERRNO_SET(e) WSASetLastError(e)
 #else
 #define SOCK_ERRNO errno
 #define SOCK_STRERROR pqStrerror
 #define SOCK_ERRNO_SET(e) (errno = (e))
 #endif

#define RESULT_BUFFER_SIZE 10000
#define ERROR_MESSAGE_SIZE 1000
#define SOCKET_ERROR_MESSAGE_SIZE 256

/*
 * ConnectionInfo -- define the connection related info.
 */
typedef struct ConnectionInfo 
{
	struct addrinfo *addr;
	int resultSock;
	
	int resultStatus;

	GpMonotonicTime startTime;
	uint64 timeout_ms;
	
	char resultBuffer[RESULT_BUFFER_SIZE];
	int32 resultMsgSize;

	char errMessage[ERROR_MESSAGE_SIZE];
	char sockErrMessage[SOCKET_ERROR_MESSAGE_SIZE];
	
} ConnectionInfo;


static bool completedReceiving(ConnectionInfo *connInfo);


/*
 * createSocket -- create the socket.
 *
 * Return true if the socket is created. Otherwise, return false.
 *
 * On success, the created socket is stored in resultSock in connInfo.
 *
 * The resultStatus in connInfo is set accordingly:
 *
 *   TRANS_ERRCODE_ERROR_SOCKET on failure on socket().
 *   TRANS_ERRCODE_SUCCESS when a socket is created.
 *   TRANS_ERRCODE_INTERRUPT_REQUESTED if an interrupt is requested.
 */
static bool
createSocket(ConnectionInfo *connInfo,
			 PrimaryMirrorTransitionClientInfo *client)
{
	struct addrinfo *currentAddr = connInfo->addr;
	
	connInfo->resultStatus = TRANS_ERRCODE_SUCCESS;

	connInfo->resultSock = -1;
	while (currentAddr != NULL)
	{
 		if (client->checkForNeedToExitFn())
		{
			connInfo->resultStatus = TRANS_ERRCODE_INTERRUPT_REQUESTED;
			break;
		}

		connInfo->resultSock = socket(currentAddr->ai_family, SOCK_STREAM, 0);
		if (connInfo->resultSock >= 0)
			break;
		
		if (currentAddr->ai_next == NULL)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed to create socket: %s (errno: %d)\n",
					 SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
					 SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}
		
		currentAddr = currentAddr->ai_next;
	}

	connInfo->addr = currentAddr;

	if (connInfo->resultStatus != TRANS_ERRCODE_SUCCESS)
	{
		return false;
	}

	return true;
}

/*
 * createConnection -- create the connection to a given socket.
 *
 * Return true if the connection is created. Otherwise, return false.
 *
 * The resultStatus in connInfo is set accordingly:
 *
 *   TRANS_ERRCODE_ERROR_SOCKET on failure on connect().
 *   TRANS_ERRCODE_SUCCESS when a socket is connected.
 *   TRANS_ERRCODE_INTERRUPT_REQUESTED if an interrupt is requested.
 */
static bool
createConnection(ConnectionInfo *connInfo,
				 PrimaryMirrorTransitionClientInfo *client)
{
	if (connInfo->resultSock < 0 ||
		connInfo->addr == NULL)
		return false;

	connInfo->resultStatus = TRANS_ERRCODE_SUCCESS;

	int status = connect(connInfo->resultSock, connInfo->addr->ai_addr, connInfo->addr->ai_addrlen);
	while (status == -1)
	{
 		if (client->checkForNeedToExitFn())
		{
			connInfo->resultStatus = TRANS_ERRCODE_INTERRUPT_REQUESTED;
			break;
		}

		if (SOCK_ERRNO != EINTR && SOCK_ERRNO != EAGAIN)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed to connect: %s (errno: %d)\n",
					 SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
					 SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}

		status = connect(connInfo->resultSock, connInfo->addr->ai_addr, connInfo->addr->ai_addrlen);
	}

	return (connInfo->resultStatus == TRANS_ERRCODE_SUCCESS);
}

/*
 * createConnectedSocket -- create a connected socket.
 */
static bool
createConnectedSocket(ConnectionInfo *connInfo,
					  PrimaryMirrorTransitionClientInfo *client)
{
	struct addrinfo *currentAddr = connInfo->addr;
	connInfo->resultStatus = TRANS_ERRCODE_SUCCESS;

	while (currentAddr != NULL)
	{
		if (createSocket(connInfo, client) &&
			createConnection(connInfo, client))
		{
			break;
		}

		if (connInfo->resultStatus == TRANS_ERRCODE_INTERRUPT_REQUESTED)
		{
			break;
		}
		
		if (connInfo->resultSock >= 0)
		{
			closesocket(connInfo->resultSock);
			connInfo->resultSock = -1;
		}

		currentAddr = currentAddr->ai_next;
	}

	return (connInfo->resultStatus == TRANS_ERRCODE_SUCCESS);
}


/*
 * getRemainingTimeout -- return the remaining timeout after subtracting 
 * the elapsed time since the start time.
 */
static uint64
getRemainingTimeout(GpMonotonicTime *startTime, uint64 timeout_ms)
{
	uint64 elapsedTime_ms = gp_get_elapsed_ms(startTime);

	if (elapsedTime_ms > timeout_ms)
		return 0;
	
	return timeout_ms - elapsedTime_ms;
}


/*
 * sendFully -- send all dataLength of data to the target socket.
 *
 * This function returns true if the data is sent successfully.
 * Otherwise, return false.
 *
 * The resultStatus is set accordingly:
 *   TRANS_ERRCODE_ERROR_SOCKET if the data can not all be sent.
 *   TRANS_ERRCODE_SUCCESS when the data is sent successfully.
 *   TRANS_ERRCODE_ERROR_TIMEOUT if the timeout is expired.
 *   TRANS_ERRCODE_INTERRUPT_REQUESTED if an interrupt is requested.
 */
static bool
sendFully(ConnectionInfo *connInfo,
		  PrimaryMirrorTransitionClientInfo *client,
		  char *data,
		  int dataLength)
{
	if (connInfo->resultSock < 0)
		return false;

	connInfo->resultStatus = TRANS_ERRCODE_SUCCESS;
	
	struct pollfd nfd;
	nfd.fd = connInfo->resultSock;
	nfd.events = POLLOUT;
	int nfds = 1;

	while (dataLength > 0)
	{
		if (client->checkForNeedToExitFn())
		{
			connInfo->resultStatus = TRANS_ERRCODE_INTERRUPT_REQUESTED;
			break;
		}

		uint64 remainingTimeout = getRemainingTimeout(&connInfo->startTime, connInfo->timeout_ms);

		int status = poll(&nfd, nfds, (int)remainingTimeout);
		if (status == 0)
		{
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_TIMEOUT;
			break;
		}
		
		if (status == -1)
		{
			if (SOCK_ERRNO == EINTR || SOCK_ERRNO == EAGAIN)
			{
				continue;
			}

			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed to poll: %s (errno: %d)\n",
					 SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
					 SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}

		int numSent = send(connInfo->resultSock, data, dataLength, 0 /* flags */);
		if (numSent < 0)
		{
			if (SOCK_ERRNO == EINTR || SOCK_ERRNO == EAGAIN)
			{
				continue;
			}
			
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed to send: %s (errno: %d)\n",
					 SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
					 SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}
		else
		{
			data += numSent;
			dataLength -= numSent;
		}
	}

	return (connInfo->resultStatus == TRANS_ERRCODE_SUCCESS);
}

/*
 * receiveMessage -- wait to receive results. 
 *
 * This function returns true on success. Otherwise, return false.
 *
 * The resultStatus is set accordingly:
 *   TRANS_ERRCODE_SUCCESS when some data has been received.
 *   TRANS_ERRCODE_ERROR_TIMEOUT if the timeout is expired.
 *   TRANS_ERRCODE_INTERRUPT_REQUESTED if an interrupt is requested.
 *   TRANS_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA when no data is received.
 *   TRANS_ERRCODE_ERROR_PROTOCOL_VIOLATED if there are too much data to be received.
 */
static bool
receiveMessage(ConnectionInfo *connInfo,
			   PrimaryMirrorTransitionClientInfo *client)
{
	if (connInfo->resultSock < 0)
	{
		return false;
	}
	
	connInfo->resultMsgSize = 0;
	
	struct pollfd nfd;
	nfd.fd = connInfo->resultSock;
	nfd.events = POLLIN;
	int nfds = 1;

	int32 bufferSizeLeft = sizeof(connInfo->resultBuffer);

	connInfo->resultStatus = TRANS_ERRCODE_SUCCESS;
	
	while (bufferSizeLeft > 0)
	{
 		if (client->checkForNeedToExitFn())
		{
			connInfo->resultStatus = TRANS_ERRCODE_INTERRUPT_REQUESTED;
			break;
		}

		uint64 remainingTimeout = getRemainingTimeout(&connInfo->startTime, connInfo->timeout_ms);

		int status = poll(&nfd, nfds, (int)remainingTimeout);
		if (status == 0)
		{
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_TIMEOUT;
			break;
		}
		
		if (status < 0)
		{
			if (SOCK_ERRNO == EINTR || SOCK_ERRNO == EAGAIN)
			{
				continue;
			}

			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed to poll: %s (errno: %d)\n",
					 SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
					 SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}

		int numRecv = recv(connInfo->resultSock,
						   connInfo->resultBuffer + connInfo->resultMsgSize,
						   bufferSizeLeft,
						   0 /* flags */);
		
		if (numRecv == 0)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
			         "peer shut down connection before response was fully received\n");
			client->errorLogFn(connInfo->errMessage);

			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}

		if (numRecv < 0)
		{
			if (SOCK_ERRNO == EINTR || SOCK_ERRNO == EAGAIN)
			{
				continue;
			}

			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
			         "failed to receive response: %s (errno: %d)\n",
			         SOCK_STRERROR(SOCK_ERRNO, connInfo->sockErrMessage, sizeof(connInfo->sockErrMessage)),
			         SOCK_ERRNO);
			client->errorLogFn(connInfo->errMessage);

			connInfo->resultStatus = TRANS_ERRCODE_ERROR_SOCKET;
			break;
		}
		
		connInfo->resultMsgSize += numRecv;
		bufferSizeLeft -= numRecv;

		/* check if response was fully received */
		if (completedReceiving(connInfo))
		{
			break;
		}

		if (bufferSizeLeft == 0)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "failed: response from server is too large\n");
			client->errorLogFn(connInfo->errMessage);
			connInfo->resultStatus = TRANS_ERRCODE_ERROR_PROTOCOL_VIOLATED;
			break;
		}
	}

	return (connInfo->resultStatus == TRANS_ERRCODE_SUCCESS);
}

/*
 * Check if segment response has been fully received
 */
static bool
completedReceiving(ConnectionInfo *connInfo)
{
	/* check if message header was received */
	if (connInfo->resultMsgSize < 4)
	{
		return false;
	}

	/*
	 * message header contains message size;
	 * check if received message size is equal or bigger than expected one
	 */
	int32 expectedMsgSize = htonl(*(int32*)connInfo->resultBuffer);
	return (expectedMsgSize <= connInfo->resultMsgSize);
}


/*
 * processResponse -- process the response from the server.
 */
static void
processResponse(ConnectionInfo *connInfo,
				PrimaryMirrorTransitionClientInfo *client)
{
	if (connInfo->resultMsgSize < 4)
	{
		snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
				 "failed: server did not respond with enough data");
		client->errorLogFn(connInfo->errMessage);
		connInfo->resultStatus = TRANS_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA;
		return;
	}
	
	int32 expectedMsgSize = htonl(*(int32*)connInfo->resultBuffer);
	if (expectedMsgSize != connInfo->resultMsgSize)
	{
		snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
				 "failed: sent %d bytes, but received %d bytes",
				 expectedMsgSize, connInfo->resultMsgSize);
		client->errorLogFn(connInfo->errMessage);
		connInfo->resultStatus = TRANS_ERRCODE_ERROR_PROTOCOL_VIOLATED;
		return;
	}
	
	if (connInfo->resultMsgSize == sizeof(connInfo->resultBuffer))
	{
		connInfo->resultMsgSize--; /* Make space for trailing '\0' */
	}
	
	connInfo->resultBuffer[connInfo->resultMsgSize] = '\0';
	char *resultMsg = connInfo->resultBuffer + sizeof(expectedMsgSize);
	
	client->receivedDataCallbackFn(resultMsg);
	
	/* Anything equal to success or STARTING with success: is considered success */
	if (strcmp(resultMsg, "Success") != 0 &&
		strncmp(resultMsg, "Success:", sizeof("Success:") - 1) != 0)
	{
		connInfo->resultStatus = TRANS_ERRCODE_ERROR_UNSPECIFIED;
		client->errorLogFn(resultMsg);
	}
}

 /**
  * Send the given data to the given address(es).
  *
  * addr may be a chain of addresses
  *
  * if data is NULL then we don't send any data...we just close immediately
  *
  * @return the error code
  */
int
sendTransitionMessage(PrimaryMirrorTransitionClientInfo *client,
					  struct addrinfo *addr,
					  void *data,
					  int dataLength,
					  int maxRetries,
					  int transitionTimeout)
{
 	int retry;
 	int save_errno = SOCK_ERRNO;
	int retrySleepTimeSeconds = 1;

	ConnectionInfo *connInfo = (ConnectionInfo *)malloc(sizeof(ConnectionInfo));
	if (connInfo == NULL)
	{
		client->errorLogFn("Out of memory\n");
		return TRANS_ERRCODE_ERROR_UNSPECIFIED;
	}
	memset(connInfo, 0, sizeof(ConnectionInfo));

	connInfo->addr = addr;
	/* convert the transition timeout to milliseconds */
	connInfo->timeout_ms = transitionTimeout * 1000;

 	struct
 	{
 		uint32		packetlen;
 		PrimaryMirrorTransitionPacket payload;
 	} transitionPacket;

	/* create the transition request packet */
	transitionPacket.packetlen = htonl((uint32) sizeof(transitionPacket));
	transitionPacket.payload.protocolCode = (MsgType) htonl(PRIMARY_MIRROR_TRANSITION_REQUEST_CODE);
	transitionPacket.payload.dataLength = htonl(dataLength);

 	for (retry = 0; retry < maxRetries; retry++)
 	{
		gp_set_monotonic_begin_time(&connInfo->startTime);

		/* reset the address since it may be modified in createConnectedSocket. */
		connInfo->addr = addr;

		if (retry > 0)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage),
					 "Retrying no %d\n", retry);
			client->errorLogFn(connInfo->errMessage);
		}

		if (createConnectedSocket(connInfo, client) &&
			sendFully(connInfo, client, (char *)&transitionPacket, sizeof(transitionPacket)) &&
			sendFully(connInfo, client, (char *)data, dataLength) &&
			receiveMessage(connInfo, client))
		{
			/*
			 * Successfully send the transition request packet and receive the
			 * response from the server.
			 */
			processResponse(connInfo, client);
			break;
		}

		if (connInfo->resultStatus == TRANS_ERRCODE_INTERRUPT_REQUESTED ||
			connInfo->resultStatus == TRANS_ERRCODE_ERROR_PROTOCOL_VIOLATED ||
			connInfo->resultStatus == TRANS_ERRCODE_ERROR_SERVER_DID_NOT_RETURN_DATA)
		{
			break;
		}

		if (connInfo->resultStatus == TRANS_ERRCODE_ERROR_TIMEOUT)
		{
			snprintf(connInfo->errMessage, sizeof(connInfo->errMessage), "failure: timeout\n");
			client->errorLogFn(connInfo->errMessage);
		}
		
		if (connInfo->resultStatus != TRANS_ERRCODE_SUCCESS)
		{
			if (connInfo->resultSock >= 0)
			{
				closesocket(connInfo->resultSock);
				connInfo->resultSock = -1;
			}
			
			sleep(retrySleepTimeSeconds);
		}
	}

	if (connInfo->resultStatus == TRANS_ERRCODE_INTERRUPT_REQUESTED)
	{
		snprintf(connInfo->errMessage, sizeof(connInfo->errMessage), "failure: interrupted\n");
		client->errorLogFn(connInfo->errMessage);
	}
	
	if (connInfo->resultStatus == TRANS_ERRCODE_ERROR_TIMEOUT)
	{
		snprintf(connInfo->errMessage, sizeof(connInfo->errMessage), "failure: timeout\n");
		client->errorLogFn(connInfo->errMessage);
		connInfo->resultStatus = TRANS_ERRCODE_ERROR_UNSPECIFIED;
	}

 	if (connInfo->resultSock >= 0)
 		closesocket(connInfo->resultSock);
 	SOCK_ERRNO_SET(save_errno);

	int resultStatus = connInfo->resultStatus;
	
	free(connInfo);
	
	return resultStatus;
}
