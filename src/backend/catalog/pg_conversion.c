/*-------------------------------------------------------------------------
 *
 * pg_conversion.c
 *	  routines to support manipulation of the pg_conversion relation
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/pg_conversion.c,v 1.40 2008/01/01 19:45:48 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/sysattr.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/acl.h"
#include "miscadmin.h"

/*
 * ConversionCreate
 *
 * Add a new tuple to pg_conversion.
 */
Oid
ConversionCreate(const char *conname, Oid connamespace,
				 Oid conowner,
				 int32 conforencoding, int32 contoencoding,
				 Oid conproc, bool def, Oid newOid)
{
	int			i;
	Relation	rel;
	HeapTuple	tup;
	bool		nulls[Natts_pg_conversion];
	Datum		values[Natts_pg_conversion];
	NameData	cname;
	Oid			oid;
	ObjectAddress myself,
				referenced;
	cqContext	cqc;
	cqContext  *pcqCtx;

	/* sanity checks */
	if (!conname)
		elog(ERROR, "no conversion name supplied");

	/* open pg_conversion */
	rel = heap_open(ConversionRelationId, RowExclusiveLock);

	/* make sure there is no existing conversion of same name */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc), rel),
				cql("SELECT COUNT(*) FROM pg_conversion "
					" WHERE conname = :1 "
					" AND connamespace = :2 ",
					CStringGetDatum((char *) conname),
					ObjectIdGetDatum(connamespace))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("conversion \"%s\" already exists", conname)));
	}

	if (def)
	{
		/*
		 * make sure there is no existing default <for encoding><to encoding>
		 * pair in this name space
		 */
		if (FindDefaultConversion(connamespace,
								  conforencoding,
								  contoencoding))
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("default conversion for %s to %s already exists",
							pg_encoding_to_char(conforencoding),
							pg_encoding_to_char(contoencoding))));
	}

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), rel),
			cql("INSERT INTO pg_conversion",
				NULL));

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_conversion; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) 0;
	}

	/* form a tuple */
	namestrcpy(&cname, conname);
	values[Anum_pg_conversion_conname - 1] = NameGetDatum(&cname);
	values[Anum_pg_conversion_connamespace - 1] = ObjectIdGetDatum(connamespace);
	values[Anum_pg_conversion_conowner - 1] = ObjectIdGetDatum(conowner);
	values[Anum_pg_conversion_conforencoding - 1] = Int32GetDatum(conforencoding);
	values[Anum_pg_conversion_contoencoding - 1] = Int32GetDatum(contoencoding);
	values[Anum_pg_conversion_conproc - 1] = ObjectIdGetDatum(conproc);
	values[Anum_pg_conversion_condefault - 1] = BoolGetDatum(def);

	tup = caql_form_tuple(pcqCtx, values, nulls);
	
	if (newOid != 0)
		HeapTupleSetOid(tup, newOid);

	/* insert a new tuple */
	oid = caql_insert(pcqCtx, tup); /* implicit update of index as well */
	Assert(OidIsValid(oid));

	myself.classId = ConversionRelationId;
	myself.objectId = HeapTupleGetOid(tup);
	myself.objectSubId = 0;

	/* create dependency on conversion procedure */
	referenced.classId = ProcedureRelationId;
	referenced.objectId = conproc;
	referenced.objectSubId = 0;
	recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

	/* create dependency on namespace */
	referenced.classId = NamespaceRelationId;
	referenced.objectId = connamespace;
	referenced.objectSubId = 0;
	recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

	/* create dependency on owner */
	recordDependencyOnOwner(ConversionRelationId, HeapTupleGetOid(tup),
							conowner);

	heap_freetuple(tup);
	caql_endscan(pcqCtx);
	heap_close(rel, RowExclusiveLock);

	return oid;
}

/*
 * ConversionDrop
 *
 * Drop a conversion after doing permission checks.
 */
void
ConversionDrop(Oid conversionOid, DropBehavior behavior)
{
	HeapTuple	tuple;
	ObjectAddress object;

	tuple = SearchSysCache(CONVOID,
						   ObjectIdGetDatum(conversionOid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for conversion %u", conversionOid);

	if (!superuser() &&
		((Form_pg_conversion) GETSTRUCT(tuple))->conowner != GetUserId())
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CONVERSION,
				  NameStr(((Form_pg_conversion) GETSTRUCT(tuple))->conname));

	ReleaseSysCache(tuple);

	/*
	 * Do the deletion
	 */
	object.classId = ConversionRelationId;
	object.objectId = conversionOid;
	object.objectSubId = 0;

	performDeletion(&object, behavior);
}

/*
 * RemoveConversionById
 *
 * Remove a tuple from pg_conversion by Oid. This function is solely
 * called inside catalog/dependency.c
 */
void
RemoveConversionById(Oid conversionOid)
{

	if (0 ==
		caql_getcount(
				NULL,
				cql("DELETE FROM pg_conversion "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(conversionOid))))
	{
		elog(ERROR, "could not find tuple for conversion %u", conversionOid);
	}
}

/*
 * FindDefaultConversion
 *
 * Find "default" conversion proc by for_encoding and to_encoding in the
 * given namespace.
 *
 * If found, returns the procedure's oid, otherwise InvalidOid.  Note that
 * you get the procedure's OID not the conversion's OID!
 */
Oid
FindDefaultConversion(Oid name_space, int32 for_encoding, int32 to_encoding)
{
	CatCList   *catlist;
	HeapTuple	tuple;
	Form_pg_conversion body;
	Oid			proc = InvalidOid;
	int			i;

	catlist = caql_begin_CacheList(
			NULL, 
			cql("SELECT * FROM pg_conversion "
				" WHERE connamespace = :1 "
				" AND conforencoding = :2 "
				" AND contoencoding = :3 "
				" ORDER BY connamespace, "
				"  conforencoding,  "
				"  contoencoding,  "
				"  oid  ",
				ObjectIdGetDatum(name_space),
				Int32GetDatum(for_encoding),
				Int32GetDatum(to_encoding)));

	for (i = 0; i < catlist->n_members; i++)
	{
		tuple = &catlist->members[i]->tuple;
		body = (Form_pg_conversion) GETSTRUCT(tuple);
		if (body->condefault)
		{
			proc = body->conproc;
			break;
		}
	}
	caql_end_CacheList(catlist);
	return proc;
}

/*
 * FindConversion
 *
 * Find conversion by namespace and conversion name.
 * Returns conversion OID.
 */
Oid
FindConversion(const char *conname, Oid connamespace)
{
	HeapTuple	tuple;
	Oid			procoid;
	Oid			conoid;
	AclResult	aclresult;
	cqContext  *pcqCtx;

	/* search pg_conversion by connamespace and conversion name */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_conversion "
				" WHERE conname = :1 "
				" AND connamespace = :2 ",
				CStringGetDatum((char *) conname),
				ObjectIdGetDatum(connamespace)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		return InvalidOid;

	procoid = ((Form_pg_conversion) GETSTRUCT(tuple))->conproc;
	conoid = HeapTupleGetOid(tuple);

	caql_endscan(pcqCtx);

	/* Check we have execute rights for the function */
	aclresult = pg_proc_aclcheck(procoid, GetUserId(), ACL_EXECUTE);
	if (aclresult != ACLCHECK_OK)
		return InvalidOid;

	return conoid;
}
