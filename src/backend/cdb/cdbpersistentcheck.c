/*-------------------------------------------------------------------------
 *
 * cdbpersistentcheck.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpersistentcheck.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/palloc.h"
#include "libpq-fe.h"

#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentcheck.h"
#include "cdb/cdbtm.h"
#include "cdb/cdbdisp_query.h"

#include "storage/itemptr.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/fmgroids.h"
#include "utils/faultinjector.h"
#include "utils/snapmgr.h"
#include "storage/smgr.h"
#include "storage/ipc.h"
#include "cdb/cdbglobalsequence.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentrecovery.h"
#include "miscadmin.h"

#define PTCHECK_LOG_LEVEL (debug_print_persistent_checks ? LOG : DEBUG5)

#define PTCHECK_RETURN_IF_DISABLED() \
{ \
	if (persistent_integrity_checks == false) \
	{ \
		return; \
	} \
}

typedef struct PT_GpPersistentRelationNode
{
	Oid			tablespaceOid;
	Oid			databaseOid;
	Oid			relfilenodeOid;
	int32		segmentFileNum;
	PersistentFileSysRelStorageMgr relationStorageManager;
	PersistentFileSysState persistentState;
	int64		createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState mirrorExistenceState;
	MirroredRelDataSynchronizationState mirrorDataSynchronizationState;
	bool		mirrorBufpoolMarkedForScanIncrementalResync;
	int64		mirrorBufpoolResyncChangedPageCount;
	XLogRecPtr	mirrorBufpoolResyncCkptLoc;
	BlockNumber mirrorBufpoolResyncCkptBlockNum;
	int64		mirrorAppendOnlyLossEof;
	int64		mirrorAppendOnlyNewEof;
	PersistentFileSysRelBufpoolKind relBufpoolKind;
	TransactionId parentXid;
	int64		persistentSerialNum;
} PT_GpPersistentRelationNode;

static inline void
GpPersistentRelationNodeGetValues(Datum *values, PT_GpPersistentRelationNode *relnode)
{
	GpPersistentRelationNode_GetValues(
									   values,
									   &relnode->tablespaceOid,
									   &relnode->databaseOid,
									   &relnode->relfilenodeOid,
									   &relnode->segmentFileNum,
									   &relnode->relationStorageManager,
									   &relnode->persistentState,
									   &relnode->createMirrorDataLossTrackingSessionNum,
									   &relnode->mirrorExistenceState,
									   &relnode->mirrorDataSynchronizationState,
									   &relnode->mirrorBufpoolMarkedForScanIncrementalResync,
									   &relnode->mirrorBufpoolResyncChangedPageCount,
									   &relnode->mirrorBufpoolResyncCkptLoc,
									   &relnode->mirrorBufpoolResyncCkptBlockNum,
									   &relnode->mirrorAppendOnlyLossEof,
									   &relnode->mirrorAppendOnlyNewEof,
									   &relnode->relBufpoolKind,
									   &relnode->parentXid,
									   &relnode->persistentSerialNum);
}

typedef struct PT_GpPersistentDatabaseNode
{
	Oid			tablespaceOid;
	Oid			databaseOid;
	PersistentFileSysState persistentState;
	int64		createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState mirrorExistenceState;
	int32		reserved;
	TransactionId parentXid;
	int64		persistentSerialNum;
} PT_GpPersistentDatabaseNode;

static inline void
GpPersistentDatabaseNodeGetValues(Datum *values, PT_GpPersistentDatabaseNode *dbnode)
{
	GpPersistentDatabaseNode_GetValues(
									   values,
									   &dbnode->tablespaceOid,
									   &dbnode->databaseOid,
									   &dbnode->persistentState,
									   &dbnode->createMirrorDataLossTrackingSessionNum,
									   &dbnode->mirrorExistenceState,
									   &dbnode->reserved,
									   &dbnode->parentXid,
									   &dbnode->persistentSerialNum);
}


typedef struct PT_GpPersistentTablespaceNode
{
	Oid			filespaceOid;
	Oid			tablespaceOid;
	PersistentFileSysState persistentState;
	int64		createMirrorDataLossTrackingSessionNum;
	MirroredObjectExistenceState mirrorExistenceState;
	int32		reserved;
	TransactionId parentXid;
	int64		persistentSerialNum;
} PT_GpPersistentTablespaceNode;


static inline void
GpPersistentTablespaceNodeGetValues(Datum *values, PT_GpPersistentTablespaceNode *tablenode)
{
	GpPersistentTablespaceNode_GetValues(
										 values,
										 &tablenode->filespaceOid,
										 &tablenode->tablespaceOid,
										 &tablenode->persistentState,
										 &tablenode->createMirrorDataLossTrackingSessionNum,
										 &tablenode->mirrorExistenceState,
										 &tablenode->reserved,
										 &tablenode->parentXid,
										 &tablenode->persistentSerialNum);
}


/*Pass 4 - Catalog cross consistency queries & data structures*/

typedef struct
{
	char	   *quertTitle;
	char	   *queryStr;
	bool		executeWhenInSyncOnly;

}			query;

#define NONDB_SPECIFIC_PTCAT_VERIFICATION_NUM_QUERIES 14
query		NonDBSpecific_PTCat_Verification_queries[] =
{
/* 1 */
	{"gp_persistent_filespace_node state check",
		" SELECT p.filespace_oid,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" case when p.mirror_existence_state = 0 then 'mirror free'"
		" when p.mirror_existence_state = 1 then 'not mirrored'"
		" when p.mirror_existence_state = 2 then 'mirror create pending'"
		" when p.mirror_existence_state = 3 then 'mirror created'"
		" when p.mirror_existence_state = 4 then 'mirror down before create'"
		" when p.mirror_existence_state = 5 then 'mirror down during create'"
		" when p.mirror_existence_state = 6 then 'mirror drop pending'"
		" when p.mirror_existence_state = 7 then 'mirror only drop remains'"
		" else 'unknown state: ' || p.mirror_existence_state"
		" end as mirror_existence_state"
		" FROM gp_persistent_filespace_node p"
		" WHERE p.persistent_state not in (0, 2)"
	" or p.mirror_existence_state not in (0,1,3)", true},
/* 2 */
	{"gp_persistent_filespace_node <=> pg_filespace",
		" SELECT  coalesce(f.oid, p.filespace_oid) as filespace_oid,"
		" f.fsname as filespace"
		" FROM (SELECT * FROM gp_persistent_filespace_node"
		" WHERE persistent_state = 2) p"
		" FULL OUTER JOIN (SELECT oid, fsname FROM pg_filespace"
		" WHERE oid != 3052) f"
		" ON (p.filespace_oid = f.oid)"
	" WHERE  (p.filespace_oid is NULL OR f.oid is NULL)", false},
/* 3 */
	{"gp_persistent_filespace_node <=> gp_global_sequence",
		" SELECT  p.filespace_oid, f.fsname as filespace,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" p.persistent_serial_num, s.sequence_num"
		" FROM    gp_global_sequence s, gp_persistent_filespace_node p"
		" LEFT JOIN pg_filespace f ON (f.oid = p.filespace_oid)"
		" WHERE   s.ctid = '(0,4)' and p.persistent_serial_num > s.sequence_num",
	false},
/* 4 */
	{"gp_persistent_database_node state check",
		" SELECT p.tablespace_oid, p.database_oid,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" case when p.mirror_existence_state = 0 then 'mirror free'"
		" when p.mirror_existence_state = 1 then 'not mirrored'"
		" when p.mirror_existence_state = 2 then 'mirror create pending'"
		" when p.mirror_existence_state = 3 then 'mirror created'"
		" when p.mirror_existence_state = 4 then 'mirror down before create'"
		" when p.mirror_existence_state = 5 then 'mirror down during create'"
		" when p.mirror_existence_state = 6 then 'mirror drop pending'"
		" when p.mirror_existence_state = 7 then 'mirror only drop remains'"
		" else 'unknown state: ' || p.mirror_existence_state"
		" end as mirror_existence_state"
		" FROM gp_persistent_database_node p"
		" WHERE p.persistent_state not in (0, 2)"
	" or p.mirror_existence_state not in (0,1,3)", true},
/* 5 */
	{"gp_persistent_database_node <=> pg_database",
		" SELECT coalesce(d.oid, p.database_oid) as database_oid,"
		" d.datname as database"
		" FROM (SELECT * FROM gp_persistent_database_node"
		" WHERE persistent_state = 2) p"
		" FULL OUTER JOIN pg_database d"
		" ON (d.oid = p.database_oid)"
	" WHERE (d.datname is null or p.database_oid is null)", false},
/* 6 */
	{"gp_persistent_database_node <=> pg_tablespace",
		" SELECT  coalesce(t.oid, p.database_oid) as database_oid,"
		" t.spcname as tablespace"
		" FROM (SELECT * FROM gp_persistent_database_node"
		" WHERE persistent_state = 2) p"
		" LEFT OUTER JOIN (SELECT oid, spcname FROM pg_tablespace"
		" WHERE oid != 1664) t"
		" ON (t.oid = p.tablespace_oid)"
	" WHERE  t.spcname is null", false},
/* 7 */
	{"gp_persistent_database_node   <=> gp_global_sequence",
		" SELECT  p.database_oid, p.tablespace_oid, d.datname as database,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" p.persistent_serial_num, s.sequence_num"
		" FROM    gp_global_sequence s, gp_persistent_database_node p"
		" LEFT JOIN pg_database d ON (d.oid = p.database_oid)"
		" WHERE   s.ctid = '(0,2)' and p.persistent_serial_num > s.sequence_num",
	false},
/* 8 */
	{"gp_persistent_tablespace_node state check",
		" SELECT p.filespace_oid, p.tablespace_oid,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" case when p.mirror_existence_state = 0 then 'mirror free'"
		" when p.mirror_existence_state = 1 then 'not mirrored'"
		" when p.mirror_existence_state = 2 then 'mirror create pending'"
		" when p.mirror_existence_state = 3 then 'mirror created'"
		" when p.mirror_existence_state = 4 then 'mirror down before create'"
		" when p.mirror_existence_state = 5 then 'mirror down during create'"
		" when p.mirror_existence_state = 6 then 'mirror drop pending'"
		" when p.mirror_existence_state = 7 then 'mirror only drop remains'"
		" else 'unknown state: ' || p.mirror_existence_state"
		" end as mirror_existence_state"
		" FROM gp_persistent_tablespace_node p"
		" WHERE p.persistent_state not in (0, 2)"
		" or p.mirror_existence_state not in (0,1,3)",
	true},
/* 9 */
	{"gp_persistent_tablespace_node <=> pg_tablespace",
		" SELECT  coalesce(t.oid, p.tablespace_oid) as tablespace_oid,"
		" t.spcname as tablespace"
		" FROM (SELECT * FROM gp_persistent_tablespace_node"
		" WHERE persistent_state = 2) p"
		" FULL OUTER JOIN ("
		" SELECT oid, spcname FROM pg_tablespace WHERE oid not in (1663, 1664)"
		" ) t ON (t.oid = p.tablespace_oid)"
	" WHERE  t.spcname is null or p.tablespace_oid is null", false},
/* 10 */
	{"gp_persistent_tablespace_node <=> pg_filespace",
		" SELECT  p.filespace_oid, f.fsname as filespace"
		" FROM (SELECT * FROM gp_persistent_tablespace_node"
		" WHERE persistent_state = 2) p"
		" LEFT OUTER JOIN pg_filespace f"
		" ON (f.oid = p.filespace_oid)"
	" WHERE  f.fsname is null", false},
/* 11 */
	{"gp_persistent_tablespace_node <=> gp_global_sequence",
		" SELECT  p.filespace_oid, p.tablespace_oid, t.spcname as tablespace,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" p.persistent_serial_num, s.sequence_num"
		" FROM    gp_global_sequence s, gp_persistent_tablespace_node p"
		" LEFT JOIN pg_tablespace t ON (t.oid = p.tablespace_oid)"
		" WHERE   s.ctid = '(0,3)' and p.persistent_serial_num > s.sequence_num",
	false},
/* 12 */
	{"gp_persistent_relation_node <=> pg_database",
		" SELECT  datname, oid, count(*)"
		" FROM ("
		" SELECT  d.datname as datname, p.database_oid as oid"
		" FROM (SELECT * FROM gp_persistent_relation_node"
		" WHERE database_oid != 0 and persistent_state = 2"
		" ) p"
		" full outer join pg_database d ON (d.oid = p.database_oid)"
		" ) x"
		" GROUP BY 1,2"
		" HAVING  datname is null or oid is null or count(*) < 100",
	false},
/* 13 */
	{"gp_persistent_relation_node <=> gp_global_sequence",
		" SELECT  p.tablespace_oid, p.database_oid, p.relfilenode_oid,"
		" p.segment_file_num,"
		" case when p.persistent_state = 0 then 'free'"
		" when p.persistent_state = 1 then 'create pending'"
		" when p.persistent_state = 2 then 'created'"
		" when p.persistent_state = 3 then 'drop pending'"
		" when p.persistent_state = 4 then 'abort create'"
		" when p.persistent_state = 5 then 'JIT create pending'"
		" when p.persistent_state = 6 then 'bulk load create pending'"
		" else 'unknown state: ' || p.persistent_state"
		" end as persistent_state,"
		" p.persistent_serial_num, s.sequence_num"
		" FROM    gp_global_sequence s, gp_persistent_relation_node p"
		" LEFT JOIN pg_tablespace t ON (t.oid = p.tablespace_oid)"
		" WHERE   s.ctid = '(0,1)' and p.persistent_serial_num > s.sequence_num",
	false},

/* 14 */
	{"pg_database <=> filesystem",
		" SELECT tablespace_oid, database_oid, count(*)"
		" FROM   gp_persistent_relation_node_check() p"
		" LEFT OUTER JOIN pg_database d"
		" ON (p.database_oid = d.oid)"
		" WHERE  d.oid is null and database_oid != 0"
		" GROUP BY tablespace_oid, database_oid",
	false}
};


#define DB_SPECIFIC_PTCAT_VERIFICATION_NUM_QUERIES 5
static query DB_PTCat_Veritifcation_queries[] =
{
/* 1 */
	{
		"gp_persistent_relation_node state check",
			" SELECT p.tablespace_oid, p.relfilenode_oid, p.segment_file_num,"
			" case when p.persistent_state = 0 then 'free'"
			" when p.persistent_state = 1 then 'create pending'"
			" when p.persistent_state = 2 then 'created'"
			" when p.persistent_state = 3 then 'drop pending'"
			" when p.persistent_state = 4 then 'abort create'"
			" when p.persistent_state = 5 then 'JIT create pending'"
			" when p.persistent_state = 6 then 'bulk load create pending'"
			" else 'unknown state: ' || p.persistent_state"
			" end as persistent_state,"
			" case when p.mirror_existence_state = 0 then 'mirror free'"
			" when p.mirror_existence_state = 1 then 'not mirrored'"
			" when p.mirror_existence_state = 2 then 'mirror create pending'"
			" when p.mirror_existence_state = 3 then 'mirror created'"
			" when p.mirror_existence_state = 4 then 'mirror down before create'"
			" when p.mirror_existence_state = 5 then 'mirror down during create'"
			" when p.mirror_existence_state = 6 then 'mirror drop pending'"
			" when p.mirror_existence_state = 7 then 'mirror only drop remains'"
			" else 'unknown state: ' || p.mirror_existence_state"
			" end as mirror_existence_state"
			" FROM gp_persistent_relation_node p"
			" WHERE (p.persistent_state not in (0, 2)"
			" or p.mirror_existence_state not in (0,1,3))"
			" and p.database_oid in ("
			" SELECT oid FROM pg_database WHERE datname = current_database()"
			" )", true
	},
/* 2 */
	{
		"gp_persistent_relation_node <=> pg_tablespace",
			" SELECT  distinct p.tablespace_oid"
			" FROM (SELECT * FROM gp_persistent_relation_node"
			" WHERE persistent_state = 2"
			" AND database_oid in ("
			" SELECT oid FROM pg_database"
			" WHERE datname = current_database()"
			" UNION ALL"
			" SELECT 0)) p"
			" LEFT OUTER JOIN pg_tablespace t"
			" ON (t.oid = p.tablespace_oid)"
			" WHERE  t.oid is null", false
	},
/* 3 */
	{
		"gp_persistent_relation_node <=> gp_relation_node",
			" SELECT  coalesce(p.relfilenode_oid, r.relfilenode_oid) as relfilenode,"
			" p.ctid, r.persistent_tid"
			" FROM  ("
			" SELECT p.ctid, p.* FROM gp_persistent_relation_node p"
			" WHERE persistent_state = 2 AND p.database_oid in ("
			" SELECT oid FROM pg_database WHERE datname = current_database()"
			" UNION ALL"
			" SELECT 0"
			" )"
			" ) p"
			" FULL OUTER JOIN gp_relation_node r"
			" ON (p.relfilenode_oid = r.relfilenode_oid and"
			" p.segment_file_num = r.segment_file_num)"
			" WHERE  (p.relfilenode_oid is NULL OR"
			" r.relfilenode_oid is NULL OR"
			" p.ctid != r.persistent_tid)", false
	},
/* 4 */
	{
		"gp_persistent_relation_node <=> pg_class",
			" SELECT  coalesce(p.relfilenode_oid, c.relfilenode) as relfilenode,"
			" c.nspname, c.relname, c.relkind, c.relstorage"
			" FROM ("
			" SELECT * FROM gp_persistent_relation_node"
			" WHERE persistent_state = 2 AND database_oid in ("
			" SELECT oid FROM pg_database WHERE datname = current_database()"
			" UNION ALL"
			" SELECT 0"
			" )"
			" ) p"
			" FULL OUTER JOIN ("
			" SELECT  n.nspname, c.relname, c.relfilenode, c.relstorage, c.relkind"
			" FROM  pg_class c"
			" LEFT OUTER JOIN pg_namespace n ON (c.relnamespace = n.oid)"
			" WHERE  c.relstorage not in ('v', 'x', 'f')"
			" ) c ON (p.relfilenode_oid = c.relfilenode)"
			" WHERE  p.relfilenode_oid is NULL OR c.relfilenode is NULL", false
	},
/* 5 */
	{
		"gp_persistent_relation_node <=> filesystem",
			" SELECT coalesce(a.tablespace_oid, b.tablespace_oid) as tablespace_oid,"
			" coalesce(a.database_oid, b.database_oid) as database_oid,"
			" coalesce(a.relfilenode_oid, b.relfilenode_oid) as relfilenode_oid,"
			" coalesce(a.segment_file_num, b.segment_file_num) as segment_file_num,"
			" a.relfilenode_oid is null as filesystem,"
			" b.relfilenode_oid is null as persistent,"
			" b.relkind, b.relstorage"
			" FROM   gp_persistent_relation_node a"
			" FULL OUTER JOIN ("
			" SELECT p.*, c.relkind, c.relstorage"
			" FROM   gp_persistent_relation_node_check() p"
			" LEFT OUTER JOIN pg_class c"
			" ON (p.relfilenode_oid = c.relfilenode)"
			" WHERE (p.segment_file_num = 0 or c.relstorage != 'h')"
			" ) b ON (a.tablespace_oid   = b.tablespace_oid    and"
			" a.database_oid     = b.database_oid      and"
			" a.relfilenode_oid  = b.relfilenode_oid   and"
			" a.segment_file_num = b.segment_file_num)"
			" WHERE (a.relfilenode_oid is null OR"
			" (a.persistent_state = 2 and b.relfilenode_oid is null))  and"
			" coalesce(a.database_oid, b.database_oid) in ("
			" SELECT oid FROM pg_database WHERE datname = current_database()"
			" UNION ALL"
			" SELECT 0"
			" )", false
	}
};


static bool connected = false;
static ResourceOwner savedResourceOwner = NULL;
static MemoryContext oldMemoryContext = NULL;

/*Pass 4 - End*/

/* Post DTM Recovery Verification related */
static PT_PostDTMRecv_Data *PT_PostDTMRecv_Info = NULL;

/*
 * Function to check existence of entry in table with specified values.
 * If entry exist throw the error else just return fine.
 *
 * Intention is to block any duplicate entries from getting IN,
 * hence must be called from every place trying to add entry to PT relation table.
 */
void
PTCheck_BeforeAddingEntry(PersistentStoreData *storeData, Datum *values)
{
	PTCHECK_RETURN_IF_DISABLED();

	elog(PTCHECK_LOG_LEVEL, "PTCheck: Checking before adding entry to PT");

	int			nKey = 0;
	bool		allowDuplicates = false;
	bool		status = true;
	ItemPointerData iptr;

	ScanKey		key = (*storeData->scanKeyInitCallback) (values, &nKey);

	if (key == NULL)
	{
		return;
	}

	Datum	   *existing_values = (Datum *) palloc(storeData->numAttributes * sizeof(Datum));

	if (existing_values == NULL)
	{
		elog(LOG, "PTCheck: Failed to allocate memory for existing_values datastructure");
		return;
	}

	/*
	 * Lets scan the table and fetch the entry with the key
	 */
	Relation	persistentRel = (*storeData->openRel) ();

	HeapScanDesc scan = heap_beginscan(persistentRel, SnapshotNow, nKey, key);

	HeapTuple	tuple = heap_getnext(scan, ForwardScanDirection);

	while (tuple != NULL)
	{
		PersistentStore_DeformTuple(storeData, persistentRel->rd_att, tuple, existing_values);

		allowDuplicates = (*storeData->allowDuplicateCallback) (existing_values, values);
		if (allowDuplicates == false)
		{
			/*
			 * Not expecting duplicate, MUST error-out then
			 */
			status = false;
			break;
		}

		(*storeData->printTupleCallback) (LOG, "PTCheck insert", &iptr, values);
		(*storeData->printTupleCallback) (LOG, "PTCheck allowing with duplicate", &iptr, existing_values);

		/*
		 * Callback returned its exception case and allow duplicate for this,
		 * hence proceed forward to check
		 */
		tuple = heap_getnext(scan, ForwardScanDirection);
	}

	heap_endscan(scan);

	(*storeData->closeRel) (persistentRel);

	if (status == false)
	{
		(*storeData->printTupleCallback) (LOG, "PTCheck insert", &iptr, values);
		(*storeData->printTupleCallback) (LOG, "PTCheck conflicts with duplicate", &iptr, existing_values);
		ereport(ERROR,
				(ERRCODE_INTERNAL_ERROR,
				 errmsg("PTCheck: Failed object entry already exist.")));
	}

	pfree(existing_values);
	pfree(key);
}

ScanKey
Persistent_RelationScanKeyInit(Datum *values, int *nKeys)
{
	PT_GpPersistentRelationNode relnode;

	GpPersistentRelationNodeGetValues(values, &relnode);

	/* tablespace_oid, database_oid, relfilenode_oid and segment_file_num */
	*nKeys = 4;
	ScanKey		key = palloc0(*nKeys * sizeof(ScanKeyData));

	if (key == NULL)
	{
		return NULL;
	}

	/*
	 * We needn't fill in sk_strategy or sk_subtype since these scankeys will
	 * never be passed to an index.
	 */
	ScanKeyInit(&key[0], Anum_gp_persistent_relation_node_tablespace_oid, InvalidStrategy, F_OIDEQ, (Datum) relnode.tablespaceOid);
	ScanKeyInit(&key[1], Anum_gp_persistent_relation_node_database_oid, InvalidStrategy, F_OIDEQ, (Datum) relnode.databaseOid);
	ScanKeyInit(&key[2], Anum_gp_persistent_relation_node_relfilenode_oid, InvalidStrategy, F_OIDEQ, (Datum) relnode.relfilenodeOid);
	ScanKeyInit(&key[3], Anum_gp_persistent_relation_node_segment_file_num, InvalidStrategy, F_INT4EQ, (Datum) relnode.segmentFileNum);

	return key;
}


/*
 * Any exceptions intended to allow duplicate entries need to be checked here
 * and for those specific cases returned TRUE and logged.
 * For all rest of the cases default is to return false to avoid duplicate entry.
 * Currently, this code is shared between realtion, database, tablespace and filespace.
 * If in future exceptions vary for the same can write different routine for the same.
 */
static inline bool
Persistent_AllowDuplicateEntry(PersistentFileSysState old_persistentState,
							   MirroredObjectExistenceState old_mirrorExistenceState,
							   PersistentFileSysState new_persistentState,
							   MirroredObjectExistenceState new_mirrorExistenceState)
{
	/*
	 * Currently, only one exception hence coidng with ifs, as not many
	 * exceptions are expected.
	 */
	if ((old_persistentState == PersistentFileSysState_AbortingCreate) &&
		(old_mirrorExistenceState == MirroredObjectExistenceState_OnlyMirrorDropRemains))
	{
		if ((new_persistentState == PersistentFileSysState_CreatePending) &&
			(new_mirrorExistenceState == MirroredObjectExistenceState_MirrorDownBeforeCreate))
		{
			return true;
		}
	}

	return false;
}

bool
Persistent_RelationAllowDuplicateEntry(Datum *exist_values, Datum *new_values)
{
	PT_GpPersistentRelationNode old_relnode;
	PT_GpPersistentRelationNode new_relnode;

	GpPersistentRelationNodeGetValues(exist_values, &old_relnode);
	GpPersistentRelationNodeGetValues(new_values, &new_relnode);

	return Persistent_AllowDuplicateEntry(old_relnode.persistentState,
										  old_relnode.mirrorExistenceState,
										  new_relnode.persistentState,
										  new_relnode.mirrorExistenceState);
}

ScanKey
Persistent_DatabaseScanKeyInit(Datum *values, int *nKeys)
{
	PT_GpPersistentDatabaseNode dbnode;

	GpPersistentDatabaseNodeGetValues(values, &dbnode);

	/* tablespace_oid and database_oid */
	*nKeys = 2;
	ScanKey		key = palloc0(*nKeys * sizeof(ScanKeyData));

	if (key == NULL)
	{
		return NULL;
	}

	/*
	 * We needn't fill in sk_strategy or sk_subtype since these scankeys will
	 * never be passed to an index.
	 */
	ScanKeyInit(&key[0], Anum_gp_persistent_database_node_tablespace_oid, InvalidStrategy, F_OIDEQ, (Datum) dbnode.tablespaceOid);
	ScanKeyInit(&key[1], Anum_gp_persistent_database_node_database_oid, InvalidStrategy, F_OIDEQ, (Datum) dbnode.databaseOid);

	return key;
}

bool
Persistent_DatabaseAllowDuplicateEntry(Datum *exist_values, Datum *new_values)
{
	/*
	 * Any exceptions intended to allow duplicate entries need to be checked
	 * here and for those specific cases returned TRUE and logged. For all
	 * rest of the cases default is to return false to avoid duplicate entry.
	 */

	/* For Database we expect no duplicates */
	return false;
}

ScanKey
Persistent_TablespaceScanKeyInit(Datum *values, int *nKeys)
{
	PT_GpPersistentTablespaceNode tablespacenode;

	GpPersistentTablespaceNodeGetValues(values, &tablespacenode);

	/* filespace_oid and tablespace_oid */
	*nKeys = 2;
	ScanKey		key = palloc0(*nKeys * sizeof(ScanKeyData));

	if (key == NULL)
	{
		return NULL;
	}

	/*
	 * We needn't fill in sk_strategy or sk_subtype since these scankeys will
	 * never be passed to an index.
	 */
	ScanKeyInit(&key[0], Anum_gp_persistent_tablespace_node_filespace_oid, InvalidStrategy, F_OIDEQ, (Datum) tablespacenode.filespaceOid);
	ScanKeyInit(&key[1], Anum_gp_persistent_tablespace_node_tablespace_oid, InvalidStrategy, F_OIDEQ, (Datum) tablespacenode.tablespaceOid);

	return key;
}

bool
Persistent_TablespaceAllowDuplicateEntry(Datum *exist_values, Datum *new_values)
{
	/*
	 * Any exceptions intended to allow duplicate entries need to be checked
	 * here and for those specific cases returned TRUE and logged. For all
	 * rest of the cases default is to return false to avoid duplicate entry.
	 */


	/* For TableSpace we expect no duplicates */
	return false;
}

ScanKey
Persistent_FilespaceScanKeyInit(Datum *values, int *nKeys)
{
	return NULL;
}

bool
Persistent_FilespaceAllowDuplicateEntry(Datum *exist_values, Datum *new_values)
{
	/*
	 * Any exceptions intended to allow duplicate entries need to be checked
	 * here and for those specific cases returned TRUE and logged. For all
	 * rest of the cases default is to return false to avoid duplicate entry.
	 */

	return false;
}

/*
 * *******Post DTM Recovery Verification - Hash helper functions *****
 */

/*
 * Currently, we'll limit number of databases to be verified post DTM
 * recovery to 10. As these verification checks will be used mostly
 * for internal development/QA purposes it can modified as and when
 * needed. */
#define PT_MAX_NUM_POSTDTMRECV_DB 10

Size
Persistent_PostDTMRecv_ShmemSize(void)
{
	Size		size;

	size = hash_estimate_size((Size) PT_MAX_NUM_POSTDTMRECV_DB, sizeof(postDTMRecv_dbTblSpc_Hash_Entry));
	size = add_size(size, sizeof(PT_PostDTMRecv_Data));

	return size;
}

void
Persistent_PostDTMRecv_ShmemInit(void)
{
	HASHCTL		info;
	int			hash_flags;
	bool		foundPtr;

	PT_PostDTMRecv_Info =
		(PT_PostDTMRecv_Data *)
		ShmemInitStruct("Post DTM Recovery Checks Info",
						sizeof(PT_PostDTMRecv_Data),
						&foundPtr);

	if (PT_PostDTMRecv_Info == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for post DTM recv. checks"))));
	}

	if (!foundPtr)
	{
		MemSet(PT_PostDTMRecv_Info, 0, sizeof(PT_PostDTMRecv_Data));
	}

	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(Oid);
	info.entrysize = sizeof(postDTMRecv_dbTblSpc_Hash_Entry);
	info.hash = tag_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash =
		ShmemInitHash("Post DTM Recv dbtblspc hash",
					  PT_MAX_NUM_POSTDTMRECV_DB,
					  PT_MAX_NUM_POSTDTMRECV_DB,
					  &info,
					  hash_flags);

	if (PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 (errmsg("not enough shared memory for post DTM recv. checks"))));
	}
}

postDTMRecv_dbTblSpc_Hash_Entry *
Persistent_PostDTMRecv_InsertHashEntry(Oid dbId, postDTMRecv_dbTblSpc_Hash_Entry *values, bool *exists)
{
	bool		foundPtr;
	postDTMRecv_dbTblSpc_Hash_Entry *entry;

	Insist(PT_PostDTMRecv_Info);
	Insist(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash != NULL);

	entry = (postDTMRecv_dbTblSpc_Hash_Entry *) hash_search(
															PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash,
															(void *) &dbId,
															HASH_ENTER,
															&foundPtr);
	if (entry == NULL)
	{
		ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
						(errmsg("Not enough shared memory"))));
	}

	if (foundPtr)
		*exists = TRUE;
	else
	{
		*exists = FALSE;
		entry->database = values->database;
		entry->tablespace = values->tablespace;
		elog(LOG, "Added %d database %d tablespace to Hash", entry->database, entry->tablespace);
	}

	return entry;
}

void
Persistent_PostDTMRecv_RemoveHashEntry(Oid dbId)
{
	bool		foundPtr;

	Insist(PT_PostDTMRecv_Info);
	Insist(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash != NULL);

	(void) hash_search(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash,
					   (void *) &dbId,
					   HASH_REMOVE,
					   &foundPtr);
}

postDTMRecv_dbTblSpc_Hash_Entry *
Persistent_PostDTMRecv_LookupHashEntry(Oid dbId, bool *exists)
{
	bool		foundPtr;
	postDTMRecv_dbTblSpc_Hash_Entry *entry;

	Insist(PT_PostDTMRecv_Info);
	Insist(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash);

	*exists = true;
	entry = (postDTMRecv_dbTblSpc_Hash_Entry *) hash_search(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash,
															(void *) &dbId,
															HASH_FIND,
															&foundPtr);
	if (!foundPtr)
	{
		*exists = false;
		return NULL;
	}

	return entry;
}

bool
Persistent_PostDTMRecv_IsHashFull(void)
{
	Insist(PT_PostDTMRecv_Info);
	Insist(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash);

	return (hash_get_num_entries(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash) == PT_MAX_NUM_POSTDTMRECV_DB);
}

void
Persistent_Set_PostDTMRecv_PTCatVerificationNeeded()
{
	Insist(PT_PostDTMRecv_Info);
	PT_PostDTMRecv_Info->postDTMRecv_PTCatVerificationNeeded = true;
}

bool
Persistent_PostDTMRecv_PTCatVerificationNeeded()
{
	return PT_PostDTMRecv_Info->postDTMRecv_PTCatVerificationNeeded;
}

void
Persistent_PrintHash()
{
	Insist(PT_PostDTMRecv_Info);
	Insist(PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash != NULL);

	postDTMRecv_dbTblSpc_Hash_Entry *entry = NULL;
	HASH_SEQ_STATUS status;

	hash_seq_init(&status, PT_PostDTMRecv_Info->postDTMRecv_dbTblSpc_Hash);

	while ((entry = (postDTMRecv_dbTblSpc_Hash_Entry *) hash_seq_search(&status)) != NULL)
	{
		elog(LOG, "Database : %d, Tablespace : %d", entry->database, entry->tablespace);
	}
}

/*
 * *******Post DTM Recovery Verification - Hash helper functions End ***
 */

/*
 * Pre-work for executing the query using SPI
 */
void
Persistent_Pre_ExecuteQuery()
{
	Assert(connected == false);
	Assert(!savedResourceOwner && !oldMemoryContext);

	savedResourceOwner = CurrentResourceOwner;
	oldMemoryContext = CurrentMemoryContext;
	PushActiveSnapshot(SnapshotNow);

	if (SPI_OK_CONNECT != SPI_connect())
	{
		ereport(ERROR, (errcode(ERRCODE_CDB_INTERNAL_ERROR),
						errmsg("SPI Unsuccessfull : "),
						errdetail("SPI_connect failed in Persistent_Pre_ExecuteQuery")));
	}
	connected = true;
}

/*
 * Performs Actual Query execution
 */
int
Persistent_ExecuteQuery(char const *query, bool readOnlyQuery)
{
	StringInfoData sqlstmt;
	int			ret;
	int			proc = 0;	/* 32 bit, only holds metadata */

	Assert(query);
	Insist(connected);

	/* Initializations */
	sqlstmt.data = NULL;

	/* Assemble our query string */
	initStringInfo(&sqlstmt);
	appendStringInfo(&sqlstmt, "%s", query);

	PG_TRY();
	{
		/* XXX: Need to set the snapshot here. Reason - Unknown */
		PushActiveSnapshot(SnapshotNow);

		/* Run the query. */
		ret = SPI_execute(sqlstmt.data, readOnlyQuery, 0);
		proc = (int) SPI_processed;

		if (ret > 0 && SPI_tuptable != NULL)
		{
			TupleDesc	tupdesc = SPI_tuptable->tupdesc;
			SPITupleTable *tuptable = SPI_tuptable;
			int			i,
						j;
			char		localbuf[8192];

			for (j = 0; j < proc; j++)
			{
				HeapTuple	tuple = tuptable->vals[j];

				for (i = 1, localbuf[0] = '\0'; i <= tupdesc->natts; i++)
				{
					snprintf(localbuf + strlen(localbuf), sizeof(localbuf) - strlen(localbuf), " %s%s",
							 SPI_getvalue(tuple, tupdesc, i),
							 (i == tupdesc->natts) ? " " : " |");
				}
				elog(LOG, "==>: %s", localbuf);
			}
		}
	}
	/* Clean up in case of error. */
	PG_CATCH();
	{
		pfree(sqlstmt.data);
		PG_RE_THROW();
	}

	PG_END_TRY();
	pfree(sqlstmt.data);
	return proc;
}

/*
 * Post successful query execution cleanup work
 */
void
Persistent_Post_ExecuteQuery(void)
{
	Assert(oldMemoryContext && savedResourceOwner);
	Insist(connected);

	SPI_finish();
	connected = false;

	MemoryContextSwitchTo(oldMemoryContext);
	CurrentResourceOwner = savedResourceOwner;
	oldMemoryContext = NULL;
	savedResourceOwner = NULL;
}

/*
 * Cleanup work if something breaks while executing the query
 */
void
Persistent_ExecuteQuery_Cleanup(void)
{
	Assert(oldMemoryContext && savedResourceOwner);

	if (connected)
	{
		SPI_finish();
		connected = false;
	}
	AbortCurrentTransaction();

	MemoryContextSwitchTo(oldMemoryContext);
	CurrentResourceOwner = savedResourceOwner;
	oldMemoryContext = NULL;
	savedResourceOwner = NULL;
}

/*
 * Performs non database specific PersistentTables-Catalog verification
 * Return true if all the verifications pass else returns false
 */
bool
Persistent_NonDBSpecificPTCatVerification(void)
{
	int			querynum = 0;
	bool		testSucceeded = true;

	/*
	 * DataState is needed because some cross cons. queries should be run only
	 * in InSync mode
	 */
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	bool		isDataInSync = (dataState == DataStateInSync);

	Persistent_Pre_ExecuteQuery();
	PG_TRY();
	{
		for (querynum = 0; querynum < NONDB_SPECIFIC_PTCAT_VERIFICATION_NUM_QUERIES; querynum++)
		{
			if (!isDataInSync && NonDBSpecific_PTCat_Verification_queries[querynum].executeWhenInSyncOnly)
				continue;

			elog(LOG, "%s", NonDBSpecific_PTCat_Verification_queries[querynum].quertTitle);
			if (Persistent_ExecuteQuery(NonDBSpecific_PTCat_Verification_queries[querynum].queryStr, true) > 0)
				testSucceeded = false;
		}
	}
	PG_CATCH();
	{
		Persistent_ExecuteQuery_Cleanup();
		elog(FATAL, "PersistentTable-Cat NonDBSpecific Verification: Failure");
	}
	PG_END_TRY();
	Persistent_Post_ExecuteQuery();

	return testSucceeded;
}

/*
 * Performs database specific PersistentTables-Catalog verification
 * Return true if all the verifications pass else returns false
 */
bool
Persistent_DBSpecificPTCatVerification(void)
{
	int			querynum = 0;
	bool		testSucceeded = true;

	/*
	 * DataState is needed because some cross cons. queries should be run only
	 * in InSync mode
	 */
	getFileRepRoleAndState(&fileRepRole, &segmentState, &dataState, NULL, NULL);
	bool		isDataInSync = (dataState == DataStateInSync);

	Persistent_Pre_ExecuteQuery();
	PG_TRY();
	{
		for (querynum = 0; querynum < DB_SPECIFIC_PTCAT_VERIFICATION_NUM_QUERIES; querynum++)
		{
			if (!isDataInSync && DB_PTCat_Veritifcation_queries[querynum].executeWhenInSyncOnly)
				continue;

			elog(LOG, "%s", DB_PTCat_Veritifcation_queries[querynum].quertTitle);
			if (Persistent_ExecuteQuery(DB_PTCat_Veritifcation_queries[querynum].queryStr, true) > 0)
				testSucceeded = false;
		}
	}
	PG_CATCH();
	{
		Persistent_ExecuteQuery_Cleanup();
		elog(FATAL, "PersistentTable-Cat DBSpecific Verification: Failure");
	}
	PG_END_TRY();
	Persistent_Post_ExecuteQuery();

	return testSucceeded;
}

/*
 * Performs a dispatch to all the non-Master segments to perform
 * non-DB specific PersistentCat verifications
 */
void
Persistent_PostDTMRecv_NonDBSpecificPTCatVerification(void)
{
	const char *cmdbuf = "select * from gp_nondbspecific_ptcat_verification()";

	/* call to all QE to perform verifications */
	CdbDispatchCommand(cmdbuf, DF_NONE, NULL);
}

/*
 * Performs a dispatch to all the non-Master segments to perform
 * DB specific PersistentCat verifications
 */
void
Persistent_PostDTMRecv_DBSpecificPTCatVerification(void)
{
	const char *cmdbuf = "select * from gp_dbspecific_ptcat_verification()";

	/* call to all QE to perform verifications */
	CdbDispatchCommand(cmdbuf, DF_NONE, NULL);
}

Datum
gp_dbspecific_ptcat_verification(PG_FUNCTION_ARGS)
{
	elog(LOG, "DB specific PersistentTable-Catalog Verification using DB %d", MyDatabaseId);
	if (!Persistent_DBSpecificPTCatVerification())
		elog(ERROR, "DB specific PersistentTable-Catalog verifications failed.");

	PG_RETURN_BOOL(true);
}

Datum
gp_nondbspecific_ptcat_verification(PG_FUNCTION_ARGS)
{
	elog(LOG, "Non-DB specific PersistentTable-Catalog Verification using DB %d", MyDatabaseId);
	if (!Persistent_NonDBSpecificPTCatVerification())
		elog(ERROR, "Non-DB specific PersistentTable-Catalog verifications failed.");

	PG_RETURN_BOOL(true);
}
