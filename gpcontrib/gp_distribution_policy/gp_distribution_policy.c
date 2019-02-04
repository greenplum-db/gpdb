/*
 * Copyright (c) 2013 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 *
 * ---------------------------------------------------------------------
 *
 * Interface to functions related to checking the correct distribution in GPDB.
 *
 * This is used to expose these functions in a dynamically linked library
 * so that they can be referenced by using CREATE FUNCTION command in SQL,
 * like below:
 *
 *CREATE OR REPLACE FUNCTION gp_distribution_policy_heap_table_check(oid, smallint[])
 * RETURNS bool
 * AS '$libdir/gp_distribution_policy.so','gp_distribution_policy_heap_table_check'
 * LANGUAGE C VOLATILE STRICT; *
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbvars.h"
#include "utils/lsyscache.h"
#include "miscadmin.h"
#include "catalog/gp_policy.h"
#include "utils/array.h"
#include "utils/tqual.h"


#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum gp_distribution_policy_heap_table_check(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distribution_policy_heap_table_check);

/* 
 * Verifies the correct data distribution (given a GpPolicy) 
 * of a heap table in a segment. 
 */
Datum
gp_distribution_policy_heap_table_check(PG_FUNCTION_ARGS)
{
	Oid			relOid = PG_GETARG_OID(0);
	bool		result = true;

	/* Open relation in segment */
	Relation rel = heap_open(relOid, AccessShareLock);

	GpPolicy *policy = rel->rd_cdbpolicy;

	/* Validate that the relation is a heap table */
	if (!RelationIsHeap(rel))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("input relation is not a heap table")));
	}

	HeapScanDesc scandesc = heap_beginscan(rel, GetActiveSnapshot(), 0, NULL);
	HeapTuple    tuple = heap_getnext(scandesc, ForwardScanDirection);
	TupleDesc	desc = RelationGetDescr(rel);

	/* Initialize hash function and structure */
	CdbHash *hash;

	hash = makeCdbHashForRelation(rel);

	while (HeapTupleIsValid(tuple))
	{
		CHECK_FOR_INTERRUPTS();

		cdbhashinit(hash);

		/* Add every attribute in the distribution policy to the hash */
		for (int i = 0; i < policy->nattrs; i++)
		{
			int			attnum = policy->attrs[i];
			bool		isNull;
			Datum		attr;

			attr = heap_getattr(tuple, attnum, desc, &isNull);

			cdbhash(hash, i + 1, attr, isNull);
		}

		/* End check if one tuple is in the wrong segment */
		if (cdbhashreduce(hash) != GpIdentity.segindex)
		{
			result = false;
			break;
		}

		tuple = heap_getnext(scandesc, ForwardScanDirection);
	}

	heap_endscan(scandesc);
	heap_close(rel, AccessShareLock);
	
	PG_RETURN_BOOL(result);
}
