/*-------------------------------------------------------------------------
 *
 * cdbconn.h
 *
 * Functions returning results from a remote database
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBCONN_H
#define CDBCONN_H

#include "gp-libpq-fe.h"               /* prerequisite for libpq-int.h */
#include "gp-libpq-int.h"              /* PQExpBufferData */

/* --------------------------------------------------------------------------------------------------
 * Structure for segment database definition and working values
 */
typedef struct SegmentDatabaseDescriptor
{
	/*
	 * Points to the SegmentDatabaseInfo structure describing the
	 * parameters for this segment database.  Information in this structure is
	 * obtained from the Greenplum administrative schema tables.
	 */
	struct CdbComponentDatabaseInfo *segment_database_info;
	
	/*
	 * Identifies the segment this group of databases is serving.  This is the
	 * content-id assigned to the segment in gp_segment_configuration.
	 *
	 * Identical to segment_database_info->segindex.
	 */
	int4         			segindex;
	
    /*
	 * A non-NULL value points to the PGconn block of a successfully
	 * established connection to the segment database.
	 */
	PGconn				   *conn;		
	
	/* Used for non-threaded gang creation */
	PostgresPollingStatusType pollStatus;

	/*
	 * Error info saved when connection cannot be established.
	 * ERRCODE_xxx (sqlstate encoded as an int) of first error, or 0.
	 */
    int                     errcode;

    /* message text; '\n' at end */
	PQExpBufferData         error_message;

    /*
     * Connection info saved at most recent PQconnectdb.
     *
     * NB: Use malloc/free, not palloc/pfree, for the items below.
     */
    int4		            motionListener; /* interconnect listener port */
    int4					backendPid;
    char                   *whoami;         /* QE identifier for msgs */
    struct SegmentDatabaseDescriptor * myAgent;

} SegmentDatabaseDescriptor;


/* Initialize a segment descriptor in storage provided by the caller. */
void
cdbconn_initSegmentDescriptor(SegmentDatabaseDescriptor        *segdbDesc,
                              struct CdbComponentDatabaseInfo  *cdbinfo);


/* Free all memory owned by a segment descriptor. */
void
cdbconn_termSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc);


/* Connect to a QE as a client via libpq. */
void
cdbconn_doConnect(SegmentDatabaseDescriptor *segdbDesc,
				  const char *gpqeid,
				  const char *options,
				  bool wait);

void
cdbconn_doConnectComplete(SegmentDatabaseDescriptor *segdbDesc);


/* Disconnect from QE */
void cdbconn_disconnect(SegmentDatabaseDescriptor *segdbDesc);

/*
 * Read result from connection and discard it.
 *
 * Retry at most N times.
 *
 * Return false if there'er still leftovers.
 */
bool cdbconn_discardResults(SegmentDatabaseDescriptor *segdbDesc,
		int retryCount);

/* Return if it's a bad connection */
bool cdbconn_isBadConnection(SegmentDatabaseDescriptor *segdbDesc);

/* Reset error message buffer */
void cdbconn_resetQEErrorMessage(SegmentDatabaseDescriptor *segdbDesc);

/* Set the slice index for error messages related to this QE. */
void setQEIdentifier(SegmentDatabaseDescriptor *segdbDesc, int sliceIndex, MemoryContext mcxt);

/* Send cancel/finish request to QE */
bool
cdbconn_signalQE(SegmentDatabaseDescriptor *segdbDesc,
				 char *errbuf,
				 bool isCancel);
#endif   /* CDBCONN_H */
