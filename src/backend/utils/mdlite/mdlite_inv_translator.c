/*-------------------------------------------------------------------------
 *
 * mdver_inv_translator.c
 *	 Implementation of Invalidation Translator (IT) for metadata lite
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "utils/mdlite.h"

/*
 * Update the bump command id when a tuple is being updated in catalog
 * relation : The catalog table being touched
 * tuple    : The affected tuple
 */
void mdlite_inv_translator(Relation relation)
{
    if (!mdlite_enabled())
    {
        return;
    }
    
    if (IsAoSegmentRelation(relation))
    {
            /*
             * We don't track for catalog tables changes in the AOSEG
             * namespace. These are modified for DML only (not DDL)
             *
             * TODO gcaragea 01/14/2015: Remove this once we have DML versioning
             */
            return;
    }
    
#ifdef MD_LITE_INSTRUMENTATION
    elog(gp_mdlite_loglevel, "MDLite : INV Translator setting current command id %u to bump", 
            GetCurrentCommandId());
#endif
    
    mdlite_local_bump_cmd_id();
}