/*-------------------------------------------------------------------------
 *
 * memtup.h
 *	  In Memory Tuple format
 *
 * Portions Copyright (c) 2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/memtup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMTUP_H
#define MEMTUP_H

#include "access/tupdesc.h"

typedef enum MemTupleBindFlag
{
	MTB_ByVal_Native = 1,	/* Fixed len, native (returned as datum ) */
	MTB_ByVal_Ptr    = 2,	/* Fixed len, convert to pointer for datum */
	MTB_ByRef  	 = 3,	/* var len */
	MTB_ByRef_CStr   = 4,   /* varlen, CString type */
} MemTupleBindFlag;

typedef struct MemTupleAttrBinding
{
	int offset; 		/* offset of attr in memtuple */
	int len;				/* attribute length */
	int len_aligned;		/* attribute length, padded for aligning the physically following attribute */
	MemTupleBindFlag flag;	/* binding flag */
	int null_byte;		/* which byte holds the null flag for the attr */
	unsigned char null_mask;		/* null bit mask */
} MemTupleAttrBinding;

typedef struct MemTupleBindingCols
{
	uint32 var_start; 	/* varlen fields start */
	MemTupleAttrBinding *bindings; /* bindings for attrs (cols) */
	short *null_saves;				/* saved space from each attribute when null */
	short *null_saves_aligned;		/* saved space from each attribute when null - uses aligned length */
	bool has_null_saves_alignment_mismatch;		/* true if one or more attributes has mismatching alignment and length  */
	bool has_dropped_attr_alignment_mismatch;	/* true if one or more dropped attributes has mismatching alignment and length */
} MemTupleBindingCols;

typedef struct MemTupleBinding
{
	TupleDesc tupdesc;
	int column_align;
	int null_bitmap_extra_size;  /* extra bytes required by null bitmap */

	MemTupleBindingCols bind;  	/* 2 bytes offsets */
	MemTupleBindingCols large_bind; /* large tup, 4 bytes offsets */
} MemTupleBinding;

typedef struct MemTupleData
{
	uint32 PRIVATE_mt_len;
	unsigned char PRIVATE_mt_bits[1]; 	/* varlen */
} MemTupleData;

typedef MemTupleData *MemTuple;

#define MEMTUP_LEAD_BIT 0x80000000
#define MEMTUP_LEN_MASK 0x3FFFFFF8
#define MEMTUP_HASNULL   1
#define MEMTUP_LARGETUP  2
#define MEMTUP_HASEXTERNAL 	 4

/*
 * MEMTUP_HAS_MATCH is a temporary flag used during hash joins. It's
 * equivalent to HEAP_TUPLE_HAS_MATCH for HeapTuples, but in GPDB we
 * store MemTuples in hash table instead of HeapTuples.
 */
#define MEMTUP_HAS_MATCH 0x40000000

#define MEMTUP_ALIGN(LEN) TYPEALIGN(8, (LEN)) 
#define MEMTUPLE_LEN_FITSHORT 0xFFF0

static inline bool is_len_memtuplen(uint32 len)
{
	return (len & MEMTUP_LEAD_BIT) != 0;
}
static inline bool memtuple_lead_bit_set(MemTuple tup)
{
	return (tup->PRIVATE_mt_len & MEMTUP_LEAD_BIT) != 0;
}
static inline uint32 memtuple_size_from_uint32(uint32 len)
{
	Assert ((len & MEMTUP_LEAD_BIT) != 0);
	return len & MEMTUP_LEN_MASK;
}
static inline uint32 memtuple_get_size(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	return (mtup->PRIVATE_mt_len & MEMTUP_LEN_MASK);
}
static inline void memtuple_set_mtlen(MemTuple mtup, uint32 mtlen)
{
	Assert((mtlen & MEMTUP_LEAD_BIT) != 0);
	mtup->PRIVATE_mt_len = mtlen;
}
static inline bool memtuple_get_hasnull(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	return (mtup->PRIVATE_mt_len & MEMTUP_HASNULL) != 0;
}
static inline void memtuple_set_hasnull(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	mtup->PRIVATE_mt_len |= MEMTUP_HASNULL;
}
static inline bool memtuple_get_islarge(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	return (mtup->PRIVATE_mt_len & MEMTUP_LARGETUP) != 0;
}
static inline void memtuple_set_islarge(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	mtup->PRIVATE_mt_len |= MEMTUP_LARGETUP; 
}
static inline bool memtuple_get_hasext(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	return (mtup->PRIVATE_mt_len & MEMTUP_HASEXTERNAL) != 0;
}
static inline void memtuple_set_hasext(MemTuple mtup)
{
	Assert(memtuple_lead_bit_set(mtup));
	mtup->PRIVATE_mt_len |= MEMTUP_HASEXTERNAL;
}


extern void destroy_memtuple_binding(MemTupleBinding *pbind);
extern MemTupleBinding* create_memtuple_binding(TupleDesc tupdesc);

extern Datum memtuple_getattr(MemTuple mtup, MemTupleBinding *pbind, int attnum, bool *isnull);
extern bool memtuple_attisnull(MemTuple mtup, MemTupleBinding *pbind, int attnum);

extern uint32 compute_memtuple_size(MemTupleBinding *pbind, Datum *values, bool *isnull, uint32 *nullsaves, bool *has_nulls);

extern MemTuple memtuple_copy(MemTuple mtup);
extern MemTuple memtuple_form(MemTupleBinding *pbind, Datum *values, bool *isnull);
extern void memtuple_form_to(MemTupleBinding *pbind, Datum *values, bool *isnull,
							 uint32 len, uint32 null_save_len, bool hasnull,
							 MemTuple mtup);
extern void memtuple_deform(MemTuple mtup, MemTupleBinding *pbind, Datum *datum, bool *isnull);
extern void memtuple_deform_misaligned(MemTuple mtup, MemTupleBinding *pbind, Datum *datum, bool *isnull);

static inline bool
MemTupleHasMatch(MemTuple mtup)
{
	return (mtup->PRIVATE_mt_len & MEMTUP_HAS_MATCH) != 0;
}

static inline void
MemTupleSetMatch(MemTuple mtup)
{
	mtup->PRIVATE_mt_len |= MEMTUP_HAS_MATCH;
}

static inline void
MemTupleClearMatch(MemTuple mtup)
{
	mtup->PRIVATE_mt_len &= ~MEMTUP_HAS_MATCH;
}

extern bool MemTupleHasExternal(MemTuple mtup, MemTupleBinding *pbind);

extern bool memtuple_has_misaligned_attribute(MemTuple mtup, MemTupleBinding *pbind);

#endif /* MEMTUP_H */
