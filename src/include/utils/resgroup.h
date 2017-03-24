/*-------------------------------------------------------------------------
 *
 * resgroup.h
 *	  GPDB resource group definitions.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef RES_GROUP_H
#define RES_GROUP_H

/*
 * GUC variables.
 */


/*
 * Data structures
 */


/*
 * Functions in resgroup.c
 */
extern Oid GetResGroupIdForName(char *name, LOCKMODE lockmode);

#endif   /* RES_GROUP_H */
