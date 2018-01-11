/*
 * prototypes for master mirror synchronisation write ahead logging.
 *
 * Copyright Greenplum, 2010.
 */
#ifndef MMXLOG_H
#define MMXLOG_H

/* 
 * We define this to make it clear where temporary master mirror sync code
 * resides. This is to be replaced by master file rep.
 */
#define MASTER_MIRROR_SYNC

#include "access/persistentfilesysobjname.h"
#include "access/xlog.h"
#include "access/xlogdefs.h"
#include "lib/stringinfo.h"

#define MMXLOG_CREATE_DIR 0x00
#define MMXLOG_CREATE_FILE 0x10
#define MMXLOG_REMOVE_DIR 0x20
#define MMXLOG_REMOVE_HEAP_FILE 0x30
#define MMXLOG_REMOVE_APPENDONLY_FILE 0x40

typedef enum mm_fs_obj_type
{
	MM_OBJ_FILESPACE,
	MM_OBJ_TABLESPACE,
	MM_OBJ_DATABASE,
	MM_OBJ_RELFILENODE
} mm_fs_obj_type;

typedef struct xl_mm_fs_obj
{
	mm_fs_obj_type objtype; /* One of the MM_OBJ_* types */

	Oid filespace;
	Oid tablespace;
	Oid database;
	Oid relfilenode;
	uint32 segnum;

	/* union for alignment */
	union
	{
		struct
		{
			uint16 master;
			uint16 mirror;
		} dbid;
		int64 alignment;
	} u;

	char master_path[MAXPGPATH];
	char mirror_path[MAXPGPATH];
} xl_mm_fs_obj;

/*
 * XLOG format.
 */

typedef struct fspc_map
{
	Oid filespaceoid;
	int16 dbid1;
	int16 dbid2;
	char path1[MAXPGPATH];
	char path2[MAXPGPATH];
} fspc_map;

typedef struct tspc_map
{
	Oid tablespaceoid;
	Oid filespaceoid;
} tspc_map;

typedef struct dbdir_map
{
	Oid tablespaceoid;
	Oid databaseoid;
} dbdir_map;

/*
 * Container of filespaces/tablespaces used to accumulate mappings
 */
typedef struct fspc_agg_state
{
	int count;
	fspc_map maps[0]; /* variable length */
} fspc_agg_state;

#define FSPC_CHECKPOINT_BYTES(count) \
	(offsetof(fspc_agg_state, maps) + sizeof(fspc_map) * (count))

typedef struct tspc_agg_state
{
	int count;
	tspc_map maps[0]; /* variable length */
} tspc_agg_state;

#define TSPC_CHECKPOINT_BYTES(count) \
	(offsetof(tspc_agg_state, maps) + sizeof(tspc_map) * (count))

typedef struct dbdir_agg_state
{
	int count;
	dbdir_map maps[0]; /* variable length */
} dbdir_agg_state;

#define DBDIR_CHECKPOINT_BYTES(count) \
	(offsetof(dbdir_agg_state, maps) + sizeof(dbdir_map) * (count))

typedef struct MasterMirrorCheckpointInfo
{
	fspc_agg_state  *fspc;
	uint32          fspcMapLen;
	tspc_agg_state  *tspc;
	uint32          tspcMapLen;
	dbdir_agg_state *dbdir;
	uint32          dbdirMapLen;
} MasterMirrorCheckpointInfo;

extern void mmxlog_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record);
extern void mmxlog_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);

/* 
 * Functions to generate WAL records to remove file system objects on the
 * master / standby master.
 */
extern void mmxlog_log_remove_filespace(Oid filespace);
extern void mmxlog_log_remove_tablespace(Oid tablespace);
extern void mmxlog_log_remove_database(Oid tablespace, Oid database);
extern void mmxlog_log_remove_relfilenode( Oid tablespace, Oid database,
										  Oid relfilenode, uint32 segnum,
										  PersistentFileSysRelStorageMgr relStorageMgr);

/*
 * Functions to generate WAL records to add file system objects on the
 * master / standby master.
 */
extern void mmxlog_log_create_filespace(Oid filespace);
extern void mmxlog_log_create_tablespace(Oid filespace, Oid tablespace);
extern void mmxlog_log_create_database(Oid tablespace, Oid database);
extern void mmxlog_log_create_relfilenode(Oid tablespace, Oid database,
										  Oid relfilenode, uint32 segnum);
extern void mmxlog_append_checkpoint_data(XLogRecData rdata[5]);
extern void mmxlog_read_checkpoint_data(MasterMirrorCheckpointInfo mmckptInfo,
										XLogRecPtr *beginLoc);
extern bool mmxlog_filespace_get_path(
	Oid fspcoid,

	char **filespacePath);
extern bool mmxlog_tablespace_get_filespace(
	Oid tspcoid,

	Oid *filespaceOid);

extern void mmxlog_add_filespace_init(
	fspc_agg_state **fas, int *maxCount);
extern void mmxlog_add_filespace(
	fspc_agg_state **fas, int *maxCount,
	Oid filespace, 
	int16 dbid1, char *path1,
	int16 dbid2, char *path2, 
	char *caller);

extern void mmxlog_add_tablespace_init(
		tspc_agg_state **tas, int *maxCount);
extern void mmxlog_add_tablespace( 
		tspc_agg_state **tas, int *maxCount,
		Oid filespace, Oid tablespace, char *caller);

extern void mmxlog_add_database_init(
		dbdir_agg_state **das, int *maxCount);
extern void mmxlog_add_database( 
		dbdir_agg_state **das, int *maxCount,
		Oid database, Oid tablespace, char *caller);

extern uint32 mmxlog_get_checkpoint_record_fields(char *recordStart,
	MasterMirrorCheckpointInfo *mmckpt);

extern bool mmxlog_get_checkpoint_info(char *cpdata, int masterMirroringLen, int checkpointLen, XLogRecPtr *beginLoc, int errlevel,
		fspc_agg_state **f,
		tspc_agg_state **t,
	        dbdir_agg_state **d);
extern bool mmxlog_verify_checkpoint_info(char *cpdata, int masterMirroringLen, int checkpointLen, XLogRecPtr *beginLoc, int errlevel);
extern bool mmxlog_get_checkpoint_counts(char *cpdata, int masterMirroringLen, int checkpointLen, XLogRecPtr *beginLoc, int errlevel, int *filespaceCount, int *tablespaceCount, int *databaseCount);

extern void mmxlog_print_filespaces(int elevel, char *caller);
extern void mmxlog_print_tablespaces(int elevel, char *caller);

extern void mmxlog_empty_hashtables(void);

#endif /* MMXLOG_H */
