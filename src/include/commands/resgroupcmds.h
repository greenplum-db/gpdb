/*-------------------------------------------------------------------------
 *
 * resgroupcmds.h
 *	  Commands for manipulating resource group.
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 * 		src/include/commands/resgroupcmds.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESGROUPCMDS_H
#define RESGROUPCMDS_H

#include "nodes/parsenodes.h"

extern void CreateResourceGroup(CreateResourceGroupStmt *stmt);
extern void DropResourceGroup(DropResourceGroupStmt *stmt);
extern void AlterResourceGroup(AlterResourceGroupStmt *stmt);

/* catalog access function */
extern Oid GetResGroupIdForName(char *name, LOCKMODE lockmode);
extern char *GetResGroupNameForId(Oid oid, LOCKMODE lockmode);
extern void GetConcurrencyForResGroup(int groupId, int *value, int *proposed);
extern float GetCpuRateLimitForResGroup(int groupId);
extern Oid GetResGroupIdForRole(Oid roleid);
extern void GetMemoryCapabilitiesForResGroup(int groupId, float *memoryLimit,
											 float *sharedQuota, float *spillRatio);
extern void AtEOXact_ResGroup(bool isCommit);

#endif   /* RESGROUPCMDS_H */
