/*-------------------------------------------------------------------------
 *
 * pg_namespace.c
 *	  routines to support manipulation of the pg_namespace relation
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/pg_namespace.c,v 1.18 2008/01/01 19:45:48 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_namespace.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"


/* ----------------
 * NamespaceCreate
 * ---------------
 */
Oid
NamespaceCreate(const char *nspName, Oid ownerId, Oid forceOid)
{
	Relation	nspdesc;
	HeapTuple	tup;
	Oid			nspoid;
	bool		nulls[Natts_pg_namespace];
	Datum		values[Natts_pg_namespace];
	NameData	nname;
	int			i;
	cqContext	cqc;
	cqContext	cqc2;
	cqContext  *pcqCtx;

	/* sanity checks */
	if (!nspName)
		elog(ERROR, "no namespace name supplied");

	nspdesc = heap_open(NamespaceRelationId, RowExclusiveLock);

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), nspdesc),
			cql("INSERT INTO pg_namespace",
				NULL));

	/* make sure there is no existing namespace of same name */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc2), nspdesc),
				cql("SELECT COUNT(*) FROM pg_namespace "
					" WHERE nspname = :1 ",
					CStringGetDatum((char *) nspName))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_SCHEMA),
				 errmsg("schema \"%s\" already exists", nspName)));
	}

	/* initialize nulls and values */
	for (i = 0; i < Natts_pg_namespace; i++)
	{
		nulls[i] = false;
		values[i] = (Datum) 0;
	}
	namestrcpy(&nname, nspName);
	values[Anum_pg_namespace_nspname - 1] = NameGetDatum(&nname);
	values[Anum_pg_namespace_nspowner - 1] = ObjectIdGetDatum(ownerId);
	nulls[Anum_pg_namespace_nspacl - 1] = true;

	tup = caql_form_tuple(pcqCtx, values, nulls);
	
	if (forceOid != InvalidOid)
		HeapTupleSetOid(tup, forceOid);		/* override heap_insert's OID
											 * selection */

	/* insert a new tuple */
	nspoid = caql_insert(pcqCtx, tup); /* implicit update of index as well */
	Assert(OidIsValid(nspoid));

	caql_endscan(pcqCtx);
	heap_close(nspdesc, RowExclusiveLock);

	/* Record dependency on owner */
	recordDependencyOnOwner(NamespaceRelationId, nspoid, ownerId);

	return nspoid;
}
