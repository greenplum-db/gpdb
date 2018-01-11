/*-------------------------------------------------------------------------
 *
 * resgroupcmds.h
 *	  Commands for manipulating resource group.
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 * 		src/include/commands/resgroupcmds.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESGROUPCMDS_H
#define RESGROUPCMDS_H

#include "nodes/parsenodes.h"
#include "utils/resgroup.h"
#include "utils/relcache.h"

extern void CreateResourceGroup(CreateResourceGroupStmt *stmt);
extern void DropResourceGroup(DropResourceGroupStmt *stmt);
extern void AlterResourceGroup(AlterResourceGroupStmt *stmt);

/* catalog access function */
extern Oid GetResGroupIdForName(const char *name, LOCKMODE lockmode);
extern char *GetResGroupNameForId(Oid oid, LOCKMODE lockmode);
extern Oid GetResGroupIdForRole(Oid roleid);
extern void GetResGroupCapabilities(Relation rel,
									Oid groupId,
									ResGroupCaps *resgroupCaps);
#endif   /* RESGROUPCMDS_H */
