/*-------------------------------------------------------------------------
 *
 * hiddencat.c
 *	  hidden catalog definition and related routines
 *
 * This "hidden" catalog is only to provide immutable builtin catalog data
 * without bumping up catalog version.  The hidden catalog content *must*
 * go into the actual catalog in the next major version.  The usage of
 * this approach should be limited, as the operation is very sensitive
 * and could cause data corruption easily if done wrongly.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/valid.h"
#include "catalog/catalog.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"

/*
 * The hidden tuples are pointing to nowhere on disk, but we need to
 * pretend as if it were on disk, to make it usable for hashing etc.
 * This is very very ugly, I know...
 */
#define ItemPointerSetHidden(pointer, posid) \
( \
	ItemPointerSetBlockNumber(pointer, InvalidBlockNumber), \
	ItemPointerSetOffsetNumber(pointer, posid) \
)

/*
 * Returns pre-defined hidden tuples for pg_proc.
 */
HeapTuple *
GetHiddenPgProcTuples(Relation pg_proc, int *len)
{
	HeapTuple	   *tuples;
	Datum			values[Natts_pg_proc];
	bool			nulls[Natts_pg_proc];
	MemoryContext	oldcontext;
	static HeapTuple *StaticPgProcTuples = NULL;
	static int StaticPgProcTupleLen = 0;

	if (StaticPgProcTuples != NULL)
	{
		*len = StaticPgProcTupleLen;
		return StaticPgProcTuples;
	}

#define N_PGPROC_TUPLES 2
	oldcontext = MemoryContextSwitchTo(CacheMemoryContext);
	tuples = palloc(sizeof(HeapTuple) * N_PGPROC_TUPLES);

	/*
	 * gp_read_error_log
	 *
	 * CREATE FUNCTION pg_catalog.gp_read_error_log(
	 *     text,
	 *     cmdtime OUT timestamptz,
	 *     relname OUT text,
	 *     filename OUT text,
	 *     linenum OUT int4,
	 *     bytenum OUT int4,
	 *     errmsg OUT text,
	 *     rawdata OUT text,
	 *     rawbytes OUT bytea
	 *   ) RETURNS SETOF record AS 'gp_read_error_log'
	 *   LANGUAGE internal VOLATILE STRICT <RUN ON SEGMENT>;
	 */
	{
		NameData		procname = {"gp_read_error_log"};
		Oid				proargtypes[] = {TEXTOID};
		ArrayType	   *array;
		Datum			allargtypes[9];
		Datum			proargmodes[9];
		Datum			proargnames[9];

		MemSet(nulls, false, sizeof(bool) * Natts_pg_proc);
		values[Anum_pg_proc_proname - 1] = NameGetDatum(&procname);
		values[Anum_pg_proc_pronamespace - 1] = ObjectIdGetDatum(PG_CATALOG_NAMESPACE);
		values[Anum_pg_proc_proowner - 1] = ObjectIdGetDatum(BOOTSTRAP_SUPERUSERID);
		values[Anum_pg_proc_prolang - 1] = ObjectIdGetDatum(INTERNALlanguageId);
		values[Anum_pg_proc_provariadic - 1] = InvalidOid;
		values[Anum_pg_proc_proisagg - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_prosecdef - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_proisstrict - 1] = BoolGetDatum(true);
		values[Anum_pg_proc_proretset - 1] = BoolGetDatum(true);
		values[Anum_pg_proc_provolatile - 1] = CharGetDatum('v');
		values[Anum_pg_proc_pronargs - 1] = Int16GetDatum(1);
		values[Anum_pg_proc_prorettype - 1] = ObjectIdGetDatum(RECORDOID);
		values[Anum_pg_proc_proiswin - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_proargtypes - 1] = PointerGetDatum(buildoidvector(proargtypes, 1));

		allargtypes[0] = TEXTOID;
		allargtypes[1] = TIMESTAMPTZOID;
		allargtypes[2] = TEXTOID;
		allargtypes[3] = TEXTOID;
		allargtypes[4] = INT4OID;
		allargtypes[5] = INT4OID;
		allargtypes[6] = TEXTOID;
		allargtypes[7] = TEXTOID;
		allargtypes[8] = BYTEAOID;
		array = construct_array(allargtypes, 9, OIDOID, 4, true, 'i');
		values[Anum_pg_proc_proallargtypes - 1] = PointerGetDatum(array);

		proargmodes[0] = CharGetDatum('i');
		proargmodes[1] = CharGetDatum('o');
		proargmodes[2] = CharGetDatum('o');
		proargmodes[3] = CharGetDatum('o');
		proargmodes[4] = CharGetDatum('o');
		proargmodes[5] = CharGetDatum('o');
		proargmodes[6] = CharGetDatum('o');
		proargmodes[7] = CharGetDatum('o');
		proargmodes[8] = CharGetDatum('o');
		array = construct_array(proargmodes, 9, CHAROID, 1, true, 'c');
		values[Anum_pg_proc_proargmodes - 1] = PointerGetDatum(array);

		proargnames[0] = CStringGetTextDatum("");
		proargnames[1] = CStringGetTextDatum("cmdtime");
		proargnames[2] = CStringGetTextDatum("relname");
		proargnames[3] = CStringGetTextDatum("filename");
		proargnames[4] = CStringGetTextDatum("linenum");
		proargnames[5] = CStringGetTextDatum("bytenum");
		proargnames[6] = CStringGetTextDatum("errmsg");
		proargnames[7] = CStringGetTextDatum("rawdata");
		proargnames[8] = CStringGetTextDatum("rawbytes");
		array = construct_array(proargnames, 9, TEXTOID, -1, false, 'i');
		values[Anum_pg_proc_proargnames - 1] = PointerGetDatum(array);
		values[Anum_pg_proc_prosrc - 1] = CStringGetTextDatum("gp_read_error_log");
		values[Anum_pg_proc_probin - 1] = (Datum) 0;
		nulls[Anum_pg_proc_probin - 1] = true;
		values[Anum_pg_proc_proacl - 1] = (Datum) 0;
		nulls[Anum_pg_proc_proacl - 1] = true;
		/* special data access property, "segment" */
		values[Anum_pg_proc_prodataaccess - 1] = CharGetDatum('s');

		tuples[0] = heap_form_tuple(RelationGetDescr(pg_proc), values, nulls);

		ItemPointerSetHidden(&tuples[0]->t_self, F_GP_READ_ERROR_LOG);
		HeapTupleSetOid(tuples[0], F_GP_READ_ERROR_LOG);
	}

	/*
	 * gp_truncate_error_log
	 *
	 * CREATE FUNCTION pg_catalog.gp_truncate_error_log(text)
	 *   RETURNS bool AS 'gp_truncate_error_log'
	 *   LANGUAGE internal VOLATILE STRICT MODIFIES SQL DATA;
	 */
	{
		NameData		procname = {"gp_truncate_error_log"};
		Oid				proargtypes[] = {TEXTOID};

		MemSet(nulls, false, sizeof(bool) * Natts_pg_proc);
		values[Anum_pg_proc_proname - 1] = NameGetDatum(&procname);
		values[Anum_pg_proc_pronamespace - 1] = ObjectIdGetDatum(PG_CATALOG_NAMESPACE);
		values[Anum_pg_proc_proowner - 1] = ObjectIdGetDatum(BOOTSTRAP_SUPERUSERID);
		values[Anum_pg_proc_prolang - 1] = ObjectIdGetDatum(INTERNALlanguageId);
		values[Anum_pg_proc_provariadic - 1] = InvalidOid;
		values[Anum_pg_proc_proisagg - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_prosecdef - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_proisstrict - 1] = BoolGetDatum(true);
		values[Anum_pg_proc_proretset - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_provolatile - 1] = CharGetDatum('v');
		values[Anum_pg_proc_pronargs - 1] = Int16GetDatum(1);
		values[Anum_pg_proc_prorettype - 1] = ObjectIdGetDatum(BOOLOID);
		values[Anum_pg_proc_proiswin - 1] = BoolGetDatum(false);
		values[Anum_pg_proc_proargtypes - 1] = PointerGetDatum(buildoidvector(proargtypes, 1));
		values[Anum_pg_proc_proallargtypes - 1] = (Datum) 0;
		nulls[Anum_pg_proc_proallargtypes - 1] = true;
		values[Anum_pg_proc_proargmodes - 1] = (Datum) 0;
		nulls[Anum_pg_proc_proargmodes - 1] = true;
		values[Anum_pg_proc_proargnames - 1] = (Datum) 0;
		nulls[Anum_pg_proc_proargnames - 1] = true;
		values[Anum_pg_proc_prosrc - 1] = CStringGetTextDatum("gp_truncate_error_log");
		values[Anum_pg_proc_probin - 1] = (Datum) 0;
		nulls[Anum_pg_proc_probin - 1] = true;
		values[Anum_pg_proc_proacl - 1] = (Datum) 0;
		nulls[Anum_pg_proc_proacl - 1] = true;
		values[Anum_pg_proc_prodataaccess - 1] = CharGetDatum('m');

		tuples[1] = heap_form_tuple(RelationGetDescr(pg_proc), values, nulls);

		ItemPointerSetHidden(&tuples[1]->t_self, F_GP_TRUNCATE_ERROR_LOG);
		HeapTupleSetOid(tuples[1], F_GP_TRUNCATE_ERROR_LOG);
	}

	StaticPgProcTuples = tuples;
	StaticPgProcTupleLen = N_PGPROC_TUPLES;

	*len = StaticPgProcTupleLen;

	MemoryContextSwitchTo(oldcontext);

	return tuples;
}

/*
 * hidden_beginscan
 *
 * Allocates the hidden scan descriptor.
 */
HiddenScanDesc
hidden_beginscan(Relation heapRelation, int nkeys, ScanKey key)
{
	HiddenScanDesc		hscan = NULL;

	switch (RelationGetRelid(heapRelation))
	{
	case ProcedureRelationId:
		{
			int				i;

			hscan = palloc(sizeof(HiddenScanDescData));
			hscan->hdn_rel = heapRelation;
			hscan->hdn_nkeys = nkeys;
			hscan->hdn_key = palloc(sizeof(ScanKeyData) * nkeys);
			/* need a copy if it's indexscan as it scribles the key */
			for (i = 0; i < nkeys; i++)
				memcpy(&hscan->hdn_key[i], &key[i], sizeof(ScanKeyData));
			hscan->hdn_tuples = GetHiddenPgProcTuples(heapRelation, &hscan->hdn_len);
			hscan->hdn_idx = 0;
		}
		break;
	default:
		break;
	}

	return hscan;
}

/*
 * hidden_getnext
 *
 * Returns the next tuple from the hidden data.  This has to be called after
 * exceeding heap/index scan.
 */
HeapTuple
hidden_getnext(HiddenScanDesc hscan, ScanDirection direction)
{
	if (ScanDirectionIsForward(direction))
	{
		if (hscan->hdn_idx < 0)
			hscan->hdn_idx = 0;

		while (hscan->hdn_idx < hscan->hdn_len)
		{
			HeapTuple	tuple;
			bool		valid = true;

			/* fetch */
			tuple = hscan->hdn_tuples[hscan->hdn_idx++];

			/* key test */
			if (hscan->hdn_nkeys > 0)
				HeapKeyTest(tuple, RelationGetDescr(hscan->hdn_rel),
							hscan->hdn_nkeys, hscan->hdn_key, valid);

			if (valid)
			{
				/* save the last tuple for the sake of "no movement" */
				hscan->hdn_lasttuple = tuple;
				return tuple;
			}
		}
	}
	else if (ScanDirectionIsBackward(direction))
	{
		if (hscan->hdn_idx >= hscan->hdn_len)
			hscan->hdn_idx = hscan->hdn_len - 1;

		while (hscan->hdn_idx >= 0)
		{
			HeapTuple	tuple;
			bool		valid = true;

			/* fetch */
			tuple = hscan->hdn_tuples[hscan->hdn_idx--];

			/* key test */
			if (hscan->hdn_nkeys > 0)
				HeapKeyTest(tuple, RelationGetDescr(hscan->hdn_rel),
							hscan->hdn_nkeys, hscan->hdn_key, valid);

			if (valid)
			{
				/* save the last tuple for the sake of "no movement" */
				hscan->hdn_lasttuple = tuple;
				return tuple;
			}
		}
	}
	else
	{
		/*
		 * ``no movement'' scan direction: refetch prior tuple
		 */
		return hscan->hdn_lasttuple;
	}

	hscan->hdn_lasttuple = NULL;
	return NULL;
}

/*
 * hidden_endscan
 *
 * Finish and frees up the scan descriptor.
 */
void
hidden_endscan(HiddenScanDesc hscan)
{
	pfree(hscan->hdn_key);
	hscan->hdn_key = NULL;
	pfree(hscan);
}
