/*-------------------------------------------------------------------------
 *
 * mdver_utils.c
 *	 Utility functions for Metadata Versioning
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "utils/mdver.h"
#include "cdb/cdbvars.h"
#include "catalog/gp_verification_history.h"

/*
 * mdver_enabled
 * 		Returns true if Metadata Versiong for MD Cache is enabled in the current context
 */
bool
mdver_enabled()
{
    /*
    * We only initialized Metadata Version on the master,
    * and only for QD or utility mode process.
    * MD Version can also be disabled by the guc
    * optimizer_release_mdcache to true
    */
    
    return !optimizer_release_mdcache &&
            GpIdentity.segindex == MASTER_CONTENT_ID &&
	   ((GP_ROLE_DISPATCH == Gp_role) || (GP_ROLE_UTILITY == Gp_role));
}
