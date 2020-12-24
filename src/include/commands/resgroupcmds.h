/*-------------------------------------------------------------------------
 *
 * resgroupcmds.h
 *	  Commands for manipulating resource group.
 *
 * Portions Copyright (c) 2006-2017, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * IDENTIFICATION
 * 		src/include/commands/resgroupcmds.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESGROUPCMDS_H
#define RESGROUPCMDS_H

#include "nodes/parsenodes.h"
#include "storage/lock.h"
#include "utils/resgroup.h"
#include "utils/relcache.h"

typedef enum ResGroupMemAuditorType
{
	RESGROUP_MEMORY_AUDITOR_VMTRACKER = 0,
	RESGROUP_MEMORY_AUDITOR_CGROUP,

	RESGROUP_MEMORY_AUDITOR_COUNT,
} ResGroupMemAuditorType;

extern void CreateResourceGroup(CreateResourceGroupStmt *stmt);
extern void DropResourceGroup(DropResourceGroupStmt *stmt);
extern void AlterResourceGroup(AlterResourceGroupStmt *stmt);

/* catalog access function */
extern Oid get_resgroup_oid(const char *name, bool missing_ok);
extern char *GetResGroupNameForId(Oid oid);
extern Oid GetResGroupIdForRole(Oid roleid);
extern void GetResGroupCapabilities(Relation rel,
									Oid groupId,
									ResGroupCaps *resgroupCaps);
extern void ResGroupCheckForRole(Oid groupId);

#endif   /* RESGROUPCMDS_H */
