/*-------------------------------------------------------------------------
 *
 * pg_attribute_encoding.c
 *	  Routines to manipulation and retrieve column encoding information.
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/pg_attribute_encoding.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_compression.h"
#include "catalog/dependency.h"
#include "fmgr.h"
#include "parser/analyze.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/formatting.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

/*
 * Add a single attribute encoding entry.
 */
static void
add_attribute_encoding_entry(Oid relid, AttrNumber attnum, Datum attoptions)
{
	Relation	rel;
	Datum values[Natts_pg_attribute_encoding];
	bool nulls[Natts_pg_attribute_encoding];
	HeapTuple tuple;
	
	Insist(attnum != InvalidAttrNumber);

	rel = heap_open(AttributeEncodingRelationId, RowExclusiveLock);

	MemSet(nulls, 0, sizeof(nulls));
	values[Anum_pg_attribute_encoding_attrelid - 1] = ObjectIdGetDatum(relid);
	values[Anum_pg_attribute_encoding_attnum - 1] = Int16GetDatum(attnum);
	values[Anum_pg_attribute_encoding_attoptions - 1] = attoptions;

	tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);

	/* insert a new tuple */
	simple_heap_insert(rel, tuple);
	CatalogUpdateIndexes(rel, tuple);

	heap_freetuple(tuple);

	heap_close(rel, RowExclusiveLock);
}

/*
 * Get the set of functions implementing a compression algorithm.
 *
 * Intercept requests for "none", since that is not a real compression
 * implementation but a fake one to indicate no compression desired.
 */
PGFunction *
get_funcs_for_compression(char *compresstype)
{
	PGFunction *func = NULL;

	if (compresstype == NULL ||
		compresstype[0] == '\0' ||
		pg_strcasecmp("none", compresstype) == 0)
	{
		return func;
	}
	else
	{
		func = GetCompressionImplementation(compresstype);

		Insist(PointerIsValid(func));
	}
	return func;
}

/*
 * Get datum representations of the attoptions field in pg_attribute_encoding
 * for the given relation.
 */
Datum *
get_rel_attoptions(Oid relid, AttrNumber max_attno)
{
	Form_pg_attribute attform;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple		tuple;
	Datum		   *dats;
	Relation 		pgae = heap_open(AttributeEncodingRelationId,
									 AccessShareLock);

	/* used for attbyval and len below */
	attform = pgae->rd_att->attrs[Anum_pg_attribute_encoding_attoptions - 1];

	dats = palloc0(max_attno * sizeof(Datum));

	ScanKeyInit(&skey,
				Anum_pg_attribute_encoding_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	scan = systable_beginscan(pgae, AttributeEncodingAttrelidIndexId, true,
							  NULL, 1, &skey);

	while (HeapTupleIsValid(tuple = systable_getnext(scan)))
	{
		Form_pg_attribute_encoding a = 
			(Form_pg_attribute_encoding)GETSTRUCT(tuple);
		int16 attnum = a->attnum;
		Datum attoptions;
		bool isnull;

		Insist(attnum > 0 && attnum <= max_attno);

		attoptions = heap_getattr(tuple, Anum_pg_attribute_encoding_attoptions,
								  RelationGetDescr(pgae), &isnull);
		Insist(!isnull);

		dats[attnum - 1] = datumCopy(attoptions,
									 attform->attbyval,
									 attform->attlen);
	}

	systable_endscan(scan);

	heap_close(pgae, AccessShareLock);

	return dats;

}

/*
 * Add pg_attribute_encoding entries for newrelid. Make them identical to those
 * stored for oldrelid.
 */
void
cloneAttributeEncoding(Oid oldrelid, Oid newrelid, AttrNumber max_attno)
{
	Datum *attoptions = get_rel_attoptions(oldrelid, max_attno);
	AttrNumber n;

	for (n = 0; n < max_attno; n++)
	{
		if (DatumGetPointer(attoptions[n]) != NULL)
			add_attribute_encoding_entry(newrelid,
										 n + 1,
										 attoptions[n]);
	}
	CommandCounterIncrement();
}

List **
RelationGetUntransformedAttributeOptions(Relation rel)
{
	List **l;
	int i;
	Datum *dats = get_rel_attoptions(RelationGetRelid(rel),
									 RelationGetNumberOfAttributes(rel));

	l = palloc0(RelationGetNumberOfAttributes(rel) * sizeof(List *));

	for (i = 0; i < RelationGetNumberOfAttributes(rel); i++)
	{
		l[i] = untransformRelOptions(dats[i]);
	}

	return l;
}

/*
 * Get all storage options for all user attributes of the table.
 */
StdRdOptions **
RelationGetAttributeOptions(Relation rel)
{
	Datum *dats;
	StdRdOptions **opts;
	int i;

	opts = palloc0(RelationGetNumberOfAttributes(rel) * sizeof(StdRdOptions *));

	dats = get_rel_attoptions(RelationGetRelid(rel),
							  RelationGetNumberOfAttributes(rel));

	for (i = 0; i < RelationGetNumberOfAttributes(rel); i++)
	{
		if (DatumGetPointer(dats[i]) != NULL)
		{
			opts[i] = (StdRdOptions *)heap_reloptions(
					RELKIND_RELATION, dats[i], false);
			pfree(DatumGetPointer(dats[i]));
		}
	}
	pfree(dats);

	return opts;
}

/*
 * Work horse underneath DefineRelation().
 *
 * Simply adds user specified ENCODING () clause information to
 * pg_attribute_encoding. Should be absolutely valid at this point.
 */
void
AddRelationAttributeEncodings(Relation rel, List *attr_encodings)
{
	Oid relid = RelationGetRelid(rel);
	ListCell *lc;

	foreach(lc, attr_encodings)
	{
		Datum attoptions;
		ColumnReferenceStorageDirective *c = lfirst(lc);
		List *encoding;
		AttrNumber attnum;

		Insist(IsA(c, ColumnReferenceStorageDirective));

		attnum = get_attnum(relid, c->column);

		if (attnum == InvalidAttrNumber)
			elog(ERROR, "column \"%s\" does not exist", c->column);

		if (attnum < 0)
			elog(ERROR, "column \"%s\" is a system column", c->column);

		encoding = c->encoding;

		if (!encoding)
			continue;

		attoptions = transformRelOptions(PointerGetDatum(NULL),
										 encoding,
										 NULL,
										 NULL,
										 true,
										 false);

		add_attribute_encoding_entry(relid, attnum, attoptions);
	}
}

void
RemoveAttributeEncodingsByRelid(Oid relid)
{
	Relation	rel;
	ScanKeyData skey;
	SysScanDesc scan;
	HeapTuple	tup;

	rel = heap_open(AttributeEncodingRelationId, RowExclusiveLock);

	ScanKeyInit(&skey,
				Anum_pg_attribute_encoding_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	scan = systable_beginscan(rel, AttributeEncodingAttrelidIndexId, true,
							  NULL, 1, &skey);
	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		simple_heap_delete(rel, &tup->t_self);
	}

	systable_endscan(scan);

	heap_close(rel, RowExclusiveLock);
}
