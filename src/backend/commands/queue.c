/*-------------------------------------------------------------------------
 *
 * queue.c
 *	  Commands for manipulating resource queues.
 *
 * Portions Copyright (c) 2006-2010, Greenplum inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *    src/backend/commands/queue.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_resourcetype.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_resqueuecapability.h"
#include "nodes/makefuncs.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/queue.h"
#include "libpq/crypt.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/formatting.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/resource_manager.h"
#include "executor/execdesc.h"
#include "utils/resscheduler.h"
#include "utils/syscache.h"
#include "cdb/memquota.h"
#include "utils/guc_tables.h"

#define INVALID_RES_LIMIT_STRING		"-1"

/**
 * Establish a lower bound on what memory limit may be set on a queue.
 */
#define MIN_RESOURCEQUEUE_MEMORY_LIMIT_KB (10 * 1024L)

/* MPP-6923: 
 * GetResourceTypeByName: find the named resource in pg_resourcetype
 *
 * Input: name of resource
 * Output: resourcetypid (int2), oid of entry
 *
 * updates output and returns true if named resource found
 *
*/
static bool
GetResourceTypeByName(char *pNameIn, int *pTypeOut, Oid *pOidOut)
{
	Relation	pg_resourcetype;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	bool		bStat = false;

	/*
	 * SELECT * FROM pg_resourcetype WHERE resname = :1 FOR UPDATE
	 *
	 * XXX XXX: maybe should be share lock, ie remove FOR UPDATE ?
	 * XXX XXX: only one
	 */
	pg_resourcetype = table_open(ResourceTypeRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resourcetype_resname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(pNameIn));
	sscan = systable_beginscan(pg_resourcetype, ResourceTypeResnameIndexId, true,
							   NULL, 1, &scankey);

	tuple = systable_getnext(sscan);
	if (HeapTupleIsValid(tuple))
	{
		Form_pg_resourcetype rtype = (Form_pg_resourcetype) GETSTRUCT(tuple);

		*pOidOut = rtype->oid;
		*pTypeOut = rtype->restypid;
		bStat = true;
	}
	systable_endscan(sscan);
	table_close(pg_resourcetype, RowExclusiveLock);

	return (bStat);
} /* end GetResourceTypeByName */

static 
bool ValidPriority(char			*pResSetting)
{
	char *priority_vals[] = {"MAX",
							 "HIGH",
							 "MEDIUM",
							 "LOW",
							 "MIN",
							 NULL};
	int ii				  = 0;

	char *pval = priority_vals[ii]; 

	while (pval)
	{
		if (0 == pg_strcasecmp(pval, pResSetting))
			return true;
		pval = priority_vals[++ii]; 
	}
	return false;
}

/**
 * Validate memory limit setting.
 */
static 
bool ValidMemoryLimit(char			*pResSetting)
{
	int valueKB;
	const char *restyp = "MEMORY_LIMIT";

	bool result = parse_int(pResSetting, &valueKB, GUC_UNIT_KB, NULL);

	if (!result)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Invalid parameter value \"%s\" for "
						"resource type \"%s\". "
						"Value must be in kB, MB or GB.",
						pResSetting, restyp)));

	}
	else if (valueKB != -1 && valueKB < MIN_RESOURCEQUEUE_MEMORY_LIMIT_KB)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Invalid parameter value \"%s\" for "
						"resource type \"%s\". "
						"Value must be at least %dkB",
						pResSetting, restyp, (int) MIN_RESOURCEQUEUE_MEMORY_LIMIT_KB)));
		
	}
	
	return true;
}


/* ValidateResqueueCapabilityEntry
 *
 * Validate the resource setting for a pg_resqueuecapability entry.
 * Insert calls to separate per-resourcetype validation functions
 * here.
 */
static
void ValidateResqueueCapabilityEntry(int			 resTypeInt,
									 char			*pResSetting)
{
	bool		 bValid = true;
	char		*restyp = "unknown";

	switch(resTypeInt)
	{
		case PG_RESRCTYPE_ACTIVE_STATEMENTS:/* rsqcountlimit: count  */
		case PG_RESRCTYPE_MAX_COST:			/* rsqcostlimit: max_cost */
		case PG_RESRCTYPE_MIN_COST:			/* rsqignorecostlimit: min_cost */
		case PG_RESRCTYPE_COST_OVERCOMMIT:	/* rsqovercommit: cost_overcommit*/
			break;

		/* start of "pg_resourcetype" entries... */
		case PG_RESRCTYPE_PRIORITY:			/* backoff.c: priority queue */
			bValid = ValidPriority(pResSetting);
			restyp = "PRIORITY";
			break;				

		case PG_RESRCTYPE_MEMORY_LIMIT:
			bValid = ValidMemoryLimit(pResSetting);
			Assert(bValid);
			restyp = "MEMORY_LIMIT";
						
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unknown resource type \"%d\"",
							resTypeInt)));

			break;
	}
	
	if (!bValid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Invalid parameter value \"%s\" for "
						"resource type \"%s\"",
						pResSetting, restyp)));

}

/* MPP-6923: */
/* AddUpdResqueueCapabilityEntryInternal:
 *
 * Internal function to add a new entry to pg_resqueuecapability, or
 * update an existing one.  Key cols are queueid, restypint.  If
 * old_tuple is set (ie not InvalidOid), the update the ressetting column,
 * else insert a new row.
 *
 */
static void
AddUpdResqueueCapabilityEntryInternal(
								  Relation		 resqueuecap_rel,
								  Oid			 queueid,
								  int			 resTypeInt,
								  char			*pResSetting,
								  Relation		 rel,
								  HeapTuple		 old_tuple)
{
	HeapTuple	new_tuple;
	Datum		values[Natts_pg_resqueuecapability];
	bool		isnull[Natts_pg_resqueuecapability];
	bool		new_record_repl[Natts_pg_resqueuecapability];

	MemSet(isnull, 0, sizeof(bool) * Natts_pg_resqueuecapability);
	MemSet(new_record_repl, 0, sizeof(bool) * Natts_pg_resqueuecapability);

	values[Anum_pg_resqueuecapability_resqueueid - 1]  = 
			ObjectIdGetDatum(queueid);
	values[Anum_pg_resqueuecapability_restypid - 1]   = resTypeInt;

	Assert(pResSetting);

	values[Anum_pg_resqueuecapability_ressetting - 1] = 
			CStringGetTextDatum(pResSetting);
	/* set this column to update */
	new_record_repl[Anum_pg_resqueuecapability_ressetting - 1]  = true;

	ValidateResqueueCapabilityEntry(resTypeInt, pResSetting);

	if (HeapTupleIsValid(old_tuple))
	{
		new_tuple = heap_modify_tuple(old_tuple,
									  RelationGetDescr(resqueuecap_rel),
									  values, isnull, new_record_repl);

		CatalogTupleUpdate(resqueuecap_rel, &old_tuple->t_self, new_tuple);
	}
	else
	{
		new_tuple = heap_form_tuple(RelationGetDescr(resqueuecap_rel), values, isnull);

		CatalogTupleInsert(resqueuecap_rel, new_tuple);
	}

	if (HeapTupleIsValid(old_tuple))
		heap_freetuple(new_tuple);
} /* end AddUpdResqueueCapabilityEntryInternal */
				
/* MPP-6923: */				  
static void
AlterResqueueCapabilityEntry(Oid queueid,
							 ListCell *initcell,
							 bool bCreate)
{
	ListCell	*lc;
	List		*elems	   = NIL;
	List		*dropelems = NIL;
	List		*dupcheck  = NIL;
	HeapTuple	 tuple;
	Relation	 rel	   = NULL;
	bool		 bWithout  = false;
	TupleDesc	 tupdesc   = NULL;

#ifdef USE_ASSERT_CHECKING
	{
		DefElem    *defel = (DefElem *) lfirst(initcell);
		Assert(0 == strcmp(defel->defname, "withliststart"));
	}
#endif

	initcell = lnext(initcell);

	/* walk the original list and build a list of valid entries */

	for_each_cell(lc, initcell)
	{
		DefElem *defel		= (DefElem *) lfirst(lc);
		Oid		 resTypeOid = InvalidOid;
		int		 resTypeInt = 0;
		List	*pentry		= NIL;
		Value	*pKeyVal	= NULL;
		Value	*pStrVal	= NULL;

		if (!bWithout && (strcmp(defel->defname, "withoutliststart") == 0))
		{
			bWithout = true;

			rel = heap_open(ResourceTypeRelationId, RowExclusiveLock);
			tupdesc = RelationGetDescr(rel);

			goto L_loop_cont;
		}

		/* ignore the basic threshold entries -- should already be processed */
		if (strcmp(defel->defname, "active_statements") == 0)
			goto L_loop_cont;
		if (strcmp(defel->defname, "max_cost") == 0)
			goto L_loop_cont;
		if (strcmp(defel->defname, "cost_overcommit") == 0)
			goto L_loop_cont;
		if (strcmp(defel->defname, "min_cost") == 0)
			goto L_loop_cont;

		if (!GetResourceTypeByName(defel->defname, &resTypeInt, &resTypeOid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" is not a valid resource type",
							defel->defname)));

		pKeyVal = makeString(defel->defname);

		/* WITHOUT clause value determined in pg_resourcetype */
		if (!bWithout)
			pStrVal = makeString(defGetString(defel));
		else
		{
			ScanKeyData scankey;
			SysScanDesc sscan;

			pStrVal = NULL; /* if NULL, delete entry from
							 * pg_resqueuecapability 
							 */
			ScanKeyInit(&scankey,
						Anum_pg_resourcetype_restypid,
						BTEqualStrategyNumber, F_INT2EQ,
						Int16GetDatum(resTypeInt));

			sscan = systable_beginscan(rel, ResourceTypeRestypidIndexId,
									   true, NULL, 1, &scankey);

			while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
			{
				char	   *shutoff_str;
				Datum		shutoff_datum;
				bool		isnull = false;
				Form_pg_resourcetype rtyp = 
						(Form_pg_resourcetype)GETSTRUCT(tuple);

				if (!rtyp->reshasdisable)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("option \"%s\" cannot be disabled",
									defel->defname)));
				}

				/* required type must have a default value if it can
				 * be disabled 
				 */
				if (!rtyp->reshasdefault)
				{
					if (!rtyp->resrequired)
						/* optional resource without a default is
						 * turned off by removing entry from
						 * pg_resqueuecapability 
						 */
						break;
					else
					{
						/* XXX XXX */
						Assert(0);
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("required option \"%s\" cannot be disabled",
										defel->defname)));
					}
				}

				/* get the shutoff string */
				shutoff_datum = 
						heap_getattr(tuple,
									 Anum_pg_resourcetype_resdisabledsetting,
									 tupdesc,
									 &isnull);
				Assert(!isnull);
				shutoff_str = TextDatumGetCString(shutoff_datum);

				pStrVal = makeString(shutoff_str);

				break;
			} /* end while heaptuple is valid */
			systable_endscan(sscan);
		}

		/* check for duplicate key specifications */
		if (list_member(dupcheck, pKeyVal))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("conflicting or redundant option for \"%s\"",
							defel->defname)));

		dupcheck = lappend(dupcheck, pKeyVal);

		pentry = list_make2(
							makeInteger(resTypeInt),
							pStrVal);

		/* list of lists - (resource type, resource setting) */
		if (bWithout)
		{
			/* if the "without" entry has an "off" value, then treat
			 * it as a regular "with" item and update it in
			 * pg_resqueuecapability, else remove its entry
			 */
			if (!pStrVal)
				dropelems = lappend(dropelems, pentry);
			else
				elems = lappend(elems, pentry);
		}
		else
			elems = lappend(elems, pentry);

	L_loop_cont:
		resTypeInt = 0; /* make compiler happy */
	}

	if (bWithout)
		heap_close(rel, RowExclusiveLock); /* close pg_resourcetype */

	if (bCreate)
	{
		SysScanDesc sscan;

		/* If creating a new resource queue, check pg_resourcetype for
		 * optional types that have a default value.  Check for
		 * corresponding match in dupcheck -- if no entry then add the
		 * one to the WITH list (elems) with the default. 
		 */
		rel = heap_open(ResourceTypeRelationId, RowExclusiveLock);
		tupdesc = RelationGetDescr(rel);

		/* Note: key is empty - scan entire table */

		sscan = systable_beginscan(rel, InvalidOid, false, NULL, 0, NULL);
		while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
		{
			List	   *pentry;
			Value	   *pResnameVal;
			char	   *default_str;
			Datum		default_datum;
			bool		isnull = false;
			Form_pg_resourcetype rtyp = 
					(Form_pg_resourcetype)GETSTRUCT(tuple);

			/* only interested in resources types with default value */
			if (!rtyp->reshasdefault)
				continue;

			/* Note: ignore the dummy entries for active_statements,
			 * max_cost, etc 
			 */
			if (rtyp->restypid < PG_RESRCTYPE_PRIORITY)
				continue;

			/* if resource has a default value, see if it was already
			 * specified 
			 */
			pResnameVal = makeString(NameStr(rtyp->resname));

			if (list_member(dupcheck, pResnameVal))
				continue;

			/* resource was not specified, so add its default value to
			 * the WITH list 
			 */

			/* get the default string */
			default_datum = 
					heap_getattr(tuple,
								 Anum_pg_resourcetype_resdefaultsetting,
								 tupdesc,
								 &isnull);
			Assert(!isnull);
			default_str = TextDatumGetCString(default_datum);

			/* add the new entry to dupcheck and WITH elems */
			dupcheck = lappend(dupcheck, pResnameVal);

			pentry = list_make2(makeInteger(rtyp->restypid),
								makeString(default_str));

			elems = lappend(elems, pentry);
		} /* end while heaptuple is valid */
		systable_endscan(sscan);

		heap_close(rel, RowExclusiveLock); /* close pg_resourcetype */
	} /* end if bCreate */

	/* insert/update valid entries in pg_resqueuecapability */

	rel = heap_open(ResQueueCapabilityRelationId, RowExclusiveLock);

	foreach(lc, elems)
	{
		ListCell		*lc2;
		List			*pentry		 = lfirst(lc);
		int				 resTypeInt;
		Node			*pVal;
		char			*pResSetting = "";
		int				 ii;
		SysScanDesc sscan;
		ScanKeyData skey;

		Assert (2 == list_length(pentry));

		lc2 = list_head(pentry);
		
		resTypeInt = intVal(lfirst(lc2));

		lc2 = lnext(lc2);
		
		pVal = lfirst(lc2);

		pResSetting = strVal(pVal);

		ScanKeyInit(&skey,
					Anum_pg_resqueuecapability_resqueueid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(queueid));
		sscan = systable_beginscan(rel, ResQueueCapabilityResqueueidIndexId, true,
								   NULL, 1, &skey);

		ii = 0;
		while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
		{
			if (HeapTupleIsValid(tuple))
			{
				if (resTypeInt ==
					((Form_pg_resqueuecapability) GETSTRUCT(tuple))->restypid)
				{
					/* found it -- update it */
					AddUpdResqueueCapabilityEntryInternal(rel,
														  queueid,
														  resTypeInt,
														  pResSetting,
														  rel,
														  tuple);
					ii++;
				}
			}
		}

		systable_endscan(sscan);

		if (!ii)
		{
			/* does not exist -- add it */
			AddUpdResqueueCapabilityEntryInternal(rel,
												  queueid,
												  resTypeInt,
												  pResSetting,
												  rel,
												  NULL /* InvalidOid */);
		}

	} /* end foreach elem */


	/* drop these items here */

	foreach(lc, dropelems)
	{
		ListCell		*lc2;
		List			*pentry		 = lfirst(lc);
		int				 resTypeInt;
		Node			*pVal;
		int				 ii;
		ScanKeyData	 key[1];
		SysScanDesc	 scan;

		Assert (2 == list_length(pentry));

		lc2 = list_head(pentry);
		
		resTypeInt = intVal(lfirst(lc2));

		lc2 = lnext(lc2);
		
		pVal = lfirst(lc2);

		/* CaQL UNDONE: no test coverage */
		ScanKeyInit(&key[0],
					Anum_pg_resqueuecapability_resqueueid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(queueid));

		scan = systable_beginscan(rel,
								  ResQueueCapabilityResqueueidIndexId,
								  /* XXX XXX XXX XXX : snapshotnow ? */
								  true, NULL, 1, key);

		ii = 0;

		while (HeapTupleIsValid(tuple = systable_getnext(scan)))
		{
			if (HeapTupleIsValid(tuple))
			{
				if (resTypeInt ==
					((Form_pg_resqueuecapability) GETSTRUCT(tuple))->restypid)
				{
					/* no "off" setting -- just remove entry */
					simple_heap_delete(rel, &tuple->t_self);
					ii++;
				}
			}
		}
		systable_endscan(scan); 

	} /* end foreach elem */

	heap_close(rel, RowExclusiveLock);
} /* end AlterResqueueCapabilityEntry */

/* MPP-6923: */				  
List * 
GetResqueueCapabilityEntry(Oid  queueid)
{
	List		*elems = NIL;
	HeapTuple	 tuple;
	ScanKeyData scankey;
	SysScanDesc sscan;
	Relation	 rel;
	TupleDesc	 tupdesc;

	Assert(IsTransactionState());

	/* SELECT * FROM pg_resqueuecapability WHERE resqueueid = :1 */
	rel = heap_open(ResQueueCapabilityRelationId, AccessShareLock);

	tupdesc = RelationGetDescr(rel);

	ScanKeyInit(&scankey,
				Anum_pg_resqueuecapability_resqueueid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(queueid));
	sscan = systable_beginscan(rel, ResQueueCapabilityResqueueidIndexId, true,
							   NULL, 1, &scankey);

	while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
	{
		if (HeapTupleIsValid(tuple))
		{
			List	   *pentry;
			int			resTypeInt;
			Datum		resSet_datum;
			char	   *resSetting;
			bool		isnull = false;

			resTypeInt =
					((Form_pg_resqueuecapability) GETSTRUCT(tuple))->restypid;

			resSet_datum = heap_getattr(tuple,
										Anum_pg_resqueuecapability_ressetting,
										tupdesc,
										&isnull);
			Assert(!isnull);
			resSetting = TextDatumGetCString(resSet_datum);

			pentry = list_make2(
					makeInteger(resTypeInt),
					makeString(resSetting));

			/* list of lists */
			elems = lappend(elems, pentry);
		}
	}
	systable_endscan(sscan);

	heap_close(rel, AccessShareLock);
	
	return (elems);
} /* end GetResqueueCapabilityEntry */

/*
 * CREATE RESOURCE QUEUE
 */
void
CreateQueue(CreateQueueStmt *stmt)
{
	Relation	pg_resqueue_rel;
	TupleDesc	pg_resqueue_dsc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple;
	Oid			queueid;
	Cost		thresholds[NUM_RES_LIMIT_TYPES];
	Datum		new_record[Natts_pg_resqueue];
	bool		new_record_nulls[Natts_pg_resqueue];
	ListCell    *option;
	ListCell    *pWithList = NULL;

	/* thresholds for active and cost limiters. */
	Cost		activelimit = INVALID_RES_LIMIT_THRESHOLD;	
	Cost		costlimit	= INVALID_RES_LIMIT_THRESHOLD;

	/* overcommit indicator */
	bool		overcommit = false;

	/* cost ignore limit for queries */
	float4		ignorelimit = 0;

	DefElem     *dactivelimit = NULL;
	DefElem     *dcostlimit	  = NULL;
	DefElem     *dovercommit  = NULL;
	DefElem     *dignorelimit = NULL;
	bool		 queueok	  = false;
	bool		 bWith		  = false;

	/* Permission check - only superuser can create queues. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to create resource queues")));

	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "active_statements") == 0)
		{
			if (dactivelimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dactivelimit = defel;
		}
		else if (strcmp(defel->defname, "max_cost") == 0)
		{
			if (dcostlimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dcostlimit = defel;
		}
		else if (strcmp(defel->defname, "cost_overcommit") == 0)
		{
			if (dovercommit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dovercommit = defel;
		}
		else if (strcmp(defel->defname, "min_cost") == 0)
		{
			if (dignorelimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dignorelimit = defel;
		}
		else if (strcmp(defel->defname, "withliststart") == 0)
		{
			/* don't allow a "with list entry" with this defname... */
			if (bWith)
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" is not a valid resource type",
							defel->defname)));

			/* marks start of WITH options... */
			pWithList = option;
			bWith = true;
		}
		else
		{
			/* silently ignore options on WITH list -- will deal with
			 * them later in AlterResqueueCapabilityEntry 
			 */
			if (!bWith)
				elog(ERROR, "option \"%s\" not recognized",
					 defel->defname);
		}
	}

	/* Perform range checks on the various thresholds.*/
	if (dactivelimit)
	{
		activelimit = (Cost) defGetInt64(dactivelimit);
		if (!(activelimit == INVALID_RES_LIMIT_THRESHOLD || (activelimit > 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("active threshold cannot be less than %d or equal to 0", 
							INVALID_RES_LIMIT_THRESHOLD)));
	}

	if (dcostlimit)
	{
		costlimit = (Cost) defGetNumeric(dcostlimit);
		if (!(costlimit == INVALID_RES_LIMIT_THRESHOLD || (costlimit > 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cost threshold cannot be less than %d or equal to 0", 
							INVALID_RES_LIMIT_THRESHOLD)));
	}

	if(dovercommit)
	{
		overcommit = defGetBoolean(dovercommit);
	}

	if(dignorelimit)
	{
		ignorelimit = (Cost) defGetNumeric(dignorelimit);
		if (!(ignorelimit == INVALID_RES_LIMIT_THRESHOLD || (ignorelimit >= 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("min_cost threshold cannot be negative")));
	}


	/*
	 * Check that at least one threshold is to be set.
	 */
	if ( !dactivelimit && !dcostlimit)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("at least one threshold (\"ACTIVE_STATEMENTS\", \"MAX_COST\") must be specified")));

	/* Check all thresholds are not invalid. */
	if (activelimit == INVALID_RES_LIMIT_THRESHOLD &&
		costlimit   == INVALID_RES_LIMIT_THRESHOLD)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("the value for at least one threshold (\"ACTIVE_STATEMENTS\", \"MAX_COST\") must be different from no limit (%d)",
						 INVALID_RES_LIMIT_THRESHOLD)));

	/*
	 * Check for an illegal name ('none' is used to signify no queue in ALTER
	 * ROLE).
	 */
	if (strcmp(stmt->queue, "none") == 0)
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("resource queue name \"%s\" is reserved",
						stmt->queue)));

	/*
	 * Check the pg_resqueue relation to be certain the queue doesn't already
	 * exist. 
	 */
	pg_resqueue_rel = heap_open(ResQueueRelationId, RowExclusiveLock);
	pg_resqueue_dsc = RelationGetDescr(pg_resqueue_rel);

	/**
	 * Get database locks in anticipation that we'll need to access
	 * this catalog table later.
	 */
	Relation resqueueCapabilityRel = 
			heap_open(ResQueueCapabilityRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resqueue_rsqname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->queue));

	sscan = systable_beginscan(pg_resqueue_rel, ResQueueRsqnameIndexId, true,
							   NULL, 1, &scankey);

	if (systable_getnext(sscan))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("resource queue \"%s\" already exists",
						stmt->queue)));

	systable_endscan(sscan);

	/*
	 * Build a tuple to insert
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));

	new_record[Anum_pg_resqueue_rsqname - 1] =
		DirectFunctionCall1(namein, CStringGetDatum(stmt->queue));

	new_record[Anum_pg_resqueue_rsqcountlimit - 1] = 
		Float4GetDatum(activelimit);

	new_record[Anum_pg_resqueue_rsqcostlimit - 1] = 
		Float4GetDatum(costlimit);

	new_record[Anum_pg_resqueue_rsqovercommit - 1] = 
		BoolGetDatum(overcommit);

	new_record[Anum_pg_resqueue_rsqignorecostlimit - 1] = 
		Float4GetDatum(ignorelimit);

	queueid = GetNewOidForResQueue(pg_resqueue_rel, ResQueueOidIndexId, Anum_pg_resqueue_oid,
								   stmt->queue);
	new_record[Anum_pg_resqueue_oid - 1] = queueid;

	tuple = heap_form_tuple(pg_resqueue_dsc, new_record, new_record_nulls);

	/*
	 * Insert new record in the pg_resqueue table
	 */
	CatalogTupleInsert(pg_resqueue_rel, tuple);

	/* process the remainder of the WITH (...) list items */
	if (bWith)
		AlterResqueueCapabilityEntry(queueid, pWithList, true);

	/* 
	 * We must bump the command counter to make the new entry 
	 * in the pg_resqueuecapability table visible 
	 */
	CommandCounterIncrement();

	/*
	 * Create the in-memory resource queue, if resource scheduling is on,
	 * otherwise don't - and gripe a little about it.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (IsResQueueEnabled())
		{
			LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

			thresholds[RES_COUNT_LIMIT]	 = activelimit;
			thresholds[RES_COST_LIMIT]	 = costlimit;

			thresholds[RES_MEMORY_LIMIT] = 
					ResourceQueueGetMemoryLimit(queueid);
			queueok	= ResCreateQueue(queueid, 
									 thresholds, 
									 overcommit, 
									 ignorelimit);
			
			LWLockRelease(ResQueueLock);

			if (!queueok)
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
						 errmsg("insufficient resource queues available"),
						 errhint("Increase max_resource_queues")));
		}
		else
		{
				ereport(WARNING,
						(errmsg("resource queue is disabled"),
						 errhint("To enable set gp_resource_manager=queue")));
		}
	}

	/* MPP-6929, MPP-7583: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
		MetaTrackAddObject(ResQueueRelationId,
						   queueid,
						   GetUserId(), /* not ownerid */
						   "CREATE", "RESOURCE QUEUE"
				);
	}

	heap_close(resqueueCapabilityRel, NoLock);
	heap_close(pg_resqueue_rel, NoLock);
}


/*
 * ALTER RESOURCE QUEUE
 */
void
AlterQueue(AlterQueueStmt *stmt)
{
	Relation	pg_resqueue_rel;
	TupleDesc	pg_resqueue_dsc;
	ScanKeyData scankey;
	SysScanDesc sscan;
	HeapTuple	tuple, new_tuple;
	Oid			queueid;
	Cost		thresholds[NUM_RES_LIMIT_TYPES];
	Datum		new_record[Natts_pg_resqueue];
	bool		new_record_nulls[Natts_pg_resqueue];
	bool		new_record_repl[Natts_pg_resqueue];
	ListCell    *option;
	ListCell    *pWithList = NULL;

	/* thresholds for active and cost limiters. */
	Cost		activelimit = INVALID_RES_LIMIT_THRESHOLD;
	Cost		costlimit	= INVALID_RES_LIMIT_THRESHOLD;

	/* overcommit indicator */
	bool		overcommit = false;

	/* cost ignore limit for queries */
	float4		ignorelimit = 0;

	DefElem     *dactivelimit = NULL;
	DefElem     *dcostlimit	  = NULL;
	DefElem     *dovercommit  = NULL;
	DefElem     *dignorelimit = NULL;
	ResAlterQueueResult		queueok;
	bool		 bWith		   = false;
	bool		 bWithOut	   = false;
	int			 numopts	   = 0;
	char		*alter_subtype = "";	/* metadata tracking: kind of
										   redundant to say "role" */

	/* Permission check - only superuser can alter queues. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to alter resource queues")));

	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "active_statements") == 0)
		{
			if (dactivelimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (!bWithOut)
				dactivelimit = defel;
			else
				dactivelimit = 
						makeDefElem("active_statements", 
									(Node *)
									makeFloat(INVALID_RES_LIMIT_STRING),
									-1);

			numopts++; alter_subtype = defel->defname;
		}
		else if (strcmp(defel->defname, "max_cost") == 0)
		{
			if (dcostlimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (!bWithOut)
				dcostlimit = defel;
			else
				dcostlimit = 
						makeDefElem("max_cost", 
									(Node *)
									makeInteger(costlimit),
									-1);

			numopts++; alter_subtype = defel->defname;
		}
		else if (strcmp(defel->defname, "cost_overcommit") == 0)
		{
			if (dovercommit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (!bWithOut)
				dovercommit = defel;
			else
				dovercommit = 
						makeDefElem("cost_overcommit", 
									(Node *)
									makeInteger(overcommit),
									-1);

			numopts++; alter_subtype = defel->defname;
		}
		else if (strcmp(defel->defname, "min_cost") == 0)
		{
			if (dignorelimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			if (!bWithOut)
				dignorelimit = defel;
			else
				dignorelimit = 
						makeDefElem("min_cost", 
									(Node *)
									makeFloat("0"), /* MPP-7817 */
									-1);

			numopts++; alter_subtype = defel->defname;

		}
		else if (strcmp(defel->defname, "withliststart") == 0)
		{
			/* don't allow a "with list entry" with this defname... */
			if (bWith)
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" is not a valid resource type",
							defel->defname)));

			/* marks start of WITH options... */
			pWithList = option;
			bWith = true;
		}
		else if (strcmp(defel->defname, "withoutliststart") == 0)
		{
			/* don't allow a "with out list entry" with this defname... */
			if (!bWith || bWithOut)
				ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" is not a valid resource type",
							defel->defname)));

			/* marks start of WITHOUT options... */
			bWithOut = true;
		}
		else
		{
			/* silently ignore options on WITH list -- will deal with
			 * them later in AlterResqueueCapabilityEntry 
			 */
			if (!bWith)
				elog(ERROR, "option \"%s\" not recognized",
					 defel->defname);

			numopts++; alter_subtype = defel->defname;
		}
	}

	if (numopts > 1)
	{
		char allopts[NAMEDATALEN];

		sprintf(allopts, "%d OPTIONS", numopts);

		alter_subtype = pstrdup(allopts);
	}
	else if (0 == numopts)
	{
		alter_subtype = "0 OPTIONS";
	}
	else
	{
		char *tempo = asc_toupper(alter_subtype, strlen(alter_subtype));

		alter_subtype = tempo;
	}

	/* Perform range checks on the various thresholds. */
	if (dactivelimit)
	{
		activelimit = (Cost) defGetInt64(dactivelimit);
		if (!(activelimit == INVALID_RES_LIMIT_THRESHOLD || (activelimit > 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("active threshold cannot be less than %d or equal to 0", 
							INVALID_RES_LIMIT_THRESHOLD)));
	}

	if (dcostlimit)
	{
		costlimit = (Cost) defGetNumeric(dcostlimit);
		if (!(costlimit == INVALID_RES_LIMIT_THRESHOLD || (costlimit > 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cost threshold must be equal to %d or greater than 0", 
							INVALID_RES_LIMIT_THRESHOLD)));
	}

	if (dovercommit)
	{
		overcommit = defGetBoolean(dovercommit);
	}

	if(dignorelimit)
	{
		ignorelimit = (Cost) defGetNumeric(dignorelimit);
		if (!(ignorelimit == INVALID_RES_LIMIT_THRESHOLD || 
			  (ignorelimit >= 0)))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("min_cost threshold cannot be negative")));
	}

	/*
	 * Check that at least one threshold is to be set.
	 */
	if (!dactivelimit && !dcostlimit && !dovercommit && !dignorelimit)
	{
		ListCell		*initcell = pWithList;

		if (bWith && initcell && lnext(initcell))
		{
			/* if have an item on the "with list", don't need to set a
			 * threshold 
			 */
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("at least one threshold, overcommit or ignore limit must be specified")));

	}

	/*
	 * Check the pg_resqueue relation to be certain the queue already
	 * exists. 
	 */
	pg_resqueue_rel = heap_open(ResQueueRelationId, RowExclusiveLock);

	/**
	 * Get database locks in anticipation that we'll need to access this catalog table later.
	 */
	Relation resqueueCapabilityRel = heap_open(ResQueueCapabilityRelationId, RowExclusiveLock);
	pg_resqueue_dsc = RelationGetDescr(pg_resqueue_rel);

	ScanKeyInit(&scankey,
				Anum_pg_resqueue_rsqname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->queue));
	sscan = systable_beginscan(pg_resqueue_rel, ResQueueRsqnameIndexId,
							   true, NULL, 1, &scankey);
	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource queue \"%s\" does not exist",
						stmt->queue)));

	/*
	 * Remember the Oid and current thresholds, for updating the in-memory
	 * queue later.
	 */
	queueid = ((Form_pg_resqueue) GETSTRUCT(tuple))->oid;
	thresholds[RES_COUNT_LIMIT] = 
		((Form_pg_resqueue) GETSTRUCT(tuple))->rsqcountlimit;
	thresholds[RES_COST_LIMIT] = 
		((Form_pg_resqueue) GETSTRUCT(tuple))->rsqcostlimit;
	thresholds[RES_MEMORY_LIMIT] = ResourceQueueGetMemoryLimit(queueid);

	/* Also set overcommit if it was *not* specified. */
	if (!dovercommit)
		overcommit = ((Form_pg_resqueue) GETSTRUCT(tuple))->rsqovercommit;

	/* Also set ignore cost limit if it was *not* specified. */
	if (!dignorelimit)
		ignorelimit = ((Form_pg_resqueue) GETSTRUCT(tuple))->rsqignorecostlimit;


	/*
	 * Build a tuple to update.
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	if (dactivelimit)
	{
		new_record[Anum_pg_resqueue_rsqcountlimit - 1] = 
			Float4GetDatum(activelimit);
		new_record_repl[Anum_pg_resqueue_rsqcountlimit - 1] = true;

		thresholds[RES_COUNT_LIMIT] = activelimit;
		
	}

	if (dcostlimit)
	{
		new_record[Anum_pg_resqueue_rsqcostlimit - 1] = 
			Float4GetDatum(costlimit);
		new_record_repl[Anum_pg_resqueue_rsqcostlimit - 1] = true;

		thresholds[RES_COST_LIMIT] = costlimit;
	}

	if (dovercommit)
	{
		new_record[Anum_pg_resqueue_rsqovercommit - 1] = 
			BoolGetDatum(overcommit);
		new_record_repl[Anum_pg_resqueue_rsqovercommit - 1] = true;

	}

	if (dignorelimit)
	{
		new_record[Anum_pg_resqueue_rsqignorecostlimit - 1] = 
			Float4GetDatum(ignorelimit);
		new_record_repl[Anum_pg_resqueue_rsqignorecostlimit - 1] = true;
	}


	/*
	 * Check we are not going to set all the thresholds to be invalid.
	 */
	if (thresholds[RES_COUNT_LIMIT] == INVALID_RES_LIMIT_THRESHOLD &&
		thresholds[RES_COST_LIMIT] == INVALID_RES_LIMIT_THRESHOLD)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("the value for at least one threshold (\"ACTIVE_STATEMENTS\", \"MAX_COST\") must be different from no limit (%d)",
						INVALID_RES_LIMIT_THRESHOLD)));

	/*
	 * update the tuple in the pg_resqueue table
	 */
	new_tuple = heap_modify_tuple(tuple, pg_resqueue_dsc, new_record,
									new_record_nulls, new_record_repl);

	CatalogTupleUpdate(pg_resqueue_rel, &tuple->t_self, new_tuple);

	systable_endscan(sscan);

	/* process the remainder of the WITH (...) list items */
	if (bWith)
		AlterResqueueCapabilityEntry(queueid, pWithList, false);

	/* 
	 * We must bump the command counter to make the altered memory limit 
	 * in the pg_resqueuecapability table visible 
	 */
	CommandCounterIncrement();

	/** Get new memory limit if changed */
	thresholds[RES_MEMORY_LIMIT] = ResourceQueueGetMemoryLimit(queueid);

	heap_freetuple(new_tuple);

	/*
	 * If resource scheduling is on, see if we can change the threshold(s)
	 * for the in-memory queue.
	 * otherwise don't - and gripe a little about it.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (IsResQueueEnabled())
		{
			LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

			queueok = ResAlterQueue(queueid, 
									thresholds, 
									overcommit, 
									ignorelimit);
			
			LWLockRelease(ResQueueLock);
			
			if (queueok != ALTERQUEUE_OK)
			{
				if (queueok == ALTERQUEUE_SMALL_THRESHOLD)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_LIMIT_VALUE),
							 errmsg("thresholds cannot be less than current values")));
				else if (queueok == ALTERQUEUE_OVERCOMMITTED)
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_LIMIT_VALUE),
								 errmsg("disabling overcommit cannot leave queue in possibly overcommitted state")));
				else
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("queue hash table corrupted")));

			}

		}
		else
		{
			ereport(WARNING,
					(errmsg("resource queue is disabled"),
					 errhint("To enable set gp_resource_manager=queue")));
		}
	}

	heap_close(resqueueCapabilityRel, NoLock);

	/* MPP-6929, MPP-7583: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
		MetaTrackUpdObject(ResQueueRelationId,
						   queueid,
						   GetUserId(), /* not ownerid */
						   "ALTER", alter_subtype
				);
	}
	heap_close(pg_resqueue_rel, NoLock);
}


/*
 * DROP RESOURCE QUEUE
 */
void
DropQueue(DropQueueStmt *stmt)
{
	Relation	 pg_resqueue_rel;
	HeapTuple	 tuple;
	ScanKeyData scankey;
	SysScanDesc sscan;
	ScanKeyData authid_scankey;
	SysScanDesc authid_scan;
	Oid			 queueid;
	bool		 queueok = false;


	/* Permission check - only superuser can drop queues. */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to drop resource queues")));


	/*
	 * Check the pg_resqueue relation to be certain the queue already
	 * exists. 
	 */
	pg_resqueue_rel = heap_open(ResQueueRelationId, RowExclusiveLock);
	
	/**
	 * Get database locks in anticipation that we'll need to access this catalog table later.
	 */
	Relation resqueueCapabilityRel = heap_open(ResQueueCapabilityRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_resqueue_rsqname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(stmt->queue));

	sscan = systable_beginscan(pg_resqueue_rel, ResQueueRsqnameIndexId, true,
							   NULL, 1, &scankey);

	tuple = systable_getnext(sscan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource queue \"%s\" does not exist",
						stmt->queue)));

	/*
	 * Remember the Oid, for destroying the in-memory
	 * queue later.
	 */
	queueid = ((Form_pg_resqueue) GETSTRUCT(tuple))->oid;

	/*
	 * Check to see if any roles are in this queue.
	 */
	Relation authIdRel = heap_open(AuthIdRelationId, RowExclusiveLock);
	ScanKeyInit(&authid_scankey,
				Anum_pg_authid_rolresqueue,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(queueid));

	authid_scan = systable_beginscan(authIdRel, AuthIdRolResQueueIndexId, true,
							   NULL, 1, &authid_scankey);

	if (systable_getnext(authid_scan) != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
				 errmsg("resource queue \"%s\" is used by at least one role",
						stmt->queue)));

	/* MPP-6926: cannot DROP default queue  */
	if (queueid == DEFAULTRESQUEUE_OID)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot drop default resource queue \"%s\"",
						stmt->queue)));

	systable_endscan(authid_scan);
	heap_close(authIdRel, RowExclusiveLock);

	/*
	 * Delete the queue from the catalog.
	 */
	simple_heap_delete(pg_resqueue_rel, &tuple->t_self);

	systable_endscan(sscan);

	/*
	 * If resource scheduling is on, see if we can destroy the in-memory queue.
	 * otherwise don't - and gripe a little about it.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (IsResQueueEnabled())
		{
			LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

			queueok = ResDestroyQueue(queueid);
		
			LWLockRelease(ResQueueLock);

			if (!queueok)
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_RESOURCES),
						 errmsg("resource queue cannot be dropped as is in use")));
		}
		else
		{
			ereport(WARNING,
					(errmsg("resource queue is disabled"),
					 errhint("To enable set gp_resource_manager=queue")));
		}
	}

	/*
	 * Remove any comments on this resource queue
	 */
	DeleteSharedComments(queueid, ResQueueRelationId);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
	}
	/* MPP-6929, MPP-7583: metadata tracking */
	MetaTrackDropObject(ResQueueRelationId,
						queueid);

	/* MPP-6923: drop the extended attributes for this queue */
	ScanKeyInit(&scankey,
				Anum_pg_resqueuecapability_resqueueid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(queueid));

	sscan = systable_beginscan(resqueueCapabilityRel, ResQueueCapabilityResqueueidIndexId,
							   true, NULL, 1, &scankey);

	while ((tuple = systable_getnext(sscan)) != NULL)
		simple_heap_delete(resqueueCapabilityRel, &tuple->t_self);

	systable_endscan(sscan);


	heap_close(resqueueCapabilityRel, NoLock);

	heap_close(pg_resqueue_rel, NoLock);
}

Oid
get_resqueue_oid(const char *queuename, bool missing_ok)
{
	Relation	rel;
	ScanKeyData	scankey;
	SysScanDesc	scan;
	HeapTuple	tuple;
	Oid			oid;

	rel = heap_open(ResQueueRelationId, AccessShareLock);
	ScanKeyInit(&scankey, Anum_pg_resqueue_rsqname,
				BTEqualStrategyNumber, F_NAMEEQ,
				CStringGetDatum(queuename));
	scan = systable_beginscan(rel, ResQueueRsqnameIndexId, true,
							  NULL, 1, &scankey);
	tuple = systable_getnext(scan);

	if (HeapTupleIsValid(tuple))
		oid = ((Form_pg_resqueue) GETSTRUCT(tuple))->oid;
	else
		oid = InvalidOid;

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	if (!OidIsValid(oid) && !missing_ok)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource queue \"%s\" does not exist",
						queuename)));

	return oid;
}
