/*-------------------------------------------------------------------------
 *
 * pg_exttable.c
 *	  routines to support manipulation of the pg_exttable relation
 *
 * Portions Copyright (c) 2009, Greenplum Inc
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_exttable.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "access/genam.h"
#include "catalog/catquery.h"
#include "access/heapam.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "mb/pg_wchar.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"
#include "utils/uri.h"


/*
 * InsertExtTableEntry
 * 
 * Adds an entry into the pg_exttable catalog table. The entry
 * includes the reloid of the external relation that was created
 * in pg_class and a text array of external location URIs among
 * other external table properties.
 */
void
InsertExtTableEntry(Oid 	tbloid, 
					bool 	iswritable,
					bool 	isweb,
					bool	issreh,
					char	formattype,
					char	rejectlimittype,
					char*	commandString,
					int		rejectlimit,
					Oid		fmtErrTblOid,
					int		encoding,
					Datum	formatOptStr,
					Datum	locationExec,
					Datum	locationUris)
{
	Relation	pg_exttable_rel;
	HeapTuple	pg_exttable_tuple = NULL;
	bool		nulls[Natts_pg_exttable];
	Datum		values[Natts_pg_exttable];
	cqContext	cqc;
	cqContext  *pcqCtx;

 	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));

    /*
     * Open and lock the pg_exttable catalog.
     */
	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), pg_exttable_rel),
			cql("INSERT INTO pg_exttable",
				NULL));

	values[Anum_pg_exttable_reloid - 1] = ObjectIdGetDatum(tbloid);
	values[Anum_pg_exttable_fmttype - 1] = CharGetDatum(formattype);
	values[Anum_pg_exttable_fmtopts - 1] = formatOptStr;

	if(commandString)
	{
		/* EXECUTE type table - store command and command location */

		values[Anum_pg_exttable_command - 1] =
		DirectFunctionCall1(textin, CStringGetDatum(commandString));
		values[Anum_pg_exttable_location - 1] = locationExec;

	}
	else
	{
		/* LOCATION type table - store uri locations. command is NULL */

		values[Anum_pg_exttable_location - 1] = locationUris;
		values[Anum_pg_exttable_command - 1] = 0;
		nulls[Anum_pg_exttable_command - 1] = true;
	}

	if(issreh)
	{
		values[Anum_pg_exttable_rejectlimit -1] = Int32GetDatum(rejectlimit);
		values[Anum_pg_exttable_rejectlimittype - 1] = CharGetDatum(rejectlimittype);

		/* if error log specified store its OID, otherwise put a NULL */
		if(fmtErrTblOid != InvalidOid)
			values[Anum_pg_exttable_fmterrtbl - 1] = ObjectIdGetDatum(fmtErrTblOid);
		else
			nulls[Anum_pg_exttable_fmterrtbl - 1] = true;
	}
	else
	{
		nulls[Anum_pg_exttable_rejectlimit -1] = true;
		nulls[Anum_pg_exttable_rejectlimittype - 1] = true;
		nulls[Anum_pg_exttable_fmterrtbl - 1] = true;
	}

	values[Anum_pg_exttable_encoding - 1] = Int32GetDatum(encoding);
	values[Anum_pg_exttable_writable - 1] = BoolGetDatum(iswritable);

	pg_exttable_tuple = caql_form_tuple(pcqCtx, values, nulls);

	/* insert a new tuple */
	caql_insert(pcqCtx, pg_exttable_tuple); 
	/* and Update indexes (implicit) */

	caql_endscan(pcqCtx);

	/*
     * Close the pg_exttable relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be held until
     * end of transaction.
     */
    heap_close(pg_exttable_rel, NoLock);

}

/*
 * Get the catalog entry for an exttable relation (from pg_exttable)
 */
ExtTableEntry*
GetExtTableEntry(Oid relid)
{
	ExtTableEntry *extentry;

	extentry = GetExtTableEntryIfExists(relid);
	if (!extentry)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("missing pg_exttable entry for relation \"%s\"",
						get_rel_name(relid))));
	return extentry;
}

/*
 * Like GetExtTableEntry(Oid), but returns NULL instead of throwing
 * an error if no pg_exttable entry is found.
 */
ExtTableEntry*
GetExtTableEntryIfExists(Oid relid)
{
	Relation	pg_exttable_rel;
	HeapTuple	tuple;
	cqContext	cqc;
	ExtTableEntry *extentry;
	Datum		locations,
				fmtcode, 
				fmtopts, 
				command, 
				rejectlimit, 
				rejectlimittype, 
				fmterrtbl, 
				encoding, 
				iswritable;
	bool		isNull;
	bool		locationNull = false;

	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	tuple = caql_getfirst(
			caql_addrel(cqclr(&cqc), pg_exttable_rel),			
			cql("SELECT * FROM pg_exttable "
				" WHERE reloid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(relid)));

	if (!HeapTupleIsValid(tuple))
	{
		heap_close(pg_exttable_rel, RowExclusiveLock);
		return NULL;
	}

	extentry = (ExtTableEntry *) palloc0(sizeof(ExtTableEntry));

	/* get the location list */
	locations = heap_getattr(tuple, 
							 Anum_pg_exttable_location, 
							 RelationGetDescr(pg_exttable_rel), 
							 &isNull);

	if (isNull)
	{
		Insist(false); /* location list is always populated (url or ON X) */
	}
	else
	{
		Datum	   *elems;
		int			nelems;
		int			i;
		char*		loc_str = NULL;
		
		deconstruct_array(DatumGetArrayTypeP(locations),
						  TEXTOID, -1, false, 'i',
						  &elems, NULL, &nelems);

		for (i = 0; i < nelems; i++)
		{
			loc_str = DatumGetCString(DirectFunctionCall1(textout, elems[i]));

			/* append to a list of Value nodes, size nelems */
			extentry->locations = lappend(extentry->locations, makeString(pstrdup(loc_str)));
		}
		
		if(loc_str && (IS_FILE_URI(loc_str) || IS_GPFDIST_URI(loc_str) || IS_GPFDISTS_URI(loc_str)))
			extentry->isweb = false;
		else
			extentry->isweb = true;

	}
		
	/* get the execute command */
	command = heap_getattr(tuple, 
						   Anum_pg_exttable_command, 
						   RelationGetDescr(pg_exttable_rel), 
						   &isNull);
	
	if(isNull)
	{
		if(locationNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("got invalid pg_exttable tuple. location and command are both NULL")));	
		
		extentry->command = NULL;
	}
	else
	{
		extentry->command = DatumGetCString(DirectFunctionCall1(textout, command));
	}
	

	/* get the format code */
	fmtcode = heap_getattr(tuple, 
						   Anum_pg_exttable_fmttype, 
						   RelationGetDescr(pg_exttable_rel), 
						   &isNull);
	
	Insist(!isNull);
	extentry->fmtcode = DatumGetChar(fmtcode);
	Insist(extentry->fmtcode == 'c' || extentry->fmtcode == 't' 
		 || extentry->fmtcode == 'b' || extentry->fmtcode == 'a'
		 || extentry->fmtcode == 'p');

	/* get the format options string */
	fmtopts = heap_getattr(tuple, 
						   Anum_pg_exttable_fmtopts, 
						   RelationGetDescr(pg_exttable_rel), 
						   &isNull);
	
	Insist(!isNull);
	extentry->fmtopts = DatumGetCString(DirectFunctionCall1(textout, fmtopts));
	


	/* get the reject limit */
	rejectlimit = heap_getattr(tuple, 
							   Anum_pg_exttable_rejectlimit, 
							   RelationGetDescr(pg_exttable_rel), 
							   &isNull);
	
	if(!isNull)
		extentry->rejectlimit = DatumGetInt32(rejectlimit);
	else
		extentry->rejectlimit = -1; /* mark that no SREH requested */

	/* get the reject limit type */
	rejectlimittype = heap_getattr(tuple, 
								   Anum_pg_exttable_rejectlimittype, 
								   RelationGetDescr(pg_exttable_rel), 
								   &isNull);
	
	extentry->rejectlimittype = DatumGetChar(rejectlimittype);
	if(!isNull)
		Insist(extentry->rejectlimittype == 'r' || extentry->rejectlimittype == 'p');
	else
		extentry->rejectlimittype = -1;
	
	/* get the error table oid */
	fmterrtbl = heap_getattr(tuple, 
							 Anum_pg_exttable_fmterrtbl, 
							 RelationGetDescr(pg_exttable_rel), 
							 &isNull);
    
	if(isNull)
		extentry->fmterrtbl = InvalidOid;
	else
		extentry->fmterrtbl = DatumGetObjectId(fmterrtbl);

	/* get the table encoding */
	encoding = heap_getattr(tuple, 
							Anum_pg_exttable_encoding, 
							RelationGetDescr(pg_exttable_rel), 
							&isNull);
	
	Insist(!isNull);
	extentry->encoding = DatumGetInt32(encoding);
	Insist(PG_VALID_ENCODING(extentry->encoding));

	/* get the table encoding */
	iswritable = heap_getattr(tuple, 
							  Anum_pg_exttable_writable, 
							  RelationGetDescr(pg_exttable_rel), 
							  &isNull);
	Insist(!isNull);
	extentry->iswritable = DatumGetBool(iswritable);

	
	/* Finish up scan and close pg_exttable catalog. */
	
	heap_close(pg_exttable_rel, RowExclusiveLock);
	
	return extentry;
}

/*
 * RemoveExtTableEntry
 * 
 * Remove an external table entry from pg_exttable. Caller's 
 * responsibility to ensure that the relation has such an entry.
 */
void
RemoveExtTableEntry(Oid relid)
{
	Relation	pg_exttable_rel;
	cqContext	cqc;

	/*
	 * now remove the pg_exttable entry
	 */
	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	if (0 == caql_getcount(
				caql_addrel(cqclr(&cqc), pg_exttable_rel),
				cql("DELETE FROM pg_exttable "
					" WHERE reloid = :1 ",
					ObjectIdGetDatum(relid))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("external table object id \"%d\" does not exist",
						relid)));
	}

	/*
	 * Delete the external table entry from the catalog (pg_exttable).
	 */

	/* Finish up scan and close exttable catalog. */

	heap_close(pg_exttable_rel, NoLock);
}
