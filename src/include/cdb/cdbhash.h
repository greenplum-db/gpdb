/*--------------------------------------------------------------------------
 *
 * cdbhash.h
 *	 Definitions and API functions for cdbhash.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbhash.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef CDBHASH_H
#define CDBHASH_H

#include "utils/rel.h"

/* GUC */
extern bool gp_use_legacy_hashops;

/*
 * reduction methods.
 */
typedef enum
{
	REDUCE_LAZYMOD = 1,
	REDUCE_BITMASK,
	REDUCE_JUMP_HASH
} CdbHashReduce;

/*
 * Structure that holds Greenplum Database hashing information.
 */
typedef struct CdbHash
{
	uint32		hash;			/* The result hash value							*/
	int			numsegs;		/* number of segments in Greenplum Database used for
								 * partitioning  */
	CdbHashReduce reducealg;	/* the algorithm used for reducing to buckets		*/
	bool		is_legacy_hash;

	int			natts;
	FmgrInfo   *hashfuncs;
} CdbHash;

/*
 * Create and initialize a CdbHash in the current memory context.
 */
extern CdbHash *makeCdbHash(int numsegs, int natts, Oid *typeoids);
extern CdbHash *makeCdbHashForRelation(Relation rel);

/*
 * Initialize CdbHash for hashing the next tuple values.
 */
extern void cdbhashinit(CdbHash *h);

/*
 * Add an attribute to the hash calculation.
 */
extern void cdbhash(CdbHash *h, int attno, Datum datum, bool isnull);

/*
 * Reduce the hash to a segment number.
 */
extern unsigned int cdbhashreduce(CdbHash *h);

/*
 * Return a random segment number, for a randomly distributed policy.
 */
extern unsigned int cdbhashrandomseg(int numsegs);

/*
 * Catalog lookup functions related to distribution keys and hash opclasses.
 */
extern Oid cdb_get_opclass_for_column_def(List *opclass, Oid attrType);
extern Oid cdb_default_distribution_opfamily_for_type(Oid typid);
extern Oid cdb_default_distribution_opclass_for_type(Oid typid);
extern Oid cdb_hashproc_in_opfamily(Oid opfamily, Oid typeoid);
extern Oid cdb_eqop_in_hash_opfamily(Oid opfamily, Oid typeoid);

/* prototypes and other things, from cdblegacyhash.c */

/* 32 bit FNV-1  non-zero initial basis */
#define FNV1_32_INIT ((uint32)0x811c9dc5)

extern uint32 magic_hash_stash;

extern bool isLegacyCdbHashFunction(Oid funcid);

extern Oid get_legacy_cdbhash_opclass_for_base_type(Oid typid);
extern uint32 cdblegacyhash_null(void);

#endif   /* CDBHASH_H */
