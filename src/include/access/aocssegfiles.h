/*-------------------------------------------------------------------------
 *
 * aocssegfiles.h
 *      AOCS segment files
 *
 * Portions Copyright (c) 2009, Greenplum INC.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/aocssegfiles.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef AOCS_SEGFILES_H
#define AOCS_SEGFILES_H

#include "access/appendonly_visimap.h"
#include "access/aosegfiles.h"
#include "utils/tqual.h"

#define Natts_pg_aocsseg 7
#define Anum_pg_aocs_segno 1
#define Anum_pg_aocs_tupcount 2
#define Anum_pg_aocs_varblockcount 3
#define Anum_pg_aocs_vpinfo 4
#define Anum_pg_aocs_modcount 5
#define Anum_pg_aocs_formatversion 6
#define Anum_pg_aocs_state 7


/*
 * pg_aocsseg_nnnnnn table values for FormData_pg_attribute.
 *
 * [Similar examples are Schema_pg_type, Schema_pg_proc, Schema_pg_attribute, etc, in
 *  pg_attribute.h]
 */
#define Schema_pg_aocsseg \
{ -1, {"segno"}, 				23, -1, 4, 1, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ -1, {"tupcount"},				20, -1, 8, 2, 0, -1, -1, true, 'p', 'd', false, false, false, true, 0 }, \
{ -1, {"varblockcount"},		20, -1, 8, 3, 0, -1, -1, true, 'p', 'd', false, false, false, true, 0 }, \
{ -1, {"vpinfo"},				17, -1, -1, 4, 0, -1, -1, false, 'x', 'i', false, false, false, true, 0 }, \
{ -1, {"modcount"},				20, -1, 8, 5, 0, -1, -1, true, 'p', 'i', false, false, false, true, 0 }, \
{ -1, {"formatversion"},		21, -1, 2, 6, 0, -1, -1, true, 'p', 's', false, false, false, true, 0 }, \
{ -1, {"state"}, 				 21, -1, 2, 7, 0, -1, -1, true, 'p', 's', false, false, false, true, 0 }

/*
 * pg_aoseg_nnnnnn table values for FormData_pg_class.
 */
#define Class_pg_aocsseg \
  {"pg_appendonly"}, PG_CATALOG_NAMESPACE, -1, BOOTSTRAP_SUPERUSERID, 0, \
               -1, DEFAULTTABLESPACE_OID, \
               25, 10000, 0, 0, false, false, false, RELKIND_RELATION, RELSTORAGE_HEAP, Natts_pg_aocsseg, \
               0, false, false, false, false, false, FirstNormalTransactionId, {0}, {{{'\0','\0','\0','\0'},{'\0'}}}


typedef struct AOCSVPInfoEntry
{
	int64		eof;
	int64		eof_uncompressed;
} AOCSVPInfoEntry;

typedef struct AOCSVPInfo
{
	/* total len.  Have to be the very first */
	int32		_len;
	int32		version;
	int32		nEntry;

	/* Var len array */
	AOCSVPInfoEntry entry[1];
} AOCSVPInfo;

static inline int
aocs_vpinfo_size(int nvp)
{
	return offsetof(AOCSVPInfo, entry) +
		sizeof(AOCSVPInfoEntry) * nvp;
}
static inline AOCSVPInfo *
create_aocs_vpinfo(int nvp)
{
	AOCSVPInfo *vpinfo = palloc0(aocs_vpinfo_size(nvp));

	SET_VARSIZE(vpinfo, aocs_vpinfo_size(nvp));
	vpinfo->nEntry = nvp;
	return vpinfo;
}

/*
 * Descriptor of a single AO Col relation file segment.
 *
 * Note that the first three variables should be the same as
 * the AO Row relation file segment (see FileSegInfo). This is
 * implicitly used by the block directory to obtain those info
 * using the same structure -- FileSegInfo.
 */
typedef struct AOCSFileSegInfo
{
	TupleVisibilitySummary tupleVisibilitySummary;

	int32		segno;

	/*
	 * total number of tuples in the segment. This number includes invisible
	 * tuples
	 */
	int64		total_tupcount;
	int64		varblockcount;

	/*
	 * Number of data modification operations
	 */
	int64		modcount;

	/*
	 * state of the segno. The state is only maintained on the segments.
	 */
	FileSegInfoState state;

	int16		formatversion;

	/* Must be last */
	AOCSVPInfo	vpinfo;
} AOCSFileSegInfo;

static inline int
aocsfileseginfo_size(int nvp)
{
	return offsetof(AOCSFileSegInfo, vpinfo) +
		aocs_vpinfo_size(nvp);
}

static inline AOCSVPInfoEntry *
getAOCSVPEntry(AOCSFileSegInfo *psinfo, int vp)
{
	if (vp >= psinfo->vpinfo.nEntry)
		elog(ERROR, "Index %d exceed size of vpinfo array size %d",
			 vp, psinfo->vpinfo.nEntry);

	return &(psinfo->vpinfo.entry[vp]);
}

struct AOCSInsertDescData;
struct AOCSAddColumnDescData;

/*
 * GetAOCSFileSegInfo.
 *
 * Get the catalog entry for an appendonly (column-oriented) relation from the
 * pg_aocsseg_* relation that belongs to the currently used
 * AppendOnly table.
 *
 * If a caller intends to append to this (logical) file segment entry they must
 * already hold a relation Append-Only segment file (transaction-scope) lock (tag
 * LOCKTAG_RELATION_APPENDONLY_SEGMENT_FILE) in order to guarantee
 * stability of the pg_aoseg information on this segment file and exclusive right
 * to append data to the segment file.
 */
extern AOCSFileSegInfo *GetAOCSFileSegInfo(Relation prel,
				   Snapshot appendOnlyMetaDataSnapshot,
				   int32 segno);


extern AOCSFileSegInfo **GetAllAOCSFileSegInfo(Relation prel,
					  Snapshot appendOnlyMetaDataSnapshot,
					  int *totalseg);

extern AOCSFileSegInfo **GetAllAOCSFileSegInfo_pg_aocsseg_rel(
									 int numOfColumsn,
									 char *relationName,
									 Relation pg_aocsseg_rel,
									 Snapshot appendOnlyMetaDataSnapshot,
									 int32 *totalseg);

extern void FreeAllAOCSSegFileInfo(AOCSFileSegInfo **allAOCSSegInfo, int totalSegFiles);

extern int64 GetAOCSTotalBytes(
				  Relation parentrel,
				  Snapshot appendOnlyMetaDataSnapshot,
				  bool compressed);
extern FileSegTotals *GetAOCSSSegFilesTotals(Relation parentrel,
					   Snapshot appendOnlyMetaDataSnapshot);

extern AOCSFileSegInfo *NewAOCSFileSegInfo(int4 segno, int4 nvp);
extern void InsertInitialAOCSFileSegInfo(Relation prel, int32 segno, int32 nvp);
extern void UpdateAOCSFileSegInfo(struct AOCSInsertDescData *desc);
extern void AOCSFileSegInfoAddVpe(
					  Relation prel, int32 segno,
					  struct AOCSAddColumnDescData *desc, int num_newcols, bool empty);
extern void AOCSFileSegInfoAddCount(Relation prel, int32 segno, int64 tupadded, int64 varblockadded, int64 modcount_added);
extern void ClearAOCSFileSegInfo(Relation prel, int segno, FileSegInfoState newState);
extern void SetAOCSFileSegInfoState(Relation parentrel, int segno, FileSegInfoState newState);
extern Datum gp_update_aocol_master_stats_internal(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot);
extern Datum aocol_compression_ratio_internal(Relation parentrel);

#endif
