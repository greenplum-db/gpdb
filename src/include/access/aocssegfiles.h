/*-------------------------------------------------------------------------
 *
 * aocssegfiles.h
 *      AOCS segment files
 *
 * Portions Copyright (c) 2009, Greenplum INC.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "utils/snapshot.h"

#define Natts_pg_aocsseg 7
#define Anum_pg_aocs_segno 1
#define Anum_pg_aocs_tupcount 2
#define Anum_pg_aocs_varblockcount 3
#define Anum_pg_aocs_vpinfo 4
#define Anum_pg_aocs_modcount 5
#define Anum_pg_aocs_formatversion 6
#define Anum_pg_aocs_state 7


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
	AOCSVPInfo *vpinfo = (AOCSVPInfo *) palloc0(aocs_vpinfo_size(nvp));

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
										   int32 segno, bool locked);


extern AOCSFileSegInfo **GetAllAOCSFileSegInfo(Relation prel,
					  Snapshot appendOnlyMetaDataSnapshot,
					  int *totalseg);
extern void FreeAllAOCSSegFileInfo(AOCSFileSegInfo **allAOCSSegInfo, int totalSegFiles);

extern FileSegTotals *GetAOCSSSegFilesTotals(Relation parentrel,
					   Snapshot appendOnlyMetaDataSnapshot);

extern void InsertInitialAOCSFileSegInfo(Relation prel, int32 segno, int32 nvp, Oid segrelid);
extern void UpdateAOCSFileSegInfo(struct AOCSInsertDescData *desc);
extern void AOCSFileSegInfoAddVpe(
					  Relation prel, int32 segno,
					  struct AOCSAddColumnDescData *desc, int num_newcols, bool empty);
extern void AOCSFileSegInfoAddCount(Relation prel, int32 segno, int64 tupadded, int64 varblockadded, int64 modcount_added);
extern void ClearAOCSFileSegInfo(Relation prel, int segno);
extern void MarkAOCSFileSegInfoAwaitingDrop(Relation parentrel, int segno);
extern float8 aocol_compression_ratio_internal(Relation parentrel);

#endif
