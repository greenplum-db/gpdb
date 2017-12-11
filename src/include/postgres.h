/*-------------------------------------------------------------------------
 *
 * postgres.h
 *	  Primary include file for PostgreSQL server .c files
 *
 * This should be the first file included by PostgreSQL backend modules.
 * Client-side code should include postgres_fe.h instead.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1995, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/postgres.h,v 1.92 2009/01/01 17:23:55 momjian Exp $
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
 *		2)		datum type + support macros
 *		3)		exception handling definitions
 *
 *	 NOTES
 *
 *	In general, this file should contain declarations that are widely needed
 *	in the backend environment, but are of no interest outside the backend.
 *
 *	Simple type definitions live in c.h, where they are shared with
 *	postgres_fe.h.	We do that since those type definitions are needed by
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

#ifdef __cplusplus
extern "C" {
#endif

#include "utils/elog.h"
#include "utils/palloc.h"
#include "storage/itemptr.h"
#include "utils/simex.h"
#include "utils/testutils.h"

/* ----------------------------------------------------------------
 *				Section 1:	variable-length datatypes (TOAST support)
 * ----------------------------------------------------------------
 */

/*
 * struct varatt_external is a "TOAST pointer", that is, the information
 * needed to fetch a stored-out-of-line Datum.	The data is compressed
 * if and only if va_extsize < va_rawsize - VARHDRSZ.  This struct must not
 * contain any padding, because we sometimes compare pointers using memcmp.
 *
 * Note that this information is stored unaligned within actual tuples, so
 * you need to memcpy from the tuple into a local struct variable before
 * you can look at these fields!  (The reason we use memcmp is to avoid
 * having to do that just to detect equality of two TOAST pointers...)
 */
struct varatt_external
{
	int32		va_rawsize;		/* Original data size (includes header) */
	int32		va_extsize;		/* External saved size (doesn't) */
	Oid			va_valueid;		/* Unique ID of value within TOAST table */
	Oid			va_toastrelid;	/* RelID of TOAST table containing it */
};

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
		char		va_data[1];
	}			va_4byte;
	struct						/* Compressed-in-line format */
	{
		uint32		va_header;
		uint32		va_rawsize; /* Original data size (excludes header) */
		char		va_data[1]; /* Compressed data */
	}			va_compressed;
} varattrib_4b;

typedef struct
{
	uint8		va_header;
	char		va_data[1];		/* Data begins here */
} varattrib_1b;

/* NOT Like Postgres! ...In GPDB, We waste a few bytes of padding, and don't always set the va_len_1be to anything */
typedef struct
{
	uint8		va_header;		/* Always 0x80  */
	uint8		va_len_1be;		/*** PG only:  Len of toast pointer w/ 1b header, ignored in GPDB  ***/ /* Physical length of datum */
	uint8		va_padding[2];	/*** GPDB only:  Alignment padding ***/
	char		va_data[1];		/* Data (for now always a TOAST pointer) */
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
 * first byte.	Also, it is not possible for a 1-byte length word to be zero;
 * this lets us disambiguate alignment padding bytes from the start of an
 * unaligned datum.  (We now *require* pad bytes to be filled with zero!)
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
/* In GPDB, VARSIZE_1B_E() is always the size of a toast pointer plus the 4 byte header */
#define VARSIZE_1B_E(PTR) (VARHDRSZ_EXTERNAL + sizeof(struct varatt_external))

#define SET_VARSIZE_4B(PTR,len) \
	(((varattrib_4b *) (PTR))->va_4byte.va_header = htonl( (len) & 0x3FFFFFFF ))
#define SET_VARSIZE_4B_C(PTR,len) \
	(((varattrib_4b *) (PTR))->va_4byte.va_header = htonl( ((len) & 0x3FFFFFFF) | 0x40000000 ))
#define SET_VARSIZE_1B(PTR,len) \
	(((varattrib_1b *) (PTR))->va_header = (len) | 0x80)
/*
 * Although this macro sets var_len_1be, data stored in GPDB might
 * not have anything set in this byte, so you can't count on it's value
 * Not really a problem, since it is always based on TOAST_POINTER_LEN
 */
#define SET_VARSIZE_1B_E(PTR,len) \
	(((varattrib_1b_e *) (PTR))->va_header = 0x80, \
	 ((varattrib_1b_e *) (PTR))->va_len_1be = (len))

#define VARHDRSZ_SHORT			1
#define VARATT_SHORT_MAX		0x7F
#define VARATT_CAN_MAKE_SHORT(PTR) \
	(VARATT_IS_4B_U(PTR) && \
	 (VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT) <= VARATT_SHORT_MAX)
#define VARATT_CONVERTED_SHORT_SIZE(PTR) \
	(VARSIZE(PTR) - VARHDRSZ + VARHDRSZ_SHORT)

/* In Postgres, this is 2 */
#define VARHDRSZ_EXTERNAL		4

#define VARDATA_4B(PTR)		(((varattrib_4b *) (PTR))->va_4byte.va_data)
#define VARDATA_4B_C(PTR)	(((varattrib_4b *) (PTR))->va_compressed.va_data)
#define VARDATA_1B(PTR)		(((varattrib_1b *) (PTR))->va_data)
#define VARDATA_1B_E(PTR)	(((varattrib_1b_e *) (PTR))->va_data)

#define VARRAWSIZE_4B_C(PTR) \
	(((varattrib_4b *) (PTR))->va_compressed.va_rawsize)

/* Externally visible macros */

/*
 * VARDATA, VARSIZE, and SET_VARSIZE are the recommended API for most code
 * for varlena datatypes.  Note that they only work on untoasted,
 * 4-byte-header Datums!
 *
 * Code that wants to use 1-byte-header values without detoasting should
 * use VARSIZE_ANY/VARSIZE_ANY_EXHDR/VARDATA_ANY.  The other macros here
 * should usually be used only by tuple assembly/disassembly code and
 * code that specifically wants to work with still-toasted Datums.
 *
 * WARNING: It is only safe to use VARDATA_ANY() -- typically with
 * PG_DETOAST_DATUM_PACKED() -- if you really don't care about the alignment.
 * Either because you're working with something like text where the alignment
 * doesn't matter or because you're not going to access its constituent parts
 * and just use things like memcpy on it anyways.
 */
#define VARDATA(PTR)						VARDATA_4B(PTR)
#define VARSIZE(PTR)						VARSIZE_4B(PTR)

#define VARSIZE_SHORT(PTR)					VARSIZE_1B(PTR)
#define VARDATA_SHORT(PTR)					VARDATA_1B(PTR)

#define VARSIZE_EXTERNAL(PTR)				VARSIZE_1B_E(PTR)
#define VARDATA_EXTERNAL(PTR)				VARDATA_1B_E(PTR)

#define VARATT_IS_COMPRESSED(PTR)			VARATT_IS_4B_C(PTR)
#define VARATT_IS_EXTERNAL(PTR)				VARATT_IS_1B_E(PTR)
#define VARATT_IS_SHORT(PTR)				VARATT_IS_1B(PTR)
#define VARATT_IS_EXTENDED(PTR)				(!VARATT_IS_4B_U(PTR))

#define SET_VARSIZE(PTR, len)				SET_VARSIZE_4B(PTR, len)
#define SET_VARSIZE_SHORT(PTR, len)			SET_VARSIZE_1B(PTR, len)
#define SET_VARSIZE_COMPRESSED(PTR, len)	SET_VARSIZE_4B_C(PTR, len)
#define SET_VARSIZE_EXTERNAL(PTR, len)		SET_VARSIZE_1B_E(PTR, len)

#define VARSIZE_ANY(PTR) \
	(VARATT_IS_1B_E(PTR) ? VARSIZE_1B_E(PTR) : \
	 (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR) : \
	  VARSIZE_4B(PTR)))

#define VARSIZE_ANY_EXHDR(PTR) \
	(VARATT_IS_1B_E(PTR) ? VARSIZE_1B_E(PTR)-VARHDRSZ_EXTERNAL : \
	 (VARATT_IS_1B(PTR) ? VARSIZE_1B(PTR)-VARHDRSZ_SHORT : \
	  VARSIZE_4B(PTR)-VARHDRSZ))

/* caution: this will not work on an external or compressed-in-line Datum */
/* caution: this will return a possibly unaligned pointer */
#define VARDATA_ANY(PTR) \
	 (VARATT_IS_1B(PTR) ? VARDATA_1B(PTR) : VARDATA_4B(PTR))


/* ----------------------------------------------------------------
 *				Section 2:	datum type + support macros
 * ----------------------------------------------------------------
 */

/*
 * Port Notes:
 *     Postgres makes the following assumption about machines:
 *
 *     sizeof(Datum) == sizeof(long) >= sizeof(void *) >= 4
 *
 *     Postgres also assumes that
 *
 *	sizeof(char) == 1
 *
 *	and that
 *
 *	sizeof(short) == 2
 *
 *  Greenplum CDB:
 *     Datum is always 8 bytes, regardless if it is 32bit or 64bit machine.
 *  so may be > sizeof(long).
 *
 * When a type narrower than Datum is stored in a Datum, we place it in the
 * low-order bits and are careful that the DatumGetXXX macro for it discards
 * the unused high-order bits (as opposed to, say, assuming they are zero).
 * This is needed to support old-style user-defined functions, since depending
 * on architecture and compiler, the return value of a function returning char
 * or short may contain garbage when called as if it returned Datum.
 */

typedef int64 Datum;
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
 * Conversion between Datum and type X.  Changed from Macro to static inline
 * functions to get proper type checking.
 */


/*
 * DatumGetBool
 *		Returns boolean value of a datum.
 *
 * Note: any nonzero value will be considered TRUE, but we ignore bits to
 * the left of the width of bool, per comment above.
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

/* FIXME: diagnostics: remove
static inline uint64 DatumGetUInt64(Datum d) { return (uint64) d; } 
static inline Datum UInt64GetDatum(uint64 ui64) { return (Datum) ui64; } 
*/

/*
 * DatumGetUInt64
 *		Returns 64-bit unsigned integer value of a datum.
 *
 * Note: this macro hides whether int64 is pass by value or by reference.
 */

#ifdef USE_FLOAT8_BYVAL
#define DatumGetUInt64(X) ((uint64) GET_8_BYTES(X))
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
#define UInt64GetDatum(X) ((Datum) SET_8_BYTES(X))
#else
#define UInt64GetDatum(X) Int64GetDatum((int64) (X))
#endif

static inline Oid DatumGetObjectId(Datum d) { return (Oid) d; } 
static inline Datum ObjectIdGetDatum(Oid oid) { return (Datum) oid; } 

static inline TransactionId DatumGetTransactionId(Datum d) { return (TransactionId) d; } 
static inline Datum TransactionIdGetDatum(TransactionId tid) { return (Datum) tid; } 

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
 *				Section 3:	exception handling definitions
 *							Assert, Trap, etc macros
 * ----------------------------------------------------------------
 */

#define COMPILE_ASSERT(e) ((void)sizeof(char[1-2*!(e)]))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

extern PGDLLIMPORT bool assert_enabled;

/*
 * USE_ASSERT_CHECKING, if defined, turns on all the assertions.
 * - plai  9/5/90
 *
 * It should _NOT_ be defined in releases or in benchmark copies
 */

/*
 * Trap
 *		Generates an exception if the given condition is true.
 */
#define Trap(condition, errorType) \
	do { \
		if ((assert_enabled) && (condition)) \
			ExceptionalCondition(CppAsString(condition), (errorType), \
								 __FILE__, __LINE__); \
	} while (0)
/*
 *	TrapMacro is the same as Trap but it's intended for use in macros:
 *
 *		#define foo(x) (AssertMacro(x != 0) && bar(x))
 *
 *	Isn't CPP fun?
 */
#define TrapMacro(condition, errorType) \
	((bool) ((! assert_enabled) || ! (condition) || \
			 (ExceptionalCondition(CppAsString(condition), (errorType), \
								   __FILE__, __LINE__))))

#ifndef USE_ASSERT_CHECKING
#define Assert(condition)
#define AssertMacro(condition)	((void)true)
#define AssertArg(condition)
#define AssertState(condition)
#define AssertImply(condition1, condition2)
#define AssertEquivalent(cond1, cond2)
#define AssertPointerAlignment(ptr, bndr)	((void)true)
#else
#define Assert(condition) \
		Trap(!(condition), "FailedAssertion")

#define AssertMacro(condition) \
		((void) TrapMacro(!(condition), "FailedAssertion"))

#define AssertArg(condition) \
		Trap(!(condition), "BadArgument")

#define AssertState(condition) \
		Trap(!(condition), "BadState")

#define AssertImply(cond1, cond2) \
		Trap(!(!(cond1) || (cond2)), "AssertImply failed")

#define AssertEquivalent(cond1, cond2) \
		Trap(!((bool)(cond1) == (bool)(cond2)), "AssertEquivalent failed")

/*
 * Check that `ptr' is `bndr' aligned.
 */
#define AssertPointerAlignment(ptr, bndr) \
	Trap(TYPEALIGN(bndr, (uintptr_t)(ptr)) != (uintptr_t)(ptr), \
		 "UnalignedPointer")

#endif   /* USE_ASSERT_CHECKING */

extern int ExceptionalCondition(const char *conditionName,
					 const char *errorType,
					 const char *fileName, int lineNumber);


#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif   /* POSTGRES_H */
