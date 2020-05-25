/*-------------------------------------------------------------------------
*
* fileam.h
*	  external file access method definitions.
*
* Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/fileam.h
*
*-------------------------------------------------------------------------
*/
#ifndef FILEAM_H
#define FILEAM_H

#include "access/formatter.h"
#include "access/sdir.h"
#include "access/url.h"
#include "utils/rel.h"

/*
 * ExternalInsertDescData is used for storing state related
 * to inserting data into a writable external table.
 */
typedef struct ExternalInsertDescData
{
	Relation	ext_rel;
	URL_FILE   *ext_file;
	char	   *ext_uri;		/* "command:<cmd>" or "tablespace:<path>" */
	bool		ext_noop;		/* no op. this segdb needs to do nothing (e.g.
								 * mirror seg) */

	TupleDesc	ext_tupDesc;
	Datum	   *ext_values;
	bool	   *ext_nulls;

	FmgrInfo   *ext_custom_formatter_func; /* function to convert to custom format */
	List	   *ext_custom_formatter_params; /* list of defelems that hold user's format parameters */

	FormatterData *ext_formatter_data;

	struct CopyStateData *ext_pstate;	/* data parser control chars and state */

} ExternalInsertDescData;

typedef ExternalInsertDescData *ExternalInsertDesc;

/*
 * ExternalSelectDescData is used for storing state related
 * to selecting data from an external table.
 */
typedef struct ExternalSelectDescData
{
	ProjectionInfo *projInfo;   /* Information for column projection */
	List *filter_quals;         /* Information for filter pushdown */

} ExternalSelectDescData;

/*
 * used for scan of external relations with the file protocol
 */
typedef struct FileScanDescData
{
	/* scan parameters */
	Relation	fs_rd;			/* target relation descriptor */
	struct URL_FILE *fs_file;	/* the file pointer to our URI */
	char	   *fs_uri;			/* the URI string */
	bool		fs_noop;		/* no op. this segdb has no file to scan */
	uint32      fs_scancounter;	/* copied from struct ExternalScan in plan */

	/* current file parse state */
	struct CopyStateData *fs_pstate;

	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs;
	Datum	   *values;
	bool	   *nulls;
	FmgrInfo   *in_functions;
	Oid		   *typioparams;
	Oid			in_func_oid;

	/* current file scan state */
	TupleDesc	fs_tupDesc;
	HeapTupleData fs_ctup;		/* current tuple in scan, if any */

	/* custom data formatter */
	FmgrInfo   *fs_custom_formatter_func; /* function to convert to custom format */
	List	   *fs_custom_formatter_params; /* list of defelems that hold user's format parameters */
	FormatterData *fs_formatter;

	/* external partition */
	bool		fs_hasConstraints;
	List		**fs_constraintExprs;
}	FileScanDescData;

typedef FileScanDescData *FileScanDesc;

typedef enum DataLineStatus
{
	LINE_OK,
	LINE_ERROR,
	NEED_MORE_DATA,
	END_MARKER
} DataLineStatus;

extern FileScanDesc external_beginscan(Relation relation,
				   uint32 scancounter, List *uriList,
				   char fmtType, bool isMasterOnly,
				   int rejLimit, bool rejLimitInRows,
				   char logErrors, int encoding, List *extOptions);
extern void external_rescan(FileScanDesc scan);
extern void external_endscan(FileScanDesc scan);
extern void external_stopscan(FileScanDesc scan);
extern ExternalSelectDesc
external_getnext_init(PlanState *state);
extern HeapTuple
external_getnext(FileScanDesc scan,
                 ScanDirection direction,
                 ExternalSelectDesc desc);
extern ExternalInsertDesc external_insert_init(Relation rel);
extern Oid	external_insert(ExternalInsertDesc extInsertDesc, HeapTuple instup);
extern void external_insert_finish(ExternalInsertDesc extInsertDesc);

extern List *appendCopyEncodingOption(List *copyFmtOpts, int encoding);

#endif   /* FILEAM_H */
