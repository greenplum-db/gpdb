/*-------------------------------------------------------------------------
 *
 * pg_exttable.c
 *	  routines to support manipulation of the pg_exttable relation
 *
 * Portions Copyright (c) 2009, Greenplum Inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2006, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/pg_exttable.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_exttable.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/reloptions.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "mb/pg_wchar.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/uri.h"
#include "miscadmin.h"

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
					Datum   optionsStr,
					Datum	locationExec,
					Datum	locationUris)
{
	Relation	pg_exttable_rel;
	HeapTuple	pg_exttable_tuple = NULL;
	bool		nulls[Natts_pg_exttable];
	Datum		values[Natts_pg_exttable];

	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));

	/*
	 * Open and lock the pg_exttable catalog.
	 */
	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	values[Anum_pg_exttable_reloid - 1] = ObjectIdGetDatum(tbloid);
	values[Anum_pg_exttable_fmttype - 1] = CharGetDatum(formattype);
	values[Anum_pg_exttable_fmtopts - 1] = formatOptStr;
	values[Anum_pg_exttable_options - 1] = optionsStr;

	if(commandString)
	{
		/* EXECUTE type table - store command and command location */

		values[Anum_pg_exttable_command - 1] =
			CStringGetTextDatum(commandString);
		values[Anum_pg_exttable_execlocation - 1] = locationExec;
		nulls[Anum_pg_exttable_urilocation - 1] = true;
	}
	else
	{
		/* LOCATION type table - store uri locations. command is NULL */

		values[Anum_pg_exttable_execlocation - 1] = locationExec;
		if (!locationExec)
			nulls[Anum_pg_exttable_execlocation - 1] = true;
		values[Anum_pg_exttable_urilocation - 1] = locationUris;
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

	pg_exttable_tuple = heap_form_tuple(RelationGetDescr(pg_exttable_rel), values, nulls);

	/* insert a new tuple */
	simple_heap_insert(pg_exttable_rel, pg_exttable_tuple);

	CatalogUpdateIndexes(pg_exttable_rel, pg_exttable_tuple);

	/*
     * Close the pg_exttable relcache entry without unlocking.
     * We have updated the catalog: consequently the lock must be held until
     * end of transaction.
     */
    heap_close(pg_exttable_rel, NoLock);

	/*
	 * Add the dependency of custom external table
	 */

	if (locationUris != (Datum) 0)
	{
		Datum	   *elems;
		int			nelems;

		deconstruct_array(DatumGetArrayTypeP(locationUris),
						  TEXTOID, -1, false, 'i',
						  &elems, NULL, &nelems);


		for (int i = 0; i < nelems; i++)
		{
			ObjectAddress	myself, referenced;
			char	   *location;
			char	   *protocol;
			Size		position;

			location = TextDatumGetCString(elems[i]);
			position = strchr(location, ':') - location;
			protocol = pnstrdup(location, position);

			myself.classId = RelationRelationId;
			myself.objectId = tbloid;
			myself.objectSubId = 0;

			referenced.classId = ExtprotocolRelationId;
			referenced.objectId = LookupExtProtocolOid(protocol, true);
			referenced.objectSubId = 0;

			/*
			 * Only tables with custom protocol should create dependency, for
			 * internal protocols will get referenced.objectId as 0.
			 */
			if (referenced.objectId)
				recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
		}

	}
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
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tuple;
	ExtTableEntry *extentry;
	Datum		urilocations,
				execlocations,
				fmtcode,
				fmtopts,
				options,
				command,
				rejectlimit,
				rejectlimittype,
				fmterrtbl,
				encoding,
				iswritable;
	bool		isNull;
	bool		locationNull = false;

	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	ScanKeyInit(&skey,
				Anum_pg_exttable_reloid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_exttable_rel, ExtTableReloidIndexId, true,
							  SnapshotNow, 1, &skey);
	tuple = systable_getnext(scan);

	if (!HeapTupleIsValid(tuple))
	{
		systable_endscan(scan);
		heap_close(pg_exttable_rel, RowExclusiveLock);
		return NULL;
	}

	extentry = (ExtTableEntry *) palloc0(sizeof(ExtTableEntry));

	/* get the location list */
	urilocations = heap_getattr(tuple,
							 Anum_pg_exttable_urilocation,
							 RelationGetDescr(pg_exttable_rel),
							 &isNull);
	if (DatumGetPointer(urilocations) == 0)
		urilocations = PointerGetDatum(construct_empty_array(TEXTOID));

	execlocations = heap_getattr(tuple,
							 Anum_pg_exttable_execlocation,
							 RelationGetDescr(pg_exttable_rel),
							 &isNull);

	if (DatumGetPointer(execlocations) == 0)
		execlocations = PointerGetDatum(construct_empty_array(TEXTOID));

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

		deconstruct_array(DatumGetArrayTypeP(urilocations),
						  TEXTOID, -1, false, 'i',
						  &elems, NULL, &nelems);

		for (i = 0; i < nelems; i++)
		{
			loc_str = TextDatumGetCString(elems[i]);

			/* append to a list of Value nodes, size nelems */
			extentry->urilocations = lappend(extentry->urilocations, makeString(pstrdup(loc_str)));
		}

		if(loc_str && (IS_FILE_URI(loc_str) || IS_GPFDIST_URI(loc_str) || IS_GPFDISTS_URI(loc_str)))
			extentry->isweb = false;
		else
			extentry->isweb = true;

		deconstruct_array(DatumGetArrayTypeP(execlocations),
						  TEXTOID, -1, false, 'i',
						  &elems, NULL, &nelems);

		for (i = 0; i < nelems; i++)
		{
			loc_str = TextDatumGetCString(elems[i]);

			/* append to a list of Value nodes, size nelems */
			extentry->execlocations = lappend(extentry->execlocations, makeString(pstrdup(loc_str)));
		}

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
		extentry->command = TextDatumGetCString(command);
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
	extentry->fmtopts = TextDatumGetCString(fmtopts);

    /* get the external table options string */
    options = heap_getattr(tuple,
                           Anum_pg_exttable_options,
                           RelationGetDescr(pg_exttable_rel),
                           &isNull);

	if (isNull)
	{
		/* options list is always populated (url or ON X) */
		elog(ERROR, "could not find options for external protocol");
	}
	else
	{
		Datum	   *elems;
		int			nelems;
		int			i;
		char	   *option_str;

		deconstruct_array(DatumGetArrayTypeP(options),
						  TEXTOID, -1, false, 'i',
						  &elems, NULL, &nelems);

		for (i = 0; i < nelems; i++)
		{
			option_str = TextDatumGetCString(elems[i]);

			/* append to a list of Value nodes, size nelems */
			extentry->options = lappend(extentry->options, makeString(option_str));
		}
	}

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
	systable_endscan(scan);
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
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tuple;

	/*
	 * now remove the pg_exttable entry
	 */
	pg_exttable_rel = heap_open(ExtTableRelationId, RowExclusiveLock);

	ScanKeyInit(&skey,
				Anum_pg_exttable_reloid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));

	scan = systable_beginscan(pg_exttable_rel, ExtTableReloidIndexId, true,
							  SnapshotNow, 1, &skey);
	tuple = systable_getnext(scan);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("external table object id \"%d\" does not exist",
						relid)));

	/*
	 * Delete the external table entry from the catalog (pg_exttable).
	 */
	do
	{
		simple_heap_delete(pg_exttable_rel, &tuple->t_self);
	}
	while (HeapTupleIsValid(tuple = systable_getnext(scan)));

	/* Finish up scan and close exttable catalog. */
	systable_endscan(scan);
	heap_close(pg_exttable_rel, NoLock);
}
