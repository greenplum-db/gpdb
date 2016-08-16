/*-------------------------------------------------------------------------
 *
 * catquery.h
 *	  catalog query
 *
 *
 * Copyright (c) 2011, 2012 Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CATQUERY_H
#define CATQUERY_H

#include "access/genam.h"
#include "access/relscan.h"
#include "access/sdir.h"
#include "catalog/catcore.h"
#include "nodes/primnodes.h"
#include "storage/lock.h"
#include "utils/catcache.h"
#include "utils/relcache.h"

/*
 * TODO: probably "hash cookie" is meaningless now.  More like parser state.
 */
typedef struct caql_hash_cookie
{
	const char *name;		/* caql string */
	int			basequery_code; /* corresponding base query */
	int			bDelete;	/* query performs DELETE */
	int			bCount;		/* SELECT COUNT(*) (or DELETE) */
	int			bUpdate;	/* SELECT ... FOR UPDATE */
	int			bInsert;	/* INSERT INTO  */
	bool		bAllEqual;	/* true if all equality operators */
	AttrNumber	attnum;		/* column number (or 0 if no column specified) */
	Oid			atttype;		/* type OID of the specified column */
	const CatCoreRelation *relation; /* target relation */
	const CatCoreIndex    *index; /* available index */
	int			syscacheid;	/* syscache matching the index (-1 if none) */
	Node	   *query;		/* parsed syntax tree */

	/* debug info */
	char	   *file;		/* file location */
	int			lineno;		/* line number */
} caql_hash_cookie;

typedef struct cqListData
{
	bool		bGood;			/* if true, struct is allocated */
	const char* caqlStr;		/* caql string */
	int			numkeys;
	int			maxkeys;
	const char* filename;		/* FILE and LINE macros */
	int			lineno;
	Datum		cqlKeys[5];		/* key array */
} cq_list;

typedef struct cqContextData
{
	int			cq_basequery_code; /* corresponding base query */
	int			cq_uniqquery_code; /* unique query number */
	bool		cq_free;		/* if true, free this ctx at endscan */
	bool		cq_freeScan;	/* if true, free scan at endscan */
	Oid			cq_relationId;	/* relation id */
	Relation	cq_heap_rel;	/* catalog being scanned */
	TupleDesc	cq_tupdesc;		/* tuple descriptor */
	SysScanDesc cq_sysScan;		/* heap or index scan */		
	bool		cq_externrel;	/* heap rel external to caql */
	bool		cq_setsnapshot;	/* use cq_snapshot (else default) */
	Snapshot	cq_snapshot;	/* snapshot to see */
	LOCKMODE	cq_lockmode;	/* locking mode */
	bool		cq_EOF;			/* true if hit end of fetch */

	bool		cq_setidxOK;	/* set the indexOK mode */
	bool		cq_useidxOK;	/* use supplied indexOK mode) */

	bool		cq_pklock_excl;	/* lock exclusive if true */

	Datum		cq_datumKeys[5];/* key array of datums */
	int			cq_NumKeys;		/* number of keys */
	ScanKeyData	cq_scanKeys[5]; /* initialized sysscan key (from datums) */

	const CatCoreRelation *relation; /* relation being read */
	const CatCoreIndex *index;	/* usable index on the relation */

	/* 	index update information */
	CatalogIndexState	cq_indstate;	/* non-NULL after CatalogOpenIndexes */
	bool				cq_bScanBlock;	/* true if ctx in
										 * beginscan/endscan block */
		
	/* 	these attributes control syscache vs normal heap/index
	 * 	scan. If setsyscache is set, then usesyscache is the
	 * 	user-setting, else catquery chooses.  If usesyscache is set
	 * 	then sysScan is NULL, and cq_cacheId is the
	 * 	SysCacheIdentifier.
	 */
	bool		cq_setsyscache;	/* use syscache (else heap/index scan) */
	bool		cq_usesyscache;	/* use syscache (internal) */
	int			cq_cacheId; 	/* cache identifier */
	Datum	   *cq_cacheKeys;	/* array of keys */
	HeapTuple   cq_lasttup;		/* last tuple fetched (for ReleaseSysCache) */

} cqContext;

/* count (and optionally delete) */
int			 caql_getcount(cqContext *pCtx, cq_list *pcql);

/* return the first tuple 
   (and set a flag if the scan finds a second match) 
*/
HeapTuple	 caql_getfirst_only(cqContext *pCtx, 
								bool *pbOnly,
								cq_list *pcql);
#define		 caql_getfirst(pCtx, pcql) caql_getfirst_only(pCtx, NULL, pcql)

/* return the specified oid column of the first tuple 
   (and set a flag if the scan finds a second match) 
*/
Oid			 caql_getoid_only(cqContext *pCtx, 
							  bool *pbOnly,
							  cq_list *pcql);
Oid			 caql_getoid_plus(cqContext *pCtx0, int *pFetchcount,
							  bool *pbIsNull, cq_list *pcql);
#define		 caql_getoid(pCtx, pcql) caql_getoid_plus(pCtx, NULL, NULL, pcql)

cqContext	*caql_beginscan(cqContext *pCtx, cq_list *pcql);
HeapTuple	 caql_getnext(cqContext *pCtx);
/* XXX XXX: endscan must specify if hold or release locks */
void		 caql_endscan(cqContext *pCtx);

/* during beginscan/endscan iteration, 
 * or subsequent to a getfirst (where a context was supplied), 
 * delete, update or modify the current tuple 
*/
/* delete current tuple */
void		 caql_delete_current(cqContext *pCtx);
/* insert tuple */
Oid			 caql_insert(cqContext *pCtx, HeapTuple tup);
/* update current tuple */
void		 caql_update_current(cqContext *pCtx, HeapTuple tup);
/* modify current tuple */
HeapTuple	 caql_modify_current(cqContext *pCtx, Datum *replValues,
								 bool *replIsnull,
								 bool *doReplace);
/* form tuple */
HeapTuple	 caql_form_tuple(cqContext *pCtx, Datum *replValues,
							 bool *replIsnull);

/* retrieve the last (current) tuple */
#define caql_get_current(pCtx) ((pCtx)->cq_lasttup)

/* 
   NOTE: don't confuse caql_getattr and caql_getattname.  

   caql_getattr extracts the specified column (by attnum) from the
   current tuple in the pcqCtx.

   caql_getattname fetches a copy of tuple from pg_attribute that
   _describes_ a column of a table. (should really be get_by_attname)

 */

/* equivalent of SysCacheGetAttr - extract a specific attribute for
 *  current tuple 
 */
Datum caql_getattr(cqContext *pCtx, AttrNumber attnum, bool *isnull);

/* equivalent of SearchSysCacheCopyAttName - return copy of the tuple
 * describing a column of a table  
 */
HeapTuple caql_getattname(cqContext *pCtx, Oid relid, const char *attname);

/* same as SearchSysCacheAttName - return a scan where the tuple is
 * *already* fetched, so use caql_get_current (not getnext!) to get
 * the tuple, and endscan to free the context
 */
cqContext *caql_getattname_scan(cqContext *pCtx, 
								Oid relid, const char *attname);

/*
 * adapted from original lsyscache.c/get_attnum()
 *
 *		Given the relation id and the attribute name,
 *		return the "attnum" field from the attribute relation.
 *
 *		Returns InvalidAttrNumber if the attr doesn't exist (or is dropped).
 */
AttrNumber caql_getattnumber(Oid relid, const char *attname);

/* return the specified Name or Text column of the first tuple 
   (and set the fetchcount or isnull if specified)
*/
char		*caql_getcstring_plus(cqContext *pCtx0, int *pFetchcount,
								  bool *pbIsNull, cq_list *pcql);
#define		 caql_getcstring(pCtx, pcql) \
		caql_getcstring_plus(pCtx, NULL, NULL, pcql)

cq_list *cql1(const char* caqlStr, const char* filename, int lineno, ...);

/* caql context modification functions 
 *
 * cqclr and caql_addrel() are ok, but the others are generally used
 * to support legacy code requirements, and may be deprecated in
 * future releases
 */
cqContext	*caql_addrel(cqContext *pCtx, Relation rel);		/*  */
cqContext	*caql_indexOK(cqContext *pCtx, bool bindexOK);		/*  */
cqContext	*caql_snapshot(cqContext *pCtx, Snapshot ss);		/*  */
cqContext	*caql_syscache(cqContext *pCtx, bool bUseCache);	/*  */

cqContext	*cqclr(cqContext	 *pCtx);						/*  */
#define	cqClearContext(pcqContext) MemSet(pcqContext, 0, sizeof(cqContext))

void caql_logquery(const char *funcname, const char *filename, int lineno,
			  int uniqquery_code, Oid arg1);

/* ifdef gnuc ! */
#define cql(x, ...) cql1(x, __FILE__, __LINE__, __VA_ARGS__)

/* MPP-18975: wrapper to assuage type checker (only CString currently). 
   See calico.pl/check_datum_type() for details.
 */
#define cqlIsDatumForCString(d) (d)

/* caqlanalyze.c */
struct caql_hash_cookie * cq_lookup(const char *str, unsigned int len, cq_list *pcql);
cqContext *caql_switch(struct caql_hash_cookie *pchn, cqContext *pCtx, cq_list *pcql);


#endif   /* CATQUERY_H */
