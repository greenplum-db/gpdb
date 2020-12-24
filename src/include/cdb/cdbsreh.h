/*-------------------------------------------------------------------------
 *
 * cdbsreh.h
 *	  routines for single row error handling
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbsreh.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBSREH_H
#define CDBSREH_H

#include "fmgr.h"
#include "cdb/cdbcopy.h"
#include "utils/memutils.h"

#define LOG_ERRORS_ENABLE			't'
#define LOG_ERRORS_PERSISTENTLY		'p'
#define LOG_ERRORS_DISABLE			'f'

#define IS_LOG_TO_FILE(c)				(c == 't' || c == 'p')
#define IS_LOG_ERRORS_ENABLE(c)			(c == 't')
#define IS_LOG_ERRORS_PERSISTENTLY(c)	(c == 'p')
#define IS_LOG_ERRORS_DISABLE(c)		(c == 'f')

/*
 * The error table is ALWAYS of the following format
 * cmdtime     timestamptz,
 * relname     text,
 * filename    text,
 * linenum     int,
 * bytenum     int,
 * errmsg      text,
 * rawdata     text,
 * rawbytes    bytea
 */
#define NUM_ERRORTABLE_ATTR 8
#define errtable_cmdtime 1
#define errtable_relname 2
#define errtable_filename 3
#define errtable_linenum 4
#define errtable_bytenum 5
#define errtable_errmsg 6
#define errtable_rawdata 7
#define errtable_rawbytes 8

/*
 * All the Single Row Error Handling state is kept here.
 * When an error happens and we are in single row error handling
 * mode this struct is updated and handed to the single row
 * error handling manager (cdbsreh.c).
 */
typedef struct CdbSreh
{
	/* bad row information */
	char	*errmsg;		/* the error message for this bad data row */
	char	*rawdata;		/* the bad data row */
	char	*relname;		/* target relation */
	int64	linenumber;		/* line number of error in original file */
	uint64	processed;      /* num logical input rows processed so far */
	bool	is_server_enc;	/* was bad row converted to server encoding? */

	/* reject limit state */
	int		rejectlimit;	/* SEGMENT REJECT LIMIT value */
	int64	rejectcount;	/* how many were rejected so far */
	bool	is_limit_in_rows; /* ROWS = true, PERCENT = false */

	MemoryContext badrowcontext;	/* per-badrow evaluation context */
	char	filename[MAXPGPATH];	/* "uri [filename]" */

	char	logerrors;				/* 't' to log errors into file, 'f' to disable log error,
									   'p' means log errors persistently, when drop the
									   external table, the error log not get dropped */
	Oid		relid;					/* parent relation id */
} CdbSreh;

extern int gp_initial_bad_row_limit;

extern CdbSreh *makeCdbSreh(int rejectlimit, bool is_limit_in_rows,
							char *filename, char *relname, char logerrors);
extern void destroyCdbSreh(CdbSreh *cdbsreh);
extern void HandleSingleRowError(CdbSreh *cdbsreh);
extern void ReportSrehResults(CdbSreh *cdbsreh, uint64 total_rejected);
extern void SendNumRows(int64 numrejected, int64 numcompleted);
extern void ErrorIfRejectLimitReached(CdbSreh *cdbsreh);
extern bool ExceedSegmentRejectHardLimit(CdbSreh *cdbsreh);
extern bool IsRejectLimitReached(CdbSreh *cdbsreh);
extern void VerifyRejectLimit(char rejectlimittype, int rejectlimit);

extern bool PersistentErrorLogDelete(Oid databaseId, Oid namespaceId, const char* fname);
extern bool ErrorLogDelete(Oid databaseId, Oid relationId);
extern Datum gp_read_error_log(PG_FUNCTION_ARGS);
extern Datum gp_truncate_error_log(PG_FUNCTION_ARGS);
extern Datum gp_read_persistent_error_log(PG_FUNCTION_ARGS);
extern Datum gp_truncate_persistent_error_log(PG_FUNCTION_ARGS);


#endif /* CDBSREH_H */
