/*-------------------------------------------------------------------------
 *
 * mdlite_utils.c
 *	 Utility functions for MDLite
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "utils/mdlite.h"
#include "cdb/cdbvars.h"
#include "catalog/gp_verification_history.h"

/*
 * Returns true if Metadata Lite for MD Cache is enabled in the current context
 */
bool mdlite_enabled()
{
    /*
    * We only initialized Metadata Lite on the master,
    * and only for QD or utility mode process.
    * MD Lite can also be disabled by the guc gp_metadata_lite.
    */
    
    /* TODO gcaragea 05/06/2014: Do we need to disable MD Versioning during (auto)vacuum? (MPP-23504) */
    
    return gp_metadata_lite &&
            GpIdentity.segindex == MASTER_CONTENT_ID &&
	   ((GP_ROLE_DISPATCH == Gp_role) || (GP_ROLE_UTILITY == Gp_role));
    return true;
}