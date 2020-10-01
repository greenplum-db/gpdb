/*-------------------------------------------------------------------------
 *
 * index.h
 *	  prototypes for catalog/index.c.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/index.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef INDEX_H
#define INDEX_H

#include "access/relscan.h"     /* Relation, Snapshot */
#include "catalog/objectaddress.h"
#include "executor/tuptable.h"  /* TupTableSlot */
#include "nodes/execnodes.h"

struct EState;                  /* #include "nodes/execnodes.h" */

#define DEFAULT_INDEX_TYPE	"btree"

/* Action code for index_set_state_flags */
typedef enum
{
	INDEX_CREATE_SET_READY,
	INDEX_CREATE_SET_VALID,
	INDEX_DROP_CLEAR_VALID,
	INDEX_DROP_SET_DEAD
} IndexStateFlagsAction;

/* state info for validate_index bulkdelete callback */
typedef struct ValidateIndexState
{
	Tuplesortstate *tuplesort;	/* for sorting the index TIDs */
	/* statistics (for debug purposes only): */
	double		htups,
				itups,
				tups_inserted;
} ValidateIndexState;


extern bool relationHasPrimaryKey(Relation rel);
extern bool relationHasUniqueIndex(Relation rel);
extern void index_check_primary_key(Relation heapRel,
									IndexInfo *indexInfo,
									bool is_alter_table,
									IndexStmt *stmt);

#define	INDEX_CREATE_IS_PRIMARY				(1 << 0)
#define	INDEX_CREATE_ADD_CONSTRAINT			(1 << 1)
#define	INDEX_CREATE_SKIP_BUILD				(1 << 2)
#define	INDEX_CREATE_CONCURRENT				(1 << 3)
#define	INDEX_CREATE_IF_NOT_EXISTS			(1 << 4)
#define	INDEX_CREATE_PARTITIONED			(1 << 5)
#define INDEX_CREATE_INVALID				(1 << 6)

extern Oid index_create(Relation heapRelation,
						const char *indexRelationName,
						Oid indexRelationId,
						Oid parentIndexRelid,
						Oid parentConstraintId,
						Oid relFileNode,
						IndexInfo *indexInfo,
						List *indexColNames,
						Oid accessMethodObjectId,
						Oid tableSpaceId,
						Oid *collationObjectId,
						Oid *classObjectId,
						int16 *coloptions,
						Datum reloptions,
						bits16 flags,
						bits16 constr_flags,
						bool allow_system_table_mods,
						bool is_internal,
						Oid *constraintId);

#define	INDEX_CONSTR_CREATE_MARK_AS_PRIMARY	(1 << 0)
#define	INDEX_CONSTR_CREATE_DEFERRABLE		(1 << 1)
#define	INDEX_CONSTR_CREATE_INIT_DEFERRED	(1 << 2)
#define	INDEX_CONSTR_CREATE_UPDATE_INDEX	(1 << 3)
#define	INDEX_CONSTR_CREATE_REMOVE_OLD_DEPS	(1 << 4)

extern Oid	index_concurrently_create_copy(Relation heapRelation,
										   Oid oldIndexId,
										   const char *newName);

extern void index_concurrently_build(Oid heapRelationId,
									 Oid indexRelationId);

extern void index_concurrently_swap(Oid newIndexId,
									Oid oldIndexId,
									const char *oldName);

extern void index_concurrently_set_dead(Oid heapId,
										Oid indexId);

extern ObjectAddress index_constraint_create(Relation heapRelation,
											 Oid indexRelationId,
											 Oid parentConstraintId,
											 IndexInfo *indexInfo,
											 const char *constraintName,
											 char constraintType,
											 bits16 constr_flags,
											 bool allow_system_table_mods,
											 bool is_internal);

extern void index_drop(Oid indexId, bool concurrent, bool concurrent_lock_mode);

extern IndexInfo *BuildIndexInfo(Relation index);

extern bool CompareIndexInfo(IndexInfo *info1, IndexInfo *info2,
							 Oid *collations1, Oid *collations2,
							 Oid *opfamilies1, Oid *opfamilies2,
							 AttrNumber *attmap, int maplen);
extern void BuildSpeculativeIndexInfo(Relation index, IndexInfo *ii);
extern void FormIndexDatum(IndexInfo *indexInfo,
						   TupleTableSlot *slot,
						   EState *estate,
						   Datum *values,
						   bool *isnull);

extern Oid setNewRelfilenodeToOid(Relation relation, TransactionId freezeXid,
					   Oid newrelfilenode);

extern void index_build(Relation heapRelation,
						Relation indexRelation,
						IndexInfo *indexInfo,
						bool isreindex,
						bool parallel);

extern void validate_index(Oid heapId, Oid indexId, Snapshot snapshot);

extern void index_set_state_flags(Oid indexId, IndexStateFlagsAction action);

extern void reindex_index(Oid indexId, bool skip_constraint_checks,
						  char relpersistence, int options);

/* Flag bits for reindex_relation(): */
#define REINDEX_REL_PROCESS_TOAST			0x01
#define REINDEX_REL_SUPPRESS_INDEX_USE		0x02
#define REINDEX_REL_CHECK_CONSTRAINTS		0x04
#define REINDEX_REL_FORCE_INDEXES_UNLOGGED	0x08
#define REINDEX_REL_FORCE_INDEXES_PERMANENT 0x10

/* GPDB: set when recursing on a partitioned table */
#define REINDEX_REL_RECURSING_PARTITIONED_TABLE 0x80

extern bool reindex_relation(Oid relid, int flags, int options);

extern bool ReindexIsProcessingHeap(Oid heapOid);
extern bool ReindexIsProcessingIndex(Oid indexOid);
extern Oid	IndexGetRelation(Oid indexId, bool missing_ok);

extern Size EstimateReindexStateSpace(void);
extern void SerializeReindexState(Size maxsize, char *start_address);
extern void RestoreReindexState(void *reindexstate);

extern void IndexSetParentIndex(Relation idx, Oid parentOid);


/*
 * itemptr_encode - Encode ItemPointer as int64/int8
 *
 * This representation must produce values encoded as int64 that sort in the
 * same order as their corresponding original TID values would (using the
 * default int8 opclass to produce a result equivalent to the default TID
 * opclass).
 *
 * As noted in validate_index(), this can be significantly faster.
 */
static inline int64
itemptr_encode(ItemPointer itemptr)
{
	BlockNumber block = ItemPointerGetBlockNumber(itemptr);
	OffsetNumber offset = ItemPointerGetOffsetNumber(itemptr);
	int64		encoded;

	/*
	 * Use the 16 least significant bits for the offset.  32 adjacent bits are
	 * used for the block number.  Since remaining bits are unused, there
	 * cannot be negative encoded values (We assume a two's complement
	 * representation).
	 */
	encoded = ((uint64) block << 16) | (uint16) offset;

	return encoded;
}

/*
 * itemptr_decode - Decode int64/int8 representation back to ItemPointer
 */
static inline void
itemptr_decode(ItemPointer itemptr, int64 encoded)
{
	BlockNumber block = (BlockNumber) (encoded >> 16);
	OffsetNumber offset = (OffsetNumber) (encoded & 0xFFFF);

	ItemPointerSet(itemptr, block, offset);
}

#endif							/* INDEX_H */
