/*-------------------------------------------------------------------------
 *
 * memtuple.c
 * 
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/backend/access/common/memtuple.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/memtup.h"
#include "access/tupmacs.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"

#include "cdb/cdbvars.h"

#define MAX_ATTR_COUNT_STATIC_ALLOC 20

/* Memory tuple format:
 * 4 byte _mt_len,
 * 	highest bit always 1.  (if 0, means it is a heaptuple).
 * 	bit 2-29th is memtuple length, in bytes.  It is always 8
 * 	bytes aligned.
 * 	bit 30 is unused.
 *   	bit 31 is set if the memtuple is longer than 64K.
 *	bit 32 is if has null.
 *
 * Followed by optional 4 byte for Oid (depends on if mtbind_has_oid)
 * 
 * Followed by optional null bitmaps.  
 *
 * Align to bind.column_align, either 4 or 8.
 *
 * Non-Null attribute:
 * 	Fixed len Attributes.
 *		The attributes are not in logical order.  First we put 8 bytes 
 * 		aligned native or native_ptr types.  The 4 bytes aligned natives
 * 		then 2 bytes aligned and varlena, then 1 bytes aligned natives.
 *		Varlena occupy 2bytes in the fixed len area.
 *
 * 	Varlena attributes.
 *
 * The end is again padded to 8 bytes aligned.
 *
 * Null attributes only occupy one bit in the nullbit map.  The non null
 * attributes is located from the binding offset/len.  If there is null attr,
 * we use the null_saves in the binding to figure out how many columns that is
 * physically precedes the attribute is null and how much space we have saved,
 * then we use off minus saved bytes to find the attribute.
 */

static int
compute_null_bitmap_extra_size(TupleDesc tupdesc, int col_align)
{
	int nbytes = (tupdesc->natts + 7) >> 3;
	int avail_bytes = (tupdesc->tdhasoid || col_align == 4) ? 0 : 4;

	Assert(col_align == 4 || col_align == 8);

	if (nbytes <= avail_bytes) 
		return 0;

	return TYPEALIGN(col_align, (nbytes - avail_bytes)); 
}

void destroy_memtuple_binding(MemTupleBinding *pbind)
{
	Assert(pbind);

	if(pbind->bind.null_saves)
		pfree(pbind->bind.null_saves);
	if(pbind->bind.null_saves_aligned)
		pfree(pbind->bind.null_saves_aligned);
	if(pbind->bind.bindings)
		pfree(pbind->bind.bindings);
	if(pbind->large_bind.null_saves)
		pfree(pbind->large_bind.null_saves);
	if(pbind->large_bind.null_saves_aligned)
		pfree(pbind->large_bind.null_saves_aligned);
	if(pbind->large_bind.bindings)
		pfree(pbind->large_bind.bindings);
	pfree(pbind);
}

/*
 * Manage the space saved by not storing nulls.  
 * Attr are rearranged in the order of 8 bytes aligned, then 4,
 * then 2, then 1.  A bit in the null bitmap is set for each 
 * null attribute.  For all possible combinations of 4 null bit,
 * we index into a short[16] array to get how many space is saved
 * by the nulls.
 */

/* Compute how much space to store the null save entries.
 * The null save entries are stored in the binding, not per tuple.
 */
static uint32
compute_null_save_entries(int i)
{
	return ((i+7)/8) * 32; 
}

/* Add null save space into the entries */
static void
add_null_save(short *null_save, int i, short sz)
{
	short* first = null_save + ((i/4) * 16);
	unsigned int bit = 1 << (i%4);

	for(i=0; i<16; ++i)
	{
		if( (i & bit) != 0)
			first[i] += sz;
	}
}

/*
 * Sets the binding length according to the following binding's alignment.
 * Adds the aligned length into the array holding the space saved from null attributes.
 * Returns true if the binding length is aligned to the following binding's alignment.
 */
static bool
add_null_save_aligned(MemTupleAttrBinding *bind, short *null_save_aligned, int i, char next_attr_align)
{
	Assert(bind);
	Assert(bind->len > 0);
	Assert(null_save_aligned);
	Assert(i >= 0);

	bind->len_aligned = att_align_nominal(bind->len, next_attr_align);
	add_null_save(null_save_aligned, i, bind->len_aligned);

	return (bind->len == bind->len_aligned);
}

/* Compute how much bytes are saved by one byte in the null bit map */
static inline short compute_null_save_b(short *null_saves, unsigned char b)
{
	unsigned int blow = (b & 0xF);
	unsigned int bhigh = (b >> 4);

	return null_saves[blow] + null_saves[16+bhigh];
}

/* compute the null saved bytes by the whole null bit map, by the attribute
 * physically precedes the one.
 */
static inline int compute_null_save(short *null_saves, unsigned char *nullbitmaps, int nbyte, unsigned char nbit)
{
	int ret = 0;
	int curr_byte = 0;
	while(curr_byte < nbyte) 
	{
		ret += compute_null_save_b(null_saves, nullbitmaps[curr_byte]);
		null_saves += 32;
		++curr_byte;
	}

	ret += compute_null_save_b(null_saves, (nullbitmaps[nbyte] & (nbit-1)));
	return ret;
}
		
#undef MEMTUPLE_INLINE_CHARTYPE
/* Determine if an attr should be treated as offset_len in memtuple */
static bool
att_bind_as_varoffset(Form_pg_attribute attr)
{
#ifdef MEMTUPLE_INLINE_CHARTYPE 
	return (attr->attlen < 0 /* Varlen type */
	        && (
	               attr->atttypid != BPCHAROID /* Any varlen type except char(N) */
		       || attr->atttypmod <= 4     /* char(0)?  ever happend? */
		       || attr->atttypmod >= 127   /* char(N) that cannot be shorted */
		       )
		);
#else
	/*
	 * XXX
	 * As optimization, one want to make some char(X) type inline, which
	 * will save 2 bytes.  However, postgres tupdesc is totally messed up
	 * the lenght of a (var)char(N) type (in typmod).  It just get randomly 
	 * set to the right thing or -1.  It is really stupid, but it just 
	 * took too much effort to fix everywhere.
	 *
	 * It is a shame.  Disable this for now.
	 */

	return attr->attlen < 0;
#endif
}

/* Create columns binding, depends on islarge, using 2 or 4 bytes for offset_len */
static void create_col_bind(MemTupleBindingCols *colbind, bool islarge, TupleDesc tupdesc, int col_align)
{
	int i = 0;
	int physical_col = 0;
	int pass = 0;

	uint32 cur_offset = (tupdesc->tdhasoid || col_align == 8) ? 8 : 4;
	uint32 null_save_entries = compute_null_save_entries(tupdesc->natts);

	/* alloc null save entries.  Zero it */
	colbind->null_saves = (short *) palloc0(sizeof(short) * null_save_entries);
	colbind->null_saves_aligned = (short *) palloc0(sizeof(short) * null_save_entries);
	colbind->has_null_saves_alignment_mismatch = false;
	colbind->has_dropped_attr_alignment_mismatch = false;

	/* alloc bindings, no need to zero because we will fill them out  */
	colbind->bindings = (MemTupleAttrBinding *) palloc(sizeof(MemTupleAttrBinding) * tupdesc->natts);
	
	/*
	 * The length of each binding is determined according to the alignment
	 * of the physically following binding. Use this pointer to keep track
	 * of the previously processed binding.
	 */
	MemTupleAttrBinding *previous_bind = NULL;

	/*
	 * First pass, do 8 bytes aligned, native type.
	 * Sencond pass, do 4 bytes aligned, native type.
	 * Third pass, do 2 bytes aligned, native type. 
	 * Finall, do 1 bytes aligned native type.
	 * 
	 * depends on islarge, varlena types are either handled in the
	 * second pass (is large, varoffset using 4 bytes), or in the 
	 * third pass (not large, varoffset using 2 bytes).
	 */
	for(pass =0; pass < 4; ++pass)
	{
		for(i=0; i<tupdesc->natts; ++i)
		{
			Form_pg_attribute attr = tupdesc->attrs[i];
			MemTupleAttrBinding *bind = &colbind->bindings[i];

			if(pass == 0 && attr->attlen > 0 && attr->attalign == 'd')
			{
				bind->offset = att_align_nominal(cur_offset, attr->attalign);
				bind->len = attr->attlen;
				add_null_save(colbind->null_saves, physical_col, attr->attlen);
				if (physical_col)
				{
					/* Set the aligned length of the previous binding according to current alignment. */
					if (add_null_save_aligned(previous_bind, colbind->null_saves_aligned, physical_col - 1, 'd'))
					{
						colbind->has_null_saves_alignment_mismatch = true;
						if (attr->attisdropped)
						{
							colbind->has_dropped_attr_alignment_mismatch = true;
						}
					}
				}

				bind->flag = attr->attbyval ? MTB_ByVal_Native : MTB_ByVal_Ptr;
				bind->null_byte = physical_col >> 3;
				bind->null_mask = 1 << (physical_col-(bind->null_byte << 3));

				physical_col += 1;
				cur_offset = bind->offset + bind->len;
				previous_bind = bind;
			}
			else if (pass == 1 &&( (attr->attlen > 0 && attr->attalign == 'i')
				              || ( islarge && att_bind_as_varoffset(attr))
					      )
				) 
			{
				bind->offset = att_align_nominal(cur_offset, 'i'); 
				bind->len = attr->attlen > 0 ? attr->attlen : 4; 
				add_null_save(colbind->null_saves, physical_col, bind->len);
				if (physical_col)
				{
					/* Set the aligned length of the previous binding according to current alignment. */
					if (add_null_save_aligned(previous_bind, colbind->null_saves_aligned, physical_col - 1, 'i'))
					{
						colbind->has_null_saves_alignment_mismatch = true;
						if (attr->attisdropped)
						{
							colbind->has_dropped_attr_alignment_mismatch = true;
						}
					}
				}

				if(attr->attlen > 0)
					bind->flag = attr->attbyval ? MTB_ByVal_Native : MTB_ByVal_Ptr;
				else if(attr->attlen == -1)
					bind->flag = MTB_ByRef;
				else
				{
					Assert(attr->attlen == -2);
					bind->flag = MTB_ByRef_CStr;
				}

				bind->null_byte = physical_col >> 3;
				bind->null_mask = 1 << (physical_col-(bind->null_byte << 3));

				physical_col += 1;
				cur_offset = bind->offset + bind->len;
				previous_bind = bind;
			}
			else if (pass == 2 && ( (attr->attlen > 0 && attr->attalign == 's') 
						|| ( !islarge && att_bind_as_varoffset(attr))
						)
				)
			{
				bind->offset = att_align_nominal(cur_offset, 's');
				bind->len = attr->attlen > 0 ? attr->attlen : 2; 
				add_null_save(colbind->null_saves, physical_col, bind->len);
				if (physical_col)
				{
					/* Set the aligned length of the previous binding according to current alignment. */
					if (add_null_save_aligned(previous_bind, colbind->null_saves_aligned, physical_col - 1, 's'))
					{
						colbind->has_null_saves_alignment_mismatch = true;
						if (attr->attisdropped)
						{
							colbind->has_dropped_attr_alignment_mismatch = true;
						}
					}
				}
				
				if(attr->attlen > 0)
					bind->flag = attr->attbyval ? MTB_ByVal_Native : MTB_ByVal_Ptr;
				else if(attr->attlen == -1)
					bind->flag = MTB_ByRef;
				else
				{
					Assert(attr->attlen == -2);
					bind->flag = MTB_ByRef_CStr;
				}

				bind->null_byte = physical_col >> 3;
				bind->null_mask = 1 << (physical_col-(bind->null_byte << 3));

				physical_col += 1;
				cur_offset = bind->offset + bind->len;
				previous_bind = bind;
			}
			else if (pass == 3 && (
						(attr->attlen > 0 && attr->attalign == 'c') 
						|| (attr->attlen < 0 && !att_bind_as_varoffset(attr))
						)
					)
			{
				bind->offset = att_align_nominal(cur_offset, 'c');

#ifdef MEMTUPLE_INLINE_CHARTYPE 
				/* Inline CHAR(N) disabled.  See att_bind_as_varoffset */
				bind->len = attr->attlen > 0 ? attr->attlen : (attr->atttypmod - 3);
#else
				bind->len = attr->attlen;
#endif

				add_null_save(colbind->null_saves, physical_col, 1);
				if (physical_col)
				{
					/* Set the aligned length of the previous binding according to current alignment. */
					if (add_null_save_aligned(previous_bind, colbind->null_saves_aligned, physical_col - 1, 'c'))
					{
						colbind->has_null_saves_alignment_mismatch = true;
						if (attr->attisdropped)
						{
							colbind->has_dropped_attr_alignment_mismatch = true;
						}
					}
				}

				if(attr->attlen > 0 && attr->attbyval)
					bind->flag = MTB_ByVal_Native;
				else
					bind->flag = MTB_ByVal_Ptr;

				bind->null_byte = physical_col >> 3;
				bind->null_mask = 1 << (physical_col-(bind->null_byte << 3));

				physical_col += 1;
				cur_offset = bind->offset + bind->len;
				previous_bind = bind;
			}
		}
	}

	if (physical_col)
	{
		/* No extra alignment required for the last binding */
		add_null_save_aligned(previous_bind, colbind->null_saves_aligned, physical_col - 1, 'c');
	}

	if (!colbind->has_null_saves_alignment_mismatch)
	{
		pfree(colbind->null_saves);
		colbind->null_saves = NULL;
	}

#ifdef USE_DEBUG_ASSERT
	for(i=0; i<tupdesc->natts; ++i)
	{
		MemTupleAttrBinding *bind = &colbind->bindings[i];
		Assert(bind->offset[i] != 0);
	}
#endif

	if(tupdesc->natts != 0)
		colbind->var_start = cur_offset;
	else
		colbind->var_start = 8;

	Assert(tupdesc->natts == physical_col);
}

/* Create a memtuple binding from the tupdesc.  Note we store
 * a ref to the tupdesc in the binding, so we assumed the life
 * span of the tupdesc is no shorter than the binding.
 */
MemTupleBinding *create_memtuple_binding(TupleDesc tupdesc) 
{
	MemTupleBinding *pbind = (MemTupleBinding *) palloc(sizeof(MemTupleBinding));
	int i = 0;

	pbind->tupdesc = tupdesc;
	pbind->column_align = 4;
	
	for(i=0; i<tupdesc->natts; ++i)
	{
		Form_pg_attribute attr = tupdesc->attrs[i];
		if(attr->attlen > 0 && attr->attalign == 'd')
			pbind->column_align = 8;
	}

	pbind->null_bitmap_extra_size = compute_null_bitmap_extra_size(tupdesc, pbind->column_align); 

	create_col_bind(&pbind->bind, false, tupdesc, pbind->column_align);
	create_col_bind(&pbind->large_bind, true, tupdesc, pbind->column_align);

	return pbind;
}

static uint32 compute_memtuple_size_using_bind(
		Datum *values,
		bool *isnull,
		bool hasnull,
		int nullbit_extra,
		uint32 *nullsaves,
		MemTupleBindingCols *colbind,
		TupleDesc tupdesc)
{
	uint32 data_length = colbind->var_start; 
	int i;

	*nullsaves = 0;

	if(hasnull)
	{
		data_length += nullbit_extra;

		for(i=0; i<tupdesc->natts; ++i)
		{
			if(isnull[i])
			{
				MemTupleAttrBinding *bind = &colbind->bindings[i];
				int len = 0;

				Assert(bind->len >= 0);
				Assert(bind->len_aligned >= 0);
				Assert(bind->len_aligned >= bind->len);

				len = bind->len_aligned;

				*nullsaves += len;
				data_length -= len;
			}
		}
	}

	for(i=0; i<tupdesc->natts; ++i)
	{
		MemTupleAttrBinding *bind = &colbind->bindings[i];
		Form_pg_attribute attr = tupdesc->attrs[i];

		if(isnull[i] || bind->flag == MTB_ByVal_Native || bind->flag == MTB_ByVal_Ptr)
			continue;

		/* Varlen stuff */
		/* We plan to convert to short varlena even if it is not currently */
		if (bind->flag == MTB_ByRef &&
			attr->attstorage != 'p' &&
			value_type_could_short(DatumGetPointer(values[i]), attr->atttypid))
		{
			data_length += VARSIZE_ANY_EXHDR(DatumGetPointer(values[i])) + VARHDRSZ_SHORT;
		}
		else
		{
			data_length = att_align_nominal(data_length, attr->attalign); 
			data_length = att_addlength_datum(data_length, attr->attlen, values[i]);
		}
	}

	return MEMTUP_ALIGN(data_length);
}

/* Compute the memtuple size. 
 * nullsave is an output param
 */
uint32 compute_memtuple_size(MemTupleBinding *pbind, Datum *values, bool *isnull, bool hasnull, uint32 *nullsaves)
{
	uint32 ret_len = 0;
	ret_len = compute_memtuple_size_using_bind(values, isnull, hasnull, pbind->null_bitmap_extra_size, nullsaves, &pbind->bind, pbind->tupdesc);

	if(ret_len <= MEMTUPLE_LEN_FITSHORT) 
		return ret_len;

	ret_len = compute_memtuple_size_using_bind(values, isnull, hasnull, pbind->null_bitmap_extra_size, nullsaves, &pbind->large_bind, pbind->tupdesc);
	Assert(ret_len > MEMTUPLE_LEN_FITSHORT);

	return ret_len;
}


static inline char* memtuple_get_attr_ptr(char *start, MemTupleAttrBinding *bind, short *null_saves, unsigned char *nullp)
{
	int ns = 0;

	if(nullp)
		ns = compute_null_save(null_saves, nullp, bind->null_byte, bind->null_mask);
	return start + bind->offset - ns;
}

static inline char* memtuple_get_attr_data_ptr(char *start, MemTupleAttrBinding *bind, short *null_saves, unsigned char* nullp)
{
	if(bind->flag == MTB_ByVal_Native || bind->flag == MTB_ByVal_Ptr)
		return memtuple_get_attr_ptr(start, bind, null_saves, nullp);

	if(bind->len == 2)
		return start + (*(uint16 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp));

	Assert(bind->len == 4);
	return start + (*(uint32 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp));
}

static inline unsigned char *memtuple_get_nullp(MemTuple mtup, MemTupleBinding *pbind)
{
	return mtup->PRIVATE_mt_bits + (mtbind_has_oid(pbind) ? sizeof(Oid) : 0);
}
static inline int memtuple_get_nullp_len(MemTupleBinding *pbind)
{
	return (pbind->tupdesc->natts + 7) >> 3;
}


/* form a memtuple from values and isnull, to a prespecified buffer */
MemTuple memtuple_form_to(
		MemTupleBinding *pbind,
		Datum *values,
		bool *isnull,
		MemTuple mtup,
		uint32 *destlen,
		bool inline_toast)
{
	bool hasnull = false;
	bool hasext = false;
	int i;
	uint32 len;
	unsigned char *nullp = NULL;
	char *start;
	char *varlen_start;
	uint32 null_save_len;
	MemTupleBindingCols *colbind;
	Datum *old_values = NULL;

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
	for(i=0; i<pbind->tupdesc->natts; ++i)
	{
		Form_pg_attribute attr = pbind->tupdesc->attrs[i];
		
#ifdef CHK_TYPE_SANE
		check_type_sanity(attr, values[i], isnull[i]);
#endif

		/* treat dropped attibutes as null */
		if (attr->attisdropped)
		{
			isnull[i] = true;
		}

		if(isnull[i])
		{
			hasnull = true;
			continue;
		}

		if (attr->attlen == -1 &&
				attr->attalign == 'd' &&
				attr->attndims == 0 &&
				!VARATT_IS_EXTENDED(DatumGetPointer(values[i])))
		{
			if (old_values == NULL)
				old_values = (Datum *)palloc0(pbind->tupdesc->natts * sizeof(Datum));
			old_values[i] = values[i];
			values[i] = toast_flatten_tuple_attribute(values[i], attr->atttypid, attr->atttypmod);
			if (values[i] == old_values[i])
				old_values[i] = 0;
		}

		if (attr->attlen == -1 && VARATT_IS_EXTERNAL(DatumGetPointer(values[i])))
		{
			if(inline_toast)
			{
				if (old_values == NULL)
					old_values = (Datum *)palloc0(pbind->tupdesc->natts * sizeof(Datum));
				old_values[i] = values[i];
				values[i] = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(values[i])));

				if (old_values[i] == values[i])
					old_values[i] = 0;
			}
			
			else
				hasext = true;
		}
	}

	/* compute needed length */
	len = compute_memtuple_size(pbind, values, isnull, hasnull, &null_save_len);
	colbind = (len <= MEMTUPLE_LEN_FITSHORT) ? &pbind->bind : &pbind->large_bind;

	if(!destlen)
	{
		Assert(!mtup);
		mtup = (MemTuple) palloc(len);
	}
	else if(*destlen < len)
	{
		*destlen = len;

		/*
		 * Set values to their old values if we have changed their values
		 * during de-toasting, and release the space allocated during
		 * de-toasting.
		 */
		if (old_values != NULL)
		{
			for(i=0; i<pbind->tupdesc->natts; ++i)
			{
				if (DatumGetPointer(old_values[i]) != NULL)
				{
					Assert(DatumGetPointer(values[i]) != NULL);
					pfree(DatumGetPointer(values[i]));
					values[i] = old_values[i];
				}
			}
			pfree(old_values);
		}
		
		return NULL;
	}
	else
	{
		*destlen = len;
		Assert(mtup);
	}

	/* Set mtlen, this set the lead bit, len, and clears hasnull bit 
	 * because the len returned from compute size is always max aligned
	 */
	Assert(len == MEMTUP_ALIGN(len));
	memtuple_set_mtlen(mtup, (len | MEMTUP_LEAD_BIT));

	if(len > MEMTUPLE_LEN_FITSHORT)
		memtuple_set_islarge(mtup);

	if(hasext)
		memtuple_set_hasext(mtup);

	/* Clear Oid */ 
	if(mtbind_has_oid(pbind))
		MemTupleSetOid(mtup, pbind, InvalidOid);

	if(hasnull)
		nullp = memtuple_get_nullp(mtup, pbind);

	start = (char *) mtup;
	varlen_start = ((char *) mtup) + colbind->var_start - null_save_len;

	if(hasnull)
	{
		memtuple_set_hasnull(mtup);

		/* if null bitmap is more than 4 bytes, add needed space */
		start += pbind->null_bitmap_extra_size;
		varlen_start += pbind->null_bitmap_extra_size;

		/* clear null bitmap. */
		memset(nullp, 0, memtuple_get_nullp_len(pbind));
	}

	/* It is very important to setup the null bitmap first before we 
	 * really put the values into place.  Where is the value in the 
	 * memtuple is determined by space saved from nulls, so the bitmap
	 * is used in the next loop. 
	 * NOTE: We cannot set the bitmap in the next loop (even at very
	 * beginning of next loop), because physical col order is different
	 * from logical. 
	 */
	for(i=0; i<pbind->tupdesc->natts; ++i)
	{
		if(isnull[i])
		{
			MemTupleAttrBinding *bind = &(colbind->bindings[i]);
			Assert(hasnull);
			nullp[bind->null_byte] |= bind->null_mask;
		}
	}

	/* Null bitmap is set up correctly, we can put in values now */
	for(i=0; i<pbind->tupdesc->natts; ++i)
	{
		Form_pg_attribute attr = pbind->tupdesc->attrs[i];
		MemTupleAttrBinding *bind = &(colbind->bindings[i]);

		uint32 attr_len;

		if(isnull[i])
			continue;

		Assert(bind->offset != 0);

		short *null_saves = colbind->null_saves_aligned;
		Assert(null_saves);

		/* Not null */
		switch(bind->flag)
		{
			case MTB_ByVal_Native:
				store_att_byval(memtuple_get_attr_ptr(start, bind, null_saves, nullp),
						values[i],
						bind->len
					       );
				break;
			case MTB_ByVal_Ptr:
				if(attr->atttypid != BPCHAROID)
					memcpy(memtuple_get_attr_ptr(start, bind, null_saves, nullp),
							DatumGetPointer(values[i]),
							bind->len
					      );
				else
				{
					if(VARATT_IS_SHORT(DatumGetPointer(values[i])))
					{
						attr_len = VARSIZE_SHORT(DatumGetPointer(values[i]));
						Assert(attr_len <= bind->len);
						memcpy(memtuple_get_attr_ptr(start, bind, null_saves, nullp),
								DatumGetPointer(values[i]),
								attr_len
						      );
					}
					else
					{
						char *p = memtuple_get_attr_ptr(start, bind, null_saves, nullp);
						Assert(VARATT_CAN_MAKE_SHORT(DatumGetPointer(values[i])));
						attr_len = VARSIZE(DatumGetPointer(values[i])) - VARHDRSZ + VARHDRSZ_SHORT;
						Assert(attr_len <= bind->len);
						*p = VARSIZE_TO_SHORT_D(values[i]);
						memcpy(p+1, VARDATA(DatumGetPointer(values[i])), attr_len-1);
					}
				}
				break;
			case MTB_ByRef:
				if(VARATT_IS_EXTERNAL(DatumGetPointer(values[i])))
				{
					varlen_start = (char *) att_align_nominal((long) varlen_start, attr->attalign);
					attr_len = VARSIZE_EXTERNAL(DatumGetPointer(values[i]));
					Assert((varlen_start - (char *) mtup) + attr_len <= len);
					memcpy(varlen_start, DatumGetPointer(values[i]), attr_len);
				}
				else if(VARATT_IS_SHORT(DatumGetPointer(values[i])))
				{
					attr_len = VARSIZE_SHORT(DatumGetPointer(values[i]));
					Assert((varlen_start - (char *) mtup) + attr_len <= len);
					memcpy(varlen_start, DatumGetPointer(values[i]), attr_len);
				}
				else if(attr->attstorage != 'p' &&
						value_type_could_short(DatumGetPointer(values[i]), attr->atttypid))
				{
					attr_len = VARSIZE(DatumGetPointer(values[i])) - VARHDRSZ + VARHDRSZ_SHORT;
					*varlen_start = VARSIZE_TO_SHORT_D(values[i]);
					Assert((varlen_start - (char *) mtup) + attr_len <= len);
					memcpy(varlen_start+1, VARDATA(DatumGetPointer(values[i])), attr_len-1);
				}
				else
				{
					/* Must be 4 byte header aligned varlena */
					varlen_start = (char *) att_align_nominal((long) varlen_start, attr->attalign);
					attr_len = VARSIZE(DatumGetPointer(values[i]));
					Assert((varlen_start - (char *) mtup) + attr_len <= len);
					memcpy(varlen_start, DatumGetPointer(values[i]), attr_len);
				}

				if(bind->len == 2)
					*(uint16 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp) = (uint16) (varlen_start - start);
				else
				{
					Assert(bind->len == 4);
					*(uint32 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp) = (uint32) (varlen_start - start);
				}

				varlen_start += attr_len;
				break;

			case MTB_ByRef_CStr:
				varlen_start = (char *) att_align_nominal((long) varlen_start, attr->attalign);
				attr_len = strlen(DatumGetCString(values[i])) + 1;
				Assert((varlen_start - (char *) mtup) + attr_len <= len);
				memcpy(varlen_start, DatumGetPointer(values[i]), attr_len);

				if(bind->len == 2)
					*(uint16 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp) = (uint16) (varlen_start - start);
				else
				{
					Assert(bind->len == 4);
					*(uint32 *) memtuple_get_attr_ptr(start, bind, null_saves, nullp) = (uint32) (varlen_start - start);
				}

				varlen_start += attr_len;
				break;
			default:
				Assert(!"Not valid binding type");
				break;
		}
	}

	Assert((varlen_start - (char *) mtup) <= len);

	/*
	 * Set values to their old values if we have changed their values
	 * during de-toasting, and release the space allocated during
	 * de-toasting.
	 */
	if (old_values != NULL)
	{
		for(i=0; i<pbind->tupdesc->natts; ++i)
		{
			if (DatumGetPointer(old_values[i]) != NULL)
			{
				Assert(DatumGetPointer(values[i]) != NULL);
				pfree(DatumGetPointer(values[i]));
				values[i] = old_values[i];
			}
		}
		pfree(old_values);
	}

	return mtup;
}

bool memtuple_attisnull(MemTuple mtup, MemTupleBinding *pbind, int attnum)
{
	MemTupleBindingCols *colbind = memtuple_get_islarge(mtup) ? &pbind->large_bind : &pbind->bind;
	Assert(mtup && pbind && pbind->tupdesc);
	Assert(attnum > 0);
	
	/*
	 * This used to be an Assert. However, we follow the logic of
	 * heap_attisnull() and treat attnums > lastatt as NULL. This
	 * is currently used in ALTER ADD COLUMN NOT NULL.
	 * 
	 * Unfortunately this also means that the caller needs to be
	 * extra careful passing in the correct attnum argument.
	 */
	if (attnum > (int) pbind->tupdesc->natts)
		return true;
	
	/*
	 * is there a NULL value in any of the attributes?
	 */
	if(!memtuple_get_hasnull(mtup))
		return false;
	
	return (mtup->PRIVATE_mt_bits[colbind->bindings[attnum-1].null_byte] & colbind->bindings[attnum-1].null_mask);
}

static Datum memtuple_getattr_by_alignment(MemTuple mtup, MemTupleBinding *pbind, int attnum, bool *isnull, bool use_null_saves_aligned)
{
	bool hasnull = memtuple_get_hasnull(mtup);
	unsigned char *nullp = hasnull ? memtuple_get_nullp(mtup, pbind) : NULL; 
	char *start = (char *) mtup + (hasnull ? pbind->null_bitmap_extra_size : 0);

	Datum ret;
	MemTupleBindingCols *colbind = memtuple_get_islarge(mtup) ? &pbind->large_bind : &pbind->bind;
	MemTupleAttrBinding *attrbind;

	Assert(mtup && pbind && pbind->tupdesc);
	Assert(attnum > 0 && attnum <= pbind->tupdesc->natts);

	if(isnull)
		*isnull = false;

	/* input attnum is 1 based.  Make it 0 based */
	--attnum;
	attrbind = &(colbind->bindings[attnum]);
	
	/* null check */
	if(hasnull && (nullp[attrbind->null_byte] & attrbind->null_mask))
	{
		if(isnull)
			*isnull = true;
		return 0;
	}

	short *null_saves = (use_null_saves_aligned ? colbind->null_saves_aligned : colbind->null_saves);
	Assert(null_saves);

	ret = fetchatt(pbind->tupdesc->attrs[attnum], memtuple_get_attr_data_ptr(start, attrbind, null_saves, nullp));

	return ret;
}

Datum memtuple_getattr(MemTuple mtup, MemTupleBinding *pbind, int attnum, bool *isnull)
{
	return memtuple_getattr_by_alignment(mtup, pbind, attnum, isnull, true /* aligned */);
}


MemTuple memtuple_copy_to(MemTuple mtup, MemTuple dest, uint32 *destlen)
{
	uint32 len = memtuple_get_size(mtup);

	if(!destlen)
		dest = (MemTuple) palloc(len);
	else
	{
		if(*destlen < len)
		{
			*destlen = len;
			return NULL;
		}

		*destlen = len;
	}

	memcpy((char *) dest, (char *) mtup, len);
	return dest;
}

static void memtuple_get_values(MemTuple mtup, MemTupleBinding *pbind, Datum *datum, bool *isnull, bool use_null_saves_aligned)
{
	int i;
	for(i=0; i<pbind->tupdesc->natts; ++i)
		datum[i] = memtuple_getattr_by_alignment(mtup, pbind, i+1, &isnull[i], use_null_saves_aligned);
}

void memtuple_deform(MemTuple mtup, MemTupleBinding *pbind, Datum *datum, bool *isnull)
{
	memtuple_get_values(mtup, pbind, datum, isnull, true /* aligned */);
}


/*
 * Deform a memtuple with old binding alignment.
 *
 * We assume that the 'mtup' was created using null_saves, where the
 * binding length is not aligned to the following binding's alignment. In
 * this case, we create an "upgraded" clone using null_saves_aligned, which
 * uses properly aligned binding length.
 */
void
memtuple_deform_misaligned(MemTuple mtup, MemTupleBinding *pbind,
						   Datum *datum, bool *isnull)
{
	memtuple_get_values(mtup, pbind, datum, isnull, false /* aligned */);
}

/*
 * Get the Oid assigned to this tuple (when WITH OIDS is used).
 *
 * Note that similarly to HeapTupleGetOid this function will 
 * sometimes get called when no oid is assigned, in which case
 * we return InvalidOid. It is possible to make the check earlier
 * and avoid this call but for simplicity and compatibility with
 * the HeapTuple interface we keep it the same. 
 */
Oid MemTupleGetOid(MemTuple mtup, MemTupleBinding *pbind)
{
	Assert(pbind);
		
	if(!mtbind_has_oid(pbind))
		return InvalidOid;

	return ((Oid *) mtup)[1];
}

void MemTupleSetOid(MemTuple mtup, MemTupleBinding *pbind __attribute__((unused)), Oid oid)
{
	Assert(pbind && mtbind_has_oid(pbind));
	((Oid *) mtup)[1] = oid;
}


bool MemTupleHasExternal(MemTuple mtup, MemTupleBinding *pbind)
{
	MemTupleBindingCols *colbind = memtuple_get_islarge(mtup) ? &pbind->large_bind : &pbind->bind;
	int i;

	for(i=0; i<pbind->tupdesc->natts; ++i)
	{
		MemTupleAttrBinding *attrbind = &(colbind->bindings[i]);
		if(attrbind->flag == MTB_ByRef)
		{
			bool isnull;
			Datum d = memtuple_getattr(mtup, pbind, i+1, &isnull);
			if(!isnull)
			{
				if(VARATT_IS_EXTERNAL(DatumGetPointer(d)))
					return true;
			}
		}
	}

	return false;
}

/*
 * Check if a memtuple has null attributes with bindings that can possibly be misaligned.
 *
 * MPP-7372: This is an issue only for memtuples stored in AO tables before applying
 * the fix that enforces the proper alignment of the binding length.
 */
bool memtuple_has_misaligned_attribute(MemTuple mtup, MemTupleBinding *pbind)
{
 	Assert(mtup);
	Assert(pbind);

	/* Check if the memtuple has an attribute with mismatching alignment and length */
	if (!(pbind->bind.has_null_saves_alignment_mismatch))
	{
		return false;
	}

	/*
	 * Check if the memtuple has a dropped attribute with mismatching alignment and length.
	 * Dropped attributes are treated as null.
	 */
	if (pbind->bind.has_dropped_attr_alignment_mismatch)
	{
		return true;
	}

	/* Check if the memtuple has no null values */
	if (!(memtuple_get_hasnull(mtup)))
	{
		return false;
	}

	unsigned char *nullp = memtuple_get_nullp(mtup, pbind);

	int attr_idx = 0;

	/*
	 * Check if an attribute with mismatching alignment and length is null.
	 */
	for (attr_idx = 0; attr_idx < pbind->tupdesc->natts; attr_idx++)
	{
		MemTupleAttrBinding *bind = &pbind->bind.bindings[attr_idx];

		if (bind->len != bind->len_aligned &&
			(nullp[bind->null_byte] & bind->null_mask))
		{
			return true;
		}
	}

	return false;
}
