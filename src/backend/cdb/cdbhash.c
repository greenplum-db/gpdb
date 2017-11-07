/*--------------------------------------------------------------------------
 *
 * cdbhash.c
 *	  Provides hashing routines to support consistant data distribution/location
 *    within Greenplum Database.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbhash.c
 *
 *--------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/tuptoaster.h"
#include "utils/builtins.h"
#include "catalog/pg_type.h"
#include "parser/parse_type.h"
#include "utils/numeric.h"
#include "utils/inet.h"
#include "utils/timestamp.h"
#include "utils/array.h"
#include "utils/date.h"
#include "utils/cash.h"
#include "utils/datetime.h"
#include "utils/nabstime.h"
#include "utils/varbit.h"
#include "utils/uuid.h"
#include "fmgr.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/complex_type.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbutil.h"

/* 32 bit FNV-1  non-zero initial basis */
#define FNV1_32_INIT ((uint32)0x811c9dc5)

/* Constant prime value used for an FNV1 hash */
#define FNV_32_PRIME ((uint32)0x01000193)

/* Constant used for hashing a NULL value */
#define NULL_VAL ((uint32)0XF0F0F0F1)

/* Constant used for hashing a NAN value  */
#define NAN_VAL ((uint32)0XE0E0E0E1)

/* Constant used for hashing an invalid value  */
#define INVALID_VAL ((uint32)0XD0D0D0D1)

/* Constant used to help defining upper limit for random generator */
#define UPPER_VAL ((uint32)0XA0B0C0D1)

/* Fast mod using a bit mask, assuming that y is a power of 2 */
#define FASTMOD(x,y)		((x) & ((y)-1))

/* local function declarations */
static uint32 fnv1_32_buf(void *buf, size_t len, uint32 hashval);
static int	inet_getkey(inet *addr, unsigned char *inet_key, int key_size);
static int	ignoreblanks(char *data, int len);
static int	ispowof2(int numsegs);


/*================================================================
 *
 * HASH API FUNCTIONS
 *
 *================================================================
 */

/*
 * Create a CdbHash for this session.
 *
 * CdbHash maintains the following information about the hash.
 * In here we set the variables that should not change in the scope of the newly created
 * CdbHash, these are:
 *
 * 1 - number of segments in Greenplum Database.
 * 2 - reduction method.
 *
 * The hash value itself will be initialized for every tuple in cdbhashinit()
 */
CdbHash *
makeCdbHash(int numsegs)
{
	CdbHash    *h;

	Assert(numsegs > 0);		/* verify number of segments is legal. */

	/* Create a pointer to a CdbHash that includes the hash properties */
	h = palloc(sizeof(CdbHash));

	/*
	 * set this hash session characteristics.
	 */
	h->hash = 0;
	h->numsegs = numsegs;

	/*
	 * set the reduction algorithm: If num_segs is power of 2 use bit mask,
	 * else use lazy mod (h mod n)
	 */
	if (ispowof2(numsegs))
	{
		h->reducealg = REDUCE_BITMASK;
	}
	else
	{
		h->reducealg = REDUCE_LAZYMOD;
	}

	/*
	 * if we distribute into a relation with an empty partitioning policy, we
	 * will round robin the tuples starting off from this index. Note that the
	 * random number is created one per makeCdbHash. This means that commands
	 * that create a cdbhash object only once for all tuples (like COPY,
	 * INSERT-INTO-SELECT) behave more like a round-robin distribution, while
	 * commands that create a cdbhash per row (like INSERT) behave more like a
	 * random distribution.
	 */
	h->rrindex = cdb_randint(0, UPPER_VAL);

	ereport(DEBUG4,
			(errmsg("CDBHASH hashing into %d segment databases", h->numsegs)));

	return h;
}


/*
 * Initialize CdbHash for hashing the next tuple values.
 */
void
cdbhashinit(CdbHash *h)
{
	/* reset the hash value to the initial offset basis */
	h->hash = FNV1_32_INIT;
}

/*
 * Implements datumHashFunction
 */
static void
addToCdbHash(void *cdbHash, void *buf, size_t len)
{
	CdbHash    *h = (CdbHash *) cdbHash;

	h->hash = fnv1_32_buf(buf, len, h->hash);
}

/*
 * Add an attribute to the CdbHash calculation.
 */
void
cdbhash(CdbHash *h, Datum datum, Oid type)
{
	hashDatum(datum, type, addToCdbHash, (void *) h);
}

/*
 * Add an attribute to the hash calculation.
 * **IMPORTANT: any new hard coded support for a data type in here
 * must be added to isGreenplumDbHashable() below!
 *
 * Note that the caller should provide the base type if the datum is
 * of a domain type. It is quite expensive to call get_typtype() and
 * getBaseType() here since this function gets called a lot for the
 * same set of Datums.
 *
 * @param hashFn called to update the hash value.
 * @param clientData passed to hashFn.
 */
void
hashDatum(Datum datum, Oid type, datumHashFunction hashFn, void *clientData)
{
	void	   *buf = NULL;		/* pointer to the data */
	size_t		len = 0;		/* length for the data buffer */

	int64		intbuf;			/* an 8 byte buffer for all integer sizes */

	float4		buf_f4;
	float8		buf_f8;
	Timestamp	tsbuf;			/* timestamp data dype is either a double or
								 * int8 (determined in compile time) */
	TimestampTz tstzbuf;
	DateADT		datebuf;
	TimeADT		timebuf;
	TimeTzADT  *timetzptr;
	Interval   *intervalptr;
	AbsoluteTime abstime_buf;
	RelativeTime reltime_buf;
	TimeInterval tinterval;
	AbsoluteTime tinterval_len;

	Numeric		num;
	bool		bool_buf;
	char		char_buf;
	Name		namebuf;

	ArrayType  *arrbuf;
	inet	   *inetptr;		/* inet/cidr */
	unsigned char inet_hkey[sizeof(inet_struct)];
	macaddr    *macptr;			/* MAC address */

	VarBit	   *vbitptr;

	oidvector  *oidvec_buf;
	Complex    *complex_ptr;
	Complex		complex_buf;
	double		complex_real;
	double		complex_imag;

	Cash		cash_buf;
	pg_uuid_t  *uuid_buf;

	/*
	 * special case buffers
	 */
	uint32		nanbuf;
	uint32		invalidbuf;

	void	   *tofree = NULL;

	if (typeIsEnumType(type))
		type = ANYENUMOID;

	/*
	 * Select the hash to be performed according to the field type we are
	 * adding to the hash.
	 */
	switch (type)
	{
			/*
			 * ======= NUMERIC TYPES ========
			 */
		case INT2OID:			/* -32 thousand to 32 thousand, 2-byte storage */
			intbuf = (int64) DatumGetInt16(datum);	/* cast to 8 byte before
													 * hashing */
			buf = &intbuf;
			len = sizeof(intbuf);
			break;

		case INT4OID:			/* -2 billion to 2 billion integer, 4-byte
								 * storage */
			intbuf = (int64) DatumGetInt32(datum);	/* cast to 8 byte before
													 * hashing */
			buf = &intbuf;
			len = sizeof(intbuf);
			break;

		case INT8OID:			/* ~18 digit integer, 8-byte storage */
			intbuf = DatumGetInt64(datum);	/* cast to 8 byte before hashing */
			buf = &intbuf;
			len = sizeof(intbuf);
			break;

		case FLOAT4OID:			/* single-precision floating point number,
								 * 4-byte storage */
			buf_f4 = DatumGetFloat4(datum);

			/*
			 * On IEEE-float machines, minus zero and zero have different bit
			 * patterns but should compare as equal.  We must ensure that they
			 * have the same hash value, which is most easily done this way:
			 */
			if (buf_f4 == (float4) 0)
				buf_f4 = 0.0;

			buf = &buf_f4;
			len = sizeof(buf_f4);
			break;

		case FLOAT8OID:			/* double-precision floating point number,
								 * 8-byte storage */
			buf_f8 = DatumGetFloat8(datum);

			/*
			 * On IEEE-float machines, minus zero and zero have different bit
			 * patterns but should compare as equal.  We must ensure that they
			 * have the same hash value, which is most easily done this way:
			 */
			if (buf_f8 == (float8) 0)
				buf_f8 = 0.0;

			buf = &buf_f8;
			len = sizeof(buf_f8);
			break;

		case NUMERICOID:

			num = DatumGetNumeric(datum);

			if (NUMERIC_IS_NAN(num))
			{
				nanbuf = NAN_VAL;
				buf = &nanbuf;
				len = sizeof(nanbuf);
			}
			else
				/* not a nan */
			{
				buf = num->n_data;
				len = (VARSIZE(num) - NUMERIC_HDRSZ);
			}

			/*
			 * If we did a pg_detoast_datum, we need to remember to pfree, or
			 * we will leak memory.  Because of the 1-byte varlena header
			 * stuff.
			 */
			if (num != (Numeric) DatumGetPointer(datum))
				tofree = num;

			break;

			/*
			 * ====== CHARACTER TYPES =======
			 */
		case CHAROID:			/* char(1), single character */
			char_buf = DatumGetChar(datum);
			buf = &char_buf;
			len = 1;
			break;

		case BPCHAROID:			/* char(n), blank-padded string, fixed storage */
		case TEXTOID:			/* text */
		case VARCHAROID:		/* varchar */
		case BYTEAOID:			/* bytea */
			{
				int			tmplen;

				varattrib_untoast_ptr_len(datum, (char **) &buf, &tmplen, &tofree);
				/* adjust length to not include trailing blanks */
				if (type != BYTEAOID && tmplen > 1)
					tmplen = ignoreblanks((char *) buf, tmplen);

				len = tmplen;
				break;
			}

		case NAMEOID:
			namebuf = DatumGetName(datum);
			len = NAMEDATALEN;
			buf = NameStr(*namebuf);

			/* adjust length to not include trailing blanks */
			if (len > 1)
				len = ignoreblanks((char *) buf, len);
			break;

			/*
			 * ====== OBJECT IDENTIFIER TYPES ======
			 */
		case OIDOID:			/* object identifier(oid), maximum 4 billion */
		case REGPROCOID:		/* function name */
		case REGPROCEDUREOID:	/* function name with argument types */
		case REGOPEROID:		/* operator name */
		case REGOPERATOROID:	/* operator with argument types */
		case REGCLASSOID:		/* relation name */
		case REGTYPEOID:		/* data type name */
		case ANYENUMOID:		/* enum type name */
			intbuf = (int64) DatumGetUInt32(datum); /* cast to 8 byte before
													 * hashing */
			buf = &intbuf;
			len = sizeof(intbuf);
			break;

		case TIDOID:			/* tuple id (6 bytes) */
			buf = DatumGetPointer(datum);
			len = SizeOfIptrData;
			break;

			/*
			 * ====== DATE/TIME TYPES ======
			 */
		case TIMESTAMPOID:		/* date and time */
			tsbuf = DatumGetTimestamp(datum);
			buf = &tsbuf;
			len = sizeof(tsbuf);
			break;

		case TIMESTAMPTZOID:	/* date and time with time zone */
			tstzbuf = DatumGetTimestampTz(datum);
			buf = &tstzbuf;
			len = sizeof(tstzbuf);
			break;

		case DATEOID:			/* ANSI SQL date */
			datebuf = DatumGetDateADT(datum);
			buf = &datebuf;
			len = sizeof(datebuf);
			break;

		case TIMEOID:			/* hh:mm:ss, ANSI SQL time */
			timebuf = DatumGetTimeADT(datum);
			buf = &timebuf;
			len = sizeof(timebuf);
			break;

		case TIMETZOID:			/* time with time zone */

			/*
			 * will not compare to TIMEOID on equal values. Postgres never
			 * attempts to compare the two as well.
			 */
			timetzptr = DatumGetTimeTzADTP(datum);
			buf = (unsigned char *) timetzptr;

			/*
			 * Specify hash length as sizeof(double) + sizeof(int4), not as
			 * sizeof(TimeTzADT), so that any garbage pad bytes in the
			 * structure won't be included in the hash!
			 */
			len = sizeof(timetzptr->time) + sizeof(timetzptr->zone);
			break;

		case INTERVALOID:		/* @ <number> <units>, time interval */
			intervalptr = DatumGetIntervalP(datum);
			buf = (unsigned char *) intervalptr;

			/*
			 * Specify hash length as sizeof(double) + sizeof(int4), not as
			 * sizeof(Interval), so that any garbage pad bytes in the
			 * structure won't be included in the hash!
			 */
			len = sizeof(intervalptr->time) + sizeof(intervalptr->month);
			break;

		case ABSTIMEOID:
			abstime_buf = DatumGetAbsoluteTime(datum);

			if (abstime_buf == INVALID_ABSTIME)
			{
				/* hash to a constant value */
				invalidbuf = INVALID_VAL;
				len = sizeof(invalidbuf);
				buf = &invalidbuf;
			}
			else
			{
				len = sizeof(abstime_buf);
				buf = &abstime_buf;
			}

			break;

		case RELTIMEOID:
			reltime_buf = DatumGetRelativeTime(datum);

			if (reltime_buf == INVALID_RELTIME)
			{
				/* hash to a constant value */
				invalidbuf = INVALID_VAL;
				len = sizeof(invalidbuf);
				buf = &invalidbuf;
			}
			else
			{
				len = sizeof(reltime_buf);
				buf = &reltime_buf;
			}

			break;

		case TINTERVALOID:
			tinterval = DatumGetTimeInterval(datum);

			/*
			 * check if a valid interval. the '0' status code stands for
			 * T_INTERVAL_INVAL which is defined in nabstime.c. We use the
			 * actual value instead of defining it again here.
			 */
			if (tinterval->status == 0 ||
				tinterval->data[0] == INVALID_ABSTIME ||
				tinterval->data[1] == INVALID_ABSTIME)
			{
				/* hash to a constant value */
				invalidbuf = INVALID_VAL;
				len = sizeof(invalidbuf);
				buf = &invalidbuf;
			}
			else
			{
				/* normalize on length of the time interval */
				tinterval_len = tinterval->data[1] - tinterval->data[0];
				len = sizeof(tinterval_len);
				buf = &tinterval_len;
			}

			break;

			/*
			 * ======= NETWORK TYPES ========
			 */
		case INETOID:
		case CIDROID:

			inetptr = DatumGetInetP(datum);
			len = inet_getkey(inetptr, inet_hkey, sizeof(inet_hkey));	/* fill-in inet_key &
																		 * get len */
			buf = inet_hkey;
			break;

		case MACADDROID:

			macptr = DatumGetMacaddrP(datum);
			len = sizeof(macaddr);
			buf = (unsigned char *) macptr;
			break;

			/*
			 * ======== BIT STRINGS ========
			 */
		case BITOID:
		case VARBITOID:

			/*
			 * Note that these are essentially strings. we don't need to worry
			 * about '10' and '010' to compare, b/c they will not, by design.
			 * (see SQL standard, and varbit.c)
			 */
			vbitptr = DatumGetVarBitP(datum);
			len = VARBITBYTES(vbitptr);
			buf = (char *) VARBITS(vbitptr);
			break;

			/*
			 * ======= other types =======
			 */
		case BOOLOID:			/* boolean, 'true'/'false' */
			bool_buf = DatumGetBool(datum);
			buf = &bool_buf;
			len = sizeof(bool_buf);
			break;

			/*
			 * ANYARRAY is a pseudo-type. We use it to include any of the
			 * array types (OIDs 1007-1033 in pg_type.h). caller needs to be
			 * sure the type is ANYARRAYOID before calling cdbhash on an array
			 * (INSERT and COPY do so).
			 */
		case ANYARRAYOID:

			arrbuf = DatumGetArrayTypeP(datum);
			len = VARSIZE(arrbuf) - VARHDRSZ;
			buf = VARDATA(arrbuf);
			break;

		case OIDVECTOROID:
			oidvec_buf = (oidvector *) DatumGetPointer(datum);
			len = oidvec_buf->dim1 * sizeof(Oid);
			buf = oidvec_buf->values;
			break;

		case CASHOID:			/* cash is stored in int64 internally */
			cash_buf = DatumGetCash(datum);
			len = sizeof(Cash);
			buf = &cash_buf;
			break;

			/* pg_uuid_t is defined as a char array of size UUID_LEN in uuid.c */
		case UUIDOID:
			uuid_buf = DatumGetUUIDP(datum);
			len = UUID_LEN;
			buf = (char *) uuid_buf;
			break;

		case COMPLEXOID:
			complex_ptr = DatumGetComplexP(datum);
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
			len = sizeof(Complex);
			buf = (unsigned char *) &complex_buf;
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_CDB_FEATURE_NOT_YET),
					 errmsg("Type %u is not hashable.", type)));

	}							/* switch(type) */

	/* do the hash using the selected algorithm */
	hashFn(clientData, buf, len);
	if (tofree)
		pfree(tofree);
}

/*
 * Add a NULL attribute to the hash calculation.
 */
void
cdbhashnull(CdbHash *h)
{
	hashNullDatum(addToCdbHash, (void *) h);
}

/*
 * Update the hash value for a null Datum
 *
 * @param hashFn called to update the hash value.
 * @param clientData passed to hashFn.
 */
void
hashNullDatum(datumHashFunction hashFn, void *clientData)
{
	uint32		nullbuf = NULL_VAL; /* stores the constant value that
									 * represents a NULL */
	void	   *buf = &nullbuf; /* stores the address of the buffer					*/
	size_t		len = sizeof(nullbuf);	/* length of the value								*/

	hashFn(clientData, buf, len);
}

/*
 * Hash a tuple of a relation with an empty policy (no hash
 * key exists) via round robin with a random initial value.
 */
void
cdbhashnokey(CdbHash *h)
{
	uint32		rrbuf = h->rrindex;
	void	   *buf = &rrbuf;
	size_t		len = sizeof(rrbuf);

	/* compute the hash */
	h->hash = fnv1_32_buf(buf, len, h->hash);

	h->rrindex++;				/* increment for next time around */
}


/*
 * Reduce the hash to a segment number.
 */
unsigned int
cdbhashreduce(CdbHash *h)
{

	int			result = 0;		/* TODO: what is a good initialization value?
								 * could we guarantee at this point that there
								 * will not be a negative segid in Greenplum
								 * Database and therefore initialize to this
								 * value for error checking? */

	Assert(h->reducealg == REDUCE_BITMASK || h->reducealg == REDUCE_LAZYMOD);

	/*
	 * Reduce our 32-bit hash value to a segment number
	 */
	switch (h->reducealg)
	{
		case REDUCE_BITMASK:
			result = FASTMOD(h->hash, (uint32) h->numsegs); /* fast mod (bitmask) */
			break;

		case REDUCE_LAZYMOD:
			result = (h->hash) % (h->numsegs);	/* simple mod */
			break;
	}

	return result;
}

bool
typeIsArrayType(Oid typeoid)
{
	Type		tup = typeidType(typeoid);
	Form_pg_type typeform;
	bool		res = false;

	typeform = (Form_pg_type) GETSTRUCT(tup);

	if (typeform->typelem != InvalidOid &&
		typeform->typtype != 'd' &&
		NameStr(typeform->typname)[0] == '_' &&
		typeform->typinput == F_ARRAY_IN)
		res = true;

	ReleaseSysCache(tup);
	return res;
}

bool
typeIsEnumType(Oid typeoid)
{
	Type		tup = typeidType(typeoid);
	Form_pg_type typeform;
	bool		res = false;

	typeform = (Form_pg_type) GETSTRUCT(tup);

	if (typeform->typtype == 'e' && typeform->typinput == F_ENUM_IN)
		res = true;

	ReleaseSysCache(tup);
	return res;
}

bool
isGreenplumDbHashable(Oid typid)
{
	/* we can hash all arrays */
	if (typeIsArrayType(typid))
		return true;

	/* if this type is a domain type, get its base type */
	if (get_typtype(typid) == 'd')
		typid = getBaseType(typid);

	/* we can hash all enums */
	if (typeIsEnumType(typid))
		return true;

	/*
	 * NB: Every GPDB-hashable datatype must also be mergejoinable, i.e. must
	 * have a B-tree operator family. There is a sanity check for that in the
	 * opr_sanity_gp regression test. If you modify the list below, please
	 * also update the list in opr_sanity_gp!
	 */
	switch (typid)
	{
		case INT2OID:
		case INT4OID:
		case INT8OID:
		case FLOAT4OID:
		case FLOAT8OID:
		case NUMERICOID:
		case CHAROID:
		case BPCHAROID:
		case TEXTOID:
		case VARCHAROID:
		case BYTEAOID:
		case NAMEOID:
		case OIDOID:
		case TIDOID:
		case REGPROCOID:
		case REGPROCEDUREOID:
		case REGOPEROID:
		case REGOPERATOROID:
		case REGCLASSOID:
		case REGTYPEOID:
		case TIMESTAMPOID:
		case TIMESTAMPTZOID:
		case DATEOID:
		case TIMEOID:
		case TIMETZOID:
		case INTERVALOID:
		case ABSTIMEOID:
		case RELTIMEOID:
		case TINTERVALOID:
		case INETOID:
		case CIDROID:
		case MACADDROID:
		case BITOID:
		case VARBITOID:
		case BOOLOID:
		case ANYARRAYOID:
		case OIDVECTOROID:
		case CASHOID:
		case UUIDOID:
		case COMPLEXOID:
			return true;
		default:
			return false;
	}
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

/*
 * returns 1 is the input int is a power of 2 and 0 otherwise.
 */
static int
ispowof2(int numsegs)
{
	return !(numsegs & (numsegs - 1));
}
