/* -------------------------------------------------------------------------
 * 
 * mdlite.h
 *	  Interface for metadata versioning
 *
 * Copyright (c) 2014, Pivotal, Inc.
 * 
 * 
 * -------------------------------------------------------------------------
 */

#ifndef MDLITE_H
#define	MDLITE_H

#include "postgres.h"
#include "utils/relcache.h"

#define DEFAULT_BUMP_CMD_ID 0

typedef struct mdlite_local
{
    uint64 local_generation;
    CommandId bump_cmd_id;
} mdlite_local;

/* Pointer to the shared memory global cache generation (GG)*/
extern uint64 *mdlite_global_generation;

/* MD Lite global cache generation init */
void mdlite_shmem_init(void);
Size mdlite_shmem_size(void);

/* MD Lite Local cache generation functions*/
mdlite_local* mdlite_create_local(void);
void mdlite_local_bump_cmd_id(void);

/*MD Lite Invalidation Translator operations*/
void mdlite_inv_translator(Relation relation);

/* MD Lite utility functions */
bool mdlite_enabled();

/* inval.c */
extern mdlite_local *GetCurrentLocalMDLite(void);


#endif	/* MDLITE_H */

