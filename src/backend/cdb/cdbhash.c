/*--------------------------------------------------------------------------
 *
 * cdbhash.c
 *	  Provides hashing routines to support consistant data distribution/location
 *    within Greenplum Database.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbhash.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_opclass.h"
#include "cdb/cdbhash.h"
#include "commands/defrem.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * GUC to enable use of legacy hash opclasses by default. If set,
 * and a CREATE TABLE command doesn't specify opclasses for distribution
 * keys, the legacy hash opclasses are used instead of the default
 * PostgreSQL hash opclasses. If a datatype doesn't have legacy hash
 * opclasses, the normal defaults are still used.
 *
 * This is normally set as a database-level setting, on databases
 * that have been pg_upgraded from a previous version. But it can also be
 * handy to set it with SET or in postgresql.conf for testing purposes.
 */
bool gp_use_legacy_hashops = false;

/* Fast mod using a bit mask, assuming that y is a power of 2 */
#define FASTMOD(x,y)		((x) & ((y)-1))

/* local function declarations */
static int	ispowof2(int numsegs);
static inline int32 jump_consistent_hash(uint64 key, int32 num_segments);

/*================================================================
 *
 * HASH API FUNCTIONS
 *
 *================================================================
 */

/*
 * Create a CdbHash for this session.
 *
 * CdbHash maintains the following information about the hash.
 * In here we set the variables that should not change in the scope of the newly created
 * CdbHash, these are:
 *
 * 1 - number of segments in Greenplum Database.
 * 2 - reduction method.
 * 3 - distribution key column hash functions.
 *
 * The hash value itself will be initialized for every tuple in cdbhashinit()
 */
CdbHash *
makeCdbHash(int numsegs, int natts, Oid *hashfuncs)
{
	CdbHash    *h;
	int			i;
	bool		is_legacy_hash = false;

	Assert(numsegs > 0);		/* verify number of segments is legal. */

	/* Allocate a new CdbHash, with space for the datatype OIDs. */
	h = palloc(sizeof(CdbHash));

	/*
	 * set this hash session characteristics.
	 */
	h->hash = 0;
	h->numsegs = numsegs;

	/* Load hash function info */
	h->hashfuncs = (FmgrInfo *) palloc(natts * sizeof(FmgrInfo));
	for (i = 0; i < natts; i++)
	{
		Oid			funcid = hashfuncs[i];

		if (isLegacyCdbHashFunction(funcid))
			is_legacy_hash = true;

		fmgr_info(funcid, &h->hashfuncs[i]);
	}
	h->natts = natts;
	h->is_legacy_hash = is_legacy_hash;

	/*
	 * set the reduction algorithm: If num_segs is power of 2 use bit mask,
	 * else use lazy mod (h mod n)
	 */
	if (is_legacy_hash)
		h->reducealg = ispowof2(numsegs) ? REDUCE_BITMASK : REDUCE_LAZYMOD;
	else
		h->reducealg = REDUCE_JUMP_HASH;

	ereport(DEBUG4,
			(errmsg("CDBHASH hashing into %d segment databases", h->numsegs)));

	return h;
}

/*
 * Convenience routine, to create a CdbHash according to a relation's
 * distribution policy.
 */
CdbHash *
makeCdbHashForRelation(Relation rel)
{
	GpPolicy   *policy = rel->rd_cdbpolicy;
	Oid		   *hashfuncs;
	int			i;
	TupleDesc	desc = RelationGetDescr(rel);

	hashfuncs = palloc(policy->nattrs * sizeof(Oid));

	for (i = 0; i < policy->nattrs; i++)
	{
		AttrNumber	attnum = policy->attrs[i];
		Oid			typeoid = TupleDescAttr(desc, attnum - 1)->atttypid;
		Oid			opfamily;

		opfamily = get_opclass_family(policy->opclasses[i]);

		hashfuncs[i] = cdb_hashproc_in_opfamily(opfamily, typeoid);
	}

	return makeCdbHash(policy->numsegments, policy->nattrs, hashfuncs);
}

/*
 * Initialize CdbHash for hashing the next tuple values.
 */
void
cdbhashinit(CdbHash *h)
{
	if (!h->is_legacy_hash)
		h->hash = 0;
	else
	{
		/* reset the hash value to the initial offset basis */
		h->hash = FNV1_32_INIT;
	}
}

/*
 * Add an attribute to the CdbHash calculation.
 *
 * Note: this must be called for each attribute, in order. If you try to hash
 * the attributes in a different order, you get a different hash value.
 */
void
cdbhash(CdbHash *h, int attno, Datum datum, bool isnull)
{
	uint32		hashkey = h->hash;

	if (!h->is_legacy_hash)
	{
		/* rotate hashkey left 1 bit at each step */
		hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

		if (!isnull)
		{
			LOCAL_FCINFO(fcinfo, 1);
			uint32		hkey;

			InitFunctionCallInfoData(*fcinfo, &h->hashfuncs[attno - 1], 1,
									 /* GPDB_12_MERGE_FIXME: always use default collation. Is that OK? */
									 DEFAULT_COLLATION_OID,
									 NULL, NULL);

			fcinfo->args[0].value = datum;
			fcinfo->args[0].isnull = false;

			hkey = DatumGetUInt32(FunctionCallInvoke(fcinfo));

			/* Check for null result, since caller is clearly not expecting one */
			if (fcinfo->isnull)
				elog(ERROR, "function %u returned NULL", fcinfo->flinfo->fn_oid);

			hashkey ^= hkey;
		}
	}
	else
	{
		magic_hash_stash = hashkey;
		if (!isnull)
		{
			LOCAL_FCINFO(fcinfo, 1);
			uint32		hkey;

			InitFunctionCallInfoData(*fcinfo, &h->hashfuncs[attno - 1], 1,
									 DEFAULT_COLLATION_OID, /* legacy hash functions don't care about collations */
									 NULL, NULL);

			fcinfo->args[0].value = datum;
			fcinfo->args[0].isnull = false;

			hkey = DatumGetUInt32(FunctionCallInvoke(fcinfo));

			/* Check for null result, since caller is clearly not expecting one */
			if (fcinfo->isnull)
				elog(ERROR, "function %u returned NULL", fcinfo->flinfo->fn_oid);

			hashkey = hkey;
		}
		else
			hashkey = cdblegacyhash_null();
		magic_hash_stash = FNV1_32_INIT;
	}
	h->hash = hashkey;
}

/*
 * Reduce the hash to a segment number.
 */
unsigned int
cdbhashreduce(CdbHash *h)
{
	int			result = 0;		/* TODO: what is a good initialization value?
								 * could we guarantee at this point that there
								 * will not be a negative segid in Greenplum
								 * Database and therefore initialize to this
								 * value for error checking? */

	Assert(h->reducealg == REDUCE_BITMASK ||
		   h->reducealg == REDUCE_LAZYMOD ||
		   h->reducealg == REDUCE_JUMP_HASH);
	Assert(h->natts > 0);

	/*
	 * Reduce our 32-bit hash value to a segment number
	 */
	switch (h->reducealg)
	{
		case REDUCE_BITMASK:
			result = FASTMOD(h->hash, (uint32) h->numsegs); /* fast mod (bitmask) */
			break;

		case REDUCE_LAZYMOD:
			result = (h->hash) % (h->numsegs);	/* simple mod */
			break;

		case REDUCE_JUMP_HASH:
			result = jump_consistent_hash(h->hash, h->numsegs);
			break;
	}

	return result;
}

/*
 * Return a random segment number, for randomly distributed policy.
 */
unsigned int
cdbhashrandomseg(int numsegs)
{
	/*
	 * Note: Using modulo like this has a bias towards low values. But that's
	 * acceptable for our use case.
	 *
	 * For example, if MAX_RANDOM_VALUE was 5, and you did "random() % 4",
	 * value 0 would occur twice as often as others, because you would get 0
	 * when random() returns 0 or 4, while other values would only be returned
	 * with one return value of random(). But in reality, MAX_RANDOM_VALUE is
	 * 2^31, and the effect is not significant when the upper bound is much
	 * smaller than MAX_RANDOM_VALUE. This function is intended for choosing a
	 * segment in random, and the number of segments is much smaller than
	 * 2^31, so we're good.
	 */
	return random() % numsegs;
}


/*================================================================
 *
 * CATALOG LOOKUP FUNCTIONS
 *
 *================================================================
 */

/*
 * Get the operator class to use for column definition.
 *
 * This is used when parsing the DISTRIBUTED BY of CREATE TABLE and ALTER TABLE,
 * to determine the operator class to use.
 *
 * Mostly the same as ResolveOpClass(), but this knows about the
 * gp_use_legacy_hashops GUC, and gives a slightly more appropriate
 * error message if no opclass is found.
 */
Oid
cdb_get_opclass_for_column_def(List *opclassName, Oid attrType)
{
	Oid			opclass = InvalidOid;

	if (opclassName)
	{
		opclass = ResolveOpClass(opclassName, attrType,
								 "hash", HASH_AM_OID);
	}
	else
	{
		if (gp_use_legacy_hashops)
			opclass = get_legacy_cdbhash_opclass_for_base_type(attrType);

		if (!opclass)
			opclass = cdb_default_distribution_opclass_for_type(attrType);

		/*
		 * Same error message as in ResolveOpClass, except for the hint, for
		 * consistency.
		 */
		if (!OidIsValid(opclass))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("data type %s has no default operator class for access method \"%s\"",
							format_type_be(attrType), "hash"),
					 errhint("You must specify an operator class or define a default operator class for the data type.")));
	}

	Assert(OidIsValid(opclass));
	return opclass;
}

/*
 * Return the default operator family to use for distributing the given type.
 *
 * This is used when redistributing data, e.g. for GROUP BY, or DISTINCT.
 */
Oid
cdb_default_distribution_opfamily_for_type(Oid typeoid)
{
	TypeCacheEntry *tcache;

	tcache = lookup_type_cache(typeoid,
							   TYPECACHE_HASH_OPFAMILY |
							   TYPECACHE_HASH_PROC |
							   TYPECACHE_EQ_OPR);

	if (!tcache->hash_opf)
		return InvalidOid;
	if (!tcache->hash_proc)
		return InvalidOid;
	if (!tcache->eq_opr)
		return InvalidOid;

	return tcache->hash_opf;
}

/*
 * Return the default operator class to use for distributing the given type.
 *
 * Like cdb_default_distribution_opfamily_for_type(), but returns the
 * operator class, instead of the family. This is used e.g when choosing
 * distribution keys during CREATE TABLE.
 */
Oid
cdb_default_distribution_opclass_for_type(Oid typeoid)
{
	Oid			opfamily;

	opfamily = cdb_default_distribution_opfamily_for_type(typeoid);
	if (!opfamily)
		return InvalidOid;

	return GetDefaultOpClass(typeoid, HASH_AM_OID);
}

/*
 * Look up the hash function, for given datatype, in given op family.
 */
Oid
cdb_hashproc_in_opfamily(Oid opfamily, Oid typeoid)
{
	Oid			hashfunc;
	CatCList   *catlist;
	int			i;

	/* First try a simple lookup. */
	hashfunc = get_opfamily_proc(opfamily, typeoid, typeoid, HASHSTANDARD_PROC);
	if (hashfunc)
		return hashfunc;

	/*
	 * Not found. Check for the case that there is a function for another datatype
	 * that's nevertheless binary coercible. (At least 'varchar' ops rely on this,
	 * to leverage the text operator.
	 */
	catlist = SearchSysCacheList1(AMPROCNUM, ObjectIdGetDatum(opfamily));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amproc amproc_form = (Form_pg_amproc) GETSTRUCT(tuple);

		if (amproc_form->amprocnum != HASHSTANDARD_PROC)
			continue;

		if (amproc_form->amproclefttype != amproc_form->amprocrighttype)
			continue;

		if (IsBinaryCoercible(typeoid, amproc_form->amproclefttype))
		{
			/* found it! */
			hashfunc = amproc_form->amproc;
			break;
		}
	}

	ReleaseSysCacheList(catlist);

	if (!hashfunc)
		elog(ERROR, "could not find hash function for type %u in operator family %u",
			 typeoid, opfamily);

	return hashfunc;
}

/*
 * Look up an equality operator for given datatype, in given op family.
 */
Oid
cdb_eqop_in_hash_opfamily(Oid opfamily, Oid typeoid)
{
	Oid			eqop;
	CatCList   *catlist;
	int			i;

	/* First try a simple lookup. */
	eqop = get_opfamily_member(opfamily, typeoid, typeoid, HTEqualStrategyNumber);
	if (eqop)
		return eqop;

	/*
	 * Not found. Check for the case that there is a function for another datatype
	 * that's nevertheless binary coercible. (At least 'varchar' ops rely on this,
	 * to leverage the text operator.
	 */
	catlist = SearchSysCacheList1(AMOPSTRATEGY, ObjectIdGetDatum(opfamily));

	for (i = 0; i < catlist->n_members; i++)
	{
		HeapTuple	tuple = &catlist->members[i]->tuple;
		Form_pg_amop amop_form = (Form_pg_amop) GETSTRUCT(tuple);

		Assert(amop_form->amopmethod = HASH_AM_OID);

		if (amop_form->amopstrategy != HTEqualStrategyNumber)
			continue;

		if (amop_form->amoplefttype != amop_form->amoprighttype)
			continue;

		if (IsBinaryCoercible(typeoid, amop_form->amoplefttype))
		{
			/* found it! */
			eqop = amop_form->amopopr;
			break;
		}
	}

	ReleaseSysCacheList(catlist);

	if (!eqop)
		elog(ERROR, "could not find equality operator for type %u in operator family %u",
			 typeoid, opfamily);

	return eqop;
}

/*================================================================
 *
 * GENERAL PURPOSE UTILS
 *
 *================================================================
 */

/*
 * returns 1 is the input int is a power of 2 and 0 otherwise.
 */
static int
ispowof2(int numsegs)
{
	return !(numsegs & (numsegs - 1));
}

/*
 * The following jump consistent hash algorithm is
 * just the one from the original paper:
 * https://arxiv.org/abs/1406.2294
 */

static inline int32
jump_consistent_hash(uint64 key, int32 num_segments)
{
	int64 b = -1;
	int64 j = 0;
	while (j < num_segments)
	{
		b = j;
		key = key * 2862933555777941757ULL + 1;
		j = (b + 1) * ((double)(1LL << 31) / (double)((key >> 33) + 1));
	}
	return b;
}
