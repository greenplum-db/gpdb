/*-------------------------------------------------------------------------
 *
 * cdbdatabaseinfo.h
 *
 *   Functions to collect information about the existing files within a
 *   database.
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdatabaseinfo.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDATABASEINFO_H
#define CDBDATABASEINFO_H

#include "catalog/pg_appendonly.h"
#include "storage/itemptr.h"
#include "storage/dbdirnode.h"
#include "utils/tqual.h"

/* ------------------------------------------------------------------------
 * Structure definitions
 * ------------------------------------------------------------------------ */

/* 
 * struct MiscEntry - 
 *   A file in the database directory that is not a strictly a relation.  
 *   Examples include:
 *     - PG_VERSION
 *     - pg_fsm.cache
 *     - etc.
 */
typedef struct MiscEntry
{
	Oid			 tablespace;
	bool		 isDir;
	char		*name;
} MiscEntry;


/*
 * struct DbInfoGpRelationNode -
 *   Describes an entry as it will apear in gp_relation_node.
 */
typedef struct DbInfoGpRelationNode
{
	ItemPointerData		gpRelationNodeTid;
	Oid					relfilenodeOid;
	int32				segmentFileNum;
	ItemPointerData		persistentTid;
	int64				persistentSerialNum;
	int64				logicalEof;
} DbInfoGpRelationNode;


/*
 * struct DbInfoSegmentFile -
 *   Describes a segment file as it exists in the filesystem.  
 *
 *   A given relfilenode N may consist of multiple files on disk, "N", "N.1",
 *   etc.  Each one of these files is a segment file of the specified
 *   relfilenode.
 */
typedef struct DbInfoSegmentFile
{
	int32		segmentFileNum;
	int64		eof;
} DbInfoSegmentFile;


/*
 * struct DbInfoAppendOnlyCatalogSegmentInfo -
 *   Describes a segment for an ao table, as it is represented in the catalog.
 *
 *   This is like DbInfoSegmentFile, except that it describes the Logical 
 *   structure of the file as the catalog sees it.
 */
typedef struct DbInfoAppendOnlyCatalogSegmentInfo
{
	int32		segmentFileNum;
	int64		logicalEof;
} DbInfoAppendOnlyCatalogSegmentInfo;

/*
 * struct DbInfoRelKeyPair -
 *   Describes a hash table key leading to a DbInfoRel element.
 *
 *   The DbInfoRel struct holds this hash table key.  The key is used
 *   to do a lookup in the HTAB dbInfoRelHashTable initialised in
 *   DatabaseInfo_Collect.  A constructed DbInfoRelKeyPair can be used
 *   to create a hash value from the HTAB->hash function.  This hash
 *   value will be converted into a bucket number which will lead to
 *   the HASHBUCKET that should contain the DbInfoRel element.
 *
 *   We use reltablespace and relfilenode in the key because there can
 *   be the same relfilenode in different tablespaces.  We cannot use
 *   relation OID because gp_relation_node does not keep that
 *   information.
 */
typedef struct DbInfoRelKeyPair
{
	Oid	reltablespace;
	Oid	relfilenode;
} DbInfoRelKeyPair;

/*
 * struct DbInfoRel - 
 *   Describes relations that exist within the database.  For each relation we
 *   track data about both the table within the catalog and the table within
 *   the filesystem.
 *
 *   Theoretically the segment files should match between gp_relation_node, 
 *   the pg_aoseg (for ao/co tables), and the filesystem with the following
 *   caveats:
 *     - We don't gather all of these lists under all circumstances
 *     - pg_aoseg may not contain the segment_file_num=0 entry
 *     - There are cases where we may leak certain physical files 
 *       (generally crash scenarios)
 */
typedef struct DbInfoRel
{
	/* Key */
	DbInfoRelKeyPair					 dbInfoRelKey;

	/* Catalog */
	bool								 inPgClass;    /* Can we find it in pg_class ? */
	ItemPointerData						 pgClassTid;   /* At what row id? */

	/* A couple key fields from pg_class */
	Oid									 relationOid;
	char								*relname;
	char								 relkind;
	char								 relstorage;
	Oid									 relam;
	int									 relnatts;

	/* segment files - as described in gp_relation_node */
	DbInfoGpRelationNode				*gpRelationNodes;
	int32								 gpRelationNodesCount;
	int32								 gpRelationNodesMaxCount;

	/* segment files - as described in pg_aoseg */
	DbInfoAppendOnlyCatalogSegmentInfo 	*appendOnlyCatalogSegmentInfo;
	int32								 appendOnlyCatalogSegmentInfoCount;
	int32								 appendOnlyCatalogSegmentInfoMaxCount;

	/* segment files - as found in the file system */
	DbInfoSegmentFile					*physicalSegmentFiles;
	int32								 physicalSegmentFilesCount;
	int32								 physicalSegmentFilesMaxCount;

} DbInfoRel;

/*
 * struct DbInfoExtraSegmentFile -
 *   Describes files found in the filesystem that have no matching entries
 *   within the catalog.
 */
typedef struct DbInfoExtraSegmentFile
{
	Oid			relfilenode;
	int32		segmentFileNum;
	Oid			tablespaceOid;
	int64		eof;
} DbInfoExtraSegmentFile;

/*
 * struct PgAppendOnlyHashEntry -
 *    used to build a cache of pg_appendonly entries
 */
typedef struct PgAppendOnlyHashEntry
{
	/* Key */
	Oid					 relationId;

	/* Pointer */
	Form_pg_appendonly	aoEntry;
} PgAppendOnlyHashEntry;


/*
 * struct DatabaseInfo -
 *   Represents all the contents of a specified database both as it exists in
 *   the catalog and also how it exists within the filesystem.
 */
typedef struct DatabaseInfo
{
	/* The database and its default tablespace */
	Oid							 database;
	Oid							 defaultTablespace;

	/* A list of all the tablespaces that this database makes use of */
	Oid							*tablespaces;
	int32						 tablespacesCount;
	int32						 tablespacesMaxCount;

	/* A list of all the relations in the database */
	DbInfoRel					*dbInfoRelArray;
	int							 dbInfoRelArrayCount;

	/* A list of all non-relation files in the database */
	MiscEntry					*miscEntries;
	int32						 miscEntriesCount;
	int32						 miscEntriesMaxCount;

	/* A list of all relfiles without catalog entries (leaked) */
	DbInfoExtraSegmentFile		*extraSegmentFiles;
	int32						 extraSegmentFilesCount;
	int32						 extraSegmentFilesMaxCount;

	/* ??? */
	DbInfoGpRelationNode		*parentlessGpRelationNodes;
	int32						 parentlessGpRelationNodesCount;
	int32						 parentlessGpRelationNodesMaxCount;

	/* 
	 * flags to indicate if we collected gp_relation_node or pg_aoseg
	 * information in the dbInfoRelArray.
	 */
	bool                         collectGpRelationNodeInfo;
	bool						 collectAppendOnlyCatalogSegmentInfo;
} DatabaseInfo;


/* ------------------------------------------------------------------------
 * Exported functions
 * ------------------------------------------------------------------------ */

/* 
 * DatabaseInfo_Collect()
 *   Scans both the catalog and the filesystem to construct a DatabaseInfo
 *   record describing the contents of the database 
 */
extern DatabaseInfo *DatabaseInfo_Collect(
	Oid 		database,
	Oid 		defaultTablespace,
	Snapshot	snapshot,
	bool        collectGpRelationNodeInfo,
	bool		collectAppendOnlyCatalogSegmentInfo,
	bool		scanFileSystem);

/*
 * DatabaseInfo_AlignAppendOnly()
 *   ???
 */
extern void DatabaseInfo_AlignAppendOnly(DatabaseInfo *info, 
										 DbInfoRel *dbInfoRel);


#ifdef suppress

/* debugging functions */
extern void DatabaseInfo_Check(DatabaseInfo *info);

extern void DatabaseInfo_Trace(DatabaseInfo *info);

#endif

#endif   /* CDBDATABASEINFO_H */
