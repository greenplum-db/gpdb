/*-------------------------------------------------------------------------
 *
 * mdlite_local_generation.c
 *	 Implementation of Local Cache Generation for MDLite
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "utils/mdlite.h"


/*
 * Creates the new Local MDLite and initialize for the current transaction
 * Same mdlite_local will be used for subtransaction as well.
 */
mdlite_local* mdlite_create_local(void)
{
    mdlite_local* local_mdlite = palloc0(sizeof(mdlite_local));
    
    /* set to default */
    local_mdlite->local_generation = 0;
    local_mdlite->bump_cmd_id = DEFAULT_BUMP_CMD_ID;
    
#ifdef MD_LITE_INSTRUMENTATION
    elog(gp_mdlite_loglevel, "MDLite : Creating local cache generation. Gp_role=%d, Gp_identity=%d",
            Gp_role,
            GpIdentity.segindex);
#endif
    
    return local_mdlite;
}

/*
 * Set  bump command id to current command id.
 */
void mdlite_local_bump_cmd_id(void)
{
    mdlite_local *local_mdlite = GetCurrentLocalMDLite();
    
    Assert(NULL != local_mdlite);
    CommandId old_cmd_id = local_mdlite->bump_cmd_id;
    local_mdlite->bump_cmd_id = GetCurrentCommandId();

#ifdef MD_LITE_INSTRUMENTATION
    elog(gp_mdlite_loglevel, "MDLite: Bump Command id changed from %u to %u",
            old_cmd_id, local_mdlite->bump_cmd_id);
#endif
}
