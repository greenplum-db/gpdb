/*-------------------------------------------------------------------------
 *
 * catquery.c
 *	  general catalog table access methods (internal api)
 *
 * Copyright (c) 2011,2012,2013 Greenplum inc
 *
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <string.h>
#include <unistd.h>

#include "catalog/caqlparse.h"
#include "catalog/catalog.h"
#include "catalog/catquery.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_class.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_description.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_filespace_entry.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_listener.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_shdescription.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_window.h"
#include "catalog/pg_tidycat.h"

#include "catalog/gp_configuration.h"
#include "catalog/gp_segment_config.h"
#include "catalog/gp_san_config.h"

#include "catalog/gp_fastsequence.h"

#include "catalog/gp_persistent.h"
#include "catalog/gp_global_sequence.h"
#include "catalog/gp_id.h"
#include "catalog/gp_version.h"
#include "catalog/gp_policy.h"

#include "miscadmin.h"
#include "utils/fmgroids.h"
#include "utils/relcache.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/inval.h"

#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbvars.h"


static void caql_UpdateIndexes(cqContext	*pCtx, 
							   Relation		 rel, 
							   HeapTuple	 tup);

#define caql_getattr_internal(pCtx, tup, attnum, isnull) \
(((pCtx)->cq_usesyscache) ? \
(SysCacheGetAttr((pCtx)->cq_cacheId, (tup), (attnum), (isnull))) : \
(heap_getattr((tup), (attnum), (pCtx)->cq_tupdesc, (isnull))))

static void
caql_heapclose(cqContext *pCtx)
{
	if (!pCtx->cq_externrel)
	{
		heap_close(pCtx->cq_heap_rel, pCtx->cq_lockmode);
		pCtx->cq_heap_rel = InvalidRelation;
	}
}

/* ----------------------------------------------------------------
 * cqclr
 * 
 * ----------------------------------------------------------------
 */

cqContext	*cqclr(cqContext	 *pCtx)
{
	cqClearContext(pCtx);
	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_addrel()
 * 
 * Add an existing relation as the heap_rel, and use its lockmode, 
 * and skip heap_open/heap_close
 * ----------------------------------------------------------------
 */

cqContext	*caql_addrel(cqContext *pCtx, Relation rel)
{
	if (RelationIsValid(rel))
	{
		Assert(!RelationIsValid(pCtx->cq_heap_rel));
		pCtx->cq_heap_rel  = rel;
		pCtx->cq_externrel = true;
	}
	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_indexOK()
 * 
 * if false, force a heapscan
 * ----------------------------------------------------------------
 */

cqContext	*caql_indexOK(cqContext *pCtx, bool bindexOK)
{
	pCtx->cq_setidxOK  = true;
	pCtx->cq_useidxOK  = bindexOK;

	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_lockmode()
 * 
 * Change the default lockmode (ie RowExclusiveLock for Delete/Update, 
 * else AccessShareLock) associated with the underlying relation.   
 * Has no effect if the relation is external.
 * ----------------------------------------------------------------
 */

cqContext	*caql_lockmode(cqContext *pCtx, LOCKMODE lm)
{
	pCtx->cq_setlockmode = true;
	pCtx->cq_lockmode	 = lm;
	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_snapshot()
 * 
 * Change the default snapshot (SnapshotNow) associated with the 
 * heap/index scan.
 * ----------------------------------------------------------------
 */

cqContext	*caql_snapshot(cqContext *pCtx, Snapshot ss)
{
	pCtx->cq_setsnapshot = true;
	pCtx->cq_snapshot	 = ss;
	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_syscache()
 * 
 * Override the default choice of syscache vs heap/index scan.
 * Note: will Assert if asked to choose cache for non-cached index.
 * ----------------------------------------------------------------
 */

cqContext	*caql_syscache(cqContext *pCtx, bool bUseCache)
{
	pCtx->cq_setsyscache = true;
	pCtx->cq_usesyscache = bUseCache;
	return (pCtx);
}

/* ----------------------------------------------------------------
 * cql1()
 * The underlying function for the cql() macro.
 * find the bind parameters in a caql string and build a key list
 * ----------------------------------------------------------------
 */
cq_list *cql1(const char* caqlStr, const char* filename, int lineno, ...)
{
	int			 maxkeys = 0;
	int			 badbind = 0;
	cq_list		*pcql	 = (cq_list *) palloc0(sizeof(cq_list));
	const char*	 pc		 = caqlStr;

	if ((NULL == caqlStr) ||
		(0 == strlen(caqlStr)))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg(
					 "Invalid or undefined CaQL string"
				 )));

	pcql->bGood	   = true;
	pcql->caqlStr  = caqlStr;
	pcql->filename = filename;
	pcql->lineno   = lineno;

	while (*pc && (maxkeys < 5))
	{
		if (*pc != ':')
		{
			pc++;
			continue;
		}

		/* look for numeric bind parameter of the form ":1" to ":5" */
		pc++;

		if (!*pc)
			break;

		switch (*pc)
		{
			case '1':
				if (maxkeys != 0)
				{
					badbind = 1;
					goto L_wrong_args;
				}
				maxkeys++;
				break;
			case '2':
				if (maxkeys != 1)
				{
					badbind = 2;
					goto L_wrong_args;
				}
				maxkeys++;
				break;
			case '3':
				if (maxkeys != 2)
				{
					badbind = 3;
					goto L_wrong_args;
				}
				maxkeys++;
				break;
			case '4':
				if (maxkeys != 3)
				{
					badbind = 4;
					goto L_wrong_args;
				}
				maxkeys++;
				break;
			case '5':
				if (maxkeys != 4)
				{
					badbind = 5;
					goto L_wrong_args;
				}
				maxkeys++;
				break;
			case '6':
			case '7':
			case '8':
			case '9':
			case '0':
			{
				badbind = 6;
				goto L_wrong_args;
			}
			default:
				break;
		} /* end switch */
	} /* end while */

	pcql->maxkeys   = maxkeys;
	
	if (maxkeys)
	{
		va_list		ap;

		va_start(ap, lineno);

		for (int ii = 0; ii < maxkeys; ii++)
		{
			pcql->cqlKeys[ii] = va_arg(ap, Datum);
		}
		va_end(ap);
	}
	return (pcql);

  L_wrong_args:
	if (badbind != 5)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg(
					 "bind parameter out of range (1-5)"
				 )));
	else
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg(
					 "missing bind parameter before :%d, or out of sequence", 
					 badbind
				 )));

	return (NULL);
} /* end cql1 */


/* ----------------------------------------------------------------
 * caql_getcount()
 * Perform COUNT(*) or DELETE
 * ----------------------------------------------------------------
 */
int caql_getcount(cqContext *pCtx0, cq_list *pcql)
{
	const char*				 caql_str = pcql->caqlStr;
	const char*				 filenam  = pcql->filename;
	int						 lineno	  = pcql->lineno;
	struct caql_hash_cookie	*pchn	  = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext				*pCtx;
	cqContext				 cqc;
	HeapTuple				 tuple;
	Relation				 rel;
	int						 ii		  = 0;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */
	rel  = pCtx->cq_heap_rel;

	/* use the SysCache */
	if (pCtx->cq_usesyscache)
	{
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);

		if (HeapTupleIsValid(tuple))
		{
			ii++;
			pCtx->cq_lasttup = tuple;
			if (pchn->bDelete)
				simple_heap_delete(rel, &tuple->t_self);

			ReleaseSysCache(tuple);
			/* only one */
		}
		caql_heapclose(pCtx);
		return (ii);
	}

	while (HeapTupleIsValid(tuple = systable_getnext(pCtx->cq_sysScan)))
	{
		if (HeapTupleIsValid(tuple) && pchn->bDelete)
		{
			pCtx->cq_lasttup = tuple;
			if (pchn->bDelete)
				simple_heap_delete(rel, &tuple->t_self);
		}

		ii++;
	}
	systable_endscan(pCtx->cq_sysScan); 
	caql_heapclose(pCtx);

	return (ii);
}

/* ----------------------------------------------------------------
 * caql_getfirst_only()
 * Return a copy the first tuple, pallocd in the current memory context,
 * and end the scan.  Clients should heap_freetuple() as necessary.
 * If pbOnly is not NULL, return TRUE if a second tuple is not found,
 * else return FALSE
 * NOTE: this function will return NULL if no tuples satisfy the
 * caql predicate -- use HeapTupleIsValid() to detect this condition.
 * ----------------------------------------------------------------
 */
HeapTuple caql_getfirst_only(cqContext *pCtx0, bool *pbOnly, cq_list *pcql)
{
	const char*				 caql_str = pcql->caqlStr;
	const char*				 filenam  = pcql->filename;
	int						 lineno	  = pcql->lineno;
	struct caql_hash_cookie	*pchn	  = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext				*pCtx;
	cqContext				 cqc;
	HeapTuple				 tuple, newTup = NULL;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	if (pbOnly) *pbOnly = true;

	/* use the SysCache */
	if (pCtx->cq_usesyscache)
	{
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);

		if (HeapTupleIsValid(tuple))
		{
			newTup = heap_copytuple(tuple);
			ReleaseSysCache(tuple);
			/* only one */
		}
		caql_heapclose(pCtx);

		pCtx->cq_lasttup = newTup; /* need this for update/delete */

		return (newTup);
	}

	if (HeapTupleIsValid(tuple = systable_getnext(pCtx->cq_sysScan)))
	{
		/* always copy the tuple, because the endscan releases tup memory */
		newTup = heap_copytuple(tuple);
 
		if (pbOnly)
		{
			*pbOnly = 
				!(HeapTupleIsValid(systable_getnext(pCtx->cq_sysScan)));
		}
	}
	systable_endscan(pCtx->cq_sysScan); 
	caql_heapclose(pCtx);

	pCtx->cq_lasttup = newTup; /* need this for update/delete */
	return (newTup);
}

/* ----------------------------------------------------------------
 * caql_begin_CacheList()
 * Return a catclist
 * 
 * In general, catquery will choose the syscache when the cql
 * statement contains an equality predicate on *all* of the syscache
 * primary key index columns, eg: 
 *
 *   cql("SELECT * FROM pg_amop WHERE amopopr = :1 and amopfamily = :2 ")
 *
 * will use the AMOPOPID syscache with index
 * AccessMethodOperatorIndexId.  However, the cql statement for a
 * list-search requires an equality predicate on a subset of the
 * initial columns of the index, with *all* of the index columns
 * specified in an ORDER BY clause, eg:
 *
 *   cql("SELECT * FROM pg_amop WHERE amopopr = :1 "
 *       " ORDER BY amopopr, amopfamily ")
 *
 * will use a syscache list-search if this cql statement is an
 * argument to caql_begin_CacheList().  However, the syscache will
 * *not* be used for this statement if it is supplied for
 * caql_beginscan(), since SearchSysCache() can only return (at most)
 * a single tuple.
 *
 * NOTE: caql_begin_CacheList() will assert (Insist!) at runtime if
 * the cql statement does not map to a syscache lookup.
 * NOTE: it may be possible to "collapse" this API into the existing
 * beginscan/getnext/endscan.
 * ----------------------------------------------------------------
 */
struct catclist *caql_begin_CacheList(cqContext *pCtx0, 
									  cq_list *pcql)
{
	const char*				 caql_str = pcql->caqlStr;
	const char*				 filenam  = pcql->filename;
	int						 lineno	  = pcql->lineno;
	struct caql_hash_cookie	*pchn	  = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext				*pCtx;
	cqContext				 cqc;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */
	Assert(!pchn->bUpdate); /* UPDATE not allowed */
	Assert(!pchn->bDelete); /* DELETE not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	/* NOTE: for case of cache list search, we must use syscache */
	pCtx->cq_bCacheList = true; 

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	/* NOTE: must use the SysCache */
	Insist (pCtx->cq_usesyscache);

	caql_heapclose(pCtx);

	return SearchSysCacheKeyArrayList(pCtx->cq_cacheId, 
									  pCtx->cq_NumKeys, 
									  pCtx->cq_cacheKeys);
}


/* ----------------------------------------------------------------
 * caql_beginscan()
 * Initialize the scan and open relations/acquire locks as necessary
 * ----------------------------------------------------------------
 */
cqContext *caql_beginscan(cqContext *pCtx0, cq_list *pcql)
{
	const char*				 caql_str = pcql->caqlStr;
	const char*				 filenam  = pcql->filename;
	int						 lineno	  = pcql->lineno;
	struct caql_hash_cookie	*pchn	  = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext				*pCtx;

	/* use the provided context, or *allocate* a clean one */
	if (pCtx0)
		pCtx = pCtx0;
	else
	{
		pCtx = (cqContext *) palloc0(sizeof(cqContext)); 
		pCtx->cq_free = true;  /* free this context in caql_endscan */
	}

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	if (pchn->bCount)
		elog(ERROR, 
			 "Cannot scan: %s -- COUNTing or DELETing\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	pCtx->cq_bScanBlock = true; /* started a scan block */
	pCtx->cq_freeScan	= true;

	if (pchn->bInsert) /* INSERT allowed, but no subsequent fetches */
	{
		pCtx->cq_freeScan = false; /* didn't allocate a scanner */
		pCtx->cq_EOF	  = true;
	}

	return (pCtx);
}

/* ----------------------------------------------------------------
 * caql_getnext()
 * Return a tuple.  The tuple is only valid until caql_endscan(),
 * or until the next call of caql_getnext().
 * NOTE: this function will return NULL when no tuples remain to 
 * satisfy the caql predicate -- use HeapTupleIsValid() to detect 
 * this condition.
 * ----------------------------------------------------------------
 */
HeapTuple caql_getnext(cqContext *pCtx)
{
	HeapTuple tuple;
	/* set EOF when get invalid tuple */

	if (pCtx->cq_EOF)
		return (NULL);

	if (!pCtx->cq_usesyscache)
	{
		tuple = systable_getnext(pCtx->cq_sysScan); 
		pCtx->cq_EOF = !(HeapTupleIsValid(tuple));
	}
	else
	{
		/* syscache is always 0 or 1 entry */
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);
		
		pCtx->cq_EOF	 = true;  /* at EOF always, because only 0 or 1 */
	}

	pCtx->cq_lasttup = tuple; /* need this for ReleaseSysCache */

	return (tuple);
}

/* ----------------------------------------------------------------
 * caql_getprev()
 * NOTE: similar to caql_getnext(), but backwards.  
 * Usage is rare and potentially dangerous.  
 * The scankey should be set to point to the *last* key, not the 
 * first, typically using COLNAME <= BINDVALUE, but this assumes
 * a unique index.  (with a non-unique index the beginscan could
 * potentially position at the *start* of a group of duplicates,
 * not the end)
 * ----------------------------------------------------------------
 */
HeapTuple caql_getprev(cqContext *pCtx)
{
	HeapTuple tuple;
	/* set EOF when get invalid tuple */

	if (pCtx->cq_EOF)
		return (NULL);

	if (!pCtx->cq_usesyscache)
	{
		tuple = systable_getprev(pCtx->cq_sysScan); 
		pCtx->cq_EOF = !(HeapTupleIsValid(tuple));
	}
	else
	{
		Insist(0); /* XXX XXX: illegal ? */
		/* syscache is always 0 or 1 entry */
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);
		
		pCtx->cq_EOF	 = true;  /* at EOF always, because only 0 or 1 */
	}

	pCtx->cq_lasttup = tuple; /* need this for ReleaseSysCache */

	return (tuple);
}

/* ----------------------------------------------------------------
 * caql_endscan()
 * free all resources associated with the scan, including tuples,
 * tables and locks.
 * NOTE: this function is *not* a "drop-in" replacement for
 * ReleaseSysCache.  ReleaseSysCache is only called for valid tuples,
 * but you must always call endscan, even if getnext never returned a
 * valid tuple.
 * ----------------------------------------------------------------
 */
void caql_endscan(cqContext *pCtx)
{
	if (pCtx->cq_indstate) /* close the indexes if they were open */
		CatalogCloseIndexes(pCtx->cq_indstate);
	pCtx->cq_indstate = NULL;

	pCtx->cq_bScanBlock = false; /* scan block has ended */

	if (pCtx->cq_freeScan)
	{
		if (!pCtx->cq_usesyscache)
			systable_endscan(pCtx->cq_sysScan); 
		else
		{
			/* XXX XXX: no need to release if never fetched a valid tuple */
			if (HeapTupleIsValid(pCtx->cq_lasttup))
				ReleaseSysCache(pCtx->cq_lasttup);
		}
	}
	caql_heapclose(pCtx);

	if (pCtx->cq_free) /* free dynamic context */
		pfree(pCtx);

	pCtx->cq_freeScan = false;
	pCtx->cq_free	  = false;
}

/* ----------------------------------------------------------------
 * caql_UpdateIndexes()

 * For caql_beginscan/caql_endscan block: 
 *   open the index once with CatalogOpenIndexes()
 *   do CatalogIndexInsert() for each tuple heap_insert or heap_update
 *   close the index with CatalogCloseIndexes() at caql_endscan()
 *
 * For all other cases (eg caql_getfirst) just do
 * CatalogUpdateIndexes(), 
 * ----------------------------------------------------------------
 */
static void caql_UpdateIndexes(cqContext	*pCtx, 
							   Relation		 rel, 
							   HeapTuple	 tup)
{
	if (RelationGetForm(rel)->relhasindex) /* check from ExecOpenIndices */
	{
		if (!pCtx->cq_bScanBlock) /* not in beginscan/endscan block */
			CatalogUpdateIndexes(rel, tup);
		else
		{
			/* open the index if necessary, then insert a tuple */
			if (!pCtx->cq_indstate)
				pCtx->cq_indstate = CatalogOpenIndexes(rel);
			CatalogIndexInsert(pCtx->cq_indstate, tup);

			/* index is closed on caql_endscan() */
		}
	}
}

/* ----------------------------------------------------------------
 * caql_delete_current()
 * during beginscan/endscan iteration, delete current tuple 
 * ----------------------------------------------------------------
 */
void caql_delete_current(cqContext *pCtx)
{
	Relation				 rel;

	rel  = pCtx->cq_heap_rel;
	Assert(RelationIsValid(rel));

	if (HeapTupleIsValid(pCtx->cq_lasttup))
		simple_heap_delete(rel, &(pCtx->cq_lasttup)->t_self);
}

/* ----------------------------------------------------------------
 * caql_insert()
 * during beginscan/endscan iteration, insert a tuple
 * NOTE: a separate call to CatalogUpdateIndexes after this will 
 * cause an error
 * ----------------------------------------------------------------
 */
Oid caql_insert(cqContext *pCtx, HeapTuple tup)
{
	Relation		 rel;
	Oid				 result;

	rel  = pCtx->cq_heap_rel;
	Assert(RelationIsValid(rel));

	result = simple_heap_insert(rel, tup);

	/* keep the catalog indexes up to date (if has any) */
	caql_UpdateIndexes(pCtx, rel, tup);

	return (result);
}

/* ----------------------------------------------------------------
 * caql_update_current()
 * during beginscan/endscan iteration, update current tuple,
 * and update catalog indexes if necessary 
 * NOTE: a separate call to CatalogUpdateIndexes after this will 
 * cause an error
 * ----------------------------------------------------------------
 */
void caql_update_current(cqContext *pCtx, HeapTuple tup)
{
	Relation				 rel;

	rel  = pCtx->cq_heap_rel;
	Assert(RelationIsValid(rel));

	Insist(HeapTupleIsValid(pCtx->cq_lasttup));

	simple_heap_update(rel, &(pCtx->cq_lasttup)->t_self, tup);

	/* keep the catalog indexes up to date (if has any) */
	caql_UpdateIndexes(pCtx, rel, tup);
}
/* ----------------------------------------------------------------
 * caql_modify_current()
 * during beginscan/endscan iteration, modify current tuple
 * ----------------------------------------------------------------
 */
HeapTuple caql_modify_current(cqContext *pCtx, Datum *replValues,
							  bool *replIsnull,
							  bool *doReplace)
{
	Relation				 rel;
	HeapTuple				 tuple = NULL;

	rel  = pCtx->cq_heap_rel;
	Assert(RelationIsValid(rel));

	Insist(HeapTupleIsValid(pCtx->cq_lasttup));

	{
		tuple = heap_modify_tuple(pCtx->cq_lasttup,
								  RelationGetDescr(rel), 
								  replValues,
								  replIsnull,
								  doReplace);
	}

	return (tuple);
}
/* ----------------------------------------------------------------
 * caql_form_tuple()
 * during beginscan/endscan iteration, form a tuple
 * ----------------------------------------------------------------
 */
HeapTuple caql_form_tuple(cqContext *pCtx, Datum *replValues,
						  bool *replIsnull)
{
	Relation				 rel;
	HeapTuple				 tuple = NULL;

	rel  = pCtx->cq_heap_rel;
	Assert(RelationIsValid(rel));

	{
		tuple = heap_form_tuple(RelationGetDescr(rel), 
								replValues,
								replIsnull);
	}

	return (tuple);
}

/* ----------------------------------------------------------------
 * caql_getattr()
 * during beginscan/endscan iteration, get a tuple attribute for
 * current tuple
 * ----------------------------------------------------------------
 */

Datum caql_getattr(cqContext *pCtx, AttrNumber attnum, bool *isnull)
{
	Assert(HeapTupleIsValid(pCtx->cq_lasttup));

	return caql_getattr_internal(pCtx, pCtx->cq_lasttup, attnum, isnull);

	/* 
	  NOTE: could this be used if caql is extended to support joins, eg
	  what would attnum be for 
	  "SELECT * FROM pg_resqueue, pg_resqueuecapability ..." ?
	  Potentially, the attnum is just the ordinal position of the combined
	  SELECT list, eg you could reference pg_resqueuecapability.restypid
	  as (Natts_pg_resqueue+Anum_pg_resourcetype_restypid).

	*/

}

/* ----------------------------------------------------------------
 * caql_getattname()
 *
 * The equivalent of SearchSysCacheCopyAttName - 
 * caql_getfirst(pCtx,	
 *    "SELECT * FROM pg_attribute
 *     WHERE attrelid = :relid
 *     AND attname = :attname
 *     AND attisdropped is false"
 *    );
 *
 * That is, find the existing ("undropped") attribute and return
 * a copy.
 * NOTE: need to be careful if this pCtx is used for update...
 * ----------------------------------------------------------------
 */
HeapTuple caql_getattname(cqContext *pCtx, Oid relid, const char *attname)
{
	HeapTuple tup;

	tup = SearchSysCacheCopyAttName(relid, attname);

	if (pCtx)
	{
		/* treat as ATTNAME cache lookup */
		pCtx->cq_usesyscache = true;
		pCtx->cq_cacheId      = ATTNAME;
		pCtx->cq_NumKeys = 2;

		/* NOTE: no valid relation for subsequent INSERT/UPDATE/DELETE
		   *unless* an external relation is supplied */
		if (!pCtx->cq_externrel)
		{
			pCtx->cq_externrel = true; /* pretend we have external relation */
			pCtx->cq_heap_rel  = InvalidRelation;
		}

		if (!pCtx->cq_setidxOK)
			pCtx->cq_useidxOK = true;

		pCtx->cq_lasttup = tup;
	}   

	return (tup);

}

/* ----------------------------------------------------------------
 * caql_getattname_scan()
 *
 * The equivalent of SearchSysCacheAttName - 
 * caql_beginscan(pCtx,	
 *    "SELECT * FROM pg_attribute
 *     WHERE attrelid = :relid
 *     AND attname = :attname
 *     AND attisdropped is false"
 *    );
 *
 * That is, find the existing ("undropped") attribute and return
 * a context where the tuple is already fetched.  Retrieve the tuple
 * using caql_get_current()
 * NOTE: this is hideous.  My abject apologies.
 * NOTE: need to be careful if this pCtx is used for update...
 * ----------------------------------------------------------------
 */
cqContext *
caql_getattname_scan(cqContext *pCtx0, Oid relid, const char *attname)
{
	cqContext				*pCtx;
	HeapTuple tup;

	/* use the provided context, or *allocate* a clean one */
	if (pCtx0)
		pCtx = pCtx0;
	else
	{
		pCtx = (cqContext *) palloc0(sizeof(cqContext)); 
		pCtx->cq_free = true;  /* free this context in caql_endscan */
	}

	tup = SearchSysCacheAttName(relid, attname);

	if (pCtx)
	{
		/* treat as ATTNAME cache lookup */
		pCtx->cq_usesyscache = true;
		pCtx->cq_cacheId      = ATTNAME;
		pCtx->cq_NumKeys = 2;

		/* NOTE: no valid relation for subsequent INSERT/UPDATE/DELETE
		   *unless* an external relation is supplied */
		if (!pCtx->cq_externrel)
		{
			pCtx->cq_externrel = true; /* pretend we have external relation */
			pCtx->cq_heap_rel  = InvalidRelation;
		}

		if (!pCtx->cq_setidxOK)
			pCtx->cq_useidxOK = true;

		pCtx->cq_freeScan = true;
		pCtx->cq_EOF  = true;

		pCtx->cq_lasttup = tup;
	}   

	return (pCtx);

}

/*
 * from lsyscach.c/get_attnum():
 *
 *		Given the relation id and the attribute name,
 *		return the "attnum" field from the attribute relation.
 *
 *		Returns InvalidAttrNumber if the attr doesn't exist (or is dropped).
 */
AttrNumber caql_getattnumber(Oid relid, const char *attname)
{
	HeapTuple	tp;

	tp = SearchSysCacheAttName(relid, attname);
	if (HeapTupleIsValid(tp))
	{
		Form_pg_attribute att_tup = (Form_pg_attribute) GETSTRUCT(tp);
		AttrNumber	result;

		result = att_tup->attnum;
		ReleaseSysCache(tp);
		return result;
	}
	else
		return InvalidAttrNumber;
}


/* XXX XXX: temp fix for gp_distro reference in getoid_plus */
typedef FormData_gp_policy *Form_gp_distribution_policy;

/* ----------------------------------------------------------------
 * caql_getoid_plus()
 * Return an oid column from the first tuple and end the scan.
 * Note: this works for regproc columns as well, but you should cast
 * the output as RegProcedure.
 * ----------------------------------------------------------------
 */
Oid caql_getoid_plus(cqContext *pCtx0, int *pFetchcount,
					 bool *pbIsNull, cq_list *pcql)
{
	const char *caql_str = pcql->caqlStr;
	const char *filenam = pcql->filename;
	int			lineno = pcql->lineno;
	struct caql_hash_cookie	*pchn = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext  *pCtx;
	cqContext	cqc;
	HeapTuple	tuple;
	Oid			result = InvalidOid;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	if (pFetchcount)
		*pFetchcount = 0;
	if (pbIsNull)
		*pbIsNull = true;

	/* use the SysCache */
	if (pCtx->cq_usesyscache)
	{
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);
	}
	else
	{
		tuple = systable_getnext(pCtx->cq_sysScan);
	}

	if (HeapTupleIsValid(tuple))
	{
		if (pFetchcount)
			*pFetchcount = 1;
		
		/* if attnum not set, (InvalidAttrNumber == 0)
		 * use tuple oid, else extract oid from column 
		 * (includes ObjectIdAttributeNumber == -2) 
		 */
		if (pchn->attnum <= InvalidAttrNumber) 
		{
			if (pbIsNull)
				*pbIsNull = false;
			result = HeapTupleGetOid(tuple);
		}
		else /* find oid column */
		{
			bool		isnull;
			Datum		d = caql_getattr_internal(pCtx, tuple, pchn->attnum,
												  &isnull);

			if (!isnull)
			{
				switch (pchn->atttype)
				{
					case OIDOID:
					case REGPROCOID:
						result = DatumGetObjectId(d);
						break;

					default:
						elog(ERROR, "column not an oid: %s\nfile: %s, line %d", 
							 caql_str, filenam, lineno);
				}
			}
			if (pbIsNull)
				*pbIsNull = isnull;
		}
	} /* end HeapTupleIsValid */

	if (pCtx->cq_usesyscache)
	{  
		if (HeapTupleIsValid(tuple))
			ReleaseSysCache(tuple);
	}
	else
	{	
		if (pFetchcount && HeapTupleIsValid(tuple))
		{				
			if (HeapTupleIsValid(systable_getnext(pCtx->cq_sysScan)))
			{
				*pFetchcount = 2;	
			}
		}
		systable_endscan(pCtx->cq_sysScan); 
	}		
	caql_heapclose(pCtx);

	return (result);
} /* end caql_getoid_plus */

/* ----------------------------------------------------------------
 * caql_getoid_only()
 * Return the oid of the first tuple and end the scan
 * If pbOnly is not NULL, return TRUE if a second tuple is not found,
 * else return FALSE
 * ----------------------------------------------------------------
 */
Oid caql_getoid_only(cqContext *pCtx0, bool *pbOnly, cq_list *pcql)
{
	const char *caql_str = pcql->caqlStr;
	const char *filenam = pcql->filename;
	int			lineno = pcql->lineno;
	struct caql_hash_cookie	*pchn = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext  *pCtx;
	cqContext	cqc;
	HeapTuple	tuple;
	Oid			result = InvalidOid;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	if (pbOnly)
		*pbOnly = true;

	/* use the SysCache */
	if (pCtx->cq_usesyscache)
	{
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);
		if (HeapTupleIsValid(tuple))
		{
			result = HeapTupleGetOid(tuple);
			ReleaseSysCache(tuple);
			/* only one */
		}
		caql_heapclose(pCtx);

		return (result);
	}

	if (HeapTupleIsValid(tuple = systable_getnext(pCtx->cq_sysScan)))
	{
		result = HeapTupleGetOid(tuple);

		if (pbOnly)
		{
			*pbOnly = 
				!(HeapTupleIsValid(tuple = 
								   systable_getnext(pCtx->cq_sysScan)));
		}
	}
	systable_endscan(pCtx->cq_sysScan); 
	caql_heapclose(pCtx);
	return (result);
}

/* ----------------------------------------------------------------
 * caql_getcstring_plus()
 * Return a cstring column from the first tuple and end the scan.
 * ----------------------------------------------------------------
 */
char *caql_getcstring_plus(cqContext *pCtx0, int *pFetchcount,
						   bool *pbIsNull, cq_list *pcql)
{
	const char *caql_str = pcql->caqlStr;
	const char *filenam = pcql->filename;
	int			lineno = pcql->lineno;
	struct caql_hash_cookie	*pchn = cq_lookup(caql_str, strlen(caql_str), pcql);
	cqContext  *pCtx;
	cqContext   cqc;
	HeapTuple   tuple;
	char	   *result = NULL;

	if (NULL == pchn)
		elog(ERROR, "invalid caql string: %s\nfile: %s, line %d", 
			 caql_str, filenam, lineno);

	Assert(!pchn->bInsert); /* INSERT not allowed */

	/* use the provided context, or provide a clean local ctx  */
	if (pCtx0)
		pCtx = pCtx0;
	else
		pCtx = cqclr(&cqc);

	pCtx = caql_switch(pchn, pCtx, pcql);
	/* NOTE: caql_switch frees the pcql */

	if (pFetchcount)
		*pFetchcount = 0;
	if (pbIsNull)
		*pbIsNull = true;

	/* use the SysCache */
	if (pCtx->cq_usesyscache)
	{
		tuple = SearchSysCacheKeyArray(pCtx->cq_cacheId, 
									   pCtx->cq_NumKeys, 
									   pCtx->cq_cacheKeys);
	}
	else
	{
		tuple = systable_getnext(pCtx->cq_sysScan);
	}

	if (HeapTupleIsValid(tuple))
	{
		bool		isnull;
		Datum		d;

		if (pFetchcount)
			*pFetchcount = 1;

		d = caql_getattr_internal(pCtx, tuple, pchn->attnum, &isnull);

		if (!isnull)
		{
			switch (pchn->atttype)
			{
				case NAMEOID:
					result = DatumGetCString(DirectFunctionCall1(nameout, d));
					break;

				case TEXTOID:
					result = DatumGetCString(DirectFunctionCall1(textout, d));
					break;

				default:
					elog(ERROR, "column not a cstring: %s\nfile: %s, line %d", 
						 caql_str, filenam, lineno);
			}
		}
		if (pbIsNull)
			*pbIsNull = isnull;
	} /* end HeapTupleIsValid */

	if (pCtx->cq_usesyscache)
	{  
		if (HeapTupleIsValid(tuple))
			ReleaseSysCache(tuple);
	}
	else
	{	
		if (pFetchcount && HeapTupleIsValid(tuple))
		{				
			if (HeapTupleIsValid(systable_getnext(pCtx->cq_sysScan)))
			{
				*pFetchcount = 2;	
			}
		}
		systable_endscan(pCtx->cq_sysScan); 
	}		
	caql_heapclose(pCtx);

	return (result);
} /* end caql_getcstring_plus */


void
caql_logquery(const char *funcname, const char *filename, int lineno,
			  int uniqquery_code, Oid arg1)
{
}

