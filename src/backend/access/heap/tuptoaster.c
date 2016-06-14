/*-------------------------------------------------------------------------
 *
 * tuptoaster.c
 *	  Support routines for external and compressed storage of
 *	  variable size attributes.
 *
 * Copyright (c) 2000-2009, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/access/heap/tuptoaster.c,v 1.72 2007/03/29 00:15:37 tgl Exp $
 *
 *
 * INTERFACE ROUTINES
 *		toast_insert_or_update -
 *			Try to make a given tuple fit into one page by compressing
 *			or moving off attributes
 *
 *		toast_delete -
 *			Reclaim toast storage when a tuple is deleted
 *
 *		heap_tuple_untoast_attr -
 *			Fetch back a given value from the "secondary" relation
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <fcntl.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "utils/fmgroids.h"
#include "utils/pg_lzcompress.h"
#include "utils/rel.h"
#include "utils/typcache.h"


#undef TOAST_DEBUG

/* ----------------
 * struct varattrib is the header of a varlena object that may have been
 * TOASTed.  Generally, only the code closely associated with TOAST logic
 * should mess directly with struct varattrib or use the VARATT_FOO macros.
 * ----------------
 */

typedef union varattrib
{
	struct						/* Normal varlena (4-byte length) */
	{
		uint8 	va_header;
		char 	va_data[1];
	} va_1byte;
	struct
	{
		uint8 	va_header;
		uint8	va_padding[3];
		int32	va_rawsize;		/* Plain data size */
		int32	va_extsize;		/* External saved size */
		Oid		va_valueid;		/* Unique identifier of value */
		Oid		va_toastrelid;	/* RelID where to find chunks */
	} va_external;
	struct						/* Compressed-in-line format */
	{
		uint32 	va_header;
		uint32	va_rawsize;		/* Original data size (excludes header) */
		char	va_data[1];		/* Compressed data */
	} va_compressed;
	struct
	{
		uint32 	va_header;
		char 	va_data[1];
	} va_4byte;
} varattrib;


/* these are used by tuptoaster.c */
#define VARHDRSZ_SHORT                                          1
#define VARSIZE_SHORT_D(D)                                      VARSIZE_SHORT(DatumGetPointer(D))
#define VARDATA_SHORT_D(D)                                      VARDATA_SHORT(DatumGetPointer(D))

/* Do we want to rename these? */
#define VARATT_IS_COMPRESSED_D(D)                       VARATT_IS_COMPRESSED(DatumGetPointer(D))
#define VARATT_SET_COMPRESSED(PTR)                      SET_VARSIZE_C(PTR)

#define VARATT_IS_EXTENDED_D(D)                         VARATT_IS_EXTENDED(DatumGetPointer(D))

#define VARATT_EXTERNAL_IS_COMPRESSED(PTR)  (((varattrib*)(PTR))->va_external.va_extsize != \
                                              ((varattrib*)(PTR))->va_external.va_rawsize - VARHDRSZ)
#define VARSIZE_ANY_EXHDR_D(D)                      VARSIZE_ANY_EXHDR(DatumGetPointer(D))

/* caution: this will not work on an external or compressed-in-line Datum */
/* caution: this will return a possibly unaligned pointer */
#define VARDATA_ANY_D(D)                                        VARDATA_ANY(DatumGetPointer(D))


#define VARATT_COULD_SHORT(PTR) (VARATT_IS_4B_U(PTR) && (VARSIZE(PTR)-VARHDRSZ+VARHDRSZ_SHORT <= VARATT_SHORT_MAX))
#define VARSIZE_TO_SHORT(PTR)   ((char)(VARSIZE(PTR)-VARHDRSZ+VARHDRSZ_SHORT) | 0x80)
#define VARSIZE_TO_SHORT_D(D)   VARSIZE_TO_SHORT(DatumGetPointer(D))


#define SET_VARSIZE_C(PTR)			(((varattrib_1b *) (PTR))->va_header |= 0x40)


/* Size of an EXTERNAL datum that contains a standard TOAST pointer */
#define TOAST_POINTER_SIZE (VARHDRSZ_EXTERNAL + sizeof(struct varatt_external))

/*
 * Testing whether an externally-stored value is compressed now requires
 * comparing extsize (the actual length of the external data) to rawsize
 * (the original uncompressed datum's size).  The latter includes VARHDRSZ
 * overhead, the former doesn't.  We never use compression unless it actually
 * saves space, so we expect either equality or less-than.
 */
/*#define VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer) \
	((toast_pointer).va_extsize < (toast_pointer).va_rawsize - VARHDRSZ)
*/
/*
 * Macro to fetch the possibly-unaligned contents of an EXTERNAL datum
 * into a local "struct varatt_external" toast pointer.  This should be
 * just a memcpy, but some versions of gcc seem to produce broken code
 * that assumes the datum contents are aligned.  Introducing an explicit
 * intermediate "varattrib_1b_e *" variable seems to fix it.
 */
#define VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr) \
do { \
	varattrib_1b_e *attre = (varattrib_1b_e *) (attr); \
	Assert(VARATT_IS_EXTERNAL(attre)); \
	memcpy(&(toast_pointer), VARDATA_EXTERNAL(attre), sizeof(toast_pointer)); \
} while (0)


/* 
 * Although this macro sets var_len_1be, data stored in GPDB might
 * not have anything set in this byte, so you can't count on it's value
 * Not really a problem, since it is always based on TOAST_POINTER_LEN
 */
#define SET_VARSIZE_1B_E(PTR,len) \
	(((varattrib_1b_e *) (PTR))->va_header = 0x80, \
	 ((varattrib_1b_e *) (PTR))->va_len_1be = (len))


#define VARRAWSIZE_4B_C(PTR) \
	(((varattrib_4b *) (PTR))->va_compressed.va_rawsize)


#define SET_VARSIZE_EXTERNAL(PTR, len)		SET_VARSIZE_1B_E(PTR, len)


#define SET_VARSIZE_C(PTR)			(((varattrib_1b *) (PTR))->va_header |= 0x40)

static void toast_delete_datum(Relation rel, Datum value);
static Datum toast_save_datum(Relation rel, Datum value, bool isFrozen,
						bool use_wal, bool use_fsm);
static struct varlena *toast_fetch_datum(struct varlena * attr);
static struct varlena *toast_fetch_datum_slice(struct varlena * attr,
						int32 sliceoffset, int32 length);


/* ----------
 * heap_tuple_fetch_attr -
 *
 *	Public entry point to get back a toasted value from
 *	external storage (possibly still in compressed format).
 *
 * This will return a datum that contains all the data internally, ie, not
 * relying on external storage, but it can still be compressed or have a short
 * header.
 ----------
 */
struct varlena *
heap_tuple_fetch_attr(struct varlena *attr)
{
	struct varlena  *result;

	if (VARATT_IS_EXTERNAL(attr))
	{
		/*
		 * This is an external stored plain value
		 */
		result = toast_fetch_datum(attr);
	}
	else
	{
		/*
		 * This is a plain value inside of the main tuple - why am I called?
		 */
		result = attr;
	}

	return result;
}


/**
 * If this function is changed then update varattrib_untoast_ptr_len as well
 */
int varattrib_untoast_len(Datum d)
{
	if (DatumGetPointer(d) == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg(" Unable to detoast datum "),
				 errprintstack(true)));
	}

	struct varlena *va = (struct varlena *) DatumGetPointer(d);
	varattrib *attr = (varattrib *) va;

	int len = -1;
	void *toFree = NULL;

	if(VARATT_IS_EXTENDED(attr))
	{
		if(VARATT_IS_EXTERNAL(attr))
		{
			attr = (varattrib *)toast_fetch_datum((struct varlena *)attr);
			/* toast_fetch_datum will palloc, so set it up for free */
			toFree = attr;
		}

		if(VARATT_IS_COMPRESSED(attr))
		{
			PGLZ_Header *tmp = (PGLZ_Header *) attr;
			len = PGLZ_RAW_SIZE(tmp);
		}
		else if(VARATT_IS_SHORT(attr))
		{
			len = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT;
		}
	}

	if(len == -1)
	{
		len = VARSIZE(attr) - VARHDRSZ;
	}

	if ( toFree)
		pfree(toFree);

	Assert(len >= 0);
	return len;
}

/**
 * If this function is changed then update varattrib_untoast_len as well
 */
void varattrib_untoast_ptr_len(Datum d, char **datastart, int *len, void **tofree)
{
	if (DatumGetPointer(d) == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg(" Unable to detoast datum "),
				 errprintstack(true)));
	}

	struct varlena *va = (struct varlena *) DatumGetPointer(d);
	varattrib *attr = (varattrib *) va;

	*len = -1;
	*tofree = NULL;

	if(VARATT_IS_EXTENDED(attr))
	{
		if(VARATT_IS_EXTERNAL(attr))
		{
			attr = (varattrib *)toast_fetch_datum((struct varlena *)attr);
			/* toast_fetch_datum will palloc, so set it up for free */
			*tofree = attr;
		}

		if(VARATT_IS_COMPRESSED(attr))
		{
			PGLZ_Header *tmp = (PGLZ_Header *) attr;
			attr = (varattrib *) palloc(PGLZ_RAW_SIZE(tmp) + VARHDRSZ);
			SET_VARSIZE(attr, PGLZ_RAW_SIZE(tmp) + VARHDRSZ);
			pglz_decompress(tmp, VARDATA(attr));

			/* If tofree is set, that is, we get it from toast_fetch_datum.  
			 * We need to free it here 
			 */
			if(*tofree)
				pfree(*tofree);
			*tofree = attr;
		}
		else if(VARATT_IS_SHORT(attr))
		{
		    /* Warning! Return unaligned pointer! */
			*len = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT;
			*datastart = VARDATA_SHORT(attr);
			attr = NULL;
		}
	}

	if(*len == -1)
	{
		*datastart = VARDATA(attr);
		*len = VARSIZE(attr) - VARHDRSZ;
	}

	Assert(*len >= 0);
}

/* ----------
 * heap_tuple_untoast_attr -
 *
 *	Public entry point to get back a toasted value from compression
 *	or external storage.
 * ----------
 */
struct varlena *
heap_tuple_untoast_attr(struct varlena *attr)
{
	if (VARATT_IS_EXTERNAL(attr))
	{
		/*
		 * This is an externally stored datum --- fetch it back from there
		 */
		attr = toast_fetch_datum(attr);
		/* fall through to IS_COMPRESSED if it's a compressed external datum */
	}
	
	if (VARATT_IS_COMPRESSED(attr))
	{
		/*
		 * This is a compressed value inside of the main tuple
		 */
		PGLZ_Header *tmp = (PGLZ_Header *) attr;

		attr = (struct varlena *) palloc(PGLZ_RAW_SIZE(tmp) + VARHDRSZ);
		SET_VARSIZE(attr, PGLZ_RAW_SIZE(tmp) + VARHDRSZ);
		pglz_decompress(tmp, VARDATA(attr));
	}
	else if (VARATT_IS_SHORT(attr))
	{
		/*
		 * This is a short-header varlena --- convert to 4-byte header format
		 */
		Size 	data_size = VARSIZE_SHORT(attr);
		Size 	new_size = data_size - VARHDRSZ_SHORT + VARHDRSZ;
		varattrib *tmp = (varattrib *)attr;
		
		/* This is a "short" varlena header but is otherwise a normal varlena */

		attr = (struct varlena *) palloc(new_size);
		SET_VARSIZE(attr, new_size);
		memcpy(VARDATA(attr), VARDATA_SHORT(tmp), data_size - VARHDRSZ_SHORT);
	}

	return attr;
}


/* ----------
 * heap_tuple_untoast_attr_slice -
 *
 *		Public entry point to get back part of a toasted value
 *		from compression or external storage.
 * ----------
 */
struct varlena *
heap_tuple_untoast_attr_slice(struct varlena * attr,
							  int32 sliceoffset, int32 slicelength)
{
	varattrib *preslice;
	varattrib *result;
	char	   *attrdata;
	int32		attrsize;

	if (VARATT_IS_EXTERNAL(attr))
	{
		/* fast path for non-compressed external datums */
		if (!VARATT_EXTERNAL_IS_COMPRESSED(attr))
			return toast_fetch_datum_slice(attr, sliceoffset, slicelength);
		/* this automatically sets the compressed flag if appropriate */
		preslice = (varattrib *)toast_fetch_datum(attr);
	}
	else
		preslice = (varattrib *)attr;

	if (VARATT_IS_COMPRESSED(preslice))
	{
		unsigned size;
		PGLZ_Header *tmp;

		tmp = (PGLZ_Header *) preslice;
		size = PGLZ_RAW_SIZE(tmp) + VARHDRSZ;

		preslice = (varattrib *) palloc(size);
		SET_VARSIZE(preslice, size);
		pglz_decompress(tmp, VARDATA(preslice));

		if (tmp != (PGLZ_Header *) attr)
			pfree(tmp);
	}

	if (VARATT_IS_SHORT(preslice))
	{
		attrdata = VARDATA_SHORT(preslice);
		attrsize = VARSIZE_SHORT(preslice) - VARHDRSZ_SHORT;
	}
	else
	{
		attrdata = VARDATA(preslice);
		attrsize = VARSIZE(preslice) - VARHDRSZ;
	}

	/* slicing of datum for compressed cases and plain value */

	if (sliceoffset >= attrsize)
	{
		sliceoffset = 0;
		slicelength = 0;
	}

	if (((sliceoffset + slicelength) > attrsize) || slicelength < 0)
		slicelength = attrsize - sliceoffset;

	result = (varattrib *) palloc(slicelength + VARHDRSZ);
	SET_VARSIZE(result, slicelength + VARHDRSZ);

	memcpy(VARDATA(result), attrdata + sliceoffset, slicelength);

	if ((struct varlena *)preslice != (struct varlena *)attr)
		pfree(preslice);

	return (struct varlena *)result;
}


/* ----------
 * toast_raw_datum_size -
 *
 *	Return the raw (detoasted) size of a varlena datum
 *	(including the VARHDRSZ header)
 * ----------
 */
Size
toast_raw_datum_size(Datum value)
{
	varattrib  *attr = (varattrib *) DatumGetPointer(value);
	Size		result;

	if (VARATT_IS_EXTERNAL(attr))
	{
		/* va_rawsize is the size of the original datum -- including header */
		result = attr->va_external.va_rawsize;
	}
	else if (VARATT_IS_COMPRESSED(attr))
	{
		/* here, va_rawsize is just the payload size */
		result = attr->va_compressed.va_rawsize + VARHDRSZ;
	}
	else if (VARATT_IS_SHORT(attr))
	{
		/*
		 * we have to normalize the header length to VARHDRSZ or else the
		 * callers of this function will be confused.
		 */
		result = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT + VARHDRSZ;
	}
	else
	{
		/* plain untoasted datum */
		result = VARSIZE(attr);
	}
	return result;
}

/* ----------
 * toast_datum_size
 *
 *	Return the physical storage size (possibly compressed) of a varlena datum
 * ----------
 */
Size
toast_datum_size(Datum value)
{
	varattrib *attr = (varattrib *) DatumGetPointer(value);
	Size		result;

	if (VARATT_IS_EXTERNAL(attr))
	{
		/*
		 * Attribute is stored externally - return the extsize whether
		 * compressed or not.  We do not count the size of the toast pointer
		 * ... should we?
		 */
		result = attr->va_external.va_extsize;
	}
	else if (VARATT_IS_SHORT(attr))
	{
		result = VARSIZE_SHORT(attr);
	}
	else
	{
		/*
		 * Attribute is stored inline either compressed or not, just calculate
		 * the size of the datum in either case.
		 */
		result = VARSIZE(attr);
	}
	return result;
}


/* ----------
 * toast_delete -
 *
 *	Cascaded delete toast-entries on DELETE
 * ----------
 */
void
toast_delete(Relation rel, HeapTuple oldtup, MemTupleBinding *pbind)
{
	TupleDesc	tupleDesc;
	Form_pg_attribute *att;
	int			numAttrs;
	int			i;
	Datum		toast_values[MaxHeapAttributeNumber];
	bool		toast_isnull[MaxHeapAttributeNumber];
	bool 		ismemtuple = is_heaptuple_memtuple(oldtup);
	
	AssertImply(ismemtuple, pbind);
	AssertImply(!ismemtuple, !pbind);

	/*
	 * We should only ever be called for tuples of plain relations ---
	 * recursing on a toast rel is bad news.
	 */
	Assert(rel->rd_rel->relkind == RELKIND_RELATION);

	/*
	 * Get the tuple descriptor and break down the tuple into fields.
	 *
	 * NOTE: it's debatable whether to use heap_deform_tuple() here or just
	 * heap_getattr() only the varlena columns.  The latter could win if there
	 * are few varlena columns and many non-varlena ones. However,
	 * heap_deform_tuple costs only O(N) while the heap_getattr way would cost
	 * O(N^2) if there are many varlena columns, so it seems better to err on
	 * the side of linear cost.  (We won't even be here unless there's at
	 * least one varlena column, by the way.)
	 */
	tupleDesc = rel->rd_att;
	att = tupleDesc->attrs;
	numAttrs = tupleDesc->natts;

	Assert(numAttrs <= MaxHeapAttributeNumber);

	if(ismemtuple)
		memtuple_deform((MemTuple) oldtup, pbind, toast_values, toast_isnull);
	else
		heap_deform_tuple(oldtup, tupleDesc, toast_values, toast_isnull);

	/*
	 * Check for external stored attributes and delete them from the secondary
	 * relation.
	 */
	for (i = 0; i < numAttrs; i++)
	{
		if (att[i]->attlen == -1)
		{
			Datum		value = toast_values[i];

			if (!toast_isnull[i] && VARATT_IS_EXTERNAL_D(value))
				toast_delete_datum(rel, value);
		}
	}
}


/* ----------
 * toast_insert_or_update -
 *
 *	Delete no-longer-used toast-entries and create new ones to
 *	make the new tuple fit on INSERT or UPDATE
 *
 * Inputs:
 *	newtup: the candidate new tuple to be inserted
 *	oldtup: the old row version for UPDATE, or NULL for INSERT
 *	use_wal, use_fsm: flags to be passed to heap_insert() for toast rows
 * Result:
 *	either newtup if no toasting is needed, or a palloc'd modified tuple
 *	that is what should actually get stored
 *
 * NOTE: neither newtup nor oldtup will be modified.  This is a change
 * from the pre-8.1 API of this routine.
 * ----------
 */

static int compute_dest_tuplen(TupleDesc tupdesc, MemTupleBinding *pbind, bool hasnull, Datum *d, bool *isnull)
{
	if(pbind) 
	{
		uint32 nullsave_dummy;
		return (int) compute_memtuple_size(pbind, d, isnull, hasnull, &nullsave_dummy, true /* aligned */);
	}

	return heap_compute_data_size(tupdesc, d, isnull);
}


HeapTuple
toast_insert_or_update(Relation rel, HeapTuple newtup, HeapTuple oldtup, 
					   MemTupleBinding *pbind, int toast_tuple_target,
					   bool isFrozen, bool use_wal, bool use_fsm)
{
	HeapTuple	result_tuple;
	TupleDesc	tupleDesc;
	Form_pg_attribute *att;
	int			numAttrs;
	int			i;

	bool		need_change = false;
	bool		need_free = false;
	bool		need_delold = false;
	bool		has_nulls = false;

	Size		maxDataLen;

	char		toast_action[MaxHeapAttributeNumber];
	bool		toast_isnull[MaxHeapAttributeNumber];
	bool		toast_oldisnull[MaxHeapAttributeNumber];
	Datum		toast_values[MaxHeapAttributeNumber];
	Datum		toast_oldvalues[MaxHeapAttributeNumber];
	int32		toast_sizes[MaxHeapAttributeNumber];
	bool		toast_free[MaxHeapAttributeNumber];
	bool		toast_delold[MaxHeapAttributeNumber];

	bool 		ismemtuple = is_heaptuple_memtuple(newtup);

	AssertImply(ismemtuple, pbind);
	AssertImply(!ismemtuple, !pbind);
	AssertImply(ismemtuple && oldtup, is_heaptuple_memtuple(oldtup));
	Assert(toast_tuple_target > 0);
	
	/*
	 * We should only ever be called for tuples of plain relations ---
	 * recursing on a toast rel is bad news.
	 */
	//Assert(rel->rd_rel->relkind == RELKIND_RELATION);
	if (rel->rd_rel->relkind != RELKIND_RELATION)
		elog(LOG,"Why are we toasting a non-relation! %c ",rel->rd_rel->relkind);

	/*
	 * Get the tuple descriptor and break down the tuple(s) into fields.
	 */
	tupleDesc = rel->rd_att;
	att = tupleDesc->attrs;
	numAttrs = tupleDesc->natts;

	Assert(numAttrs <= MaxHeapAttributeNumber);

	if(ismemtuple)
		memtuple_deform((MemTuple) newtup, pbind, toast_values, toast_isnull);
	else
		heap_deform_tuple(newtup, tupleDesc, toast_values, toast_isnull);

	if (oldtup != NULL)
	{
		if(ismemtuple)
			memtuple_deform((MemTuple) oldtup, pbind, toast_oldvalues, toast_oldisnull);
		else
			heap_deform_tuple(oldtup, tupleDesc, toast_oldvalues, toast_oldisnull);
	}
	/* ----------
	 * Then collect information about the values given
	 *
	 * NOTE: toast_action[i] can have these values:
	 *		' '		default handling
	 *		'p'		already processed --- don't touch it
	 *		'x'		incompressible, but OK to move off
	 *
	 * NOTE: toast_sizes[i] is only made valid for varlena attributes with
	 *		toast_action[i] different from 'p'.
	 * ----------
	 */
	memset(toast_action, ' ', numAttrs * sizeof(char));
	memset(toast_free, 0, numAttrs * sizeof(bool));
	memset(toast_delold, 0, numAttrs * sizeof(bool));

	for (i = 0; i < numAttrs; i++)
	{
		varattrib *old_value;
		varattrib *new_value;

		if (oldtup != NULL)
		{
			/*
			 * For UPDATE get the old and new values of this attribute
			 */
			old_value = (varattrib *) DatumGetPointer(toast_oldvalues[i]);
			new_value = (varattrib *) DatumGetPointer(toast_values[i]);

			/*
			 * If the old value is an external stored one, check if it has
			 * changed so we have to delete it later.
			 */
			if (att[i]->attlen == -1 && !toast_oldisnull[i] &&
				VARATT_IS_EXTERNAL(old_value))
			{
				if (toast_isnull[i] || !VARATT_IS_EXTERNAL(new_value) ||
					memcmp((char *) old_value, (char *) new_value,
						   VARSIZE_EXTERNAL(old_value)) != 0)
				{
					/*
					 * The old external stored value isn't needed any more
					 * after the update
					 */
					toast_delold[i] = true;
					need_delold = true;
				}
				else
				{
					/*
					 * This attribute isn't changed by this update so we reuse
					 * the original reference to the old value in the new
					 * tuple.
					 */
					toast_action[i] = 'p';
					continue;
				}
			}
		}
		else
		{
			/*
			 * For INSERT simply get the new value
			 */
			new_value = (varattrib *) DatumGetPointer(toast_values[i]);
		}

		/*
		 * Handle NULL attributes
		 */
		if (toast_isnull[i])
		{
			toast_action[i] = 'p';
			has_nulls = true;
			continue;
		}

		/*
		 * Now look at varlena attributes
		 */
		if (att[i]->attlen == -1)
		{
			/*
			 * If the table's attribute says PLAIN always, force it so.
			 */
			if (att[i]->attstorage == 'p')
				toast_action[i] = 'p';

			/*
			 * We took care of UPDATE above, so any external value we find
			 * still in the tuple must be someone else's we cannot reuse.
			 * Fetch it back (without decompression, unless we are forcing
			 * PLAIN storage).	If necessary, we'll push it out as a new
			 * external value below.
			 */
			if (VARATT_IS_EXTERNAL(new_value))
			{
				if (att[i]->attstorage == 'p')
					new_value = (varattrib *)heap_tuple_untoast_attr((struct varlena *)new_value);
				else
					new_value = (varattrib *)heap_tuple_fetch_attr((struct varlena *)new_value);
				toast_values[i] = PointerGetDatum(new_value);
				toast_free[i] = true;
				need_change = true;
				need_free = true;
			}

			/*
			 * Remember the size of this attribute
			 */
			toast_sizes[i] = VARSIZE_ANY(new_value);
		}
		else
		{
			/*
			 * Not a varlena attribute, plain storage always
			 */
			toast_action[i] = 'p';
		}
	}

	/* ----------
	 * Compress and/or save external until data fits into target length
	 *
	 *	1: Inline compress attributes with attstorage 'x', and store very
	 *	   large attributes with attstorage 'x' or 'e' external immediately
	 *	2: Store attributes with attstorage 'x' or 'e' external
	 *	3: Inline compress attributes with attstorage 'm'
	 *	4: Store attributes with attstorage 'm' external
	 * ----------
	 */

	if(!ismemtuple)
	{
		/* compute header overhead --- this should match heap_form_tuple() */
		maxDataLen = offsetof(HeapTupleHeaderData, t_bits);
		if (has_nulls)
			maxDataLen += BITMAPLEN(numAttrs);
		if (newtup->t_data->t_infomask & HEAP_HASOID)
			maxDataLen += sizeof(Oid);
		maxDataLen = MAXALIGN(maxDataLen);
		Assert(maxDataLen == newtup->t_data->t_hoff);
		/* now convert to a limit on the tuple data size */
		maxDataLen = toast_tuple_target - maxDataLen;
	}
	else
		maxDataLen = toast_tuple_target;

	/*
	 * Look for attributes with attstorage 'x' to compress.  Also find large
	 * attributes with attstorage 'x' or 'e', and store them external.
	 */
	while (compute_dest_tuplen(tupleDesc, pbind, has_nulls,
							   toast_values, toast_isnull) > maxDataLen)
	{
		int			biggest_attno = -1;
		int32		biggest_size = MAXALIGN(sizeof(varattrib));
		Datum		old_value;
		Datum		new_value;

		/*
		 * Search for the biggest yet unprocessed internal attribute
		 */
		for (i = 0; i < numAttrs; i++)
		{
			if (toast_action[i] != ' ')
				continue;
			if (VARATT_IS_EXTERNAL_D(toast_values[i]))
				continue;
			if (VARATT_IS_COMPRESSED_D(toast_values[i]))
				continue;
			if (att[i]->attstorage != 'x')
				continue;
			if (toast_sizes[i] > biggest_size)
			{
				biggest_attno = i;
				biggest_size = toast_sizes[i];
			}
		}

		if (biggest_attno < 0)
			break;

		/*
		 * Attempt to compress it inline, if it has attstorage 'x'
		 */
		i = biggest_attno;
		old_value = toast_values[i];
		new_value = toast_compress_datum(old_value);

		if (DatumGetPointer(new_value) != NULL)
		{
			/* successful compression */
			if (toast_free[i])
				pfree(DatumGetPointer(old_value));
			toast_values[i] = new_value;
			toast_free[i] = true;
			toast_sizes[i] = VARSIZE_D(toast_values[i]);
			need_change = true;
			need_free = true;
		}
		else
		{
			/*
			 * incompressible data, ignore on subsequent compression passes
			 */
			toast_action[i] = 'x';
		}
	}

	/*
	 * Second we look for attributes of attstorage 'x' or 'e' that are still
	 * inline.
	 */
	while (compute_dest_tuplen(tupleDesc, pbind, has_nulls,
							   toast_values, toast_isnull) > maxDataLen &&
		   rel->rd_rel->reltoastrelid != InvalidOid)
	{
		int			biggest_attno = -1;
		int32		biggest_size = MAXALIGN(sizeof(varattrib));
		Datum		old_value;

		/*------
		 * Search for the biggest yet inlined attribute with
		 * attstorage equals 'x' or 'e'
		 *------
		 */
		for (i = 0; i < numAttrs; i++)
		{
			if (toast_action[i] == 'p')
				continue;
			if (VARATT_IS_EXTERNAL_D(toast_values[i]))
				continue;
			if (att[i]->attstorage != 'x' && att[i]->attstorage != 'e')
				continue;
			if (toast_sizes[i] > biggest_size)
			{
				biggest_attno = i;
				biggest_size = toast_sizes[i];
			}
		}

		if (biggest_attno < 0)
			break;

		/*
		 * Store this external
		 */
		i = biggest_attno;
		old_value = toast_values[i];
		toast_action[i] = 'p';
		toast_values[i] = toast_save_datum(rel, toast_values[i], isFrozen, use_wal, use_fsm);
		if (toast_free[i])
			pfree(DatumGetPointer(old_value));
		toast_free[i] = true;

		need_change = true;
		need_free = true;
	}

	/*
	 * Round 3 - this time we take attributes with storage 'm' into
	 * compression
	 */
	while (compute_dest_tuplen(tupleDesc, pbind, has_nulls,
							   toast_values, toast_isnull) > maxDataLen)
	{
		int			biggest_attno = -1;
		int32		biggest_size = MAXALIGN(sizeof(varattrib));
		Datum		old_value;
		Datum		new_value;

		/*
		 * Search for the biggest yet uncompressed internal attribute
		 */
		for (i = 0; i < numAttrs; i++)
		{
			if (toast_action[i] != ' ')
				continue;
			if (VARATT_IS_EXTERNAL_D(toast_values[i]))
				continue;		/* can't happen, toast_action would be 'p' */
			if (VARATT_IS_COMPRESSED_D(toast_values[i]))
				continue;
			if (att[i]->attstorage != 'm')
				continue;
			if (toast_sizes[i] > biggest_size)
			{
				biggest_attno = i;
				biggest_size = toast_sizes[i];
			}
		}

		if (biggest_attno < 0)
			break;

		/*
		 * Attempt to compress it inline
		 */
		i = biggest_attno;
		old_value = toast_values[i];
		new_value = toast_compress_datum(old_value);

		if (DatumGetPointer(new_value) != NULL)
		{
			/* successful compression */
			if (toast_free[i])
				pfree(DatumGetPointer(old_value));
			toast_values[i] = new_value;
			toast_free[i] = true;
			toast_sizes[i] = VARSIZE_D(toast_values[i]);
			need_change = true;
			need_free = true;
		}
		else
		{
			/* incompressible, ignore on subsequent compression passes */
			toast_action[i] = 'x';
		}
	}

	/*
	 * Finally we store attributes of type 'm' external, if possible.
	 */
	while (compute_dest_tuplen(tupleDesc, pbind, has_nulls,
							   toast_values, toast_isnull) > maxDataLen &&
		   rel->rd_rel->reltoastrelid != InvalidOid)
	{
		int			biggest_attno = -1;
		int32		biggest_size = MAXALIGN(sizeof(varattrib));
		Datum		old_value;

		/*--------
		 * Search for the biggest yet inlined attribute with
		 * attstorage = 'm'
		 *--------
		 */
		for (i = 0; i < numAttrs; i++)
		{
			if (toast_action[i] == 'p')
				continue;
			if (VARATT_IS_EXTERNAL_D(toast_values[i]))
				continue;		/* can't happen, toast_action would be 'p' */
			if (att[i]->attstorage != 'm')
				continue;
			if (toast_sizes[i] > biggest_size)
			{
				biggest_attno = i;
				biggest_size = toast_sizes[i];
			}
		}

		if (biggest_attno < 0)
			break;

		/*
		 * Store this external
		 */
		i = biggest_attno;
		old_value = toast_values[i];
		toast_action[i] = 'p';
		toast_values[i] = toast_save_datum(rel, toast_values[i], isFrozen, use_wal, use_fsm);
		if (toast_free[i])
			pfree(DatumGetPointer(old_value));
		toast_free[i] = true;

		need_change = true;
		need_free = true;
	}

	/* XXX Maybe we should check here for any compressed inline attributes that
	 * didn't save enough to warrant keeping. In particular attributes whose
	 * rawsize is < 128 bytes and didn't save at least 3 bytes... or even maybe
	 * more given alignment issues 
	 */

	/*
	 * In the case we toasted any values, we need to build a new heap tuple
	 * with the changed values.
	 */
	if (need_change)
	{
		if(ismemtuple)
			result_tuple = (HeapTuple) memtuple_form_to(pbind, toast_values, toast_isnull, NULL, NULL, false);
		else
		{
			HeapTupleHeader olddata = newtup->t_data;
			HeapTupleHeader new_data;
			int32		new_len;

			/*
			 * Calculate the new size of the tuple.  Header size should not
			 * change, but data size might.
			 */
			new_len = offsetof(HeapTupleHeaderData, t_bits);
			if (has_nulls)
				new_len += BITMAPLEN(numAttrs);
			if (olddata->t_infomask & HEAP_HASOID)
				new_len += sizeof(Oid);
			new_len = MAXALIGN(new_len);
			Assert(new_len == olddata->t_hoff);
			new_len += heap_compute_data_size(tupleDesc,
					toast_values, toast_isnull);

			/*
			 * Allocate and zero the space needed, and fill HeapTupleData fields.
			 */
			result_tuple = (HeapTuple) palloc0(HEAPTUPLESIZE + new_len);
			result_tuple->t_len = new_len;
			result_tuple->t_self = newtup->t_self;
			new_data = (HeapTupleHeader) ((char *) result_tuple + HEAPTUPLESIZE);
			result_tuple->t_data = new_data;

			/*
			 * Put the existing tuple header and the changed values into place
			 */
			memcpy(new_data, olddata, olddata->t_hoff);

			heap_fill_tuple(tupleDesc,
					toast_values,
					toast_isnull,
					(char *) new_data + olddata->t_hoff,
					&(new_data->t_infomask),
					has_nulls ? new_data->t_bits : NULL);
		}
	}
	else
		result_tuple = newtup;

	/*
	 * Free allocated temp values
	 */
	if (need_free)
		for (i = 0; i < numAttrs; i++)
			if (toast_free[i])
				pfree(DatumGetPointer(toast_values[i]));

	/*
	 * Delete external values from the old tuple
	 */
	if (need_delold)
		for (i = 0; i < numAttrs; i++)
			if (toast_delold[i])
				toast_delete_datum(rel, toast_oldvalues[i]);

	return result_tuple;
}


/* ----------
 * toast_flatten_tuple_attribute -
 *
 *	If a Datum is of composite type, "flatten" it to contain no toasted fields.
 *	This must be invoked on any potentially-composite field that is to be
 *	inserted into a tuple.	Doing this preserves the invariant that toasting
 *	goes only one level deep in a tuple.
 *
 *	Note that flattening does not mean expansion of short-header varlenas,
 *	so in one sense toasting is allowed within composite datums.
 * ----------
 */
Datum
toast_flatten_tuple_attribute(Datum value,
							  Oid typeId, int32 typeMod)
{
	TupleDesc	tupleDesc;
	HeapTupleHeader olddata;
	HeapTupleHeader new_data;
	int32		new_len;
	HeapTupleData tmptup;
	Form_pg_attribute *att;
	int			numAttrs;
	int			i;
	bool		need_change = false;
	bool		has_nulls = false;
	Datum		toast_values[MaxTupleAttributeNumber];
	bool		toast_isnull[MaxTupleAttributeNumber];
	bool		toast_free[MaxTupleAttributeNumber];

	/*
	 * See if it's a composite type, and get the tupdesc if so.
	 */
	tupleDesc = lookup_rowtype_tupdesc_noerror(typeId, typeMod, true);
	if (tupleDesc == NULL)
		return value;			/* not a composite type */

	att = tupleDesc->attrs;
	numAttrs = tupleDesc->natts;

	/*
	 * Break down the tuple into fields.
	 */
	olddata = DatumGetHeapTupleHeader(value);
	Assert(typeId == HeapTupleHeaderGetTypeId(olddata));
	Assert(typeMod == HeapTupleHeaderGetTypMod(olddata));
	/* Build a temporary HeapTuple control structure */
	tmptup.t_len = HeapTupleHeaderGetDatumLength(olddata);
	ItemPointerSetInvalid(&(tmptup.t_self));
	tmptup.t_data = olddata;

	Assert(numAttrs <= MaxTupleAttributeNumber);
	heap_deform_tuple(&tmptup, tupleDesc, toast_values, toast_isnull);

	memset(toast_free, 0, numAttrs * sizeof(bool));

	for (i = 0; i < numAttrs; i++)
	{
		/*
		 * Look at non-null varlena attributes
		 */
		if (toast_isnull[i])
			has_nulls = true;
		else if (att[i]->attlen == -1)
		{
			varattrib *new_value;

			new_value = (varattrib *) DatumGetPointer(toast_values[i]);
			if (VARATT_IS_EXTERNAL(new_value) || VARATT_IS_COMPRESSED(new_value))
			{
				new_value = (varattrib *)heap_tuple_untoast_attr((struct varlena *)new_value);
				toast_values[i] = PointerGetDatum(new_value);
				toast_free[i] = true;
				need_change = true;
			}
		}
	}

	/*
	 * If nothing to untoast, just return the original tuple.
	 */
	if (!need_change)
	{
		ReleaseTupleDesc(tupleDesc);
		return value;
	}

	/*
	 * Calculate the new size of the tuple.  Header size should not change,
	 * but data size might.
	 */
	new_len = offsetof(HeapTupleHeaderData, t_bits);
	if (has_nulls)
		new_len += BITMAPLEN(numAttrs);
	if (olddata->t_infomask & HEAP_HASOID)
		new_len += sizeof(Oid);
	new_len = MAXALIGN(new_len);
	Assert(new_len == olddata->t_hoff);
	new_len += heap_compute_data_size(tupleDesc, toast_values, toast_isnull);

	new_data = (HeapTupleHeader) palloc0(new_len);

	/*
	 * Put the tuple header and the changed values into place
	 */
	memcpy(new_data, olddata, olddata->t_hoff);

	HeapTupleHeaderSetDatumLength(new_data, new_len);

	heap_fill_tuple(tupleDesc,
					toast_values,
					toast_isnull,
					(char *) new_data + olddata->t_hoff,
					&(new_data->t_infomask),
					has_nulls ? new_data->t_bits : NULL);

	/*
	 * Free allocated temp values
	 */
	for (i = 0; i < numAttrs; i++)
		if (toast_free[i])
			pfree(DatumGetPointer(toast_values[i]));
	ReleaseTupleDesc(tupleDesc);

	return PointerGetDatum(new_data);
}


/* ----------
 * toast_compress_datum -
 *
 *	Create a compressed version of a varlena datum
 *
 *	If we fail (ie, compressed result is actually bigger than original)
 *	then return NULL.  We must not use compressed data if it'd expand
 *	the tuple!
 *
 *	We use VAR{SIZE,DATA}_ANY so we can handle short varlenas here without
 *	copying them.  But we can't handle external or compressed datums.
 * ----------
 */
Datum
toast_compress_datum(Datum value)
{
	varattrib  *tmp;
	int32		valsize = VARSIZE_ANY_EXHDR_D(value);

	Assert(!VARATT_IS_EXTERNAL(DatumGetPointer(value)));
	Assert(!VARATT_IS_COMPRESSED(DatumGetPointer(value)));

	/*
	 * No point in wasting a palloc cycle if value size is out of the allowed
	 * range for compression
	 */
	if (valsize < PGLZ_strategy_default->min_input_size ||
		valsize > PGLZ_strategy_default->max_input_size)
		return PointerGetDatum(NULL);
		
	tmp = (varattrib *) palloc(PGLZ_MAX_OUTPUT(valsize));
	if (pglz_compress(VARDATA_ANY_D(value), valsize,
					  (PGLZ_Header *) tmp, PGLZ_strategy_default) &&
		VARSIZE(tmp) < VARSIZE_ANY_D(value))
	{
		/* successful compression */
		VARATT_SET_COMPRESSED(tmp);
		return PointerGetDatum(tmp);
	}
	else
	{
		/* incompressible data */
		pfree(tmp);
		return PointerGetDatum(NULL);
	}
}


/* ----------
 * toast_save_datum -
 *
 *	Save one single datum into the secondary relation and return
 *	a Datum reference for it.
 * ----------
 */
static Datum
toast_save_datum(Relation rel, Datum value, bool isFrozen, bool use_wal, bool use_fsm)
{
	Relation	toastrel;
	Relation	toastidx;
	HeapTuple	toasttup;
	TupleDesc	toasttupDesc;
	Datum		t_values[3];
	bool		t_isnull[3];
	TransactionId myxid = GetCurrentTransactionId();
	CommandId	mycid = GetCurrentCommandId();
	varattrib  *result;
	struct
	{
		struct varlena hdr;
		char		data[TOAST_MAX_CHUNK_SIZE]; /* make struct big enough */
		int32		align_it;	/* ensure struct is aligned well enough */
	}			chunk_data;
	int32		chunk_size;
	int32		chunk_seq = 0;
	char	   *data_p;
	int32		data_todo;
	int32		rawsize, extsize;

	/*
	 * Open the toast relation and its index.  We can use the index to check
	 * uniqueness of the OID we assign to the toasted item, even though it has
	 * additional columns besides OID.
	 */
	toastrel = heap_open(rel->rd_rel->reltoastrelid, RowExclusiveLock);
	toasttupDesc = toastrel->rd_att;
	toastidx = index_open(toastrel->rd_rel->reltoastidxid, RowExclusiveLock);

	/*
	 * Create the varattrib reference
	 */
	result = (varattrib *) palloc(sizeof(varattrib));

	/* rawsize is the size of the datum that will result after decompression --
	 * including the full header. so we have to adjust for short headers.
	 *
	 * extsize is the actual size of the data payload in the toast records
	 * without any headers
	 */
	if (VARATT_IS_SHORT_D(value)) 
	{
		rawsize = VARSIZE_SHORT_D(value) - VARHDRSZ_SHORT + VARHDRSZ;
		extsize = VARSIZE_SHORT_D(value) - VARHDRSZ_SHORT;
		data_p = VARDATA_SHORT_D(value);
		data_todo = VARSIZE_SHORT_D(value) - VARHDRSZ_SHORT;
	}
	else if (VARATT_IS_COMPRESSED_D(value))
	{
		/* rawsize in a compressed datum is the just the size of the payload */
		rawsize = ((varattrib *) DatumGetPointer(value))->va_compressed.va_rawsize + VARHDRSZ;
		extsize = VARSIZE_D(value) - VARHDRSZ;
		data_p = VARDATA_D(value);
		data_todo = VARSIZE_D(value) - VARHDRSZ;
		/* 	we used to set result->va_header |= VARATT_FLAG_COMPRESSED; down
		 * 	below. we don't any longer and depend on the equality holding:
		 * 	extsize = rawsize + VARHDRSZ*/
	}
	else 
	{
		rawsize = VARSIZE_D(value);
		extsize = VARSIZE_D(value) - VARHDRSZ;
		data_p = VARDATA_D(value);
		data_todo = VARSIZE_D(value) - VARHDRSZ;
	}
	
	SET_VARSIZE_EXTERNAL(result, TOAST_POINTER_SIZE);
	result->va_external.va_rawsize = rawsize;
	result->va_external.va_extsize = extsize;
	result->va_external.va_valueid = GetNewOidWithIndex(toastrel, toastidx);
	result->va_external.va_toastrelid = rel->rd_rel->reltoastrelid;

#ifdef USE_ASSERT_CHECKING
	Assert( (VARATT_IS_COMPRESSED_D(value)||0) == (VARATT_EXTERNAL_IS_COMPRESSED(result)||0) );

	if (VARATT_IS_COMPRESSED_D(value)) 
	{
		Assert(VARATT_EXTERNAL_IS_COMPRESSED(result));
		elog(DEBUG4,
			 "saved toast datum, original varsize %ud rawsize %ud new extsize %ud rawsize %uld\n", 
			 VARSIZE_D(value), ((varattrib *) DatumGetPointer(value))->va_compressed.va_rawsize,
			 result->va_external.va_extsize, result->va_external.va_rawsize);
	}
	else
	{
		Assert(!VARATT_EXTERNAL_IS_COMPRESSED(result));
		elog(DEBUG4,
			 "saved toast datum, original varsize %ud new extsize %ud rawsize %ud\n", 
			 VARSIZE_D(value),
			 result->va_external.va_extsize, result->va_external.va_rawsize);
	}
#endif

	/*
	 * Initialize constant parts of the tuple data
	 */
	t_values[0] = ObjectIdGetDatum(result->va_external.va_valueid);
	t_values[2] = PointerGetDatum(&chunk_data);
	t_isnull[0] = false;
	t_isnull[1] = false;
	t_isnull[2] = false;

	/*
	 * Split up the item into chunks
	 */
	while (data_todo > 0)
	{
		/*
		 * Calculate the size of this chunk
		 */
		chunk_size = Min(TOAST_MAX_CHUNK_SIZE, data_todo);

		/*
		 * Build a tuple and store it
		 */
		t_values[1] = Int32GetDatum(chunk_seq++);
		SET_VARSIZE(&chunk_data, chunk_size + VARHDRSZ);
		memcpy(VARDATA(&chunk_data), data_p, chunk_size);
		toasttup = heap_form_tuple(toasttupDesc, t_values, t_isnull);
		if (!HeapTupleIsValid(toasttup))
			elog(ERROR, "failed to build TOAST tuple");

		if(!isFrozen)
		{
			/* the normal case. regular insert */
			heap_insert(toastrel, toasttup, mycid, use_wal, use_fsm, myxid);
		}
		else
		{
			/* insert and freeze the tuple. used for errtables and their related toast data */
			frozen_heap_insert(toastrel, toasttup);
		}
			
		//heap_insert(relation, tup, GetCurrentCommandId(),
		//			   true, true, GetCurrentTransactionId());

		/*
		 * Create the index entry.	We cheat a little here by not using
		 * FormIndexDatum: this relies on the knowledge that the index columns
		 * are the same as the initial columns of the table.
		 *
		 * Note also that there had better not be any user-created index on
		 * the TOAST table, since we don't bother to update anything else.
		 */
		index_insert(toastidx, t_values, t_isnull,
					 &(toasttup->t_self),
					 toastrel, toastidx->rd_index->indisunique);

		/*
		 * Free memory
		 */
		heap_freetuple(toasttup);

		/*
		 * Move on to next chunk
		 */
		data_todo -= chunk_size;
		data_p += chunk_size;
	}

	/*
	 * Done - close toast relation
	 */
	index_close(toastidx, RowExclusiveLock);
	heap_close(toastrel, RowExclusiveLock);

	return PointerGetDatum(result);
}


/* ----------
 * toast_delete_datum -
 *
 *	Delete a single external stored value.
 * ----------
 */
static void
toast_delete_datum(Relation rel __attribute__((unused)), Datum value)
{
	varattrib  *attr = (varattrib *) DatumGetPointer(value);
	Relation	toastrel;
	Relation	toastidx;
	ScanKeyData toastkey;
	IndexScanDesc toastscan;
	HeapTuple	toasttup;

	if (!VARATT_IS_EXTERNAL(attr))
		return;

	/*
	 * Open the toast relation and its index
	 */
	toastrel = heap_open(attr->va_external.va_toastrelid,
						 RowExclusiveLock);
	toastidx = index_open(toastrel->rd_rel->reltoastidxid, RowExclusiveLock);

	/*
	 * Setup a scan key to fetch from the index by va_valueid (we don't
	 * particularly care whether we see them in sequence or not)
	 */
	ScanKeyInit(&toastkey,
				(AttrNumber) 1,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(attr->va_external.va_valueid));

	/*
	 * Find all the chunks.  (We don't actually care whether we see them in
	 * sequence or not, but since we've already locked the index we might as
	 * well use systable_beginscan_ordered.)
	 */
	toastscan = index_beginscan(toastrel, toastidx,
								SnapshotToast, 1, &toastkey);
	while ((toasttup = index_getnext(toastscan, ForwardScanDirection)) != NULL)
	{
		/*
		 * Have a chunk, delete it
		 */
		simple_heap_delete(toastrel, &toasttup->t_self);
	}

	/*
	 * End scan and close relations
	 */
	index_endscan(toastscan);
	index_close(toastidx, RowExclusiveLock);
	heap_close(toastrel, RowExclusiveLock);
}


/* ----------
 * toast_fetch_datum -
 *
 *	Reconstruct an in memory Datum from the chunks saved
 *	in the toast relation
 * ----------
 */
static struct varlena *
toast_fetch_datum(struct varlena *attr)
{
	Relation	toastrel;
	Relation	toastidx;
	ScanKeyData toastkey;
	IndexScanDesc toastscan;
	HeapTuple	ttup;
	TupleDesc	toasttupDesc;
	varattrib  *result;
	int32		ressize;
	int32		residx,
				nextidx;
	int32		numchunks;
	Pointer		chunk;
	bool		isnull;
	int32		chunksize;
	void 	   *chunkdata;

	ressize = ((varattrib *)attr)->va_external.va_extsize;
	numchunks = ((ressize - 1) / TOAST_MAX_CHUNK_SIZE) + 1;

	result = (varattrib *) palloc(ressize + VARHDRSZ);
	SET_VARSIZE(result, ressize + VARHDRSZ);
	if (VARATT_EXTERNAL_IS_COMPRESSED(attr))
		VARATT_SET_COMPRESSED(result);
	
	/*
	 * Open the toast relation and its index
	 */
	toastrel = heap_open(((varattrib *)attr)->va_external.va_toastrelid, AccessShareLock);
	toasttupDesc = toastrel->rd_att;
	toastidx = index_open(toastrel->rd_rel->reltoastidxid, AccessShareLock);

	/*
	 * Setup a scan key to fetch from the index by va_valueid
	 */
	ScanKeyInit(&toastkey,
				(AttrNumber) 1,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(((varattrib *)attr)->va_external.va_valueid));

	/*
	 * Read the chunks by index
	 *
	 * Note that because the index is actually on (valueid, chunkidx) we will
	 * see the chunks in chunkidx order, even though we didn't explicitly ask
	 * for it.
	 */
	nextidx = 0;

	toastscan = index_beginscan(toastrel, toastidx,
								SnapshotToast, 1, &toastkey);
	while ((ttup = index_getnext(toastscan, ForwardScanDirection)) != NULL)
	{
		/*
		 * Have a chunk, extract the sequence number and the data
		 */
		residx = DatumGetInt32(fastgetattr(ttup, 2, toasttupDesc, &isnull));
		Assert(!isnull);
		chunk = DatumGetPointer(fastgetattr(ttup, 3, toasttupDesc, &isnull));
		Assert(!isnull);
		if (VARATT_IS_SHORT(chunk)) 
		{
			chunksize = VARSIZE_SHORT(chunk) - VARHDRSZ_SHORT;
			chunkdata = VARDATA_SHORT(chunk);
		}
		else if (!VARATT_IS_EXTENDED(chunk)) 
		{
			chunksize = VARSIZE(chunk) - VARHDRSZ;
			chunkdata = VARDATA(chunk);
		}
		else 
		{
			elog(ERROR, "found toasted toast chunk?");
			chunksize = 0; /* shut compiler up */
			chunkdata = NULL;
		}

		/*
		 * Some checks on the data we've found
		 */
		if (residx != nextidx)
			elog(ERROR, "unexpected chunk number %d (expected %d) for toast value %u",
				 residx, nextidx,
				 ((varattrib *)attr)->va_external.va_valueid);
		if (residx < numchunks - 1)
		{
			if (chunksize != TOAST_MAX_CHUNK_SIZE)
				elog(ERROR, "unexpected chunk size %d in chunk %d of %d for toast value %u (expected %d)",
					 chunksize, residx,
					 ((varattrib *)attr)->va_external.va_valueid, numchunks-1,
					 (int)TOAST_MAX_CHUNK_SIZE);
		}
		else if (residx == numchunks-1)
		{
			if ((residx * TOAST_MAX_CHUNK_SIZE + chunksize) != ressize)
				elog(ERROR, "unexpected chunk size %d in final chunk %d for toast value %u (expected %d)",
					 chunksize, residx,
					 ((varattrib *)attr)->va_external.va_valueid,
					 ressize - residx*(int)TOAST_MAX_CHUNK_SIZE);
		}
		else
			elog(ERROR, "unexpected chunk number %d for toast value %u (expected in %d..%d)",
				 residx,
				 ((varattrib *)attr)->va_external.va_valueid,
				 0, numchunks-1);

		/*
		 * Copy the data into proper place in our result
		 */
		memcpy(((char *) VARDATA(result)) + residx * TOAST_MAX_CHUNK_SIZE,
			   chunkdata,
			   chunksize);

		nextidx++;
	}

	/*
	 * Final checks that we successfully fetched the datum
	 */
	if (nextidx != numchunks)
		elog(ERROR, "missing chunk number %d for toast value %u",
			 nextidx,
			 ((varattrib *)attr)->va_external.va_valueid);

	/*
	 * End scan and close relations
	 */
	index_endscan(toastscan);
	index_close(toastidx, AccessShareLock);
	heap_close(toastrel, AccessShareLock);

	return (struct varlena *)result;
}

/* ----------
 * toast_fetch_datum_slice -
 *
 *	Reconstruct a segment of a Datum from the chunks saved
 *	in the toast relation
 * ----------
 */
static struct varlena *
toast_fetch_datum_slice(struct varlena *attr, int32 sliceoffset, int32 length)
{
	Relation	toastrel;
	Relation	toastidx;
	ScanKeyData toastkey[3];
	int			nscankeys;
	IndexScanDesc toastscan;
	HeapTuple	ttup;
	TupleDesc	toasttupDesc;
	varattrib  *result;
	int32		attrsize;
	int32		residx;
	int32		nextidx;
	int			numchunks;
	int			startchunk;
	int			endchunk;
	int32		startoffset;
	int32		endoffset;
	int			totalchunks;
	Pointer		chunk;
	bool		isnull;
	int32		chunksize;
	int32		chcpystrt;
	int32		chcpyend;

	attrsize = ((varattrib *)attr)->va_external.va_extsize;
	totalchunks = ((attrsize - 1) / TOAST_MAX_CHUNK_SIZE) + 1;

	if (sliceoffset >= attrsize)
	{
		sliceoffset = 0;
		length = 0;
	}

	if (((sliceoffset + length) > attrsize) || length < 0)
		length = attrsize - sliceoffset;

	result = (varattrib *) palloc(length + VARHDRSZ);
	SET_VARSIZE(result, length + VARHDRSZ);

	if (VARATT_EXTERNAL_IS_COMPRESSED(attr))
		VARATT_SET_COMPRESSED(result);

	if (length == 0)
		return (struct varlena *)result;			/* Can save a lot of work at this point! */

	startchunk = sliceoffset / TOAST_MAX_CHUNK_SIZE;
	endchunk = (sliceoffset + length - 1) / TOAST_MAX_CHUNK_SIZE;
	numchunks = (endchunk - startchunk) + 1;

	startoffset = sliceoffset % TOAST_MAX_CHUNK_SIZE;
	endoffset = (sliceoffset + length - 1) % TOAST_MAX_CHUNK_SIZE;

	/*
	 * Open the toast relation and its index
	 */
	toastrel = heap_open(((varattrib *)attr)->va_external.va_toastrelid, AccessShareLock);
	toasttupDesc = toastrel->rd_att;
	toastidx = index_open(toastrel->rd_rel->reltoastidxid, AccessShareLock);

	/*
	 * Setup a scan key to fetch from the index. This is either two keys or
	 * three depending on the number of chunks.
	 */
	ScanKeyInit(&toastkey[0],
				(AttrNumber) 1,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(((varattrib *)attr)->va_external.va_valueid));

	/*
	 * Use equality condition for one chunk, a range condition otherwise:
	 */
	if (numchunks == 1)
	{
		ScanKeyInit(&toastkey[1],
					(AttrNumber) 2,
					BTEqualStrategyNumber, F_INT4EQ,
					Int32GetDatum(startchunk));
		nscankeys = 2;
	}
	else
	{
		ScanKeyInit(&toastkey[1],
					(AttrNumber) 2,
					BTGreaterEqualStrategyNumber, F_INT4GE,
					Int32GetDatum(startchunk));
		ScanKeyInit(&toastkey[2],
					(AttrNumber) 2,
					BTLessEqualStrategyNumber, F_INT4LE,
					Int32GetDatum(endchunk));
		nscankeys = 3;
	}

	/*
	 * Read the chunks by index
	 *
	 * The index is on (valueid, chunkidx) so they will come in order
	 */
	nextidx = startchunk;
	toastscan = index_beginscan(toastrel, toastidx,
								SnapshotToast, nscankeys, toastkey);
	while ((ttup = index_getnext(toastscan, ForwardScanDirection)) != NULL)
	{
		/*
		 * Have a chunk, extract the sequence number and the data
		 */
		residx = DatumGetInt32(fastgetattr(ttup, 2, toasttupDesc, &isnull));
		Assert(!isnull);
		chunk = DatumGetPointer(fastgetattr(ttup, 3, toasttupDesc, &isnull));
		Assert(!isnull);
		if (VARATT_IS_SHORT((varattrib *)chunk))
			chunksize = VARSIZE_SHORT((varattrib *)chunk) - VARHDRSZ_SHORT;
		else if (!VARATT_IS_EXTENDED((varattrib *)chunk))
			chunksize = VARSIZE((varattrib *)chunk) - VARHDRSZ;
		else {
			elog(ERROR, "found toasted toast chunk?");
			chunksize = 0; /* shut compiler up */
		}
		
		
		/*
		 * Some checks on the data we've found
		 */
		if ((residx != nextidx) || (residx > endchunk) || (residx < startchunk))
			elog(ERROR, "unexpected chunk number %d (expected %d) for toast value %u",
				 residx, nextidx,
				 ((varattrib *)attr)->va_external.va_valueid);
		if (residx < totalchunks - 1)
		{
			if (chunksize != TOAST_MAX_CHUNK_SIZE)
				elog(ERROR, "unexpected chunk size %d in chunk %d for toast value %u of %d when fetching slice (expected %d)",
					 chunksize, residx,
					 ((varattrib *)attr)->va_external.va_valueid, totalchunks-1,
					 (int)TOAST_MAX_CHUNK_SIZE);
		}
		else if (residx == totalchunks-1)
		{
			if ((residx * TOAST_MAX_CHUNK_SIZE + chunksize) != attrsize)
				elog(ERROR, "unexpected chunk size %d in chunk %d for final toast value %u when fetching slice (expected %d)",
					 chunksize, residx,
					 ((varattrib *)attr)->va_external.va_valueid,
					 attrsize - residx * (int)TOAST_MAX_CHUNK_SIZE);
		}
		else 
		{
			elog(ERROR, "unexpected chunk");
		}
		

		/*
		 * Copy the data into proper place in our result
		 */
		chcpystrt = 0;
		chcpyend = chunksize - 1;
		if (residx == startchunk)
			chcpystrt = startoffset;
		if (residx == endchunk)
			chcpyend = endoffset;

		memcpy(((char *) VARDATA(result)) +
			   (residx * TOAST_MAX_CHUNK_SIZE - sliceoffset) + chcpystrt,
			   VARDATA((varattrib *)chunk) + chcpystrt,
			   (chcpyend - chcpystrt) + 1);

		nextidx++;
	}

	/*
	 * Final checks that we successfully fetched the datum
	 */
	if (nextidx != (endchunk + 1))
		elog(ERROR, "missing chunk number %d for toast value %u",
			 nextidx,
			 ((varattrib *)attr)->va_external.va_valueid);

	/*
	 * End scan and close relations
	 */
	index_endscan(toastscan);
	index_close(toastidx, AccessShareLock);
	heap_close(toastrel, AccessShareLock);

	return (struct varlena *)result;
}
