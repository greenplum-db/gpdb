/*-------------------------------------------------------------------------
 *
 * mdlite_global_generation.c
 *	 Implementation of Global Cache Generation of MDLite
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/mdlite.h"

/* Name to identify the MD Lite Global Cache Generation (GG) shared memory area*/
#define MDLITE_GLOBAL_GEN_SHMEM_NAME "MDLite Global Cache Generation"

/* Pointer to the shared memory global cache generation (GG) */
uint64 *mdlite_global_generation = NULL;

/*
 * Initalize the shared memory data structure needed for MD Lite
 */
void mdlite_shmem_init(void) {

    bool attach = false;

    /*Allocate or attach to shared memory area */
    void *shmem_base = ShmemInitStruct(MDLITE_GLOBAL_GEN_SHMEM_NAME,
            sizeof (*mdlite_global_generation),
            &attach);
    mdlite_global_generation = (uint64 *) shmem_base;
    
#ifdef MD_LITE_INSTRUMENTAION
    elog(gp_mdlite_loglevel,
            "MDLite: Creating global cache generation");
#endif

    Assert(0 == *mdlite_global_generation);
}

/*
 * Compute the size of shared memory required for the MD Lite component
 */
Size mdlite_shmem_size(void)
{
    return sizeof(*mdlite_global_generation);
}