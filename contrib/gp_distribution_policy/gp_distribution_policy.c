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
#include "cdb/cdbhash.h"
#include "cdb/cdbvars.h"
#include "utils/lsyscache.h"
#include "miscadmin.h"
#include "catalog/gp_policy.h"
#include "utils/tqual.h"


#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum gp_distribution_policy_heap_table_check(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distribution_policy_heap_table_check);

/* Extracts array of the form {1,2,3} to an ArrayType. */
static void
extract_INT2OID_array(Datum array_datum, int *lenp, int16 **vecp);

/* Allocate GpPolicy struct from an ArrayType */
static GpPolicy *
set_distribution_policy (Datum array_distribution);

/* Get base type OID */
static Oid
get_base_dataType(Oid att_datatype);


/*
 * Extract len and pointer to buffer from an int16[] (vector) Datum
 * representing a PostgreSQL INT2OID type.
 */
static void
extract_INT2OID_array(Datum array_datum, int *lenp, int16 **vecp)
{
	ArrayType  *array_type;
	
	Assert(lenp != NULL);
	Assert(vecp != NULL);

	array_type = DatumGetArrayTypeP(array_datum);
	
	Assert(ARR_NDIM(array_type) == 1);
	Assert(ARR_ELEMTYPE(array_type) == INT2OID);
	Assert(ARR_LBOUND(array_type)[0] == 1);
	*lenp = ARR_DIMS(array_type)[0];
	*vecp = (int16 *) ARR_DATA_PTR(array_type);
	
	return;
}

/* 
 * Allocate GpPolicy from a given distribution in an Array of OIDs 
 */
static GpPolicy *
set_distribution_policy (Datum array_distribution)
{
	GpPolicy  *policy = NULL;
	
	int nattrs = 0;
	int16      *attrnums = NULL;
	
	extract_INT2OID_array(array_distribution, &nattrs, &attrnums);
	
	/* Validate that there is a distribution policy */
	if (0 >= nattrs)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("there is no distribution set in target table")));
	}

	policy = (GpPolicy *)palloc(sizeof(*policy) - sizeof(policy->attrs) + 
								nattrs * sizeof(policy->attrs[0]));
	
	policy->ptype = POLICYTYPE_PARTITIONED;
	policy->nattrs = nattrs;
	
	for (int i = 0; i < nattrs; i++)
	{
		policy->attrs[i] = attrnums[i];
	}
	
	return policy;
}

/* 
 * Get base data type from complex data types (e.g. Arrays) 
 */
static Oid
get_base_dataType(Oid att_datatype)
{
	Oid att_type = att_datatype;
	
	if (get_typtype(att_type) == 'd')
	{
		att_type = getBaseType(att_type);
	}
	
	switch (att_type)
	{
		case INT2ARRAYOID:
		case INT4ARRAYOID:
		case INT8ARRAYOID:
		case FLOAT4ARRAYOID:
		case FLOAT8ARRAYOID:
		case REGTYPEARRAYOID:
			att_type = ANYARRAYOID;
			/* Fall through */
			break;
		default:
			break;
	}
	
	return att_type;
}

/* 
 * Verifies the correct data distribution (given a GpPolicy) 
 * of a heap table in a segment. 
 */
Datum
gp_distribution_policy_heap_table_check(PG_FUNCTION_ARGS)
{
	bool result = true;
	
	Assert(2 == PG_NARGS());

	Oid relOid = PG_GETARG_OID(0);
	Datum  array_distribution = PG_GETARG_DATUM(1);

	Assert(array_distribution);

	/* Get distribution policy from arguments */
	GpPolicy  *policy = set_distribution_policy(array_distribution);

	/* Open relation in segment */
	Relation rel = heap_open(relOid, AccessShareLock);

	/* Validate that the relation is a heap table */
	if (!RelationIsHeap(rel))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				errmsg("input relation is not a heap table")));
	}

	HeapScanDesc scandesc = heap_beginscan(rel, SnapshotNow, 0, NULL);
	HeapTuple    tuple = heap_getnext(scandesc, ForwardScanDirection);

	while (HeapTupleIsValid(tuple))
	{
		CHECK_FOR_INTERRUPTS();

		/* Initialize hash function and structure */
		CdbHash *hash = makeCdbHash(GpIdentity.numsegments);
		cdbhashinit(hash);
		
		for(int i = 0; i < policy->nattrs; i++)
		{
			bool            isNull;
			Datum           attr;
			TupleDesc desc = RelationGetDescr(rel);
			
			attr = heap_getattr(tuple, policy->attrs[i],
							desc, &isNull);
			
			Oid att_type = get_base_dataType(desc->attrs[i]->atttypid);
			/* Consider every attribute in the distribution policy
			* as part of the hash computation */
			cdbhash(hash, attr, att_type);
			
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
