/*-------------------------------------------------------------------------
 *
 * postgres.h
 *	  Primary include file for PostgreSQL server .c files
 *
 * This should be the first file included by PostgreSQL backend modules.
 * Client-side code should include postgres_fe.h instead.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1995, Regents of the University of California
 *
 * src/include/postgres.h
 *
 *-------------------------------------------------------------------------
 */
/*
 *----------------------------------------------------------------
 *	 TABLE OF CONTENTS
 *
 *		When adding stuff to this file, please try to put stuff
 *		into the relevant section, or add new sections as appropriate.
 *
 *	  section	description
 *	  -------	------------------------------------------------
 *		1)		variable-length datatypes (TOAST support)
 *		2)		Datum type + support macros
 *
 *	 NOTES
 *
 *	In general, this file should contain declarations that are widely needed
 *	in the backend environment, but are of no interest outside the backend.
 *
 *	Simple type definitions live in c.h, where they are shared with
 *	postgres_fe.h.  We do that since those type definitions are needed by
 *	frontend modules that want to deal with binary data transmission to or
 *	from the backend.  Type definitions in this file should be for
 *	representations that never escape the backend, such as Datum or
 *	TOASTed varlena objects.
 *
 *----------------------------------------------------------------
 */
#ifndef POSTGRES_H
#define POSTGRES_H

#include "c.h"
#include "utils/elog.h"
#include "utils/palloc.h"
#include "storage/itemptr.h"

/* ----------------------------------------------------------------
 *				Section 1:	variable-length datatypes (TOAST support)
 * ----------------------------------------------------------------
 */

/*
 * struct varatt_external is a traditional "TOAST pointer", that is, the
 * information needed to fetch a Datum stored out-of-line in a TOAST table.
 * The data is compressed if and only if va_extsize < va_rawsize - VARHDRSZ.
 * This struct must not contain any padding, because we sometimes compare
 * these pointers using memcmp.
 *
 * Note that this information is stored unaligned within actual tuples, so
 * you need to memcpy from the tuple into a local struct variable before
 * you can look at these fields!  (The reason we use memcmp is to avoid
 * having to do that just to detect equality of two TOAST pointers...)
 */
typedef struct varatt_external
{
	int32		va_rawsize;		/* Original data size (includes header) */
	int32		va_extsize;		/* External saved size (doesn't) */
	Oid			va_valueid;		/* Unique ID of value within TOAST table */
	Oid			va_toastrelid;	/* RelID of TOAST table containing it */
}			varatt_external;

/*
 * struct varatt_indirect is a "TOAST pointer" representing an out-of-line
 * Datum that's stored in memory, not in an external toast relation.
 * The creator of such a Datum is entirely responsible that the referenced
 * storage survives for as long as referencing pointer Datums can exist.
 *
 * Note that just as for struct varatt_external, this struct is stored
 * unaligned within any containing tuple.
 */
typedef struct varatt_indirect
{
	struct varlena *pointer;	/* Pointer to in-memory varlena */
}			varatt_indirect;

/*
 * struct varatt_expanded is a "TOAST pointer" representing an out-of-line
 * Datum that is stored in memory, in some type-specific, not necessarily
 * physically contiguous format that is convenient for computation not
 * storage.  APIs for this, in particular the definition of struct
 * ExpandedObjectHeader, are in src/include/utils/expandeddatum.h.
 *
 * Note that just as for struct varatt_external, this struct is stored
 * unaligned within any containing tuple.
 */
typedef struct ExpandedObjectHeader ExpandedObjectHeader;

typedef struct varatt_expanded
{
	ExpandedObjectHeader *eohptr;
} varatt_expanded;

/*
 * Type tag for the various sorts of "TOAST pointer" datums.  The peculiar
 * value for VARTAG_ONDISK comes from a requirement for on-disk compatibility
 * with a previous notion that the tag field was the pointer datum's length.
 *
 * GPDB: In PostgreSQL VARTAG_ONDISK is set to 18 in order to match the
 * historic (VARHDRSZ_EXTERNAL + sizeof(struct varatt_external)) value of the
 * pointer datum's length. In Greenplum VARHDRSZ_EXTERNAL is two bytes longer
 * than PostgreSQL due to extra padding in varattrib_1b_e, so VARTAG_ONDISK has
 * to be set to 20.
 */
typedef enum vartag_external
{
	VARTAG_INDIRECT = 1,
	VARTAG_EXPANDED_RO = 2,
	VARTAG_EXPANDED_RW = 3,
	VARTAG_ONDISK = 20
} vartag_external;

/* this test relies on the specific tag values above */
#define VARTAG_IS_EXPANDED(tag) \
	(((tag) & ~1) == VARTAG_EXPANDED_RO)

#define VARTAG_SIZE(tag) \
	((tag) == VARTAG_INDIRECT ? sizeof(varatt_indirect) : \
	 VARTAG_IS_EXPANDED(tag) ? sizeof(varatt_expanded) : \
	 (tag) == VARTAG_ONDISK ? sizeof(varatt_external) : \
	 TrapMacro(true, "unrecognized TOAST vartag"))

/*
 * These structs describe the header of a varlena object that may have been
 * TOASTed.  Generally, don't reference these structs directly, but use the
 * macros below.
 *
 * We use separate structs for the aligned and unaligned cases because the
 * compiler might otherwise think it could generate code that assumes
 * alignment while touching fields of a 1-byte-header varlena.
 */
typedef union
{
	struct						/* Normal varlena (4-byte length) */
	{
		uint32		va_header;
		char		va_data[FLEXIBLE_ARRAY_MEMBER];
	}			va_4byte;
	struct						/* Compressed-in-line format */
	{
		uint32		va_header;
		uint32		va_rawsize; /* Original data size (excludes header) */
		char		va_data[FLEXIBLE_ARRAY_MEMBER]; /* Compressed data */
	}			va_compressed;
} varattrib_4b;

typedef struct
{
	uint8		va_header;
	char		va_data[FLEXIBLE_ARRAY_MEMBER]; /* Data begins here */
} varattrib_1b;

/* NOT Like Postgres! ...In GPDB, We waste a few bytes of padding */
/* TOAST pointers are a subset of varattrib_1b with an identifying tag byte */
typedef struct
{
	uint8		va_header;		/* Always 0x80  */
	uint8		va_tag;			/* Type of datum */
	uint8		va_padding[2];	/*** GPDB only:  Alignment padding ***/
	char		va_data[FLEXIBLE_ARRAY_MEMBER]; /* Type-specific data */
} varattrib_1b_e;

/*
 * Bit layouts for varlena headers: (GPDB always stores this big-endian format)
 *
 * 00xxxxxx 4-byte length word, aligned, uncompressed data (up to 1G)
 * 01xxxxxx 4-byte length word, aligned, *compressed* data (up to 1G)
 * 10000000 1-byte length word, unaligned, TOAST pointer
 * 1xxxxxxx 1-byte length word, unaligned, uncompressed data (up to 126b)
 *
 * Greenplum differs from PostgreSQL here... In Postgres, they use different
 * macros for big-endian and little-endian machines, so the length is contiguous,
 * while the 4 byte lengths are stored in native endian format.
 *
 * Greenplum stored the 4 byte varlena header in network byte order, so it always
 * look big-endian in the tuple.   This is a bit ugly, but changing it would require
 * all our customers to initdb.
 *
 * The "xxx" bits are the length field (which includes itself in all cases).
 * In the big-endian case we mask to extract the length.
 * Note that in both cases the flag bits are in the physically
 * first byte.  Also, it is not possible for a 1-byte length word to be zero;
 * this lets us disambiguate alignment padding bytes from the start of an
 * unaligned datum.  (We now *require* pad bytes to be filled with zero!)
 *
 * In TOAST pointers the va_tag field (see varattrib_1b_e) is used to discern
 * the specific type and length of the pointer datum.
 */

/*
 * Endian-dependent macros.  These are considered internal --- use the
 * external macros below instead of using these directly.
 *
 * Note: IS_1B is true for external toast records but VARSIZE_1B will return 0
 * for such records. Hence you should usually check for IS_EXTERNAL before
 * checking for IS_1B.
 */

#define VARATT_IS_4B(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x80) == 0x00)
#define VARATT_IS_4B_U(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0xC0) == 0x00)
#define VARATT_IS_4B_C(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0xC0) == 0x40)
#define VARATT_IS_1B(PTR) \
	((((varattrib_1b *) (PTR))->va_header & 0x80) == 0x80)
#define VARATT_IS_1B_E(PTR) \
	((((varattrib_1b *) (PTR))->va_header) == 0x80)
#define VARATT_NOT_PAD_BYTE(PTR) \
	(*((uint8 *) (PTR)) != 0)

/* VARSIZE_4B() should only be used on known-aligned data */
#define VARSIZE_4B(PTR) \
	(ntohl(((varattrib_4b *) (PTR))->va_4byte.va_header) & 0x3FFFFFFF)
#define VARSIZE_1B(PTR) \
	(((varattrib_1b *) (PTR))->va_header & 0x7F)
#define VARTAG_1B_E(PTR) \
	(((varattrib_1b_e *) (PTR))->va_tag)

#define SET_VARSIZE_4B(PTR,len) \
	(((varattrib_4b *) (PTR))->va_4byte.va_header = htonl( (len) & 0x3FFFFFFF ))
#define SET_VARSIZE_4B_C(PTR,len) \
	(((varattrib_4b *) (PTR))->va_4byte.va_header = htonl( ((len) & 0x3FFFFFFF) | 0x40000000 ))
#define SET_VARSIZE_1B(PTR,len) \
	(((varattrib_1b *) (PTR))->va_header = (len) | 0x80)
#define SET_VARTAG_1B_E(PTR,tag) \
	(((varattrib_1b_e *) (PTR))->va_header = 0x80, \
	 ((varattrib_1b_e *) (PTR))->va_tag = (tag))

#define VARHDRSZ_SHORT			offsetof(varattrib_1b, va_data)
#define VARATT_SHORT_MAX		0x7F
#define VARATT_CAN_MAKE_SHORT(PTR) \
	(VARATT_IS_4B_U(PTR) && \
	 (VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT) <= VARATT_SHORT_MAX)
#define VARATT_CONVERTED_SHORT_SIZE(PTR) \
	(VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT)

/* In Postgres, this is 2, but in GPDB, it's 4, due to padding */
#define VARHDRSZ_EXTERNAL		offsetof(varattrib_1b_e, va_data)

#define VARDATA_4B(PTR)		(((varattrib_4b *) (PTR))->va_4byte.va_data)
#define VARDATA_4B_C(PTR)	(((varattrib_4b *) (PTR))->va_compressed.va_data)
#define VARDATA_1B(PTR)		(((varattrib_1b *) (PTR))->va_data)
#define VARDATA_1B_E(PTR)	(((varattrib_1b_e *) (PTR))->va_data)

#define VARRAWSIZE_4B_C(PTR) \
	(((varattrib_4b *) (PTR))->va_compressed.va_rawsize)

/* Externally visible macros */

/*
 * In consumers oblivious to data alignment, call PG_DETOAST_DATUM_PACKED(),
 * VARDATA_ANY(), VARSIZE_ANY() and VARSIZE_ANY_EXHDR().  Elsewhere, call
 * PG_DETOAST_DATUM(), VARDATA() and VARSIZE().  Directly fetching an int16,
 * int32 or wider field in the struct representing the datum layout requires
 * aligned data.  memcpy() is alignment-oblivious, as are most operations on
 * datatypes, such as text, whose layout struct contains only char fields.
 *
 * Code assembling a new datum should call VARDATA() and SET_VARSIZE().
 * (Datums begin life untoasted.)
 *
 * Other macros here should usually be used only by tuple assembly/disassembly
 * code and code that specifically wants to work with still-toasted Datums.
 */
#define VARDATA(PTR)						VARDATA_4B(PTR)
#define VARSIZE(PTR)						VARSIZE_4B(PTR)

#define VARSIZE_SHORT(PTR)					VARSIZE_1B(PTR)
#define VARDATA_SHORT(PTR)					VARDATA_1B(PTR)

#define VARTAG_EXTERNAL(PTR)				VARTAG_1B_E(PTR)
#define VARSIZE_EXTERNAL(PTR)				(VARHDRSZ_EXTERNAL + VARTAG_SIZE(VARTAG_EXTERNAL(PTR)))
#define VARDATA_EXTERNAL(PTR)				VARDATA_1B_E(PTR)

#define VARATT_IS_COMPRESSED(PTR)			VARATT_IS_4B_C(PTR)
#define VARATT_IS_EXTERNAL(PTR)				VARATT_IS_1B_E(PTR)
#define VARATT_IS_EXTERNAL_ONDISK(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_ONDISK)
#define VARATT_IS_EXTERNAL_INDIRECT(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_INDIRECT)
#define VARATT_IS_EXTERNAL_EXPANDED_RO(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_EXPANDED_RO)
#define VARATT_IS_EXTERNAL_EXPANDED_RW(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_EXTERNAL(PTR) == VARTAG_EXPANDED_RW)
#define VARATT_IS_EXTERNAL_EXPANDED(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && VARTAG_IS_EXPANDED(VARTAG_EXTERNAL(PTR)))
#define VARATT_IS_EXTERNAL_NON_EXPANDED(PTR) \
	(VARATT_IS_EXTERNAL(PTR) && !VARTAG_IS_EXPANDED(VARTAG_EXTERNAL(PTR)))
#define VARATT_IS_SHORT(PTR)				VARATT_IS_1B(PTR)
#define VARATT_IS_EXTENDED(PTR)				(!VARATT_IS_4B_U(PTR))

#define SET_VARSIZE(PTR, len)				SET_VARSIZE_4B(PTR, len)
#define SET_VARSIZE_SHORT(PTR, len)			SET_VARSIZE_1B(PTR, len)
#define SET_VARSIZE_COMPRESSED(PTR, len)	SET_VARSIZE_4B_C(PTR, len)

#define SET_VARTAG_EXTERNAL(PTR, tag)		SET_VARTAG_1B_E(PTR, tag)

#define VARSIZE_ANY(PTR) \
	(VARATT_IS_1B_E(PTR) ? VARSIZE_EXTERNAL(PTR) : \
	 (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR) : \
	  VARSIZE_4B(PTR)))

/* Size of a varlena data, excluding header */
#define VARSIZE_ANY_EXHDR(PTR) \
	(VARATT_IS_1B_E(PTR) ? VARSIZE_EXTERNAL(PTR)-VARHDRSZ_EXTERNAL : \
	 (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR)-VARHDRSZ_SHORT : \
	  VARSIZE_4B(PTR)-VARHDRSZ))

/* caution: this will not work on an external or compressed-in-line Datum */
/* caution: this will return a possibly unaligned pointer */
#define VARDATA_ANY(PTR) \
	 (VARATT_IS_1B(PTR) ? VARDATA_1B(PTR) : VARDATA_4B(PTR))


/* ----------------------------------------------------------------
 *				Section 2:	Datum type + support macros
 * ----------------------------------------------------------------
 */

/*
 * A Datum contains either a value of a pass-by-value type or a pointer to a
 * value of a pass-by-reference type.  Therefore, we require:
 *
 * sizeof(Datum) == sizeof(void *) == 4 or 8
 *
 *  Greenplum CDB:
 *     Datum is always 8 bytes, regardless if it is 32bit or 64bit machine.
 *  so may be > sizeof(void *). To align with postgres, which defines Datum as
 *  uintptr_t type, it is defined as a uintptr_t to make sure the raw Datum
 *  comparator work. GPDB's document requires a x86_64 environment where
 *  uintptr_t is 64bits which doesn't violate the original 64bits definition.
 *  Although it is unclear why did GPDB had that restriction at the beginning.
 *
 * The macros below and the analogous macros for other types should be used to
 * convert between a Datum and the appropriate C type.
 */

typedef uintptr_t Datum;
typedef union Datum_U
{
	Datum d;

	float4 f4[2];
	float8 f8;

	void *ptr;
} Datum_U;

#define SIZEOF_DATUM 8

typedef Datum *DatumPtr;

#define GET_1_BYTE(datum)	(((Datum) (datum)) & 0x000000ff)
#define GET_2_BYTES(datum)	(((Datum) (datum)) & 0x0000ffff)
#define GET_4_BYTES(datum)	(((Datum) (datum)) & 0xffffffff)
#if SIZEOF_DATUM == 8
#define GET_8_BYTES(datum)	((Datum) (datum))
#endif
#define SET_1_BYTE(value)	(((Datum) (value)) & 0x000000ff)
#define SET_2_BYTES(value)	(((Datum) (value)) & 0x0000ffff)
#define SET_4_BYTES(value)	(((Datum) (value)) & 0xffffffff)
#if SIZEOF_DATUM == 8
#define SET_8_BYTES(value)	((Datum) (value))
#endif
/*
 * A NullableDatum is used in places where both a Datum and its nullness needs
 * to be stored. This can be more efficient than storing datums and nullness
 * in separate arrays, due to better spatial locality, even if more space may
 * be wasted due to padding.
 */
typedef struct NullableDatum
{
#define FIELDNO_NULLABLE_DATUM_DATUM 0
	Datum		value;
#define FIELDNO_NULLABLE_DATUM_ISNULL 1
	bool		isnull;
	/* due to alignment padding this could be used for flags for free */
} NullableDatum;


/* 
 * Conversion between Datum and type X.  Changed from Macro to static inline
 * functions to get proper type checking.
 */

/*
 * DatumGetBool
 *		Returns boolean value of a datum.
 *
 * Note: any nonzero value will be considered true.
 */
static inline bool DatumGetBool(Datum d) { return ((bool)d) != 0; }
static inline Datum BoolGetDatum(bool b) { return (b ? 1 : 0); } 

static inline char DatumGetChar(Datum d) { return (char) d; }
static inline Datum CharGetDatum(char c) { return (Datum) c; } 

static inline int8 DatumGetInt8(Datum d) { return (int8) d; } 
static inline Datum Int8GetDatum(int8 i8) { return (Datum) i8; }

static inline uint8 DatumGetUInt8(Datum d) { return (uint8) d; } 
static inline Datum UInt8GetDatum(uint8 ui8) { return (Datum) ui8; } 

static inline int16 DatumGetInt16(Datum d) { return (int16) d; } 
static inline Datum Int16GetDatum(int16 i16) { return (Datum) i16; } 

static inline uint16 DatumGetUInt16(Datum d) { return (uint16) d; } 
static inline Datum UInt16GetDatum(uint16 ui16) { return (Datum) ui16; } 

static inline int32 DatumGetInt32(Datum d) { return (int32) d; } 
static inline Datum Int32GetDatum(int32 i32) { return (Datum) i32; } 

static inline uint32 DatumGetUInt32(Datum d) { return (uint32) d; } 
static inline Datum UInt32GetDatum(uint32 ui32) { return (Datum) ui32; } 

static inline int64 DatumGetInt64(Datum d) { return (int64) d; } 
static inline Datum Int64GetDatum(int64 i64) { return (Datum) i64; } 
static inline Datum Int64GetDatumFast(int64 x) { return Int64GetDatum(x); } 

/*
 * DatumGetUInt64
 *		Returns 64-bit unsigned integer value of a datum.
 *
 * Note: this macro hides whether int64 is pass by value or by reference.
 */

#ifdef USE_FLOAT8_BYVAL
#define DatumGetUInt64(X) ((uint64) (X))
#else
#define DatumGetUInt64(X) (* ((uint64 *) DatumGetPointer(X)))
#endif

/*
 * UInt64GetDatum
 *		Returns datum representation for a 64-bit unsigned integer.
 *
 * Note: if int64 is pass by reference, this function returns a reference
 * to palloc'd space.
 */

#ifdef USE_FLOAT8_BYVAL
#define UInt64GetDatum(X) ((Datum) (X))
#else
#define UInt64GetDatum(X) Int64GetDatum((int64) (X))
#endif

static inline Oid DatumGetObjectId(Datum d) { return (Oid) d; } 
static inline Datum ObjectIdGetDatum(Oid oid) { return (Datum) oid; } 

static inline TransactionId DatumGetTransactionId(Datum d) { return (TransactionId) d; } 
static inline Datum TransactionIdGetDatum(TransactionId tid) { return (Datum) tid; } 

static inline TransactionId DatumGetMultiXactId(Datum d) { return (TransactionId) d; } 
static inline Datum MultiXactIdGetDatum(TransactionId tid) { return (Datum) tid; } 

static inline CommandId DatumGetCommandId(Datum d) { return (CommandId) d; } 
static inline Datum CommandIdGetDatum(CommandId cid) { return (Datum) cid; } 

/*
 * DatumGetPointer
 *		Returns pointer value of a datum.
 */

#define DatumGetPointer(X) ((Pointer) (X))

/*
 * PointerGetDatum
 *		Returns datum representation for a pointer.
 */

#define PointerGetDatum(X) ((Datum) (X))

static inline char *DatumGetCString(Datum d) { return (char* ) DatumGetPointer(d); } 
static inline Datum CStringGetDatum(const char *p) { return PointerGetDatum(p); }

static inline Name DatumGetName(Datum d) { return (Name) DatumGetPointer(d); }
static inline Datum NameGetDatum(const Name n) { return PointerGetDatum(n); }

#ifndef WORDS_BIGENDIAN 
static inline float4 DatumGetFloat4(Datum d) { Datum_U du; du.d = d; return du.f4[0]; } 
static inline Datum Float4GetDatum(float4 f) { Datum_U du; du.d = 0; du.f4[0] = f; return du.d; } 
#else
static inline float4 DatumGetFloat4(Datum d) { Datum_U du; du.d = d; return du.f4[1]; } 
static inline Datum Float4GetDatum(float4 f) { Datum_U du; du.d = 0; du.f4[1] = f; return du.d; } 
#endif

static inline float8 DatumGetFloat8(Datum d) { Datum_U du; du.d = d; return du.f8; } 
static inline Datum Float8GetDatum(float8 f) { Datum_U du; du.f8 = f; return du.d; }
static inline Datum Float8GetDatumFast(float8 f) { return Float8GetDatum(f); }

static inline ItemPointer DatumGetItemPointer(Datum d) { return (ItemPointer) DatumGetPointer(d); }
static inline Datum ItemPointerGetDatum(ItemPointer i) { return PointerGetDatum(i); }


static inline bool IsAligned(void *p, int align)
{
        int64 i = (int64) PointerGetDatum(p);
        return ((i & (align-1)) == 0);
}

/* ----------------------------------------------------------------
 *				Section 3:	exception handling backend support
 * ----------------------------------------------------------------
 */

#define COMPILE_ASSERT(e) ((void)sizeof(char[1-2*!(e)]))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/*
 * Backend only infrastructure for the assertion-related macros in c.h.
 *
 * ExceptionalCondition must be present even when assertions are not enabled.
 */
extern void ExceptionalCondition(const char *conditionName,
					 const char *errorType,
			   const char *fileName, int lineNumber) pg_attribute_noreturn();

#endif							/* POSTGRES_H */
