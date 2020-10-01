#include "postgres.h"

#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"

#include "access/aocssegfiles.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "storage/bufmgr.h"
#include "utils/numeric.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

/* numeric upgrade tests */
extern Datum setAOFormatVersion(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(flush_relation_buffers);
Datum
flush_relation_buffers(PG_FUNCTION_ARGS)
{
	Oid relid = PG_GETARG_OID(0);
	Relation r = table_open(relid, AccessShareLock);

	FlushRelationBuffers(r);
	table_close(r, AccessShareLock);
	PG_RETURN_BOOL(true);
}

/* Override the format version for an AO/CO table. */
PG_FUNCTION_INFO_V1(setAOFormatVersion);
Datum
setAOFormatVersion(PG_FUNCTION_ARGS)
{
	Oid				aosegrelid = PG_GETARG_OID(0);
	int16			formatversion = PG_GETARG_INT16(1);
	bool			columnoriented = PG_GETARG_BOOL(2);
	Relation		aosegrel;
	SysScanDesc		scan;
	HeapTuple		oldtuple;
	HeapTuple		newtuple;
	TupleDesc		tupdesc;
	Datum		   *values;
	bool		   *isnull;
	bool		   *replace;
	int				natts;
	int				formatversion_attnum;

	/*
	 * The segment descriptor's rowtype is different for row- and
	 * column-oriented tables.
	 */
	natts = columnoriented ? Natts_pg_aocsseg : Natts_pg_aoseg;
	formatversion_attnum = columnoriented ? Anum_pg_aocs_formatversion :
											Anum_pg_aoseg_formatversion;

	/* Create our replacement attribute. */
	values = palloc(sizeof(Datum) * natts);
	isnull = palloc0(sizeof(bool) * natts);
	replace = palloc0(sizeof(bool) * natts);

	values[formatversion_attnum - 1] = Int16GetDatum(formatversion);
	replace[formatversion_attnum - 1] = true;

	/* Open the segment descriptor table. */
	aosegrel = table_open(aosegrelid, RowExclusiveLock);

	if (!RelationIsValid(aosegrel))
		elog(ERROR, "could not open aoseg table with OID %d", (int) aosegrelid);

	tupdesc = RelationGetDescr(aosegrel);

	/* Try to sanity-check a little bit... */
	if (tupdesc->natts != natts)
		elog(ERROR, "table with OID %d does not appear to be an aoseg table",
			 (int) aosegrelid);

	/* Scan over the rows, overriding the formatversion for each entry. */
	scan = systable_beginscan(aosegrel, InvalidOid, false,
							  NULL, 0, NULL);
	while (HeapTupleIsValid((oldtuple = systable_getnext(scan))))
	{
		newtuple = heap_modify_tuple(oldtuple, tupdesc, values, isnull, replace);
		CatalogTupleUpdate(aosegrel, &oldtuple->t_self, newtuple);
		pfree(newtuple);
	}
	systable_endscan(scan);

	/* Done. Clean up. */
	heap_close(aosegrel, RowExclusiveLock);

	pfree(replace);
	pfree(isnull);
	pfree(values);

	PG_RETURN_BOOL(true);
}
