/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  Commands for manipulating resource group.
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 * IDENTIFICATION
 * 		src/include/commands/resgroup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RESGROUP_H
#define RESGROUP_H

#include "nodes/parsenodes.h"

extern void CreateResourceGroup(CreateResourceGroupStmt *stmt);
extern void DropResourceGroup(DropResourceGroupStmt *stmt);

#endif   /* RESGROUP_H */
