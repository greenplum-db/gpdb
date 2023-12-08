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
#define FIELDNO_MEMTUPLEATTRBINDING_OFFSET 0
	int offset; 		/* offset of attr in memtuple */
#define FIELDNO_MEMTUPLEATTRBINDING_LEN 1
	int len;				/* attribute length */
	int len_aligned;		/* attribute length, padded for aligning the physically following attribute */
	MemTupleBindFlag flag;	/* binding flag */
#define FIELDNO_MEMTUPLEATTRBINDING_NULLBYTE 4
	int null_byte;		/* which byte holds the null flag for the attr */
#define FIELDNO_MEMTUPLEATTRBINDING_NULLMASK 5
	unsigned char null_mask;		/* null bit mask */
} MemTupleAttrBinding;

typedef struct MemTupleBindingCols
{
	uint32 var_start; 	/* varlen fields start */
#define FIELDNO_MEMTUPLEBINDINGCOLS_BINDINGS 1
	MemTupleAttrBinding *bindings; /* bindings for attrs (cols) */
#define FIELDNO_MEMTUPLEBINDINGCOLS_NULLSAVES 2
	short *null_saves;		/* saved space from each attribute when null - uses aligned length */
} MemTupleBindingCols;

typedef struct MemTupleBinding
{
	TupleDesc tupdesc;
	int column_align;
#define FIELDNO_MEMTUPLEBINDING_NULLBITMAPEXTRASIZE 2
	int null_bitmap_extra_size;  /* extra bytes required by null bitmap */
#define FIELDNO_MEMTUPLEBINDING_NATTS 3
	int natts; 			/* number of attributes in memtuple (note: it could be smaller than tupdesc->natts) */
#define FIELDNO_MEMTUPLEBINDING_COLBIND 4
	MemTupleBindingCols bind;  	/* 2 bytes offsets */
#define FIELDNO_MEMTUPLEBINDING_LCOLBIND 5
	MemTupleBindingCols large_bind; /* large tup, 4 bytes offsets */
} MemTupleBinding;

typedef struct MemTupleData
{
#define FIELDNO_MEMTUPLEDATA_HEADER 0
	uint32 PRIVATE_mt_len;
#define FIELDNO_MEMTUPLEDATA_DATA 1
	unsigned char PRIVATE_mt_bits[1]; 	/* varlen */
} MemTupleData;

typedef MemTupleData *MemTuple;

#define MEMTUP_LEAD_BIT 0x80000000
#define MEMTUP_LEN_MASK 0x3FFFFFF8
#define MEMTUP_HASNULL   1
#define MEMTUP_LARGETUP  2
#define MEMTUP_HASEXTERNAL 	 4

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
extern MemTupleBinding* create_memtuple_binding(TupleDesc tupdesc, int expected_natts);

extern Datum memtuple_getattr(MemTuple mtup, MemTupleBinding *pbind, int attnum, bool *isnull);

extern uint32 compute_memtuple_size(MemTupleBinding *pbind, Datum *values, bool *isnull, uint32 *nullsaves, bool *has_nulls);

extern MemTuple memtuple_form(MemTupleBinding *pbind, Datum *values, bool *isnull);
extern MemTuple memtuple_form_to(MemTupleBinding *pbind, Datum *values, bool *isnull,
								 uint32 len, uint32 null_save_len, bool hasnull,
								 MemTuple mtup);
extern void memtuple_deform(MemTuple mtup, MemTupleBinding *pbind, Datum *datum, bool *isnull);

extern bool MemTupleHasExternal(MemTuple mtup, MemTupleBinding *pbind);

extern MemTuple memtuple_copytuple(MemTuple tuple);
#endif /* MEMTUP_H */
