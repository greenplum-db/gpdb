/*--------------------------------------------------------------------------
 *
 * cdbpartindex.c
 *	Provides utility routines to support indexes over partitioned tables
 *	within Greenplum Database.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpartindex.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/hash.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_partition_rule.h"
#include "cdb/cdbpartition.h"
#include "cdb/partitionselection.h"
#include "cdb/cdbvars.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "parser/parse_expr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/tqual.h"

/* initial estimate for number of logical indexes */
#define INITIAL_NUM_LOGICAL_INDEXES_ESTIMATE 100

static int	logicalIndexId = 1;
static int	numLogicalIndexes = 0;
static int	numIndexesOnDefaultParts = 0;

/*
 * hash table to identify distinct "logical indexes". Each index with the same
 * index key, index pred, index expr is considered equivalent.
 */
static HTAB *PartitionIndexHash;

/* hash table to hold information about distinct logical indexes. */
static HTAB *LogicalIndexInfoHash;

/*
 * Hash entry for PartitionIndexHash
 * hashkey is (indexkey + indexpred + indexexprs)
 */
typedef struct
{
	char	   *key;
	int			logicalIndexId;
} PartitionIndexHashEntry;

/*
 * Hashentry for LogicalIndexInfoHash
 * hashkey is (logicalIndexId)
 */
typedef struct
{
	char	   *key;
	int			logicalIndexId;
	Oid			logicalIndexOid;
	struct IndexInfo *ii;
	LogicalIndexType indType;
	List	   *partList;
	List	   *defaultPartList;
} LogicalIndexInfoHashEntry;

/*
 * Each node in the PartitionIndexNode tree describes a partition
 * and a list of it's logical indexes
 */
typedef struct PartitionIndexNode
{
	Oid			paroid;			/* OID of row in pg_partition */
	Oid			parparentrule;	/* OID of row in pg_partition_rule */
	Oid			parchildrelid;	/* table oid of the part */
	Oid			parrelid;		/* needed to map attnums */
	bool		isDefault;		/* is this a default part */
	Bitmapset  *index;			/* logical indexes for this part */
	List	   *children;		/* child parts */
	List	   *defaultLevels;	/* the level at which the part key is default */
} PartitionIndexNode;

static void recordIndexesOnLeafPart(PartitionIndexNode **pNodePtr,
						Oid partOid, Oid rootOid);
static void recordIndexes(PartitionIndexNode **partIndexTree);
static void getPartitionIndexNode(Oid rootOid, int16 level,
					  Oid parent, PartitionIndexNode **n,
					  bool isDefault, List *defaultLevels);
static void indexParts(PartitionIndexNode **np, bool isDefault);
static void dumpPartsIndexInfo(PartitionIndexNode *n, int level);
static LogicalIndexes *createPartsIndexResult(Oid root);
static bool collapseIndexes(PartitionIndexNode **partitionIndexNode,
				LogicalIndexInfoHashEntry **entry);
static void createIndexHashTables(void);
static IndexInfo *populateIndexInfo(Relation indRel);
static Node *mergeIntervals(Node *intervalFst, Node *intervalSnd);
static void extractStartEndRange(Node *clause, Node **ppnodeStart, Node **ppnodeEnd);
static void extractOpExprComponents(OpExpr *opexpr, Var **ppvar, Const **ppconst, Oid *opno);

/*
 * TODO: similar routines in cdbpartition.c. Move up to cdbpartition.h ?
 */

/* Hash entire string */
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
	return strcmp(((PartitionIndexHashEntry *) key1)->key, key2);
}

/* Copy string by copying pointer. */
static void *
key_string_copy(void *dest, const void *src, Size keysize)
{
	Assert(keysize == sizeof(char *));

	*((char **) dest) = (char *) src;	/* trust caller re allocation */
	return NULL;				/* not used */
}

/*-------------------------------------------------------------------------
 * BuildLogicalIndexInfo
 *   Returns an array of "logical indexes" on partitioned
 *   tables.
 *
 *   Memory for result is allocated from caller context.
 *
 *   If input relid does not represent a partitioned table, the function
 *   returns NULL.
 *
 *  This function does the following
 *	1) Builds a PartitionIndexNode tree representing the partitioned
 *	   table hierarchy.
 * 	2) Builds 2 hash tables -
 *	 	- 1st hash table is used to find the logical indexes in a
 *		  partitioning tree hierarchy - a logical
 * 		  index corresponds to multiple physical indexes each
 *		  of which have the same index key + index pred + index exprs. Each
 *		  logical index is assigned a unique logicalIndexId.
 *		- The 2nd hash table table is used to hold information associated
 *		  with a logical index for easy lookup.
 *	3) Annotates leaves of hierarchy with logical index ids.
 *	   See recordIndexes function comments.
 *	4) Consolidates logical index information i.e. if a logical index exists on all
 * 	   child parts of a parent part, the parent part is annotated with the logical
 *	   index id and logical index id is cleared from child parts.
 *	5) For each logical index, OR the check constraints of the part on which
 * 	   the index exists (i.e. generate a partial index predicate for each
 *	   logical index)
 * 	6) Since default parts, don't have constraints associated with them,
 * 	   each logical index on a default part is represented as a separate
 *	   logical index in the output. Some additional information is
 *	   included so the specific default partition may be identified.
 *
 *	Example of a partitioning hierarchy, and the output generated.
 *	I1..In are logical index ids.
 *
 *	After Step 3 (leaves annotated with logical index ids)
 *
 *				Root
 *		  / 		|		\
 *		 /		|		 \
 *		/		|		  \
 *		A1 		A2		default  - pk1 is partkey at level 0
 *	     /      \	       /    \		/       \
 *	    /	     \        /      \	       /         \
 *	   /          \	     /        \	      /           \
 * B1(I1,I2)   def(I1,I2) B1(I1,I2)  def(I2) B1(I1,I2,I4) def(I1,I2,I3) - pk2 is partkey at level 1
 *
 *
 * 	After step 4 - (consolidation of logical indexes)
 *
 *			      Root (I2)
 *		 / 		| 	       \
 *		/		|		\
 * 	       /             	|		 \
 *   	    A1 (I1)		A2		default	(I1)
 *	 /      \            /      \            /     \
 *	/        \          /        \	 	/       \
 *     /          \	   /          \	       /         \
 *    B1         def   B1(I1)         def   B1(I4)    def(I3)
 *
 * The output returned from BuildLogicalIndexInfo will be
 *
 * logicalIndexOid 	nColumns  indexKeys  indPred indExprs  indIsUnique  partCons   				defaultLevels
 * 	I2		..	  ..	     ..	     ..	       ..           -						-
 * 	I1		..	  ..	     ..	     ..	       ..           pk1=A1 OR (pk1=A2 AND pk2=B1)		-
 * 	I1'		..	  ..	     ..	     ..	       ..           -						0
 * 	I3		..	  ..	     ..	     ..	       ..           -						0,1
 * 	I4		..	  ..	     ..	     ..	       ..           pk2=B1	 				0
 *-------------------------------------------------------------------------
 */
LogicalIndexes *
BuildLogicalIndexInfo(Oid relid)
{
	MemoryContext callerContext = NULL;
	MemoryContext partContext = NULL;
	LogicalIndexes *partsLogicalIndexes = NULL;
	HASH_SEQ_STATUS hash_seq;
	LogicalIndexInfoHashEntry *entry;
	PartitionIndexNode *n = NULL;

	/*
	 * create a memory context to hold allocations, so we can get rid of at
	 * the end of this function.
	 */
	partContext = AllocSetContextCreate(CurrentMemoryContext,
										"Partitioned Logical Index Context",
										ALLOCSET_DEFAULT_MINSIZE,
										ALLOCSET_DEFAULT_INITSIZE,
										ALLOCSET_DEFAULT_MAXSIZE);

	/* switch to the new context */
	callerContext = MemoryContextSwitchTo(partContext);

	/* initialize globals */
	logicalIndexId = 1;
	numLogicalIndexes = 0;
	numIndexesOnDefaultParts = 0;

	/* construct the partition node tree */
	getPartitionIndexNode(relid, 0, InvalidOid, &n, false, NIL);

	if (!n)
		return NULL;

	/* create the hash tables to hold the logical index info */
	createIndexHashTables();


	/* now walk the tree and annotate with logical index id at each leaf */
	recordIndexes(&n);

	hash_freeze(LogicalIndexInfoHash);

	/*
	 * get rid of first hash table here as we have identified all logical
	 * indexes
	 */
	hash_destroy(PartitionIndexHash);

	/* walk the tree and pull-up indexes for each logical index */
	hash_seq_init(&hash_seq, LogicalIndexInfoHash);
	while ((entry = hash_seq_search(&hash_seq)))
	{
		collapseIndexes(&n, &entry);
		numLogicalIndexes++;
	}

	dumpPartsIndexInfo(n, 0);

	/* associate index with parts */
	indexParts(&n, false);

	/* switch to caller context and handle results */
	MemoryContextSwitchTo(callerContext);

	/* generate output rows */
	if ((numLogicalIndexes + numIndexesOnDefaultParts) > 0)
		partsLogicalIndexes = createPartsIndexResult(relid);

	hash_destroy(LogicalIndexInfoHash);

	/* all information has been copied into caller context */
	MemoryContextDelete(partContext);

	return partsLogicalIndexes;
}

/*
 * getPartitionIndexNode
 *   Construct a PartitionIndexNode tree for the given part.
 *   Recurs to construct branches.
 */
static void
getPartitionIndexNode(Oid rootOid,
					  int16 level,
					  Oid parent,
					  PartitionIndexNode **n,
					  bool isDefault,
					  List *defaultLevels)
{
	HeapTuple	tuple;
	ScanKeyData scankey[3];
	SysScanDesc sscan;
	Relation	partRel;
	Relation	partRuleRel;
	Oid			parOid;
	bool		inctemplate = false;
	Oid			parrelid;
	int			parlevel;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	/*
	 * select oid as partid, * from pg_partition where parrelid = :relid and
	 * parlevel = :level and paristemplate = false;
	 */
	partRel = heap_open(PartitionRelationId, AccessShareLock);

	ScanKeyInit(&scankey[0],
				Anum_pg_partition_parrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(rootOid));
	ScanKeyInit(&scankey[1],
				Anum_pg_partition_parlevel,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(level));
	ScanKeyInit(&scankey[2],
				Anum_pg_partition_paristemplate,
				BTEqualStrategyNumber, F_BOOLEQ,
				BoolGetDatum(inctemplate));

	sscan = systable_beginscan(partRel, PartitionParrelidParlevelParistemplateIndexId, true,
							   NULL, 3, scankey);
	tuple = systable_getnext(sscan);

	if (HeapTupleIsValid(tuple))
	{
		parOid = HeapTupleGetOid(tuple);
		Form_pg_partition partDesc = (Form_pg_partition) GETSTRUCT(tuple);
		parrelid = partDesc->parrelid;
		parlevel = partDesc->parlevel;

		if (level == 0)
		{
			Assert(parent == InvalidOid);
			Assert(*n == NULL);

			/* handle root part specially */
			*n = palloc0(sizeof(PartitionIndexNode));
			(*n)->paroid = parOid;
			(*n)->parrelid = (*n)->parchildrelid = parrelid;
			(*n)->isDefault = false;
		}
		systable_endscan(sscan);
		heap_close(partRel, AccessShareLock);
	}
	else
	{
		systable_endscan(sscan);
		heap_close(partRel, AccessShareLock);
		return;
	}

	/* recurse to fill the children */
	partRuleRel = heap_open(PartitionRuleRelationId, AccessShareLock);

	/*
	 * SELECT * FROM pg_partition_rule WHERE paroid = :1 AND parparentrule =
	 * :2
	 */
	ScanKeyInit(&scankey[0],
				Anum_pg_partition_rule_paroid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(parOid));
	ScanKeyInit(&scankey[1],
				Anum_pg_partition_rule_parparentrule,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(parent));
	sscan = systable_beginscan(partRuleRel, PartitionRuleParoidParparentruleParruleordIndexId, true,
							   NULL, 2, scankey);
	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		PartitionIndexNode *child;
		Form_pg_partition_rule rule_desc = (Form_pg_partition_rule) GETSTRUCT(tuple);

		child = palloc(sizeof(PartitionIndexNode));
		memset(child, 0, sizeof(PartitionIndexNode));
		child->paroid = HeapTupleGetOid(tuple);
		child->parrelid = parrelid;
		child->parchildrelid = rule_desc->parchildrelid;

		/*
		 * For every node, we keep track of every level at which this node has
		 * default partitioning
		 */
		child->isDefault = isDefault;
		child->defaultLevels = defaultLevels;

		/* current part is a default part */
		if (rule_desc->parisdefault)
		{
			child->isDefault = true;
			child->defaultLevels = lappend_int(child->defaultLevels, parlevel);
		}

		/* insert child into children */
		(*n)->children = lappend((*n)->children, child);

		getPartitionIndexNode(rootOid, level + 1, HeapTupleGetOid(tuple),
							  &child, child->isDefault, child->defaultLevels);
	}

	systable_endscan(sscan);
	heap_close(partRuleRel, AccessShareLock);
}

/*
 * constructIndexHashKey
 *   The index hash key is a string representing the index key,
 *   index predicate, and index exprs
 *
 *   Note, all attnums are mapped to the root.
 */
static char *
constructIndexHashKey(Oid partOid,
					  Oid rootOid,
					  AttrNumber *attMap,
					  IndexInfo *ii,
					  LogicalIndexType indType)
{
	StringInfoData buf;
	Expr	   *predExpr = NULL;
	Expr	   *exprsExpr = NULL;

	initStringInfo(&buf);

	/* map the attrnos in the indKey to root part */
	for (int i = 0; i < ii->ii_NumIndexAttrs; i++)
	{
		if (attMap && ii->ii_KeyAttrNumbers[i] != 0)
		{
			ii->ii_KeyAttrNumbers[i] = attMap[(ii->ii_KeyAttrNumbers[i]) - 1];
		}
		appendStringInfo(&buf, "%d", ii->ii_KeyAttrNumbers[i]);
	}

	/* map the attrnos in indPred */
	predExpr = (Expr *) ii->ii_Predicate;

	change_varattnos_of_a_node((Node *) predExpr, attMap);

	appendStringInfo(&buf, "%s", nodeToString(predExpr));

	/* remember mapped predicate */
	ii->ii_Predicate = (List *) predExpr;

	exprsExpr = (Expr *) ii->ii_Expressions;

	change_varattnos_of_a_node((Node *) exprsExpr, attMap);

	appendStringInfo(&buf, "%s", nodeToString(exprsExpr));
	appendStringInfo(&buf, "%d", (int) indType);

	/* remember mapped exprsList */
	ii->ii_Expressions = (List *) exprsExpr;

	return buf.data;
}

/*
 * createIndexHashTables
 *  create the hash tables to hold logical indexes
 */
static void
createIndexHashTables()
{
	MemoryContext context = NULL;
	HASHCTL		hash_ctl;

	/*
	 * this hash is to identify the logical indexes. 1 logical index
	 * corresponds to multiple physical indexes which have the same indkey +
	 * indpred + indexprs
	 */
	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(char *);
	hash_ctl.entrysize = sizeof(PartitionIndexHashEntry);
	hash_ctl.hash = key_string_hash;
	hash_ctl.match = key_string_compare;
	hash_ctl.keycopy = key_string_copy;
	hash_ctl.hcxt = context;
	PartitionIndexHash = hash_create("Partition Index Hash",
									 INITIAL_NUM_LOGICAL_INDEXES_ESTIMATE,
									 &hash_ctl,
									 HASH_ELEM | HASH_FUNCTION |
									 HASH_COMPARE | HASH_KEYCOPY |
									 HASH_CONTEXT);

	Assert(PartitionIndexHash != NULL);

	/* this hash is used to hold the logical indexes for easy lookup */
	memset(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(uint32);
	hash_ctl.entrysize = sizeof(LogicalIndexInfoHashEntry);
	hash_ctl.hash = tag_hash;
	hash_ctl.hcxt = context;
	LogicalIndexInfoHash = hash_create("Logical Index Info Hash",
									   INITIAL_NUM_LOGICAL_INDEXES_ESTIMATE,
									   &hash_ctl,
									   HASH_ELEM | HASH_FUNCTION);
	Assert(LogicalIndexInfoHash != NULL);
}

/*
 * recordIndexes
 *   Invoke the recordIndexesOnPart routine on every leaf partition.
 */
static void
recordIndexes(PartitionIndexNode **partIndexTree)
{
	ListCell   *lc;
	PartitionIndexNode *partIndexNode = *partIndexTree;

	if (partIndexNode->children)
	{
		foreach(lc, partIndexNode->children)
		{
			PartitionIndexNode *child = (PartitionIndexNode *) lfirst(lc);

			recordIndexes(&child);
		}
	}
	else
		/* at a leaf */
		recordIndexesOnLeafPart(partIndexTree,
								partIndexNode->parchildrelid,
								partIndexNode->parrelid);
}

/*
 * recordIndexesOnLeafPart
 *   Given a part
 * 	- fetch each index on the part from pg_index
 *	- map attnums in index key, index expr, index pred to the root attnums
 *	- insert the logical index into the PartitionIndexHash using the
 *		index key, index exprs, index pred as hash  key.
 *	- fetch the associated logical index id from the hash and record it in the tree.
 *	- insert the logical index into the LogicalIndexInfo Hash using the logical index id
 * 	  as the hash key.
 */
static void
recordIndexesOnLeafPart(PartitionIndexNode **pNodePtr,
						Oid partOid,
						Oid rootOid)
{
	char	   *partIndexHashKey;
	bool		foundPartIndexHash;
	bool		foundLogicalIndexHash;
	AttrNumber *attmap = NULL;
	struct IndexInfo *ii = NULL;
	PartitionIndexHashEntry *partIndexHashEntry;
	LogicalIndexInfoHashEntry *logicalIndexInfoHashEntry;
	PartitionIndexNode *pNode = *pNodePtr;
	List	   *indexoidlist;
	ListCell   *lc;

	Relation	partRel = heap_open(partOid, AccessShareLock);

	char		relstorage = partRel->rd_rel->relstorage;

	/* fetch each index on part */
	indexoidlist = RelationGetIndexList(partRel);
	foreach(lc, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(lc);
		LogicalIndexType indType = INDTYPE_BITMAP;
		Relation	indRel;

		/*
		 * We're holding a lock on the table. We assume that's enough to
		 * prevent concurrent DDL on the index.
		 */
		indRel = index_open(indexoid, NoLock);

		if (GIN_AM_OID == indRel->rd_rel->relam)
		{
			indType = INDTYPE_GIN;
		}
		else if (GIST_AM_OID == indRel->rd_rel->relam)
		{
			indType = INDTYPE_GIST;
		}
		else if (RELSTORAGE_HEAP == relstorage && BTREE_AM_OID == indRel->rd_rel->relam)
		{
			/*
			 * we only send btree indexes as type btree if it is a normal heap table,
			 * AO and AOCO tables with btree indexes are sent as type bitmap
			 */
			indType = INDTYPE_BTREE;
		}

		/* 
		 * when constructing hash key, we need to map attnums in part indexes
		 * to root attnums. Get the attMap needed for mapping.
		 */
		if (!attmap)
		{
			Relation	rootRel = heap_open(rootOid, AccessShareLock);

			TupleDesc	rootTupDesc = rootRel->rd_att;
			TupleDesc	partTupDesc = partRel->rd_att;

			attmap = varattnos_map(partTupDesc, rootTupDesc);

			/* can we close here ? */
			heap_close(rootRel, AccessShareLock);
		}

		/* populate index info structure */
		ii = populateIndexInfo(indRel);

		index_close(indRel, NoLock);

		/* construct hash key for the index */
		partIndexHashKey = constructIndexHashKey(partOid, rootOid, attmap, ii, indType);

		/* lookup PartitionIndexHash table */
		partIndexHashEntry = (PartitionIndexHashEntry *) hash_search(PartitionIndexHash,
																	 (void *) partIndexHashKey,
																	 HASH_ENTER,
																	 &foundPartIndexHash);

		if (!foundPartIndexHash)
		{
			/* first time seeing this index */
			partIndexHashEntry->key = partIndexHashKey;

			/* assign a logical index id */
			partIndexHashEntry->logicalIndexId = logicalIndexId++;

			/*
			 * insert the entry into LogicalIndexInfoHash to keep track of
			 * logical indexes
			 */
			logicalIndexInfoHashEntry = (LogicalIndexInfoHashEntry *) hash_search(LogicalIndexInfoHash,
																				  (void *) &(partIndexHashEntry->logicalIndexId),
																				  HASH_ENTER,
																				  &foundLogicalIndexHash);


			if (foundLogicalIndexHash)
			{
				/*
				 * we should not find the entry in the logical index hash as
				 * this is the first time we are seeing it
				 */
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("error during BuildLogicalIndexInfo. Found indexrelid \"%d\" in hash",
								indexoid
								)));
			}

			logicalIndexInfoHashEntry->logicalIndexId = partIndexHashEntry->logicalIndexId;
			logicalIndexInfoHashEntry->logicalIndexOid = indexoid;
			logicalIndexInfoHashEntry->ii = ii;
			logicalIndexInfoHashEntry->partList = NIL;
			logicalIndexInfoHashEntry->defaultPartList = NIL;
			logicalIndexInfoHashEntry->indType = indType;
		}
		else
		{
			/*
			 * we can release IndexInfo as we already have the information in
			 * the hash
			 */
			if (ii->ii_Expressions)
				pfree(ii->ii_Expressions);
			if (ii->ii_Predicate)
				pfree(ii->ii_Predicate);
			pfree(ii);
		}


		/* update the PartitionIndexNode -> index bitmap */
		pNode->index = bms_add_member(pNode->index, partIndexHashEntry->logicalIndexId);
	}

	heap_close(partRel, AccessShareLock);
}

/*
 * collapseIndexes
 *   Walk the PartitionNode tree
 *	- For every node
 *		- If a logical index exists on all the children of the node, record the index
 *	  	  on the node and wipeout the index from the children
 *	- recurse up the tree to the root.
 */
static bool
collapseIndexes(PartitionIndexNode **partitionIndexNode,
				LogicalIndexInfoHashEntry **entry)
{
	ListCell   *lc,
			   *lc1;
	PartitionIndexNode *pn = *partitionIndexNode;
	bool		exists = TRUE;
	int			lid = (*entry)->logicalIndexId;
	int			childStatus = TRUE;

	if (pn->children)
	{
		foreach(lc, pn->children)
		{
			PartitionIndexNode *child = (PartitionIndexNode *) lfirst(lc);

			/* not a leaf, so check the children */
			exists = collapseIndexes(&child, entry);

			/* For every logical index we are traversing the WHOLE tree. */
			if (!exists)
			{
				/* there is a child that doesn't have the index */
				childStatus = FALSE;
			}
		}

		if (childStatus)
		{
			/* if index exists on every child, record in the parent */
			pn->index = bms_add_member(pn->index, lid);

			/* remove the index from the children */
			foreach(lc1, pn->children)
			{
				PartitionIndexNode *child = (PartitionIndexNode *) lfirst(lc1);

				bms_del_member(child->index, lid);
			}
		}
	}
	else
	{
		/* a leaf - check if index exists */
		if (!bms_is_member(lid, pn->index))
			exists = FALSE;
	}

	return exists;
}

/*
 * indexParts
 *   Walks the tree of nodes and for each logical index that exists on the node,
 *   records the node (partition id) with the logical index in the hash.
 *   If the logical index exists on a default part, record some additional
 *   information to indentify the specific default part.
 *
 *   This consolidates all the information regarding the logical index in the
 *   hash table. As we exit from the function we have a hash table with an entry
 *   for every logical index
 * 	logical index id -> (logicalIndexOid, IndexInfo, partList, defaultPartList)
 */
static void
indexParts(PartitionIndexNode **np, bool isDefault)
{
	LogicalIndexInfoHashEntry *entry;
	PartitionIndexNode *n = *np;
	bool		found;
	int			x;

	if (!n)
		return;

	x = -1;
	while ((x = bms_next_member(n->index, x)) >= 0)
	{
		entry = (LogicalIndexInfoHashEntry *) hash_search(LogicalIndexInfoHash,
														  (void *) &x, HASH_FIND, &found);
		if (!found)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("error during BuildLogicalIndexInfo. Index not found \"%d\" in hash",
							x)));
		}

		if (n->isDefault || isDefault)
		{
			/*
			 * We keep track of the default node in the PartitionIndexNode
			 * tree, since we need to output the defaultLevels information to
			 * the caller.
			 */
			entry->defaultPartList = lappend(entry->defaultPartList, n);
			numIndexesOnDefaultParts++;
		}
		else
		{
			/*
			 * For regular non-default parts we just track the part oid which
			 * will be used to get the part constraint.
			 */
			entry->partList = lappend_oid(entry->partList, n->parchildrelid);
		}
	}

	if (n->children)
	{
		ListCell   *lc;

		foreach(lc, n->children)
		{
			PartitionIndexNode *child = (PartitionIndexNode *) lfirst(lc);

			indexParts(&child, n->isDefault);
		}
	}
}

/*
 * getPartConstraints
 *   Given an OID, returns a Node that represents all the check constraints
 *   on the table AND'd together, only if these constraints cover the keys in
 *   the given list. Otherwise, it returns NULL
 */
static Node *
getPartConstraints(Oid partOid, Oid rootOid, List *partKey)
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
	AttrMap    *map;

	/* create the map needed for mapping attnums */
	Relation	rootRel = heap_open(rootOid, AccessShareLock);
	Relation	partRel = heap_open(partOid, AccessShareLock);

	map_part_attrs(partRel, rootRel, &map, false);

	heap_close(rootRel, AccessShareLock);
	heap_close(partRel, AccessShareLock);

	/* Fetch the pg_constraint row. */
	conRel = heap_open(ConstraintRelationId, AccessShareLock);

	ScanKeyInit(&scankey,
				Anum_pg_constraint_conrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(partOid));
	sscan = systable_beginscan(conRel, ConstraintRelidIndexId, true,
							   NULL, 1, &scankey);

	/* list of keys referenced in the found constraints */
	List	   *keys = NIL;

	while (HeapTupleIsValid(conTup = systable_getnext(sscan)))
	{
		/*
		 * we defer the filter on contype to here in order to take advantage
		 * of the index on conrelid
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

		if (result)
			result = (Node *) make_andclause(list_make2(result, conExpr));
		else
			result = conExpr;

		/* fetch the key associated with this constraint */
		conKeyDatum = heap_getattr(conTup, Anum_pg_constraint_conkey,
								   RelationGetDescr(conRel), &conKeyIsNull);
		Datum	   *dats = NULL;
		int			numKeys = 0;

		/* extract key elements */
		deconstruct_array(DatumGetArrayTypeP(conKeyDatum), INT2OID, 2, true, 's', &dats, NULL, &numKeys);
		for (int i = 0; i < numKeys; i++)
		{
			int16		key_elem = DatumGetInt16(dats[i]);

			keys = lappend_int(keys, key_elem);
		}
	}

	systable_endscan(sscan);
	heap_close(conRel, AccessShareLock);

	ListCell   *lc = NULL;

	foreach(lc, partKey)
	{
		int			partKeyCol = lfirst_int(lc);

		if (!list_member_int(keys, partKeyCol))
		{
			/* passed in key is not found in the constraint. return NULL */
			if (NULL != result)
			{
				pfree(result);
			}

			result = NULL;
			break;
		}
	}

	if (map)
	{
		pfree(map);
	}
	list_free(keys);
	return result;
}

/*
 * get_relation_part_constraints
 *  return the part constraints for a partitioned table given the oid of the root
 */
Node *
get_relation_part_constraints(Oid rootOid, List **defaultLevels)
{
	if (!rel_is_partitioned(rootOid))
	{
		return NULL;
	}

	/* get number of partitioning levels */
	List	   *partkeys = rel_partition_keys_ordered(rootOid);
	int			nLevels = list_length(partkeys);

	Node	   *allCons = NULL;

	for (int level = 0; level < nLevels; level++)
	{
		List	   *partKey = (List *) list_nth(partkeys, level);

		PartitionNode *pn = get_parts(rootOid, level, 0 /* parent */ , false /* inctemplate */ , false /* includesubparts */ );

		Assert(NULL != pn);
		List	   *partOids = all_partition_relids(pn);

		Node	   *partCons = NULL;
		ListCell   *lc = NULL;

		foreach(lc, partOids)
		{
			Oid			partOid = lfirst_oid(lc);

			/* fetch part constraint mapped to root */
			partCons = getPartConstraints(partOid, rootOid, partKey);

			if (NULL == partCons)
			{
				/* default partition has NULL constriant in GPDB's catalog */
				*defaultLevels = lappend_int(*defaultLevels, level);
				continue;
			}

			/* OR them to current constraints */
			if (NULL == allCons)
			{
				allCons = partCons;
			}
			else
			{
				allCons = (Node *) mergeIntervals(allCons, partCons);
			}
		}
	}

	if (NULL == allCons)
	{
		allCons = makeBoolConst(false /* value */ , false /* isnull */ );
	}

	list_free(partkeys);
	return allCons;
}

/*
 * populateIndexInfo
 *  Populate the IndexInfo structure with the information from pg_index tuple.
 */
static IndexInfo *
populateIndexInfo(Relation indRel)
{
	IndexInfo  *ii;

	/*
	 * We could just call existing BuildIndexInfo here. But BuildIndexInfo
	 * needs to open each index, and populate the relcache entry for the index
	 * relation. We can get the information by just looking into pg_index
	 * tuple.
	 */
	ii = makeNode(IndexInfo);

	ii->ii_Unique = indRel->rd_index->indisunique;
	ii->ii_NumIndexAttrs = indRel->rd_index->indnatts;

	for (int i = 0; i < indRel->rd_index->indnatts; i++)
	{
		ii->ii_KeyAttrNumbers[i] = indRel->rd_index->indkey.values[i];
	}

	ii->ii_Expressions = RelationGetIndexExpressions(indRel);
	ii->ii_Predicate = RelationGetIndexPredicate(indRel);

	return ii;
}

/*
 * generateLogicalIndexPred
 *  generate the partial index predicate associated with each logical index.
 *  The predicate is generated by OR'ing the constraints of all parts on which the logical
 *  index exists.
 */
static void
generateLogicalIndexPred(LogicalIndexes *li,
						 int *curIdx,
						 LogicalIndexInfoHashEntry *entry,
						 Oid root,
						 int *numLogicalIndexes)
{
	Node	   *conList;
	ListCell   *lc;
	AttrNumber *indexKeys;

	if (list_length(entry->partList) > 0)
	{
		/* populate the index information */
		li->logicalIndexInfo[*curIdx]->partCons = NULL;
		li->logicalIndexInfo[*curIdx]->logicalIndexOid = entry->logicalIndexOid;
		li->logicalIndexInfo[*curIdx]->nColumns = entry->ii->ii_NumIndexAttrs;
		indexKeys = palloc(sizeof(AttrNumber) * entry->ii->ii_NumIndexAttrs);
		memcpy((char *) indexKeys,
			   &(entry->ii->ii_KeyAttrNumbers), sizeof(AttrNumber) * entry->ii->ii_NumIndexAttrs);
		li->logicalIndexInfo[*curIdx]->indexKeys = indexKeys;
		li->logicalIndexInfo[*curIdx]->indExprs = (List *) copyObject(entry->ii->ii_Expressions);
		li->logicalIndexInfo[*curIdx]->indPred = (List *) copyObject(entry->ii->ii_Predicate);
		li->logicalIndexInfo[*curIdx]->indIsUnique = entry->ii->ii_Unique;
		li->logicalIndexInfo[*curIdx]->indType = entry->indType;

		/* fetch the partList from the logical index hash entry */
		foreach(lc, entry->partList)
		{
			Oid			partOid = lfirst_oid(lc);

			if (partOid != root)
			{
				/* fetch part constraint mapped to root */
				conList = getPartConstraints(partOid, root, NIL /* partKey */ );

				/* OR them to current constraints */
				if (li->logicalIndexInfo[*curIdx]->partCons)
				{
					li->logicalIndexInfo[*curIdx]->partCons = mergeIntervals(li->logicalIndexInfo[*curIdx]->partCons, conList);
				}
				else
				{
					li->logicalIndexInfo[*curIdx]->partCons = conList;
				}
			}
		}

		(*curIdx)++;
		(*numLogicalIndexes)++;
	}

	/* from the hash entry, fetch the defaultPartList */
	foreach(lc, entry->defaultPartList)
	{
		PartitionIndexNode *node = (PartitionIndexNode *) lfirst(lc);
		AttrNumber *indexKeys;

		li->logicalIndexInfo[*curIdx]->partCons = NULL;
		li->logicalIndexInfo[*curIdx]->logicalIndexOid = entry->logicalIndexOid;
		li->logicalIndexInfo[*curIdx]->nColumns = entry->ii->ii_NumIndexAttrs;
		indexKeys = palloc(sizeof(AttrNumber) * entry->ii->ii_NumIndexAttrs);
		memcpy((char *) indexKeys,
			   &(entry->ii->ii_KeyAttrNumbers), sizeof(AttrNumber) * entry->ii->ii_NumIndexAttrs);
		li->logicalIndexInfo[*curIdx]->indexKeys = indexKeys;
		li->logicalIndexInfo[*curIdx]->indExprs = (List *) copyObject(entry->ii->ii_Expressions);
		li->logicalIndexInfo[*curIdx]->indPred = (List *) copyObject(entry->ii->ii_Predicate);
		li->logicalIndexInfo[*curIdx]->indIsUnique = entry->ii->ii_Unique;
		li->logicalIndexInfo[*curIdx]->indType = entry->indType;

		/*
		 * Default parts don't have constraints, so they cannot be represented
		 * as part of the partial index predicate.
		 *
		 * Instead each index on a default part is represented as a different
		 * logical index. Since we need a way to identify the specific default
		 * part, we include the defaultLevels information, in addition to ANY
		 * constraint on the default part.
		 */
		li->logicalIndexInfo[*curIdx]->partCons = getPartConstraints(node->parchildrelid, root, NIL /* partKey */ );

		/* get the level on which partitioning key is default */
		li->logicalIndexInfo[*curIdx]->defaultLevels = list_copy(node->defaultLevels);
		(*curIdx)++;
		(*numLogicalIndexes)++;
	}
}

/*
 * createPartsIndexResult
 *  Populate the result array of logical indexes. Most of the information needed
 *  is in the IndexInfoHash. The partial index predicate needs to be generated
 *  by a call to generateLogicalIndexPred.
 */
static LogicalIndexes *
createPartsIndexResult(Oid root)
{
	HASH_SEQ_STATUS hash_seq;
	int			numResultRows = 0;
	LogicalIndexes *li;
	LogicalIndexInfoHashEntry *entry;

	hash_seq_init(&hash_seq, LogicalIndexInfoHash);

	/*
	 * we allocate space for twice the number of logical indexes. this is
	 * because, every index on a default part will be returned as a separate
	 * index.
	 */
	numResultRows = numLogicalIndexes + numIndexesOnDefaultParts;

	li = (LogicalIndexes *) palloc0(sizeof(LogicalIndexes));

	li->numLogicalIndexes = 0;
	li->logicalIndexInfo = NULL;

	li->logicalIndexInfo = (LogicalIndexInfo **) palloc0(numResultRows * sizeof(LogicalIndexInfo *));

	/* should we limit the amount of memory being allocated here ? */
	for (int i = 0; i < numResultRows; i++)
		li->logicalIndexInfo[i] = (LogicalIndexInfo *) palloc0(sizeof(LogicalIndexInfo));

	int			curIdx = 0;

	/*
	 * for each logical index, get the part constraints as partial index
	 * predicates
	 */
	while ((entry = hash_seq_search(&hash_seq)))
	{
		generateLogicalIndexPred(li, &curIdx, entry, root, &li->numLogicalIndexes);
	}

	return li;
}

/*
 * getPhysicalIndexRelid
 *   This function returns the physical index relid corresponding
 *   to the index described by the logicalIndexInfo input structure
 *   in the relation identified by the partOid.
 *   The 1st matching index is returned.
 *
 * NOTE: index attnums in the provided logicalIndexInfo input structure
 * have to be mapped to the root.
 */
Oid
getPhysicalIndexRelid(Relation partRel, LogicalIndexInfo *iInfo)
{
	Oid			resultOid = InvalidOid;
	bool		indKeyEqual;
	List	   *indexoidlist;
	ListCell   *lc;
	Relation	indRel = NULL;

	/* fetch each index on partition */
	indexoidlist = RelationGetIndexList(partRel);
	foreach(lc, indexoidlist)
	{
		Oid			indexoid = lfirst_oid(lc);
		Form_pg_index indForm;

		if (indRel)
			index_close(indRel, NoLock);

		/*
		 * We're holding a lock on the table. We assume that's enough to
		 * prevent concurrent DDL on the index.
		 */
		indRel = index_open(indexoid, NoLock);
		indForm = indRel->rd_index;

		if (indForm->indnatts != iInfo->nColumns)
			continue;

		/* compare indKey */
		indKeyEqual = true;
		for (int i = 0; i < indForm->indnatts; i++)
		{
			if (indForm->indkey.values[i] != iInfo->indexKeys[i])
			{
				indKeyEqual = false;
				break;
			}
		}

		if (!indKeyEqual)
			continue;

		/* compare uniqueness attribute */
		if (iInfo->indIsUnique != indForm->indisunique)
			continue;

		/* compare indPred */
		List	   *indPred = RelationGetIndexPredicate(indRel);

		if (!equal(iInfo->indPred, indPred))
			continue;
		list_free(indPred);

		/* compare indExprs */
		List	   *indExprs = RelationGetIndexExpressions(indRel);

		if (equal(iInfo->indExprs, indExprs))
		{
			/* found matching index */
			resultOid = indForm->indexrelid;
			break;
		}
		list_free(indExprs);

		/* TODO: antova - Mar 28, 2014; compare if this is a bitmap index */
	}

	if (indRel)
		index_close(indRel, NoLock);

	return resultOid;
}

/*
 * dumpPartsIndexInfo
 *   print debugging info
 */
static void
dumpPartsIndexInfo(PartitionIndexNode *n, int level)
{
	ListCell   *lc;
	StringInfoData logicalIndexes;
	int			x;

	initStringInfo(&logicalIndexes);

	if (!n)
		return;

	for (int i = 0; i <= level; i++)
		appendStringInfo(&logicalIndexes, "%s", "   ");

	appendStringInfo(&logicalIndexes, "%d ", n->parchildrelid);

	appendStringInfo(&logicalIndexes, "%s ", " (");

	x = -1;
	while ((x = bms_next_member(n->index, x)) >= 0)
		appendStringInfo(&logicalIndexes, "%d ", x);

	appendStringInfo(&logicalIndexes, "%s ", " )");

	elog(DEBUG5, "%s", logicalIndexes.data);

	if (n->children)
	{
		foreach(lc, n->children)
		{
			PartitionIndexNode *child = (PartitionIndexNode *) lfirst(lc);

			dumpPartsIndexInfo(child, level + 1);
			appendStringInfo(&logicalIndexes, "     ");
		}
	}
}

/*
 * 	mergeIntervals
 *   merge the two intervals by creating a disjunction of the given intervals, or collapsing
 *   them into one interval when possible
 *
 *   This function's arguments represent two range constraints, which the function attempts
 *   to collapse into one if they share a common boundary. If no collapse is possible,
 *   the function returns a disjunction of the two intervals.
 */
static Node *
mergeIntervals(Node *intervalFst, Node *intervalSnd)
{
	Node	   *pnodeStart1 = NULL;
	Node	   *pnodeEnd1 = NULL;
	Node	   *pnodeStart2 = NULL;
	Node	   *pnodeEnd2 = NULL;

	extractStartEndRange(intervalFst, &pnodeStart1, &pnodeEnd1);
	extractStartEndRange(intervalSnd, &pnodeStart2, &pnodeEnd2);

	if (NULL != pnodeEnd1 && NULL != pnodeStart2)
	{
		OpExpr	   *opexprEnd1 = (OpExpr *) pnodeEnd1;
		OpExpr	   *opexprStart2 = (OpExpr *) pnodeStart2;
		Var		   *pvarEnd1 = NULL;
		Var		   *pvarStart2 = NULL;
		Const	   *pconstEnd1 = NULL;
		Const	   *pconstStart2 = NULL;
		Oid			opnoEnd1 = InvalidOid;
		Oid			opnoStart2 = InvalidOid;

		/* extract boundaries of the two intervals */
		extractOpExprComponents(opexprEnd1, &pvarEnd1, &pconstEnd1, &opnoEnd1);
		extractOpExprComponents(opexprStart2, &pvarStart2, &pconstStart2, &opnoStart2);
		if (InvalidOid != opnoEnd1 && InvalidOid != opnoStart2 &&
			equal(pvarEnd1, pvarStart2) && equal(pconstEnd1, pconstStart2) &&	/* middle point is the
																				 * same */
			opnoEnd1 == get_negator(opnoStart2) /* this guaranteed that the
												 * middle point is included in
												 * exactly 1 of the intervals */
			)
		{
			if (NULL != pnodeStart1 && NULL != pnodeEnd2)
			{
				/* merge intervals of the form (x,y), [y, z) into (x,z) */
				return (Node *) make_andclause(list_make2(pnodeStart1, pnodeEnd2));
			}

			if (NULL != pnodeStart1)
			{
				/* merge intervals of the form (x,y), [y, inf) into (x, inf) */
				Var		   *pvarStart1 = NULL;
				Const	   *pconstStart1 = NULL;
				Oid			opnoStart1 = InvalidOid;
				OpExpr	   *opexprStart1 = (OpExpr *) pnodeStart1;

				extractOpExprComponents(opexprStart1, &pvarStart1, &pconstStart1, &opnoStart1);

				return (Node *) make_opclause(opnoStart1,
											  opexprStart1->opresulttype,
											  opexprStart1->opretset,
											  (Expr *) pvarStart1,
											  (Expr *) pconstStart1,
											  opexprStart1->opcollid,
											  opexprStart1->inputcollid);
			}

			if (NULL != pnodeEnd2)
			{
				/* merge intervals of the form (-inf,x), [x, y) into (-inf, y) */
				Var		   *pvarEnd2 = NULL;
				Const	   *pconstEnd2 = NULL;
				Oid			opnoEnd2 = InvalidOid;
				OpExpr	   *opexprEnd2 = (OpExpr *) pnodeEnd2;

				extractOpExprComponents(opexprEnd2, &pvarEnd2, &pconstEnd2, &opnoEnd2);

				return (Node *) make_opclause(opnoEnd2,
											  opexprEnd2->opresulttype,
											  opexprEnd2->opretset,
											  (Expr *) pvarEnd2,
											  (Expr *) pconstEnd2,
											  opexprEnd2->opcollid,
											  opexprEnd2->inputcollid);
			}

			Assert(NULL == pnodeStart1 && NULL == pnodeEnd2);

			/* merge (-inf, x), [x,inf) into true */

			return (Node *) makeBoolConst(true /* value */ , false /* isnull */ );
		}
	}
	return (Node *) make_orclause(list_make2(intervalFst, intervalSnd));
}

/*
 * extractStartEndRange
 *   Given an expression representing an interval constraint, extract the expressions defining the interval's start and end
 */
static void
extractStartEndRange(Node *clause, Node **ppnodeStart, Node **ppnodeEnd)
{
	if (IsA(clause, OpExpr))
	{
		OpExpr	   *opexpr = (OpExpr *) clause;
		Expr	   *pexprRight = list_nth(opexpr->args, 1);
		CmpType		cmptype = get_comparison_type(opexpr->opno);

		if (CmptEq == cmptype)
		{
			*ppnodeStart = *ppnodeEnd = clause;
			return;
		}

		if ((CmptLEq == cmptype || CmptLT == cmptype) && IsA(pexprRight, Const))
		{
			/* op expr of the form pk <= Const or pk < Const */
			*ppnodeStart = NULL;
			*ppnodeEnd = clause;
			return;
		}

		if ((CmptGEq == cmptype || CmptGT == cmptype) && IsA(pexprRight, Const))
		{
			/* op expr of the form pk >= Const or pk > Const */
			*ppnodeEnd = NULL;
			*ppnodeStart = clause;
			return;
		}
	}

	/*
	 * only expressions of the form x > C1 and x < C2 supported: ignore all
	 * other expression types
	 */

	if (!IsA(clause, BoolExpr) ||
		AND_EXPR != ((BoolExpr *) clause)->boolop ||
		2 < list_length(((BoolExpr *) clause)->args))
	{
		return;
	}

	BoolExpr   *bool_expr = (BoolExpr *) clause;

	Node	   *nodeStart = list_nth(bool_expr->args, 0);
	Node	   *nodeEnd = list_nth(bool_expr->args, 1);

	if (!IsA(nodeStart, OpExpr) ||!IsA(nodeEnd, OpExpr))
	{
		return;
	}

	OpExpr	   *opexprStart = (OpExpr *) nodeStart;
	OpExpr	   *opexprEnd = (OpExpr *) nodeEnd;

	CmpType		cmptLeft = get_comparison_type(opexprStart->opno);
	CmpType		cmptRight = get_comparison_type(opexprEnd->opno);

	if ((CmptGT != cmptLeft && CmptGEq != cmptLeft) || (CmptLT != cmptRight && CmptLEq != cmptRight))
	{
		return;
	}

	*ppnodeStart = nodeStart;
	*ppnodeEnd = nodeEnd;
}

/*
 * extractOpExprComponents
 *   for an opexpr of type Var op Const, extract its components
 */
static void
extractOpExprComponents(OpExpr *opexpr, Var **ppvar, Const **ppconst, Oid *opno)
{
	Node	   *pnodeLeft = list_nth(opexpr->args, 0);
	Node	   *pnodeRight = list_nth(opexpr->args, 1);

	if (IsA(pnodeLeft, Var) &&IsA(pnodeRight, Const))
	{
		*ppvar = (Var *) pnodeLeft;
		*ppconst = (Const *) pnodeRight;
		*opno = opexpr->opno;
	}
	else if (IsA(pnodeLeft, Const) &&IsA(pnodeRight, Var) &&InvalidOid != get_commutator(opexpr->opno))
	{
		*ppvar = (Var *) pnodeRight;
		*ppconst = (Const *) pnodeLeft;
		*opno = get_commutator(opexpr->opno);
	}
}


/*
 * LogicalIndexInfoForIndexOid
 *   construct the logical index info for a given index oid
 */
LogicalIndexInfo *
logicalIndexInfoForIndexOid(Oid rootOid, Oid indexOid)
{
	Relation	indRel;

	indRel = index_open(indexOid, AccessShareLock);

	IndexInfo  *pindexInfo = populateIndexInfo(indRel);
	Oid			partOid = indRel->rd_index->indrelid;

	/* remap attributes */
	Relation	rootRel = heap_open(rootOid, AccessShareLock);
	Relation	partRel = heap_open(partOid, AccessShareLock);

	TupleDesc	rootTupDesc = rootRel->rd_att;
	TupleDesc	partTupDesc = partRel->rd_att;

	char		relstorage = partRel->rd_rel->relstorage;
	AttrNumber *attMap = varattnos_map(rootTupDesc, partTupDesc);

	heap_close(rootRel, AccessShareLock);
	heap_close(partRel, AccessShareLock);

	/* populate the index information */
	LogicalIndexInfo *plogicalIndexInfo = (LogicalIndexInfo *) palloc0(sizeof(LogicalIndexInfo));

	plogicalIndexInfo->nColumns = pindexInfo->ii_NumIndexAttrs;
	plogicalIndexInfo->indIsUnique = pindexInfo->ii_Unique;
	plogicalIndexInfo->logicalIndexOid = indexOid;

	int			numIndexKeys = pindexInfo->ii_NumIndexAttrs;

	plogicalIndexInfo->indexKeys = palloc(sizeof(AttrNumber) * numIndexKeys);
	memcpy((char *) plogicalIndexInfo->indexKeys, &(pindexInfo->ii_KeyAttrNumbers), sizeof(AttrNumber) * numIndexKeys);

	plogicalIndexInfo->indExprs = copyObject(pindexInfo->ii_Expressions);
	plogicalIndexInfo->indPred = copyObject(pindexInfo->ii_Predicate);

	/* remap expression fields */
	/* map the attrnos if necessary */
	if (attMap)
	{
		for (int i = 0; i < plogicalIndexInfo->nColumns; i++)
		{
			if (plogicalIndexInfo->indexKeys[i] != 0)
			{
				plogicalIndexInfo->indexKeys[i] = attMap[(plogicalIndexInfo->indexKeys[i]) - 1];
			}
		}
		change_varattnos_of_a_node((Node *) plogicalIndexInfo->indExprs, attMap);
		change_varattnos_of_a_node((Node *) plogicalIndexInfo->indPred, attMap);
	}

	plogicalIndexInfo->indType = INDTYPE_BITMAP;
	if (GIN_AM_OID == indRel->rd_rel->relam)
	{
		plogicalIndexInfo->indType = INDTYPE_GIN;
	}
	else if (GIST_AM_OID == indRel->rd_rel->relam)
	{
		plogicalIndexInfo->indType = INDTYPE_GIST;
	}
	else if (RELSTORAGE_HEAP == relstorage && BTREE_AM_OID == indRel->rd_rel->relam)
	{
		plogicalIndexInfo->indType = INDTYPE_BTREE;
	}

	index_close(indRel, AccessShareLock);

	return plogicalIndexInfo;
}
