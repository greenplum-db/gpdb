/*-------------------------------------------------------------------------
 *
 * catalog.h
 *	  prototypes for functions in backend/catalog/catalog.c
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/catalog.h,v 1.40 2008/06/19 00:46:06 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef CATALOG_H
#define CATALOG_H

#include "catalog/pg_class.h"
#include "storage/relfilenode.h"
#include "utils/relcache.h"

#include "catalog/oid_dispatch.h"


extern char *relpath(RelFileNode rnode);
extern char *GetDatabasePath(Oid dbNode, Oid spcNode);
extern void FormDatabasePath(char *databasePath, char *filespaceLocation, Oid tablespaceOid, Oid databaseOid);
extern void FormTablespacePath(char *tablespacePath, char *filespaceLocation, Oid tablespaceOid);
extern void FormRelationPath(char *relationPath, char *filespaceLocation, RelFileNode rnode);

extern bool IsSystemRelation(Relation relation);
extern bool IsToastRelation(Relation relation);

extern bool IsSystemClass(Form_pg_class reltuple);
extern bool IsToastClass(Form_pg_class reltuple);

extern bool IsSystemNamespace(Oid namespaceId);
extern bool IsToastNamespace(Oid namespaceId);
extern bool IsAoSegmentNamespace(Oid namespaceId);

extern bool IsReservedName(const char *name);
extern char* GetReservedPrefix(const char *name);

extern bool IsSharedRelation(Oid relationId);

extern Oid	GetNewOid(Relation relation);
extern Oid	GetNewOidWithIndex(Relation relation, Oid indexId,
							   AttrNumber oidcolumn);
extern Oid	GetNewSequenceRelationOid(Relation relation);
extern Oid GetNewRelFileNode(Oid reltablespace, bool relisshared);

#endif   /* CATALOG_H */
