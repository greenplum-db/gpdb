/*-------------------------------------------------------------------------
 *
 * cdbconn.h
 *
 * Functions returning results from a remote database
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbconn.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBCONN_H
#define CDBCONN_H


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
	int32         			segindex;
	
    /*
	 * A non-NULL value points to the PGconn block of a successfully
	 * established connection to the segment database.
	 */
	PGconn				   *conn;		

    /*
     * Connection info saved at most recent PQconnectdb.
     *
     * NB: Use malloc/free, not palloc/pfree, for the items below.
     */
    uint32		            motionListener; /* interconnect listener port */
    int32					backendPid;
    char                   *whoami;         /* QE identifier for msgs */
	bool					isWriter;
	int						identifier;		/* unique identifier in the cdbcomponent segment pool */
} SegmentDatabaseDescriptor;

SegmentDatabaseDescriptor *

cdbconn_createSegmentDescriptor(struct CdbComponentDatabaseInfo  *cdbinfo, int identifier, bool isWriter);

/* Free all memory owned by a segment descriptor. */
void
cdbconn_termSegmentDescriptor(SegmentDatabaseDescriptor *segdbDesc);


/* Connect to a QE as a client via libpq. */
void
cdbconn_doConnectStart(SegmentDatabaseDescriptor *segdbDesc,
					   const char *gpqeid,
					   const char *options);
void
cdbconn_doConnectComplete(SegmentDatabaseDescriptor *segdbDesc);

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

/* Return if it's a connection OK */
bool cdbconn_isConnectionOk(SegmentDatabaseDescriptor *segdbDesc);

/* Set the slice index for error messages related to this QE. */
void cdbconn_setQEIdentifier(SegmentDatabaseDescriptor *segdbDesc, int sliceIndex);

/*
 * Send cancel/finish signal to still-running QE through libpq.
 *
 * errbuf is used to return error message(recommended size is 256 bytes).
 *
 * Returns true if we successfully sent a signal
 * (not necessarily received by the target process).
 */
bool cdbconn_signalQE(SegmentDatabaseDescriptor *segdbDesc, char *errbuf, bool isCancel);

extern void forwardQENotices(void);

#endif   /* CDBCONN_H */
