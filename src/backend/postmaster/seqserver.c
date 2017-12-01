/*-------------------------------------------------------------------------
 *
 * seqserver.c
 *	  Process under QD postmaster used for doling out sequence values to
 * 		QEs.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/postmaster/seqserver.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"


#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "access/xact.h"
#include "miscadmin.h"
#include "libpq/pqsignal.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbselect.h"
#include "commands/sequence.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "postmaster/seqserver.h"
#include "catalog/catalog.h"
#include "catalog/pg_database.h"
#include "catalog/pg_tablespace.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"			/* PostmasterIsAlive */
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/sinvaladt.h"
#include "tcop/tcopprot.h"
#include "utils/ps_status.h"
#include "storage/backendid.h"
#include "utils/resowner.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

#include "tcop/tcopprot.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#endif


 /*
  * backlog for listen() call:
  */
#define LISTEN_BACKLOG 64
#define getInt16(pNetData)	  ntohs(*((int16 *)(pNetData)))
#define getInt32(pNetData)	  ntohl(*((int32 *)(pNetData)))
#define getOid(pNetData)	



/*=========================================================================
 * FUNCTIONS PROTOTYPES
 */


#ifdef EXEC_BACKEND
static pid_t seqserver_forkexec(void);
#endif
NON_EXEC_STATIC void SeqServerMain(int argc, char *argv[]);

static void SeqServerLoop(void);
static bool processSequenceRequest( int sockfd );
static int listenerSetup(void);


/*=========================================================================
 * GLOBAL STATE VARIABLES
 */
/* Socket file descriptor for the listener. */
static int	listenerFd;

/* our timeout value for select() and other socket operations. */
static struct timeval tval;

static bool am_seqserver = false;
uint8 *inputBuff;

/*=========================================================================
 * VISIBLE FUNCTIONS
 */
int SeqServerShmemSize(void)
{
	/* just our one int4 */
	return sizeof(SeqServerControlBlock);
}

void SeqServerShmemInit(void)
{
	bool		found;

	/* Create or attach to the SharedSnapshot shared structure */
	seqServerCtl = (SeqServerControlBlock *) ShmemInitStruct("Sequence Server", SeqServerShmemSize(), &found);
}

/*
 * Main entry point for seqserver controller process.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
seqserver_start(void)
{
	pid_t		SeqServerPID;

#ifdef EXEC_BACKEND
	switch ((SeqServerPID = seqserver_forkexec()))
#else
	switch ((SeqServerPID = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork seqserver process: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			SeqServerMain(0, NULL);
			break;
#endif
		default:
			return (int) SeqServerPID;
	}

	/* shouldn't get here */
	return 0;
}

/*
 * Used by clients to sequence server to send a Sequence Request
 */
void
sendSequenceRequest(int     sockfd, 
					Relation seqrel,
                    int     session_id,
					int64  *plast, 
                    int64  *pcached,
			 		int64  *pincrement,
			 		bool   *poverflow)
{	
	int		n;
	int    bytesWritten = 0;
	int	   bytesRead = 0;
	int			saved_err;
	mpp_fd_set	rset, wset;

	Oid     dbid = seqrel->rd_node.dbNode;
    Oid     tablespaceid = seqrel->rd_node.spcNode;
    Oid     seq_oid = seqrel->rd_id;
    bool    isTemp = seqrel->rd_istemp;

	/* request message */
    NextValRequest  request;
	int     reqlen = sizeof(request);
	
	/* response message */
    NextValResponse response;
	int     resplen = sizeof(response);
	
	
	if (sockfd == -1)
		elog(ERROR, "Lost Connection to seqserver");
		
	/*
	 * send the sequence request to the sequence server
	 */
    memset(&request, 0, sizeof(request));	
	request.startCookie = SEQ_SERVER_REQUEST_START;
	request.dbid = htonl(dbid);
	request.tablespaceid = htonl(tablespaceid);
	request.seq_oid = htonl(seq_oid);
	request.isTemp = htonl(isTemp);
    request.session_id = htonl(session_id);
	request.endCookie = SEQ_SERVER_REQUEST_END;

	/*
	 * write the request
	 */	
	while (bytesWritten < reqlen)
	{
		CHECK_FOR_INTERRUPTS();
					
		n = write(sockfd, ((char *)&request)+bytesWritten, reqlen - bytesWritten);
		saved_err = errno;
	
		if (n == 0)
		{
			elog(ERROR, "Error: seqserver socket closed.");
			return;
		}
		
		if (n < 0)
		{
			if (saved_err != EINTR && saved_err != EWOULDBLOCK)
			{
				ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
								errmsg("Error: Could not write()  message to seqserver: %s",
									   strerror(errno)),
								errdetail("error during write() call (error:%d) to seqserver on sockfd: %d ",
										  errno, sockfd)));
			}

			if (saved_err == EWOULDBLOCK)
			{
				/* we shouldn't really get here since we are dealing with
				 * small messages, but once we've read a bit of data we
				 * need to finish out reading till we get the message (or error)
				 */
				do
				{
					struct timeval timeout = tval;
					
					CHECK_FOR_INTERRUPTS();
					
					MPP_FD_ZERO(&wset);
					MPP_FD_SET(sockfd, &wset);
					n = select(sockfd + 1, NULL, (fd_set *)&wset, NULL, &timeout);
					if (n == 0 || (n < 0 && errno == EINTR))
						continue;
					else if (n < 0)
					{
						saved_err = errno;

						ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
										errmsg("Error: Could not write()  message to seqserver: %s",
											   strerror(errno)),
										errdetail("error during write() call (error:%d) to seqserver on sockfd: %d ",
												  errno, sockfd)));
						return;
					}
				}
				while (n < 1);
			}
			/* else saved_err == EINTR */

			continue;
		}

		bytesWritten += n;
	}
	
	/*
	 * read the response
	 */	
	while (bytesRead < resplen)
	{
		CHECK_FOR_INTERRUPTS();
			
		n = read(sockfd, ((char *)&response)+bytesRead, resplen - bytesRead);
		saved_err = errno;
		
		if (n == 0)
		{
			elog( ERROR, "seqserver socket is closed");
			return;
		}
		
		if (n < 0)
		{
			if (saved_err != EINTR && saved_err != EWOULDBLOCK)
			{
				elog( ERROR, "Error: Could not read() message from seqserver."
					  "(error:%d). Closing connection.", saved_err);
			}

			if (saved_err == EWOULDBLOCK)
			{
				/* we shouldn't really get here since we are dealing with
				 * small messages, but once we've read a bit of data we
				 * need to finish out reading till we get the message (or error)
				 */
				do
				{
					struct timeval timeout = tval;
					
					CHECK_FOR_INTERRUPTS();
					
					MPP_FD_ZERO(&rset);
					MPP_FD_SET(sockfd, &rset);
					n = select(sockfd + 1, (fd_set *)&rset, NULL, NULL, &timeout);
					if (n == 0 || (n < 0 && errno == EINTR))
						continue;
					else if (n < 0)
					{
						saved_err = errno;

						ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
									    errmsg("Error: Could not read() message from seqserver"),
										errdetail("error during read() call (error:%d) from remote"
												  "sockfd = %d", errno, sockfd)));
						return;		
					}
				}
				while (n < 1);
			}
			/* else saved_err == EINTR */

			continue;
		}
		else
			bytesRead += n;
	}
	
	if (response.poverflowed)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("nextval: reached %s value of sequence \"%s\" (" INT64_FORMAT ")",
								response.pincrement>0 ? "maximum":"minimum",
								RelationGetRelationName(seqrel), response.plast)));
	}

	*plast = response.plast;
	*pcached = response.pcached;
	*pincrement = response.pincrement;
	*poverflow = response.poverflowed;
}




/*=========================================================================
 * HELPER FUNCTIONS
 */


#ifdef EXEC_BACKEND
/*
 * seqserver_forkexec()
 *
 * Format up the arglist for the serqserver process, then fork and exec.
 */
static pid_t
seqserver_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--forkseqserver";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}
#endif   /* EXEC_BACKEND */

static bool shutdown_requested=false;

static void
RequestShutdown(SIGNAL_ARGS)
{
	shutdown_requested = true;
}

static char *knownDatabase = "postgres";

/*
 * AutoVacMain
 */
NON_EXEC_STATIC void
SeqServerMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;
	char	   *fullpath;

	IsUnderPostmaster = true;
	am_seqserver = true;

	/* Stay away from PMChildSlot */
	MyPMChildSlot = -1;

	/* reset MyProcPid */
	MyProcPid = getpid();
	
	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/* Identify myself via ps */
	init_ps_display("seqserver process", "", "", "");

	SetProcessingMode(InitProcessing);

	/*
	 * Set up signal handlers.	We operate on databases much like a regular
	 * backend, so we use the same signal handling.  See equivalent code in
	 * tcop/postgres.c.
	 *
	 * Currently, we don't pay attention to postgresql.conf changes that
	 * happen during a single daemon iteration, so we can ignore SIGHUP.
	 */
	pqsignal(SIGHUP, SIG_IGN);

	/*
	 * Presently, SIGINT will lead to autovacuum shutdown, because that's how
	 * we handle ereport(ERROR).  It could be improved however.
	 */
	pqsignal(SIGINT, StatementCancelHandler);
	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, quickdie); /* we don't do any seq-server specific cleanup, just use the standard. */
	pqsignal(SIGALRM, handle_sig_alarm);

	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, procsignal_sigusr1_handler);
	/* We don't listen for async notifies */
	pqsignal(SIGUSR2, RequestShutdown);
	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	/* Early initialization */
	BaseInit();

	/* See InitPostgres()... */
    InitProcess();	
	InitBufferPoolBackend();
	InitXLOGAccess();

	SetProcessingMode(NormalProcessing);

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		/*
		 * We can now go away.	Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);

	/* set up a listener port and put it in shmem */
	seqServerCtl->seqServerPort = listenerSetup();
	
	/*
	 * setup scratch buffers
	 */
	inputBuff = palloc(sizeof(NextValRequest));
	
	/*
	 * The following additional initialization allows us to call the persistent meta-data modules.
	 */

	/*
	 * Create a resource owner to keep track of our resources (currently only
	 * buffer pins).
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "SeqServer");

	/*
	 * Add my PGPROC struct to the ProcArray.
	 *
	 * Once I have done this, I am visible to other backends!
	 */
	InitProcessPhase2();

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	SharedInvalBackendInit(false);

	if (MyBackendId > MaxBackends || MyBackendId <= 0)
		elog(FATAL, "bad backend id: %d", MyBackendId);

	/*
	 * bufmgr needs another initialization call too
	 */
	InitBufferPoolBackend();

	/* heap access requires the rel-cache */
	RelationCacheInitialize();
	InitCatalogCache();

	/*
	 * It's now possible to do real access to the system catalogs.
	 *
	 * Load relcache entries for the system catalogs.  This must create at
	 * least the minimum set of "nailed-in" cache entries.
	 */
	RelationCacheInitializePhase2();

	/*
	 * Start a new transaction here before first access to db, and get a
	 * snapshot.  We don't have a use for the snapshot itself, but we're
	 * interested in the secondary effect that it sets RecentGlobalXmin.
	 */
	StartTransactionCommand();
	(void) GetTransactionSnapshot();

	/*
	 * In order to access the catalog, we need a database, and a
	 * tablespace; our access to the heap is going to be slightly
	 * limited, so we'll just use some defaults.
	 */
	if (!FindMyDatabase(knownDatabase, &MyDatabaseId, &MyDatabaseTableSpace))
		ereport(FATAL,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database \"%s\" does not exit", knownDatabase)));

	/* Now we can mark our PGPROC entry with the database ID */
	/* (We assume this is an atomic store so no lock is needed) */
	MyProc->databaseId = MyDatabaseId;

	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

	SetDatabasePath(fullpath);

	RelationCacheInitializePhase3();

	/* close the transaction we started above */
	CommitTransactionCommand();

	SeqServerLoop();

	ResourceOwnerRelease(CurrentResourceOwner,
						 RESOURCE_RELEASE_BEFORE_LOCKS,
						 false, true);							

	/* One iteration done, go away */
	proc_exit(0);
}

static void
SeqServerLoop(void)
{
	int			n,
		highsock = 0,
		newsockfd;
	mpp_fd_set	rset,
		rrset;
	
	struct sockaddr_in addr;
	socklen_t	addrlen;
	
	List   *connectedSockets = NIL;
	ListCell   *cell;
	
	tval.tv_sec = 3;
	tval.tv_usec = 500000;
	
	MPP_FD_ZERO(&rset);
	MPP_FD_SET(listenerFd, &rset);
	highsock = listenerFd + 1;
	
	/* we'll handle many incoming sockets but keep the sockets in blocking
	 * mode since we are dealing with very small messages.
	 */
	while(true)
	{
		struct timeval timeout = tval;
				
		CHECK_FOR_INTERRUPTS();

		if (shutdown_requested)
			break;
		
		/* no need to live on if postmaster has died */
		if (!PostmasterIsAlive(true))
			exit(1);
		
		memcpy(&rrset, &rset, sizeof(mpp_fd_set));
		
		n = select(highsock + 1, (fd_set *)&rrset, NULL, NULL, &timeout);
		if (n == 0 || (n < 0 && errno == EINTR))
		{
			/* intr or timeout: Have we been here too long ? */			
			continue;
		}

		if (n < 0)
		{
			/* this may be a little severe, but if we error on select() 
			 * we'll just go ahead and blow up.  This will result in the 
			 * postmaster re-spawning a new process. 
			 */
			elog(ERROR,"Error during select() call (error:%d).", errno);
			break;	
		}
		
		elog(DEBUG5, "seqserver select() returned %d ready sockets", n);

		/* is it someone tickling our listener port? */
		if (MPP_FD_ISSET(listenerFd, &rrset))
		{
			elog(DEBUG3, "seqserver: accepting an Incoming Connection ");
            addrlen = sizeof(addr);
			if ((newsockfd = accept(listenerFd, (struct sockaddr *) & addr, &addrlen)) < 0)
			{
				/*
				 * TODO: would be nice to read the errno and try and provide
				 * more useful info as to why this happened.
				 */
				elog(LOG, "seqserver: error during accept() call (error:%d)", errno);
			}
			
			/* make socket non-blocking BEFORE we connect. */
			if (!pg_set_noblock(newsockfd))
			{
				/*
				 * TODO: would be nice to read the errno and try and provide
				 * more useful info as to why this happened.
				 */
				elog(DEBUG3, "seqserver: Could not set outbound socket to non-blocking mode"
					 "error during fcntl() call (error:%d) for sockfd: %d",
					 errno, newsockfd);
			}
			
			if (newsockfd > highsock)
				highsock = newsockfd + 1;

			MPP_FD_SET(newsockfd, &rset);

			connectedSockets = lappend_int(connectedSockets, newsockfd);						
		}
		
		
		/* loop through all of our established sockets */					
		cell = list_head(connectedSockets);
		while (cell != NULL)
		{
			int	fd = lfirst_int(cell);

			/* get the next cell ready before we delete */			
			cell = lnext(cell);
			
			if (MPP_FD_ISSET(fd, &rrset))
			{				
				if (!processSequenceRequest(fd))
				{
					/* close it down */	
					MPP_FD_CLR( fd, &rset);
					connectedSockets = list_delete_int(connectedSockets, fd);
					shutdown(fd, SHUT_WR);
					close(fd);
				}							
			}	
		}
	} /* end server loop */

	return;
}

/*
 * Used by sequenceServer to process incoming Sequence Requests
 */
static bool
processSequenceRequest(int sockfd )
{
	NextValRequest nextValRequest;
    NextValResponse response;
	int			saved_err;
	mpp_fd_set	rset, wset;
	int64 plast;
	int64 pcached;
	int64 pincrement;
	bool poverflow = false;
	
	int n;
	int bytesRead = 0;
	int bytesWritten = 0;
	int reqlen = sizeof(NextValRequest);
    int outputBuffLength = sizeof(NextValResponse);

	bool checkCookie = true;

	/* Read in the incoming request message */
	while (bytesRead < reqlen)
	{
		CHECK_FOR_INTERRUPTS();

		n = read(sockfd, inputBuff + bytesRead, reqlen - bytesRead);
		saved_err = errno;

		if (n == 0)
		{
			elog(DEBUG3, "client socket is closed");
			return false;
		}
		
		if (n < 0)
		{
			if (saved_err != EINTR && saved_err != EWOULDBLOCK)
			{
				elog(DEBUG3, "Error: Could not read() message from client."
					  "(error:%d). Closing connection.", saved_err);
				return false;
			}

			if (saved_err == EWOULDBLOCK)
			{
				/* we shouldn't really get here since we are dealing with
				 * small messages, but once we've read a bit of data we
				 * need to finish out reading till we get the message (or error)
				 */
				do
				{
					struct timeval timeout = tval;
					
					CHECK_FOR_INTERRUPTS();
					
					MPP_FD_ZERO(&rset);
					MPP_FD_SET(sockfd, &rset);

					n = select(sockfd + 1, (fd_set *)&rset, NULL, NULL, &timeout);
					if (n == 0 || (n < 0 && errno == EINTR))
						continue;
					else if (n < 0)
					{
						saved_err = errno;

						elog(DEBUG3, "error reading incoming message from client. (errno: %d)",
							 saved_err);
						return false;
					}
				}
				while (n < 1);
				
			}
			/* else saved_err == EINTR */
			continue;
		}

		bytesRead += n;

		/*
		 * MPP-10189: validate input, if it looks like we got
		 * something invalid, either slam the connection shut, or
		 * should we look for a valid embedded message ?
		 */
		if (checkCookie && bytesRead > sizeof(uint32_t))
		{
			uint32_t c;

			memcpy(&c, inputBuff, sizeof(uint32_t));
			if (c != SEQ_SERVER_REQUEST_START)
			{
				elog(LOG, "bogus start cookie.");
				return false; /* this will close the connection */
			}
			checkCookie = false;
		}
	}

	/*
	 * MPP-10189: Verify end-cookie
	 */
	{
		uint32_t c;

		memcpy(&c, inputBuff + bytesRead - sizeof(uint32_t), sizeof(uint32_t));
		if (c != SEQ_SERVER_REQUEST_END)
		{
			elog(LOG, "bogus end cookie.");
			return false; /* this will close the connection */
		}
	}

	/*
	 * process what we've read
	 */
	memcpy(&nextValRequest, inputBuff, sizeof(nextValRequest));
	nextValRequest.dbid    	    = ntohl(nextValRequest.dbid);
	nextValRequest.tablespaceid = ntohl(nextValRequest.tablespaceid);
	nextValRequest.seq_oid      = ntohl(nextValRequest.seq_oid);
	nextValRequest.isTemp       = ntohl(nextValRequest.isTemp);
    nextValRequest.session_id   = ntohl(nextValRequest.session_id);
	
	elog(DEBUG5, "Received nextval request for dbid: %ld tablespaceid: %ld seqoid: "
				  "%ld isTemp: %s session_id: %ld",
				  (long int)nextValRequest.dbid,
				  (long int)nextValRequest.tablespaceid,
				  (long int)nextValRequest.seq_oid,
				  nextValRequest.isTemp ? "true" : "false",
				  (long)nextValRequest.session_id);

	/*
	 * Process request.
	 *
	 * MPP-10189: May throw an error, we need to catch it -- just slam
	 * our connection shut (caller will do that if we return false).
	 */
	PG_TRY();
	{
		/*
		 * Processing the request might need to do catalog lookups, which requires
		 * a transaction. Updating the sequence itself is not transactional, so this
		 * transaction should be read-only.
		 */
		StartTransactionCommand();

		cdb_sequence_nextval_server(nextValRequest.tablespaceid,
									nextValRequest.dbid,
									nextValRequest.seq_oid,
									nextValRequest.isTemp,
									&plast,
									&pcached,
									&pincrement,
									&poverflow);

		CommitTransactionCommand();
	}
	PG_CATCH();
	{
		if (!elog_demote(LOG))
		{
    		elog(LOG, "unable to demote error");
			PG_RE_THROW();
		}
		return false;
	}
	PG_END_TRY();

	/*
	 * Create the response
	 */

	response.plast = plast;
	response.pcached = pcached;
	response.pincrement = pincrement;
	response.poverflowed = poverflow;

	/*
	 * Write the response
	 */
	while (bytesWritten < outputBuffLength )
	{
		CHECK_FOR_INTERRUPTS();

		n = write(sockfd, ((uint8 *)&response)+bytesWritten, outputBuffLength - bytesWritten);
		saved_err = errno;

		if (n == 0)
		{
			elog(DEBUG3, "client socket is closed");
			return false;
		}

		if (n < 0)
		{
			if (saved_err != EINTR && saved_err != EWOULDBLOCK)
			{
				elog(DEBUG3, "Error: Could not write() message from client."
					  "(error:%d). Closing connection.", sockfd);
				return false;
			}

			if (saved_err == EWOULDBLOCK)
			{
				/*
				 * we shouldn't really get here since we are dealing with
				 * small messages, but once we've read a bit of data we
				 * need to finish out reading till we get the message (or error)
				 */
				do
				{
					struct timeval timeout = tval;
					
					CHECK_FOR_INTERRUPTS();
					
					MPP_FD_ZERO(&wset);
					MPP_FD_SET(sockfd, &wset);
					n = select(sockfd + 1, NULL, (fd_set *)&wset, NULL, &timeout);
					if (n == 0 || (n < 0 && errno == EINTR))
						continue;
					else if (n < 0)
					{
						saved_err = errno;

						elog(DEBUG3, "error writing incoming message from client. (errno: %d)", saved_err);
						return false;
					}
				}
				while (n < 1);				
			}
			/* else saved_err == EINTR */
			continue;
		}
		else
			bytesWritten += n;
	}
	
	return true;
}




/* listenerSetup
 *
 * Sets up a listener on a system-picked port.  Returns the port that the
 * system port.
 */
static int
listenerSetup(void)
{
	int listenerPort;

	struct sockaddr_storage addr;
	socklen_t	addrlen;

    struct addrinfo hints;
	struct addrinfo *addrs, *rp;
	int  s;
	char service[32];

	/*
	 * we let the system pick the TCP port here so we don't have to
	 * manage port resources ourselves.
	 */
    snprintf(service,32,"%d",0);
    memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 	/* TCP socket */
	hints.ai_flags = AI_PASSIVE;    	/* For wildcard IP address */
	hints.ai_protocol = 0;          	/* Any protocol */

	s = getaddrinfo(NULL, service, &hints, &addrs);
	if (s != 0)
		elog(ERROR,"getaddrinfo says %s",gai_strerror(s));

	/*
	 * getaddrinfo() returns a list of address structures,
	 * one for each valid address and family we can use.
	 *
	 * Try each address until we successfully bind.
	 * If socket (or bind) fails, we (close the socket
	 * and) try the next address.  This can happen if
	 * the system supports IPv6, but IPv6 is disabled from
	 * working, or if it supports IPv6 and IPv4 is disabled.
	 */

	/*
	 * If there is both an AF_INET6 and an AF_INET choice,
	 * we prefer the AF_INET6, because on UNIX it can receive either
	 * protocol, whereas AF_INET can only get IPv4.  Otherwise we'd need
	 * to bind two sockets, one for each protocol.
	 *
	 * Why not just use AF_INET6 in the hints?  That works perfect
	 * if we know this machine supports IPv6 and IPv6 is enabled,
	 * but we don't know that.
	 */

#ifdef HAVE_IPV6
	if (addrs->ai_family == AF_INET && addrs->ai_next != NULL && addrs->ai_next->ai_family == AF_INET6)
	{
		/*
		 * We got both an INET and INET6 possibility, but we want to prefer the INET6 one if it works.
		 * Reverse the order we got from getaddrinfo so that we try things in our preferred order.
		 * If we got more possibilities (other AFs??), I don't think we care about them, so don't
		 * worry if the list is more that two, we just rearrange the first two.
		 */
		struct addrinfo *temp = addrs->ai_next; 	/* second node */
		addrs->ai_next = addrs->ai_next->ai_next; 	/* point old first node to third node if any */
		temp->ai_next = addrs;   					/* point second node to first */
		addrs = temp;								/* start the list with the old second node */
	}
#endif

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		/*
		 * getaddrinfo gives us all the parameters for the socket() call
		 * as well as the parameters for the bind() call.
		 */
		elog(DEBUG1,"seqserver  listener socket %d %d %d",rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		listenerFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (listenerFd == -1)
			continue;

		if (bind(listenerFd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;              /* Success */

		close(listenerFd);
	}

	freeaddrinfo(addrs);

	if (rp == NULL)
	{               /* No address succeeded */
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
			   errmsg("SeqServer Error: Could not setup listener socket: %m"),
				  errdetail("error during bind() call (error:%d).", errno)));
		
	}

	if (listen(listenerFd, LISTEN_BACKLOG) < 0)
	{
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("SeqServer Error: Could not setup listener socket: %s", strerror(errno)),
				errdetail("error during listen() call (error:%d).", errno)));
	}


	addrlen = sizeof(addr);

	if (getsockname(listenerFd, (struct sockaddr *) & addr, &addrlen) < 0)
	{
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("SeqServer Error: Could not setup listener socket: %s", strerror(errno)),
		   errdetail("error during getsockname() call (error:%d).", errno)));
	}


	/* display which port was chosen by the system. */
	if (addr.ss_family == AF_INET6)
		listenerPort = ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
	else
		listenerPort = ntohs(((struct sockaddr_in*)&addr)->sin_port);

	elog(DEBUG1, "SeqServer listener on port: %d", listenerPort);

	return listenerPort;
}

