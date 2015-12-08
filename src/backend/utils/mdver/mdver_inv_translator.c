/*-------------------------------------------------------------------------
 *
 * mdver_inv_translator.c
 *	 Implementation of Invalidation Translator (IT) for metadata version
 *
 * Copyright (c) 2015, Pivotal, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "catalog/catalog.h"
#include "utils/guc.h"
#include "utils/mdver.h"

/*
 * Singleton static flag to mark the mdcache as dirty when a local
 * command updates the catalog
 */
bool mdver_dirty_mdcache = false;

static void mdver_mark_dirty_mdcache(void);

/*
 * mdver_inv_translator
 * 		This component intercepts all changes to catalog(metadata) done by a query.
 * 		When a relevant catalog update is detected, this IT component
 * 		updates the bump command id so at command end mdcache is purged and at tx commit
 * 		new global cache generation can be generated.
 * 		relation : The catalog table being touched
 */
void
mdver_inv_translator(Relation relation)
{
    if (!mdver_enabled())
    {
        return;
    }
    mdver_local* local_mdver = GetCurrentLocalMDVer();
    
    /*
     * We set local_mdver to null when transaction commit at AtEOXact_Inval.
     * There are some catalog updates after this for e.g. by storage manager.
     * We don't want to track those changes similar to how it is done for other
     * cache invalidation
     */
    if (NULL == local_mdver)
    {
        return;
    }
    
    /*
     * We don't track(bump command id) catalog tables changes in the AOSEG namespace.
     * These are modified for DML only (not DDL)
     */
    if (IsAoSegmentRelation(relation))
    {
		return;
    }
    
#ifdef MD_VERSIONING_INSTRUMENTATION
    elog(gp_mdver_loglevel, "MDVer : INV Translator marking command and transaction as dirty");
#endif
    
    mdver_mark_dirty_xact(local_mdver);
    mdver_mark_dirty_mdcache();
}

static void
mdver_mark_dirty_mdcache(void)
{
	mdver_dirty_mdcache = true;
}
