/*-------------------------------------------------------------------------
 *
 * relcache.h
 *	  Relation descriptor cache definitions.
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/relcache.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RELCACHE_H
#define RELCACHE_H

#include "access/tupdesc.h"
#include "nodes/bitmapset.h"


/*
 * Name of relcache init file(s), used to speed up backend startup
 */
#define RELCACHE_INIT_FILENAME	"pg_internal.init"

typedef struct RelationData *Relation;

/* ----------------
 *		RelationPtr is used in the executor to support index scans
 *		where we have to keep track of several index relations in an
 *		array.  -cim 9/10/89
 * ----------------
 */
typedef Relation *RelationPtr;

/*
 * Routines to open (lookup) and close a relcache entry
 */
extern Relation RelationIdGetRelation(Oid relationId);
extern void RelationClose(Relation relation);

/*
 * Routines to compute/retrieve additional cached information
 */
struct GpPolicy *RelationGetPartitioningKey(Relation relation);
extern List *RelationGetFKeyList(Relation relation);
extern List *RelationGetIndexList(Relation relation);
extern List *RelationGetStatExtList(Relation relation);
extern Oid	RelationGetPrimaryKeyIndex(Relation relation);
extern Oid	RelationGetReplicaIndex(Relation relation);
extern List *RelationGetIndexExpressions(Relation relation);
extern List *RelationGetDummyIndexExpressions(Relation relation);
extern List *RelationGetIndexPredicate(Relation relation);

typedef enum IndexAttrBitmapKind
{
	INDEX_ATTR_BITMAP_ALL,
	INDEX_ATTR_BITMAP_KEY,
	INDEX_ATTR_BITMAP_PRIMARY_KEY,
	INDEX_ATTR_BITMAP_IDENTITY_KEY
} IndexAttrBitmapKind;

extern Bitmapset *RelationGetIndexAttrBitmap(Relation relation,
											 IndexAttrBitmapKind keyAttrs);

extern void RelationGetExclusionInfo(Relation indexRelation,
									 Oid **operators,
									 Oid **procs,
									 uint16 **strategies);

extern void RelationInitIndexAccessInfo(Relation relation);

/* caller must include pg_publication.h */
struct PublicationActions;
extern struct PublicationActions *GetRelationPublicationActions(Relation relation);

extern void RelationInitTableAccessMethod(Relation relation);

/*
 * Routines to support ereport() reports of relation-related errors
 */
extern int	errtable(Relation rel);
extern int	errtablecol(Relation rel, int attnum);
extern int	errtablecolname(Relation rel, const char *colname);
extern int	errtableconstraint(Relation rel, const char *conname);

/*
 * Routines for backend startup
 */
extern void RelationCacheInitialize(void);
extern void RelationCacheInitializePhase2(void);
extern void RelationCacheInitializePhase3(void);

/*
 * Routine to create a relcache entry for an about-to-be-created relation
 */
extern Relation RelationBuildLocalRelation(const char *relname,
										   Oid relnamespace,
										   TupleDesc tupDesc,
										   Oid relid,
										   Oid accessmtd,
										   Oid relfilenode,
										   Oid reltablespace,
										   bool shared_relation,
										   bool mapped_relation,
										   char relpersistence,
										   char relkind);

/*
 * Routine to manage assignment of new relfilenode to a relation
 */
extern void RelationSetNewRelfilenode(Relation relation, char persistence);

/*
 * Routines for flushing/rebuilding relcache entries in various scenarios
 */
extern void RelationForgetRelation(Oid rid);

extern void RelationCacheInvalidateEntry(Oid relationId);

extern void RelationCacheInvalidate(bool debug_discard);

extern void RelationCloseSmgrByOid(Oid relationId);

extern void AtEOXact_RelationCache(bool isCommit);
extern void AtEOSubXact_RelationCache(bool isCommit, SubTransactionId mySubid,
									  SubTransactionId parentSubid);

/*
 * Routines to help manage rebuilding of relcache init files
 */
extern bool RelationIdIsInInitFile(Oid relationId);
extern void RelationCacheInitFilePreInvalidate(void);
extern void RelationCacheInitFilePostInvalidate(void);
extern void RelationCacheInitFileRemove(void);

/* should be used only by relcache.c and catcache.c */
extern bool criticalRelcachesBuilt;

/* should be used only by relcache.c and postinit.c */
extern bool criticalSharedRelcachesBuilt;

#endif							/* RELCACHE_H */
