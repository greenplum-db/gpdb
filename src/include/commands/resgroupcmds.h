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

#define RESGROUP_MAX_MEMORY_LIMIT	(100)

extern void CreateResourceGroup(CreateResourceGroupStmt *stmt);
extern void DropResourceGroup(DropResourceGroupStmt *stmt);
extern void AlterResourceGroup(AlterResourceGroupStmt *stmt);

/* catalog access function */
extern Oid GetResGroupIdForName(char *name, LOCKMODE lockmode);
extern char *GetResGroupNameForId(Oid oid, LOCKMODE lockmode);
extern Oid GetResGroupIdForRole(Oid roleid);
extern void GetResGroupCapabilities(Oid groupId, ResGroupCaps *resgroupCaps);
extern void HandleResGroupDDLCallbacks(bool isCommit);

#endif   /* RESGROUPCMDS_H */
