/*-------------------------------------------------------------------------
*
* fileam.h
*	  external file access method definitions.
*
* Copyright (c) 2007-2008, Greenplum inc
*
*-------------------------------------------------------------------------
*/
#ifndef FILEAM_H
#define FILEAM_H

#include "c.h"
#include "access/formatter.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/sdir.h"
#include "access/tupmacs.h"
#include "access/xlogutils.h"
#include "nodes/primnodes.h"
#include "storage/lmgr.h"
#include "utils/rel.h"
#include "utils/tqual.h"
#include "access/url.h"



/*
 * ExternalInsertDescData is used for storing state related
 * to inserting data into a writable external table.
 */
typedef struct ExternalInsertDescData
{
	Relation		ext_rel;
	FILE*			ext_file;
	char*			ext_uri;		/* "command:<cmd>" or "tablespace:<path>" */
	bool			ext_noop;		/* no op. this segdb needs to do nothing (e.g. mirror seg) */
	
	TupleDesc		ext_tupDesc;
	Datum*			ext_values;
	bool*			ext_nulls;
	
	FormatterData*  ext_formatter_data;

	struct CopyStateData *ext_pstate; 	/* data parser control chars and state */

} ExternalInsertDescData;

typedef ExternalInsertDescData *ExternalInsertDesc;

typedef enum DataLineStatus
{
	LINE_OK,
	LINE_ERROR,
	NEED_MORE_DATA,
	END_MARKER
} DataLineStatus;

extern FileScanDesc external_beginscan(Relation relation, Index scanrelid,
								   uint32 scancounter, List *uriList,
								   List *fmtOpts, char fmtType, bool isMasterOnly,
								   int rejLimit, bool rejLimitInRows,
								   Oid fmterrtbl, int encoding);
extern void external_rescan(FileScanDesc scan);
extern void external_endscan(FileScanDesc scan);
extern void external_stopscan(FileScanDesc scan);
extern HeapTuple external_getnext(FileScanDesc scan, ScanDirection direction);
extern ExternalInsertDesc external_insert_init(Relation rel);
extern Oid external_insert(ExternalInsertDesc extInsertDesc, HeapTuple instup);
extern void external_insert_finish(ExternalInsertDesc extInsertDesc);
extern void external_set_env_vars(extvar_t *extvar, char* uri, bool csv, char* escape, char* quote, bool header, uint32 scancounter);
extern void AtEOXact_ExtTables(bool isCommit);
extern void AtEOXact_ResetDataSourceCtx(void);
char*	linenumber_atoi(char buffer[20],int64 linenumber);


#endif   /* FILEAM_H */
