/*-------------------------------------------------------------------------
 *
 * seqserver.c
 *	  Process under QD postmaster used for doling out sequence values to
 * 		QEs.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/seqserver.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SEQSERVER_H
#define SEQSERVER_H

#ifndef _WIN32
#include <stdint.h>             /* uint32_t (C99) */
#else
typedef unsigned int uint32_t;
#endif

#define SEQ_SERVER_REQUEST_START (0x53455152)
#define SEQ_SERVER_REQUEST_END (0x53455145)

/*
 * TODO: eventually we may want different requests to the sequence server
 * 	     other than just next_val().  at that point we should generalize
 * 		 the request/response processing code here and in the seqserver
 */
typedef struct NextValRequest
{
	uint32_t	startCookie;
	uint32_t    dbid;
	uint32_t    tablespaceid;	
	uint32_t    seq_oid;
	uint32_t    isTemp;
	uint32_t    session_id;
	uint32_t	endCookie;
}	NextValRequest;

typedef struct NextValResponse
{
	int64  	 plast;
	int64    pcached;
	int64    pincrement;
	bool 	 poverflowed;
} NextValResponse;

extern void SeqServerShmemInit(void);
extern int SeqServerShmemSize(void);
extern int seqserver_start(void);


/* used to call nextval on a sequence by going through the seqserver*/
extern void 
sendSequenceRequest(int     sockfd, 
					Relation seqrel,
                    int     session_id,
					int64  *plast, 
                    int64  *pcached,
			 		int64  *pincrement,
			 		bool   *poverflow);



#endif   /* SEQSERVER_H */
