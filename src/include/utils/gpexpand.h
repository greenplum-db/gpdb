/*-------------------------------------------------------------------------
 *
 * gpexpand.h
 *	  Helper functions for gpexpand.
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 * src/include/utils/gpexpand.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GPEXPAND_H
#define GPEXPAND_H

extern int GpExpandVersionShmemSize(void);
extern void GpExpandVersionShmemInit(void);
extern int GetGpExpandVersion(void);

extern Datum gp_expand_lock_catalog(PG_FUNCTION_ARGS);

extern void gp_expand_protect_catalog_changes(Relation relation);

extern Datum gp_expand_bump_version(PG_FUNCTION_ARGS);


#endif   /* GPEXPAND_H */

