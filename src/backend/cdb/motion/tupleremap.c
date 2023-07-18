/*-------------------------------------------------------------------------
 *
 * tupleremap.c
 *
 * Derived from executor/tqueue.c in upstream PostgreSQL.
 *
 * Reused the remap logic on the motion receiver for record type remap,
 * with some changes:
 *
 * - TupleQueueReader is renamed to TupleRemapper;
 * - {Create,Destroy}TupleQueueReader() are renamed to
 *   {Create,Destroy}TupleRemapper;
 * - all TQ prefixes are renamed to TR;
 * - typmodmap use array instead of hash table for faster lookup;
 * - put range support in conditional compilation, only enabled if
 *   TYPTYPE_RANGE is defined;
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/cdb/motion/tupleremap.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_type.h"
#include "cdb/tupleremap.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rangetypes.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "utils/faultinjector.h"

/*
 * When a tuple is received on the motion receiver, the typmod of RECORDOID
 * represents the index of cache on the sender, which is meaningless on the
 * receiver. So we build a typmod map of RECORDOID type on the motion receiver,
 * and translate the typmod of tuple from remote to local.
 *
 * Motion receiver build a tree of TupleRemapInfo nodes to help them identify
 * which (sub) fields of transmitted tuples are composite and may thus need
 * remap processing.  We might need to look within arrays and ranges, not only
 * composites, to find composite sub-fields.  A NULL TupleRemapInfo pointer
 * indicates that it is known that the described field is not composite and
 * has no composite substructure.
 *
 * Note that we currently have to look at each composite field at runtime,
 * even if we believe it's of a named composite type (i.e., not RECORD).
 * This is because we allow the actual value to be a compatible transient
 * RECORD type.  That's grossly inefficient, and it would be good to get
 * rid of the requirement, but it's not clear what would need to change.
 *
 * Also, we allow the top-level tuple structure, as well as the actual
 * structure of composite subfields, to change from one tuple to the next
 * at runtime.  This may well be entirely historical, but it's mostly free
 * to support given the previous requirement; and other places in the system
 * also permit this, so it's not entirely clear if we could drop it.
 */

typedef enum
{
	TUPLE_REMAP_ARRAY,			/* array */
	TUPLE_REMAP_RANGE,			/* range */
	TUPLE_REMAP_RECORD			/* composite type, named or transient */
} TupleRemapClass;

typedef struct TupleRemapInfo TupleRemapInfo;

typedef struct ArrayRemapInfo
{
	int16		typlen;			/* array element type's storage properties */
	bool		typbyval;
	char		typalign;
	TupleRemapInfo *element_remap;	/* array element type's remap info */
} ArrayRemapInfo;

typedef struct RangeRemapInfo
{
	TypeCacheEntry *typcache;	/* range type's typcache entry */
	TupleRemapInfo *bound_remap;	/* range bound type's remap info */
} RangeRemapInfo;

typedef struct RecordRemapInfo
{
	/* Original (remote) type ID info last seen for this composite field */
	Oid			rectypid;
	int32		rectypmod;
	/* Local RECORD typmod, or -1 if unset; not used on sender side */
	int32		localtypmod;
	/* If no fields of the record require remapping, these are NULL: */
	TupleDesc	tupledesc;		/* copy of record's tupdesc */
	TupleRemapInfo **field_remap;	/* each field's remap info */
} RecordRemapInfo;

struct TupleRemapInfo
{
	TupleRemapClass remapclass;
	union
	{
		ArrayRemapInfo arr;
		RangeRemapInfo rng;
		RecordRemapInfo rec;
	}			u;
};

/*
 * TupleRemapper object's private contents
 *
 * tupledesc is a pointer to data supplied by remapper's caller.
 * The typmodmap and remap info are owned by the TupleRemapper and
 * are kept in mycontext.
 *
 * "typedef struct TupleRemapper TupleRemapper" is in tupleremap.h
 */
struct TupleRemapper
{
	MemoryContext mycontext;	/* context containing TupleRemapper (with typmodmap and field_remapinfo) */
	int32	   *typmodmap;		/* typmod map from remote to local */
	int			typmodmapsize;	/* size of typmodmap */
	TupleDesc	tupledesc;		/* current top-level tuple descriptor */
	TupleRemapInfo **field_remapinfo;	/* current top-level remap info */
	bool		remap_needed;	/* is remap needed */
};

static HeapTuple TRRemapTuple(TupleRemapper *remapper,
							  TupleDesc tupledesc,
							  TupleRemapInfo **field_remapinfo,
							  HeapTuple tuple);
static MinimalTuple TRRemapMinimalTuple(TupleRemapper *remapper,
										TupleDesc tupledesc,
										TupleRemapInfo **field_remapinfo,
										MinimalTuple tuple);
static Datum TRRemap(TupleRemapper *remapper, TupleRemapInfo *remapinfo,
					 Datum value, bool *changed);
static Datum TRRemapArray(TupleRemapper *remapper, ArrayRemapInfo *remapinfo,
						  Datum value, bool *changed);
static Datum TRRemapRange(TupleRemapper *remapper, RangeRemapInfo *remapinfo,
						  Datum value, bool *changed);
static Datum TRRemapRecord(TupleRemapper *remapper, RecordRemapInfo *remapinfo,
						   Datum value, bool *changed);
static TupleRemapInfo *BuildTupleRemapInfo(Oid typid, MemoryContext mycontext);
static TupleRemapInfo *BuildArrayRemapInfo(Oid elemtypid,
										   MemoryContext mycontext);
static TupleRemapInfo *BuildRangeRemapInfo(Oid rngtypid,
										   MemoryContext mycontext);
static TupleRemapInfo **BuildFieldRemapInfo(TupleDesc tupledesc,
											MemoryContext mycontext);


/*
 * Create a tuple remapper.
 */
TupleRemapper *
CreateTupleRemapper(void)
{
	TupleRemapper *remapper = palloc0(sizeof(TupleRemapper));

	remapper->mycontext = CurrentMemoryContext;
	remapper->typmodmap = NULL;
	remapper->typmodmapsize = 0;
	remapper->tupledesc = NULL;
	remapper->field_remapinfo = NULL;
	remapper->remap_needed = false;

	return remapper;
}

/*
 * Destroy a tuple remapper.
 */
void
DestroyTupleRemapper(TupleRemapper *remapper)
{
	if (remapper->typmodmap != NULL)
		pfree(remapper->typmodmap);
	/* Is it worth trying to free substructure of the remap tree? */
	if (remapper->field_remapinfo != NULL)
		pfree(remapper->field_remapinfo);
	pfree(remapper);
}

/*
 * Remap a tuple if needed
 *
 * Form a new tuple with all the remote typmods remapped to local typmods.
 */
MinimalTuple
TRCheckAndRemap(TupleRemapper *remapper, TupleDesc tupledesc, MinimalTuple tuple)
{
	if (!remapper->remap_needed)
		return tuple;

	if (!remapper->field_remapinfo)
	{
		Assert(remapper->tupledesc == NULL);
		remapper->field_remapinfo = BuildFieldRemapInfo(tupledesc,
														remapper->mycontext);
		if (remapper->field_remapinfo != NULL)
		{
			/* Remapping is required. Save a copy of the tupledesc */
			remapper->tupledesc = tupledesc;
		}
	}

	return TRRemapMinimalTuple(remapper, tupledesc, remapper->field_remapinfo, tuple);
}

/*
 * Handle the record type cache from motion sender.
 *
 * Add the record types to local cache, and build a map from remote to local.
 */
void
TRHandleTypeLists(TupleRemapper *remapper, List *typelist)
{
	int			j;
	ListCell   *cell;
	int			mapsize = remapper->typmodmapsize + list_length(typelist);

	/* alloc typmodmap in remapper->mycontext, usually it's InterconnectContext */
	MemoryContext oldcontext = MemoryContextSwitchTo(remapper->mycontext);
	if (remapper->typmodmap)
		remapper->typmodmap = repalloc(remapper->typmodmap, mapsize * sizeof(int32));
	else
		remapper->typmodmap = palloc(mapsize * sizeof(int32));
	MemoryContextSwitchTo(oldcontext);

	for (j = 0; j < list_length(typelist); j++)
		remapper->typmodmap[remapper->typmodmapsize + j] = -1;

	remapper->typmodmapsize = mapsize;

	foreach(cell, typelist)
	{
		int32		local_typmod;
		TupleDescNode *descnode = (TupleDescNode *) lfirst(cell);
		int32		remote_typmod = descnode->tuple->tdtypmod;

		/*
		 * assign_record_type_typmod() will update tdtypmod to the local
		 * typmod
		 */
		assign_record_type_typmod(descnode->tuple);

		local_typmod = descnode->tuple->tdtypmod;

		Assert(remote_typmod >= 0);
		Assert(local_typmod >= 0);
		Assert(remote_typmod < remapper->typmodmapsize);

		remapper->typmodmap[remote_typmod] = local_typmod;

		if (!remapper->remap_needed && local_typmod != remote_typmod)
			remapper->remap_needed = true;
	}

	SIMPLE_FAULT_INJECTOR("handled_typmodmap");
}


/*
 * Remap a single Datum, which can be a RECORD datum using the remote system's
 * typmods.
 */
Datum
TRRemapDatum(TupleRemapper *remapper, Oid typeid, Datum value)
{
	TupleRemapInfo *remapinfo;
	bool		changed;

	remapinfo = BuildTupleRemapInfo(typeid, remapper->mycontext);

	if (!remapinfo)
		return value;

	value = TRRemap(remapper, remapinfo, value, &changed);

	pfree(remapinfo);

	return value;
}

/*
 * Copy the given tuple, remapping any transient typmods contained in it.
 */
static HeapTuple
TRRemapTuple(TupleRemapper *remapper,
			 TupleDesc tupledesc,
			 TupleRemapInfo **field_remapinfo,
			 HeapTuple tuple)
{
	Datum	   *values;
	bool	   *isnull;
	bool		changed = false;
	int			i;

	/*
	 * If no remapping is necessary, just copy the tuple into a single
	 * palloc'd chunk, as caller will expect.
	 */
	if (field_remapinfo == NULL)
		return tuple;

	/* Deform tuple so we can remap record typmods for individual attrs. */
	values = (Datum *) palloc(tupledesc->natts * sizeof(Datum));
	isnull = (bool *) palloc(tupledesc->natts * sizeof(bool));

	heap_deform_tuple(tuple, tupledesc, values, isnull);

	/* Recursively process each interesting non-NULL attribute. */
	for (i = 0; i < tupledesc->natts; i++)
	{
		if (isnull[i] || field_remapinfo[i] == NULL)
			continue;
		values[i] = TRRemap(remapper, field_remapinfo[i], values[i], &changed);
	}

	/* Reconstruct the modified tuple, if anything was modified. */
	if (changed)
		return heap_form_tuple(tupledesc, values, isnull);
	else
		return tuple;
}

/*
 * Like TRRemapTuple(), but for a MinimalTuple
 */
static MinimalTuple
TRRemapMinimalTuple(TupleRemapper *remapper,
					TupleDesc tupledesc,
					TupleRemapInfo **field_remapinfo,
					MinimalTuple tuple)
{
	Datum	   *values;
	bool	   *isnull;
	bool		changed = false;
	int			i;
	HeapTupleData htup;

	/*
	 * If no remapping is necessary, just copy the tuple into a single
	 * palloc'd chunk, as caller will expect.
	 */
	if (field_remapinfo == NULL)
		return tuple;

	/* Deform tuple so we can remap record typmods for individual attrs. */
	values = (Datum *) palloc(tupledesc->natts * sizeof(Datum));
	isnull = (bool *) palloc(tupledesc->natts * sizeof(bool));

	htup.t_len = tuple->t_len + MINIMAL_TUPLE_OFFSET;
	htup.t_data = (HeapTupleHeader) ((char *) tuple - MINIMAL_TUPLE_OFFSET);

	heap_deform_tuple(&htup, tupledesc, values, isnull);

	/* Recursively process each interesting non-NULL attribute. */
	for (i = 0; i < tupledesc->natts; i++)
	{
		if (isnull[i] || field_remapinfo[i] == NULL)
			continue;
		values[i] = TRRemap(remapper, field_remapinfo[i], values[i], &changed);
	}

	/* Reconstruct the modified tuple, if anything was modified. */
	if (changed)
		return heap_form_minimal_tuple(tupledesc, values, isnull);
	else
		return tuple;
}

/*
 * Process the given datum and replace any transient record typmods
 * contained in it.  Set *changed to TRUE if we actually changed the datum.
 *
 * remapinfo is previously-computed remapping info about the datum's type.
 *
 * This function just dispatches based on the remap class.
 */
static Datum
TRRemap(TupleRemapper *remapper, TupleRemapInfo *remapinfo,
		Datum value, bool *changed)
{
	/* This is recursive, so it could be driven to stack overflow. */
	check_stack_depth();

	switch (remapinfo->remapclass)
	{
		case TUPLE_REMAP_ARRAY:
			return TRRemapArray(remapper, &remapinfo->u.arr, value, changed);

		case TUPLE_REMAP_RANGE:
			return TRRemapRange(remapper, &remapinfo->u.rng, value, changed);

		case TUPLE_REMAP_RECORD:
			return TRRemapRecord(remapper, &remapinfo->u.rec, value, changed);
	}

	elog(ERROR, "unrecognized remapper remap class: %d",
		 (int) remapinfo->remapclass);
	return (Datum) 0;
}

/*
 * Process the given array datum and replace any transient record typmods
 * contained in it.  Set *changed to TRUE if we actually changed the datum.
 */
static Datum
TRRemapArray(TupleRemapper *remapper, ArrayRemapInfo *remapinfo,
			 Datum value, bool *changed)
{
	ArrayType  *arr = DatumGetArrayTypeP(value);
	Oid			typid = ARR_ELEMTYPE(arr);
	bool		element_changed = false;
	Datum	   *elem_values;
	bool	   *elem_nulls;
	int			num_elems;
	int			i;

	/* Deconstruct the array. */
	deconstruct_array(arr, typid, remapinfo->typlen,
					  remapinfo->typbyval, remapinfo->typalign,
					  &elem_values, &elem_nulls, &num_elems);

	/* Remap each element. */
	for (i = 0; i < num_elems; i++)
	{
		if (!elem_nulls[i])
			elem_values[i] = TRRemap(remapper,
									 remapinfo->element_remap,
									 elem_values[i],
									 &element_changed);
	}

	if (element_changed)
	{
		/* Reconstruct and return the array.  */
		*changed = true;
		arr = construct_md_array(elem_values, elem_nulls,
								 ARR_NDIM(arr), ARR_DIMS(arr), ARR_LBOUND(arr),
								 typid, remapinfo->typlen,
								 remapinfo->typbyval, remapinfo->typalign);
		return PointerGetDatum(arr);
	}

	/* Else just return the value as-is. */
	return value;
}

/*
 * Process the given range datum and replace any transient record typmods
 * contained in it.  Set *changed to TRUE if we actually changed the datum.
 */
static Datum
TRRemapRange(TupleRemapper *remapper, RangeRemapInfo *remapinfo,
			 Datum value, bool *changed)
{
	RangeType  *range = DatumGetRangeTypeP(value);
	bool		bound_changed = false;
	RangeBound	lower;
	RangeBound	upper;
	bool		empty;

	/* Extract the lower and upper bounds. */
	range_deserialize(remapinfo->typcache, range, &lower, &upper, &empty);

	/* Nothing to do for an empty range. */
	if (empty)
		return value;

	/* Remap each bound, if present. */
	if (!upper.infinite)
		upper.val = TRRemap(remapper, remapinfo->bound_remap,
							upper.val, &bound_changed);
	if (!lower.infinite)
		lower.val = TRRemap(remapper, remapinfo->bound_remap,
							lower.val, &bound_changed);

	if (bound_changed)
	{
		/* Reserialize.  */
		*changed = true;
		range = range_serialize(remapinfo->typcache, &lower, &upper, empty);
		return RangeTypePGetDatum(range);
	}

	/* Else just return the value as-is. */
	return value;
}

/*
 * Process the given record datum and replace any transient record typmods
 * contained in it.  Set *changed to TRUE if we actually changed the datum.
 */
static Datum
TRRemapRecord(TupleRemapper *remapper, RecordRemapInfo *remapinfo,
			  Datum value, bool *changed)
{
	HeapTupleHeader tup;
	Oid			typid;
	int32		typmod;
	bool		changed_typmod;
	TupleDesc	tupledesc;

	/* Extract type OID and typmod from tuple. */
	tup = DatumGetHeapTupleHeader(value);
	typid = HeapTupleHeaderGetTypeId(tup);
	typmod = HeapTupleHeaderGetTypMod(tup);

	/*
	 * If first time through, or if this isn't the same composite type as last
	 * time, identify the required typmod mapping, and then look up the
	 * necessary information for processing the fields.
	 */
	if (typid != remapinfo->rectypid || typmod != remapinfo->rectypmod)
	{
		/* Free any old data. */
		if (remapinfo->tupledesc != NULL)
			FreeTupleDesc(remapinfo->tupledesc);
		/* Is it worth trying to free substructure of the remap tree? */
		if (remapinfo->field_remap != NULL)
			pfree(remapinfo->field_remap);

		/* If transient record type, look up matching local typmod. */
		if (typid == RECORDOID)
		{
			Assert(remapper->typmodmap != NULL);
			Assert(typmod >= 0);
			if (typmod >= remapper->typmodmapsize)
				elog(ERROR, "received unknown record type with typmod %d", typmod);

			remapinfo->localtypmod = remapper->typmodmap[typmod];
			Assert(remapinfo->localtypmod >= 0);
		}
		else
			remapinfo->localtypmod = -1;

		/* Look up tuple descriptor in typcache. */
		tupledesc = lookup_rowtype_tupdesc(typid, remapinfo->localtypmod);

		/* Figure out whether fields need recursive processing. */
		remapinfo->field_remap = BuildFieldRemapInfo(tupledesc,
													 remapper->mycontext);
		if (remapinfo->field_remap != NULL)
		{
			/*
			 * We need to inspect the record contents, so save a copy of the
			 * tupdesc.  (We could possibly just reference the typcache's
			 * copy, but then it's problematic when to release the refcount.)
			 */
			MemoryContext oldcontext = MemoryContextSwitchTo(remapper->mycontext);

			remapinfo->tupledesc = CreateTupleDescCopy(tupledesc);
			MemoryContextSwitchTo(oldcontext);
		}
		else
		{
			/* No fields of the record require remapping. */
			remapinfo->tupledesc = NULL;
		}
		remapinfo->rectypid = typid;
		remapinfo->rectypmod = typmod;

		/* Release reference count acquired by lookup_rowtype_tupdesc. */
		DecrTupleDescRefCount(tupledesc);
	}

	/* If transient record, replace remote typmod with local typmod. */
	if (typid == RECORDOID && typmod != remapinfo->localtypmod)
	{
		typmod = remapinfo->localtypmod;
		changed_typmod = true;
	}
	else
		changed_typmod = false;

	/*
	 * If we need to change the typmod, or if there are any potentially
	 * remappable fields, replace the tuple.
	 */
	if (changed_typmod || remapinfo->field_remap != NULL)
	{
		HeapTupleData htup;
		HeapTuple	atup;

		/* For now, assume we always need to change the tuple in this case. */
		*changed = true;

		/* Copy tuple, possibly remapping contained fields. */
		ItemPointerSetInvalid(&htup.t_self);
		htup.t_len = HeapTupleHeaderGetDatumLength(tup);
		htup.t_data = tup;
		atup = TRRemapTuple(remapper,
							remapinfo->tupledesc,
							remapinfo->field_remap,
							&htup);

		/* Apply the correct labeling for a local Datum. */
		HeapTupleHeaderSetTypeId(atup->t_data, typid);
		HeapTupleHeaderSetTypMod(atup->t_data, typmod);
		HeapTupleHeaderSetDatumLength(atup->t_data, htup.t_len);

		/* And return the results. */
		return PointerGetDatum(atup->t_data);
	}

	/* Else just return the value as-is. */
	return value;
}

/*
 * Build remap info for the specified data type, storing it in mycontext.
 * Returns NULL if neither the type nor any subtype could require remapping.
 */
static TupleRemapInfo *
BuildTupleRemapInfo(Oid typid, MemoryContext mycontext)
{
	HeapTuple	tup;
	Form_pg_type typ;

	/* This is recursive, so it could be driven to stack overflow. */
	check_stack_depth();

restart:
	tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
	if (!HeapTupleIsValid(tup))
		elog(ERROR, "cache lookup failed for type %u", typid);
	typ = (Form_pg_type) GETSTRUCT(tup);

	/* Look through domains to underlying base type. */
	if (typ->typtype == TYPTYPE_DOMAIN)
	{
		typid = typ->typbasetype;
		ReleaseSysCache(tup);
		goto restart;
	}

	/* If it's a true array type, deal with it that way. */
	if (OidIsValid(typ->typelem) && typ->typlen == -1)
	{
		typid = typ->typelem;
		ReleaseSysCache(tup);
		return BuildArrayRemapInfo(typid, mycontext);
	}

	/* Similarly, deal with ranges appropriately. */
	if (typ->typtype == TYPTYPE_RANGE)
	{
		ReleaseSysCache(tup);
		return BuildRangeRemapInfo(typid, mycontext);
	}

	/*
	 * If it's a composite type (including RECORD), set up for remapping.  We
	 * don't attempt to determine the status of subfields here, since we do
	 * not have enough information yet; just mark everything invalid.
	 */
	if (typ->typtype == TYPTYPE_COMPOSITE || typid == RECORDOID)
	{
		TupleRemapInfo *remapinfo;

		remapinfo = (TupleRemapInfo *)
			MemoryContextAlloc(mycontext, sizeof(TupleRemapInfo));
		remapinfo->remapclass = TUPLE_REMAP_RECORD;
		remapinfo->u.rec.rectypid = InvalidOid;
		remapinfo->u.rec.rectypmod = -1;
		remapinfo->u.rec.localtypmod = -1;
		remapinfo->u.rec.tupledesc = NULL;
		remapinfo->u.rec.field_remap = NULL;
		ReleaseSysCache(tup);
		return remapinfo;
	}

	/* Nothing else can possibly need remapping attention. */
	ReleaseSysCache(tup);
	return NULL;
}

static TupleRemapInfo *
BuildArrayRemapInfo(Oid elemtypid, MemoryContext mycontext)
{
	TupleRemapInfo *remapinfo;
	TupleRemapInfo *element_remapinfo;

	/* See if element type requires remapping. */
	element_remapinfo = BuildTupleRemapInfo(elemtypid, mycontext);
	/* If not, the array doesn't either. */
	if (element_remapinfo == NULL)
		return NULL;
	/* OK, set up to remap the array. */
	remapinfo = (TupleRemapInfo *)
		MemoryContextAlloc(mycontext, sizeof(TupleRemapInfo));
	remapinfo->remapclass = TUPLE_REMAP_ARRAY;
	get_typlenbyvalalign(elemtypid,
						 &remapinfo->u.arr.typlen,
						 &remapinfo->u.arr.typbyval,
						 &remapinfo->u.arr.typalign);
	remapinfo->u.arr.element_remap = element_remapinfo;
	return remapinfo;
}

static TupleRemapInfo *
BuildRangeRemapInfo(Oid rngtypid, MemoryContext mycontext)
{
	TupleRemapInfo *remapinfo;
	TupleRemapInfo *bound_remapinfo;
	TypeCacheEntry *typcache;

	/*
	 * Get range info from the typcache.  We assume this pointer will stay
	 * valid for the duration of the query.
	 */
	typcache = lookup_type_cache(rngtypid, TYPECACHE_RANGE_INFO);
	if (typcache->rngelemtype == NULL)
		elog(ERROR, "type %u is not a range type", rngtypid);

	/* See if range bound type requires remapping. */
	bound_remapinfo = BuildTupleRemapInfo(typcache->rngelemtype->type_id,
										  mycontext);
	/* If not, the range doesn't either. */
	if (bound_remapinfo == NULL)
		return NULL;
	/* OK, set up to remap the range. */
	remapinfo = (TupleRemapInfo *)
		MemoryContextAlloc(mycontext, sizeof(TupleRemapInfo));
	remapinfo->remapclass = TUPLE_REMAP_RANGE;
	remapinfo->u.rng.typcache = typcache;
	remapinfo->u.rng.bound_remap = bound_remapinfo;
	return remapinfo;
}

/*
 * Build remap info for fields of the type described by the given tupdesc.
 * Returns an array of TupleRemapInfo pointers, or NULL if no field
 * requires remapping.  Data is allocated in mycontext.
 */
static TupleRemapInfo **
BuildFieldRemapInfo(TupleDesc tupledesc, MemoryContext mycontext)
{
	TupleRemapInfo **remapinfo;
	bool		noop = true;
	int			i;

	/* Recursively determine the remapping status of each field. */
	remapinfo = (TupleRemapInfo **)
		MemoryContextAlloc(mycontext,
						   tupledesc->natts * sizeof(TupleRemapInfo *));
	for (i = 0; i < tupledesc->natts; i++)
	{
		Form_pg_attribute attr = TupleDescAttr(tupledesc, i);

		if (attr->attisdropped)
		{
			remapinfo[i] = NULL;
			continue;
		}
		remapinfo[i] = BuildTupleRemapInfo(attr->atttypid, mycontext);
		if (remapinfo[i] != NULL)
			noop = false;
	}

	/* If no fields require remapping, report that by returning NULL. */
	if (noop)
	{
		pfree(remapinfo);
		remapinfo = NULL;
	}

	return remapinfo;
}
