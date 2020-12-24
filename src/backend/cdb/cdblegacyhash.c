/*--------------------------------------------------------------------------
 *
 * cdblegacyhash.c
 *	  Hash opclass implementations compatible with legacy DISTRIBUTED BY
 *	  hashing.
 *
 * These are compatibility routines, which implement a hashing that's
 * compatible with the "cdbhash" functions in GPDB 5 and below.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdblegacyhash.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbhash.h"
#include "catalog/pg_am.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/cash.h"
#include "utils/complex_type.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/fmgroids.h"
#include "utils/inet.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "utils/uuid.h"
#include "utils/varbit.h"

#define PG_GETARG_ITEMPOINTER(n) DatumGetItemPointer(PG_GETARG_DATUM(n))

/*
 * FNV1_32_INIT is defined in cdbhash.h, so that it can be used directly
 * from cdbhash.c
 */

/* Constant prime value used for an FNV1 hash */
#define FNV_32_PRIME ((uint32)0x01000193)

/* Constant used for hashing a NULL value */
#define NULL_VAL ((uint32)0XF0F0F0F1)

/* Constant used for hashing a NAN value  */
#define NAN_VAL ((uint32)0XE0E0E0E1)

/* Constant used for hashing an invalid value  */
#define INVALID_VAL ((uint32)0XD0D0D0D1)

/* local function declarations */
static uint32 fnv1_32_buf(void *buf, size_t len, uint32 hashval);
static int	inet_getkey(inet *addr, unsigned char *inet_key, int key_size);
static int	ignoreblanks(char *data, int len);

/*
 * The normal PostgreSQL hash functions take one value as argument, and
 * return a hashcode, i.e. hashcode = hashfunc(arg). When multiple columns
 * need to be hashed together, the hash function is called separately for
 * each column, and the results are combined, with a 1 bit rotation to for
 * better mixing:
 *
 * hashcode =  hashfunc2(col2) ^ rot(hashfunc1(col1))
 *
 * The legacy hashing scheme, however, uses the already-computed hash code
 * as part of the computation:
 *
 * hashcode = hashfunc1(col2, hashfunc(col1, FNV1_32_INIT))
 *
 * Note that the hash function effectively has two arguments: the new
 * datum to be hashed, and the hash code computed so far. That's incompatible
 * with Postgres hash functions, which only have one column.
 *
 * We could represent the legacy hash functions as two-argument functions,
 * and store them in pg_amproc entries with a different strategy number than
 * the normal hash functions use (they use HASHPROC, which is 1). But that
 * seems like it might cause different problems. So instead, we pass the
 * "previous" hash value in this global variable, 'magic_hash_stash'. That
 * way, the legacy hash opclasses work like any other hash opclass, except
 * when the hash function is called from cdbhash.c. cdbhash.c needs to treat
 * them specially anyway, since the "reduction" algorithm used to derive the
 * segment number from the hash code is different too. (We could represent the
 * reduction algorithm as yet another support function, but we don't need the
 * flexibility, so let's not make things more complicated for the sake of it.)
 *
 * The 'magic_hash_stash' is initialized at FNV1_32_INIT, and reset back to
 * that value after any hash computation in cdbhash.c. That way, the result
 * you get if you call one of the legacy functions directly, is the same as
 * cdbhash would compute for a single column.
 */
uint32	magic_hash_stash = FNV1_32_INIT;

static uint32
hashFn(char *buf, int len)
{
	return fnv1_32_buf(buf, len, magic_hash_stash);
}

Datum cdblegacyhash_int2(PG_FUNCTION_ARGS);
Datum cdblegacyhash_int4(PG_FUNCTION_ARGS);
Datum cdblegacyhash_int8(PG_FUNCTION_ARGS);
Datum cdblegacyhash_float4(PG_FUNCTION_ARGS);
Datum cdblegacyhash_float8(PG_FUNCTION_ARGS);
Datum cdblegacyhash_numeric(PG_FUNCTION_ARGS);
Datum cdblegacyhash_char(PG_FUNCTION_ARGS);
Datum cdblegacyhash_text(PG_FUNCTION_ARGS);
Datum cdblegacyhash_bpchar(PG_FUNCTION_ARGS);
Datum cdblegacyhash_bytea(PG_FUNCTION_ARGS);
Datum cdblegacyhash_name(PG_FUNCTION_ARGS);
Datum cdblegacyhash_oid(PG_FUNCTION_ARGS);
Datum cdblegacyhash_tid(PG_FUNCTION_ARGS);
Datum cdblegacyhash_timestamp(PG_FUNCTION_ARGS);
Datum cdblegacyhash_timestamptz(PG_FUNCTION_ARGS);
Datum cdblegacyhash_date(PG_FUNCTION_ARGS);
Datum cdblegacyhash_time(PG_FUNCTION_ARGS);
Datum cdblegacyhash_timetz(PG_FUNCTION_ARGS);
Datum cdblegacyhash_interval(PG_FUNCTION_ARGS);
Datum cdblegacyhash_inet(PG_FUNCTION_ARGS);
Datum cdblegacyhash_macaddr(PG_FUNCTION_ARGS);
Datum cdblegacyhash_bit(PG_FUNCTION_ARGS);
Datum cdblegacyhash_bool(PG_FUNCTION_ARGS);
Datum cdblegacyhash_array(PG_FUNCTION_ARGS);
Datum cdblegacyhash_oidvector(PG_FUNCTION_ARGS);
Datum cdblegacyhash_cash(PG_FUNCTION_ARGS);
Datum cdblegacyhash_uuid(PG_FUNCTION_ARGS);
Datum cdblegacyhash_complex(PG_FUNCTION_ARGS);
Datum cdblegacyhash_anyenum(PG_FUNCTION_ARGS);

Oid
get_legacy_cdbhash_opclass_for_base_type(Oid orig_typid)
{
	Oid			typid;
	char	   *opclass_name;

	/* look through domains */
	typid = getBaseType(orig_typid);

	/*
	 * These are the datatypes that were "GPDB-hashable" in GPDB 5.
	 *
	 * NOTE: We don't support array types, even though GPDB 5 used to implement
	 * hashing for them. The reason is that the hash function for those was
	 * actually broken, see https://github.com/greenplum-db/gpdb/issues/5467.
	 */
	switch (typid)
	{
		case INT2OID:
			opclass_name = "cdbhash_int2_ops";
			break;
		case INT4OID:
			opclass_name = "cdbhash_int4_ops";
			break;
		case INT8OID:
			opclass_name = "cdbhash_int8_ops";
			break;
		case FLOAT4OID:
			opclass_name = "cdbhash_float4_ops";
			break;
		case FLOAT8OID:
			opclass_name = "cdbhash_float8_ops";
			break;
		case NUMERICOID:
			opclass_name = "cdbhash_numeric_ops";
			break;
		case CHAROID:
			opclass_name = "cdbhash_char_ops";
			break;
		case BPCHAROID:
			opclass_name = "cdbhash_bpchar_ops";
			break;
		case TEXTOID:
		case VARCHAROID:
			opclass_name = "cdbhash_text_ops";
			break;
		case BYTEAOID:
			opclass_name = "cdbhash_bytea_ops";
			break;
		case NAMEOID:
			opclass_name = "cdbhash_name_ops";
			break;
		case OIDOID:
			opclass_name = "cdbhash_oid_ops";
			break;
		case TIDOID:
			opclass_name = "cdbhash_tid_ops";
			break;
		case REGPROCOID:
		case REGPROCEDUREOID:
		case REGOPEROID:
		case REGOPERATOROID:
		case REGCLASSOID:
		case REGTYPEOID:
			opclass_name = "cdbhash_oid_ops";
			break;
		case TIMESTAMPOID:
			opclass_name = "cdbhash_timestamp_ops";
			break;
		case TIMESTAMPTZOID:
			opclass_name = "cdbhash_timestamptz_ops";
			break;
		case DATEOID:
			opclass_name = "cdbhash_date_ops";
			break;
		case TIMEOID:
			opclass_name = "cdbhash_time_ops";
			break;
		case TIMETZOID:
			opclass_name = "cdbhash_timetz_ops";
			break;
		case INTERVALOID:
			opclass_name = "cdbhash_interval_ops";
			break;
		case INETOID:
		case CIDROID:
			opclass_name = "cdbhash_inet_ops";
			break;
		case MACADDROID:
			opclass_name = "cdbhash_macaddr_ops";
			break;
		case BITOID:
		case VARBITOID:
			opclass_name = "cdbhash_bit_ops";
			break;
		case BOOLOID:
			opclass_name = "cdbhash_bool_ops";
			break;
		case OIDVECTOROID:
			opclass_name = "cdbhash_oidvector_ops";
			break;
		case CASHOID:
			opclass_name = "cdbhash_cash_ops";
			break;
		case UUIDOID:
			opclass_name = "cdbhash_uuid_ops";
			break;
		case COMPLEXOID:
			opclass_name = "cdbhash_complex_ops";
			break;

		case ANYENUMOID:
			opclass_name = "cdbhash_enum_ops";
			break;

		default:
			/* Is it an an enum? */
			if (type_is_enum(typid))
				opclass_name = "cdbhash_enum_ops";
			else
				opclass_name = NULL;
			break;
	}

	if (!opclass_name)
		return InvalidOid;

	return get_opclass_oid(HASH_AM_OID,
						   list_make2(makeString("pg_catalog"),
									  makeString(opclass_name)),
						   false);
}

bool
isLegacyCdbHashFunction(Oid funcid)
{
	switch (funcid)
	{
		case F_CDBLEGACYHASH_INT2:
		case F_CDBLEGACYHASH_INT4:
		case F_CDBLEGACYHASH_INT8:
		case F_CDBLEGACYHASH_FLOAT4:
		case F_CDBLEGACYHASH_FLOAT8:
		case F_CDBLEGACYHASH_NUMERIC:
		case F_CDBLEGACYHASH_CHAR:
		case F_CDBLEGACYHASH_TEXT:
		case F_CDBLEGACYHASH_BPCHAR:
		case F_CDBLEGACYHASH_BYTEA:
		case F_CDBLEGACYHASH_NAME:
		case F_CDBLEGACYHASH_OID:
		case F_CDBLEGACYHASH_TID:
		case F_CDBLEGACYHASH_TIMESTAMP:
		case F_CDBLEGACYHASH_TIMESTAMPTZ:
		case F_CDBLEGACYHASH_DATE:
		case F_CDBLEGACYHASH_TIME:
		case F_CDBLEGACYHASH_TIMETZ:
		case F_CDBLEGACYHASH_INTERVAL:
		case F_CDBLEGACYHASH_INET:
		case F_CDBLEGACYHASH_MACADDR:
		case F_CDBLEGACYHASH_BIT:
		case F_CDBLEGACYHASH_BOOL:
		case F_CDBLEGACYHASH_ARRAY:
		case F_CDBLEGACYHASH_OIDVECTOR:
		case F_CDBLEGACYHASH_CASH:
		case F_CDBLEGACYHASH_UUID:
		case F_CDBLEGACYHASH_COMPLEX:
		case F_CDBLEGACYHASH_ANYENUM:
			return true;

		default:
			return false;
	}
}

Datum
cdblegacyhash_int2(PG_FUNCTION_ARGS)
{
	/* an 8 byte buffer for all integer sizes */
	int64		intbuf = (int64)  PG_GETARG_INT16(0);

	/* do the hash using the selected algorithm */
	PG_RETURN_UINT32(hashFn((char *) &intbuf, sizeof(intbuf)));
}

Datum
cdblegacyhash_int4(PG_FUNCTION_ARGS)
{
	/* an 8 byte buffer for all integer sizes */
	int64		intbuf = (int64)  PG_GETARG_INT32(0);

	/* do the hash using the selected algorithm */
	PG_RETURN_UINT32(hashFn((char *) &intbuf, sizeof(intbuf)));
}

Datum
cdblegacyhash_int8(PG_FUNCTION_ARGS)
{
	/* an 8 byte buffer for all integer sizes */
	int64		intbuf = PG_GETARG_INT64(0);

	/* do the hash using the selected algorithm */
	PG_RETURN_UINT32(hashFn((char *) &intbuf, sizeof(intbuf)));
}

Datum
cdblegacyhash_float4(PG_FUNCTION_ARGS)
{
	float4		buf_f4 = PG_GETARG_FLOAT4(0);

	/*
	 * On IEEE-float machines, minus zero and zero have different bit
	 * patterns but should compare as equal.  We must ensure that they
	 * have the same hash value, which is most easily done this way:
	 */
	if (buf_f4 == (float4) 0)
		buf_f4 = 0.0;

	PG_RETURN_UINT32(hashFn((char *) &buf_f4, sizeof(buf_f4)));
}

Datum
cdblegacyhash_float8(PG_FUNCTION_ARGS)
{
	float8		buf_f8 = PG_GETARG_FLOAT8(0);

	/*
	 * On IEEE-float machines, minus zero and zero have different bit
	 * patterns but should compare as equal.  We must ensure that they
	 * have the same hash value, which is most easily done this way:
	 */
	if (buf_f8 == (float8) 0)
		buf_f8 = 0.0;

	PG_RETURN_UINT32(hashFn((char *) &buf_f8, sizeof(buf_f8)));
}

Datum
cdblegacyhash_numeric(PG_FUNCTION_ARGS)
{
	Numeric		num = PG_GETARG_NUMERIC(0);
	void	   *buf;		/* pointer to the data */
	size_t		len;		/* length for the data buffer */
	uint32		hash;

	if (numeric_is_nan(num))
	{
		static const uint32 nanbuf = NAN_VAL;

		buf = (void *) &nanbuf;
		len = sizeof(nanbuf);
	}
	else
		/* not a nan */
	{
		buf = numeric_digits(num);
		len = numeric_len(num);
	}

	/* do the hash using the selected algorithm */
	hash = hashFn(buf, len);

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(num, 0);

	PG_RETURN_UINT32(hash);
}

Datum
cdblegacyhash_char(PG_FUNCTION_ARGS)
{
	char		char_buf = PG_GETARG_CHAR(0);

	PG_RETURN_UINT32(hashFn(&char_buf, 1));
}

/* also for BPCHAR and VARCHAR */
Datum
cdblegacyhash_text(PG_FUNCTION_ARGS)
{
	text	   *text_buf = PG_GETARG_TEXT_PP(0);
	int			len;
	void	   *buf;		/* pointer to the data */
	uint32		hash;

	buf = (void *) VARDATA_ANY(text_buf);
	len = VARSIZE_ANY_EXHDR(text_buf);
	/* adjust length to not include trailing blanks */
	if (len > 1)
		len = ignoreblanks((char *) buf, len);

	hash = hashFn(buf, len);

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(text_buf, 0);

	PG_RETURN_UINT32(hash);
}

Datum
cdblegacyhash_bpchar(PG_FUNCTION_ARGS)
{
	return cdblegacyhash_text(fcinfo);
}

Datum
cdblegacyhash_bytea(PG_FUNCTION_ARGS)
{
	bytea	   *bytea_buf = PG_GETARG_BYTEA_PP(0);
	int			len;
	void	   *buf;		/* pointer to the data */
	uint32		hash;

	buf = (void *) VARDATA_ANY(bytea_buf);
	len = VARSIZE_ANY_EXHDR(bytea_buf);

	hash = hashFn(buf, len);

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(bytea_buf, 0);

	PG_RETURN_UINT32(hash);
}

Datum
cdblegacyhash_name(PG_FUNCTION_ARGS)
{
	char	   *namebuf = NameStr(*PG_GETARG_NAME(0));
	int			len;

	/* adjust length to not include trailing blanks */
	len = ignoreblanks((char *) namebuf, NAMEDATALEN);

	PG_RETURN_UINT32(hashFn(namebuf, len));
}

Datum
cdblegacyhash_oid(PG_FUNCTION_ARGS)
{
	/* an 8 byte buffer for all integer sizes */
	int64		intbuf = (int64)  PG_GETARG_INT32(0);

	/* do the hash using the selected algorithm */
	PG_RETURN_UINT32(hashFn((char *) &intbuf, sizeof(intbuf)));
}

Datum
cdblegacyhash_tid(PG_FUNCTION_ARGS)
{
	ItemPointer	tid = PG_GETARG_ITEMPOINTER(0);

	/* See hashtid() for why we're not using sizeof(ItemPointerData) here */
	PG_RETURN_UINT32(hashFn((char *) tid,
							sizeof(BlockIdData) + sizeof(OffsetNumber)));
}

Datum
cdblegacyhash_timestamp(PG_FUNCTION_ARGS)
{
	Timestamp tsbuf = PG_GETARG_TIMESTAMP(0);

	PG_RETURN_UINT32(hashFn((char *) &tsbuf, sizeof(tsbuf)));
}

Datum
cdblegacyhash_timestamptz(PG_FUNCTION_ARGS)
{
	TimestampTz tstzbuf = PG_GETARG_TIMESTAMPTZ(0);

	PG_RETURN_UINT32(hashFn((char *) &tstzbuf, sizeof(tstzbuf)));
}

Datum
cdblegacyhash_date(PG_FUNCTION_ARGS)
{
	DateADT	datebuf = PG_GETARG_DATEADT(0);

	PG_RETURN_UINT32(hashFn((char *) &datebuf, sizeof(datebuf)));
}

Datum
cdblegacyhash_time(PG_FUNCTION_ARGS)
{
	TimeADT timebuf = PG_GETARG_TIMEADT(0);

	PG_RETURN_UINT32(hashFn((char *) &timebuf, sizeof(timebuf)));
}

Datum
cdblegacyhash_timetz(PG_FUNCTION_ARGS)
{
	TimeTzADT *timetzptr = PG_GETARG_TIMETZADT_P(0);
	int			len;
	char	   *buf;		/* pointer to the data */

	/*
	 * will not compare to TIMEOID on equal values. Postgres never
	 * attempts to compare the two as well.
	 */
	buf = (char *) timetzptr;

	/*
	 * Specify hash length as sizeof(double) + sizeof(int4), not as
	 * sizeof(TimeTzADT), so that any garbage pad bytes in the
	 * structure won't be included in the hash!
	 */
	len = sizeof(timetzptr->time) + sizeof(timetzptr->zone);

	PG_RETURN_UINT32(hashFn(buf, len));
}

Datum
cdblegacyhash_interval(PG_FUNCTION_ARGS)
{
	Interval *intervalptr = PG_GETARG_INTERVAL_P(0);
	int			len;
	void	   *buf;		/* pointer to the data */

	buf = (unsigned char *) intervalptr;

	/*
	 * Specify hash length as sizeof(double) + sizeof(int4), not as
	 * sizeof(Interval), so that any garbage pad bytes in the
	 * structure won't be included in the hash!
	 */
	len = sizeof(intervalptr->time) + sizeof(intervalptr->month);

	PG_RETURN_UINT32(hashFn(buf, len));
}

Datum
cdblegacyhash_inet(PG_FUNCTION_ARGS)
{
	/* inet/cidr */
	inet	   *inetptr = PG_GETARG_INET_PP(0);
	int			len;
	void	   *buf;		/* pointer to the data */
	unsigned char inet_hkey[sizeof(inet_struct)];
	uint32		hash;

	len = inet_getkey(inetptr, inet_hkey, sizeof(inet_hkey));	/* fill-in inet_key &
																 * get len */
	buf = inet_hkey;
	hash = hashFn(buf, len);

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(inetptr, 0);

	PG_RETURN_UINT32(hash);
}

Datum
cdblegacyhash_macaddr(PG_FUNCTION_ARGS)
{
	macaddr *macptr = PG_GETARG_MACADDR_P(0);

	PG_RETURN_UINT32(hashFn((char *) macptr, sizeof(macaddr)));
}

Datum
cdblegacyhash_bit(PG_FUNCTION_ARGS)
{
	/*
	 * Note that these are essentially strings. we don't need to worry
	 * about '10' and '010' to compare, b/c they will not, by design.
	 * (see SQL standard, and varbit.c)
	 */
	VarBit *vbitptr = PG_GETARG_VARBIT_P(0);
	int			len;
	void	   *buf;		/* pointer to the data */
	uint32		hash;

	len = VARBITBYTES(vbitptr);
	buf = (char *) VARBITS(vbitptr);

	hash = hashFn(buf, len);

	/* Avoid leaking memory for toasted inputs */
	PG_FREE_IF_COPY(vbitptr, 0);

	PG_RETURN_UINT32(hash);
}

Datum
cdblegacyhash_bool(PG_FUNCTION_ARGS)
{
	char		bool_buf = PG_GETARG_BOOL(0);

	PG_RETURN_UINT32(hashFn(&bool_buf, sizeof(bool_buf)));
}

Datum
cdblegacyhash_array(PG_FUNCTION_ARGS)
{
	ArrayType  *arrbuf = PG_GETARG_ARRAYTYPE_P(0);
	int			len;
	void	   *buf;		/* pointer to the data */

	len = VARSIZE(arrbuf) - VARHDRSZ;
	buf = VARDATA(arrbuf);

	PG_RETURN_UINT32(hashFn(buf, len));
}

Datum
cdblegacyhash_oidvector(PG_FUNCTION_ARGS)
{
	oidvector  *oidvec_buf = (oidvector *) PG_GETARG_POINTER(0);
	int			len;
	void	   *buf;		/* pointer to the data */

	len = oidvec_buf->dim1 * sizeof(Oid);
	buf = oidvec_buf->values;

	PG_RETURN_UINT32(hashFn(buf, len));
}

Datum
cdblegacyhash_cash(PG_FUNCTION_ARGS)
{
	Cash		cash_buf = PG_GETARG_CASH(0);

	PG_RETURN_UINT32(hashFn((char *) &cash_buf, sizeof(Cash)));
}

Datum
cdblegacyhash_uuid(PG_FUNCTION_ARGS)
{
	pg_uuid_t  *uuid_buf = PG_GETARG_UUID_P(0);

	PG_RETURN_UINT32(hashFn((char *) uuid_buf, UUID_LEN));
}

Datum
cdblegacyhash_complex(PG_FUNCTION_ARGS)
{
	Complex *complex_ptr = PG_GETARG_COMPLEX_P(0);
	double		complex_real;
	double		complex_imag;
	Complex		complex_buf;

	complex_real = re(complex_ptr);
	complex_imag = im(complex_ptr);

	/*
	 * On IEEE-float machines, minus zero and zero have different bit
	 * patterns but should compare as equal.  We must ensure that they
	 * have the same hash value, which is most easily done this way:
	 */
	if (complex_real == (float8) 0)
	{
		complex_real = 0.0;
	}
	if (complex_imag == (float8) 0)
	{
		complex_imag = 0.0;
	}

	INIT_COMPLEX(&complex_buf, complex_real, complex_imag);

	PG_RETURN_UINT32(hashFn((char *) &complex_buf, sizeof(Complex)));
}

Datum
cdblegacyhash_anyenum(PG_FUNCTION_ARGS)
{
	/* an 8 byte buffer for all integer sizes */
	int64		intbuf = (int64)  PG_GETARG_INT32(0);

	/* do the hash using the selected algorithm */
	PG_RETURN_UINT32(hashFn((char *) &intbuf, sizeof(intbuf)));
}

/*
 * Update the hash value for a null Datum, for any datatype.
 */
uint32
cdblegacyhash_null(void)
{
	uint32		nullbuf = NULL_VAL; /* stores the constant value that
									 * represents a NULL */
	return hashFn((char *) &nullbuf, sizeof(nullbuf));
}

/*
 * fnv1_32_buf - perform a 32 bit FNV 1 hash on a buffer
 *
 * input:
 *	buf - start of buffer to hash
 *	len - length of buffer in octets (bytes)
 *	hval	- previous hash value or FNV1_32_INIT if first call.
 *
 * returns:
 *	32 bit hash as a static hash type
 */
static uint32
fnv1_32_buf(void *buf, size_t len, uint32 hval)
{
	unsigned char *bp = (unsigned char *) buf;	/* start of buffer */
	unsigned char *be = bp + len;	/* beyond end of buffer */

	/*
	 * FNV-1 hash each octet in the buffer
	 */
	while (bp < be)
	{
		/* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(NO_FNV_GCC_OPTIMIZATION)
		hval *= FNV_32_PRIME;
#else
		hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif

		/* xor the bottom with the current octet */
		hval ^= (uint32) *bp++;
	}

	/* return our new hash value */
	return hval;
}

/*
 * Support function for hashing on inet/cidr (see network.c)
 *
 * Since network_cmp considers only ip_family, ip_bits, and ip_addr,
 * only these fields may be used in the hash; in particular don't use type.
 */
static int
inet_getkey(inet *addr, unsigned char *inet_key, int key_size)
{
	int			addrsize;

	switch (((inet_struct *) VARDATA_ANY(addr))->family)
	{
		case PGSQL_AF_INET:
			addrsize = 4;
			break;
		case PGSQL_AF_INET6:
			addrsize = 16;
			break;
		default:
			addrsize = 0;
	}

	Assert(addrsize + 2 <= key_size);
	inet_key[0] = ((inet_struct *) VARDATA_ANY(addr))->family;
	inet_key[1] = ((inet_struct *) VARDATA_ANY(addr))->bits;
	memcpy(inet_key + 2, ((inet_struct *) VARDATA_ANY(addr))->ipaddr, addrsize);

	return (addrsize + 2);
}

/*================================================================
 *
 * GENERAL PURPOSE UTILS
 *
 *================================================================
 */

/*
 * Given the original length of the data array this function is
 * recalculating the length after ignoring any trailing blanks. The
 * actual data remains unmodified.
 */
static int
ignoreblanks(char *data, int len)
{
	/* look for trailing blanks and skip them in the hash calculation */
	while (data[len - 1] == ' ')
	{
		len--;

		/*
		 * If only 1 char is left, leave it alone! The string is either empty
		 * or has 1 char
		 */
		if (len == 1)
			break;
	}

	return len;
}
