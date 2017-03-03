/*-------------------------------------------------------------------------
 *
 * oid_dispatch.h
 *	  prototypes for functions in backend/catalog/oid_dispatch.c
 *
 *
 * Portions Copyright 2016 Pivotal Software, Inc.
 *
 * src/include/catalog/oid_dispatch.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef OID_DISPATCH_H
#define OID_DISPATCH_H

#include "utils/relcache.h"

/* Functions used in master */
extern void AddDispatchOidFromTuple(Relation catalogrel, HeapTuple tuple);
extern List *GetAssignedOidsForDispatch(void);

/* Functions used in QE nodes */
extern void AddPreassignedOids(List *l);
extern void AddPreassignedOidFromBinaryUpgrade(Oid oid, Oid catalog,
			char *objname, Oid namespaceOid, Oid keyOid1, Oid keyOid2);
extern Oid GetPreassignedOidForTuple(Relation catalogrel, HeapTuple tuple);
extern Oid GetPreassignedOidForRelation(Oid namespaceOid, const char *relname);
extern Oid GetPreassignedOidForType(Oid namespaceOid, const char *typname);
extern Oid GetPreassignedOidForDatabase(const char *datname);

extern void AtEOXact_DispatchOids(bool isCommit);

#endif   /* OID_DISPATCH_H */
