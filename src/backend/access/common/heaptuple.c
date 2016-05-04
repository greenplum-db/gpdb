/*-------------------------------------------------------------------------
 *
 * heaptuple.c
 *	  This file contains heap tuple accessor and mutator routines, as well
 *	  as various tuple utilities.
 *
 * Some notes about varlenas and this code:
 *
 * Before Postgres 8.3 varlenas always had a 4-byte length header, and
 * therefore always needed 4-byte alignment (at least).  This wasted space
 * for short varlenas, for example CHAR(1) took 5 bytes and could need up to
 * 3 additional padding bytes for alignment.
 *
 * Now, a short varlena (up to 126 data bytes) is reduced to a 1-byte header
 * and we don't align it.  To hide this from datatype-specific functions that
 * don't want to deal with it, such a datum is considered "toasted" and will
 * be expanded back to the normal 4-byte-header format by pg_detoast_datum.
 * (In performance-critical code paths we can use pg_detoast_datum_packed
 * and the appropriate access macros to avoid that overhead.)  Note that this
 * conversion is performed directly in heap_form_tuple, without invoking
 * tuptoaster.c.
 *
 * This change will break any code that assumes it needn't detoast values
 * that have been put into a tuple but never sent to disk.	Hopefully there
 * are few such places.
 *
 * Varlenas still have alignment 'i' (or 'd') in pg_type/pg_attribute, since
 * that's the normal requirement for the untoasted format.  But we ignore that
 * for the 1-byte-header format.  This means that the actual start position
 * of a varlena datum may vary depending on which format it has.  To determine
 * what is stored, we have to require that alignment padding bytes be zero.
 * (Postgres actually has always zeroed them, but now it's required!)  Since
 * the first byte of a 1-byte-header varlena can never be zero, we can examine
 * the first byte after the previous datum to tell if it's a pad byte or the
 * start of a 1-byte-header varlena.
 *
 * Note that while formerly we could rely on the first varlena column of a
 * system catalog to be at the offset suggested by the C struct for the
 * catalog, this is now risky: it's only safe if the preceding field is
 * word-aligned, so that there will never be any padding.
 *
 * We don't pack varlenas whose attstorage is 'p', since the data type
 * isn't expecting to have to detoast values.  This is used in particular
 * by oidvector and int2vector, which are used in the system catalogs
 * and we'd like to still refer to them via C struct offsets.
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/access/common/heaptuple.c,v 1.115 2007/02/09 03:35:33 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/sysattr.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "executor/tuptable.h"

#include "catalog/pg_type.h"
#include "cdb/cdbvars.h"                /* Gp_segment */

#include "utils/debugbreak.h"

/* Does att's datatype allow packing into the 1-byte-header varlena format? */
#define ATT_IS_PACKABLE(att) \
	((att)->attlen == -1 && (att)->attstorage != 'p')
/* Use this if it's already known varlena */
#define VARLENA_ATT_IS_PACKABLE(att) \
	((att)->attstorage != 'p')


/* ----------------------------------------------------------------
 *						misc support routines
 * ----------------------------------------------------------------
 */


/*
 * heap_compute_data_size
 *		Determine size of the data area of a tuple to be constructed
 */
Size
heap_compute_data_size(TupleDesc tupleDesc,
					   Datum *values,
					   bool *isnull)
{
	Size		data_length = 0;
	int			i;
	int			numberOfAttributes = tupleDesc->natts;
	Form_pg_attribute *att = tupleDesc->attrs;

	for (i = 0; i < numberOfAttributes; i++)
	{
		if (isnull[i])
			continue;

		/* we're anticipating converting to a short varlena header even if it's
		 * not currently */
 		if (att[i]->attlen == -1 && value_type_could_short(values[i], att[i]->atttypid))
		{
			/* no alignment and we will convert to 1-byte header */; 
			data_length += VARSIZE_ANY_EXHDR_D(values[i]) + VARHDRSZ_SHORT;
		}
		else
		{
			data_length = att_align(data_length, att[i]->attalign); 
			data_length = att_addlength(data_length, att[i]->attlen, values[i]);
		}
	}

	return data_length;
}

/*
 * heap_fill_tuple
 *		Load data portion of a tuple from values/isnull arrays
 *
 * We also fill the null bitmap (if any) and set the infomask bits
 * that reflect the tuple's data contents.
 *
 * NOTE: it is now REQUIRED that the caller have pre-zeroed the data area.
 *
 *
 * @param isnull will only be used if <code>bit</code> is non-NULL
 * @param bit should be non-NULL (refer to td->t_bits) if isnull is set and contains non-null values
 */
Size
heap_fill_tuple(TupleDesc tupleDesc,
				Datum *values, bool *isnull,
				char *data, uint16 *infomask, bits8 *bit)
{
	char	   *start = data;
	bits8	   *bitP;
	int			bitmask;
	int			i;
	int			numberOfAttributes = tupleDesc->natts;
	Form_pg_attribute *att = tupleDesc->attrs;

	if (bit != NULL)
	{
		bitP = &bit[-1];
		bitmask = HIGHBIT;
	}
	else
	{
		/* just to keep compiler quiet */
		bitP = NULL;
		bitmask = 0;
	}

	*infomask &= ~(HEAP_HASNULL | HEAP_HASVARWIDTH | HEAP_HASEXTENDED);

	for (i = 0; i < numberOfAttributes; i++)
	{
		Size		data_length;

		if (bit != NULL)
		{
			if (bitmask != HIGHBIT)
				bitmask <<= 1;
			else
			{
				bitP += 1;
				*bitP = 0x0;
				bitmask = 1;
			}

			if (isnull[i])
			{
				*infomask |= HEAP_HASNULL;
				continue;
			}

			*bitP |= bitmask;
		}

		/*
		 * XXX we use the att_align macros on the pointer value itself, not on
		 * an offset.  This is a bit of a hack.
		 */

		if (att[i]->attbyval)
		{
			/* pass-by-value */
			data = (char *) att_align_zero(data, att[i]->attalign);
			store_att_byval(data, values[i], att[i]->attlen);
			data_length = att[i]->attlen;
		}
		else if (att[i]->attlen == -1)
		{
			/* varlena */
			*infomask |= HEAP_HASVARWIDTH;
			if (VARATT_IS_COMPRESSED_D(values[i]))
				*infomask |= HEAP_HASCOMPRESSED;
			if (VARATT_IS_EXTERNAL_D(values[i])) 
			{
				*infomask |= HEAP_HASEXTERNAL;
				data = (char *) att_align_zero(data, att[i]->attalign);
				data_length = VARSIZE_EXTERNAL(DatumGetPointer(values[i]));
				memcpy(data, DatumGetPointer(values[i]), data_length);
			}
			else if (VARATT_IS_SHORT_D(values[i])) 
			{
				/* no alignment for short varlenas */
				data_length = VARSIZE_SHORT(DatumGetPointer(values[i]));
				memcpy(data, DatumGetPointer(values[i]), data_length);
			}
			else if (VARATT_COULD_SHORT_D(values[i]) &&
					 att[i]->atttypid != INT2VECTOROID &&
					 att[i]->atttypid != OIDVECTOROID &&
					 att[i]->atttypid < FirstNormalObjectId)
			{
				/* convert to short varlena -- no alignment */
				data_length = VARSIZE_D(values[i]) - VARHDRSZ + VARHDRSZ_SHORT;
				*data = VARSIZE_TO_SHORT_D(values[i]);
				memcpy(data+1,
					   VARDATA_D(values[i]),
					   data_length-1);
			}
			else
			{
				/* must store full 4-byte header varlena */
				data = (char *) att_align_zero(data, att[i]->attalign);
				data_length = VARSIZE(DatumGetPointer(values[i]));
				memcpy(data, DatumGetPointer(values[i]), data_length);
			}
		}
		else if (att[i]->attlen == -2)
		{
			/* cstring */
			data = (char *) att_align_zero(data, att[i]->attalign);
			*infomask |= HEAP_HASVARWIDTH;
			data_length = strlen(DatumGetCString(values[i])) + 1;
			memcpy(data, DatumGetPointer(values[i]), data_length);
		}
		else
		{
			/* fixed-length pass-by-reference */
			data = (char *) att_align_zero(data, att[i]->attalign);
			Assert(att[i]->attlen > 0);
			data_length = att[i]->attlen;
			memcpy(data, DatumGetPointer(values[i]), data_length);
		}

		data += data_length;
	}
	return data - start;
}

/* ----------------------------------------------------------------
 *						heap tuple interface
 * ----------------------------------------------------------------
 */

/* ----------------
 *		heap_attisnull	- returns TRUE iff tuple attribute is not present
 * ----------------
 */
bool
heap_attisnull(HeapTuple tup, int attnum)
{
	Assert(!is_heaptuple_memtuple(tup));

	if (attnum > (int) HeapTupleHeaderGetNatts(tup->t_data))
		return true;

	if (attnum > 0)
	{
		if (HeapTupleNoNulls(tup))
			return false;
		return att_isnull(attnum - 1, tup->t_data->t_bits);
	}

	switch (attnum)
	{
		case TableOidAttributeNumber:
		case SelfItemPointerAttributeNumber:
		case ObjectIdAttributeNumber:
		case MinTransactionIdAttributeNumber:
		case MinCommandIdAttributeNumber:
		case MaxTransactionIdAttributeNumber:
		case MaxCommandIdAttributeNumber:
        case GpSegmentIdAttributeNumber:       /*CDB*/
			/* these are never null */
			break;

		default:
			elog(ERROR, "invalid attnum: %d", attnum);
	}

	return false;
}
bool
heap_attisnull_normalattr(HeapTuple tup, int attnum)
{
	Assert(attnum > 0);
	if (HeapTupleNoNulls(tup))
		return false;
	return att_isnull(attnum - 1, tup->t_data->t_bits);
}

/* ----------------
 *		nocachegetattr
 *
 *		This only gets called from fastgetattr() macro, in cases where
 *		we can't use a cacheoffset and the value is not null.
 *
 *		This caches attribute offsets in the attribute descriptor.
 *
 *		An alternative way to speed things up would be to cache offsets
 *		with the tuple, but that seems more difficult unless you take
 *		the storage hit of actually putting those offsets into the
 *		tuple you send to disk.  Yuck.
 *
 *		This scheme will be slightly slower than that, but should
 *		perform well for queries which hit large #'s of tuples.  After
 *		you cache the offsets once, examining all the other tuples using
 *		the same attribute descriptor will go much quicker. -cim 5/4/91
 *
 *		NOTE: if you need to change this code, see also heap_deform_tuple.
 *		Also see nocache_index_getattr, which is the same code for index
 *		tuples.
 * ----------------
 */
Datum
nocachegetattr(HeapTuple tuple,
			   int attnum,
			   TupleDesc tupleDesc)
{
	HeapTupleHeader tup = tuple->t_data;
	Form_pg_attribute *att = tupleDesc->attrs;
	char	   *tp;				/* ptr to att in tuple */
	bits8	   *bp = tup->t_bits;		/* ptr to null bitmap in tuple */
	bool		slow = false;	/* do we have to walk nulls? */

	Assert(!is_heaptuple_memtuple(tuple));
	/* If any cached offsets are there we can check that they make sense, but
	 * there may not be any at all, so pass -1 for the attnum we know is valid */

#ifdef IN_MACRO
/* This is handled in the macro */
	Assert(attnum > 0);

	if (isnull)
		*isnull = false;
#endif

	attnum--;

	/* ----------------
	 *	 Three cases:
	 *
	 *	 1: No nulls and no variable-width attributes.
	 *	 2: Has a null or a var-width AFTER att.
	 *	 3: Has nulls or var-widths BEFORE att.
	 * ----------------
	 */

	if (HeapTupleNoNulls(tuple))
	{
#ifdef IN_MACRO
/* This is handled in the macro */
		if (att[attnum]->attcacheoff != -1)
		{
			return fetchatt(att[attnum],
							(char *) tup + tup->t_hoff +
							att[attnum]->attcacheoff);
		}
#endif
	}
	else
	{
		/*
		 * there's a null somewhere in the tuple
		 *
		 * check to see if desired att is null
		 */

#ifdef IN_MACRO
/* This is handled in the macro */
		if (att_isnull(attnum, bp))
		{
			if (isnull)
				*isnull = true;
			return (Datum) NULL;
		}
#endif

		/*
		 * Now check to see if any preceding bits are null...
		 */
		{
			int			byte = attnum >> 3;
			int			finalbit = attnum & 0x07;

			/* check for nulls "before" final bit of last byte */
			if ((~bp[byte]) & ((1 << finalbit) - 1))
				slow = true;
			else
			{
				/* check for nulls in any "earlier" bytes */
				int			i;

				for (i = 0; i < byte; i++)
				{
					if (bp[i] != 0xFF)
					{
						slow = true;
						break;
					}
				}
			}
		}
	}

	tp = (char *) tup + tup->t_hoff;

	/*
	 * now check for any non-fixed length attrs before our attribute
	 */
	if (!slow)
	{
		/*
		 * If we get here, there are no nulls up to and including the target
		 * attribute.  If we have a cached offset, we can use it.
		 */
		if (att[attnum]->attcacheoff >= 0)
		{
			return fetchatt(att[attnum],
							tp + att[attnum]->attcacheoff);
		}

		/*
		 * Otherwise, check for non-fixed-length attrs up to and including
		 * target.	If there aren't any, it's safe to cheaply initialize the
		 * cached offsets for these attrs.
		 */
		if (HeapTupleHasVarWidth(tuple))
		{
			int			j;

			/*
			 * In for(), we test <= and not < because we want to see if we can
			 * go past it in initializing offsets.
			 */
			for (j = 0; j <= attnum; j++)
			{
				if (att[j]->attlen <= 0)
				{
					slow = true;
					break;
				}
			}
		}
	}

	/*
	 * If slow is false, and we got here, we know that we have a tuple with no
	 * nulls or var-widths before the target attribute. If possible, we also
	 * want to initialize the remainder of the attribute cached offset values.
	 */
	if (!slow)
	{
		int			j = 1;
		long		off;
		int			natts = HeapTupleHeaderGetNatts(tup);

		/*
		 * If we get here, we have a tuple with no nulls or var-widths up to
		 * and including the target attribute, so we can use the cached offset
		 * ... only we don't have it yet, or we'd not have got here.  Since
		 * it's cheap to compute offsets for fixed-width columns, we take the
		 * opportunity to initialize the cached offsets for *all* the leading
		 * fixed-width columns, in hope of avoiding future visits to this
		 * routine.
		 */

		/* this is always true */
		att[0]->attcacheoff = 0;

		while (j < attnum && att[j]->attcacheoff > 0)
			j++;

		off = att[j - 1]->attcacheoff + att[j - 1]->attlen;

		for (; j <= attnum ||
		/* Can we compute more?  We will probably need them */
			 (j < natts &&
			  att[j]->attcacheoff == -1 &&
			  (HeapTupleNoNulls(tuple) || !att_isnull(j, bp)) &&
			  (HeapTupleAllFixed(tuple) || att[j]->attlen > 0)); j++)
		{
			/* don't need to worry about shortvarlenas here since we're only
			 * looking at non-varlenas. Note that it's important that we check
			 * that the target attribute itself is a nonvarlena too since we
			 * can't use cached offsets for even the first varlena any more. */
			off = att_align(off, att[j]->attalign);

			att[j]->attcacheoff = off;

			off = att_addlength(off, att[j]->attlen, PointerGetDatum(tp + off));
		}

		return fetchatt(att[attnum], tp + att[attnum]->attcacheoff);
	}
	else
	{
		bool		usecache = true;
		int			off = 0;
		int			i;

		/* this is always true */
		att[0]->attcacheoff = 0;

		/*
		 * Now we know that we have to walk the tuple CAREFULLY.
		 *
		 * Note - This loop is a little tricky.  For each non-null attribute,
		 * we have to first account for alignment padding before the attr,
		 * then advance over the attr based on its length.	Nulls have no
		 * storage and no alignment padding either.  We can use/set
		 * attcacheoff until we reach either a null or a var-width attribute.
		 */

		for (i = 0; i < attnum; i++)
		{
			if (HeapTupleHasNulls(tuple) && att_isnull(i, bp))
			{
				usecache = false;
				continue;
			}

			/* If we know the next offset, we can skip the alignment calc */
			if (usecache && att[i]->attcacheoff != -1)
				off = att[i]->attcacheoff;
			else
			{
				/* if it's a varlena it may or may not be aligned, so check for
				 * something that looks like a padding byte before aligning. If
				 * we're already aligned it may be the leading byte of a 4-byte
				 * header but then the att_align is harmless. Don't bother
				 * looking if it's not a varlena though.*/
				if (att[i]->attlen != -1 || !tp[off])
					off = att_align(off, att[i]->attalign);
				if (usecache && att[i]->attlen != -1)
					att[i]->attcacheoff = off;
			}

			if (att[i]->attlen < 0)
				usecache = false;

			off = att_addlength(off, att[i]->attlen, PointerGetDatum(tp + off));
		}

		if (att[attnum]->attlen != -1 || !tp[off])
			off = att_align(off, att[attnum]->attalign);

		return fetchatt(att[attnum], tp + off);
	}
}

/* ----------------
 *		heap_getsysattr
 *
 *		Fetch the value of a system attribute for a tuple.
 *
 * This is a support routine for the heap_getattr macro.  The macro
 * has already determined that the attnum refers to a system attribute.
 * ----------------
 */
Datum
heap_getsysattr(HeapTuple tup, int attnum, bool *isnull)
{
	Datum			result;

	Assert(tup);
	Assert(!is_heaptuple_memtuple(tup));

	/* Currently, no sys attribute ever reads as NULL. */
	if (isnull)
		*isnull = false;

	switch (attnum)
	{
		case SelfItemPointerAttributeNumber:
			/* pass-by-reference datatype */
			result = PointerGetDatum(&(tup->t_self));
			break;
		case ObjectIdAttributeNumber:
			result = ObjectIdGetDatum(HeapTupleGetOid(tup));
			break;
		case MinTransactionIdAttributeNumber:
			result = TransactionIdGetDatum(HeapTupleHeaderGetXmin(tup->t_data));
			break;
		case MaxTransactionIdAttributeNumber:
			result = TransactionIdGetDatum(HeapTupleHeaderGetXmax(tup->t_data));
			break;
		case MinCommandIdAttributeNumber:
		case MaxCommandIdAttributeNumber:
			/*
			 * cmin and cmax are now both aliases for the same field, which
			 * can in fact also be a combo command id.	XXX perhaps we should
			 * return the "real" cmin or cmax if possible, that is if we are
			 * inside the originating transaction?
			 */
			result = CommandIdGetDatum(HeapTupleHeaderGetRawCommandId(tup->t_data));
			break;
		case TableOidAttributeNumber:
            /* CDB: Must now use a TupleTableSlot to access the 'tableoid'. */
			result = ObjectIdGetDatum(InvalidOid);
			elog(ERROR, "Invalid reference to \"tableoid\" system attribute");
			break;
		case GpSegmentIdAttributeNumber:                       /*CDB*/
			result = Int32GetDatum(Gp_segment);
			break;
		default:
			result = Int32GetDatum(Gp_segment);
			Assert(!"Invalid attnum for getsysattr");
			elog(ERROR, "invalid attnum: %d", attnum);
			break;
	}
	return result;
}

/* ----------------
 *		heap_copytuple
 *
 *		returns a copy of an entire tuple
 *
 * The HeapTuple struct, tuple header, and tuple data are all allocated
 * as a single palloc() block.
 * ----------------
 */
HeapTuple
heaptuple_copy_to(HeapTuple tuple, HeapTuple dest, uint32 *destlen)
{
	HeapTuple	newTuple;
	uint32 len;

	if (!HeapTupleIsValid(tuple) || tuple->t_data == NULL)
		return NULL;

	Assert(!is_heaptuple_memtuple(tuple));

	len = HEAPTUPLESIZE + tuple->t_len;
	if(destlen && *destlen < len)
	{
		*destlen = len;
		return NULL;
	}

	if(destlen)
	{
		*destlen = len;
		newTuple = dest;
	}
	else
		newTuple = (HeapTuple) palloc(HEAPTUPLESIZE + tuple->t_len);

	newTuple->t_len = tuple->t_len;
	newTuple->t_self = tuple->t_self;
	newTuple->t_data = (HeapTupleHeader) ((char *) newTuple + HEAPTUPLESIZE);
	memcpy((char *) newTuple->t_data, (char *) tuple->t_data, tuple->t_len);
	return newTuple;
}

/* ----------------
 *		heap_copytuple_with_tuple
 *
 *		copy a tuple into a caller-supplied HeapTuple management struct
 *
 * Note that after calling this function, the "dest" HeapTuple will not be
 * allocated as a single palloc() block (unlike with heap_copytuple()).
 * ----------------
 */
void
heap_copytuple_with_tuple(HeapTuple src, HeapTuple dest)
{
	if (!HeapTupleIsValid(src) || src->t_data == NULL)
	{
		dest->t_data = NULL;
		return;
	}

	Assert(!is_heaptuple_memtuple(src));

	dest->t_len = src->t_len;
	dest->t_self = src->t_self;
	dest->t_data = (HeapTupleHeader) palloc(src->t_len);
	memcpy((char *) dest->t_data, (char *) src->t_data, src->t_len);
}

/*
 * heap_form_tuple
 *		construct a tuple from the given values[] and isnull[] arrays,
 *		which are of the length indicated by tupleDescriptor->natts
 *
 * The result is allocated in the current memory context.
 */
HeapTuple
heaptuple_form_to(TupleDesc tupleDescriptor, Datum *values, bool *isnull, HeapTuple dst, uint32 *dstlen)
{
	HeapTuple	tuple;			/* return tuple */
	HeapTupleHeader td;			/* tuple data */
	unsigned long len, predicted_len, actual_len;
	int			hoff;
	bool		hasnull = false;
	Form_pg_attribute *att = tupleDescriptor->attrs;
	int			numberOfAttributes = tupleDescriptor->natts;
	int			i;

	if (numberOfAttributes > MaxTupleAttributeNumber)
		ereport(ERROR,
				(errcode(ERRCODE_TOO_MANY_COLUMNS),
				 errmsg("number of columns (%d) exceeds limit (%d)",
						numberOfAttributes, MaxTupleAttributeNumber)));

	/*
	 * Check for nulls and embedded tuples; expand any toasted attributes in
	 * embedded tuples.  This preserves the invariant that toasting can only
	 * go one level deep.
	 *
	 * We can skip calling toast_flatten_tuple_attribute() if the attribute
	 * couldn't possibly be of composite type.  All composite datums are
	 * varlena and have alignment 'd'; furthermore they aren't arrays. Also,
	 * if an attribute is already toasted, it must have been sent to disk
	 * already and so cannot contain toasted attributes.
	 */
	for (i = 0; i < numberOfAttributes; i++)
	{
		if (isnull[i])
			hasnull = true;
		else if (att[i]->attlen == -1 &&
				 att[i]->attalign == 'd' &&
				 att[i]->attndims == 0 &&
				 !VARATT_IS_EXTENDED_D(values[i]))
		{
			values[i] = toast_flatten_tuple_attribute(values[i],
													  att[i]->atttypid,
													  att[i]->atttypmod);
		}
	}

	/*
	 * Determine total space needed
	 */
	len = offsetof(HeapTupleHeaderData, t_bits);

	if (hasnull)
		len += BITMAPLEN(numberOfAttributes);

	if (tupleDescriptor->tdhasoid)
		len += sizeof(Oid);

	hoff = len = MAXALIGN(len); /* align user data safely */

	predicted_len = heap_compute_data_size(tupleDescriptor, values, isnull);

	len += predicted_len;

	if(dstlen && (*dstlen) < (HEAPTUPLESIZE + len))
	{
		*dstlen = HEAPTUPLESIZE + len;
		return NULL;
	}

	if(dstlen)
	{
		*dstlen = HEAPTUPLESIZE + len;
		tuple = dst;
	}
	else
		tuple = (HeapTuple) palloc(HEAPTUPLESIZE + len);

	/*
	 * Allocate and zero the space needed.	Note that the tuple body and
	 * HeapTupleData management structure are allocated in one chunk.
	 */
	tuple->t_data = td = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);

	/*
	 * And fill in the information.  Note we fill the Datum fields even though
	 * this tuple may never become a Datum.
	 */
	tuple->t_len = len;
	ItemPointerSetInvalid(&(tuple->t_self));

	/* 
	 * The following 3 calls will setup the first 12 bytes of td (tuple->t_data) 
	 */
	HeapTupleHeaderSetDatumLength(td, len);
	HeapTupleHeaderSetTypeId(td, tupleDescriptor->tdtypeid);
	HeapTupleHeaderSetTypMod(td, tupleDescriptor->tdtypmod);

	/* t_ctid does not matter */

	/* num of attrs are stored in t_infomask2.  Clear the other flags first */
	td->t_infomask2 = 0;
	HeapTupleHeaderSetNatts(td, numberOfAttributes);

	/* 
	 * Set up t_hoff.  This need to be done before set up t_infomask
	 * because HeapTupleHeaderSetOid will use t_hoff 
	 */
	td->t_hoff = hoff;

	if (tupleDescriptor->tdhasoid)
	{
		td->t_infomask = HEAP_HASOID;
		HeapTupleHeaderSetOid(td, InvalidOid);
	}
	else
		td->t_infomask = 0;

	/* Really fill in the data. */
	actual_len = 
		heap_fill_tuple(tupleDescriptor,
						values,
						isnull,
						(char *) td + hoff,
						&td->t_infomask,
						(hasnull ? td->t_bits : NULL));
	
	Assert(predicted_len == actual_len);
	Assert(!is_heaptuple_memtuple(tuple));

	return tuple;
}

/*
 *		heap_formtuple
 *
 *		construct a tuple from the given values[] and nulls[] arrays
 *
 *		Null attributes are indicated by a 'n' in the appropriate byte
 *		of nulls[]. Non-null attributes are indicated by a ' ' (space).
 *
 * OLD API with char 'n'/' ' convention for indicating nulls.
 * This is deprecated and should not be used in new code, but we keep it
 * around for use by old add-on modules.
 */
HeapTuple
heap_formtuple(TupleDesc tupleDescriptor,
			   Datum *values,
			   char *nulls)
{
	bool *isnull = (bool *) palloc(sizeof(bool) * tupleDescriptor->natts);
	HeapTuple ret;

	int i;
	for(i=0; i<tupleDescriptor->natts; ++i)
		isnull[i] = (nulls[i] != ' ');

	ret = heaptuple_form_to(tupleDescriptor, values, isnull, NULL, NULL);
	pfree(isnull);

	return ret;
}

/*
 * heap_modify_tuple
 *		form a new tuple from an old tuple and a set of replacement values.
 *
 * The replValues, replIsnull, and doReplace arrays must be of the length
 * indicated by tupleDesc->natts.  The new tuple is constructed using the data
 * from replValues/replIsnull at columns where doReplace is true, and using
 * the data from the old tuple at columns where doReplace is false.
 *
 * The result is allocated in the current memory context.
 */
HeapTuple
heap_modify_tuple(HeapTuple tuple,
				  TupleDesc tupleDesc,
				  Datum *replValues,
				  bool *replIsnull,
				  bool *doReplace)
{
	int			numberOfAttributes = tupleDesc->natts;
	int			attoff;
	Datum	   *values;
	bool	   *isnull;
	HeapTuple	newTuple;

	Assert(!is_heaptuple_memtuple(tuple));

	/*
	 * allocate and fill values and isnull arrays from either the tuple or the
	 * repl information, as appropriate.
	 *
	 * NOTE: it's debatable whether to use heap_deform_tuple() here or just
	 * heap_getattr() only the non-replaced colums.  The latter could win if
	 * there are many replaced columns and few non-replaced ones. However,
	 * heap_deform_tuple costs only O(N) while the heap_getattr way would cost
	 * O(N^2) if there are many non-replaced columns, so it seems better to
	 * err on the side of linear cost.
	 */
	values = (Datum *) palloc(numberOfAttributes * sizeof(Datum));
	isnull = (bool *) palloc(numberOfAttributes * sizeof(bool));

	heap_deform_tuple(tuple, tupleDesc, values, isnull);

	for (attoff = 0; attoff < numberOfAttributes; attoff++)
	{
		if (doReplace[attoff])
		{
			values[attoff] = replValues[attoff];
			isnull[attoff] = replIsnull[attoff];
		}
	}

	/*
	 * create a new tuple from the values and isnull arrays
	 */
	newTuple = heap_form_tuple(tupleDesc, values, isnull);

	pfree(values);
	pfree(isnull);

	/*
	 * copy the identification info of the old tuple: t_ctid, t_self, and OID
	 * (if any)
	 */
	newTuple->t_data->t_ctid = tuple->t_data->t_ctid;
	newTuple->t_self = tuple->t_self;
	if (tupleDesc->tdhasoid)
		HeapTupleSetOid(newTuple, HeapTupleGetOid(tuple));

	return newTuple;
}

/*
 *		heap_modifytuple
 *
 *		forms a new tuple from an old tuple and a set of replacement values.
 *		returns a new palloc'ed tuple.
 *
 * OLD API with char 'n'/' ' convention for indicating nulls, and
 * char 'r'/' ' convention for indicating whether to replace columns.
 * This is deprecated and should not be used in new code, but we keep it
 * around for use by old add-on modules.
 */
HeapTuple
heap_modifytuple(HeapTuple tuple,
				 TupleDesc tupleDesc,
				 Datum *replValues,
				 char *replNulls,
				 char *replActions)
{
	bool *replIsNull = (bool *) palloc(sizeof(bool) * tupleDesc->natts);
	bool *doRepl = (bool *) palloc(sizeof(bool) * tupleDesc->natts);
	HeapTuple ret;

	int i;
	for(i=0; i<tupleDesc->natts; ++i)
	{
		replIsNull[i] = (replNulls[i] != ' ');
		doRepl[i] = (replActions[i] == 'r');
	}

	ret = heap_modify_tuple(tuple, tupleDesc, replValues, replIsNull, doRepl);
	pfree(replIsNull);
	pfree(doRepl);

	return ret;
}

/*
 * heap_deform_tuple
 *		Given a tuple, extract data into values/isnull arrays; this is
 *		the inverse of heap_form_tuple.
 *
 *		Storage for the values/isnull arrays is provided by the caller;
 *		it should be sized according to tupleDesc->natts not tuple->t_natts.
 *
 *		Note that for pass-by-reference datatypes, the pointer placed
 *		in the Datum will point into the given tuple.
 *
 *		When all or most of a tuple's fields need to be extracted,
 *		this routine will be significantly quicker than a loop around
 *		heap_getattr; the loop will become O(N^2) as soon as any
 *		noncacheable attribute offsets are involved.
 */
void
heap_deform_tuple(HeapTuple tuple, TupleDesc tupleDesc,
				  Datum *values, bool *isnull)
{
	HeapTupleHeader tup = tuple->t_data;
	bool		hasnulls = HeapTupleHasNulls(tuple);
	Form_pg_attribute *att = tupleDesc->attrs;
	int			tdesc_natts = tupleDesc->natts;
	int			natts;			/* number of atts to extract */
	int			attnum;
	char	   *tp;				/* ptr to tuple data */
	long		off;			/* offset in tuple data */
	bits8	   *bp = tup->t_bits;		/* ptr to null bitmap in tuple */
	bool		slow = false;	/* can we use/set attcacheoff? */

	Assert(!is_heaptuple_memtuple(tuple));
	natts = HeapTupleHeaderGetNatts(tup);

	/*
	 * In inheritance situations, it is possible that the given tuple actually
	 * has more fields than the caller is expecting.  Don't run off the end of
	 * the caller's arrays.
	 */
	natts = Min(natts, tdesc_natts);

	tp = (char *) tup + tup->t_hoff;

	off = 0;

	for (attnum = 0; attnum < natts; attnum++)
	{
		Form_pg_attribute thisatt = att[attnum];

		if (hasnulls && att_isnull(attnum, bp))
		{
			values[attnum] = (Datum) 0;
			isnull[attnum] = true;
			slow = true;		/* can't use attcacheoff anymore */
			continue;
		}

		isnull[attnum] = false;

		if (!slow && thisatt->attcacheoff >= 0)
			off = thisatt->attcacheoff;
		else
		{
			/* if it's a varlena it may or may not be aligned, so check for
			 * something that looks like a padding byte before aligning. If
			 * we're already aligned it may be the leading byte of a 4-byte
			 * header but then the att_align is harmless. Don't bother looking
			 * if it's not a varlena though.*/
			if (thisatt->attlen != -1 || !tp[off])
				off = att_align(off, thisatt->attalign);

			if (!slow && thisatt->attlen != -1)
				thisatt->attcacheoff = off;

		}
		if (!slow && thisatt->attlen < 0)
			slow = true;

		values[attnum] = fetchatt(thisatt, tp + off);

#ifdef USE_ASSERT_CHECKING
		/* Ignore attributes with dropped types */
		if (thisatt->attlen == -1 && !thisatt->attisdropped)
		{
			Assert(VARATT_IS_SHORT_D(values[attnum]) || 
				   !VARATT_COULD_SHORT_D(values[attnum]) ||
				   thisatt->atttypid == OIDVECTOROID ||
				   thisatt->atttypid == INT2VECTOROID ||
				   thisatt->atttypid >= FirstNormalObjectId);	
		}
#endif

		off = att_addlength(off, thisatt->attlen, PointerGetDatum(tp + off));
	}

	/*
	 * If tuple doesn't have all the atts indicated by tupleDesc, read the
	 * rest as null
	 */
	for (; attnum < tdesc_natts; attnum++)
	{
		values[attnum] = (Datum) 0;
		isnull[attnum] = true;
	}
}

/*
 *		heap_deformtuple
 *
 *		Given a tuple, extract data into values/nulls arrays; this is
 *		the inverse of heap_formtuple.
 *
 *		Storage for the values/nulls arrays is provided by the caller;
 *		it should be sized according to tupleDesc->natts not tuple->t_natts.
 *
 *		Note that for pass-by-reference datatypes, the pointer placed
 *		in the Datum will point into the given tuple.
 *
 *		When all or most of a tuple's fields need to be extracted,
 *		this routine will be significantly quicker than a loop around
 *		heap_getattr; the loop will become O(N^2) as soon as any
 *		noncacheable attribute offsets are involved.
 *
 * OLD API with char 'n'/' ' convention for indicating nulls.
 * This is deprecated and should not be used in new code, but we keep it
 * around for use by old add-on modules.
 */
void
heap_deformtuple(HeapTuple tuple,
				 TupleDesc tupleDesc,
				 Datum *values,
				 char *nulls)
{
	int			i;
	bool	   *isnull = (bool *) palloc(tupleDesc->natts * sizeof(bool));

	heap_deform_tuple(tuple, tupleDesc, values, isnull);

	for(i=0; i<tupleDesc->natts; ++i)
		nulls[i] = isnull[i] ? 'n' : ' ';

	pfree(isnull);
}

/*
 * slot_deform_tuple
 *		Given a TupleTableSlot, extract data from the slot's physical tuple
 *		into its Datum/isnull arrays.  Data is extracted up through the
 *		natts'th column (caller must ensure this is a legal column number).
 *
 *		This is essentially an incremental version of heap_deform_tuple:
 *		on each call we extract attributes up to the one needed, without
 *		re-computing information about previously extracted attributes.
 *		slot->tts_nvalid is the number of attributes already extracted.
 */
static void
slot_deform_tuple(TupleTableSlot *slot, int natts)
{
	HeapTuple	tuple = TupGetHeapTuple(slot); 
	TupleDesc	tupleDesc = slot->tts_tupleDescriptor;
	Datum	   *values = slot->PRIVATE_tts_values;
	bool	   *isnull = slot->PRIVATE_tts_isnull;
	HeapTupleHeader tup = tuple->t_data;
	bool		hasnulls = HeapTupleHasNulls(tuple);
	Form_pg_attribute *att = tupleDesc->attrs;
	int			attnum;
	char	   *tp;				/* ptr to tuple data */
	long		off;			/* offset in tuple data */
	bits8	   *bp = tup->t_bits;		/* ptr to null bitmap in tuple */
	bool		slow;			/* can we use/set attcacheoff? */

	/*
	 * Check whether the first call for this tuple, and initialize or restore
	 * loop state.
	 */
	attnum = slot->PRIVATE_tts_nvalid;
	if (attnum == 0)
	{
		/* Start from the first attribute */
		off = 0;
		slow = false;
	}
	else
	{
		/* Restore state from previous execution */
		off = slot->PRIVATE_tts_off;
		slow = slot->PRIVATE_tts_slow;
	}

	tp = (char *) tup + tup->t_hoff;

	for (; attnum < natts; attnum++)
	{
		Form_pg_attribute thisatt = att[attnum];

		if (hasnulls && att_isnull(attnum, bp))
		{
			values[attnum] = (Datum) 0;
			isnull[attnum] = true;
			slow = true;		/* can't use attcacheoff anymore */
			continue;
		}

		isnull[attnum] = false;

		if (!slow && thisatt->attcacheoff >= 0)
			off = thisatt->attcacheoff;
		else
		{
			/* if it's a varlena it may or may not be aligned, so check for
			 * something that looks like a padding byte before aligning. If
			 * we're already aligned it may be the leading byte of a 4-byte
			 * header but then the att_align is harmless. Don't bother looking
			 * if it's not a varlena though.*/
			if (thisatt->attlen != -1 || !tp[off])
				off = att_align(off, thisatt->attalign);

			if (!slow && thisatt->attlen != -1)
				thisatt->attcacheoff = off;
		}
		if (!slow && thisatt->attlen < 0)
			slow = true;

		values[attnum] = fetchatt(thisatt, tp + off);

		off = att_addlength(off, thisatt->attlen, PointerGetDatum(tp + off));
	}

	/*
	 * Save state for next execution
	 */
	slot->PRIVATE_tts_nvalid = attnum;
	slot->PRIVATE_tts_off = off;
	slot->PRIVATE_tts_slow = slow;
}

/*
 * slot_getsomeattrs
 *		This function forces the entries of the slot's Datum/isnull
 *		arrays to be valid at least up through the attnum'th entry.
 */
void
_slot_getsomeattrs(TupleTableSlot *slot, int attnum)
{
	HeapTuple	tuple;
	int			attno;

	/* Quick out if we have 'em all already */
	if (slot->PRIVATE_tts_nvalid >= attnum)
		return;

	/* Check for caller error */
	if (attnum <= 0 || attnum > slot->tts_tupleDescriptor->natts)
		elog(ERROR, "invalid attribute number %d", attnum);

	/*
	 * otherwise we had better have a physical tuple (tts_nvalid should equal
	 * natts in all virtual-tuple cases)
	 */
	tuple = TupGetHeapTuple(slot); 
	if (tuple == NULL)			/* internal error */
		elog(ERROR, "cannot extract attribute from empty tuple slot");

	/*
	 * load up any slots available from physical tuple
	 */
	attno = HeapTupleHeaderGetNatts(tuple->t_data);
	attno = Min(attno, attnum);

	slot_deform_tuple(slot, attno);

	/*
	 * If tuple doesn't have all the atts indicated by tupleDesc, read the
	 * rest as null
	 */
	for (; attno < attnum; attno++)
	{
		slot->PRIVATE_tts_values[attno] = (Datum) 0;
		slot->PRIVATE_tts_isnull[attno] = true;
	}

	slot->PRIVATE_tts_nvalid = attnum;
	TupSetVirtualTuple(slot);
}

/*
 * heap_freetuple
 */
void
heap_freetuple(HeapTuple htup)
{
	pfree(htup);
}

/* ----------------
 *		heap_addheader
 *
 * This routine forms a HeapTuple by copying the given structure (tuple
 * data) and adding a generic header.  Note that the tuple data is
 * presumed to contain no null fields and no varlena fields.
 *
 * This routine is really only useful for certain system tables that are
 * known to be fixed-width and null-free.  Currently it is only used for
 * pg_attribute tuples.
 * ----------------
 */
HeapTuple
heap_addheader(int natts,		/* max domain index */
			   bool withoid,	/* reserve space for oid */
			   Size structlen,	/* its length */
			   void *structure) /* pointer to the struct */
{
	HeapTuple	tuple;
	HeapTupleHeader td;
	Size		len;
	int			hoff;

	AssertArg(natts > 0);

	/* header needs no null bitmap */
	hoff = offsetof(HeapTupleHeaderData, t_bits);
	if (withoid)
		hoff += sizeof(Oid);
	hoff = MAXALIGN(hoff);
	len = hoff + structlen;

	tuple = (HeapTuple) palloc0(HEAPTUPLESIZE + len);
	tuple->t_data = td = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);

	tuple->t_len = len;
	ItemPointerSetInvalid(&(tuple->t_self));

	/* we don't bother to fill the Datum fields */

	HeapTupleHeaderSetNatts(td, natts);
	td->t_hoff = hoff;

	if (withoid)				/* else leave infomask = 0 */
		td->t_infomask = HEAP_HASOID;

	memcpy((char *) td + hoff, structure, structlen);

	return tuple;
}
