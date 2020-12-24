/*-------------------------------------------------------------------------
 *
 * aosegfiles.h
 *	  Internal specifications of the pg_aoseg_* Append Only file segment
 *	  list relation.
 *
 * Portions Copyright (c) 2008, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/aosegfiles.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef AOSEGFILES_H
#define AOSEGFILES_H

#include "catalog/pg_appendonly.h"
#include "utils/rel.h"
#include "utils/snapshot.h"
#include "utils/visibility_summary.h"

#define Natts_pg_aoseg					8
#define Anum_pg_aoseg_segno				1
#define Anum_pg_aoseg_eof				2
#define Anum_pg_aoseg_tupcount			3
#define Anum_pg_aoseg_varblockcount		4
#define Anum_pg_aoseg_eofuncompressed	5
#define Anum_pg_aoseg_modcount          6
#define Anum_pg_aoseg_formatversion     7
#define Anum_pg_aoseg_state             8

#define InvalidFileSegNumber			-1
#define InvalidUncompressedEof			-1

#define AO_FILESEGINFO_ARRAY_SIZE		8


/*
 * The state of a segment file.
 *
 * The state AOSEG_STATE_USECURRENT is a pseudo-state that is used
 * when a state parameter in a function should not change the
 * current state. It should not appear on disk.
 *
 * AOSEG_STATE_DEFAULT is the default state. The segment file can be used for
 * insertions and compactions. The contents if the segment file is visible
 * under the limitations of the visimap and the eof.
 *
 * AOSEG_STATE_AWAITING_DROP marks a segfile that has been compacted, but
 * there might still be old transactions that need it. The segment cannot be
 * used for insertions.
 */
typedef enum FileSegInfoState
{
	/* 0 is reserved */
	AOSEG_STATE_USECURRENT = 0,
	AOSEG_STATE_DEFAULT = 1,
	AOSEG_STATE_AWAITING_DROP = 2
} FileSegInfoState;


/*
 * Descriptor of a single AO relation file segment.
 */
typedef struct FileSegInfo
{
	TupleVisibilitySummary tupleVisibilitySummary;

	/* the file segment number */
	int			segno;

	/* total (i.e. including invisible) number of tuples in this fileseg */
	int64		total_tupcount;

	/* total number of varblocks in this fileseg */
	int64		varblockcount;

	/*
	 * Number of data modification operations
	 */
	int64		modcount;

	/* the effective eof for this segno  */
	int64		eof;

	/*
	 * what would have been the eof if we didn't compress this rel (= eof if
	 * no compression)
	 */
	int64		eof_uncompressed;

	/* File format version number */
	int16		formatversion;

	/*
	 * state of the segno. The state is only maintained on the segments.
	 */
	FileSegInfoState state;

} FileSegInfo;

/*
 * Structure that sums up the field total of all file 'segments'.
 * Note that even though we could actually use FileSegInfo for
 * this purpose we choose not too since it's likely that FileSegInfo
 * will go back to using int instead of float8 now that each segment
 * has a size limit.
 */
typedef struct FileSegTotals
{
	int			totalfilesegs;	/* total number of file segments */
	int64		totalbytes;		/* the sum of all 'eof' values  */
	int64		totaltuples;	/* the sum of all 'tupcount' values */
	int64		totalvarblocks; /* the sum of all 'varblockcount' values */
	int64		totalbytesuncompressed; /* the sum of all 'eofuncompressed'
										 * values */
} FileSegTotals;

extern void InsertInitialSegnoEntry(Relation parentrel, int segno);

extern void ValidateAppendonlySegmentDataBeforeStorage(int segno);

 /*
  * GetFileSegInfo
  *
  * Get the catalog entry for an appendonly (row-oriented) relation from the
  * pg_aoseg_* relation that belongs to the currently used AppendOnly table.
  *
  * If a caller intends to append to this file segment entry they must already
  * hold a relation Append-Only segment file (transaction-scope) lock (tag
  * LOCKTAG_RELATION_APPENDONLY_SEGMENT_FILE) in order to guarantee stability
  * of the pg_aoseg information on this segment file and exclusive right to
  * append data to the segment file.
  */
extern FileSegInfo *GetFileSegInfo(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot, int segno, bool locked);

extern FileSegInfo **GetAllFileSegInfo(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot, int *totalsegs);

extern void UpdateFileSegInfo(Relation parentrel,
				  int segno,
				  int64 eof,
				  int64 eof_uncompressed,
				  int64 tuples_added,
				  int64 varblocks_added,
				  int64 modcount_added,
				  FileSegInfoState newState);

extern void ClearFileSegInfo(Relation parentrel, int segno);
extern void MarkFileSegInfoAwaitingDrop(Relation parentrel, int segno);
extern void IncrementFileSegInfoModCount(Relation parentrel, int segno);
extern FileSegTotals *GetSegFilesTotals(Relation parentrel, Snapshot appendOnlyMetaDataSnapshot);

extern void FreeAllSegFileInfo(FileSegInfo **allSegInfo,
				   int totalSegFiles);

extern bool pg_aoseg_tuple_could_be_updated(Relation relation, HeapTuple tuple);
extern bool pg_aoseg_tuple_is_locked_by_me(HeapTuple tuple);

extern Datum get_ao_distribution(PG_FUNCTION_ARGS);
extern Datum get_ao_compression_ratio(PG_FUNCTION_ARGS);

#endif							/* AOSEGFILES_H */
