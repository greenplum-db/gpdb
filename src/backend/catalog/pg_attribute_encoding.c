/*-------------------------------------------------------------------------
 *
 * pg_attribute_encoding.c
 *	  Routines to manipulation and retrieve column encoding information.
 *
 * Portions Copyright (c) EMC, 2011
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

/*
 * Add a single attribute encoding entry.
 */
static void
add_attribute_encoding_entry(Oid relid, AttrNumber attnum, FileNumber filenum, Datum attoptions)
{
	Relation	rel;
	Datum values[Natts_pg_attribute_encoding];
	bool nulls[Natts_pg_attribute_encoding];
	HeapTuple tuple;

	Assert(attnum != InvalidAttrNumber);
	Assert(filenum != InvalidFileNumber);

	rel = heap_open(AttributeEncodingRelationId, RowExclusiveLock);

	MemSet(nulls, 0, sizeof(nulls));
	values[Anum_pg_attribute_encoding_attrelid - 1] = ObjectIdGetDatum(relid);
	values[Anum_pg_attribute_encoding_attnum - 1] = Int16GetDatum(attnum);
	values[Anum_pg_attribute_encoding_filenum - 1] = Int16GetDatum(filenum);
	values[Anum_pg_attribute_encoding_attoptions - 1] = attoptions;

	tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);

	/* insert a new tuple */
	CatalogTupleInsert(rel, tuple);

	heap_freetuple(tuple);

	heap_close(rel, RowExclusiveLock);
}

static void
update_attribute_encoding_entry(Oid relid, AttrNumber attnum, Datum newattoptions)
{
	Relation 	rel;
	SysScanDesc scan;
	ScanKeyData skey[2];
	HeapTuple	oldtup;
	HeapTuple   newtup;
	Datum	    values[Natts_pg_attribute_encoding];
	bool	    nulls[Natts_pg_attribute_encoding];
	bool		repl[Natts_pg_attribute_encoding];

	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));
	MemSet(repl, false, sizeof(repl));

	rel = heap_open(AttributeEncodingRelationId, RowExclusiveLock);

	ScanKeyInit(&skey[0],
				Anum_pg_attribute_encoding_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&skey[1],
				Anum_pg_attribute_encoding_attnum,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(attnum));
	scan = systable_beginscan(rel, AttributeEncodingAttrelidAttnumIndexId, true,
							  NULL, 2, skey);

	oldtup = systable_getnext(scan);
	Assert(HeapTupleIsValid(oldtup));

	heap_deform_tuple(oldtup, RelationGetDescr(rel), values, nulls);

	values[Anum_pg_attribute_encoding_attoptions - 1] = newattoptions;
	repl[Anum_pg_attribute_encoding_attoptions - 1] = true;

	newtup = heap_modify_tuple(oldtup, RelationGetDescr(rel), values, nulls, repl);

	CatalogTupleUpdate(rel, &oldtup->t_self, newtup);
	heap_freetuple(newtup);

	systable_endscan(scan);
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

		Assert(PointerIsValid(func));
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
	attform = TupleDescAttr(pgae->rd_att, Anum_pg_attribute_encoding_attoptions - 1);

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

		Assert(attnum > 0 && attnum <= max_attno);

		attoptions = heap_getattr(tuple, Anum_pg_attribute_encoding_attoptions,
								  RelationGetDescr(pgae), &isnull);
		Assert(!isnull);

		dats[attnum - 1] = datumCopy(attoptions,
									 attform->attbyval,
									 attform->attlen);
	}

	systable_endscan(scan);

	heap_close(pgae, AccessShareLock);

	return dats;

}

/*
 * Given a relation, get all column encodings for that relation as a list of
 * ColumnReferenceStorageDirective structures.
 */
List *
rel_get_column_encodings(Relation rel)
{
	List **colencs = RelationGetUntransformedAttributeOptions(rel);
	List *out = NIL;

	if (colencs)
	{
		AttrNumber attno;

		for (attno = 0; attno < RelationGetNumberOfAttributes(rel); attno++)
		{
			if (colencs[attno] && !TupleDescAttr(rel->rd_att, attno)->attisdropped)
			{
				ColumnReferenceStorageDirective *d =
					makeNode(ColumnReferenceStorageDirective);
				d->column = pstrdup(NameStr(TupleDescAttr(rel->rd_att, attno)->attname));
				d->encoding = colencs[attno];
		
				out = lappend(out, d);
			}
		}
	}
	return out;
}

/*
 * Add pg_attribute_encoding entries for newrelid. Make them identical to those
 * stored for oldrelid.
 */
void
CloneAttributeEncodings(Oid oldrelid, Oid newrelid, AttrNumber max_attno)
{
	Datum *attoptions = get_rel_attoptions(oldrelid, max_attno);
	AttrNumber n;

	for (n = 0; n < max_attno; n++)
	{
		if (DatumGetPointer(attoptions[n]) != NULL)
			add_attribute_encoding_entry(newrelid,
										 n + 1,
										 n + 1,
										 attoptions[n]);
	}
}

void
UpdateAttributeEncodings(Oid relid, List *new_attr_encodings)
{
	ListCell *lc;
	foreach(lc, new_attr_encodings)
	{
		Datum                           newattoptions;
		ColumnReferenceStorageDirective *c = lfirst(lc);
		List                            *encoding;
		AttrNumber                      attnum;

		Assert(IsA(c, ColumnReferenceStorageDirective));

		attnum = get_attnum(relid, c->column);

		if (attnum == InvalidAttrNumber)
			elog(ERROR, "column \"%s\" does not exist", c->column);

		if (attnum < 0)
			elog(ERROR, "column \"%s\" is a system column", c->column);

		encoding = c->encoding;

		if (!encoding)
			continue;

		newattoptions = transformRelOptions(PointerGetDatum(NULL),
											encoding,
											NULL,
											NULL,
											true,
											false);

		update_attribute_encoding_entry(relid, attnum, newattoptions);
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
			opts[i] = (StdRdOptions *) default_reloptions(
					dats[i], false,
					RELOPT_KIND_APPENDOPTIMIZED);
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
 *
 * Note that we need to take dropped columns into consideration
 * as well so we cannot use get_attnum().
 */
void
AddRelationAttributeEncodings(Oid relid, List *attr_encodings)
{
	ListCell *lc;
	ListCell *lc_filenum;
	List *filenums = GetNextNAvailableFilenums(relid, attr_encodings->length);

	if (filenums->length != attr_encodings->length)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("filenums exhausted for relid %u", relid),
					errhint("recreate the table")));

	forboth(lc, attr_encodings, lc_filenum, filenums)
	{
		Datum attoptions;
		ColumnReferenceStorageDirective *c = lfirst(lc);
		List *encoding;
		AttrNumber attnum;
		HeapTuple	tuple;
		Form_pg_attribute att_tup;

		Assert(IsA(c, ColumnReferenceStorageDirective));

		tuple = SearchSysCache2(ATTNAME,
								ObjectIdGetDatum(relid),
								CStringGetDatum(c->column));

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "column \"%s\" does not exist", c->column);

		att_tup = (Form_pg_attribute) GETSTRUCT(tuple);
		attnum = att_tup->attnum;
		Assert(attnum != InvalidAttrNumber);

		ReleaseSysCache(tuple);

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

		add_attribute_encoding_entry(relid, attnum, lfirst_int(lc_filenum), attoptions);
	}
	list_free(filenums);
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

/*
 * Returns the filenum value for a relation/attnum entry in pg_attribute_encoding
 */
FileNumber
GetFilenumForAttribute(Oid relid, AttrNumber attnum)
{
	Relation    rel;
	SysScanDesc scan;
	ScanKeyData skey[2];
	HeapTuple	tup;
	FileNumber  filenum;
	bool        isnull;

	Assert(OidIsValid(relid));
	Assert(AttributeNumberIsValid(attnum));

	rel = heap_open(AttributeEncodingRelationId, AccessShareLock);

	ScanKeyInit(&skey[0],
				Anum_pg_attribute_encoding_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	ScanKeyInit(&skey[1],
				Anum_pg_attribute_encoding_attnum,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum(attnum));
	scan = systable_beginscan(rel, AttributeEncodingAttrelidAttnumIndexId, true,
							  NULL, 2, skey);

	tup = systable_getnext(scan);
	Assert(HeapTupleIsValid(tup));
	filenum = heap_getattr(tup, Anum_pg_attribute_encoding_filenum,
							  RelationGetDescr(rel), &isnull);
	Assert(!isnull);
	systable_endscan(scan);
	heap_close(rel, AccessShareLock);
	return filenum;
}

/*
 * Returns a sorted list of first n unused filenums in pg_attribute_encoding
 * for the relation
 * In the outside chance that filenums have been exhausted,
 * the list may contain < n unused filenums
 */
List *
GetNextNAvailableFilenums(Oid relid, int n)
{
	Relation    rel;
	SysScanDesc scan;
	ScanKeyData skey[1];
	HeapTuple	tup;
	bool        isnull;
	bool        used[MaxFileNumber];
	List        *newfilenums = NIL;

	Assert(OidIsValid(relid));

	MemSet(used, false, sizeof(used));
	rel = heap_open(AttributeEncodingRelationId, AccessShareLock);

	ScanKeyInit(&skey[0],
				Anum_pg_attribute_encoding_attrelid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(relid));
	scan = systable_beginscan(rel, AttributeEncodingAttrelidIndexId, true,
							  NULL, 1, skey);

	while (HeapTupleIsValid(tup = systable_getnext(scan)))
	{
		FileNumber usedfilenum = heap_getattr(tup, Anum_pg_attribute_encoding_filenum,
							   RelationGetDescr(rel), &isnull);
		Assert(!isnull);
		used[usedfilenum-1] = true;
	}

	systable_endscan(scan);
	heap_close(rel, AccessShareLock);

	for (int i = 0; i < MaxFileNumber; ++i)
	{
		if(!used[i])
		{
			newfilenums = lappend_int(newfilenums, i + 1);
			if (newfilenums->length == n)
				break;
		}
	}
	return newfilenums;
}
