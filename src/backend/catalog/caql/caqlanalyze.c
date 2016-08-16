/*-------------------------------------------------------------------------
 *
 * caqlanalyze.c
 *	  parser analyzer for CaQL.
 *
 * The analysis should be quick.  We try not to use palloc or heavy process
 * in this file.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "access/heapam.h"
#include "catalog/catcore.h"
#include "catalog/catquery.h"
#include "catalog/caqlparse.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpersistentstore.h"
#include "utils/array.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/syscache.h"


/*
 * Settings of parser cache.  NUM is the total element length and
 * PER_BUCKET is elements per bucket.  Because we use division, make sure
 * NUM / PER_BUCKET is divisible.
 */
#define CAQL_PARSER_CACHE_NUM 256
#define CAQL_PARSER_CACHE_PER_BUCKET 8
#define CAQL_PARSER_CACHE_BUCKET \
	(CAQL_PARSER_CACHE_NUM / CAQL_PARSER_CACHE_PER_BUCKET)

/*
 * The cache information.  We store flat cookie in an entry
 * so that we can avoid additional memory allocation.
 */
typedef struct caql_parser_cache
{
	uint32				hval;		/* hash value */
	MemoryContext		context;	/* private memory context */
	caql_hash_cookie	cookie;		/* body */
} caql_parser_cache;

/* The parser cache array. */
static caql_parser_cache parser_cache[CAQL_PARSER_CACHE_NUM];

/* Prototypes */
static SysScanDesc caql_basic_fn_all(caql_hash_cookie *pchn,
				  cqContext *pCtx, cq_list *pcql, bool bLockEntireTable);
static List *caql_query_predicate(Node *query);
static bool caql_process_predicates(caql_hash_cookie *pchn,
									List *predicates);
static bool caql_lookup_predicate(caql_hash_cookie *pchn, CaQLExpr *expr,
						   AttrNumber *attnum, StrategyNumber *strategy, Oid *fnoid,
						   Oid *typid);
static const CatCoreIndex *caql_find_index(caql_hash_cookie *pchn,
											Node *query);
static const CatCoreIndex *caql_find_index_by(caql_hash_cookie *pchn,
											   List *keylist, bool is_orderby);
static caql_hash_cookie *caql_get_parser_cache(const char *str, unsigned int len);
static void caql_put_parser_cache(const char *str, unsigned int len,
									caql_hash_cookie *cookie);


/*
 * Preprocess the query string and build up hash_cookie, which will be
 * passed to caql_switch later.
 */
struct caql_hash_cookie *
cq_lookup(const char *str, unsigned int len, cq_list *pcql)
{
	Node				   *query;
	struct caql_hash_cookie *hash_cookie;

	hash_cookie = caql_get_parser_cache(str, len);
	if (hash_cookie != NULL)
		return hash_cookie;

	query = caql_raw_parser(str, (const char *) pcql->filename, pcql->lineno);
	if (query == NULL)
		return NULL;

	hash_cookie = palloc0(sizeof(struct caql_hash_cookie));
	switch(nodeTag(query))
	{
	case T_CaQLSelect:
		{
			CaQLSelect	   *node = (CaQLSelect *) query;
			char		   *attname;

			if (node->forupdate)
				hash_cookie->bUpdate = true;
			if (node->count)
				hash_cookie->bCount = true;

			hash_cookie->relation = catcore_lookup_rel(node->from);
			if (hash_cookie->relation == NULL)
				elog(ERROR, "could not find relation \"%s\" in %s at %s:%d",
							node->from, str, pcql->filename, pcql->lineno);

			attname = strVal(linitial(node->targetlist));

			/*
			 * Look up attribute number if target list has a column.
			 * '*' includes count('*').  The first character test is
			 * not wrong due to the syntax limitation, and this is quick.
			 */
			if (attname[0] != '*')
			{
				hash_cookie->attnum =
					catcore_lookup_attnum(hash_cookie->relation, attname,
										  &hash_cookie->atttype);
				if (hash_cookie->attnum == InvalidAttrNumber)
					elog(ERROR, "could not find attribute \"%s\" in %s at %s:%d",
								attname, str, pcql->filename, pcql->lineno);
			}

			hash_cookie->bAllEqual =
				caql_process_predicates(hash_cookie, node->where);
		}
		break;
	case T_CaQLInsert:
		{
			CaQLInsert	   *node = (CaQLInsert *) query;

			hash_cookie->bInsert = true;

			hash_cookie->relation = catcore_lookup_rel(node->into);
			if (hash_cookie->relation == NULL)
				elog(ERROR, "could not find relation \"%s\" in %s at %s:%d",
							node->into, str, pcql->filename, pcql->lineno);

		}
		break;
	case T_CaQLDelete:
		{
			CaQLDelete	   *node = (CaQLDelete *) query;

			hash_cookie->bDelete = true;

			hash_cookie->relation = catcore_lookup_rel(node->from);
			if (hash_cookie->relation == NULL)
				elog(ERROR, "could not find relation \"%s\" in %s at %s:%d",
							node->from, str, pcql->filename, pcql->lineno);

			hash_cookie->bAllEqual =
				caql_process_predicates(hash_cookie, node->where);
		}
		break;
	default:
		return NULL;
	}

	hash_cookie->name = str;
	hash_cookie->query = query;
	hash_cookie->file = (char *) pcql->filename;
	hash_cookie->lineno = pcql->lineno;
	/* Find an available index based on predicates or ORDER BY */
	hash_cookie->index = caql_find_index(hash_cookie, query);
	if (hash_cookie->index != NULL)
		hash_cookie->syscacheid = GetSysCacheId(hash_cookie->index->indexoid);
	else
		hash_cookie->syscacheid = -1;

	caql_put_parser_cache(str, len, hash_cookie);

	return hash_cookie;
}

/*
 * This makes decisions wheather we should use syscache, index scan, or
 * heap scan, based on the query and the settings.  Then it opens and
 * initializes relation and scan.
 *
 * Note: it frees pcql.
 */
cqContext *
caql_switch(struct caql_hash_cookie *pchn,
			cqContext *pCtx,
			cq_list *pcql)
{
	int			i;

	Assert(pCtx);

	/* set the snapshot and lockmodes */
	if (!pCtx->cq_setsnapshot)
		pCtx->cq_snapshot = SnapshotNow;

	if (pchn->bDelete || pchn->bUpdate || pchn->bInsert)
		pCtx->cq_lockmode = RowExclusiveLock;
	else
		pCtx->cq_lockmode = AccessShareLock;

	/* get everything we need from cql */
	for (i = 0; i < pcql->maxkeys; i++)
	{
		pCtx->cq_datumKeys[i] = pcql->cqlKeys[i];
	}
	pCtx->cq_NumKeys = pcql->maxkeys;
	pCtx->cq_cacheKeys = &pCtx->cq_datumKeys[0];

	pCtx->relation = pchn->relation;
	pCtx->index = pchn->index;

#ifdef CAQL_LOGQUERY
	caql_logquery("caql_basic_fn_000", pcql->filename, pcql->lineno,
				  pCtx->cq_uniqquery_code, DatumGetObjectId(pcql->cqlKeys[0]));
#endif   /* CAQL_LOGQUERY */

	pCtx->cq_sysScan = caql_basic_fn_all(pchn, pCtx, pcql, false);

	/* NOTE: free up the cql before we return */
	pcql->bGood = false;
	pfree(pcql);

	return pCtx;
}

/*
 * Workhorse for caql_switch.
 */
static SysScanDesc
caql_basic_fn_all(caql_hash_cookie *pchn, cqContext *pCtx,
				  cq_list *pcql, bool bLockEntireTable)
{
	const CatCoreIndex   *index = pchn->index;
	SysScanDesc		desc;
	Oid				scanIndexId = InvalidOid;		/* index id */
	int				i, numScanKeys;
	List		   *predicates;
	ListCell	   *l;
	bool			can_use_idxOK;

	/* the caller states no index scan? */
	can_use_idxOK = !(pCtx->cq_setidxOK && !pCtx->cq_useidxOK);

	pCtx->cq_relationId = pchn->relation->relid;

	predicates = caql_query_predicate(pchn->query);
	numScanKeys = list_length(predicates);
	Assert(numScanKeys <= MAX_SCAN_NUM);

	/*
	 * Use the syscache if available and unless the caller states otherwise.
	 */
	if (index && pchn->syscacheid >= 0)
	{
		int		nkeys = index->nkeys;

		pCtx->cq_usesyscache = true;

		/*
		 * Must match all columns of the index to use syscache.
		 */
		if (pCtx->cq_NumKeys != nkeys)
			pCtx->cq_usesyscache = false;

		/*
		 * If all predicates are not equals, syscache is not used.
		 */
		if (!pchn->bAllEqual)
			pCtx->cq_usesyscache = false;

		/* ARD-28, MPP-16119: can only use SnapshowNow with syscache */
		if (pCtx->cq_snapshot != SnapshotNow)
		{
			/* Complain in debug, but behave in production */
			Assert(!pCtx->cq_usesyscache);

			pCtx->cq_usesyscache = true;
		}

		if (pCtx->cq_usesyscache)
		{
			pCtx->cq_cacheId = pchn->syscacheid;

			/* number of keys must match */
			Assert(pCtx->cq_NumKeys == nkeys);
		}
	}

	/*
	 * Use index scan if available and unless the caller states otherwise.
	 */
	if (index && can_use_idxOK)
	{
		scanIndexId = index->indexoid;
		pCtx->cq_useidxOK = true;
	}

	/*
	 * If we don't have index id, don't use index scan.
	 */
	if (!OidIsValid(scanIndexId))
	{
		/*
		 * Complain in debug, but behave in production.
		 * (possibly the caller made wrong decision)
		 */
		Assert(!pCtx->cq_useidxOK);
		pCtx->cq_useidxOK = false;
	}

	/*
	 * If relation is not supplied by the caller, open it here.
	 */
	if (!pCtx->cq_externrel)
	{
		if (pCtx->cq_usesyscache &&
			(AccessShareLock == pCtx->cq_lockmode))
		{
			pCtx->cq_externrel = true; /* pretend we have external relation */
			pCtx->cq_heap_rel = InvalidRelation;

			return NULL;
		}
		else
		{
			pCtx->cq_heap_rel = heap_open(pCtx->cq_relationId,
										  pCtx->cq_lockmode);
			pCtx->cq_tupdesc = RelationGetDescr(pCtx->cq_heap_rel);
		}
	}
	else
	{
		/* make sure the supplied relation matches the caql */
		if (RelationIsValid(pCtx->cq_heap_rel))
		{
			Assert(pchn->relation->relid == RelationGetRelid(pCtx->cq_heap_rel));
			pCtx->cq_tupdesc = RelationGetDescr(pCtx->cq_heap_rel);
		}
	}

	/*
	 * If we're using syscache, we don't need to begin scan.
	 */
	if (pCtx->cq_usesyscache)
	{
		Assert(0 <= pCtx->cq_cacheId);
		return NULL;
	}

	/*
	 * If it's INSERT, we don't need to begin scan.
	 */
	if (IsA(pchn->query, CaQLInsert))
		return NULL;

	/*
	 * Otherwise, begin scan.  Initialize scan keys with given parameters.
	 */
	foreach_with_count (l, predicates, i)
	{
		CaQLExpr	   *expr = (CaQLExpr *) lfirst(l);
		int				paramno;

		paramno = expr->right;
		ScanKeyInit(&pCtx->cq_scanKeys[i],
					expr->attnum, expr->strategy, expr->fnoid,
					pCtx->cq_datumKeys[paramno - 1]);
	}
	desc = systable_beginscan(pCtx->cq_heap_rel,
							  scanIndexId,
							  pCtx->cq_useidxOK,
							  pCtx->cq_snapshot, numScanKeys, pCtx->cq_scanKeys);

	return desc;
}

/*
 * Returns where clause if it's SELECT or DELETE
 */
static List *
caql_query_predicate(Node *query)
{
	switch(nodeTag(query))
	{
	case T_CaQLSelect:
		return ((CaQLSelect *) query)->where;
	case T_CaQLInsert:
		return NIL;
	case T_CaQLDelete:
		return ((CaQLDelete *) query)->where;
	default:
		elog(ERROR, "unexpected node type(%d)", nodeTag(query));
	}

	return NIL;
}

/*
 * Preprocesses list of CaQLExpr and finds attribute number and operator
 * information based on the attribute type.  Returns true if all predicate
 * operators are equality comparisons.
 */
static bool
caql_process_predicates(struct caql_hash_cookie *pchn, List *predicates)
{
	ListCell	   *l;
	bool			all_equal = true;

	foreach (l, predicates)
	{
		CaQLExpr	   *expr = (CaQLExpr *) lfirst(l);

		all_equal &= caql_lookup_predicate(pchn, expr,
										   &expr->attnum,
										   &expr->strategy,
										   &expr->fnoid,
										   &expr->typid);
	}

	return all_equal;
}

/*
 * Extracts attribute number, strategy number and function oid
 * from a predicate expression.  Returns true if the expression
 * is an equality comparison.
 */
static bool
caql_lookup_predicate(struct caql_hash_cookie *pchn, CaQLExpr *expr,
					  AttrNumber *attnum, StrategyNumber *strategy, Oid *fnoid,
					  Oid *typid)
{
	const CatCoreRelation  *relation = pchn->relation;
	const CatCoreAttr	   *attr;
	const CatCoreType	   *typ;
	char				   *attname = expr->left;

	if (expr->right < 1 || expr->right > 5)
		elog(ERROR, "invalid parameter number(%d) in %s",
					expr->right, pchn->name);

	attr = catcore_lookup_attr(relation, attname);
	if (!attr)
		elog(ERROR, "could not find attribute \"%s\" in %s",
					attname, pchn->name);

	*attnum = attr->attnum;
	typ = attr->atttyp;
	*typid = typ->typid;
	if (strcmp(expr->op, "=") == 0)
	{
		*strategy = BTEqualStrategyNumber;
		*fnoid = typ->eqfunc;
	}
	else if (strcmp(expr->op, "<") == 0)
	{
		*strategy = BTLessStrategyNumber;
		*fnoid = typ->ltfunc;
	}
	else if (strcmp(expr->op, "<=") == 0)
	{
		*strategy = BTLessEqualStrategyNumber;
		*fnoid = typ->lefunc;
	}
	else if (strcmp(expr->op, ">=") == 0)
	{
		*strategy = BTGreaterEqualStrategyNumber;
		*fnoid = typ->gefunc;
	}
	else if (strcmp(expr->op, ">") == 0)
	{
		*strategy = BTGreaterStrategyNumber;
		*fnoid = typ->gtfunc;
	}
	else
		elog(ERROR, "could not find operator \"%s\" in %s",
					expr->op, pchn->name);

	if (*fnoid == InvalidOid)
		elog(ERROR, "could not find btree operator for type \"%d\" in %s",
					typ->typid, pchn->name);

	return *strategy == BTEqualStrategyNumber;
}

/*
 * Finds a usable index based on WHERE and ORDER BY clause.
 */
static const CatCoreIndex *
caql_find_index(caql_hash_cookie *pchn, Node *query)
{
	const CatCoreIndex	   *index = NULL;
	const CatCoreRelation  *relation = pchn->relation;
	List				   *predicates;

	if (relation->nindexes == 0)
		return NULL;

	if (!(IsA(query, CaQLSelect) || IsA(query, CaQLDelete)))
		return NULL;

	predicates = caql_query_predicate(query);
	index = caql_find_index_by(pchn, predicates, false);

	if (index)
		return index;

	if (IsA(query, CaQLSelect))
		return caql_find_index_by(pchn, ((CaQLSelect *) query)->orderby, true);

	return NULL;
}

/*
 * Finds a usable index based on the clause.  If is_orderby is true,
 * it processes ORDER BY, otherwise WHERE.
 */
static const CatCoreIndex *
caql_find_index_by(caql_hash_cookie *pchn, List *keylist, bool is_orderby)
{
	const CatCoreRelation *relation = pchn->relation;
	const CatCoreIndex *indexes;
	int					i;

	if (list_length(keylist) == 0 || list_length(keylist) > MAX_SCAN_NUM)
		return NULL;

	indexes = relation->indexes;
	for (i = 0; i < relation->nindexes; i++)
	{
		ListCell   *l;
		const CatCoreIndex *index;
		int			j;
		bool		match = true;

		index = &indexes[i];
		foreach_with_count (l, keylist, j)
		{
			if (is_orderby)
			{
				char				   *sortcol = (char *) strVal(lfirst(l));
				const CatCoreAttr	   *attr;

				/*
				 * This is the only place to look up ORDER BY at the moment,
				 * but if we do it elsewhere we'd probably better speed things
				 * up by preprocess.
				 */
				attr = catcore_lookup_attr(relation, sortcol);
				if (!attr)
					elog(ERROR, "could not find attribute \"%s\" in %s",
								sortcol, pchn->name);

				if (attr->attnum != index->attnums[j])
				{
					match = false;
					break;
				}
			}
			else
			{
				CaQLExpr	   *expr = (CaQLExpr *) lfirst(l);

				/*
				 * Index is usable only if attributes match in the same order.
				 * Because we only support btree operators, we are sure
				 * this is enough.
				 */
				if (expr->attnum != index->attnums[j])
				{
					match = false;
					break;
				}
			}
		}

		/* found */
		if (match)
			return index;
	}

	return NULL;
}

/*
 * Finds the pre-parsed cookie by the query string.
 */
static caql_hash_cookie *
caql_get_parser_cache(const char *str, unsigned int len)
{
	uint32		hval;
	int			bucket_num, i;
	caql_parser_cache *bucket;

	/*
	 * Identify the bucket.
	 */
	hval = (uint32) hash_any((const unsigned char *) str, len);
	bucket_num = hval % CAQL_PARSER_CACHE_BUCKET;
	bucket = &parser_cache[bucket_num * CAQL_PARSER_CACHE_PER_BUCKET];

	for (i = 0; i < CAQL_PARSER_CACHE_PER_BUCKET; i++)
	{
		caql_parser_cache	   *entry = &bucket[i];

		/*
		 * Check the hash value first to speed up the comparison.
		 */
		if (entry->hval == hval && entry->context &&
			entry->cookie.name && strcmp(entry->cookie.name, str) == 0)
			return &entry->cookie;
	}

	return NULL;
}

/*
 * Stores the parser cache entry to persist in cache memory context.
 * relation and index are not copied because they are in the static memory.
 * We create a small memory context for this cache, so that we can free the
 * copied parse tree easily.
 * We carefully set cache->context at the end, to deal with the out-of-memory
 * situation during the copy.  Otherwise, the cache entry will be used wrongly
 * as it is half-backed.
 */
static void
copy_caql_parser_cache(caql_parser_cache *cache, int hval,
					   caql_hash_cookie *cookie)
{
	MemoryContext		mycontext, oldcontext;

	cache->hval = hval;
	memcpy(&cache->cookie, cookie, sizeof(caql_hash_cookie));
	mycontext = AllocSetContextCreate(CacheMemoryContext,
									  cache->cookie.name,
									  ALLOCSET_SMALL_MINSIZE,
									  ALLOCSET_SMALL_INITSIZE,
									  ALLOCSET_SMALL_MAXSIZE);

	/*
	 * We need to copy each pointer.
	 */
	oldcontext = MemoryContextSwitchTo(mycontext);
	cache->cookie.name = pstrdup(cache->cookie.name);
	cache->cookie.query = copyObject(cache->cookie.query);
	if (cache->cookie.file)
		cache->cookie.file = pstrdup(cache->cookie.file);
	MemoryContextSwitchTo(oldcontext);
	/* This tells this entry is ready */
	cache->context = mycontext;
}

/*
 * Frees the parser cache entry.  Because everything is stored in the private
 * context, it is only to delete that.  Make sure all the pointer entries are
 * set NULL.
 */
static void
free_caql_parser_cache(caql_parser_cache *cache)
{
	MemoryContext		mycontext;

	mycontext = cache->context;
	/* This tells that this entry is not valid anymore */
	cache->context = NULL;
	if (mycontext)
		MemoryContextDelete(mycontext);
	cache->cookie.name = NULL;
	cache->cookie.query = NULL;
	cache->cookie.file = NULL;
}

/*
 * Save the cookie in the cache.  The cache strategy is to split an array
 * into several buckets and to balance inputs by hash value modulo.  Once
 * a bucket becomes full, we remove the first entry and slide others by one.
 * This design assumes each bucket is small enough to move things around
 * very quickly.  By allocating enough buckets, we don't expect so many
 * cache misses in normal use cases.
 */
static void
caql_put_parser_cache(const char *str, unsigned int len,
					  caql_hash_cookie *cookie)
{
	uint32		hval;
	int			bucket_num, i;
	caql_parser_cache *bucket;

	/*
	 * Identify the bucket.
	 */
	hval = (uint32) hash_any((const unsigned char *) str, len);
	bucket_num = hval % CAQL_PARSER_CACHE_BUCKET;
	bucket = &parser_cache[bucket_num * CAQL_PARSER_CACHE_PER_BUCKET];

	for (i = 0; i < CAQL_PARSER_CACHE_PER_BUCKET; i++)
	{
		caql_parser_cache	   *entry = &bucket[i];

		/*
		 * If the entry is empty, context is NULL.
		 */
		if (entry->context == NULL)
			copy_caql_parser_cache(entry, hval, cookie);
		/*
		 * Return immediately if we already have it in our cache.
		 * If the context is not ready, it is an invalid cache.
		 */
		else if (entry->hval == hval && entry->context &&
				 strcmp(entry->cookie.name, str) == 0)
			return;
	}

	/*
	 * If the bucket is full, remove the first and append the new.
	 */
	free_caql_parser_cache(bucket);
	memmove(bucket, &bucket[1], sizeof(caql_parser_cache) *
									(CAQL_PARSER_CACHE_PER_BUCKET - 1));
	copy_caql_parser_cache(&bucket[CAQL_PARSER_CACHE_PER_BUCKET - 1],
						   hval, cookie);
}
