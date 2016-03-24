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
	/* segment_database_info
	 *
	 * Points to the SegmentDatabaseInfo structure describing the
	 * parameters for this segment database.  Information in this structure is
	 * obtained from the Greenplum administrative schema tables.
	 */
	struct CdbComponentDatabaseInfo *segment_database_info;
	
	/* segindex
	 *
	 * Identifies the segment this group of databases is serving.  This is the
	 * segindex assigned to the segment in gp_segment_config
	 */
	int4         			segindex; /* this is actually the *content-id* */

    /* conn
	 *
	 * A non-NULL value points to the PGconn block of a successfully
	 * established connection to the segment database.
	 */
	PGconn				   *conn;
	PostgresPollingStatusType poll_status;

    /*
     * Connection info saved at most recent PQconnectdb.
     *
     * NB: Use malloc/free, not palloc/pfree, for the items below.
     */
    int4		            motionListener; /* interconnect listener port */
    int4					backendPid;
    char                    whoami[100];         /* QE identifier for msgs */
    struct SegmentDatabaseDescriptor * myAgent;

} SegmentDatabaseDescriptor;


/* Initialize a segment descriptor in storage provided by the caller. */
void
cdbconn_initSegmentDescriptor(SegmentDatabaseDescriptor        *segdbDesc,
                              struct CdbComponentDatabaseInfo  *cdbinfo);

/* Connect to a QE as a client via libpq. */
void                            /* returns true if connected */
cdbconn_doConnectStart(SegmentDatabaseDescriptor *segdbDesc,
		       const char *gpqeid,
		       const char *options);

void                            /* returns true if connected */
cdbconn_doConnectComplete(SegmentDatabaseDescriptor *segdbDesc);

/* Set the slice index for error messages related to this QE. */
void
cdbconn_setSliceIndex(SegmentDatabaseDescriptor    *segdbDesc,
                      int                           sliceIndex);

#endif   /* CDBCONN_H */
