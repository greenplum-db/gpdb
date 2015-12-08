/* -------------------------------------------------------------------------
 * 
 * mdver.h
 *	  Interface for metadata versioning
 *
 * Copyright (c) 2014, Pivotal, Inc.
 * 
 * 
 * -------------------------------------------------------------------------
 */

#ifndef MDVER_H
#define	MDVER_H

#include "postgres.h"
#include "utils/relcache.h"

typedef struct mdver_local
{
	/*
	 * An integer stored inside the Metadata Cache component of a Backend
	 * process. This holds the last global generation observed by the backend. This
	 * is updated only when a command starts.
	 */
    uint64 local_generation;

    /*
     * Flag is set to true for a transaction that changed metadata. At the
     * end of the transaction, this will trigger an update to the global
     * generation counter (GG).
     */
    bool transaction_dirty;

} mdver_local;

/* Pointer to the shared memory global cache generation (GG) */
extern uint64 *mdver_global_generation;

/* Set to true if mdcache is marked as "dirty" and needs purging */
extern bool mdver_dirty_mdcache;

/* Metadata Versioning global generation functionality */
void mdver_shmem_init(void);
Size mdver_shmem_size(void);
uint64 mdver_get_global_generation(void);
void mdver_bump_global_generation(mdver_local* local_mdver);

/* Metadata Versioning local generation functionality */
mdver_local* mdver_create_local(void);
void mdver_mark_dirty_xact(mdver_local* local_mdver);
bool mdver_command_begin(void);

/* Metadata Versioning Invalidation Translator operations */
void mdver_inv_translator(Relation relation);

/* Metadata Versioning utility functions */
bool mdver_enabled();

/* inval.c */
extern mdver_local *GetCurrentLocalMDVer(void);


#endif	/* MDVER_H */

