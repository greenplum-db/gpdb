/*-------------------------------------------------------------------------
 *
 * cdbcat.h
 *	  routines for reading info from Greenplum Database schema tables
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBCAT_H
#define CDBCAT_H

#include "access/attnum.h"
#include "utils/relcache.h"

extern void checkPolicyForUniqueIndex(Relation rel, AttrNumber *indattr,
									  int nidxatts, bool isprimary, 
									  bool has_exprs, bool has_pkey,
									  bool has_ukey);

#endif   /* CDBCAT_H */
