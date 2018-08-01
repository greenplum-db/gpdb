/*--------------------------------------------------------------------------
 *
 * cdbcopy.h
 *	 Definitions and API functions for cdbcopy.c
 *	 These are functions that are used by the backend
 *	 COPY command in Greenplum Database.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbcopy.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef CDBCOPY_H
#define CDBCOPY_H

#include "access/aosegfiles.h" /* for InvalidFileSegNumber const */ 
#include "lib/stringinfo.h"
#include "cdb/cdbgang.h"

#define COPYOUT_CHUNK_SIZE 16 * 1024

typedef enum SegDbState
{
	/*
	 * (it is best to avoid names like OUT that are likely to be #define'd or
	 * typedef'd in some platform-dependent runtime library header file)
	 */
	SEGDB_OUT,					/* Not participating in COPY (invalid etc...) */
	SEGDB_IDLE,					/* Participating but COPY not yet started */
	SEGDB_COPY,					/* COPY in progress */
	SEGDB_DONE					/* COPY completed (with or without errors) */
}	SegDbState;

typedef struct CdbCopy
{
	Gang	   *primary_writer;
	int			total_segs;		/* total number of segments in cdb */
	int		   *mirror_map;		/* indicates how many db's each segment has */
	bool		copy_in;		/* direction: true for COPY FROM false for COPY TO */
	bool		io_errors;		/* true if any I/O error occurred trying to
								 * communicate with segDB's */
	bool		skip_ext_partition;/* skip external partition */ 

	SegDbState		**segdb_state;
	
	StringInfoData	err_msg;		/* error message for cdbcopy operations */
	StringInfoData  err_context; /* error context from QE error */
	StringInfoData	copy_out_buf;/* holds a chunk of data from the database */
		
	List			*outseglist;    /* segs that currently take part in copy out. 
									 * Once a segment gave away all it's data rows
									 * it is taken out of the list */
	PartitionNode *partitions;
	List		  *ao_segnos;
	HTAB		  *aotupcounts; /* hash of ao relation id to processed tuple count */
	bool		hasReplicatedTable;
} CdbCopy;



/* global function declarations */
extern CdbCopy *makeCdbCopy(bool copy_in);
extern void cdbCopyStart(CdbCopy *cdbCopy, CopyStmt *stmt, struct GpPolicy *policy);
extern void cdbCopySendDataToAll(CdbCopy *c, const char *buffer, int nbytes);
extern void cdbCopySendData(CdbCopy *c, int target_seg, const char *buffer, int nbytes);
extern bool cdbCopyGetData(CdbCopy *c, bool cancel, uint64 *rows_processed);
extern int cdbCopyAbort(CdbCopy *c);
/*
 * GPDB_91_MERGE_FIXME: let's consistently use uint64 as type for counting rows
 * of any kind.
 */
extern int cdbCopyEndAndFetchRejectNum(CdbCopy *c, int64 *total_rows_completed, char *abort_msg);

#endif   /* CDBCOPY_H */
