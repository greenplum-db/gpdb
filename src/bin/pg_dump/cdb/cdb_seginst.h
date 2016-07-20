/*-------------------------------------------------------------------------
 *
 * cdb_seginst.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDB_SEGINST_H
#define CDB_SEGINST_H

#include "libpq-fe.h"

typedef enum segment_role
{
	ROLE_SEGDB = 0,
	ROLE_MASTER
} SegRole;

/* --------------------------------------------------------------------------------------------------
 * Structure for segment database information
 *
 * The information contained in this structure represents a row from a join of rows from the tables
 * pg_database, mpp.listener, and mpp.host.
 *
 */
typedef struct segment_database
{
	int4		dbid;
	int4		content;
	SegRole		role;			/* is it a master database or just any segdb? */
	int4		port;
	char	   *pszHost;
	char	   *pszDBName;
	char	   *pszDBUser;
	char	   *pszDBPswd;
} SegmentDatabase;

/*----------------------------------------------------------------------------------------------------
 * This structure is helpful when gp_dump used --gp-s=p and therefore generated backup
 * files only on the primary nodes. In that case --gp-l=p should be used in gp_restore.
 * This structure will hold the segdb information of the segdb that has the backup
 * file (segdb_source) and the mirror or primary segdb we want to dump it to (segdb_target).
 * when the target segdb is a primary, it will be the same as the source segdb. When --gp-l
 * is not used, the source and target in each pair will always be the same.
 */
typedef struct restore_pair
{
	SegmentDatabase segdb_source;	/* segdb where backup file is located at */
	SegmentDatabase segdb_target;	/* segdb we want to restore. same as source or its mirror */

}	RestorePair;

/* --------------------------------------------------------------------------------------------------
 * Structure for an array of segment database pairs
 */
typedef struct segment_database_pair_array
{
	int			count;
	SegmentDatabase *pData;
} SegmentDatabaseArray;

typedef struct segment_database_restore_pair_array
{
	int			count;
	RestorePair *pData;
}	RestorePairArray;


/* --------------------------------------------------------------------------------------------------
 * Enum representing the actors of a backup or restore operation.
 */
typedef enum actor_set
{
	SET_NO_MIRRORS,
	SET_INDIVIDUAL
}	ActorSet;

typedef enum backup_file_location
{
	FILE_ON_PRIMARIES,
	FILE_ON_INDIVIDUAL
}	BackupLoc;



extern void FreeSegmentDatabaseArray(SegmentDatabaseArray *pSegDBAr);
extern void FreeRestorePairArray(RestorePairArray * restorePairAr);

/* Read all the rows in the join */
extern bool GetDumpSegmentDatabaseArray(PGconn *pConn, int server_version, SegmentDatabaseArray *pSegDBAr, ActorSet actors, char *raw_dump_set, char *pszDBName, char *pszUserName, bool dataOnly, bool schemaOnly);
extern bool GetRestoreSegmentDatabaseArray(PGconn *pConn, RestorePairArray * pSegDBPairAr, BackupLoc backupLocation, char *raw_restore_set, bool dataOnly);

#endif   /* CDB_SEGINST_H */
