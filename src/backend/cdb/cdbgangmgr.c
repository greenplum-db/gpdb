/*-------------------------------------------------------------------------
 *
 * cdbgangmgr.c
 *
 *	  Gang manager
 *
 * Copyright (c) 2005-2016, Pivotal inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbgang.h"
#include "cdb/cdbgangmgr.h"

extern GangMgrMethods LegacyGangMgrMethods;

inline GangMgrMethods GetGangMgr() {
	return LegacyGangMgrMethods;
}
