/*-------------------------------------------------------------------------
 *
 * cdbcat.h
 *	  routines for reading info from Greenplum Database schema tables
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbcat.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBCAT_H
#define CDBCAT_H

#include "access/attnum.h"
#include "utils/relcache.h"

/*
 * Context information for index_check_policy_compatible(), used
 * for building error message.
 */
typedef struct
{
	bool		for_alter_dist_policy;
	bool		is_constraint;
	bool		is_unique;
	bool		is_primarykey;

	char	   *constraint_name;
} index_check_policy_compatible_context;

extern bool index_check_policy_compatible(GpPolicy *policy,
							  TupleDesc desc,
							  AttrNumber *indattr,
							  Oid *indclasses,
							  Oid *exclop,
							  int nidxatts,
							  bool report_error,
							  index_check_policy_compatible_context *error_context);

#endif   /* CDBCAT_H */
