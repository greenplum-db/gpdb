/*--------------------------------------------------------------------------
 *
 * cdbpartition.c
 *	  Provides utility routines to support sharding via partitioning
 *	  within Greenplum Database.
 *
 *    Many items are just extensions of tablecmds.c.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpartition.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/hash.h"
#include "access/heapam.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_constraint_fn.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_type.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_partition_encoding.h"
#include "catalog/namespace.h"
#include "cdb/cdbpartition.h"
#include "cdb/cdbvars.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "nodes/makefuncs.h"
#include "optimizer/var.h"
#include "parser/analyze.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_partition.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_utilcmd.h"
#include "storage/lmgr.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/elog.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/ruleutils.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

#define DEFAULT_CONSTRAINT_ESTIMATE 16
#define MIN_XCHG_CONTEXT_SIZE 4096
#define INIT_XCHG_BLOCK_SIZE 4096
#define MAX_XCHG_BLOCK_SIZE 4096

typedef struct
{
	char	   *key;
	bool        indexed_cons;
	List	   *table_cons;
	List	   *part_cons;
	List	   *cand_cons;
} ConstraintEntry;

typedef struct
{
	Node	   *entry;
} ConNodeEntry;


typedef enum
{
	PART_TABLE,
	PART_PART,
	PART_CAND
} PartExchangeRole;
static void record_constraints(Relation pgcon, MemoryContext context,
				   HTAB *hash_tbl, Relation rel,
				   PartExchangeRole xrole);

static void exchange_constraint_names(Relation part,
									   Relation cand,
									   NameData part_name,
									   NameData cand_name);

static void rename_constraint(Relation rel,
							   char *old_name,
							   char *new_name);

static char *constraint_names(List *cons);

static void constraint_diffs(List *cons_a, List *cons_b, bool match_inheritance,
							  List **missing, List **extra);

static void add_template_encoding_clauses(Oid relid, Oid paroid, List *stenc);

static PartitionNode *findPartitionNodeEntry(PartitionNode *partitionNode, Oid partOid);

static uint32
			constrNodeHash(const void *keyPtr, Size keysize);

static int
			constrNodeMatch(const void *keyPtr1, const void *keyPtr2, Size keysize);

static void parruleord_open_gap(Oid partid, int16 level, Oid parent,
					int16 ruleord, int16 stopkey, bool closegap);
static bool has_external_partition(PartitionNode *n);

/*
 * Hash keys are null-terminated C strings assumed to be stably
 * allocated. We accomplish this by allocating them in a context
 * that lives as long as the hash table that contains them.
 */

/* Hash entire string. */
static uint32
key_string_hash(const void *key, Size keysize)
{
	Size		s_len = strlen((const char *) key);

	Assert(keysize == sizeof(char *));
	return DatumGetUInt32(hash_any((const unsigned char *) key, (int) s_len));
}

/* Compare entire string. */
static int
key_string_compare(const void *key1, const void *key2, Size keysize)
{
	Assert(keysize == sizeof(char *));
	return strcmp(((ConstraintEntry *) key1)->key, key2);
}

/* Copy string by copying pointer. */
static void *
key_string_copy(void *dest, const void *src, Size keysize)
{
	Assert(keysize == sizeof(char *));

	*((char **) dest) = (char *) src;	/* trust caller re allocation */
	return NULL;				/* not used */
}

static char parttype_to_char(PartitionByType type);
static void add_partition(Partition *part);
static void add_partition_rule(PartitionRule *rule);
static Oid	get_part_oid(Oid rootrelid, int16 parlevel, bool istemplate);
static Datum *magic_expr_to_datum(Relation rel, PartitionNode *partnode,
					Node *expr, bool **ppisnull);
static Oid	selectPartitionByRank(PartitionNode *partnode, int rnk);
static bool compare_partn_opfuncid(PartitionNode *partnode,
					   char *pub, char *compare_op,
					   List *colvals,
					   Datum *values, bool *isnull,
					   TupleDesc tupdesc);
static PartitionNode *selectListPartition(PartitionNode *partnode, Datum *values, bool *isnull,
					TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
					Oid *foundOid, PartitionRule **prule, Oid exprTypid);
static void get_cmp_func(Oid opclass, FmgrInfo *fmgrinfo, Oid lhstypid, Oid rhstypid);
static int range_test(Datum tupval, FmgrInfo *cmpfuncs, int keyno,
		   PartitionRule *rule);
static PartitionNode *selectRangePartition(PartitionNode *partnode, Datum *values, bool *isnull,
					 TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
					 Oid *foundOid, int *pSearch, PartitionRule **prule, Oid exprTypid);
static Oid selectPartition1(PartitionNode *partnode, Datum *values, bool *isnull,
				 TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
				 int *pSearch,
				 PartitionNode **ppn_out);
static int atpxPart_validate_spec(
					   PartitionBy *pBy,
					   Relation rel,
					   CreateStmt *ct,
					   PartitionElem *pelem,
					   PartitionNode *pNode,
					   char *partName,
					   bool isDefault,
					   PartitionByType part_type,
					   char *partDesc);

static void atpxSkipper(PartitionNode *pNode, int *skipped);

static List *build_rename_part_recurse(PartitionRule *rule, const char *old_parentname,
						  const char *new_parentname,
						  int *skipped);
static Oid
			get_opfuncid_by_opname(List *opname, Oid lhsid, Oid rhsid);

static PgPartRule *get_pprule_from_ATC(Relation rel, AlterTableCmd *cmd);

static List *get_partition_rules(PartitionNode *pn);

static bool
			relation_has_supers(Oid relid);

static NewConstraint *constraint_apply_mapped(HeapTuple tuple, TupleConversionMap *map, Relation cand,
						bool validate, Relation pgcon);

static char *ChooseConstraintNameForPartitionCreate(const char *rname,
									   const char *cname,
									   const char *label,
									   List *used_names);

static Bitmapset *get_partition_key_bitmapset(Oid relid);

static List *get_deparsed_partition_encodings(Oid relid, Oid paroid);
static List *rel_get_leaf_relids_from_rule(Oid ruleOid);

/*
 * Is the given relation the default partition of a partition table.
 */
bool
rel_is_default_partition(Oid relid)
{
	Relation	partrulerel;
	HeapTuple	tuple;
	bool		parisdefault;
	ScanKeyData scankey;
	SysScanDesc sscan;

	/*
	 * Though pg_partition and pg_partition_rule are only populated on the
	 * entry database, we accept calls from QEs running a segment, but return
	 * false.
	 */
	if (!IS_QUERY_DISPATCHER())
		return false;

	partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);

	Insist(HeapTupleIsValid(tuple));

	parisdefault = ((Form_pg_partition_rule) GETSTRUCT(tuple))->parisdefault;

	systable_endscan(sscan);
	heap_close(partrulerel, AccessShareLock);

	return parisdefault;
}

/* Is the given relation the top relation of a partitioned table?
 *
 *   exists (select *
 *           from pg_partition
 *           where parrelid = relid)
 *
 * False for interior branches and leaves or when called other
 * then on the entry database, i.e., only meaningful on the
 * entry database.
 */
bool
rel_is_partitioned(Oid relid)
{
	ScanKeyData scankey;
	Relation	rel;
	SysScanDesc sscan;
	bool		result;

	/*
	 * Though pg_partition and pg_partition_rule are only populated on the
	 * entry database, we accept calls from QEs running a segment, but return
	 * false.
	 */
	if (!IS_QUERY_DISPATCHER())
		return false;

	ScanKeyInit(&scankey, Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	rel = heap_open(PartitionRelationId, AccessShareLock);
	sscan = systable_beginscan(rel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);

	result = (systable_getnext(sscan) != NULL);

	systable_endscan(sscan);

	heap_close(rel, AccessShareLock);

	return result;
}

/*
 * Return an integer list of the attribute numbers of the partitioning
 * key of the partitioned table identified by the argument or NIL.
 *
 * This is similar to get_partition_attrs but is driven by OID and
 * the partition catalog, not by a PartitionNode.
 *
 * Note: Only returns a non-empty list of keys for partitioned table
 *       as a whole.  Returns empty for non-partitioned tables or for
 *       parts of partitioned tables.  Key attributes are attribute
 *       numbers in the partitioned table.
 */
List *
rel_partition_key_attrs(Oid relid)
{
	Relation	rel;
	ScanKeyData key;
	SysScanDesc scan;
	HeapTuple	tuple;
	List	   *pkeys = NIL;

	/*
	 * Table pg_partition is only populated on the entry database, however, we
	 * disable calls from outside dispatch to foil use of utility mode.  (Full
	 * UCS may may this test obsolete.)
	 */
	if (Gp_session_role != GP_ROLE_DISPATCH)
		elog(ERROR, "mode not dispatch");

	rel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&key,
				Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));


	scan = systable_beginscan(rel, PartitionParrelidIndexId, true,
							  NULL, 1, &key);

	tuple = systable_getnext(scan);

	while (HeapTupleIsValid(tuple))
	{
		Index		i;
		Form_pg_partition p = (Form_pg_partition) GETSTRUCT(tuple);

		if (p->paristemplate)
		{
			tuple = systable_getnext(scan);
			continue;
		}

		for (i = 0; i < p->parnatts; i++)
		{
			pkeys = lappend_int(pkeys, (Oid) p->paratts.values[i]);
		}

		tuple = systable_getnext(scan);
	}

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	return pkeys;
}

/*
 * Return a list of lists representing the partitioning keys of the partitioned
 * table identified by the argument or NIL. The keys are in the order of
 * partitioning levels. Each of the lists inside the main list correspond to one
 * level, and may have one or more attribute numbers depending on whether the
 * part key for that level is composite or not.
 *
 * Note: Only returns a non-empty list of keys for partitioned table
 *       as a whole.  Returns empty for non-partitioned tables or for
 *       parts of partitioned tables.  Key attributes are attribute
 *       numbers in the partitioned table.
 */
List *
rel_partition_keys_ordered(Oid relid)
{
	List	   *pkeys = NIL;

	rel_partition_keys_kinds_ordered(relid, &pkeys, NULL);
	return pkeys;
}

/*
 * Output a list of lists representing the partitioning keys and a list representing
 * the partitioning kinds of the partitioned table identified by the relid or NIL.
 * The keys and kinds are in the order of partitioning levels.
 */
void
rel_partition_keys_kinds_ordered(Oid relid, List **pkeys, List **pkinds)
{
	Relation	partrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	List	   *levels = NIL;
	List	   *keysUnordered = NIL;
	List	   *kindsUnordered = NIL;
	int			nlevels = 0;
	HeapTuple	tuple = NULL;

	partrel = heap_open(PartitionRelationId, AccessShareLock);

	/* SELECT * FROM pg_partition WHERE parrelid = :1 */
	ScanKeyInit(&scankey, Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	sscan = systable_beginscan(partrel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		Form_pg_partition p = (Form_pg_partition) GETSTRUCT(tuple);

		if (p->paristemplate)
		{
			continue;
		}

		List	   *levelkeys = NIL;

		for (int i = 0; i < p->parnatts; i++)
		{
			levelkeys = lappend_int(levelkeys, (Oid) p->paratts.values[i]);
		}

		nlevels++;
		levels = lappend_int(levels, p->parlevel);

		if (pkeys != NULL)
			keysUnordered = lappend(keysUnordered, levelkeys);

		if (pkinds != NULL)
			kindsUnordered = lappend_int(kindsUnordered, p->parkind);
	}
	systable_endscan(sscan);
	heap_close(partrel, AccessShareLock);

	if (1 == nlevels)
	{
		list_free(levels);

		if (pkeys != NULL)
			*pkeys = keysUnordered;

		if (pkinds != NULL)
			*pkinds = kindsUnordered;

		return;
	}

	/* now order the keys and kinds by level */
	for (int i = 0; i < nlevels; i++)
	{
		ListCell   *cell;
		int			pos = 0;

		/* Find i's position in the 'levels' list. */
		foreach(cell, levels)
		{
			if (lfirst_int(cell) == i)
				break;
			++pos;
		}
		Assert(cell != NULL);

		if (pkeys != NULL)
			*pkeys = lappend(*pkeys, list_nth(keysUnordered, pos));

		if (pkinds != NULL)
			*pkinds = lappend_int(*pkinds, list_nth_int(kindsUnordered, pos));
	}
	list_free(levels);
	list_free(keysUnordered);
	list_free(kindsUnordered);
}

 /*
  * Does relation have a external partition? Returns true only when the input
  * is the root partition of a partitioned table and it has external
  * partitions.
  */
bool
rel_has_external_partition(Oid relid)
{
	PartitionNode *n = get_parts(relid, 0 /* level */ ,
								 0 /* parent */ , false /* inctemplate */ , true /* includesubparts */ );

	if (n == NULL)
		return false;

	return has_external_partition(n);
}

/*
 * Does relation have an appendonly partition?
 * Returns true only when the input is the root partition
 * of a partitioned table and it has appendonly partitions.
 */
bool
rel_has_appendonly_partition(Oid relid)
{
	ListCell   *lc = NULL;
	List	   *leaf_oid_list = NIL;
	PartitionNode *n = get_parts(relid, 0 /* level */ ,
								 0 /* parent */ , false /* inctemplate */ , true /* includesubparts */ );

	if (n == NULL || n->num_rules == 0)
		return false;

	leaf_oid_list = all_leaf_partition_relids(n);	/* all leaves */

	foreach(lc, leaf_oid_list)
	{
		Relation	rel = heap_open(lfirst_oid(lc), NoLock);

		if (RelationIsAppendOptimized(rel))
		{
			heap_close(rel, NoLock);
			return true;
		}
		heap_close(rel, NoLock);
	}

	return false;
}

/*
 * Is relid a child in a partitioning hierarchy?
 *
 *    exists (select *
 *            from pg_partition_rule
 *            where parchildrelid = relid)
 *
 * False for the partitioned table as a whole or when called
 * other then on the entry database, i.e., only meaningful on
 * the entry database.
 */
bool
rel_is_child_partition(Oid relid)
{
	ScanKeyData scankey;
	Relation	rel;
	SysScanDesc sscan;
	bool		result;

	/*
	 * Though pg_partition and  pg_partition_rule are populated only on the
	 * entry database, are some unguarded calles that may come from segments,
	 * so we return false, even though we don't actually know.
	 */
	if (!IS_QUERY_DISPATCHER())
		return false;

	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	rel = heap_open(PartitionRuleRelationId, AccessShareLock);
	sscan = systable_beginscan(rel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);

	result = (systable_getnext(sscan) != NULL);

	systable_endscan(sscan);

	heap_close(rel, AccessShareLock);

	return result;
}

/*
 * Is relid an interior node of a partitioning hierarchy?
 *
 * To determine if it is an interior node or not, we need to find the depth of
 * our partition and check if there is at least one more level below us.
 *
 * Call only on entry database.
 */
bool
rel_is_interior_partition(Oid relid)
{
	HeapTuple	tuple;
	Oid			paroid = InvalidOid;
	bool		is_interior = false;
	int			mylevel = 0;
	Relation	partrulerel;
	Relation	partrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Oid			partitioned_rel = InvalidOid;	/* OID of the root table of
												 * the partition set */

	/*
	 * Find the pg_partition_rule entry to see if this is a child at all and,
	 * if so, to locate the OID for the pg_partition entry.
	 *
	 * SELECT paroid FROM pg_partition_rule WHERE parchildrelid = :1
	 */
	partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);

	if (tuple)
		paroid = ((Form_pg_partition_rule) GETSTRUCT(tuple))->paroid;
	else
		paroid = InvalidOid;

	systable_endscan(sscan);
	heap_close(partrulerel, AccessShareLock);

	if (!OidIsValid(paroid))
		return false;

	tuple = SearchSysCache1(PARTOID, ObjectIdGetDatum(paroid));

	Insist(HeapTupleIsValid(tuple));

	mylevel = ((Form_pg_partition) GETSTRUCT(tuple))->parlevel;
	partitioned_rel = ((Form_pg_partition) GETSTRUCT(tuple))->parrelid;
	ReleaseSysCache(tuple);

	/* SELECT * FROM pg_partition WHERE parrelid = :1 */
	partrel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partitioned_rel));
	sscan = systable_beginscan(partrel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		/* not interested in templates */
		if (((Form_pg_partition) GETSTRUCT(tuple))->paristemplate == false)
		{
			int			depth = ((Form_pg_partition) GETSTRUCT(tuple))->parlevel;
			if (depth > mylevel)
			{
				is_interior = true;
				break;
			}
		}
	}

	systable_endscan(sscan);
	heap_close(partrel, AccessShareLock);

	return is_interior;
}

/*
 * Is relid a leaf node of a partitioning hierarchy? If no, it does not follow
 * that it is the root.
 *
 * To determine if it is a leaf or not, we need to find the depth of our
 * partition and compare this to the maximum depth of the partition set itself.
 * If they're equal, we have a leaf, otherwise, something else.
 *
 * Call only on entry database.
 */
bool
rel_is_leaf_partition(Oid relid)
{
	HeapTuple	tuple;
	Oid			paroid = InvalidOid;
	int			maxdepth = 0;
	int			mylevel = 0;
	Relation	partrulerel;
	Relation	partrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Oid			partitioned_rel = InvalidOid;	/* OID of the root table of
												 * the partition set */

	/*
	 * Find the pg_partition_rule entry to see if this is a child at all and,
	 * if so, to locate the OID for the pg_partition entry.
	 *
	 * SELECT paroid FROM pg_partition_rule WHERE parchildrelid = :1
	 */
	partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);

	if (tuple)
		paroid = ((Form_pg_partition_rule) GETSTRUCT(tuple))->paroid;
	else
		paroid = InvalidOid;

	systable_endscan(sscan);
	heap_close(partrulerel, AccessShareLock);

	if (!OidIsValid(paroid))
		return false;

	tuple = SearchSysCache1(PARTOID, ObjectIdGetDatum(paroid));

	Insist(HeapTupleIsValid(tuple));

	mylevel = ((Form_pg_partition) GETSTRUCT(tuple))->parlevel;
	partitioned_rel = ((Form_pg_partition) GETSTRUCT(tuple))->parrelid;
	ReleaseSysCache(tuple);


	/* SELECT * FROM pg_partition WHERE parrelid = :1 */
	partrel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partitioned_rel));
	sscan = systable_beginscan(partrel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);

	/*
	 * Of course, we could just maxdepth++ but this seems safer -- we don't
	 * have to worry about the starting depth being 0, 1 or something else.
	 */
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		/* not interested in templates */
		if (((Form_pg_partition) GETSTRUCT(tuple))->paristemplate == false)
		{
			int			depth = ((Form_pg_partition) GETSTRUCT(tuple))->parlevel;

			maxdepth = Max(maxdepth, depth);
		}
	}

	systable_endscan(sscan);
	heap_close(partrel, AccessShareLock);

	return maxdepth == mylevel;
}

/* Determine the status of the given table with respect to partitioning.
 *
 * Uses lower level routines. Returns PART_STATUS_NONE for a non-partitioned
 * table or when called other then on the entry database, i.e., only meaningful
 * on  the entry database.
 */
PartStatus
rel_part_status(Oid relid)
{
	if (Gp_role != GP_ROLE_DISPATCH)
	{
		ereport(DEBUG1,
				(errmsg("requesting part status outside dispatch - returning none")));
		return PART_STATUS_NONE;
	}

	if (rel_is_partitioned(relid))
	{
		Assert(!rel_is_child_partition(relid) && !rel_is_leaf_partition(relid));
		return PART_STATUS_ROOT;
	}
	else						/* not an actual partitioned table root */
	{
		if (rel_is_child_partition(relid))
			return rel_is_leaf_partition(relid) ? PART_STATUS_LEAF : PART_STATUS_INTERIOR;
		else					/* not a part of a partitioned table */
			Assert(!rel_is_child_partition(relid));
	}
	return PART_STATUS_NONE;
}

/*
 * Insert the given tuple in the list of tuples according to
 * increasing OID value.
 */
static List *
sorted_insert_list(List *list, HeapTuple tuple)
{
	ListCell   *lc;
	ListCell   *lc_prev = NULL;
	HeapTuple	list_tup;
	List	   *ret_list = list;

	foreach(lc, ret_list)
	{
		list_tup = lfirst(lc);
		if (HeapTupleGetOid(list_tup) >
			HeapTupleGetOid(tuple))
		{
			break;
		}
		lc_prev = lc;
	}
	if (lc_prev == NULL)
	{
		ret_list = lcons(tuple, ret_list);
	}
	else
	{
		lappend_cell(ret_list, lc_prev, tuple);
	}
	return ret_list;
}

/* Locate all the constraints on the given open relation (rel) and
 * record them in the hash table (hash_tbl) of ConstraintEntry
 * structs.
 *
 * Depending on the value of xrole, the given relation must be either
 * the root, an existing part, or an exchange candidate for the same
 * partitioned table.  A copy of each constraint tuple found is
 * appended to the corresponding field of the hash entry.
 *
 * The key  of the hash table is a string representing the constraint
 * in SQL. This should be comparable across parts of a partitioning
 * hierarchy regardless of the history (hole pattern) or storage type
 * of the table.
 *
 * Note that pgcon (the ConstraintRelationId appropriately locked)
 * is supplied externally for efficiency.  No other relation should
 * be supplied via this argument.
 *
 * Memory allocated in here (strings, tuples, lists, list cells, etc)
 * is all associated with the hash table and is allocated in the given
 * memory context, so it will be easy to free in bulk.
 */
static void
record_constraints(Relation pgcon,
				   MemoryContext context,
				   HTAB *hash_tbl,
				   Relation rel,
				   PartExchangeRole xrole)
{
	HeapTuple	tuple;
	Oid			conid;
	char	   *condef;
	ConstraintEntry *entry;
	bool		found;
	MemoryContext oldcontext;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Form_pg_constraint con;


	ScanKeyInit(&scankey, Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(RelationGetRelid(rel)));
	sscan = systable_beginscan(pgcon, ConstraintRelidIndexId, true,
							   NULL, 1, &scankey);

	/* For each constraint on rel: */
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		oldcontext = MemoryContextSwitchTo(context);

		conid = HeapTupleGetOid(tuple);

		condef = pg_get_constraintexpr_string(conid);
		entry = (ConstraintEntry *) hash_search(hash_tbl,
												(void *) condef,
												HASH_ENTER,
												&found);

		/*
		 * A tuple isn't a Node, but we'll stick it in a List anyway, and just
		 * be careful.
		 */
		if (!found)
		{
			entry->key = condef;
			entry->indexed_cons = false;
			entry->table_cons = NIL;
			entry->part_cons = NIL;
			entry->cand_cons = NIL;
		}

		con = (Form_pg_constraint) GETSTRUCT(tuple);

		switch (con->contype)
		{
			case CONSTRAINT_PRIMARY:
			case CONSTRAINT_UNIQUE:
				entry->indexed_cons = true;
				break;
			/*
			 * Other types of constraints should not need special treatment
			 */
			case CONSTRAINT_CHECK:
			case CONSTRAINT_FOREIGN:
			case CONSTRAINT_TRIGGER:
				break;
			default:
				Assert(false);
		}

		tuple = heap_copytuple(tuple);
		switch (xrole)
		{
			case PART_TABLE:
				entry->table_cons = sorted_insert_list(
													   entry->table_cons, tuple);
				break;
			case PART_PART:
				entry->part_cons = sorted_insert_list(
													  entry->part_cons, tuple);
				break;
			case PART_CAND:
				entry->cand_cons = sorted_insert_list(
													  entry->cand_cons, tuple);
				break;
			default:
				Assert(FALSE);
		}

		MemoryContextSwitchTo(oldcontext);
	}
	systable_endscan(sscan);
}

/* Subroutine of ATPExecPartExchange used to swap constraints on existing
 * part and candidate part.  Note that this runs on both the QD and QEs
 * so must not assume availability of partition catalogs.
 *
 * table -- open relation of the parent partitioned table
 * part -- open relation of existing part to exchange
 * cand -- open relation of the candidate part
 * validate -- whether to collect constraints into a result list for
 *             enforcement during phase 3 (WITH/WITHOUT VALIDATION).
 */
List *
cdb_exchange_part_constraints(Relation table,
							  Relation part,
							  Relation cand,
							  bool validate,
							  bool is_split,
							  AlterPartitionCmd *pc)
{
	HTAB	   *hash_tbl;
	HASHCTL		hash_ctl;
	HASH_SEQ_STATUS hash_seq;
	Relation	pgcon;
	MemoryContext context;
	MemoryContext oldcontext;
	ConstraintEntry *entry;
	TupleConversionMap *p2t = NULL;
	TupleConversionMap *c2t = NULL;

	HeapTuple	tuple;
	Form_pg_constraint con;

	List	   *excess_constraints = NIL;
	List	   *missing_constraints = NIL;
	List	   *missing_inherited_constraints = NIL;
	List	   *missing_part_constraints = NIL;
	List	   *indexed_constraints = NIL;
	List	   *validation_list = NIL;
	int			delta_checks = 0;


	/*
	 * Setup an empty hash table mapping constraint definition strings to
	 * ConstraintEntry structures.
	 */
	context = AllocSetContextCreate(CurrentMemoryContext,
									"Constraint Exchange Context",
									MIN_XCHG_CONTEXT_SIZE,
									INIT_XCHG_BLOCK_SIZE,
									MAX_XCHG_BLOCK_SIZE);

	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(char *);
	hash_ctl.entrysize = sizeof(ConstraintEntry);
	hash_ctl.hash = key_string_hash;
	hash_ctl.match = key_string_compare;
	hash_ctl.keycopy = key_string_copy;
	hash_ctl.hcxt = context;
	hash_tbl = hash_create("Constraint Exchange Map",
						   DEFAULT_CONSTRAINT_ESTIMATE,
						   &hash_ctl,
						   HASH_ELEM | HASH_FUNCTION |
						   HASH_COMPARE | HASH_KEYCOPY |
						   HASH_CONTEXT);

	/* Open pg_constraint here for use in the subroutine and below. */
	pgcon = heap_open(ConstraintRelationId, AccessShareLock);

	/*
	 * We need attribute numbers normalized to the partitioned table. Note
	 * that these maps are inverse to the usual table-to-part maps.
	 */
	oldcontext = MemoryContextSwitchTo(context);
	map_part_attrs(part, table, &p2t, TRUE);
	map_part_attrs(cand, table, &c2t, TRUE);
	MemoryContextSwitchTo(oldcontext);

	/* Find and record constraints on the players. */
	record_constraints(pgcon, context, hash_tbl, table, PART_TABLE);
	record_constraints(pgcon, context, hash_tbl, part, PART_PART);
	record_constraints(pgcon, context, hash_tbl, cand, PART_CAND);
	hash_freeze(hash_tbl);

	/*
	 * Each entry in the hash table represents a single logically equivalent
	 * constraint which may appear zero or more times (under different names)
	 * on each of the three involved relations.  By construction, it will
	 * appear on at least one list.
	 *
	 * For each hash table entry:
	 */
	hash_seq_init(&hash_seq, hash_tbl);
	while ((entry = hash_seq_search(&hash_seq)))
	{
		if (entry->indexed_cons)
		{
			/*
			 * INDEX BACKED CONSTRAINTS
			 *
			 * Index backed constraints may inherit from a parent partition,
			 * so they need to have the pg_inherits and pg_depend relationships
			 * swapped.
			 *
			 * Unlike other constraints, because they are backed by indexes,
			 * they must have a unique name, so the constraint name on the partition must be
			 * swapped with the constraint name on the new table.
			 */
			List	   *missing = NIL;
			List	   *extra = NIL;

			if (list_length(entry->table_cons) > 0)
			{
				/*
				 * Find any constraints that are missing from the partition that
				 * exist on the parent. The missing constraints will need to be
				 * added to the partition before we will allow a partition
				 * exchange.
				 */
				constraint_diffs(entry->table_cons, entry->part_cons, true, &missing, &extra);
				missing_inherited_constraints = list_concat(missing_inherited_constraints, missing);
				excess_constraints = list_concat(excess_constraints, extra);
			}

			missing = NIL;
			extra = NIL;

			/*
			 * Compare indexes between the existing and the incoming partition.
			 */
			constraint_diffs(entry->part_cons, entry->cand_cons, false, &missing, &extra);
			missing_constraints = list_concat(missing_constraints, missing);
			excess_constraints = list_concat(excess_constraints, extra);

			ListCell *lc;

			/*
			 * Pop off any constraints that were added to either missing
			 * or extra. What remains are the matching constraint pairs
			 * for name exchange.
			 */
			foreach(lc, missing)
			{
				list_delete_ptr(entry->table_cons, lfirst(lc));
			}

			foreach(lc, extra)
			{
				list_delete_ptr(entry->cand_cons, lfirst(lc));
			}

			if (list_length(entry->part_cons) > 0 && list_length(entry->cand_cons) > 0)
				indexed_constraints = lappend(indexed_constraints, entry);
		}
		else if (list_length(entry->table_cons) > 0)
		{
			/*
			 * REGULAR CONSTRAINT
			 *
			 * Constraints on the whole partitioned table are regular (in the
			 * sense that they do not enforce partitioning rules and
			 * corresponding constraints must occur on every part).
			 */

			List	   *missing = NIL;
			List	   *extra = NIL;

			if (list_length(entry->part_cons) == 0)
			{
				/*
				 * The regular constraint is missing from the existing part,
				 * so there is a database anomaly.  Warn rather than issuing
				 * an error, because this may be an attempt to use EXCHANGE to
				 * correct the problem.  There may be multiple constraints
				 * with different names, but report only the first name since
				 * the constraint expression itself is all that matters.
				 */
				tuple = linitial(entry->table_cons);
				con = (Form_pg_constraint) GETSTRUCT(tuple);

				ereport(WARNING,
						(errcode(ERRCODE_WARNING),
						 errmsg("ignoring inconsistency: \"%s\" has no constraint corresponding to \"%s\" on \"%s\"",
								RelationGetRelationName(part),
								NameStr(con->conname),
								RelationGetRelationName(table))));
			}

			/*
			 * The regular constraint should ultimately appear on the
			 * candidate part the same number of times and with the same name
			 * as it appears on the partitioned table. The call to
			 * constraint_diff will find matching names and we'll be left with
			 * occurrences of the constraint that must be added to the
			 * candidate (missing) and occurrences that must be dropped from
			 * the candidate (extra).
			 */
			constraint_diffs(entry->table_cons, entry->cand_cons, false, &missing, &extra);
			missing_constraints = list_concat(missing_constraints, missing);
			excess_constraints = list_concat(excess_constraints, extra);
		}
		else if (list_length(entry->part_cons) > 0) /* and none on whole */
		{
			/*
			 * PARTITION CONSTRAINT
			 *
			 * Constraints on the part and not the whole must guard a
			 * partition rule, so they must be CHECK constraints on
			 * partitioning columns. They are managed internally, so there
			 * must be only one of them. (Though a part will have a partition
			 * constraint for each partition level, a given constraint should
			 * appear only once per part.)
			 *
			 * They should either already occur on the candidate or be added.
			 * Partition constraint names are not carefully managed so they
			 * shouldn't be regarded as meaningful.
			 *
			 * Since we use the partition constraint of the part to check or
			 * construct the partition constraint of the candidate, we insist
			 * it is in good working order, and issue an error, if not.
			 */
			int			n = list_length(entry->part_cons);

			if (n > 1)
			{
				elog(ERROR,
					 "multiple partition constraints (same key) on \"%s\"",
					 RelationGetRelationName(part));
			}

			/*
			 * Get the model partition constraint.
			 */
			tuple = linitial(entry->part_cons);
			con = (Form_pg_constraint) GETSTRUCT(tuple);

			/*
			 * Check it, though this is cursory in that we don't check that
			 * the right attributes are involved and that the semantics are
			 * right.
			 */
			if (con->contype != CONSTRAINT_CHECK)
			{
				elog(ERROR,
					 "invalid partition constraint on \"%s\"",
					 RelationGetRelationName(part));
			}

			n = list_length(entry->cand_cons);

			if (n == 0)
			{
				/*
				 * The partition constraint is missing from the candidate and
				 * must be added.
				 */
				missing_part_constraints = lappend(missing_part_constraints,
												   (HeapTuple) linitial(entry->part_cons));
			}
			else if (n == 1)
			{
				/*
				 * One instance of the partition constraint exists on the
				 * candidate, so let's not worry about name drift.  All is
				 * well.
				 */
			}
			else
			{
				/*
				 * Several instances of the partition constraint exist on the
				 * candidate. If one has a matching name, prefer it. Else,
				 * just chose the first (arbitrary).
				 */
				List	   *missing = NIL;
				List	   *extra = NIL;

				constraint_diffs(entry->part_cons, entry->cand_cons, false, &missing, &extra);

				if (list_length(missing) == 0)
				{
					excess_constraints = list_concat(excess_constraints, extra);
				}
				else			/* missing */
				{
					ListCell   *lc;
					bool		skip = TRUE;

					foreach(lc, entry->cand_cons)
					{
						HeapTuple	tuple = (HeapTuple) lfirst(lc);

						if (skip)
						{
							skip = FALSE;
						}
						else
						{
							excess_constraints = lappend(excess_constraints, tuple);
						}
					}
				}
			}
		}
		else if (list_length(entry->cand_cons) > 0) /* and none on whole or
													 * part */
		{
			/*
			 * MAVERICK CONSTRAINT
			 *
			 * Constraints on only the candidate are extra and must be dropped
			 * before the candidate can replace the part.
			 */
			excess_constraints = list_concat(excess_constraints,
											 entry->cand_cons);
		}
		else					/* Defensive: Can't happen that no constraints
								 * are set. */
		{
			elog(ERROR, "constraint hash table inconsistent");
		}
	}


	if (excess_constraints)
	{
		/*
		 * Disallow excess constraints.  We could drop them automatically, but
		 * they may carry semantic information about the candidate that is
		 * important to the user, so make the user decide whether to drop
		 * them.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INTEGRITY_CONSTRAINT_VIOLATION),
				 errmsg("invalid constraint(s) found on \"%s\": %s",
						RelationGetRelationName(cand),
						constraint_names(excess_constraints)),
				 errhint("Drop the invalid constraints and retry.")));
	}

	if (missing_inherited_constraints)
	{
		/*
		 * If the parent table has an extra constraint that is not
		 * inherited by the existing partition, then don't allow the echange
		 * to occur until that has been corrected.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INTEGRITY_CONSTRAINT_VIOLATION),
					errmsg("Inherited constraints(s) found on \"%s\" that do not exist on \"%s\": %s",
						RelationGetRelationName(table),
						RelationGetRelationName(part),
						constraint_names(missing_inherited_constraints)),
					errhint("Attach missing constraints and retry.")));
	}

	if (missing_part_constraints)
	{
		ListCell   *lc;

		foreach(lc, missing_part_constraints)
		{
			HeapTuple	missing_part_constraint = (HeapTuple) lfirst(lc);

			/*
			 * We need a constraint like the missing one for the part, but
			 * translated for the candidate.
			 */
			TupleConversionMap *map;
			struct NewConstraint *nc;
			Form_pg_constraint mcon = (Form_pg_constraint) GETSTRUCT(missing_part_constraint);

			if (mcon->contype != CONSTRAINT_CHECK)
				elog(ERROR, "Invalid partition constraint, not CHECK type");

			map_part_attrs(part, cand, &map, TRUE);
			nc = constraint_apply_mapped(missing_part_constraint, map, cand,
										 validate, pgcon);
			if (nc)
				validation_list = lappend(validation_list, nc);

			delta_checks++;
		}
	}

	if (missing_constraints)
	{
		/*
		 * We need constraints like the missing ones for the whole, but
		 * translated for the candidate.
		 */
		TupleConversionMap *map;
		struct NewConstraint *nc;
		ListCell   *lc;

		map_part_attrs(table, cand, &map, TRUE);
		foreach(lc, missing_constraints)
		{
			HeapTuple	tuple = (HeapTuple) lfirst(lc);
			Form_pg_constraint mcon = (Form_pg_constraint) GETSTRUCT(tuple);

			nc = constraint_apply_mapped(tuple, map, cand,
										 validate, pgcon);
			if (nc)
				validation_list = lappend(validation_list, nc);

			if (mcon->contype == CONSTRAINT_CHECK)
				delta_checks++;
		}
	}

	if (indexed_constraints)
	{
		ListCell   *lcentry;

		foreach(lcentry, indexed_constraints)
		{
			ListCell   *lcpart;
			ListCell   *lccand;

			entry = (ConstraintEntry *) lfirst(lcentry);

			forboth(lcpart, entry->part_cons, lccand, entry->cand_cons)
			{
				HeapTuple	parttuple = (HeapTuple) lfirst(lcpart);
				Form_pg_constraint part_constraint = (Form_pg_constraint) GETSTRUCT(parttuple);

				HeapTuple	candtuple = (HeapTuple) lfirst(lccand);
				Form_pg_constraint cand_constraint = (Form_pg_constraint) GETSTRUCT(candtuple);

				exchange_constraint_names(part, cand, part_constraint->conname, cand_constraint->conname);
			}
		}
	}


	if (delta_checks)
	{
		SetRelationNumChecks(cand, cand->rd_rel->relchecks + delta_checks);
	}

	hash_destroy(hash_tbl);
	MemoryContextDelete(context);
	heap_close(pgcon, AccessShareLock);

	return validation_list;
}

static void
exchange_constraint_names(Relation part, Relation cand, NameData part_name, NameData cand_name)
{
	char		temp_name[NAMEDATALEN];

	elog(DEBUG1, "Exchanging names for %s and %s", NameStr(part_name),
		NameStr(cand_name));

	snprintf(temp_name, NAMEDATALEN, "pg_temp_exchange_%u_%i", RelationGetRelid(part), MyBackendId);

	/*
	 * Since index names are unique within a namespace, assign a constraint a
	 * temporary name to help with the exchange.
	 */
	rename_constraint(part, NameStr(part_name), temp_name);
	rename_constraint(cand, NameStr(cand_name), NameStr(part_name));
	rename_constraint(part, temp_name, NameStr(cand_name));
}

static void
rename_constraint(Relation rel, char *old_name, char *new_name)
{
	RenameStmt renamestmt;

	renamestmt.renameType = OBJECT_TABCONSTRAINT;
	renamestmt.relationType = OBJECT_TABLE;
	renamestmt.relation = makeRangeVar(get_namespace_name(rel->rd_rel->relnamespace),
									   RelationGetRelationName(rel), -1);
	renamestmt.subname = old_name;
	renamestmt.newname = new_name;
	renamestmt.behavior = DROP_RESTRICT;

	RenameConstraint(&renamestmt);

	CommandCounterIncrement();
}
/*
 * Return a string of comma-delimited names of the constraints in the
 * argument list of pg_constraint tuples.  This is primarily for use
 * in messages.
 */
static char *
constraint_names(List *cons)
{
	ListCell   *lc;
	StringInfoData str;
	char	   *p;

	initStringInfo(&str);

	p = "";
	foreach(lc, cons)
	{
		HeapTuple	tuple = lfirst(lc);
		Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(tuple);

		appendStringInfo(&str, "%s\"%s\"", p, NameStr(con->conname));
		p = ", ";
	}

	return str.data;
}


/*
 * Identify constraints in the first list that don't correspond to
 * constraints in the second (missing) and vice versa (extra). Assume
 * that the constraints all match semantically, i.e, their expressions
 * are equivalent. (There are no checks for this.) Also assume there
 * are no duplicate constraint names in either argument list. (This
 * isn't checked, but is asserted.
 *
 * There are two checking modes.  One matches by name, one prefers
 * matching names, but accepts mismatches based on the assumed
 * semantic equivlence.
 *
 * Matching by name (which applies to regular constraints on the whole
 * table) may have missing and/or extra constraints.
 *
 * Matching by expression (which applies to partition constraints)
 * can yield only one or the other of extra or missing constraints.
 * The cleverness is that we match by name to avoid choosing as the
 * missing or extra constraints ones that match by name with items in
 * the other list.
 */
static void
constraint_diffs(List *cons_a, List *cons_b, bool match_inheritance, List **missing, List **extra)
{
	ListCell   *cell_a,
			   *cell_b;
	Index		pos_a,
				pos_b;
	int		   *match_a,
			   *match_b;
	int			n;

	int			len_a = list_length(cons_a);
	int			len_b = list_length(cons_b);

	Assert(missing != NULL);
	Assert(extra != NULL);

	if (len_a == 0)
	{
		*extra = list_copy(cons_b);
		*missing = NIL;
		return;
	}

	if (len_b == 0)
	{
		*extra = NIL;
		*missing = list_copy(cons_a);
		return;
	}

	match_a = (int *) palloc(len_a * sizeof(int));
	for (pos_a = 0; pos_a < len_a; pos_a++)
		match_a[pos_a] = -1;

	match_b = (int *) palloc(len_b * sizeof(int));
	for (pos_b = 0; pos_b < len_b; pos_b++)
		match_b[pos_b] = -1;

	if(match_inheritance)
	{
		pos_b = 0;
		foreach(cell_b, cons_b)
		{
			HeapTuple tuple_b = (HeapTuple) lfirst(cell_b);
			Form_pg_constraint b = (Form_pg_constraint) GETSTRUCT(tuple_b);
			pos_a = 0;

			foreach(cell_a, cons_a)
			{
				HeapTuple tuple_a = lfirst(cell_a);
				Form_pg_constraint a = (Form_pg_constraint) GETSTRUCT(tuple_a);

				List *inheritors = find_inheritance_children(a->conindid, NoLock);

				if (list_member_oid(inheritors, b->conindid))
				{
					/* No duplicate inheritors on either list. */
					Assert(match_a[pos_a] == -1 && match_b[pos_b] == -1);

					match_b[pos_b] = pos_a;
					match_a[pos_a] = pos_b;
					break;
				}
				pos_a++;
			}
			pos_b++;
		}
	}

	*missing = NIL;
	*extra = NIL;

	n = len_a - len_b;
	if (n > 0 || match_inheritance)
	{
		pos_a = 0;
		foreach(cell_a, cons_a)
		{
			if (match_a[pos_a] == -1)
				*missing = lappend(*missing, lfirst(cell_a));
			pos_a++;
			n--;
			if (n <= 0 && !match_inheritance)
				break;
		}
	}

	n = len_b - len_a;
	if (n > 0 || match_inheritance)
	{
		pos_b = 0;
		foreach(cell_b, cons_b)
		{
			if (match_b[pos_b] == -1)
				*extra = lappend(*extra, lfirst(cell_b));
			pos_b++;
			n--;
			if (n <= 0 && !match_inheritance)
				break;
		}
	}

	pfree(match_a);
	pfree(match_b);
}

/*
 * Translate internal representation to catalog partition type indication
 * ('r' or 'l').
 */
static char
parttype_to_char(PartitionByType type)
{
	char		c;

	switch (type)
	{
		case PARTTYP_RANGE:
			c = 'r';
			break;
		case PARTTYP_LIST:
			c = 'l';
			break;
		default:
			c = 0;				/* quieten compiler */
			elog(ERROR, "unknown partitioning type %i", type);

	}
	return c;
}

/*
 * Translate a catalog partition type indication ('r' or 'l') to the
 * internal representation.
 */
PartitionByType
char_to_parttype(char c)
{
	PartitionByType pt = PARTTYP_RANGE; /* just to shut GCC up */

	switch (c)
	{
		case 'r':
			pt = PARTTYP_RANGE;
			break;
		case 'l':				/* list */
			pt = PARTTYP_LIST;
			break;
		default:
			elog(ERROR, "unrecognized partitioning kind '%c'",
				 c);
			Assert(false);
			break;
	}							/* end switch */

	return pt;
}

/*
 * Add metadata for a partition level.
 */
static void
add_partition(Partition *part)
{
	Datum		values[Natts_pg_partition];
	bool		isnull[Natts_pg_partition];
	Relation	partrel;
	HeapTuple	tup;
	oidvector  *opclass;
	int2vector *attnums;

	MemSet(isnull, 0, sizeof(bool) * Natts_pg_partition);

	values[Anum_pg_partition_parrelid - 1] = ObjectIdGetDatum(part->parrelid);
	values[Anum_pg_partition_parkind - 1] = CharGetDatum(part->parkind);
	values[Anum_pg_partition_parlevel - 1] = Int16GetDatum(part->parlevel);
	values[Anum_pg_partition_paristemplate - 1] =
		BoolGetDatum(part->paristemplate);
	values[Anum_pg_partition_parnatts - 1] = Int16GetDatum(part->parnatts);

	attnums = buildint2vector(part->paratts, part->parnatts);
	values[Anum_pg_partition_paratts - 1] = PointerGetDatum(attnums);

	opclass = buildoidvector(part->parclass, part->parnatts);
	values[Anum_pg_partition_parclass - 1] = PointerGetDatum(opclass);

	partrel = heap_open(PartitionRelationId, RowExclusiveLock);

	tup = heap_form_tuple(RelationGetDescr(partrel), values, isnull);

	/* Insert tuple into the relation */
	part->partid = simple_heap_insert(partrel, tup);
	CatalogUpdateIndexes(partrel, tup);

	heap_close(partrel, NoLock);
}

/*
 * Add a partition rule. A partition rule represents a discrete partition
 * child.
 */
static void
add_partition_rule(PartitionRule *rule)
{
	Datum		values[Natts_pg_partition_rule];
	bool		isnull[Natts_pg_partition_rule];
	Relation	rulerel;
	HeapTuple	tup;
	NameData	name;

	MemSet(isnull, 0, sizeof(bool) * Natts_pg_partition_rule);

	values[Anum_pg_partition_rule_paroid - 1] = ObjectIdGetDatum(rule->paroid);
	values[Anum_pg_partition_rule_parchildrelid - 1] =
		ObjectIdGetDatum(rule->parchildrelid);
	values[Anum_pg_partition_rule_parparentrule - 1] =
		ObjectIdGetDatum(rule->parparentoid);

	name.data[0] = '\0';
	namestrcpy(&name, rule->parname);
	values[Anum_pg_partition_rule_parname - 1] = NameGetDatum(&name);

	values[Anum_pg_partition_rule_parisdefault - 1] =
		BoolGetDatum(rule->parisdefault);
	values[Anum_pg_partition_rule_parruleord - 1] =
		Int16GetDatum(rule->parruleord);
	values[Anum_pg_partition_rule_parrangestartincl - 1] =
		BoolGetDatum(rule->parrangestartincl);
	values[Anum_pg_partition_rule_parrangeendincl - 1] =
		BoolGetDatum(rule->parrangeendincl);

	values[Anum_pg_partition_rule_parrangestart - 1] =
		CStringGetTextDatum(nodeToString(rule->parrangestart));
	values[Anum_pg_partition_rule_parrangeend - 1] =
			CStringGetTextDatum(nodeToString(rule->parrangeend));
	values[Anum_pg_partition_rule_parrangeevery - 1] =
			CStringGetTextDatum(nodeToString(rule->parrangeevery));
	values[Anum_pg_partition_rule_parlistvalues - 1] =
			CStringGetTextDatum(nodeToString(rule->parlistvalues));

	/*
	 * If rule->parreloptions is NIL, the function `transformRelOptions`
	 * will return the first argument.
	 */
	values[Anum_pg_partition_rule_parreloptions - 1] =
		transformRelOptions((Datum) 0, rule->parreloptions, NULL, NULL, true, false);
	/*
	 * There some cases that transformRelOptions in the above will return a NULL.
	 * Add a check here.
	 */
	if (!values[Anum_pg_partition_rule_parreloptions - 1])
		isnull[Anum_pg_partition_rule_parreloptions - 1] = true;

	values[Anum_pg_partition_rule_partemplatespace - 1] =
		ObjectIdGetDatum(rule->partemplatespaceId);

	rulerel = heap_open(PartitionRuleRelationId, RowExclusiveLock);

	tup = heap_form_tuple(RelationGetDescr(rulerel), values, isnull);

	/* Insert tuple into the relation */
	rule->parruleid = simple_heap_insert(rulerel, tup);
	CatalogUpdateIndexes(rulerel, tup);

	heap_close(rulerel, NoLock);
}

/*
 * Oid of the row of pg_partition corresponding to the given relation and level.
 */
static Oid
get_part_oid(Oid rootrelid, int16 parlevel, bool istemplate)
{
	Relation	partrel;
	ScanKeyData scankey[3];
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			paroid;

	/*
	 * select oid from pg_partition where parrelid = :rootrelid and parlevel =
	 * :parlevel and paristemplate = :istemplate;
	 */

	/*
	 * pg_partition and  pg_partition_rule are populated only on the entry
	 * database, so our result is only meaningful there.
	 */
	Insist(IS_QUERY_DISPATCHER());

	partrel = heap_open(PartitionRelationId, AccessShareLock);
	ScanKeyInit(&scankey[0], Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(rootrelid));
	ScanKeyInit(&scankey[1], Anum_pg_partition_parlevel,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(parlevel));
	ScanKeyInit(&scankey[2], Anum_pg_partition_paristemplate,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(istemplate));

	/* XXX XXX: SnapshotSelf */
	sscan = systable_beginscan(partrel, PartitionParrelidParlevelParistemplateIndexId, true,
							   SnapshotSelf, 3, scankey);

	tuple = systable_getnext(sscan);

	if (tuple)
		paroid = HeapTupleGetOid(tuple);
	else
		paroid = InvalidOid;

	systable_endscan(sscan);
	heap_close(partrel, NoLock);

	return paroid;
}

/*
 * delete the template for a partition
 */
int
del_part_template(Oid rootrelid, int16 parlevel, Oid parent)
{
	bool		istemplate = true;
	Oid			paroid = InvalidOid;
	ItemPointerData tid;
	ScanKeyData scankey[3];
	Relation	part_rel;
	Relation	part_rule_rel;
	SysScanDesc sscan;
	HeapTuple	tuple;

	part_rel = heap_open(PartitionRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey[0], Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(rootrelid));
	ScanKeyInit(&scankey[1], Anum_pg_partition_parlevel,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(parlevel));
	ScanKeyInit(&scankey[2], Anum_pg_partition_paristemplate,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(istemplate));

	sscan = systable_beginscan(part_rel, PartitionParrelidParlevelParistemplateIndexId, true,
							   NULL, 3, scankey);

	tuple = systable_getnext(sscan);
	if (tuple == NULL)
	{
		/* not found */
		systable_endscan(sscan);
		heap_close(part_rel, RowExclusiveLock);
		return 0;
	}

	paroid = HeapTupleGetOid(tuple);
	tid = tuple->t_self;

	/* should only be one matching template per level */
	if (systable_getnext(sscan))
	{
		systable_endscan(sscan);
		heap_close(part_rel, RowExclusiveLock);
		return 2;
	}

	simple_heap_delete(part_rel, &tid);

	systable_endscan(sscan);

	/* Also delete pg_partition_rule entries */

	part_rule_rel = heap_open(PartitionRuleRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey[0], Anum_pg_partition_rule_paroid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(paroid));
	ScanKeyInit(&scankey[1], Anum_pg_partition_rule_parparentrule,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(parent));
	sscan = systable_beginscan(part_rule_rel, PartitionRuleParoidParparentruleParruleordIndexId, true,
							   NULL, 2, scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		simple_heap_delete(part_rule_rel, &tuple->t_self);
	}

	systable_endscan(sscan);

	heap_close(part_rule_rel, RowExclusiveLock);
	heap_close(part_rel, RowExclusiveLock);

	/* make visible */
	CommandCounterIncrement();

	return 1;
}								/* end del_part_template */


/*
 * add_part_to_catalog() - add a partition to the catalog
 *
 * NOTE: If bTemplate_Only = false, add both actual partitions and the
 * template definitions (if specified).  However, if bTemplate_Only =
 * true, then only treat the partition spec as a template.
 */
void
add_part_to_catalog(Oid relid, PartitionBy *pby,
					bool bTemplate_Only /* = false */ )
{
	char		pt = parttype_to_char(pby->partType);
	ListCell   *lc;
	PartitionSpec *spec;
	Oid			paroid = InvalidOid;
	Oid			rootrelid = InvalidOid;
	Relation	rel;
	Oid			parttemplid = InvalidOid;
	bool		add_temp = bTemplate_Only;	/* normally false */

	spec = (PartitionSpec *) pby->partSpec;

	/* only create partition catalog entries on the master */
	if (Gp_role == GP_ROLE_EXECUTE)
		return;

	/*
	 * Get the partitioned table relid.
	 */
	rootrelid = RangeVarGetRelid(pby->parentRel, NoLock, false);
	paroid = get_part_oid(rootrelid, pby->partDepth,
						  bTemplate_Only /* = false */ );

	/* create a partition for this level, if one doesn't exist */
	if (!OidIsValid(paroid))
	{
		AttrNumber *attnums;
		Oid		   *parclass;
		Partition  *part = makeNode(Partition);
		int			i = 0;

		part->parrelid = rootrelid;
		part->parkind = pt;
		part->parlevel = pby->partDepth;

		if (pby->partSpec)
			part->paristemplate = ((PartitionSpec *) pby->partSpec)->istemplate;
		else
			part->paristemplate = false;

		part->parnatts = list_length(pby->keys);

		attnums = palloc(list_length(pby->keys) * sizeof(AttrNumber));

		foreach(lc, pby->keys)
		{
			int			colnum = lfirst_int(lc);

			attnums[i++] = colnum;
		}

		part->paratts = attnums;

		parclass = palloc(list_length(pby->keys) * sizeof(Oid));

		i = 0;
		foreach(lc, pby->keyopclass)
		{
			Oid			opclass = lfirst_oid(lc);

			parclass[i++] = opclass;
		}
		part->parclass = parclass;

		add_partition(part);

		/*
		 * If we added a template, we treat that as a 'virtual' entry and then
		 * add a modifiable entry, which is not a template.
		 */
		if (part->paristemplate)
		{
			add_temp = true;
			parttemplid = part->partid;

			if (spec && spec->enc_clauses)
			{
				add_template_encoding_clauses(relid, parttemplid,
											  spec->enc_clauses);
			}

			/* if only building a template, don't add "real" entries */
			if (!bTemplate_Only)
			{
				part->paristemplate = false;
				add_partition(part);

			}
		}
		paroid = part->partid;
	}
	else
		/* oid of the template accompanying the real partition */
		parttemplid = get_part_oid(rootrelid, pby->partDepth, true);

	/* create partition rule */
	if (spec)
	{
		Node	   *listvalues = NULL;
		Node	   *rangestart = NULL;
		Node	   *rangeend = NULL;
		Node	   *rangeevery = NULL;
		bool		rangestartinc = false;
		bool		rangeendinc = false;
		int16		parruleord = 0;
		PartitionRule *rule = makeNode(PartitionRule);
		PartitionElem *el;
		char	   *parname = NULL;
		Oid			parentoid = InvalidOid;

		Assert(list_length(spec->partElem) == 1);

		el = linitial(spec->partElem);

		parruleord = el->partno;

		if (el->partName)
			parname = el->partName;

		switch (pby->partType)
		{
			case PARTTYP_LIST:
				{
					PartitionValuesSpec *vspec =
					(PartitionValuesSpec *) el->boundSpec;

					/* might be NULL if this is a default spec */
					if (vspec)
						listvalues = (Node *) vspec->partValues;
				}
				break;
			case PARTTYP_RANGE:
				{
					PartitionBoundSpec *bspec =
					(PartitionBoundSpec *) el->boundSpec;
					PartitionRangeItem *ri;

					/* remember, could be a default clause */
					if (bspec)
					{
						Assert(IsA(bspec, PartitionBoundSpec));
						ri = (PartitionRangeItem *) bspec->partStart;
						if (ri)
						{
							Assert(ri->partedge == PART_EDGE_INCLUSIVE ||
								   ri->partedge == PART_EDGE_EXCLUSIVE);

							rangestartinc = ri->partedge == PART_EDGE_INCLUSIVE;
							rangestart = (Node *) ri->partRangeVal;
						}

						ri = (PartitionRangeItem *) bspec->partEnd;
						if (ri)
						{
							Assert(ri->partedge == PART_EDGE_INCLUSIVE ||
								   ri->partedge == PART_EDGE_EXCLUSIVE);

							rangeendinc = ri->partedge == PART_EDGE_INCLUSIVE;
							rangeend = (Node *) ri->partRangeVal;
						}

						if (bspec->partEvery)
						{
							ri = (PartitionRangeItem *) bspec->partEvery;
							rangeevery = (Node *) ri->partRangeVal;
						}
						else
							rangeevery = NULL;
					}
				}
				break;
			default:
				elog(ERROR, "unknown partitioning type %i", pby->partType);
				break;
		}

		/* Find our parent */
		if (!bTemplate_Only && (pby->partDepth > 0))
		{
			Oid			inhoid;
			ScanKeyData scankey;
			SysScanDesc sscan;
			HeapTuple	tuple;

			rel = heap_open(InheritsRelationId, AccessShareLock);

			/* SELECT inhparent FROM pg_inherits WHERE inhrelid = :1 */
			ScanKeyInit(&scankey, Anum_pg_inherits_inhrelid,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(relid));
			/* XXX XXX: SnapshotAny */
			sscan = systable_beginscan(rel, InheritsRelidSeqnoIndexId, true,
									   SnapshotAny, 1, &scankey);
			tuple = systable_getnext(sscan);
			if (!tuple)
				elog(ERROR, "could not find pg_inherits row for rel %u", relid);

			inhoid = ((Form_pg_inherits) GETSTRUCT(tuple))->inhparent;

			systable_endscan(sscan);
			heap_close(rel, NoLock);

			rel = heap_open(PartitionRuleRelationId, AccessShareLock);

			ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(inhoid));

			/* XXX XXX: SnapshotAny */
			sscan = systable_beginscan(rel, PartitionRuleParchildrelidIndexId, true,
									   SnapshotAny, 1, &scankey);
			tuple = systable_getnext(sscan);
			if (!tuple)
				elog(ERROR, "could not find pg_partition_rule row with parchildrelid %u", relid);
			parentoid = HeapTupleGetOid(tuple);

			systable_endscan(sscan);
			heap_close(rel, NoLock);
		}
		else
			add_temp = true;

		/* we still might have to add template rules */
		if (!add_temp && OidIsValid(parttemplid))
		{
			ScanKeyData scankey[3];
			SysScanDesc sscan;
			Relation	partrulerel;
			HeapTuple	tuple;

			partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

			/*
			 * SELECT parchildrelid FROM pg_partition_rule WHERE paroid = :1
			 * AND parparentrule = :2 AND parruleord = :3
			 */
			ScanKeyInit(&scankey[0], Anum_pg_partition_rule_paroid,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(parttemplid));
			ScanKeyInit(&scankey[1], Anum_pg_partition_rule_parparentrule,
						BTEqualStrategyNumber, F_OIDEQ,
						ObjectIdGetDatum(InvalidOid));
			ScanKeyInit(&scankey[2], Anum_pg_partition_rule_parruleord,
						BTEqualStrategyNumber, F_INT2EQ,
						Int16GetDatum(parruleord));
			/* XXX XXX: SnapshotAny */
			sscan = systable_beginscan(partrulerel,
									   PartitionRuleParoidParparentruleParruleordIndexId, true,
									   SnapshotAny, 3, scankey);

			tuple = systable_getnext(sscan);
			if (tuple)
			{
				Assert(((Form_pg_partition_rule) GETSTRUCT(tuple))->parchildrelid == InvalidOid);
			}
			else
				add_temp = true;
			systable_endscan(sscan);
			heap_close(partrulerel, AccessShareLock);
		}

		rule->paroid = paroid;
		rule->parchildrelid = relid;
		rule->parparentoid = parentoid;
		rule->parisdefault = el->isDefault;
		rule->parname = parname;
		rule->parruleord = parruleord;
		rule->parrangestartincl = rangestartinc;
		rule->parrangestart = rangestart;
		rule->parrangeendincl = rangeendinc;
		rule->parrangeend = rangeend;
		rule->parrangeevery = rangeevery;
		rule->parlistvalues = (List *) listvalues;
		rule->partemplatespaceId = InvalidOid;	/* only valid for template */

		if (!bTemplate_Only)
			add_partition_rule(rule);

		if (OidIsValid(parttemplid) && add_temp)
		{
			rule->paroid = parttemplid;
			rule->parparentoid = InvalidOid;
			rule->parchildrelid = InvalidOid;

			if (el->storeAttr)
			{
				if (((AlterPartitionCmd *) el->storeAttr)->arg1)
					rule->parreloptions =
						(List *) ((AlterPartitionCmd *) el->storeAttr)->arg1;
				if (((AlterPartitionCmd *) el->storeAttr)->arg2)
				{
					Oid			tablespaceId;

					tablespaceId =
						get_settable_tablespace_oid(
													strVal(((AlterPartitionCmd *) el->storeAttr)->arg2));

					/* get_settable_tablespace_oid will error out for us */
					Assert(OidIsValid(tablespaceId));

					/* only valid for template definitions */
					rule->partemplatespaceId = tablespaceId;
				}
			}
			add_partition_rule(rule);
		}
	}

	/* allow subsequent callers to see our work */
	CommandCounterIncrement();
}								/* end add_part_to_catalog */

/*
 * parruleord_open_gap
 *
 * iterate over the specified set of range partitions (in *DESCENDING*
 * order) in pg_partition_rule and increment the parruleord in order
 * to open a "gap" for a new partition.  The stopkey is inclusive: to
 * insert a new partition at parruleord=5, set the stopkey to 5.  The
 * current partition at parruleord=5 (and all subsequent partitions)
 * are incremented by 1 to allow the insertion of the new partition.
 *
 * If closegap is set, parruleord values are decremented, to close a
 * gap in parruleord sequence.
 */
static void
parruleord_open_gap(Oid partid, int16 level, Oid parent, int16 ruleord,
					int16 stopkey, bool closegap)
{
	Relation	rel;
	Relation	irel;
	HeapTuple	tuple;
	ScanKeyData scankey[3];
	SysScanDesc sd;

	/*
	 * Ensure that ruleord argument did not wrap around due to int2
	 * typecast. We check if ruleord is less than 1 to also ensure that 0
	 * (default partition) is not given as an argument.
	 */
	if (ruleord < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("too many partitions, parruleord overflow")));

	/*---
	 * This is equivalent to:
	 * SELECT * FROM pg_partition_rule
	 * WHERE paroid = :1
	 * AND parparentrule = :2
	 * AND parruleord <= :3
	 * ORDER BY parruleord DESC
	 * FOR UPDATE
	 *
	 * Note that the attribute numbers below are attribute numbers
	 * of the index, rather than the table.
	 *---
	 */
	rel = heap_open(PartitionRuleRelationId, RowExclusiveLock);
	irel = index_open(PartitionRuleParoidParparentruleParruleordIndexId, RowExclusiveLock);

	ScanKeyInit(&scankey[0], Anum_pg_partition_rule_paroid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partid));
	ScanKeyInit(&scankey[1], Anum_pg_partition_rule_parparentrule,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(parent));
	ScanKeyInit(&scankey[2], Anum_pg_partition_rule_parruleord,
				BTLessEqualStrategyNumber, F_INT2LE,
				Int16GetDatum(ruleord));
	sd = systable_beginscan_ordered(rel, irel, NULL, 3, scankey);
	while (HeapTupleIsValid(tuple = systable_getnext_ordered(sd, BackwardScanDirection)))
	{
		Form_pg_partition_rule rule_desc;

		Insist(HeapTupleIsValid(tuple));

		tuple = heap_copytuple(tuple);

		rule_desc =
			(Form_pg_partition_rule) GETSTRUCT(tuple);

		if (rule_desc->parruleord < stopkey)
			break;

		closegap ? rule_desc->parruleord-- : rule_desc->parruleord++;

		simple_heap_update(rel, &tuple->t_self, tuple);
		CatalogUpdateIndexes(rel, tuple);

		heap_freetuple(tuple);
	}
	systable_endscan_ordered(sd);
	heap_close(irel, RowExclusiveLock);
	heap_close(rel, RowExclusiveLock);

}								/* end parruleord_open_gap */

/*
 * Build up a PartitionRule based on a tuple from pg_partition_rule
 * Exported for ruleutils.c
 */
PartitionRule *
ruleMakePartitionRule(HeapTuple tuple)
{
	Form_pg_partition_rule rule_desc =
		(Form_pg_partition_rule)GETSTRUCT(tuple);
	char *rule_str;
	Datum rule_datum;
	bool isnull;

	PartitionRule *rule;

	rule = makeNode(PartitionRule);

	rule->parruleid = HeapTupleGetOid(tuple);
	rule->paroid = rule_desc->paroid;
	rule->parchildrelid = rule_desc->parchildrelid;
	rule->parparentoid = rule_desc->parparentrule;
	rule->parisdefault = rule_desc->parisdefault;

	rule->parname = pstrdup(NameStr(rule_desc->parname));

	rule->parruleord = rule_desc->parruleord;
	rule->parrangestartincl = rule_desc->parrangestartincl;
	rule->parrangeendincl = rule_desc->parrangeendincl;

	/* start range */
	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_parrangestart,
								 &isnull);
	Assert(!isnull);
	rule_str = TextDatumGetCString(rule_datum);

	rule->parrangestart = stringToNode(rule_str);

	pfree(rule_str);

	/* end range */
	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_parrangeend,
								 &isnull);
	Assert(!isnull);
	rule_str = TextDatumGetCString(rule_datum);

	rule->parrangeend = stringToNode(rule_str);

	pfree(rule_str);

	/* every */
	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_parrangeevery,
								 &isnull);
	Assert(!isnull);
	rule_str = TextDatumGetCString(rule_datum);

	rule->parrangeevery = stringToNode(rule_str);

	/* list values */
	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_parlistvalues,
								 &isnull);
	Assert(!isnull);
	rule_str = TextDatumGetCString(rule_datum);

	rule->parlistvalues = stringToNode(rule_str);

	pfree(rule_str);

	if (rule->parlistvalues)
		Assert(IsA(rule->parlistvalues, List));

	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_parreloptions,
								 &isnull);

	if (isnull)
		rule->parreloptions = NIL;
	else
	{
		ArrayType  *array = DatumGetArrayTypeP(rule_datum);
		Datum	   *options;
		int			noptions;
		List	   *opts = NIL;
		int			i;

		/* XXX XXX: why not use untransformRelOptions ? */

		Assert(ARR_ELEMTYPE(array) == TEXTOID);

		deconstruct_array(array, TEXTOID, -1, false, 'i',
						  &options, NULL, &noptions);

		/* key/value pairs for storage clause */
		for (i = 0; i < noptions; i++)
		{
			char	   *n = TextDatumGetCString(options[i]);
			Value	   *v = NULL;
			char	   *s;

			s = strchr(n, '=');

			if (s)
			{
				*s = '\0';
				s++;
				if (*s)
					v = makeString(s);
			}

			opts = lappend(opts, makeDefElem(n, (Node *) v));
		}
		rule->parreloptions = opts;

	}

	rule_datum = SysCacheGetAttr(PARTRULEOID, tuple,
								 Anum_pg_partition_rule_partemplatespace,
								 &isnull);
	if (isnull)
		rule->partemplatespaceId = InvalidOid;
	else
		rule->partemplatespaceId = DatumGetObjectId(rule_datum);

	return rule;
}

/*
 * Construct a Partition node from a pg_partition tuple and its description.
 * Result is in the given memory context.
 */
Partition *
partMakePartition(HeapTuple tuple)
{
	oidvector  *oids;
	int2vector *atts;
	bool		isnull;
	Form_pg_partition partrow = (Form_pg_partition) GETSTRUCT(tuple);
	Partition  *p;

	p = makeNode(Partition);

	p->partid = HeapTupleGetOid(tuple);
	p->parrelid = partrow->parrelid;
	p->parkind = partrow->parkind;
	p->parlevel = partrow->parlevel;
	p->paristemplate = partrow->paristemplate;
	p->parnatts = partrow->parnatts;

	atts = (int2vector *) DatumGetPointer(SysCacheGetAttr(PARTOID, tuple,
														  Anum_pg_partition_paratts,
														  &isnull));
	Assert(!isnull);
	oids = (oidvector *) DatumGetPointer(SysCacheGetAttr(PARTOID, tuple,
														 Anum_pg_partition_parclass,
														 &isnull));
	Assert(!isnull);

	p->paratts = palloc(sizeof(int16) * p->parnatts);
	p->parclass = palloc(sizeof(Oid) * p->parnatts);

	memcpy(p->paratts, atts->values, sizeof(int16) * p->parnatts);
	memcpy(p->parclass, oids->values, sizeof(Oid) * p->parnatts);

	return p;
}

/*
 * Construct a PartitionNode-PartitionRule tree for the given part.
 * Recurs to construct branches.  Note that the PartitionRule (and,
 * hence, the Oid) of the given part itself is not included in the
 * result.
 *
 *    relid				--	pg_class.oid of the partitioned table
 *    level				--	partitioning level
 *    parent			--	pg_partition_rule.oid of the parent of
 *    inctemplate		--	should the tree include template rules?
 *    mcxt				--	memory context
 *    includesubparts	--	whether or not to include sub partitions
 */
PartitionNode *
get_parts(Oid relid, int16 level, Oid parent, bool inctemplate,
		  bool includesubparts)
{
	PartitionNode *pnode = NULL;
	HeapTuple	tuple;
	Relation	rel;
	List	   *rules = NIL;
	ListCell   *lc;
	int			num_rules;
	int			ruleno;
	ScanKeyData scankey[3];
	SysScanDesc sscan;

	/*
	 * Though pg_partition and  pg_partition_rule are populated only on the
	 * entry database, we accept calls from QEs running on a segment database,
	 * but always return NULL; so our result is only meaningful on the entry
	 * database.
	 */
	if (!IS_QUERY_DISPATCHER())
		return pnode;

	/*
	 * select oid as partid, * from pg_partition where parrelid = :relid and
	 * parlevel = :level and paristemplate = :inctemplate;
	 */
	rel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&scankey[0],
				Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&scankey[1],
				Anum_pg_partition_parlevel,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(level));
	ScanKeyInit(&scankey[2],
				Anum_pg_partition_paristemplate,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(inctemplate));
	sscan = systable_beginscan(rel, PartitionParrelidParlevelParistemplateIndexId, true,
							   NULL, 3, scankey);
	tuple = systable_getnext(sscan);
	if (HeapTupleIsValid(tuple))
	{
		pnode = makeNode(PartitionNode);
		pnode->part = partMakePartition(tuple);
	}

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);

	if (!pnode)
		return pnode;

	/*
	 * select * from pg_partition_rule where paroid = :pnode->part->partid and
	 * -- pg_partition.oid parparentrule = :parent;
	 */
	rel = heap_open(PartitionRuleRelationId, AccessShareLock);

	if (includesubparts)
	{
		ScanKeyInit(&scankey[0],
					Anum_pg_partition_rule_paroid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(pnode->part->partid));
		ScanKeyInit(&scankey[1],
					Anum_pg_partition_rule_parparentrule,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(parent));
		sscan = systable_beginscan(rel, PartitionRuleParoidParparentruleParruleordIndexId, true,
								   NULL, 2, scankey);
	}
	else
	{
		ScanKeyInit(&scankey[0],
					Anum_pg_partition_rule_paroid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(pnode->part->partid));
		sscan = systable_beginscan(rel, PartitionRuleParoidParparentruleParruleordIndexId, true,
								   NULL, 1, scankey);
	}

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		PartitionRule *rule;

		rule = ruleMakePartitionRule(tuple);
		if (includesubparts)
		{
			rule->children = get_parts(relid, level + 1, rule->parruleid,
									   inctemplate, true /* includesubparts */ );
		}

		if (rule->parisdefault)
			pnode->default_part = rule;
		else
		{
			rules = lappend(rules, rule);
		}
	}

	/*
	 * NOTE: this assert is valid, except for the case of splitting the very
	 * last partition of a table.  For that case, we must drop the last
	 * partition before re-adding the new pieces, which violates this
	 * invariant
	 */
	/* Assert(inctemplate || list_length(rules) || pnode->default_part); */

	num_rules = list_length(rules);
	pnode->rules = palloc(num_rules * sizeof(PartitionRule *));
	pnode->num_rules = num_rules;
	ruleno = 0;
	foreach(lc, rules)
	{
		PartitionRule *rule = (PartitionRule *) lfirst(lc);

		pnode->rules[ruleno++] = rule;
	}
	list_free(rules);

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);
	return pnode;
}

PartitionNode *
RelationBuildPartitionDesc(Relation rel, bool inctemplate)
{
	return RelationBuildPartitionDescByOid(RelationGetRelid(rel), inctemplate);
}

PartitionNode *
RelationBuildPartitionDescByOid(Oid relid, bool inctemplate)
{
	PartitionNode *n;

	n = get_parts(relid, 0, 0, inctemplate, true /* includesubparts */ );

	return n;
}

/*
 * Return a Bitmapset of the attribute numbers of a partitioned table
 * (not a part).  The attribute numbers refer to the root partition table.
 * Call only on the entry database.  Returns an empty set, if called on a
 * regular table or a part.
 *
 * Note: Reads the pg_partition catalog.  If you have a PartitionNode,
 * in hand, use get_partition_attrs and construct a Bitmapset from its
 * result instead.
 */
Bitmapset *
get_partition_key_bitmapset(Oid relid)
{
	Relation	rel;
	HeapTuple	tuple;
	TupleDesc	tupledesc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Bitmapset  *partition_key = NULL;

	/*
	 * Reject calls from QEs running on a segment database, since pg_partition
	 * and  pg_partition_rule are populated only on the entry database.
	 */
	Insist(IS_QUERY_DISPATCHER());

	/*
	 * select paratts from pg_partition where parrelid = :relid and not
	 * paristemplate;
	 */
	rel = heap_open(PartitionRelationId, AccessShareLock);
	tupledesc = RelationGetDescr(rel);

	ScanKeyInit(&scankey,
				Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	sscan = systable_beginscan(rel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		int			i;
		int16		natts;
		int2vector *atts;
		bool		isnull;
		Form_pg_partition partrow = (Form_pg_partition) GETSTRUCT(tuple);

		if (partrow->paristemplate)
			continue;			/* no interest in template parts */

		natts = partrow->parnatts;
		atts = (int2vector *) DatumGetPointer(
											  heap_getattr(tuple, Anum_pg_partition_paratts,
														   tupledesc, &isnull));
		Insist(!isnull);

		for (i = 0; i < natts; i++)
			partition_key = bms_add_member(partition_key, atts->values[i]);
	}

	systable_endscan(sscan);
	heap_close(rel, AccessShareLock);

	return partition_key;
}


/*
 * Return a list of partition attributes. Order is not guaranteed.
 * Caller must free the result.
 */
List *
get_partition_attrs(PartitionNode *pn)
{
	List	   *attrs = NIL;
	int			i;

	if (!pn)
		return NIL;

	for (i = 0; i < pn->part->parnatts; i++)
		attrs = lappend_int(attrs, pn->part->paratts[i]);

	/* We don't want duplicates, do just go down a single branch */
	if (pn->num_rules > 0)
	{
		PartitionRule *rule = pn->rules[0];

		return list_concat_unique_int(attrs, get_partition_attrs(rule->children));
	}
	else
		return attrs;
}

bool
partition_policies_equal(GpPolicy *p, PartitionNode *pn)
{
	if (!pn)
		return true;

	for (int i = 0; i < pn->num_rules; i++)
	{
		PartitionRule *rule = pn->rules[i];
		Relation	rel = heap_open(rule->parchildrelid, NoLock);

		if (p->nattrs != rel->rd_cdbpolicy->nattrs)
		{
			heap_close(rel, NoLock);
			return false;
		}
		else
		{
			if (p->attrs == 0)
				/* random policy, skip */
				;
			if (memcmp(p->attrs, rel->rd_cdbpolicy->attrs,
					   (sizeof(AttrNumber) * p->nattrs)))
			{
				heap_close(rel, NoLock);
				return false;
			}
		}
		if (!partition_policies_equal(p, rule->children))
		{
			heap_close(rel, NoLock);
			return false;
		}
		heap_close(rel, NoLock);
	}
	return true;
}

AttrNumber
max_partition_attr(PartitionNode *pn)
{
	AttrNumber	n = 0;
	List	   *l = get_partition_attrs(pn);

	if (l)
	{
		ListCell   *lc;

		foreach(lc, l)
		{
			AttrNumber	att = lfirst_int(lc);

			n = Max(att, n);
		}
		pfree(l);
	}
	return n;
}

int
num_partition_levels(PartitionNode *pn)
{
	PartitionNode *tmp;
	int			level = 0;

	tmp = pn;

	/* just descend one branch of the tree until we hit a leaf */
	while (tmp)
	{
		level++;
		if (tmp->num_rules > 0)
		{
			PartitionRule *rule = tmp->rules[0];

			tmp = rule->children;
		}
		else if (tmp->default_part)
		{
			PartitionRule *rule = tmp->default_part;

			tmp = rule->children;
		}
		else
			tmp = NULL;
	}

	return level;
}

/* Return the pg_class Oids of the relations representing parts of the
 * PartitionNode tree headed by the argument PartitionNode.
 */
List *
all_partition_relids(PartitionNode *pn)
{
	if (!pn)
		return NIL;
	else
	{
		List	   *out = NIL;

		for (int i = 0; i < pn->num_rules; i++)
		{
			PartitionRule *rule = pn->rules[i];

			Assert(OidIsValid(rule->parchildrelid));
			out = lappend_oid(out, rule->parchildrelid);

			out = list_concat(out, all_partition_relids(rule->children));
		}
		if (pn->default_part)
		{
			out = lappend_oid(out, pn->default_part->parchildrelid);
			out = list_concat(out,
							  all_partition_relids(pn->default_part->children));
		}
		return out;
	}
}

/*
 * getPartConstraintsContainsKeys
 *   Given an OID, returns a Node that represents the check constraints
 *   on the table having constraint keys from the given key list.
 *   If there are multiple constraints they are AND'd together.
 */
static Node *
getPartConstraintsContainsKeys(Oid partOid, Oid rootOid, List *partKey)
{
	ScanKeyData scankey;
	SysScanDesc sscan;
	Relation	conRel;
	HeapTuple	conTup;
	Node	   *conExpr;
	Node	   *result = NULL;
	Datum		conBinDatum;
	Datum		conKeyDatum;
	char	   *conBin;
	bool		conbinIsNull = false;
	bool		conKeyIsNull = false;
	TupleConversionMap *map;

	/* create the map needed for mapping attnums */
	Relation	rootRel = heap_open(rootOid, AccessShareLock);
	Relation	partRel = heap_open(partOid, AccessShareLock);

	map_part_attrs(partRel, rootRel, &map, false);

	heap_close(rootRel, AccessShareLock);
	heap_close(partRel, AccessShareLock);

	/* Fetch the pg_constraint row. */
	conRel = heap_open(ConstraintRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partOid));
	sscan = systable_beginscan(conRel, ConstraintRelidIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(conTup = systable_getnext(sscan)))
	{
		/*
		 * we defer the filter on contype to here in order to take advantage
		 * of the index on conrelid in the scan
		 */
		Form_pg_constraint conEntry = (Form_pg_constraint) GETSTRUCT(conTup);

		if (conEntry->contype != 'c')
		{
			continue;
		}
		/* Fetch the constraint expression in parsetree form */
		conBinDatum = heap_getattr(conTup, Anum_pg_constraint_conbin,
								   RelationGetDescr(conRel), &conbinIsNull);

		Assert(!conbinIsNull);
		/* map the attnums in constraint expression to root attnums */
		conBin = TextDatumGetCString(conBinDatum);
		conExpr = stringToNode(conBin);
		conExpr = attrMapExpr(map, conExpr);

		/* fetch the key associated with this constraint */
		conKeyDatum = heap_getattr(conTup, Anum_pg_constraint_conkey,
								   RelationGetDescr(conRel), &conKeyIsNull);
		Datum	   *dats = NULL;
		int			numKeys = 0;

		bool		found = false;

		/* extract key elements */
		deconstruct_array(DatumGetArrayTypeP(conKeyDatum), INT2OID, 2, true, 's', &dats, NULL, &numKeys);
		for (int i = 0; i < numKeys; i++)
		{
			int16		key_elem = DatumGetInt16(dats[i]);

			if (list_member_int(partKey, key_elem))
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			if (result)
				result = (Node *) make_andclause(list_make2(result, conExpr));
			else
				result = conExpr;
		}
	}

	if (map)
	{
		pfree(map);
	}
	systable_endscan(sscan);
	heap_close(conRel, AccessShareLock);

	return result;
}

/*
 * Create a hash table with both key and hash entry as a constraint Node*
 * Input:
 *	nEntries - estimated number of elements in the hash table
 * Outout:
 *	a pointer to the created hash table
 */
static HTAB *
createConstraintHashTable(unsigned int nEntries)
{
	HASHCTL		hash_ctl;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));

	hash_ctl.keysize = sizeof(Node **);
	hash_ctl.entrysize = sizeof(ConNodeEntry);
	hash_ctl.hash = constrNodeHash;
	hash_ctl.match = constrNodeMatch;

	return hash_create("ConstraintHashTable", nEntries, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE);
}

/*
 * Hash function for a constraint node
 * Input:
 *	keyPtr - pointer to hash key
 *	keysize - not used, hash function must have this signature
 * Output:
 *	result - hash value as an unsigned integer
 */
static uint32
constrNodeHash(const void *keyPtr, Size keysize)
{
	uint32		result = 0;
	Node	   *constr = *((Node **) keyPtr);
	int			con_len = 0;

	if (constr)
	{
		char	   *constr_bin = nodeToBinaryStringFast(constr, &con_len);

		Assert(con_len > 0);
		result = tag_hash(constr_bin, con_len);
		pfree(constr_bin);
	}

	return result;
}

/**
 * Match function for two constraint nodes.
 * Input:
 *	keyPtr1, keyPtr2 - pointers to two hash keys
 *	keysize - not used, hash function must have this signature
 * Output:
 *	0 if two hash keys match, 1 otherwise
 */
static int
constrNodeMatch(const void *keyPtr1, const void *keyPtr2, Size keysize)
{
	Node	   *left = *((Node **) keyPtr1);
	Node	   *right = *((Node **) keyPtr2);

	return equal(left, right) ? 0 : 1;
}

/*
 * Check if a partitioning hierarchy is uniform, i.e. for each partitioning level,
 * all the partition nodes should have the same number of children, AND the child nodes
 * at the same position w.r.t the subtree should have the same constraint on the partition
 * key of that level.
 * The time complexity of this check is linear to the number of nodes in the partitioning
 * hierarchy.
 */
bool
rel_partitioning_is_uniform(Oid rootOid)
{
	Assert(OidIsValid(rootOid));
	Assert(rel_is_partitioned(rootOid));

	bool		result = true;

	MemoryContext uniformityMemoryContext = AllocSetContextCreate(CurrentMemoryContext,
																  "PartitioningIsUniform",
																  ALLOCSET_DEFAULT_MINSIZE,
																  ALLOCSET_DEFAULT_INITSIZE,
																  ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContext callerMemoryContext = MemoryContextSwitchTo(uniformityMemoryContext);

	PartitionNode *pnRoot = RelationBuildPartitionDescByOid(rootOid, false /* inctemplate */ );
	List	   *queue = list_make1(pnRoot);

	while (result)
	{
		/*
		 * we process the partitioning tree level by level, each outer loop
		 * corresponds to one level
		 */
		int			size = list_length(queue);

		if (0 == size)
		{
			break;
		}

		/*
		 * Look ahead to get the number of children of the first partition
		 * node in this level. This allows us to initialize a hash table on
		 * the constraints which each partition node in this level will be
		 * compared to.
		 */
		PartitionNode *pn_ahead = (PartitionNode *) linitial(queue);
		int			nChildren = pn_ahead->num_rules + (pn_ahead->default_part ? 1 : 0);
		HTAB	   *conHash = createConstraintHashTable(nChildren);

		/* get the list of part keys for this level */
		List	   *lpartkey = NIL;

		for (int i = 0; i < pn_ahead->part->parnatts; i++)
		{
			lpartkey = lappend_int(lpartkey, pn_ahead->part->paratts[i]);
		}

		/* now iterate over all partition nodes on this level */
		bool		fFirstNode = true;

		while (size > 0 && result)
		{
			PartitionNode *pn = (PartitionNode *) linitial(queue);
			List	   *lrules = get_partition_rules(pn);
			int			curr_nChildren = list_length(lrules);

			if (curr_nChildren != nChildren)
			{
				result = false;
				break;
			}

			/* loop over the children's constraints of this node */
			ListCell   *lc = NULL;

			foreach(lc, lrules)
			{
				PartitionRule *pr = (PartitionRule *) lfirst(lc);
				Node	   *curr_con = getPartConstraintsContainsKeys(pr->parchildrelid, rootOid, lpartkey);
				bool		found = false;

				/*
				 * we populate the hash table with the constraints of the
				 * children of the first node in this level
				 */
				if (fFirstNode)
				{
					/* add current constraint to hash table */
					void	   *con_entry;

					con_entry = hash_search(conHash, &curr_con, HASH_ENTER, &found);
					((ConNodeEntry *) con_entry)->entry = curr_con;
				}

				/*
				 * starting from the second node in this level, we probe the
				 * children's constraints
				 */
				else
				{
					hash_search(conHash, &curr_con, HASH_FIND, &found);
					if (!found)
					{
						result = false;
						break;
					}
				}

				if (pr->children)
				{
					queue = lappend(queue, pr->children);
				}
			}
			size--;
			fFirstNode = false;
			queue = list_delete_first(queue);
			pfree(lrules);
		}

		hash_destroy(conHash);
		pfree(lpartkey);

	}

	MemoryContextSwitchTo(callerMemoryContext);
	MemoryContextDelete(uniformityMemoryContext);

	return result;
}

/* Return the pg_class Oids of the relations representing leaf parts of the
 * PartitionNode tree headed by the argument PartitionNode.
 *
 * The caller should be responsible for freeing the list after
 * using it.
 */
List *
all_leaf_partition_relids(PartitionNode *pn)
{
	if (NULL == pn)
	{
		return NIL;
	}

	List	   *leaf_relids = NIL;
	for (int i = 0; i < pn->num_rules; i++)
	{
		PartitionRule *rule = pn->rules[i];

		if (NULL != rule->children)
		{
			leaf_relids = list_concat(leaf_relids, all_leaf_partition_relids(rule->children));
		}
		else
		{
			leaf_relids = lappend_oid(leaf_relids, rule->parchildrelid);
		}

	}
	if (NULL != pn->default_part)
	{
		if (NULL != pn->default_part->children)
		{
			leaf_relids = list_concat(leaf_relids,
									  all_leaf_partition_relids(pn->default_part->children));
		}
		else
		{
			leaf_relids = lappend_oid(leaf_relids, pn->default_part->parchildrelid);
		}
	}
	return leaf_relids;
}

/*
 * Given an Oid of a partition rule, return all leaf-level table Oids that are
 * descendants of the given rule.
 * Input:
 *	ruleOid - the oid of an entry in pg_partition_rule
 * Output:
 *	a list of Oids of all leaf-level partition tables under the given rule in
 *	the partitioning hierarchy.
 */
static List *
rel_get_leaf_relids_from_rule(Oid ruleOid)
{
	ScanKeyData scankey;
	Relation	part_rule_rel;
	SysScanDesc sscan;
	bool		hasChildren = false;
	List	   *lChildrenOid = NIL;
	HeapTuple	tuple;

	if (!OidIsValid(ruleOid))
	{
		return NIL;
	}

	part_rule_rel = heap_open(PartitionRuleRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_rule_parparentrule,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(ruleOid));

	/* No suitable index */
	sscan = systable_beginscan(part_rule_rel, InvalidOid, false,
							   NULL, 1, &scankey);

	/*
	 * If we are still in mid-level, recursively call this function on
	 * children rules of the given rule.
	 */
	while ((tuple = systable_getnext(sscan)) != NULL)
	{
		hasChildren = true;
		lChildrenOid = list_concat(lChildrenOid, rel_get_leaf_relids_from_rule(HeapTupleGetOid(tuple)));
	}

	/*
	 * if ruleOid is not parent of any rule, we have reached the leaf level
	 * and we need to append parchildrelid of this entry to the output
	 */
	if (!hasChildren)
	{
		HeapTuple	tuple;
		Form_pg_partition_rule rule_desc;

		tuple = SearchSysCache1(PARTRULEOID, ObjectIdGetDatum(ruleOid));
		if (!tuple)
			elog(ERROR, "cache lookup failed for partition rule with OID %u", ruleOid);
		rule_desc = (Form_pg_partition_rule) GETSTRUCT(tuple);

		lChildrenOid = lcons_oid(rule_desc->parchildrelid, lChildrenOid);

		ReleaseSysCache(tuple);
	}

	systable_endscan(sscan);

	heap_close(part_rule_rel, AccessShareLock);

	return lChildrenOid;
}

/* Given a partition table Oid (root or interior), return the Oids of all leaf-level
 * children below it. Similar to all_leaf_partition_relids() but takes Oid as input.
 */
List *
rel_get_leaf_children_relids(Oid relid)
{
	PartStatus	ps = rel_part_status(relid);
	List	   *leaf_relids = NIL;

	Assert(PART_STATUS_INTERIOR == ps || PART_STATUS_ROOT == ps);

	if (PART_STATUS_ROOT == ps)
	{
		PartitionNode *pn;

		pn = get_parts(relid, 0 /* level */ , 0 /* parent */ , false /* inctemplate */ ,
					   true /* includesubparts */ );
		leaf_relids = all_leaf_partition_relids(pn);
		pfree(pn);
	}
	else if (PART_STATUS_INTERIOR == ps)
	{
		Relation	partrulerel;
		ScanKeyData scankey;
		SysScanDesc sscan;
		HeapTuple	tuple;

		/* SELECT * FROM pg_partition_rule WHERE parchildrelid = :1 */
		partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

		ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(relid));

		sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
								   NULL, 1, &scankey);
		tuple = systable_getnext(sscan);
		if (HeapTupleIsValid(tuple))
		{
			leaf_relids = rel_get_leaf_relids_from_rule(HeapTupleGetOid(tuple));
		}
		systable_endscan(sscan);
		heap_close(partrulerel, AccessShareLock);
	}
	else if (PART_STATUS_LEAF == ps)
	{
		leaf_relids = list_make1_oid(relid);
	}

	return leaf_relids;
}

/* Return the pg_class Oids of the relations representing interior parts of the
 * PartitionNode tree headed by the argument PartitionNode.
 *
 * The caller is responsible for freeing the list after using it.
 */
List *
all_interior_partition_relids(PartitionNode *pn)
{
	if (NULL == pn)
	{
		return NIL;
	}

	List	   *interior_relids = NIL;

	for (int i = 0; i < pn->num_rules; i++)
	{
		PartitionRule *rule = pn->rules[i];

		if (rule->children)
		{
			interior_relids = lappend_oid(interior_relids, rule->parchildrelid);
			interior_relids = list_concat(interior_relids, all_interior_partition_relids(rule->children));
		}
	}

	if (pn->default_part)
	{
		if (pn->default_part->children)
		{
			interior_relids = lappend_oid(interior_relids, pn->default_part->parchildrelid);
			interior_relids = list_concat(interior_relids,
										  all_interior_partition_relids(pn->default_part->children));
		}
	}

	return interior_relids;
}

/*
 * Return the number of leaf parts of the partitioned table with the given oid
 */
int
countLeafPartTables(Oid rootOid)
{
	Assert(rel_is_partitioned(rootOid));

	PartitionNode *pn = get_parts(rootOid, 0 /* level */ , 0 /* parent */ , false /* inctemplate */ ,
								  true /* include subparts */ );

	List	   *lRelOids = all_leaf_partition_relids(pn);

	Assert(list_length(lRelOids) > 0);
	int			count = list_length(lRelOids);

	list_free(lRelOids);
	pfree(pn);
	return count;
}

/* Return the pg_class Oids of the relations representing the parts
 * of the PartitionRule tree headed by the argument PartitionRule.
 *
 * This local function is similar to all_partition_relids but enters
 * at a PartitionRule which is more convenient in some cases, e.g.,
 * on the topRule of a PgPartRule.
 */
List *
all_prule_relids(PartitionRule *prule)
{
	PartitionNode *pnode;

	List	   *oids = NIL;		/* of pg_class Oid */

	if (prule)
	{
		oids = lappend_oid(oids, prule->parchildrelid);

		pnode = prule->children;
		if (pnode)
		{
			oids = list_concat(oids, all_prule_relids(pnode->default_part));
			for (int i = 0; i < pnode->num_rules; i++)
			{
				PartitionRule *child = pnode->rules[i];

				oids = list_concat(oids, all_prule_relids(child));
			}
		}
	}
	return oids;
}

/*
 * Returns the parent Oid from the given part Oid.
 */
Oid
rel_partition_get_root(Oid relid)
{
	Relation	inhrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			masteroid;

	inhrel = heap_open(InheritsRelationId, AccessShareLock);

	/* SELECT inhparent FROM pg_inherits WHERE inhrelid = :1 */
	ScanKeyInit(&scankey, Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	sscan = systable_beginscan(inhrel, InheritsRelidSeqnoIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);
	if (!tuple)
		masteroid = InvalidOid;
	else
		masteroid = ((Form_pg_inherits) GETSTRUCT(tuple))->inhparent;

	systable_endscan(sscan);
	heap_close(inhrel, AccessShareLock);

	return masteroid;
}

/* Get the top relation of the partitioned table of which the given
 * relation is a part, or error.
 *
 *   select parrelid
 *   from pg_partition
 *   where oid = (
 *     select paroid
 *     from pg_partition_rule
 *     where parchildrelid = relid);
 */
Oid
rel_partition_get_master(Oid relid)
{
	Relation	partrulerel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			paroid;
	Oid			masteroid;

	/*
	 * pg_partition and  pg_partition_rule are populated only on the entry
	 * database, so our result is only meaningful there.
	 */
	Insist(IS_QUERY_DISPATCHER());

	partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

	/* SELECT paroid FROM pg_partition_rule WHERE parchildrelid = :1 */
	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);
	if (tuple)
		paroid = ((Form_pg_partition_rule) GETSTRUCT(tuple))->paroid;
	else
		paroid = InvalidOid;

	systable_endscan(sscan);
	heap_close(partrulerel, AccessShareLock);

	if (!OidIsValid(paroid))
		return InvalidOid;

	tuple = SearchSysCache1(PARTOID, ObjectIdGetDatum(paroid));
	if (!tuple)
		elog(ERROR, "could not find pg_partition entry with oid %d for "
			 "pg_partition_rule with child table %d", paroid, relid);

	masteroid = ((Form_pg_partition) GETSTRUCT(tuple))->parrelid;
	ReleaseSysCache(tuple);

	return masteroid;

}								/* end rel_partition_get_master */

/* given a relid, build a path list from the master tablename down to
 * the partition for that relation, using partition names if possible,
 * else rank or value expressions.
 */
List *
rel_get_part_path1(Oid relid)
{
	Relation	partrulerel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			paroid = InvalidOid;
	Oid			parparentrule = InvalidOid;
	List	   *lrelid = NIL;

	/*
	 * pg_partition and  pg_partition_rule are populated only on the entry
	 * database, so our result is only meaningful there.
	 */
	Insist(IS_QUERY_DISPATCHER());

	partrulerel = heap_open(PartitionRuleRelationId, AccessShareLock);

	/*
	 * Use the relid of the table to find the first rule
	 *
	 * SELECT paroid FROM pg_partition_rule WHERE parchildrelid = :1
	 */
	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	sscan = systable_beginscan(partrulerel, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);

	if (HeapTupleIsValid(tuple))
	{
		Form_pg_partition_rule rule_desc =
		(Form_pg_partition_rule) GETSTRUCT(tuple);

		paroid = rule_desc->paroid;
		parparentrule = rule_desc->parparentrule;

		/* prepend relid of child table to list */
		lrelid = lcons_oid(rule_desc->parchildrelid, lrelid);
	}

	systable_endscan(sscan);
	heap_close(partrulerel, AccessShareLock);

	if (!OidIsValid(paroid))
		return NIL;

	/* walk up the tree using the parent rule oid */

	while (OidIsValid(parparentrule))
	{
		tuple = SearchSysCache1(PARTRULEOID, ObjectIdGetDatum(parparentrule));
		if (HeapTupleIsValid(tuple))
		{
			Form_pg_partition_rule rule_desc =
			(Form_pg_partition_rule) GETSTRUCT(tuple);

			paroid = rule_desc->paroid;
			parparentrule = rule_desc->parparentrule;

			/* prepend relid of child table to list */
			lrelid = lcons_oid(rule_desc->parchildrelid, lrelid);

			ReleaseSysCache(tuple);
		}
		else
			parparentrule = InvalidOid; /* we are done */
	}

	return lrelid;

}								/* end rel_get_part_path1 */

static List *
rel_get_part_path(Oid relid)
{
	PartitionNode *pNode = NULL;
	Partition  *part = NULL;
	List	   *lrelid = NIL;
	List	   *lnamerank = NIL;
	List	   *lnrv = NIL;
	ListCell   *lc;
	Oid			masteroid = InvalidOid;

	masteroid = rel_partition_get_master(relid);

	if (!OidIsValid(masteroid))
		return NIL;

	/* call the guts of RelationBuildPartitionDesc */
	pNode = get_parts(masteroid, 0, 0, false, true /* includesubparts */ );

	if (!pNode)
	{
		return NIL;
	}

	part = pNode->part;

	/*
	 * get the relids for each table that corresponds to the partition
	 * heirarchy from the master to the specified partition
	 */
	lrelid = rel_get_part_path1(relid);

	/*
	 * walk the partition tree, finding the partition for each relid, and
	 * extract useful information (name, rank, value)
	 */
	foreach(lc, lrelid)
	{
		Oid			parrelid = lfirst_oid(lc);
		PartitionRule *prule;
		int			rulerank = 1;

		Assert(pNode);

		part = pNode->part;

		rulerank = 1;

		for (int i = 0; i < pNode->num_rules; i++)
		{
			prule = pNode->rules[i];

			if (parrelid == prule->parchildrelid)
			{
				pNode = prule->children;
				goto L_rel_get_part_path_match;
			}
			rulerank++;
		}

		/* if we get here, then must match the default partition */
		prule = pNode->default_part;

		Assert(parrelid == prule->parchildrelid);

		/* default partition must have a name (and no rank) */
		Assert(prule->parname && strlen(prule->parname));

		pNode = prule->children;
		rulerank = 0;

L_rel_get_part_path_match:

		if (!rulerank)			/* must be default, so it has a name, but no
								 * rank or value */
		{
			lnrv = list_make3(prule->parname, NULL, NULL);
		}
		else if (part->parkind == 'l')	/* list partition by value */
		{
			char	   *idval = NULL;
			ListCell   *lc3;
			List	   *l1 = (List *) prule->parlistvalues;
			StringInfoData sid1;
			int16		nkeys = part->parnatts;
			int16		parcol = 0;

			initStringInfo(&sid1);

			/* foreach(lc3, l1) */
			/* don't loop -- just need first set of values */

			lc3 = list_head(l1);

			if (lc3)
			{
				List	   *vals = lfirst(lc3);
				ListCell   *lcv = list_head(vals);

				/*
				 * Note: similar code in ruleutils.c:partition_rule_def_worker
				 */

				for (parcol = 0; parcol < nkeys; parcol++)
				{
					Const	   *con = lfirst(lcv);

					if (lcv != list_head(vals))
						appendStringInfoString(&sid1, ", ");

					idval =
						deparse_expression((Node *) con,
										   deparse_context_for(get_rel_name(relid),
															   relid),
										   false, false);

					appendStringInfo(&sid1, "%s", idval);

					lcv = lnext(lcv);
				}				/* end for parcol */
			}
			/* list - no rank */
			lnrv = list_make3(prule->parname, NULL, sid1.data);
		}
		else					/* range (or hash) - use rank (though rank is
								 * not really appropriate for hash) */
		{
			char	   *rtxt = palloc(NAMEDATALEN);

			sprintf(rtxt, "%d", rulerank);

			/* not list - rank, but no value */
			lnrv = list_make3(prule->parname, rtxt, NULL);
		}

		/* build the list of (lists of name, rank, value) for each level */
		lnamerank = lappend(lnamerank, lnrv);

	}							/* end foreach lc (walking list of relids) */

	return lnamerank;
}								/* end rel_get_part_path */

char *
rel_get_part_path_pretty(Oid relid,
						 char *separator,
						 char *lastsep)
{
	List	   *lnamerank = NIL;
	List	   *lnrv = NIL;
	ListCell   *lc,
			   *lc2;
	int			maxlen;
	StringInfoData sid1,
				sid2;

	lnamerank = rel_get_part_path(relid);

	maxlen = list_length(lnamerank);

	if (!maxlen)
		return NULL;

	Assert(separator);
	Assert(lastsep);

	initStringInfo(&sid1);
	initStringInfo(&sid2);

	foreach(lc, lnamerank)
	{
		int			lcnt = 0;

		lnrv = (List *) lfirst(lc);

		maxlen--;

		appendStringInfo(&sid1, "%s", maxlen ? separator : lastsep);

		lcnt = 0;

		foreach(lc2, lnrv)
		{
			char	   *str = (char *) lfirst(lc2);

			resetStringInfo(&sid2);

			switch (lcnt)
			{
				case 0:
					if (str && strlen(str))
					{
						appendStringInfo(&sid2, "\"%s\"", str);
						goto l_pretty;
					}
					break;

				case 1:
					if (str && strlen(str))
					{
						appendStringInfo(&sid2, "FOR(RANK(%s))", str);
						goto l_pretty;
					}
					break;

				case 2:
					if (str && strlen(str))
					{
						appendStringInfo(&sid2, "FOR(%s)", str);
						goto l_pretty;
					}
					break;
				default:
					break;
			}
			lcnt++;
		}

l_pretty:

		appendStringInfo(&sid1, "%s", sid2.data);
	}

	return sid1.data;
}								/* end rel_get_part_path_pretty */


/*
 * ChoosePartitionName: given a table name, partition "depth", and a
 * partition name, generate a unique name using ChooseRelationName.
 * The partition depth is the raw parlevel (zero-based), which is
 * incremented by 1 to be one-based.
 *
 * Note: calls CommandCounterIncrement.
 *
 * Returns a palloc'd string (from ChooseRelationName)
 */
char *
ChoosePartitionName(const char *tablename, int partDepth,
					const char *partname, Oid namespaceId)
{
	char	   *relname;
	char		depthstr[NAMEDATALEN];
	char		prtstr[NAMEDATALEN];

	/* build a relation name (see transformPartitionBy */
	snprintf(depthstr, sizeof(depthstr), "%d", partDepth + 1);
	snprintf(prtstr, sizeof(prtstr), "prt_%s", partname);

	relname = ChooseRelationName(tablename,
								 depthstr,	/* depth */
								 prtstr,	/* part spec */
								 namespaceId);
	CommandCounterIncrement();

	return relname;
}

/*
 * Given a constant expression, build a datum according to
 * part->paratts and the relation tupledesc.  Needs work for
 * type_coercion, multicol, etc.  The returned Datum * is suitable for
 * use in SelectPartition
 */
static Datum *
magic_expr_to_datum(Relation rel, PartitionNode *partnode,
					Node *expr, bool **ppisnull)
{
	Partition  *part = partnode->part;
	TupleDesc	tupleDesc;
	Datum	   *values;
	bool	   *isnull;
	int			ii,
				jj;

	Assert(rel);

	tupleDesc = RelationGetDescr(rel);

	/* Preallocate values/isnull arrays */
	ii = tupleDesc->natts;
	values = (Datum *) palloc(ii * sizeof(Datum));
	isnull = (bool *) palloc(ii * sizeof(bool));
	memset(values, 0, ii * sizeof(Datum));
	memset(isnull, true, ii * sizeof(bool));

	*ppisnull = isnull;

	Assert(IsA(expr, List));

	jj = list_length((List *) expr);

	if (jj > ii)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("too many columns in boundary specification (%d > %d)",
						jj, ii)));

	if (jj > part->parnatts)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("too many columns in boundary specification (%d > %d)",
						jj, part->parnatts)));

	{
		ListCell   *lc;
		int			i = 0;

		foreach(lc, (List *) expr)
		{
			Node	   *n1 = (Node *) lfirst(lc);
			Const	   *c1;
			AttrNumber	attno = part->paratts[i++];
			Form_pg_attribute attribute = tupleDesc->attrs[attno - 1];
			Oid			lhsid = attribute->atttypid;

			if (!IsA(n1, Const))
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("not a constant expression")));

			c1 = (Const *) n1;

			if (lhsid != c1->consttype)
			{
				/* see coerce_partition_value */
				Node	   *out;

				out = coerce_partition_value(NULL, n1, lhsid, attribute->atttypmod,
											 char_to_parttype(partnode->part->parkind));
				if (!out)
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("cannot coerce column type (%s versus %s)",
									format_type_be(c1->consttype),
									format_type_be(lhsid))));

				Assert(IsA(out, Const));

				c1 = (Const *) out;
			}

			/* XXX: cache */
			values[attno - 1] = c1->constvalue;
			isnull[attno - 1] = c1->constisnull;
		}
	}

	return values;
}								/* end magic_expr_to_datum */

/*
 * Assume the partition rules are in the "correct" order and return
 * the nth rule (1-based).  If rnk is negative start from the end, ie
 * -1 is the last rule.
 */
static Oid
selectPartitionByRank(PartitionNode *partnode, int rnk)
{
	Oid			relid = InvalidOid;
	PartitionRule *rule;

	Assert(partnode->part->parkind == 'r');

	if (rnk > partnode->num_rules)
		return relid;

	if (rnk == 0)
		return relid;

	if (rnk > 0)
		rnk--;					/* list_nth is zero-based, not one-based */
	else if (rnk < 0)
	{
		rnk = partnode->num_rules + rnk; /* if negative go from end */

		/* mpp-3265 */
		if (rnk < 0)			/* oops -- too negative */
			return relid;
	}

	rule = partnode->rules[rnk];

	return rule->parchildrelid;
}								/* end selectPartitionByRank */

static bool
compare_partn_opfuncid(PartitionNode *partnode,
					   char *pub, char *compare_op,
					   List *colvals,
					   Datum *values, bool *isnull,
					   TupleDesc tupdesc)

{
	Partition  *part = partnode->part;
	List	   *last_opname = list_make2(makeString(pub),
										 makeString(compare_op));
	List	   *opname = NIL;
	ListCell   *lc;
	int			numCols = 0;
	int			colCnt = 0;
	int			ii = 0;

	if (1 == strlen(compare_op))
	{
		/* handle case of less than or greater than */
		if (0 == strcmp("<", compare_op))
			compare_op = "<=";
		if (0 == strcmp(">", compare_op))
			compare_op = ">=";

		/*
		 * for a list of values, when performing less than or greater than
		 * comparison, only the final value is compared using less than or
		 * greater.  All prior values must be compared with LTE/GTE.  For
		 * example, comparing the list (1,2,3) to see if it is less than
		 * (1,2,4), we see that 1 <= 1, 2 <= 2, and 3 < 4.  So the last_opname
		 * is the specified compare_op, and the prior opnames are LTE or GTE.
		 */

	}

	opname = list_make2(makeString(pub), makeString(compare_op));

	colCnt = numCols = list_length(colvals);

	foreach(lc, colvals)
	{
		Const	   *c = lfirst(lc);
		AttrNumber	attno = part->paratts[ii];

		if (isnull && isnull[attno - 1])
		{
			if (!c->constisnull)
				return false;
		}
		else
		{
			Oid			lhsid = tupdesc->attrs[attno - 1]->atttypid;
			Oid			rhsid = lhsid;
			Oid			opfuncid;
			Datum		res;
			Datum		d = values[attno - 1];

			if (1 == colCnt)
			{
				opname = last_opname;
			}

			opfuncid = get_opfuncid_by_opname(opname, lhsid, rhsid);
			res = OidFunctionCall2Coll(opfuncid, c->constcollid, c->constvalue, d);

			if (!DatumGetBool(res))
				return false;
		}

		ii++;
		colCnt--;
	}							/* end foreach */

	return true;
}								/* end compare_partn_opfuncid */

/*
 *	Given a partition-by-list PartitionNode, search for
 *	a part that matches the given datum value.
 *
 *	Input parameters:
 *	partnode: the PartitionNode that we search for matched parts
 *	values, isnull: datum values to search for parts
 *	tupdesc: TupleDesc for retrieving values
 *	accessMethods: PartitionAccessMethods
 *	foundOid: output parameter for matched part Oid
 *	prule: output parameter for matched part PartitionRule
 *	exprTypeOid: type of the expression used to select partition
 */
static PartitionNode *
selectListPartition(PartitionNode *partnode, Datum *values, bool *isnull,
					TupleDesc tupdesc, PartitionAccessMethods *accessMethods, Oid *foundOid, PartitionRule **prule,
					Oid exprTypeOid)
{
	Partition  *part = partnode->part;
	MemoryContext oldcxt = NULL;
	FmgrInfo   *cmpfuncs;

	if (accessMethods && accessMethods->cmpfuncs[partnode->part->parlevel])
		cmpfuncs = accessMethods->cmpfuncs[partnode->part->parlevel];
	else
	{
		int			natts = partnode->part->parnatts;

		cmpfuncs = (FmgrInfo *) palloc0(sizeof(FmgrInfo) * natts);

		for (int keyno = 0; keyno < natts; keyno++)
		{
			AttrNumber	attno = part->paratts[keyno];

			/*
			 * Compute the type of the LHS and RHS for the
			 * equality comparator. The way we call the comparator
			 * is comp(expr, rule) So lhstypid = type(expr) and
			 * rhstypeid = type(rule)
			 */

			/*
			 * The tupdesc tuple descriptor matches the table
			 * schema, so it has the rule type
			 */
			Oid			rhstypid = tupdesc->attrs[attno - 1]->atttypid;

			/*
			 * exprTypeOid is passed to us from our caller which
			 * evaluated the expression. In some cases (e.g legacy
			 * optimizer doing explicit casting), we don't compute
			 * specify exprTypeOid. Assume lhstypid = rhstypid in
			 * those cases
			 */
			Oid			lhstypid = exprTypeOid;

			if (!OidIsValid(lhstypid))
				lhstypid = rhstypid;

			get_cmp_func(partnode->part->parclass[keyno], &cmpfuncs[keyno],
						 lhstypid, rhstypid);
		}

		if (accessMethods)
			accessMethods->cmpfuncs[partnode->part->parlevel] = cmpfuncs;
	}

	if (accessMethods && accessMethods->part_cxt)
		oldcxt = MemoryContextSwitchTo(accessMethods->part_cxt);

	*foundOid = InvalidOid;

	/* With LIST, we have no choice at the moment except to be exhaustive */
	for (int ruleno = 0; ruleno < partnode->num_rules; ruleno++)
	{
		PartitionRule *rule = partnode->rules[ruleno];
		List	   *vals = rule->parlistvalues;
		ListCell   *lc2;
		bool		matched = false;

		/*
		 * list values are stored in a list of lists to support multi column
		 * partitions.
		 *
		 * At this level, we're processing the list of possible values for the
		 * given rule, for example: values(1, 2, 3) values((1, '2005-01-01'),
		 * (2, '2006-01-01'))
		 *
		 * Each iteraction is one element of the values list. In the first
		 * example, we iterate '1', '2' then '3'. For the second, we iterate
		 * through '(1, '2005-01-01')' then '(2, '2006-01-01')'. It's for that
		 * reason that we need the inner loop.
		 */
		foreach(lc2, vals)
		{
			ListCell   *lc3;
			List	   *colvals = (List *) lfirst(lc2);
			int			i = 0;

			matched = true;		/* prove untrue */

			foreach(lc3, colvals)
			{
				Const	   *c = lfirst(lc3);
				AttrNumber	attno = part->paratts[i];

				if (isnull[attno - 1])
				{
					if (!c->constisnull)
					{
						matched = false;
						break;
					}
				}
				else if (c->constisnull)
				{
					/* constant is null but datum isn't so break */
					matched = false;
					break;
				}
				else
				{
					int			res;
					Datum		d = values[attno - 1];
					FmgrInfo   *finfo = &cmpfuncs[i];

					res = DatumGetInt32(FunctionCall2Coll(finfo, c->constcollid, d, c->constvalue));

					if (res != 0)
					{
						matched = false;
						break;
					}
				}
				i++;
			}
			if (matched)
				break;
		}

		if (matched)
		{
			*foundOid = rule->parchildrelid;
			*prule = rule;

			/* go to the next level */
			if (oldcxt)
				MemoryContextSwitchTo(oldcxt);
			return rule->children;
		}
	}

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	return NULL;

}

/*
 * Look up comparison function between types 'lhstypid' and 'rhstypid'.
 *
 * If either type ID is InvalidOid, the operator class's opcintype is used
 * in its place. If either type is a binary-coercible to opcintype, the
 * function for opcintype is looked up instaed.
 */
static void
get_cmp_func(Oid opclass, FmgrInfo *fmgrinfo, Oid lhstypid, Oid rhstypid)
{
	HeapTuple	opclasstup;
	Form_pg_opclass opclassform;
	Oid			funcid;

	opclasstup = SearchSysCache1(CLAOID,
								 ObjectIdGetDatum(opclass));
	if (!HeapTupleIsValid(opclasstup))
		elog(ERROR, "cache lookup failed for opclass %u", opclass);

	opclassform = (Form_pg_opclass) GETSTRUCT(opclasstup);

	if (lhstypid == InvalidOid)
		lhstypid = opclassform->opcintype;
	if (lhstypid != opclassform->opcintype && IsBinaryCoercible(lhstypid, opclassform->opcintype))
		lhstypid = opclassform->opcintype;

	if (rhstypid == InvalidOid)
		rhstypid = opclassform->opcintype;
	if (rhstypid != opclassform->opcintype && IsBinaryCoercible(rhstypid, opclassform->opcintype))
		rhstypid = opclassform->opcintype;

	/* Get a support function for the specified opfamily and datatypes */
	funcid = get_opfamily_proc(opclassform->opcfamily,
							   lhstypid,
							   rhstypid,
							   BTORDER_PROC);
	if (!OidIsValid(funcid))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
				 errmsg("operator class \"%s\" of access method btree is missing comparison support function between types %s and %s",
						NameStr(opclassform->opcname),
						format_type_be(lhstypid),
						format_type_be(rhstypid))));

	fmgr_info(funcid, fmgrinfo);

	ReleaseSysCache(opclasstup);
}

/*
 * range_test
 *   Test if an expression value falls in the range of a given partition rule
 *
 *  Input parameters:
 *    tupval: The value of the expression
 *    cmpfuncs: Comparison functions
 *    keyno: The index of the partitioning key considered (for composite partitioning keys)
 *    rule: The rule whose boundaries we're testing
 *
 */
static int
range_test(Datum tupval, FmgrInfo *cmpfuncs, int keyno,
		   PartitionRule *rule)
{
	Const	   *c;
	FmgrInfo   *finfo  = &cmpfuncs[keyno];
	int32		res;

	Assert(PointerIsValid(rule->parrangestart) ||
		   PointerIsValid(rule->parrangeend));

	/* might be a rule with no START value */
	if (PointerIsValid(rule->parrangestart))
	{
		Assert(IsA(rule->parrangestart, List));
#if NOT_YET
		c = (Const *) list_nth((List *) rule->parrangestart, keyno);
#else
		c = (Const *) linitial((List *) rule->parrangestart);
#endif

		/*
		 * Is the value in the range?
		 */
		res = DatumGetInt32(FunctionCall2Coll(finfo, c->constcollid, tupval, c->constvalue));

		if (res < 0)
			return -1;
		else if (res == 0 && !rule->parrangestartincl)
			return -1;
	}

	/* There might be no END value */
	if (PointerIsValid(rule->parrangeend))
	{
#if NOT_YET
		c = (Const *) list_nth((List *) rule->parrangeend, keyno);
#else
		c = (Const *) linitial((List *) rule->parrangeend);
#endif

		/*
		 * Is the value in the range? If rule->parrangeendincl, we request for
		 * comparator exprVal <= ruleVal ( ==> strictly_less = false)
		 * Otherwise, we request comparator exprVal < ruleVal ( ==>
		 * strictly_less = true)
		 */
		res = DatumGetInt32(FunctionCall2Coll(finfo, c->constcollid, tupval, c->constvalue));

		if (res > 0)
			return 1;
		else if (res == 0 && !rule->parrangeendincl)
			return 1;
	}
	return 0;
}

/*
 * Given a partition specific part, a tuple as represented by values and isnull and
 * a list of rules, return an Oid in *foundOid or the next set of rules.
 */
static PartitionNode *
selectRangePartition(PartitionNode *partnode, Datum *values, bool *isnull,
					 TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
					 Oid *foundOid, int *pSearch, PartitionRule **prule, Oid exprTypeOid)
{
	PartitionRule **rules = partnode->rules;
	int			num_rules = partnode->num_rules;
	int			high = num_rules - 1;
	int			low = 0;
	int			searchpoint = 0;
	int			mid = 0;
	bool		matched = false;
	PartitionRule *rule = NULL;
	PartitionNode *pNode = NULL;
	FmgrInfo   *cmpfuncs;
	MemoryContext oldcxt = NULL;

	Assert(partnode->part->parkind == 'r');

	/*
	 * For composite partitioning keys, exprTypeOid should always be
	 * InvalidOid
	 */
	AssertImply(partnode->part->parnatts > 1, !OidIsValid(exprTypeOid));

	if (accessMethods && accessMethods->cmpfuncs[partnode->part->parlevel])
		cmpfuncs = accessMethods->cmpfuncs[partnode->part->parlevel];
	else
	{
		int			natts = partnode->part->parnatts;

		/*
		 * We're still in our caller's memory context so the memory will
		 * persist long enough for us.
		 */
		cmpfuncs = (FmgrInfo *) palloc0(sizeof(FmgrInfo) * natts);

		for (int keyno = 0; keyno < natts; keyno++)
		{
			AttrNumber	attno = partnode->part->paratts[keyno];
			Oid			rhstypid = tupdesc->attrs[attno - 1]->atttypid;
			Oid			lhstypid = exprTypeOid;

			if (!OidIsValid(lhstypid))
				lhstypid = rhstypid;

			get_cmp_func(partnode->part->parclass[keyno], &cmpfuncs[keyno],
						 lhstypid, rhstypid);
		}
	}

	if (accessMethods && accessMethods->part_cxt)
		oldcxt = MemoryContextSwitchTo(accessMethods->part_cxt);

	*foundOid = InvalidOid;

	/*
	 * Use a binary search to try and pin point the region within the set of
	 * rules where the rule is. If the partition is across a single column,
	 * the rule located by the binary search is the only possible candidate.
	 * If the partition is across more than one column, we need to search
	 * sequentially to either side of the rule to see if the match is there.
	 * The reason for this complexity is the nature of find a point within an
	 * interval.
	 *
	 * Consider the following intervals:
	 *
	 * 1. start( 1, 8)   end( 10,  9) 2. start( 1, 9)   end( 15, 10) 3. start(
	 * 1, 11)  end(100, 12) 4. start(15, 10)  end( 30, 11)
	 *
	 * If we were to try and find the partition for a tuple (25, 10), using
	 * the binary search for the first element, we'd select partition 3 but
	 * partition 4 is also a candidate. It is only when we look at the second
	 * element that we find the single definitive rule.
	 */
	while (low <= high)
	{
		AttrNumber	attno = partnode->part->paratts[0];
		Datum		exprValue = values[attno - 1];
		int			ret;

		mid = low + (high - low) / 2;

		rule = rules[mid];

		if (isnull[attno - 1])
		{
			pNode = NULL;
			goto l_fin_range;
		}

		if (OidIsValid(exprTypeOid))
		{
			ret = range_test(exprValue, cmpfuncs, 0, rule);
		}
		else
		{
			/*
			 * In some cases, we don't have an expression type oid. In those
			 * cases, the expression and partition rules have the same type.
			 */
			ret = range_test(exprValue, cmpfuncs, 0, rule);
		}

		if (ret > 0)
		{
			searchpoint = mid;
			low = mid + 1;
			continue;
		}
		else if (ret < 0)
		{
			high = mid - 1;
			continue;
		}
		else
		{
			matched = true;
			break;
		}
	}

	if (matched)
	{
		int			j;

		/* Non-composite partition key, we matched so we're done */
		if (partnode->part->parnatts == 1)
		{
			*foundOid = rule->parchildrelid;
			*prule = rule;

			pNode = rule->children;
			goto l_fin_range;
		}

		/*
		 * We have more than one partition key.. Must match on the other keys
		 * as well
		 */
		j = mid;
		do
		{
			int			i;
			bool		matched = true;
			bool		first_fail = false;

			for (i = 0; i < partnode->part->parnatts; i++)
			{
				AttrNumber	attno = partnode->part->paratts[i];
				Datum		d = values[attno - 1];
				int			ret;

				if (j != mid)
					rule = rules[j];

				if (isnull[attno - 1])
				{
					pNode = NULL;
					goto l_fin_range;
				}

				/*
				 * For composite partition keys, we don't support casting
				 * comparators, so both sides must be of identical types
				 */
				Assert(!OidIsValid(exprTypeOid));
				ret = range_test(d, cmpfuncs, i, rule);
				if (ret != 0)
				{
					matched = false;

					/*
					 * If we've gone beyond the boundary of rules which match
					 * the first tuple, no use looking further
					 */
					if (i == 0)
						first_fail = true;

					break;
				}
			}

			if (first_fail)
				break;

			if (matched)
			{
				*foundOid = rule->parchildrelid;
				*prule = rule;

				pNode = rule->children;
				goto l_fin_range;
			}

		}
		while (--j >= 0);

		j = mid;
		do
		{
			int			i;
			bool		matched = true;
			bool		first_fail = false;

			for (i = 0; i < partnode->part->parnatts; i++)
			{
				AttrNumber	attno = partnode->part->paratts[i];
				Datum		d = values[attno - 1];
				int			ret;

				rule = rules[j];

				if (isnull[attno - 1])
				{
					pNode = NULL;
					goto l_fin_range;
				}

				/*
				 * For composite partition keys, we don't support casting
				 * comparators, so both sides must be of identical types
				 */
				Assert(!OidIsValid(exprTypeOid));
				ret = range_test(d, cmpfuncs, i, rule);
				if (ret != 0)
				{
					matched = false;

					/*
					 * If we've gone beyond the boundary of rules which match
					 * the first tuple, no use looking further
					 */
					if (i == 0)
						first_fail = true;

					break;
				}
			}

			if (first_fail)
				break;
			if (matched)
			{
				*foundOid = rule->parchildrelid;
				*prule = rule;

				pNode = rule->children;
				goto l_fin_range;
			}

		}
		while (++j < num_rules);
	}							/* end if matched */

	pNode = NULL;

l_fin_range:
	if (pSearch)
		*pSearch = searchpoint;

	if (oldcxt)
		MemoryContextSwitchTo(oldcxt);

	if (accessMethods)
		accessMethods->cmpfuncs[partnode->part->parlevel] = cmpfuncs;

	return pNode;
}								/* end selectrangepartition */


/*
 * selectPartition1()
 *
 * Given pdata and prules, try and find a suitable partition for the input key.
 * values is an array of datums representing the partitioning key, isnull
 * tells us which of those is NULL. pSearch allows the caller to get the
 * position in the partition range where the key falls (might be hypothetical).
 */
static Oid
selectPartition1(PartitionNode *partnode, Datum *values, bool *isnull,
				 TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
				 int *pSearch,
				 PartitionNode **ppn_out)
{
	Oid			relid = InvalidOid;
	Partition  *part = partnode->part;
	PartitionNode *pn = NULL;
	PartitionRule *prule = NULL;

	if (ppn_out)
		*ppn_out = NULL;

	/* what kind of partition? */
	switch (part->parkind)
	{
		case 'r':				/* range */
			pn = selectRangePartition(partnode, values, isnull, tupdesc,
									  accessMethods, &relid, pSearch, &prule, InvalidOid);
			break;
		case 'l':				/* list */
			pn = selectListPartition(partnode, values, isnull, tupdesc,
									 accessMethods, &relid, &prule, InvalidOid);
			break;
		default:
			elog(ERROR, "unrecognized partitioning kind '%c'",
				 part->parkind);
			break;
	}

	if (pn)
	{
		if (ppn_out)
		{
			*ppn_out = pn;
			return relid;
		}
		return selectPartition1(pn, values, isnull, tupdesc, accessMethods,
								pSearch, ppn_out);
	}
	else
	{
		/* retry a default */
		if (partnode->default_part && !OidIsValid(relid))
		{
			if (partnode->default_part->children)
			{
				if (ppn_out)
				{
					*ppn_out = partnode->default_part->children;

					/*
					 * don't return the relid, it is invalid -- return the
					 * relid of the default partition instead
					 */
					return partnode->default_part->parchildrelid;
				}
				return selectPartition1(partnode->default_part->children,
										values, isnull, tupdesc, accessMethods,
										pSearch, ppn_out);
			}
			else
				return partnode->default_part->parchildrelid;
		}
		else
			return relid;
	}
}

Oid
selectPartition(PartitionNode *partnode, Datum *values, bool *isnull,
				TupleDesc tupdesc, PartitionAccessMethods *accessMethods)
{
	return selectPartition1(partnode, values, isnull, tupdesc, accessMethods,
							NULL, NULL);
}

/*
 * get_next_level_matched_partition()
 *
 * Given pdata and prules, try to find a suitable partition for the input key.
 * values is an array of datums representing the partitioning key, isnull
 * tells us which of those is NULL. It will return NULL if no part matches.
 *
 * Input parameters:
 * partnode: the PartitionNode that we search for matched parts
 * values, isnull: datum values to search for parts
 * tupdesc: TupleDesc for retrieving values
 * accessMethods: PartitionAccessMethods
 * exprTypid: the type of the datum
 *
 * return: PartitionRule of which constraints match the input key
 */
PartitionRule *
get_next_level_matched_partition(PartitionNode *partnode, Datum *values, bool *isnull,
								 TupleDesc tupdesc, PartitionAccessMethods *accessMethods,
								 Oid exprTypid)
{
	Oid			relid = InvalidOid;
	Partition  *part = partnode->part;
	PartitionRule *prule = NULL;

	/* what kind of partition? */
	switch (part->parkind)
	{
		case 'r':				/* range */
			selectRangePartition(partnode, values, isnull, tupdesc,
								 accessMethods, &relid, NULL, &prule, exprTypid);
			break;
		case 'l':				/* list */
			selectListPartition(partnode, values, isnull, tupdesc,
								accessMethods, &relid, &prule, exprTypid);
			break;
		default:
			elog(ERROR, "unrecognized partitioning kind '%c'",
				 part->parkind);
			break;
	}

	if (NULL != prule)
	{
		return prule;
	}
	/* retry a default */
	return partnode->default_part;
}

/*
 * get_part_rule
 *
 * find PartitionNode and the PartitionRule for a partition if it
 * exists.
 *
 * If bExistError is not set, just return the PgPartRule.
 * If bExistError is set, return an error message based upon
 * bMustExist.  That is, if the table does not exist and
 * bMustExist=true then return an error.  Conversely, if the table
 * *does* exist and bMustExist=false then return an error.
 *
 * If pSearch is set, return a ptr to the position where the pid
 * *might* be.  For get_part_rule1, pNode is the position to start at
 * (ie not necessarily at the top).  relname is a char string to
 * describe the current relation/partition for error messages, eg
 * 'relation "foo"' or 'partition "baz" of relation "foo"'.
 *
 */
PgPartRule *
get_part_rule1(Relation rel,
			   AlterPartitionId *pid,
			   bool bExistError,
			   bool bMustExist,
			   int *pSearch,
			   PartitionNode *pNode,
			   char *relname,
			   PartitionNode **ppNode
)
{
	char		namBuf[NAMEDATALEN];	/* the real partition name */

	/* a textual representation of the partition id (for error msgs) */
	char		partIdStr[(NAMEDATALEN * 2)];

	PgPartRule *prule = NULL;

	Oid			partrelid = InvalidOid;
	int			idrank = 0;		/* only set for range partns by rank */

	if (!pid)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("no partition id specified for \"%s\"",
						relname)));

	namBuf[0] = 0;

	/*
	 * build the partition "id string" for error messages as a partition name,
	 * value, or rank.
	 *
	 * Later on, if we discover (the partition exists) and (it has a name)
	 * then we update the partIdStr to the name
	 */
	switch (pid->idtype)
	{
		case AT_AP_IDNone:		/* no ID */
			/* should never happen */
			partIdStr[0] = 0;

			break;
		case AT_AP_IDName:		/* IDentify by Name */
			snprintf(partIdStr, sizeof(partIdStr), " \"%s\"",
					 strVal(pid->partiddef));
			snprintf(namBuf, sizeof(namBuf), "%s",
					 strVal(pid->partiddef));
			break;
		case AT_AP_IDValue:		/* IDentifier FOR Value */
			snprintf(partIdStr, sizeof(partIdStr), " for specified value");
			break;
		case AT_AP_IDRank:		/* IDentifier FOR Rank */
			{
				snprintf(partIdStr, sizeof(partIdStr), " for specified rank");

				if (IsA(pid->partiddef, Integer))
					idrank = intVal(pid->partiddef);
				else if (IsA(pid->partiddef, Float))
					idrank = floor(floatVal(pid->partiddef));
				else
					Assert(false);

				snprintf(partIdStr, sizeof(partIdStr),
						 " for rank %d",
						 idrank);
			}
			break;
		case AT_AP_ID_oid:		/* IDentifier by oid */
			snprintf(partIdStr, sizeof(partIdStr), " for oid %u",
					 *((Oid *) (pid->partiddef)));
			break;
		case AT_AP_IDDefault:	/* IDentify DEFAULT partition */
			snprintf(partIdStr, sizeof(partIdStr), " for DEFAULT");
			break;
		case AT_AP_IDRule:
			{
				PgPartRule *p = linitial((List *) pid->partiddef);

				snprintf(partIdStr, sizeof(partIdStr), "%s",
						 p->partIdStr);
				return p;
				break;
			}
		default:				/* XXX XXX */
			Assert(false);

	}

	if (bExistError && !pNode)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("%s is not partitioned",
						relname)));

	/*
	 * if id is a value or rank, get the relid of the partition if it exists
	 */
	if (pNode)
	{
		if (pid->idtype == AT_AP_IDValue)
		{
			TupleDesc	tupledesc = RelationGetDescr(rel);
			bool	   *isnull;
			PartitionNode *pNode2 = NULL;
			Datum	   *d = magic_expr_to_datum(rel, pNode,
												pid->partiddef, &isnull);

			/*
			 * MPP-4011: get right pid for FOR(value).  pass a pNode ptr down
			 * to prevent recursion in selectPartition -- we only want the
			 * top-most partition for the value in this case
			 */
			if (ppNode)
				partrelid = selectPartition1(pNode, d, isnull, tupledesc, NULL,
											 pSearch, ppNode);
			else
				partrelid = selectPartition1(pNode, d, isnull, tupledesc, NULL,
											 pSearch, &pNode2);

			/* build a string rep for the value */
			{
				ParseState *pstate = make_parsestate(NULL);
				Node	   *pval = (Node *) pid->partiddef;
				char	   *idval = NULL;

				pval = (Node *) transformExpressionList(pstate,
														(List *) pval,
														EXPR_KIND_PARTITION_EXPRESSION);

				free_parsestate(pstate);

				idval =
					deparse_expression(pval,
									   deparse_context_for(RelationGetRelationName(rel),
														   RelationGetRelid(rel)),
									   false, false);

				if (idval)
					snprintf(partIdStr, sizeof(partIdStr),
							 " for value (%s)",
							 idval);
			}

		}
		else if (pid->idtype == AT_AP_IDRank)
		{
			if (pNode->part->parkind == 'l')
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot find partition by RANK -- \"%s\" is LIST partitioned",
								relname)));

			partrelid = selectPartitionByRank(pNode, idrank);
		}
	}

	/* check thru the list of partition rules to match by relid or name */
	if (pNode)
	{
		int			rulerank = 1;

		/* set up the relid for the default partition if necessary */
		if ((pid->idtype == AT_AP_IDDefault)
			&& pNode->default_part)
			partrelid = pNode->default_part->parchildrelid;

		for (int i = 0; i < pNode->num_rules; i++)
		{
			PartitionRule *rule = pNode->rules[i];
			bool		foundit = false;

			if ((pid->idtype == AT_AP_IDValue)
				|| (pid->idtype == AT_AP_IDRank))
			{
				if ((partrelid != InvalidOid)
					&& (partrelid == rule->parchildrelid))
				{
					foundit = true;

					if (strlen(rule->parname))
					{
						snprintf(partIdStr, sizeof(partIdStr), " \"%s\"",
								 rule->parname);
						snprintf(namBuf, sizeof(namBuf), "%s",
								 rule->parname);
					}
				}
			}
			else if (pid->idtype == AT_AP_IDName)
			{
				if (0 == strcmp(rule->parname, namBuf))
					foundit = true;
			}

			if (foundit)
			{
				prule = makeNode(PgPartRule);

				prule->pNode = pNode;
				prule->topRule = rule;
				prule->topRuleRank = rulerank;	/* 1-based */
				prule->relname = relname;
				break;
			}
			rulerank++;
		}						/* end foreach */

		/* if cannot find, check default partition */
		if (!prule && pNode->default_part)
		{
			PartitionRule *rule = pNode->default_part;
			bool		foundit = false;

			if ((pid->idtype == AT_AP_IDValue)
				|| (pid->idtype == AT_AP_IDRank)
				|| (pid->idtype == AT_AP_IDDefault))
			{
				if ((partrelid != InvalidOid)
					&& (partrelid == rule->parchildrelid))
				{
					foundit = true;

					if (strlen(rule->parname))
					{
						snprintf(partIdStr, sizeof(partIdStr), " \"%s\"",
								 rule->parname);
						snprintf(namBuf, sizeof(namBuf), "%s",
								 rule->parname);
					}
				}
			}
			else if (pid->idtype == AT_AP_IDName)
			{
				if (0 == strcmp(rule->parname, namBuf))
					foundit = true;
			}

			if (foundit)
			{
				prule = makeNode(PgPartRule);

				prule->pNode = pNode;
				prule->topRule = rule;
				prule->topRuleRank = 0; /* 1-based -- 0 means no rank */
				prule->relname = relname;
			}
		}
	}							/* end if pnode */

	/*
	 * if the partition exists, set the "id string" in prule and indicate
	 * whether it is the partition name.  The ATPExec commands will notify
	 * users of the "real" name if the original specification was by value or
	 * rank
	 */
	if (prule)
	{
		prule->partIdStr = pstrdup(partIdStr);
		prule->isName = (strlen(namBuf) > 0);
	}

	if (!bExistError)
		goto L_fin_partrule;

	/* MPP-3722: complain if for(value) matches the default partition */
	if ((pid->idtype == AT_AP_IDValue)
		&& prule &&
		(prule->topRule == prule->pNode->default_part))
	{
		if (bMustExist)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("FOR expression matches DEFAULT partition%s of %s",
							prule->isName ? partIdStr : "", relname),
					 errhint("FOR expression may only specify a non-default partition in this context.")));

	}

	if (bMustExist && !prule)
	{
		switch (pid->idtype)
		{
			case AT_AP_IDNone:	/* no ID */
				/* should never happen */
				Assert(false);
				break;
			case AT_AP_IDName:	/* IDentify by Name */
			case AT_AP_IDValue: /* IDentifier FOR Value */
			case AT_AP_IDRank:	/* IDentifier FOR Rank */
			case AT_AP_ID_oid:	/* IDentifier by oid */
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("partition%s of %s does not exist",
								partIdStr, relname)));
				break;
			case AT_AP_IDDefault:	/* IDentify DEFAULT partition */
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("DEFAULT partition of %s does not exist",
								relname)));
				break;
			default:			/* XXX XXX */
				Assert(false);

		}
	}
	else if (!bMustExist && prule)
	{
		switch (pid->idtype)
		{
			case AT_AP_IDNone:	/* no ID */
				/* should never happen */
				Assert(false);
				break;
			case AT_AP_IDName:	/* IDentify by Name */
			case AT_AP_IDValue: /* IDentifier FOR Value */
			case AT_AP_IDRank:	/* IDentifier FOR Rank */
			case AT_AP_ID_oid:	/* IDentifier by oid */
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
						 errmsg("partition%s of %s already exists",
								partIdStr, relname)));
				break;
			case AT_AP_IDDefault:	/* IDentify DEFAULT partition */
				ereport(ERROR,
						(errcode(ERRCODE_DUPLICATE_OBJECT),
						 errmsg("DEFAULT partition%s of %s already exists",
								prule->isName ? partIdStr : "", relname)));
				break;
			default:			/* XXX XXX */
				Assert(false);

		}

	}

L_fin_partrule:

	return prule;
}								/* end get_part_rule1 */

PgPartRule *
get_part_rule(Relation rel,
			  AlterPartitionId *pid,
			  bool bExistError,
			  bool bMustExist,
			  int *pSearch,
			  bool inctemplate)
{
	PartitionNode *pNode = NULL;
	PartitionNode *pNode2 = NULL;
	char		relnamBuf[(NAMEDATALEN * 2)];
	char	   *relname;

	if (!pid)
		return NULL;

	snprintf(relnamBuf, sizeof(relnamBuf), "relation \"%s\"",
			 RelationGetRelationName(rel));

	relname = pstrdup(relnamBuf);

	pNode = RelationBuildPartitionDesc(rel, inctemplate);

	if (pid && ((pid->idtype != AT_AP_IDList)
				&& (pid->idtype != AT_AP_IDRule)))
		return get_part_rule1(rel,
							  pid,
							  bExistError, bMustExist,
							  pSearch, pNode, relname, NULL);

	if (pid->idtype == AT_AP_IDRule)
	{
		List	   *l1 = (List *) pid->partiddef;
		ListCell   *lc;
		AlterPartitionId *pid2 = NULL;
		PgPartRule *prule2 = NULL;

		lc = list_head(l1);
		prule2 = (PgPartRule *) lfirst(lc);
		if (prule2 && prule2->topRule && prule2->topRule->children)
			pNode = prule2->topRule->children;

		lc = lnext(lc);

		pid2 = (AlterPartitionId *) lfirst(lc);

		return get_part_rule1(rel, pid2, bExistError, bMustExist, pSearch,
							  pNode, pstrdup(prule2->relname), &pNode2);
	}

	if (pid->idtype == AT_AP_IDList)
	{
		List	   *l1 = (List *) pid->partiddef;
		ListCell   *lc;
		AlterPartitionId *pid2 = NULL;
		PgPartRule *prule2 = NULL;
		StringInfoData sid1,
					sid2;

		initStringInfo(&sid1);
		initStringInfo(&sid2);
		appendStringInfoString(&sid1, relnamBuf);

		foreach(lc, l1)
		{

			pid2 = (AlterPartitionId *) lfirst(lc);

			prule2 = get_part_rule1(rel,
									pid2,
									bExistError, bMustExist,
									pSearch, pNode, sid1.data, &pNode2);

			pNode = pNode2;

			if (!pNode)
			{
				if (prule2 && prule2->topRule && prule2->topRule->children)
					pNode = prule2->topRule->children;
			}

			appendStringInfo(&sid2, "partition%s of %s",
							 prule2->partIdStr, sid1.data);
			resetStringInfo(&sid1);
			appendStringInfo(&sid1, "%s", sid2.data);
			resetStringInfo(&sid2);
		}						/* end foreach */

		return prule2;
	}
	return NULL;
}								/* end get_part_rule */

static void
fixup_table_storage_options(CreateStmt *ct)
{
	if (!ct->options)
	{
		ct->options = list_make2(makeDefElem("appendonly",
											 (Node *) makeString("true")),
								 makeDefElem("orientation",
											 (Node *) makeString("column")));
	}
}

/*
 * The user is adding a partition and we have a subpartition template that has
 * been brought into the mix. At partition create time, if the user added any
 * storage encodings to the subpartition template, we would have cached them in
 * pg_partition_encoding. Retrieve them now, deparse and apply to the
 * PartitionSpec as if this was CREATE TABLE.
 */
static void
apply_template_storage_encodings(CreateStmt *ct, Oid relid, Oid paroid,
								 PartitionSpec *tmpl)
{
	List	   *encs = get_deparsed_partition_encodings(relid, paroid);

	if (encs)
	{
		/*
		 * If the user didn't specify WITH (...) at create time, we need to
		 * force the new partitions to be AO/CO.
		 */
		fixup_table_storage_options(ct);
		tmpl->partElem = list_concat(tmpl->partElem,
									 encs);
	}
}

/*
 * atpxPart_validate_spec: kludge up a PartitionBy statement and use
 * validate_partition_spec to transform and validate a partition
 * boundary spec.
 */

static int
atpxPart_validate_spec(PartitionBy *pBy,
					   Relation rel,
					   CreateStmt *ct,
					   PartitionElem *pelem,
					   PartitionNode *pNode,
					   char *partName,
					   bool isDefault,
					   PartitionByType part_type,
					   char *partDesc)
{
	PartitionSpec *spec = makeNode(PartitionSpec);
	List	   *schema = NIL;
	List *constraints = NIL;
	RangeVar *parent_rv = makeRangeVar(
		get_namespace_name(RelationGetNamespace(rel)),
		pstrdup(RelationGetRelationName(rel)),
		-1);
	SetSchemaAndConstraints(parent_rv, &schema, &constraints);

	int			result;
	PartitionNode *pNode_tmpl = NULL;

	spec->partElem = list_make1(pelem);

	pelem->partName = partName;
	pelem->isDefault = isDefault;
	pelem->AddPartDesc = pstrdup(partDesc);

	/* generate a random name for the partition relation if necessary */
	if (partName)
		pelem->rrand = 0;
	else
		pelem->rrand = random();

	pBy->partType = part_type;
	pBy->keys = NULL;
	pBy->subPart = NULL;
	pBy->partSpec = (Node *) spec;
	pBy->partDepth = pNode->part->parlevel;
	/* Note: pBy->partQuiet already set by caller */
	pBy->parentRel =
		makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
					 pstrdup(RelationGetRelationName(rel)), -1);
	pBy->location = -1;
	pBy->partDefault = NULL;
	pBy->bKeepMe = true;		/* nefarious: we need to keep the "top"
								 * partition by statement because
								 * analyze.c:do_parse_analyze needs to find it
								 * to re-order the ALTER statements */

	/* fixup the pnode_tmpl to get the right parlevel */
	if (pNode && (pNode->num_rules > 0 || pNode->default_part))
	{
		pNode_tmpl = get_parts(pNode->part->parrelid,
							   pNode->part->parlevel + 1,
							   InvalidOid,	/* no parent for template */
							   true,
							   true /* includesubparts */
			);
	}

	{							/* find the partitioning keys (recursively) */

		PartitionBy *pBy2 = pBy;
		PartitionBy *parent_pBy2 = NULL;
		PartitionNode *pNode2 = pNode;

		int			ii;
		TupleDesc	tupleDesc = RelationGetDescr(rel);
		List	   *pbykeys = NIL;
		List	   *pbyopclass = NIL;
		Oid			accessMethodId = BTREE_AM_OID;

		while (pNode2)
		{
			pbykeys = NIL;
			pbyopclass = NIL;

			for (ii = 0; ii < pNode2->part->parnatts; ii++)
			{
				AttrNumber	attno =
				pNode2->part->paratts[ii];
				Form_pg_attribute attribute =
				tupleDesc->attrs[attno - 1];
				char	   *attributeName =
				NameStr(attribute->attname);
				Oid			opclass =
				InvalidOid;

				opclass =
					GetDefaultOpClass(attribute->atttypid, accessMethodId);

				if (pbykeys)
				{
					pbykeys = lappend(pbykeys, makeString(attributeName));
					pbyopclass = lappend_oid(pbyopclass, opclass);
				}
				else
				{
					pbykeys = list_make1(makeString(attributeName));
					pbyopclass = list_make1_oid(opclass);
				}
			}					/* end for */

			pBy2->keys = pbykeys;
			pBy2->keyopclass = pbyopclass;

			if (parent_pBy2)
				parent_pBy2->subPart = (Node *) pBy2;

			parent_pBy2 = pBy2;

			if (pNode2 && (pNode2->num_rules > 0 || pNode2->default_part))
			{
				PartitionRule *prule;
				PartitionElem *el = NULL;	/* for the subpartn template */

				if (pNode2->default_part)
					prule = pNode2->default_part;
				else
					prule = pNode2->rules[0];

				if (prule && prule->children)
				{
					pNode2 = prule->children;

					Assert(('l' == pNode2->part->parkind) ||
						   ('r' == pNode2->part->parkind));

					pBy2 = makeNode(PartitionBy);
					pBy2->partType = char_to_parttype(pNode2->part->parkind);
					pBy2->keys = NULL;
					pBy2->subPart = NULL;
					pBy2->partSpec = NULL;
					pBy2->partDepth = pNode2->part->parlevel;
					pBy2->partQuiet = pBy->partQuiet;
					pBy2->parentRel =
						makeRangeVar(
									 get_namespace_name(
														RelationGetNamespace(rel)),
									 pstrdup(RelationGetRelationName(rel)), -1);
					pBy2->location = -1;
					pBy2->partDefault = NULL;

					el = NULL;

					/* build the template (if it exists) */
					if (pNode_tmpl)
					{
						PartitionSpec *spec_tmpl = makeNode(PartitionSpec);

						spec_tmpl->istemplate = true;

						/* add entries for rules at current level */
						for (int i = 0; i < pNode_tmpl->num_rules; i++)
						{
							PartitionRule *rule_tmpl = pNode_tmpl->rules[i];

							el = makeNode(PartitionElem);

							if (rule_tmpl->parname && strlen(rule_tmpl->parname) > 0)
								el->partName = rule_tmpl->parname;

							el->isDefault = rule_tmpl->parisdefault;

							/* MPP-6904: use storage options from template */
							if (rule_tmpl->parreloptions ||
								rule_tmpl->partemplatespaceId)
							{
								Node	   *tspaceName = NULL;
								AlterPartitionCmd *apc =
								makeNode(AlterPartitionCmd);

								el->storeAttr = (Node *) apc;

								if (rule_tmpl->partemplatespaceId)
									tspaceName =
										(Node *) makeString(
															get_tablespace_name(
																				rule_tmpl->partemplatespaceId
																				));

								apc->partid = NULL;
								apc->arg2 = tspaceName;
								apc->arg1 = (Node *) rule_tmpl->parreloptions;

							}

							/* LIST */
							if (rule_tmpl->parlistvalues)
							{
								PartitionValuesSpec *vspec =
								makeNode(PartitionValuesSpec);

								el->boundSpec = (Node *) vspec;

								vspec->partValues = rule_tmpl->parlistvalues;
							}

							/* RANGE */
							if (rule_tmpl->parrangestart ||
								rule_tmpl->parrangeend)
							{
								PartitionBoundSpec *bspec =
								makeNode(PartitionBoundSpec);
								PartitionRangeItem *ri;

								if (rule_tmpl->parrangestart)
								{
									ri =
										makeNode(PartitionRangeItem);

									ri->partedge =
										rule_tmpl->parrangestartincl ?
										PART_EDGE_INCLUSIVE :
										PART_EDGE_EXCLUSIVE;

									ri->partRangeVal =
										(List *) rule_tmpl->parrangestart;

									bspec->partStart = (Node *) ri;

								}
								if (rule_tmpl->parrangeend)
								{
									ri =
										makeNode(PartitionRangeItem);

									ri->partedge =
										rule_tmpl->parrangeendincl ?
										PART_EDGE_INCLUSIVE :
										PART_EDGE_EXCLUSIVE;

									ri->partRangeVal =
										(List *) rule_tmpl->parrangeend;

									bspec->partEnd = (Node *) ri;

								}
								if (rule_tmpl->parrangeevery)
								{
									ri =
										makeNode(PartitionRangeItem);

									ri->partRangeVal =
										(List *) rule_tmpl->parrangeevery;

									bspec->partEvery = (Node *) ri;

								}

								el->boundSpec = (Node *) bspec;

							}	/* end if RANGE */

							spec_tmpl->partElem = lappend(spec_tmpl->partElem,
														  el);
						}		/* end foreach */

						/* MPP-4725 */
						/* and the default partition */
						if (pNode_tmpl->default_part)
						{
							PartitionRule *rule_tmpl =
							pNode_tmpl->default_part;

							el = makeNode(PartitionElem);

							if (rule_tmpl->parname && strlen(rule_tmpl->parname) > 0)
								el->partName = rule_tmpl->parname;

							el->isDefault = rule_tmpl->parisdefault;

							spec_tmpl->partElem = lappend(spec_tmpl->partElem,
														  el);

						}

						/* apply storage encoding for this template */
						apply_template_storage_encodings(ct,
														 RelationGetRelid(rel),
														 pNode_tmpl->part->partid,
														 spec_tmpl);

						/*
						 * the PartitionElem should hang off the pby partspec,
						 * and subsequent templates should hang off the
						 * subspec for the prior PartitionElem.
						 */

						pBy2->partSpec = (Node *) spec_tmpl;

					}			/* end if pNode_tmpl */

					/* fixup the pnode_tmpl to get the right parlevel */
					if (pNode2 && (pNode2->num_rules > 0 || pNode2->default_part))
					{
						pNode_tmpl = get_parts(pNode2->part->parrelid,
											   pNode2->part->parlevel + 1,
											   InvalidOid,	/* no parent for
															 * template */
											   true,
											   true /* includesubparts */
							);
					}

				}
				else
					pNode2 = NULL;
			}
			else
				pNode2 = NULL;

		}						/* end while */
	}

	CreateStmtContext cxt;

	MemSet(&cxt, 0, sizeof(cxt));

	cxt.pstate = make_parsestate(NULL);
	cxt.columns = schema;

	result = validate_partition_spec(&cxt, ct, pBy, "", -1);
	free_parsestate(cxt.pstate);

	return result;
}								/* end atpxPart_validate_spec */

Node *
atpxPartAddList(Relation rel,
				bool is_split,
				List *colencs,
				PartitionNode *pNode,
				char *partName, /* pid->partiddef (or NULL) */
				bool isDefault,
				PartitionElem *pelem,
				PartitionByType part_type,
				PgPartRule *par_prule,
				char *lrelname,
				bool bSetTemplate,
				Oid ownerid)
{
	DestReceiver *dest = None_Receiver;
	int			maxpartno = 0;
	typedef enum
	{
		FIRST = 0,				/* New partition lies before first. */
		MIDDLE,					/* New partition lies in the middle. */
		LAST					/* New partition lies after last. */
	} NewPosition;
	NewPosition newPos = MIDDLE;
	bool		bOpenGap = false;
	PartitionBy *pBy;
	Node	   *pSubSpec = NULL;	/* return the subpartition spec */
	Relation	par_rel = rel;
	PartitionNode pNodebuf;
	PartitionNode *pNode2 = &pNodebuf;
	CreateStmt *ct;

	/* get the relation for the parent of the new partition */
	if (par_prule && par_prule->topRule)
		par_rel =
			heap_open(par_prule->topRule->parchildrelid, AccessShareLock);

	Assert((PARTTYP_LIST == part_type) || (PARTTYP_RANGE == part_type));

	/* XXX XXX: handle case of missing boundary spec for range with EVERY */

	if (pelem && pelem->boundSpec)
	{
		if (PARTTYP_RANGE == part_type)
		{
			PartitionBoundSpec *pbs = NULL;
			PgPartRule *prule = NULL;
			AlterPartitionId pid;
			ParseState *pstate = make_parsestate(NULL);
			TupleDesc	tupledesc = RelationGetDescr(rel);

			MemSet(&pid, 0, sizeof(AlterPartitionId));

			pid.idtype = AT_AP_IDRank;
			pid.location = -1;

			Assert(IsA(pelem->boundSpec, PartitionBoundSpec));

			pbs = (PartitionBoundSpec *) pelem->boundSpec;
			pSubSpec = pelem->subSpec;	/* look for subpartition spec */

			/* no EVERY */
			if (pbs->partEvery)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("cannot specify EVERY when adding RANGE partition to %s",
								lrelname)));

			if (!(pbs->partStart || pbs->partEnd))
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("need START or END when adding RANGE partition to %s",
								lrelname)));

			/* if no START, then START after last partition */
			if (!(pbs->partStart))
			{
				Datum	   *d_end = NULL;
				bool	   *isnull;
				bool		bstat;

				pid.partiddef = (Node *) makeInteger(-1);

				prule = get_part_rule1(rel, &pid, false, false,
									   NULL,
									   pNode,
									   lrelname,
									   &pNode2);

				/*
				 * ok if no prior -- just means this is first partition (XXX
				 * XXX though should always have 1 partition in the table...)
				 */

				if (!(prule && prule->topRule))
				{
					maxpartno = 1;
					bOpenGap = true;
					goto L_fin_no_start;
				}

				{
					Node	   *n1;

					if (!IsA(pbs->partEnd, PartitionRangeItem))
					{
						/*
						 * pbs->partEnd isn't a PartitionRangeItem!  This
						 * probably means an invalid split of a default part,
						 * but we aren't really sure. See MPP-14613.
						 */
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("invalid partition range specification")));
					}

					PartitionRangeItem *ri =
					(PartitionRangeItem *) pbs->partEnd;

					PartitionRangeItemIsValid(NULL, ri);

					n1 = (Node *) copyObject(ri->partRangeVal);
					n1 = (Node *) transformExpressionList(pstate,
														  (List *) n1,
														  EXPR_KIND_PARTITION_EXPRESSION);

					d_end =
						magic_expr_to_datum(rel, pNode,
											n1, &isnull);
				}

				if (prule && prule->topRule && prule->topRule->parrangeend
					&& list_length((List *) prule->topRule->parrangeend))
				{
					bstat =
						compare_partn_opfuncid(pNode,
											   "pg_catalog",
											   "<",
											   (List *) prule->topRule->parrangeend,
											   d_end, isnull, tupledesc);

					/*
					 * if the current end is less than the new end then use it
					 * as the start of the new partition
					 */

					if (bstat)
					{
						PartitionRangeItem *ri = makeNode(PartitionRangeItem);

						ri->location = -1;

						ri->partRangeVal =
							copyObject(prule->topRule->parrangeend);

						/* invert the inclusive/exclusive */
						ri->partedge = prule->topRule->parrangeendincl ?
							PART_EDGE_EXCLUSIVE :
							PART_EDGE_INCLUSIVE;

						/* should be final partition */
						maxpartno = prule->topRule->parruleord + 1;
						newPos = LAST;
						pbs->partStart = (Node *) ri;
						goto L_fin_no_start;
					}
				}

				/*
				 * if the last partition doesn't have an end, or the end isn't
				 * less than the new end, check if new end is less than
				 * current start
				 */

				pid.partiddef = (Node *) makeInteger(1);

				prule = get_part_rule1(rel, &pid, false, false,
									   NULL,
									   pNode,
									   lrelname,
									   &pNode2);

				if (!(prule && prule->topRule && prule->topRule->parrangestart
					  && list_length((List *) prule->topRule->parrangestart)))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("new partition overlaps existing "
									"partition")));

				bstat =
					compare_partn_opfuncid(pNode,
										   "pg_catalog",
										   ">",
										   (List *) prule->topRule->parrangestart,
										   d_end, isnull, tupledesc);


				if (!bstat)
				{
					/*
					 * As we support explicit inclusive and exclusive ranges
					 * we need to be even more careful.
					 *
					 * We can proceed if we have the following:
					 *
					 * END (R) EXCLUSIVE ; START (R) INCLUSIVE END (R)
					 * INCLUSIVE ; START (R) EXCLUSIVE
					 *
					 * XXX: this should be refactored into a single generic
					 * function that can be used here and in the unbounded end
					 * case, checked further down. That said, a lot of this
					 * code should be refactored.
					 */
					PartitionRangeItem *ri = (PartitionRangeItem *) pbs->partEnd;

					if ((ri->partedge == PART_EDGE_EXCLUSIVE &&
						 prule->topRule->parrangestartincl) ||
						(ri->partedge == PART_EDGE_INCLUSIVE &&
						 !prule->topRule->parrangestartincl))
					{
						bstat = compare_partn_opfuncid(pNode, "pg_catalog", "=",
													   (List *) prule->topRule->parrangestart,
													   d_end, isnull, tupledesc);
					}

				}

				if (bstat)
				{
					/* should be first partition */
					maxpartno = prule->topRule->parruleord - 1;
					if (0 == maxpartno)
					{
						maxpartno = 1;
						bOpenGap = true;
					}
					newPos = FIRST;
				}
				else
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("new partition overlaps existing "
									"partition")));

		L_fin_no_start:
				bstat = false;	/* fix warning */

			}
			else if (!(pbs->partEnd))
			{					/* if no END, then END before first partition
								 * *ONLY IF** START of this partition is
								 * before first partition ... */

				Datum	   *d_start = NULL;
				bool	   *isnull;
				bool		bstat;

				pid.partiddef = (Node *) makeInteger(1);

				prule = get_part_rule1(rel, &pid, false, false,
									   NULL,
									   pNode,
									   lrelname,
									   &pNode2);

				/* NOTE: invert all the logic of case of missing partStart */

				/*
				 * ok if no successor [?] -- just means this is first
				 * partition (XXX XXX though should always have 1 partition in
				 * the table... [XXX XXX unless did a SPLIT of a single
				 * partition !! ])
				 */

				if (!(prule && prule->topRule))
				{
					maxpartno = 1;
					bOpenGap = true;
					goto L_fin_no_end;
				}

				{
					Node	   *n1;
					PartitionRangeItem *ri =
					(PartitionRangeItem *) pbs->partStart;

					PartitionRangeItemIsValid(NULL, ri);
					n1 = (Node *) copyObject(ri->partRangeVal);
					n1 = (Node *) transformExpressionList(pstate,
														  (List *) n1,
														  EXPR_KIND_PARTITION_EXPRESSION);

					d_start =
						magic_expr_to_datum(rel, pNode,
											n1, &isnull);
				}


				if (prule && prule->topRule && prule->topRule->parrangestart
					&& list_length((List *) prule->topRule->parrangestart))
				{
					bstat =
						compare_partn_opfuncid(pNode,
											   "pg_catalog",
											   ">",
											   (List *) prule->topRule->parrangestart,
											   d_start, isnull, tupledesc);

					/*
					 * if the current start is greater than the new start then
					 * use the current start as the end of the new partition
					 */

					if (bstat)
					{
						PartitionRangeItem *ri = makeNode(PartitionRangeItem);

						ri->location = -1;

						ri->partRangeVal =
							copyObject(prule->topRule->parrangestart);

						/* invert the inclusive/exclusive */
						ri->partedge = prule->topRule->parrangestartincl ?
							PART_EDGE_EXCLUSIVE :
							PART_EDGE_INCLUSIVE;

						/* should be first partition */
						maxpartno = prule->topRule->parruleord - 1;
						if (0 == maxpartno)
						{
							maxpartno = 1;
							bOpenGap = true;
						}
						newPos = FIRST;
						pbs->partEnd = (Node *) ri;
						goto L_fin_no_end;
					}
				}

				/*
				 * if the first partition doesn't have an start, or the start
				 * isn't greater than the new start, check if new start is
				 * greater than current end
				 */

				pid.partiddef = (Node *) makeInteger(-1);

				prule = get_part_rule1(rel, &pid, false, false,
									   NULL,
									   pNode,
									   lrelname,
									   &pNode2);

				if (!(prule && prule->topRule && prule->topRule->parrangeend
					  && list_length((List *) prule->topRule->parrangeend)))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("new partition overlaps existing "
									"partition")));

				bstat =
					compare_partn_opfuncid(pNode,
										   "pg_catalog",
										   "<",
										   (List *) prule->topRule->parrangeend,
										   d_start, isnull, tupledesc);
				if (bstat)
				{
					/* should be final partition */
					maxpartno = prule->topRule->parruleord + 1;
					newPos = LAST;
				}
				else
				{
					PartitionRangeItem *ri =
					(PartitionRangeItem *) pbs->partStart;

					/* check for equality */
					bstat =
						compare_partn_opfuncid(pNode,
											   "pg_catalog",
											   "=",
											   (List *) prule->topRule->parrangeend,
											   d_start, isnull, tupledesc);

					/*
					 * if new start not >= to current end, then new start <
					 * current end, so it overlaps. Or if new start == current
					 * end, but the inclusivity is not opposite for the
					 * boundaries (eg inclusive end abuts inclusive start for
					 * same start/end value) then it overlaps
					 */
					if (!bstat ||
						(bstat &&
						 (prule->topRule->parrangeendincl ==
						  (ri->partedge == PART_EDGE_INCLUSIVE)))
						)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
								 errmsg("new partition overlaps existing "
										"partition")));

					/* doesn't overlap, should be final partition */
					maxpartno = prule->topRule->parruleord + 1;

				}
		L_fin_no_end:
				bstat = false;	/* fix warning */

			}
			else
			{					/* both start and end are specified */
				PartitionRangeItem *ri;
				bool		bOverlap = false;
				bool	   *isnull;
				int			startSearchpoint;
				int			endSearchpoint;
				Datum	   *d_start = NULL;
				Datum	   *d_end = NULL;

				/* see if start or end overlaps */
				pid.idtype = AT_AP_IDValue;

				/* check the start */

				ri = (PartitionRangeItem *) pbs->partStart;
				PartitionRangeItemIsValid(NULL, ri);

				pid.partiddef = (Node *) copyObject(ri->partRangeVal);
				pid.partiddef =
					(Node *) transformExpressionList(pstate,
													 (List *) pid.partiddef,
													 EXPR_KIND_PARTITION_EXPRESSION);

				prule = get_part_rule1(rel, &pid, false, false,
									   &startSearchpoint,
									   pNode,
									   lrelname,
									   &pNode2);

				/* found match for start value in rules */
				if (prule && !(prule->topRule->parisdefault && is_split))
				{
					bool		bstat;
					PartitionRule *a_rule = prule->topRule;

					d_start =
						magic_expr_to_datum(rel, pNode,
											pid.partiddef, &isnull);

					/*
					 * if start value was inclusive then it definitely
					 * overlaps
					 */
					if (ri->partedge == PART_EDGE_INCLUSIVE)
					{
						bOverlap = true;
						goto L_end_overlap;
					}

					/*
					 * not inclusive -- check harder if START really overlaps
					 */

					if (0 ==
						list_length((List *) a_rule->parrangeend))
					{
						/* infinite end > new start - overlap */
						bOverlap = true;
						goto L_end_overlap;
					}

					bstat =
						compare_partn_opfuncid(pNode,
											   "pg_catalog",
											   ">",
											   (List *) a_rule->parrangeend,
											   d_start, isnull, tupledesc);
					if (bstat)
					{
						/* end > new start - overlap */
						bOverlap = true;
						goto L_end_overlap;
					}

					/*
					 * Must be the case that new start == end of a_rule
					 * (because if the end < new start then how could we find
					 * it in the interval for prule ?) This is ok if they have
					 * opposite INCLUSIVE/EXCLUSIVE ->  New partition does not
					 * overlap.
					 */

					Assert(compare_partn_opfuncid(pNode,
												  "pg_catalog",
												  "=",
												  (List *) a_rule->parrangeend,
												  d_start, isnull, tupledesc));

					if (a_rule->parrangeendincl ==
						(ri->partedge == PART_EDGE_INCLUSIVE))
					{
						/*
						 * start and end must be of opposite types, else they
						 * overlap
						 */
						bOverlap = true;
						goto L_end_overlap;
					}

					/*
					 * opposite inclusive/exclusive, so in middle of range of
					 * existing partitions
					 */
					newPos = MIDDLE;
					goto L_check_end;
				}				/* end if prule */

				/* check for basic case of START > last partition */
				if (pNode && pNode->rules && pNode->num_rules > 0)
				{
					bool		bstat;
					PartitionRule *a_rule = /* get last rule */
						pNode->rules[pNode->num_rules - 1];

					d_start =
						magic_expr_to_datum(rel, pNode,
											pid.partiddef, &isnull);

					if (0 ==
						list_length((List *) a_rule->parrangeend))
					{
						/* infinite end > new start */
						bstat = false;
					}
					else
						bstat =
							compare_partn_opfuncid(pNode,
												   "pg_catalog",
												   "<",
												   (List *) a_rule->parrangeend,
												   d_start, isnull, tupledesc);

					/*
					 * if the new partition start > end of the last partition
					 * then it is the new final partition. Don't bother
					 * checking the new end for overlap (just check if end >
					 * start in validation phase
					 */
					if (bstat)
					{
						newPos = LAST;
						/* should be final partition */
						maxpartno = a_rule->parruleord + 1;

						goto L_end_overlap;
					}

					/*
					 * could be the case that new start == end of last.  This
					 * is ok if they have opposite INCLUSIVE/EXCLUSIVE.  New
					 * partition is still final partition for this case
					 */

					if (0 ==
						list_length((List *) a_rule->parrangeend))
					{
						/* infinite end > new start */
						bstat = false;
					}
					else
						bstat =
							compare_partn_opfuncid(pNode,
												   "pg_catalog",
												   "=",
												   (List *) a_rule->parrangeend,
												   d_start, isnull, tupledesc);
					if (bstat)
					{
						if (a_rule->parrangeendincl ==
							(ri->partedge == PART_EDGE_INCLUSIVE))
						{
							/*
							 * start and end must be of opposite types, else
							 * they overlap
							 */
							bOverlap = true;
							goto L_end_overlap;
						}

						newPos = LAST;
						/* should be final partition */
						maxpartno = a_rule->parruleord + 1;

						goto L_end_overlap;
					}
					else
					{
						/*
						 * tricky case: the new start is less than the end of
						 * the final partition, but it does not intersect any
						 * existing partitions.  So we are trying to add a
						 * partition in the middle of the existing partitions
						 * or before the first partition.
						 */
						a_rule = pNode->rules[0]; 	/* get first rule */

						if (0 ==
							list_length((List *) a_rule->parrangestart))
						{
							/* new start > negative infinite start */
							bstat = false;
						}
						else
							bstat =
								compare_partn_opfuncid(pNode,
													   "pg_catalog",
													   ">",
													   (List *) a_rule->parrangestart,
													   d_start, isnull, tupledesc);

						/*
						 * if the new partition start < start of the first
						 * partition then it is the new first partition.
						 * Check the new end for overlap.
						 *
						 * NOTE: ignore the case where new start == 1st start
						 * and inclusive vs exclusive because that is just
						 * stupid.
						 *
						 */
						if (bstat)
						{
							newPos = FIRST;
							/* should be first partition */
							maxpartno = a_rule->parruleord - 1;
							if (0 == maxpartno)
							{
								maxpartno = 1;
								bOpenGap = true;
							}

						}
						else
						{
							newPos = MIDDLE;
						}
					}
				}
				else
				{
					/* if no "rules", then this is the first partition */

					newPos = LAST;
					/* should be final partition */
					maxpartno = 1;

					goto L_end_overlap;
				}

		L_check_end:
				/* check the end */

				/*
				 * check for basic case of END < first partition (the opposite
				 * of START > last partition)
				 */

				ri = (PartitionRangeItem *) pbs->partEnd;
				PartitionRangeItemIsValid(NULL, ri);

				pid.partiddef = (Node *) copyObject(ri->partRangeVal);
				pid.partiddef =
					(Node *) transformExpressionList(pstate,
													 (List *) pid.partiddef,
													 EXPR_KIND_PARTITION_EXPRESSION);

				prule = get_part_rule1(rel, &pid, false, false,
									   &endSearchpoint,
									   pNode,
									   lrelname,
									   &pNode2);

				/* found match for end value in rules */
				if (prule && !(prule->topRule->parisdefault &&
							   is_split))
				{
					bool		bstat;
					PartitionRule *a_rule = prule->topRule;

					d_end =
						magic_expr_to_datum(rel, pNode,
											pid.partiddef, &isnull);

					/*
					 * if end value was inclusive then it definitely overlaps
					 */
					if (ri->partedge == PART_EDGE_INCLUSIVE)
					{
						bOverlap = true;
						goto L_end_overlap;
					}

					/*
					 * not inclusive -- check harder if END really overlaps
					 */
					if (0 ==
						list_length((List *) a_rule->parrangestart))
					{
						/* -infinite start < new end - overlap */
						bOverlap = true;
						goto L_end_overlap;
					}

					bstat =
						compare_partn_opfuncid(pNode,
											   "pg_catalog",
											   "<",
											   (List *) a_rule->parrangestart,
											   d_end, isnull, tupledesc);
					if (bstat)
					{
						/* start < new end - overlap */
						bOverlap = true;
						goto L_end_overlap;
					}

					/*
					 * Must be the case that new end = start of a_rule
					 * (because if the start > new end then how could we find
					 * it in the interval for prule ?) This is ok if they have
					 * opposite INCLUSIVE/EXCLUSIVE ->  New partition does not
					 * overlap.
					 */

					Assert(compare_partn_opfuncid(pNode,
												  "pg_catalog",
												  "=",
												  (List *) a_rule->parrangestart,
												  d_end, isnull, tupledesc));

					if (a_rule->parrangestartincl ==
						(ri->partedge == PART_EDGE_INCLUSIVE))
					{
						/*
						 * start and end must be of opposite types, else they
						 * overlap
						 */
						bOverlap = true;
						goto L_end_overlap;
					}
				}				/* end if prule */

				/* check for case of END < first partition */
				if (pNode && pNode->rules && pNode->num_rules > 0)
				{
					bool		bstat;
					PartitionRule *a_rule = pNode->rules[0];	/* get first rule */

					d_end =
						magic_expr_to_datum(rel, pNode,
											pid.partiddef, &isnull);

					if (0 ==
						list_length((List *) a_rule->parrangestart))
					{
						/* new end > negative infinite start */
						bstat = false;
					}
					else
						bstat =
							compare_partn_opfuncid(pNode,
												   "pg_catalog",
												   ">",
												   (List *) a_rule->parrangestart,
												   d_end, isnull, tupledesc);

					/*
					 * if the new partition end < start of the first partition
					 * then it is the new first partition.
					 */
					if (bstat)
					{
						/* check if start was also ok for first partition */
						switch (newPos)
						{
							case FIRST:

								/*
								 * since new start < first start and new end <
								 * first start should be first.
								 */

								/* should be first partition */
								maxpartno = a_rule->parruleord - 1;
								if (0 == maxpartno)
								{
									maxpartno = 1;
									bOpenGap = true;
								}

								break;
							case MIDDLE:
							case LAST:
							default:

								/*
								 * new end is less than first partition start
								 * but new start isn't -- must be end < start
								 */
								break;
						}
						goto L_end_overlap;
					}

					/*
					 * could be the case that new end == start of first.  This
					 * is ok if they have opposite INCLUSIVE/EXCLUSIVE.  New
					 * partition is still first partition for this case
					 */

					if (0 ==
						list_length((List *) a_rule->parrangestart))
					{
						/* new end > negative infinite start */
						bstat = false;
					}
					else
						bstat =
							compare_partn_opfuncid(pNode,
												   "pg_catalog",
												   "=",
												   (List *) a_rule->parrangestart,
												   d_end, isnull, tupledesc);
					if (bstat)
					{
						if (a_rule->parrangestartincl ==
							(ri->partedge == PART_EDGE_INCLUSIVE))
						{
							/*
							 * start and end must be of opposite types, else
							 * they overlap
							 */
							bOverlap = true;
							goto L_end_overlap;
						}
						/* check if start was also ok for first partition */
						switch (newPos)
						{
							case FIRST:

								/*
								 * since new start < first start and new end <
								 * first start should be first.
								 */

								/* should be first partition */
								maxpartno = a_rule->parruleord - 1;
								if (0 == maxpartno)
								{
									maxpartno = 1;
									bOpenGap = true;
								}

								break;
							case MIDDLE:
							case LAST:
							default:

								/*
								 * new end is less than first partition start
								 * but new start isn't -- must be end < start
								 */
								break;
						}
						goto L_end_overlap;
					}
					else
					{
						/*
						 * tricky case: the new end is greater than the start
						 * of the first partition, but it does not intersect
						 * any existing partitions.  So we are trying to add a
						 * partition in the middle of the existing partitions
						 * or after the last partition.
						 */
						a_rule = pNode->rules[pNode->num_rules - 1];	/* get last rule */
						if (0 ==
							list_length((List *) a_rule->parrangeend))
						{
							/* new end < infinite end */
							bstat = false;
						}
						else

							bstat =
								compare_partn_opfuncid(pNode,
													   "pg_catalog",
													   "<",
													   (List *) a_rule->parrangeend,
													   d_end, isnull, tupledesc);

						/*
						 * if the new partition end > end of the last
						 * partition then it is the new last partition (maybe)
						 *
						 * NOTE: ignore the case where new end == last end and
						 * inclusive vs exclusive because that is just stupid.
						 *
						 */
						if (bstat)
						{
							switch (newPos)
							{
								case LAST:

									/*
									 * since new start > last end and new end
									 * > last end should be last.
									 */

									/* should be last partition */
									maxpartno = a_rule->parruleord + 1;

									break;
								case FIRST:

									/*
									 * since new start < first start and new
									 * end > last end we would overlap all
									 * partitions!!!
									 */
								case MIDDLE:

									/*
									 * since new start < last end and new end
									 * > last end we would overlap last
									 * partition
									 */
									bOverlap = true;
									goto L_end_overlap;
									break;
								default:

									/*
									 * new end is less than last partition end
									 * but new start isn't -- must be end <
									 * start
									 */
									break;
							}
						}
						else
						{
							switch (newPos)
							{
								case FIRST:

									/*
									 * since new start < first start and new
									 * end in middle we overlap
									 */
									bOverlap = true;
									goto L_end_overlap;

									break;
								case MIDDLE:
									/* both start and end in middle */
									break;
								case LAST:
								default:

									/*
									 * since new start > last end and new end
									 * in middle -- must be end < start
									 */
									break;
							}
						}
					}
				}
				else
				{
					/* if no "rules", then this is the first partition */

					newPos = LAST;
					/* should be final partition */
					maxpartno = 1;

					goto L_end_overlap;
				}


				/*
				 * if the individual start and end values don't intersect an
				 * existing partition, make sure they don't define a range
				 * which contains an existing partition, ie new start <
				 * existing start and new end > existing end
				 */

				if (!bOverlap && (newPos == MIDDLE))
				{
					bOpenGap = true;
					int			prev_partno = 0;

					/*
					 * hmm, not always true.  see MPP-3667, MPP-3636, MPP-3593
					 *
					 * if (startSearchpoint != endSearchpoint) { bOverlap =
					 * true; goto L_end_overlap; }
					 *
					 */
					while (1)
					{
						bool		bstat;
						PartitionRule *a_rule = pNode->rules[startSearchpoint];	/* get the rule */

						/* MPP-3621: fix ADD for open intervals */

						if (0 ==
							list_length((List *) a_rule->parrangeend))
						{
							/* new end < infinite end */
							bstat = false;
						}
						else
							bstat =
								compare_partn_opfuncid(pNode,
													   "pg_catalog",
													   "<=",
													   (List *) a_rule->parrangeend,
													   d_start, isnull, tupledesc);

						if (bstat)
						{
							startSearchpoint++;
							Assert(startSearchpoint <= pNode->num_rules);
							prev_partno = a_rule->parruleord;
							continue;
						}

						/*
						 * if previous partition was less than current, then
						 * this one should be larger. if not, then it
						 * overlaps...
						 */
						if (
							(0 ==
							 list_length((List *) a_rule->parrangestart))
							||
							!compare_partn_opfuncid(pNode,
													"pg_catalog",
													">=",
													(List *) a_rule->parrangestart,
													d_end, isnull, tupledesc))
						{
							prule = NULL;	/* could get the right prule... */
							bOverlap = true;
							goto L_end_overlap;
						}

						if (prev_partno != 0 && prev_partno + 1 < a_rule->parruleord)
						{
							/* Found gap.  No need to open it. */
							bOpenGap = false;
							maxpartno = prev_partno + 1;
						}
						else
							/* shift a_rule up so new rule has a place to fit */
							maxpartno = a_rule->parruleord;

						break;
					}			/* end while */

				}				/* end 0 == middle */

		L_end_overlap:
				if (bOverlap)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
							 errmsg("new partition overlaps existing "
									"partition%s",
									(prule && prule->isName) ?
									prule->partIdStr : "")));

			}

			free_parsestate(pstate);
		}						/* end if parttype_range */
	}							/* end if pelem && pelem->boundspec */

	/*
	 * Create a phony CREATE TABLE statement for the parent table. The
	 * parse_analyze call later expands it, and we extract just the
	 * constituent commands we need to create the new partition, and ignore
	 * the commands for the already-existing parent table
	 */
	ct = makeNode(CreateStmt);
	ct->relation = makeRangeVar(get_namespace_name(RelationGetNamespace(par_rel)),
								RelationGetRelationName(par_rel), -1);
	ct->relation->relpersistence = rel->rd_rel->relpersistence;

	/*
	 * in analyze.c, fill in tableelts with a list of TableLikeClause of the
	 * partition parent table, and fill in TableLikeClauses with copy of rangevar
	 * for parent table
	 */
	TableLikeClause *tlc = makeNode(TableLikeClause);

	tlc->relation = copyObject(ct->relation);
	tlc->options = CREATE_TABLE_LIKE_DEFAULTS
		| CREATE_TABLE_LIKE_CONSTRAINTS
		| CREATE_TABLE_LIKE_INDEXES;

	/*
	 * fill in remaining fields from parse time (gram.y): the new partition is
	 * LIKE the parent and it inherits from it
	 */
	ct->tableElts = lappend(ct->tableElts, tlc);
	ct->constraints = NIL;

	if (pelem->storeAttr)
		ct->options = (List *) ((AlterPartitionCmd *) pelem->storeAttr)->arg1;

	ct->tableElts = list_concat(ct->tableElts, list_copy(colencs));

	ct->oncommit = ONCOMMIT_NOOP;
	if (pelem->storeAttr && ((AlterPartitionCmd *) pelem->storeAttr)->arg2)
		ct->tablespacename = strVal(((AlterPartitionCmd *) pelem->storeAttr)->arg2);
	else
		ct->tablespacename = NULL;

	pBy = makeNode(PartitionBy);
	if (pelem->subSpec)			/* treat subspec as partition by... */
	{
		pBy->partSpec = pelem->subSpec;
		pBy->partDepth = 0;
		pBy->partQuiet = PART_VERBO_NODISTRO;
		pBy->location = -1;
		pBy->partDefault = NULL;
		pBy->parentRel = copyObject(ct->relation);
	}
	else if (bSetTemplate)
	{
		/* if creating a template, silence partition name messages */
		pBy->partQuiet = PART_VERBO_NOPARTNAME;
	}
	else
	{
		/* just silence distribution policy messages */
		pBy->partQuiet = PART_VERBO_NODISTRO;
	}

	ct->distributedBy = NULL;
	ct->partitionBy = (Node *) pBy;
	ct->relKind = RELKIND_RELATION;

	ct->is_add_part = true;		/* subroutines need to know this */
	ct->ownerid = ownerid;

	if (!ct->distributedBy)
		ct->distributedBy = make_distributedby_for_rel(rel);

	/* for ADD PARTITION or SPLIT PARTITION, there should be no case where the rel is temporary */
	Assert(rel->rd_rel->relpersistence != RELPERSISTENCE_TEMP);
	/* this function does transformExpr on the boundary specs */
	(void) atpxPart_validate_spec(pBy, rel, ct, pelem, pNode, partName,
								  isDefault, part_type, "");

	if (pelem && pelem->boundSpec)
	{
		if (PARTTYP_LIST == part_type)
		{
			ListCell   *lc;
			PartitionValuesSpec *spec;
			AlterPartitionId pid;

			Assert(IsA(pelem->boundSpec, PartitionValuesSpec));

			MemSet(&pid, 0, sizeof(AlterPartitionId));

			spec = (PartitionValuesSpec *) pelem->boundSpec;

			/* only check this if we aren't doing split */
			if (1)
			{
				foreach(lc, spec->partValues)
				{
					List	   *vals = lfirst(lc);
					PgPartRule *prule = NULL;

					pid.idtype = AT_AP_IDValue;
					pid.partiddef = (Node *) vals;
					pid.location = -1;

					prule = get_part_rule1(rel, &pid, false, false,
										   NULL,
										   pNode,
										   lrelname,
										   &pNode2);

					if (prule && !(prule->topRule->parisdefault && is_split))
						ereport(ERROR,
								(errcode(ERRCODE_UNDEFINED_OBJECT),
								 errmsg("new partition definition overlaps "
										"existing partition%s of %s",
										prule->partIdStr,
										lrelname)));

				}				/* end foreach */
			}

			/* give a new maxpartno for the list partition */
			if (pNode && pNode->rules && pNode->num_rules > 0)
			{
				maxpartno = 1;

				for (int i = 0; i < pNode->num_rules; i++)
				{
					PartitionRule *rule = pNode->rules[i];

					if (rule->parruleord > maxpartno)
						break;
					++maxpartno;
				}
			}
		}
	}

	if (maxpartno < 0 || maxpartno > PG_INT16_MAX)
	{
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
			errmsg("too many partitions, parruleord overflow")));
	}

	if (newPos == FIRST && pNode && pNode->num_rules > 0)
	{
		/*
		 * Adding new partition at the beginning.  Find a hole in existing
		 * parruleord sequence by scanning rules list.	 Open gap only until
		 * the hole to accommodate the new rule at parruleord = 1.
		 */
		PartitionRule *rule = NULL;
		int			hole = 1;

		for (int i = 0; i < pNode->num_rules; i++)
		{
			rule = pNode->rules[i];

			if (rule->parruleord > hole)
			{
				break;
			}
			++hole;
		}

		/*
		 * Open gap only if hole found in the middle.  If hole exists right at
		 * the beginning (first partition's parruleord > 1), the gap is
		 * already open for us.
		 */
		if (hole > 1)
		{
			parruleord_open_gap(
								pNode->part->partid, pNode->part->parlevel,
								rule->parparentoid, --hole, 1,
								false /* closegap */ );
		}
	}
	else if (newPos == LAST && pNode && pNode->num_rules > 0)
	{
		/*
		 * Adding the new partition at the end.	 Find the hole closest to the
		 * end of the rule list.	 Close gap from the last rule only until
		 * this hole.  The new partition then gets the last partition's
		 * parruleord.
		 */
		int			hole = 1,
					stopkey = -1;

		for (int i = 0; i < pNode->num_rules; i++)
		{
			PartitionRule *rule = pNode->rules[i];

			if (rule->parruleord > hole)
			{
				hole = stopkey = rule->parruleord;
			}
			++hole;
		}
		if (stopkey != -1)
		{
			PartitionRule *last_rule = pNode->rules[pNode->num_rules - 1];

			parruleord_open_gap(
								pNode->part->partid, pNode->part->parlevel,
								last_rule->parparentoid, last_rule->parruleord, stopkey,
								true /* closegap */ );
			/* Let the new rule reuse last rule's parruleord. */
			--maxpartno;
		}
	}
	else if (bOpenGap)
	{
		/*
		 * Adding new partition in between first and the last one. Check if a
		 * hole exists by scanning rule list.  If one exists, either open or
		 * close gap based on location of the hole relative to maxpartno.
		 */
		PartitionRule *rule = NULL;
		int			hole = 1;

		for (int i = 0; i < pNode->num_rules; i++)
		{
			rule = pNode->rules[i];
			if (rule->parruleord > hole)
				break;
			++hole;
		}
		if (maxpartno > hole)
		{
			/*
			 * Found a hole before maxpartno.  Make room for new partition in
			 * the slot previous to maxpartno.  Decrement parruleord values
			 * from this slot until the hole.
			 */
			parruleord_open_gap(
								pNode->part->partid,
								pNode->part->parlevel,
								rule->parparentoid,
								--maxpartno,
								++hole,
								true /* closegap */ );
		}
		else if (maxpartno < hole)
		{
			/*
			 * Found a hole after maxpartno.  Open gap for maxpartno by
			 * incrementing parruleord values from the hole until maxpartno.
			 */
			parruleord_open_gap(
								pNode->part->partid,
								pNode->part->parlevel,
								rule->parparentoid,
								hole,
								maxpartno,
								false /* closegap */ );
		}
		/* if (hole == maxpartno) we don't need to open a gap. */
	}

	{
		List	   *l1;
		ListCell   *lc;
		int			ii = 0;
		bool		bFixFirstATS = true;
		bool		bFirst_TemplateOnly = true; /* ignore dummy entry */
		int			pby_templ_depth = 0;	/* template partdepth */
		Oid			skipTableRelid = InvalidOid;

		/*
		 * This transformCreateStmt() expands the phony create of a
		 * partitioned table that we just build into the constituent commands
		 * we need to create the new part.  (This will include some commands
		 * for the parent that we don't need, since the parent already
		 * exists.)
		 */
		l1 = transformCreateStmt(ct, "ADD PARTITION", true);

		/*
		 * skip the first cell because the table already exists -- don't
		 * recreate it
		 */
		lc = list_head(l1);

		if (lc)
		{
			Node	   *s = lfirst(lc);

			/*
			 * MPP-10421: but save the relid of the skipped table, because we
			 * skip indexes associated with it...
			 */
			if (IsA(s, CreateStmt))
			{
				CreateStmt *t = (CreateStmt *) s;

				skipTableRelid = RangeVarGetRelid(t->relation, NoLock, true);
			}
		}

		for_each_cell(lc, lnext(lc))
		{
			Node	   *q = lfirst(lc);

			/*
			 * MPP-6379, MPP-10421: If the statement is an expanded index
			 * creation statement on the parent (or the "skipped table"),
			 * ignore it. We get into this situation when the parent has one
			 * or more indexes on it that our new partition is inheriting.
			 */
			if (IsA(q, IndexStmt))
			{
				IndexStmt  *istmt = (IndexStmt *) q;
				Oid			idxRelid = RangeVarGetRelid(istmt->relation, NoLock, true);

				if (idxRelid == RelationGetRelid(rel))
					continue;

				if (OidIsValid(skipTableRelid) &&
					(idxRelid == skipTableRelid))
					continue;
			}

			/*
			 * XXX XXX: fix the first Alter Table Statement to have the
			 * correct maxpartno.  Whoohoo!!
			 */
			if (bFixFirstATS && q && IsA(q, AlterTableStmt))
			{
				PartitionSpec *spec = NULL;
				AlterTableStmt *ats;
				AlterTableCmd *atc;
				List	   *cmds;

				bFixFirstATS = false;

				ats = (AlterTableStmt *) q;
				Assert(IsA(ats, AlterTableStmt));

				cmds = ats->cmds;

				Assert(cmds && (list_length(cmds) > 1));

				atc = (AlterTableCmd *) lsecond(cmds);

				Assert(atc->def);

				pBy = (PartitionBy *) atc->def;

				Assert(IsA(pBy, PartitionBy));

				spec = (PartitionSpec *) pBy->partSpec;

				if (spec)
				{
					List	   *l2 = spec->partElem;
					PartitionElem *pel;

					if (l2 && list_length(l2))
					{
						pel = (PartitionElem *) linitial(l2);

						pel->partno = maxpartno;
					}

				}

			}					/* end first alter table fixup */
			else if (IsA(q, CreateStmt))
			{
				/* propagate owner */
				((CreateStmt *) q)->ownerid = ownerid;
			}

			/*
			 * normal case - add partitions using CREATE statements that get
			 * dispatched to the segments
			 */
			if (!bSetTemplate)
				ProcessUtility(q,
							   synthetic_sql,
							   PROCESS_UTILITY_SUBCOMMAND,
							   NULL,
							   dest,
							   NULL);
			else
			{					/* setting subpartition template only */

				/*
				 * find all the alter table statements that contain
				 * partaddinternal, and extract the definitions.  Only build
				 * the catalog entries for subpartition templates, not "real"
				 * table entries.
				 */
				if (IsA(q, AlterTableStmt))
				{
					AlterTableStmt *at2 = (AlterTableStmt *) q;
					List	   *l2 = at2->cmds;
					ListCell   *lc2;

					foreach(lc2, l2)
					{
						AlterTableCmd *ac2 = (AlterTableCmd *) lfirst(lc2);

						if (ac2->subtype == AT_PartAddInternal)
						{
							PartitionBy *templ_pby =
							(PartitionBy *) ac2->def;

							Assert(IsA(templ_pby, PartitionBy));

							/*
							 * skip the first one because it's the fake parent
							 * partition definition for the subpartition
							 * template entries
							 */

							if (bFirst_TemplateOnly)
							{
								bFirst_TemplateOnly = false;

								/*
								 * MPP-5992: only set one level of templates
								 * -- we might have templates for
								 * subpartitions of the subpartitions, which
								 * would add duplicate templates into the
								 * table. Only add templates of the specified
								 * depth and skip deeper template definitions.
								 */
								pby_templ_depth = templ_pby->partDepth + 1;

							}
							else
							{
								if (templ_pby->partDepth == pby_templ_depth)
									add_part_to_catalog(
														RelationGetRelid(rel),
														(PartitionBy *) ac2->def,
														true);
							}

						}
					}			/* end foreach lc2 l2 */
				}
			}					/* end else setting subpartition templates
								 * only */

			ii++;
		}						/* end for each cell */

	}

	if (par_prule && par_prule->topRule)
		heap_close(par_rel, NoLock);

	return pSubSpec;
}								/* end atpxPartAddList */


List *
atpxDropList(Relation rel, PartitionNode *pNode)
{
	List	   *l1 = NIL;

	if (!pNode)
		return l1;

	/* add the child lists first */
	for (int i = 0; i < pNode->num_rules; i++)
	{
		PartitionRule *rule = pNode->rules[i];
		List	   *l2 = NIL;

		if (rule->children)
			l2 = atpxDropList(rel, rule->children);
		else
			l2 = NIL;

		if (l2)
		{
			if (l1)
				l1 = list_concat(l1, l2);
			else
				l1 = l2;
		}
	}

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;
		List	   *l2 = NIL;

		if (rule->children)
			l2 = atpxDropList(rel, rule->children);
		else
			l2 = NIL;

		if (l2)
		{
			if (l1)
				l1 = list_concat(l1, l2);
			else
				l1 = l2;
		}
	}

	/* add entries for rules at current level */
	for (int i = 0; i < pNode->num_rules; i++)
	{
		PartitionRule *rule = pNode->rules[i];
		char	   *prelname;
		char	   *nspname;
		Relation	rel;

		rel = heap_open(rule->parchildrelid, AccessShareLock);
		prelname = pstrdup(RelationGetRelationName(rel));
		nspname = pstrdup(get_namespace_name(RelationGetNamespace(rel)));
		heap_close(rel, NoLock);

		if (l1)
			l1 = lappend(l1, list_make2(makeString(nspname),
										makeString(prelname)));
		else
			l1 = list_make1(list_make2(makeString(nspname),
									   makeString(prelname)));
	}

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;
		char	   *prelname;
		char	   *nspname;
		Relation	rel;

		rel = heap_open(rule->parchildrelid, AccessShareLock);
		prelname = pstrdup(RelationGetRelationName(rel));
		nspname = pstrdup(get_namespace_name(RelationGetNamespace(rel)));
		heap_close(rel, NoLock);

		if (l1)
			l1 = lappend(l1, list_make2(makeString(nspname),
										makeString(prelname)));
		else
			l1 = list_make1(list_make2(makeString(nspname),
									   makeString(prelname)));
	}

	return l1;
}								/* end atpxDropList */


void
exchange_part_rule(Oid oldrelid, Oid newrelid)
{
	HeapTuple	tuple;
	Relation	catalogRelation;
	ScanKeyData scankey;
	SysScanDesc sscan;

	/*
	 * pg_partition and  pg_partition_rule are populated only on the entry
	 * database, so a call to this function is only meaningful there.
	 */
	Insist(IS_QUERY_DISPATCHER());

	catalogRelation = heap_open(PartitionRuleRelationId, RowExclusiveLock);

	/* SELECT * FROM pg_partition_rule WHERE parchildrelid = :1 FOR UPDATE */
	ScanKeyInit(&scankey, Anum_pg_partition_rule_parchildrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(oldrelid));
	sscan = systable_beginscan(catalogRelation, PartitionRuleParchildrelidIndexId, true,
							   NULL, 1, &scankey);
	tuple = systable_getnext(sscan);
	if (HeapTupleIsValid(tuple))
	{
		/* make a modifiable copy */
		tuple = heap_copytuple(tuple);

		((Form_pg_partition_rule) GETSTRUCT(tuple))->parchildrelid = newrelid;

		simple_heap_update(catalogRelation, &tuple->t_self, tuple);
		CatalogUpdateIndexes(catalogRelation, tuple);

		heap_freetuple(tuple);
	}

	systable_endscan(sscan);
	heap_close(catalogRelation, NoLock);
}

void
exchange_permissions(Oid oldrelid, Oid newrelid)
{
	HeapTuple	oldtuple;
	HeapTuple	newtuple;
	Datum		save;
	bool		saveisnull;
	Datum		values[Natts_pg_class];
	bool		nulls[Natts_pg_class];
	bool		replaces[Natts_pg_class];
	HeapTuple	replace_tuple;
	bool		isnull;
	Relation	rel = heap_open(RelationRelationId, RowExclusiveLock);

	oldtuple = SearchSysCache1(RELOID, ObjectIdGetDatum(oldrelid));
	if (!HeapTupleIsValid(oldtuple))
		elog(ERROR, "cache lookup failed for relation %u", oldrelid);

	save = SysCacheGetAttr(RELOID, oldtuple,
						   Anum_pg_class_relacl,
						   &saveisnull);

	newtuple = SearchSysCache1(RELOID, ObjectIdGetDatum(newrelid));
	if (!HeapTupleIsValid(newtuple))
		elog(ERROR, "cache lookup failed for relation %u", newrelid);

	/* finished building new ACL value, now insert it */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));
	MemSet(replaces, false, sizeof(replaces));

	replaces[Anum_pg_class_relacl - 1] = true;
	values[Anum_pg_class_relacl - 1] = SysCacheGetAttr(RELOID, newtuple,
													   Anum_pg_class_relacl,
													   &isnull);
	if (isnull)
		nulls[Anum_pg_class_relacl - 1] = true;

	replace_tuple = heap_modify_tuple(oldtuple,
									  RelationGetDescr(rel),
									  values, nulls, replaces);
	simple_heap_update(rel, &oldtuple->t_self, replace_tuple);
	CatalogUpdateIndexes(rel, replace_tuple);

	/* XXX: Update the shared dependency ACL info */

	/* finished building new ACL value, now insert it */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));
	MemSet(replaces, false, sizeof(replaces));

	replaces[Anum_pg_class_relacl - 1] = true;
	values[Anum_pg_class_relacl - 1] = save;

	if (saveisnull)
		nulls[Anum_pg_class_relacl - 1] = true;

	replace_tuple = heap_modify_tuple(newtuple,
									  RelationGetDescr(rel),
									  values, nulls, replaces);
	simple_heap_update(rel, &newtuple->t_self, replace_tuple);
	CatalogUpdateIndexes(rel, replace_tuple);

	/* update shared dependency */

	ReleaseSysCache(oldtuple);
	ReleaseSysCache(newtuple);
	heap_close(rel, NoLock);
}

static void
atpxSkipper(PartitionNode *pNode, int *skipped)
{
	if (!pNode)
		return;

	/* add entries for rules at current level */
	for (int i = 0; i < pNode->num_rules; i++)
	{
		PartitionRule *rule = pNode->rules[i];

		if (skipped)
			*skipped += 1;

		if (rule->children)
			atpxSkipper(rule->children, skipped);
	}							/* end foreach */

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;

		if (skipped)
			*skipped += 1;

		if (rule->children)
			atpxSkipper(rule->children, skipped);
	}
}								/* end atpxSkipper */

static List *
build_rename_part_recurse(PartitionRule *rule, const char *old_parentname,
						  const char *new_parentname,
						  int *skipped)
{

	RangeVar   *rv;
	Relation	rel;
	char	   *relname = NULL;
	char		newRelNameBuf[(NAMEDATALEN * 2)];
	List	   *l1 = NIL;

	rel = heap_open(rule->parchildrelid, AccessShareLock);

	relname = pstrdup(RelationGetRelationName(rel));

	rv = makeRangeVar(get_namespace_name(RelationGetNamespace(rel)),
					  relname, -1);

	/* unlock, because we have a lock on the master */
	heap_close(rel, AccessShareLock);

	/*
	 * The child name should contain the old parent name as a prefix - check
	 * the length and compare to make sure.
	 *
	 * To build the new child name, just use the new name as a prefix, and use
	 * the remainder of the child name (the part after the old parent name
	 * prefix) as the suffix.
	 */
	if (strlen(old_parentname) > strlen(relname))
	{
		if (skipped)
			*skipped += 1;

		atpxSkipper(rule->children, skipped);
	}
	else
	{
		if (0 != (strncmp(old_parentname, relname, strlen(old_parentname))))
		{
			if (skipped)
				*skipped += 1;
			atpxSkipper(rule->children, skipped);
		}
		else
		{
			snprintf(newRelNameBuf, sizeof(newRelNameBuf), "%s%s",
					 new_parentname, relname + strlen(old_parentname));

			if (strlen(newRelNameBuf) > NAMEDATALEN)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
						 errmsg("relation name \"%s\" for child partition is too long",
								newRelNameBuf)));

			l1 = lappend(l1, list_make2(rv, pstrdup(newRelNameBuf)));

			/* add the child lists next (not first) */
			{
				List	   *l2 = NIL;

				if (rule->children)
					l2 = atpxRenameList(rule->children,
										relname, newRelNameBuf, skipped);

				if (l2)
					l1 = list_concat(l1, l2);
			}
		}
	}
	return l1;
}

List *
atpxRenameList(PartitionNode *pNode,
			   char *old_parentname, const char *new_parentname, int *skipped)
{
	List	   *l1 = NIL;

	if (!pNode)
		return l1;

	/* add entries for rules at current level */
	for (int i = 0; i < pNode->num_rules; i++)
	{
		PartitionRule *rule = pNode->rules[i];

		l1 = list_concat(l1,
						 build_rename_part_recurse(rule,
												   old_parentname,
												   new_parentname,
												   skipped));
	}							/* end foreach */

	/* and the default partition */
	if (pNode->default_part)
	{
		PartitionRule *rule = pNode->default_part;

		l1 = list_concat(l1,
						 build_rename_part_recurse(rule,
												   old_parentname,
												   new_parentname,
												   skipped));
	}

	return l1;
}								/* end atpxRenameList */


static Oid
get_opfuncid_by_opname(List *opname, Oid lhsid, Oid rhsid)
{
	Oid			opfuncid;
	Operator	op;

	op = oper(NULL, opname, lhsid, rhsid, false, -1);

	if (op == NULL)				/* should not fail */
		elog(ERROR, "could not find operator");

	opfuncid = ((Form_pg_operator) GETSTRUCT(op))->oprcode;

	ReleaseSysCache(op);
	return opfuncid;
}


/* Construct the PgPartRule for a branch of a partitioning hierarchy.
 *
 *		rel - the partitioned relation (top-level)
 *		cmd - an AlterTableCmd, possibly nested in type AT_PartAlter AlterTableCmds
 *            identifying a subset of the parts of the partitioned relation.
 */
static PgPartRule *
get_pprule_from_ATC(Relation rel, AlterTableCmd *cmd)
{
	List	   *pids = NIL;		/* of AlterPartitionId */
	AlterPartitionId *pid = NULL;
	PgPartRule *pprule = NULL;
	AlterPartitionId *work_partid = NULL;

	AlterTableCmd *atc = cmd;


	/* Get list of enclosing ALTER PARTITION ids. */
	while (atc->subtype == AT_PartAlter)
	{
		AlterPartitionCmd *apc = (AlterPartitionCmd *) atc->def;

		pid = (AlterPartitionId *) apc->partid;
		Insist(IsA(pid, AlterPartitionId));

		atc = (AlterTableCmd *) apc->arg1;
		Insist(IsA(atc, AlterTableCmd));

		pids = lappend(pids, pid);
	}

	/*
	 * The effective ALTER TABLE command is in atc. The pids list (of
	 * AlterPartitionId nodes) represents the path to top partitioning branch
	 * of rel.  Since we are only called for branches and leaves (never the
	 * root) of the partition, the pid list should not empty.
	 *
	 * Use the AlterPartitionId interpretter, get_part_rule, to do the
	 * interpretation.
	 */
	Insist(list_length(pids) > 0);

	work_partid = makeNode(AlterPartitionId);

	work_partid->idtype = AT_AP_IDList;
	work_partid->partiddef = (Node *) pids;
	work_partid->location = -1;

	pprule = get_part_rule(rel,
						   work_partid,
						   true, true,	/* parts must exist */
						   NULL,	/* no implicit results */
						   false	/* no template rules */
		);

	return pprule;
}

/* Return the pg_class OIDs of the relations representing the parts of
 * a partitioned table designated by the given AlterTable command.
 *
 *		rel - the partitioned relation (top-level)
 *		cmd - an AlterTableCmd, possibly nested in type AT_PartAlter AlterTableCmds
 *            identifying a subset of the parts of the partitioned relation.
 */
List *
basic_AT_oids(Relation rel, AlterTableCmd *cmd)
{
	PgPartRule *pprule = get_pprule_from_ATC(rel, cmd);

	if (!pprule)
		return NIL;

	return all_prule_relids(pprule->topRule);
}

/*
 * Return the basic AlterTableCmd found by peeling off intervening layers of
 * ALTER PARTITION from the given AlterTableCmd.
 */
AlterTableCmd *
basic_AT_cmd(AlterTableCmd *cmd)
{
	while (cmd->subtype == AT_PartAlter)
	{
		AlterPartitionCmd *apc = (AlterPartitionCmd *) cmd->def;

		Insist(IsA(apc, AlterPartitionCmd));
		cmd = (AlterTableCmd *) apc->arg1;
		Insist(IsA(cmd, AlterTableCmd));
	}
	return cmd;
}


/* Determine whether we can implement a requested distribution on a part of
 * the specified partitioned table.
 *
 * In 3.3
 *   DISTRIBUTED RANDOMLY or distributed just like the whole partitioned
 *   table is implementable.  Anything else is not.
 *
 * rel              Pointer to cache entry for the whole partitioned table
 * dist_cnames      List of column names proposed for distribution some part
 */
bool
can_implement_dist_on_part(Relation rel, DistributedBy *dist)
{
	ListCell	*lc;
	int		i;

	if (Gp_role != GP_ROLE_DISPATCH)
	{
		ereport(DEBUG1,
				(errmsg("requesting redistribution outside dispatch - returning no")));
		return false;
	}

	/* Random is okay.  It is represented by a list of one empty list. */
	if (dist->ptype == POLICYTYPE_PARTITIONED && dist->keyCols == NIL)
		return true;

	if (dist->ptype == POLICYTYPE_REPLICATED)
		return false;

	/* Require an exact match to the policy of the parent. */
	if (list_length(dist->keyCols) != rel->rd_cdbpolicy->nattrs)
		return false;

	i = 0;
	foreach(lc, dist->keyCols)
	{
		DistributionKeyElem *dkelem = (DistributionKeyElem *) lfirst(lc);
		AttrNumber	attnum;
		HeapTuple	tuple;
		bool		ok = false;

		Assert(IsA(dkelem, DistributionKeyElem));

		tuple = SearchSysCacheAttName(RelationGetRelid(rel), dkelem->name);
		if (!HeapTupleIsValid(tuple))
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("column \"%s\" of relation \"%s\" does not exist",
							dkelem->name,
							RelationGetRelationName(rel))));

		attnum = ((Form_pg_attribute) GETSTRUCT(tuple))->attnum;

		if (attnum == rel->rd_cdbpolicy->attrs[i++])
			ok = true;

		ReleaseSysCache(tuple);

		if (!ok)
			return false;
	}
	return true;
}



/* Test whether we can exchange newrel into oldrel's space within the
 * partitioning hierarchy of rel as far as the table schema is concerned.
 * (Does not, e.g., look into constraint agreement, etc.)
 *
 * If throw is true, throw an appropriate error in case the answer is
 * "no, can't exchange".  If throw is false, just return the answer
 * quietly.
 */
bool
is_exchangeable(Relation rel, Relation oldrel, Relation newrel, bool throw)
{
	TupleConversionMap *map_new = NULL;
	TupleConversionMap *map_old = NULL;
	bool		old_is_external;
	bool		new_is_external;
	bool		old_is_exchangeable;
	bool		new_is_exchangeable;
	bool		congruent = TRUE;

	/* Both parts must be relations. */
	old_is_external = (oldrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
					   rel_is_external_table(RelationGetRelid(oldrel)));
	new_is_external = (newrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE &&
					   rel_is_external_table(RelationGetRelid(newrel)));
	old_is_exchangeable = (oldrel->rd_rel->relkind == RELKIND_RELATION || old_is_external);
	new_is_exchangeable = (newrel->rd_rel->relkind == RELKIND_RELATION || new_is_external);
	if (!(old_is_exchangeable || new_is_exchangeable))
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot exchange relation "
							"which is not a table")));
	}

	if(oldrel->rd_rel->relpersistence != newrel->rd_rel->relpersistence)
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					errmsg("cannot exchange relations with differing persistence types")));
	}

	if (new_is_external)
	{
		if (rel_is_default_partition(oldrel->rd_id))
		{
			congruent = FALSE;
			if (throw)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("cannot exchange DEFAULT partition with external table")));
		}

		ExtTableEntry *extEntry = GetExtTableEntry(newrel->rd_id);

		if (extEntry && extEntry->iswritable)
		{
			congruent = FALSE;
			if (throw)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("cannot exchange relation which is a WRITABLE external table")));
		}
	}

	/*
	 * Attributes of the existing part (oldrel) must be compatible with the
	 * partitioned table as a whole.  This might be an assertion, but we don't
	 * want this case to pass in a production build, so we use an internal
	 * error.
	 */
	if (congruent && !map_part_attrs(rel, oldrel, &map_old, FALSE))
	{
		congruent = FALSE;
		if (throw)
			elog(ERROR, "existing part \"%s\" not congruent with"
				 "partitioned table \"%s\"",
				 RelationGetRelationName(oldrel),
				 RelationGetRelationName(rel));
	}

	/* From here on we need to be careful to free the maps. */

	/*
	 * Attributes of new part must be compatible with the partitioned table.
	 * (We assume that the attributes of the old part are compatible.)
	 */
	if (congruent && !map_part_attrs(rel, newrel, &map_new, throw))
		congruent = FALSE;

	/* Both parts must have the same owner. */
	if (congruent && oldrel->rd_rel->relowner != newrel->rd_rel->relowner)
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("owner of \"%s\" must be the same as that of \"%s\"",
							RelationGetRelationName(newrel),
							RelationGetRelationName(rel))));
	}

	/* Both part tables must have the same "WITH OID"s setting */
	if (congruent && oldrel->rd_rel->relhasoids != newrel->rd_rel->relhasoids)
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("\"%s\" and \"%s\" must have same OIDs setting",
							RelationGetRelationName(rel),
							RelationGetRelationName(newrel))));
	}

	/* The new part table must not be involved in inheritance. */
	if (congruent && has_subclass(RelationGetRelid(newrel)))
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot EXCHANGE table \"%s\" as it has child table(s)",
							RelationGetRelationName(newrel))));
	}

	if (congruent && relation_has_supers(RelationGetRelid(newrel)))
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot exchange table \"%s\" as it inherits other table(s)",
							RelationGetRelationName(newrel))));
	}

	/* The new part table must not have rules on it. */
	if (congruent && (newrel->rd_rules || oldrel->rd_rules))
	{
		congruent = FALSE;
		if (throw)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cannot exchange table which has rules defined on it")));
	}

	/*
	 * The distribution policies of the existing part (oldpart) and the
	 * candidate part (newpart) must match that of the whole partitioned
	 * table.  However, we can only check this where the policy table is
	 * populated, i.e., on the entry database.  Note checking the policy of
	 * the existing part is defensive.  It SHOULD match. Skip the check when
	 * either the oldpart or the newpart is external.
	 */
	if (congruent && Gp_role == GP_ROLE_DISPATCH &&
		!new_is_external && !old_is_external)
	{
		GpPolicy   *parpol = rel->rd_cdbpolicy;
		GpPolicy   *oldpol = oldrel->rd_cdbpolicy;
		GpPolicy   *newpol = newrel->rd_cdbpolicy;
		GpPolicy   *adjpol = NULL;

		if (map_old != NULL)
		{
			int			i;
			AttrNumber	remapped_parent_attr = 0;

			for (i = 0; i < parpol->nattrs; i++)
			{
				remapped_parent_attr = attrMap(map_old, parpol->attrs[i]);

				if (!(parpol->attrs[i] > 0	/* assert parent live */
					  && oldpol->attrs[i] > 0	/* assert old part live */
					  && remapped_parent_attr == oldpol->attrs[i]	/* assert match */
					  ))
					elog(ERROR,
						 "discrepancy in partitioning policy of \"%s\"",
						 RelationGetRelationName(rel));
			}
		}
		else
		{
			if (!GpPolicyEqual(parpol, oldpol))
				elog(ERROR,
					 "discrepancy in partitioning policy of \"%s\"",
					 RelationGetRelationName(rel));
		}

		if (map_new != NULL)
		{
			int			i;

			adjpol = GpPolicyCopy(parpol);

			for (i = 0; i < adjpol->nattrs; i++)
			{
				adjpol->attrs[i] = attrMap(map_new, parpol->attrs[i]);
				Assert(newpol->attrs[i] > 0);	/* check new part */
			}
		}
		else
		{
			adjpol = parpol;
		}

		if (!GpPolicyEqual(adjpol, newpol))
		{
			congruent = FALSE;
			if (throw)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("distribution policy for \"%s\" must be the same as that for \"%s\"",
								RelationGetRelationName(newrel),
								RelationGetRelationName(rel))));
		}
		else if (false && memcmp(oldpol->attrs, newpol->attrs,
								 sizeof(AttrNumber) * adjpol->nattrs))
		{
			congruent = FALSE;
			if (throw)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("distribution policy matches but implementation lags")));
		}
	}

	if (map_old != NULL)
		pfree(map_old);
	if (map_new != NULL)
		pfree(map_new);

	return congruent;
}


/*
 * Apply the constraint represented by the argument pg_constraint tuple
 * remapped through the argument attribute map to the candidate relation.
 *
 * In addition, if validate is true and the constraint is one we enforce
 * on partitioned tables, allocate and return a NewConstraint structure
 * for use in phase 3 to validate the relation (i.e., to make sure it
 * conforms to its constraints).
 *
 * Note that pgcon (the ConstraintRelationId appropriately locked)
 * is supplied externally for efficiency.  No other relation should
 * be supplied via this argument.
 */
static NewConstraint *
constraint_apply_mapped(HeapTuple tuple, TupleConversionMap *map, Relation cand,
						bool validate, Relation pgcon)
{
	Datum		val;
	bool		isnull;
	Datum	   *dats;
	int16	   *keys;
	int			nkeys;
	int			i;
	Node	   *conexpr;
	char	   *consrc;
	char	   *conbin;
	Form_pg_constraint con = (Form_pg_constraint) GETSTRUCT(tuple);
	NewConstraint *newcon = NULL;

	/* Translate pg_constraint.conkey */
	val = heap_getattr(tuple, Anum_pg_constraint_conkey,
					   RelationGetDescr(pgcon), &isnull);
	Assert(!isnull);

	deconstruct_array(DatumGetArrayTypeP(val),
					  INT2OID, 2, true, 's',
					  &dats, NULL, &nkeys);

	keys = palloc(sizeof(int16) * nkeys);
	for (i = 0; i < nkeys; i++)
	{
		int16		key = DatumGetInt16(dats[i]);

		keys[i] = (int16) attrMap(map, key);
	}

	/* Translate pg_constraint.conbin */
	val = heap_getattr(tuple, Anum_pg_constraint_conbin,
					   RelationGetDescr(pgcon), &isnull);
	if (!isnull)
	{
		conbin = TextDatumGetCString(val);
		conexpr = stringToNode(conbin);
		conexpr = attrMapExpr(map, conexpr);
		conbin = nodeToString(conexpr);
	}
	else
	{
		conbin = NULL;
		conexpr = NULL;
	}


	/* Don't translate pg_constraint.consrc -- per doc'n, use original */
	val = heap_getattr(tuple, Anum_pg_constraint_consrc,
					   RelationGetDescr(pgcon), &isnull);
	if (!isnull)
	{
		consrc = TextDatumGetCString(val);
	}
	else
	{
		consrc = NULL;
	}

	/* Apply translated constraint to candidate. */
	switch (con->contype)
	{
		case CONSTRAINT_CHECK:
			{
				Assert(conexpr && conbin && consrc);

				CreateConstraintEntry(NameStr(con->conname),
									  con->connamespace, // XXX should this be RelationGetNamespace(cand)?
									  con->contype,
									  con->condeferrable,
									  con->condeferred,
									  con->convalidated,
									  RelationGetRelid(cand),
									  keys,
									  nkeys,
									  InvalidOid,
									  InvalidOid,
									  InvalidOid,
									  NULL,
									  NULL,
									  NULL,
									  NULL,
									  0,
									  ' ',
									  ' ',
									  ' ',
									  NULL,		/* exclOp */
									  conexpr,
									  conbin,
									  consrc,
									  con->conislocal,
									  con->coninhcount,
									  false, /* conNoInherit */
									  true /* is_internal */);
				break;
			}

		case CONSTRAINT_FOREIGN:
			{
				int16	   *fkeys;
				int			nfkeys;
				Oid			indexoid = InvalidOid;
				Oid		   *opclasses = NULL;
				Relation	frel;

				val = heap_getattr(tuple, Anum_pg_constraint_confkey,
								   RelationGetDescr(pgcon), &isnull);
				Assert(!isnull);

				deconstruct_array(DatumGetArrayTypeP(val),
								  INT2OID, 2, true, 's',
								  &dats, NULL, &nfkeys);
				fkeys = palloc(sizeof(int16) * nfkeys);
				for (i = 0; i < nfkeys; i++)
				{
					fkeys[i] = DatumGetInt16(dats[i]);
				}

				frel = heap_open(con->confrelid, AccessExclusiveLock);
				indexoid = transformFkeyCheckAttrs(frel, nfkeys, fkeys, opclasses);

				CreateConstraintEntry(NameStr(con->conname),
									  RelationGetNamespace(cand),
									  con->contype,
									  con->condeferrable,
									  con->condeferred,
									  con->convalidated,
									  RelationGetRelid(cand),
									  keys,
									  nkeys,
									  InvalidOid,
									  indexoid,
									  con->confrelid,
									  fkeys,
									  NULL,
									  NULL,
									  NULL,
									  nfkeys,
									  con->confupdtype,
									  con->confdeltype,
									  con->confmatchtype,
									  NULL,		/* exclOp */
									  NULL,		/* no check constraint */
									  NULL,
									  NULL,
									  con->conislocal,
									  con->coninhcount,
									  true, /* conNoInherit */
									  true /* is_internal */);

				heap_close(frel, AccessExclusiveLock);
				break;
			}
		case CONSTRAINT_PRIMARY:
		case CONSTRAINT_UNIQUE:
			{
				/*
				 * Index-backed constraints are handled as indexes.  No action
				 * here.
				 */
				char	   *what = (con->contype == CONSTRAINT_PRIMARY) ? "PRIMARY KEY" : "UNIQUE";
				char	   *who = NameStr(con->conname);

				if (validate)
				{
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("%s constraint \"%s\" missing", what, who),
							 errhint("Add %s constraint \"%s\" to the candidate table or drop it from the partitioned table.",
									 what, who)));
				}
				else
				{
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("WITHOUT VALIDATION incompatible with missing %s constraint \"%s\"",
									what, who),
							 errhint("Add %s constraint %s to the candidate table or drop it from the partitioned table.",
									 what, who)));

				}
				break;
			}
		default:
			/* Defensive, can't occur. */
			elog(ERROR, "invalid constraint type: %c", con->contype);
			break;
	}

	newcon = NULL;

	if (validate)
	{
		switch (con->contype)
		{
			case CONSTRAINT_CHECK:
				{
					newcon = (NewConstraint *) palloc0(sizeof(NewConstraint));
					newcon->name = pstrdup(NameStr(con->conname));
					/* ExecQual wants implicit-AND format */
					newcon->qual = (Node *) make_ands_implicit((Expr *) conexpr);
					newcon->contype = CONSTR_CHECK;
					break;
				}
			case CONSTRAINT_FOREIGN:
				{
					elog(WARNING, "Won't enforce FK constraint.");
					break;
				}
			case CONSTRAINT_PRIMARY:
				{
					elog(WARNING, "Won't enforce PK constraint.");
					break;
				}
			case CONSTRAINT_UNIQUE:
				{
					elog(WARNING, "Won't enforce ND constraint.");
					break;
				}
			default:
				{
					elog(WARNING, "!! NOT READY FOR TYPE %c CONSTRAINT !!", con->contype);
					break;
				}
		}
	}
	return newcon;
}


static bool
relation_has_supers(Oid relid)
{
	ScanKeyData scankey;
	Relation	rel;
	SysScanDesc sscan;
	bool		result;

	rel = heap_open(InheritsRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_inherits_inhrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	sscan = systable_beginscan(rel, InheritsRelidSeqnoIndexId, true,
							   NULL, 1, &scankey);

	result = (systable_getnext(sscan) != NULL);

	systable_endscan(sscan);

	heap_close(rel, AccessShareLock);

	return result;
}

/*
 * Preprocess a CreateStmt for a partitioned table before letting it
 * fall into the regular tranformation code.  This is an analyze-time
 * function.
 *
 * At the moment, the only fixing needed is to change default constraint
 * names to explicit ones so that they will propagate correctly through
 * to the parts of a partitioned table.
 */
void
fixCreateStmtForPartitionedTable(CreateStmt *stmt)
{
	ListCell   *lc_elt;
	Constraint *con;
	List	   *unnamed_cons = NIL;
	List	   *unnamed_cons_col = NIL;
	List	   *unnamed_cons_lbl = NIL;
	List	   *used_names = NIL;
	char	   *no_name = "";
	int			i;

	/* Caller should check this! */
	Assert(stmt->partitionBy && !stmt->is_part_child);

	foreach(lc_elt, stmt->tableElts)
	{
		Node	   *elt = lfirst(lc_elt);

		switch (nodeTag(elt))
		{
			case T_ColumnDef:
				{
					ListCell   *lc_con;

					ColumnDef  *cdef = (ColumnDef *) elt;

					foreach(lc_con, cdef->constraints)
					{
						Node	   *conelt = lfirst(lc_con);
						con = (Constraint *) conelt;

						Assert(IsA(conelt, Constraint));

						if (con->conname)
						{
							used_names = lappend(used_names, con->conname);
							continue;
						}
						switch (con->contype)
						{
							case CONSTR_CHECK:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, cdef->colname);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "check");
								break;
							case CONSTR_PRIMARY:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, cdef->colname);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "pkey");
								break;
							case CONSTR_UNIQUE:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, cdef->colname);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "key");
								break;
							case CONSTR_FOREIGN:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, cdef->colname);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "fkey");
								break;
							default:
								break;
						}
					}
					break;
				}
			case T_Constraint:
				{
					con = (Constraint *) elt;

					if (con->conname)
					{
						used_names = lappend(used_names, con->conname);
					}
					else
					{
						switch (con->contype)
						{
							case CONSTR_CHECK:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, no_name);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "check");
								break;
							case CONSTR_PRIMARY:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, no_name);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "pkey");
								break;
							case CONSTR_UNIQUE:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, no_name);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "key");
								break;
							case CONSTR_FOREIGN:
								unnamed_cons = lappend(unnamed_cons, con);
								unnamed_cons_col = lappend(unnamed_cons_col, no_name);
								unnamed_cons_lbl = lappend(unnamed_cons_lbl, "fkey");
							default:
								break;
						}
					}
					break;
				}
			case T_TableLikeClause:
				{
					break;
				}
			default:
				break;
		}
	}

	used_names = list_union(used_names, NIL);	/* eliminate dups */

	for (i = 0; i < list_length(unnamed_cons); i++)
	{
		char	   *label = list_nth(unnamed_cons_lbl, i);
		char	   *colname = NULL;
		Constraint *con = list_nth(unnamed_cons, i);

		Assert(IsA(con, Constraint));

		/* Conventionally, no column name for PK. */
		if (0 != strcmp(label, "pkey"))
			colname = list_nth(unnamed_cons_col, i);

		con->conname = ChooseConstraintNameForPartitionCreate(stmt->relation->relname,
															  colname,
															  label,
															  used_names);
		used_names = lappend(used_names, con->conname);
	}
}


/*
 * Subroutine for fixCreateStmtForPartitionedTable.
 *
 * Similar to ChooseConstraintNameForPartitionEarly but doesn't use the
 * catalogs since we're dealing with a currently non-existent namespace
 * (the space of constraint names on the table to be created).
 *
 * Modelled on ChooseConstraintName, though synchronization isn't a
 * requirement, just a nice idea.
 *
 * Caller is responsible for supplying the (unqualified) relation name,
 * optional column name (NULL or "" is okay for a table constraint),
 * label (e.g. "check"), and list of names to avoid.
 *
 * Result is palloc'd and caller's responsibility.
 */
char *
ChooseConstraintNameForPartitionCreate(const char *rname,
									   const char *cname,
									   const char *label,
									   List *used_names)
{
	int			pass = 0;
	char	   *conname = NULL;
	char		modlabel[NAMEDATALEN];
	bool		found = false;
	ListCell   *lc;

	Assert(rname && *rname);

	/* Allow caller to pass "" instead of NULL for non-singular cname */
	if (cname && *cname == '\0')
		cname = NULL;

	/* try the unmodified label first */
	StrNCpy(modlabel, label, sizeof(modlabel));

	for (;;)
	{
		conname = makeObjectName(rname, cname, modlabel);
		found = false;

		foreach(lc, used_names)
		{
			if (strcmp((char *) lfirst(lc), conname) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found)
			break;				/* we have a winner */

		pfree(conname);
		snprintf(modlabel, sizeof(modlabel), "%s%d", label, ++pass);
	}
	return conname;
}

/*
 * Determine whether the given attributes can be enforced unique within
 * the partitioning policy of the given partitioned table.  If not, issue
 * an error.  The argument primary just conditions the message text.
 */
void
index_check_partitioning_compatible(Relation rel,
									AttrNumber *indattr, Oid *exclops, int nidxatts,
									bool primary)
{
	int			i;
	Bitmapset  *ikey;
	Bitmapset  *pkey;

	if (exclops)
	{
		/*
		 * For now, don't allow exclusion constraints on partitioned tables at
		 * all.
		 *
		 * XXX: There's no fundamental reason they couldn't be made to work.
		 * As long as the index contains all the partitioning key columns,
		 * with the equality operators as the exclusion operators, they would
		 * work. These are the same conditions as with compatibility with
		 * distribution keys. But the code to check that hasn't been written
		 * yet.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("exclusion constraints are not supported on partitioned tables")));
	}

	ikey = NULL;
	for (i = 0; i < nidxatts; i++)
		ikey = bms_add_member(ikey, indattr[i]);

	pkey = get_partition_key_bitmapset(RelationGetRelid(rel));

	if (!bms_is_subset(pkey, ikey))
	{
		char	   *what = "UNIQUE";

		if (primary)
			what = "PRIMARY KEY";

		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("%s constraint must contain all columns in the partition key of relation \"%s\"",
						what, RelationGetRelationName(rel)),
				 errhint("Include the partition key or create a part-wise UNIQUE index instead.")));
	}

	bms_free(pkey);
	bms_free(ikey);
}

/**
 * Does a partition node correspond to a leaf partition?
 */
static bool
IsLeafPartitionNode(PartitionNode *p)
{
	Assert(p);

	/**
	 * If all of the rules have no children, this is a leaf partition.
	 */
	for (int i = 0; i < p->num_rules; i++)
	{
		PartitionRule *rule = p->rules[i];

		if (rule->children)
		{
			return false;
		}
	}

	/**
	 * If default partition has children then, this is not a leaf
	 */
	if (p->default_part
		&& p->default_part->children)
	{
		return false;
	}

	return true;
}

/*
 * Given a partition node, return all the associated rules, including the default partition rule if present
 */
static List *
get_partition_rules(PartitionNode *pn)
{
	Assert(pn);

	List	   *result = NIL;

	if (pn->default_part)
	{
		result = lappend(result, pn->default_part);
	}

	for (int i = 0; i < pn->num_rules; i++)
		result = lappend(result, pn->rules[i]);

	return result;
}

/**
 * Given a partition node, return list of children. Should not be called on a leaf partition node.
 *
 * Input:
 *	p - input partition node
 * Output:
 *	List of partition nodes corresponding to its children across all rules.
 */
static List *
PartitionChildren(PartitionNode *p)
{
	Assert(p);
	Assert(!IsLeafPartitionNode(p));

	List	   *result = NIL;

	for (int i = 0; i < p->num_rules; i++)
	{
		PartitionRule *rule = p->rules[i];

		if (rule->children)
		{
			result = lappend(result, rule->children);
		}
	}

	/**
	 * Also add default child
	 */
	if (p->default_part
		&& p->default_part->children)
	{
		result = lappend(result, p->default_part->children);
	}

	return result;
}

/*
 * selectPartitionMulti()
 *
 * Given an input partition node, values and nullness, and partition state,
 * find matching leaf partitions. This is similar to selectPartition() with one
 * big difference around nulls. If there is a null value corresponding to a partitioning attribute,
 * then all children are considered matches.
 *
 * The input values/isnull should match the layout of tuples in the
 * partitioned table.
 *
 * Output:
 *	leafPartitionOids - list of leaf partition oids, null if there are no matches
 */
List *
selectPartitionMulti(PartitionNode *partnode, Datum *values, bool *isnull,
					 TupleDesc tupdesc, PartitionAccessMethods *accessMethods)
{
	Assert(partnode);

	List	   *leafPartitionOids = NIL;

	List	   *inputList = list_make1(partnode);

	while (list_length(inputList) > 0)
	{
		List	   *levelOutput = NIL;

		ListCell   *lc = NULL;

		foreach(lc, inputList)
		{
			PartitionNode *candidatePartNode = (PartitionNode *) lfirst(lc);
			bool		foundNull = false;

			for (int i = 0; i < candidatePartNode->part->parnatts; i++)
			{
				AttrNumber	attno = candidatePartNode->part->paratts[i];

				/**
				 * If corresponding value is null, then we should pick all of its
				 * children (or itself if it is a leaf partition)
				 */
				if (isnull[attno - 1])
				{
					foundNull = true;
					if (IsLeafPartitionNode(candidatePartNode))
					{
						/**
						 * Extract out Oids of all children
						 */
						leafPartitionOids = list_concat(leafPartitionOids, all_partition_relids(candidatePartNode));
					}
					else
					{
						levelOutput = list_concat(levelOutput, PartitionChildren(candidatePartNode));
					}
				}
			}

			/**
			 * If null was not found on the attribute, and if this is a leaf partition,
			 * then there will be an exact match. If it is not a leaf partition, then
			 * we have to find the right child to investigate.
			 */
			if (!foundNull)
			{
				if (IsLeafPartitionNode(candidatePartNode))
				{
					Oid			matchOid = selectPartition1(candidatePartNode, values, isnull, tupdesc, accessMethods, NULL, NULL);

					if (matchOid != InvalidOid)
					{
						leafPartitionOids = lappend_oid(leafPartitionOids, matchOid);
					}
				}
				else
				{
					PartitionNode *childPartitionNode = NULL;

					selectPartition1(candidatePartNode, values, isnull, tupdesc, accessMethods, NULL, &childPartitionNode);
					if (childPartitionNode)
					{
						levelOutput = lappend(levelOutput, childPartitionNode);
					}
				}
			}

		}

		/**
		 * Start new level
		 */
		list_free(inputList);
		inputList = levelOutput;
	}

	return leafPartitionOids;
}

/*
 * Add a partition encoding clause for a subpartition template. We need to be
 * able to recall these for when we later add partitions which inherit the
 * subpartition template definition.
 */
static void
add_partition_encoding(Oid relid, Oid paroid, AttrNumber attnum, List *encoding)
{
	Relation	rel;
	Datum		partoptions;
	Datum		values[Natts_pg_partition_encoding];
	bool		nulls[Natts_pg_partition_encoding];
	HeapTuple	tuple;

	rel = heap_open(PartitionEncodingRelationId, RowExclusiveLock);

	Insist(attnum > 0);

	partoptions = transformRelOptions(PointerGetDatum(NULL),
									  encoding,
									  NULL, NULL,
									  true,
									  false);

	MemSet(nulls, 0, sizeof(nulls));

	values[Anum_pg_partition_encoding_parencoid - 1] = ObjectIdGetDatum(paroid);
	values[Anum_pg_partition_encoding_parencattnum - 1] = Int16GetDatum(attnum);
	values[Anum_pg_partition_encoding_parencattoptions - 1] = partoptions;

	tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);

	/* Insert tuple into the relation */
	simple_heap_insert(rel, tuple);
	CatalogUpdateIndexes(rel, tuple);

	heap_freetuple(tuple);

	heap_close(rel, RowExclusiveLock);
}

static void
remove_partition_encoding_entry(Oid paroid, AttrNumber attnum)
{
	Relation	rel;
	HeapTuple	tup;
	ScanKeyData scankey;
	SysScanDesc sscan;

	rel = heap_open(PartitionEncodingRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_partition_encoding_parencoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(paroid));

	sscan = systable_beginscan(rel, PartitionEncodingParencoidAttnumIndexId,
							   true, NULL, 1, &scankey);
	while (HeapTupleIsValid(tup = systable_getnext(sscan)))
	{
		if (attnum != InvalidAttrNumber)
		{
			Form_pg_partition_encoding ppe =
			(Form_pg_partition_encoding) GETSTRUCT(tup);

			if (ppe->parencattnum != attnum)
				continue;
		}
		simple_heap_delete(rel, &tup->t_self);
	}

	systable_endscan(sscan);
	heap_close(rel, RowExclusiveLock);
}

/*
 * Remove all trace of partition encoding for a relation. This is the DROP TABLE
 * case. May be called even if there's no entry for the partition.
 */
static void
remove_partition_encoding_by_key(Oid relid, AttrNumber attnum)
{
	Relation	partrel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tup;

	/* XXX XXX: not FOR UPDATE because update a child table... */

	partrel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&scankey, Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	sscan = systable_beginscan(partrel, PartitionParrelidIndexId, true,
							   NULL, 1, &scankey);
	while (HeapTupleIsValid(tup = systable_getnext(sscan)))
	{
		Form_pg_partition part = (Form_pg_partition) GETSTRUCT(tup);

		if (part->paristemplate)
			remove_partition_encoding_entry(HeapTupleGetOid(tup), attnum);
	}

	systable_endscan(sscan);
	heap_close(partrel, AccessShareLock);
}

void
RemovePartitionEncodingByRelid(Oid relid)
{
	remove_partition_encoding_by_key(relid, InvalidAttrNumber);
}

/*
 * Remove a partition encoding entry for a specific attribute number.
 * May be called when no such entry actually exists.
 */
void
RemovePartitionEncodingByRelidAttribute(Oid relid, AttrNumber attnum)
{
	remove_partition_encoding_by_key(relid, attnum);
}

/*
 * For all encoding clauses, create a pg_partition_encoding entry
 */
static void
add_template_encoding_clauses(Oid relid, Oid paroid, List *stenc)
{
	ListCell   *lc;

	foreach(lc, stenc)
	{
		ColumnReferenceStorageDirective *c = lfirst(lc);
		AttrNumber	attnum;

		/*
		 * Don't store default clauses since we have no need of them when we
		 * add partitions later.
		 */
		if (c->deflt)
			continue;

		attnum = get_attnum(relid, c->column);

		Insist(attnum > 0);

		add_partition_encoding(relid, paroid, attnum, c->encoding);
	}
}

Datum *
get_partition_encoding_attoptions(Relation rel, Oid paroid)
{
	Relation	pgpeenc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tup;
	Datum	   *opts;

	/*
	 * XXX XXX: should be FOR UPDATE ? why ? probably should be an AccessShare
	 */
	pgpeenc = heap_open(PartitionEncodingRelationId, RowExclusiveLock);

	opts = palloc0(sizeof(Datum) * RelationGetNumberOfAttributes(rel));

	/* SELECT * FROM pg_partition_encoding WHERE parencoid = :1 */
	ScanKeyInit(&scankey, Anum_pg_partition_encoding_parencoid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(paroid));
	sscan = systable_beginscan(pgpeenc, PartitionEncodingParencoidIndexId, true,
							   NULL, 1, &scankey);
	while (HeapTupleIsValid(tup = systable_getnext(sscan)))
	{
		Datum		paroptions;
		AttrNumber	attnum;
		bool		isnull;

		attnum = ((Form_pg_partition_encoding) GETSTRUCT(tup))->parencattnum;
		paroptions = heap_getattr(tup,
								  Anum_pg_partition_encoding_parencattoptions,
								  RelationGetDescr(pgpeenc),
								  &isnull);

		Insist(!isnull);
		Insist((attnum - 1) >= 0);

		opts[attnum - 1] = datumCopy(paroptions, false, -1);
	}

	systable_endscan(sscan);
	heap_close(pgpeenc, RowExclusiveLock);

	return opts;
}

static List *
get_deparsed_partition_encodings(Oid relid, Oid paroid)
{
	int			i;
	List	   *out = NIL;
	Relation	rel = heap_open(relid, AccessShareLock);
	Datum	   *opts = get_partition_encoding_attoptions(rel, paroid);

	for (i = 0; i < RelationGetNumberOfAttributes(rel); i++)
	{
		if (opts[i] && !rel->rd_att->attrs[i]->attisdropped)
		{
			ColumnReferenceStorageDirective *c =
			makeNode(ColumnReferenceStorageDirective);

			c->encoding = untransformRelOptions(opts[i]);
			c->column = get_attname(relid, i + 1);
			out = lappend(out, c);
		}
	}

	heap_close(rel, AccessShareLock);

	return out;
}

/**
 * Function that returns a string representation of partition oids.
 *
 * elements: an array of datums, containing oids of partitions.
 * n: length of the elements array.
 *
 * Result is allocated in the current memory context.
 */
char *
DebugPartitionOid(Datum *elements, int n)
{

	StringInfoData str;

	initStringInfo(&str);
	appendStringInfo(&str, "{");
	for (int i = 0; i < n; i++)
	{
		Oid			o = DatumGetObjectId(elements[i]);

		appendStringInfo(&str, "%s, ", get_rel_name(o));
	}
	appendStringInfo(&str, "}");
	return str.data;
}

/*
 * findPartitionMetadataEntry
 *   Find PartitionMetadata object for a given partition oid from a list.
 *
 * Input arguments:
 * partsMetadata: list of PartitionMetadata
 * partOid: Part Oid
 * partsAndRules: output parameter for matched PartitionNode
 * accessMethods: output parameter for PartitionAccessMethods
 *
 */
void
findPartitionMetadataEntry(List *partsMetadata, Oid partOid, PartitionNode **partsAndRules, PartitionAccessMethods **accessMethods)
{
	ListCell   *lc = NULL;

	foreach(lc, partsMetadata)
	{
		PartitionMetadata *metadata = (PartitionMetadata *) lfirst(lc);

		*partsAndRules = findPartitionNodeEntry(metadata->partsAndRules, partOid);

		if (NULL != *partsAndRules)
		{
			/*
			 * accessMethods define the lookup access methods for partitions,
			 * one for each level
			 */
			*accessMethods = metadata->accessMethods;
			return;
		}
	}
}

/*
 * findPartitionNodeEntry
 *   Find PartitionNode object for a given partition oid
 *
 * Input arguments:
 * partsMetadata: list of PartitionMetadata
 * partOid: Part Oid
 * return: matched PartitionNode
 */
static PartitionNode *
findPartitionNodeEntry(PartitionNode *partitionNode, Oid partOid)
{
	if (NULL == partitionNode)
	{
		return NULL;
	}

	Assert(NULL != partitionNode->part);
	if (partitionNode->part->parrelid == partOid)
	{
		return partitionNode;
	}

	/*
	 * check recursively in child parts in case we have the oid of an
	 * intermediate node
	 */
	PartitionNode *childNode = NULL;

	for (int i = 0; i < partitionNode->num_rules; i++)
	{
		PartitionRule *childRule = partitionNode->rules[i];

		childNode = findPartitionNodeEntry(childRule->children, partOid);
		if (NULL != childNode)
		{
			return childNode;
		}
	}

	/*
	 * check recursively in the default part, if any
	 */
	if (NULL != partitionNode->default_part)
	{
		childNode = findPartitionNodeEntry(partitionNode->default_part->children, partOid);
	}

	return childNode;
}

/*
 * check parition rule if contains
 * external partition table
 */
static bool
has_external_partition(PartitionNode *n)
{
	for (int i = 0; i < n->num_rules; i++)
	{
		PartitionRule *rule = n->rules[i];

		if (rel_is_external_table(rule->parchildrelid))
		{
			return true;
		}
		else
		{
			if (rule->children && has_external_partition(rule->children))
				return true;
		}
	}

	return false;
}
