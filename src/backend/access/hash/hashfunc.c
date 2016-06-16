/*-------------------------------------------------------------------------
 *
 * hashfunc.c
 *	  Support functions for hash access method.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/access/hash/hashfunc.c,v 1.55 2008/01/01 19:45:46 momjian Exp $
 *
 * NOTES
 *	  These functions are stored in pg_amproc.	For each operator class
 *	  defined for hash indexes, they compute the hash value of the argument.
 *
 *	  Additional hash functions appear in /utils/adt/ files for various
 *	  specialized datatypes.
 *
 *	  It is expected that every bit of a hash function's 32-bit result is
 *	  as random as every other; failure to ensure this is likely to lead
 *	  to poor performance of hash joins, for example.  In most cases a hash
 *	  function should use hash_any() or its variant hash_uint32().
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "access/tuptoaster.h"


/* Note: this is used for both "char" and boolean datatypes */
Datum
hashchar(PG_FUNCTION_ARGS)
{
	return hash_uint32((int32) PG_GETARG_CHAR(0));
}

Datum
hashint2(PG_FUNCTION_ARGS)
{
	return hash_uint32((int32) PG_GETARG_INT16(0));
}

Datum
hashint4(PG_FUNCTION_ARGS)
{
	return hash_uint32(PG_GETARG_INT32(0));
}

Datum
hashint8(PG_FUNCTION_ARGS)
{
	/*
	 * The idea here is to produce a hash value compatible with the values
	 * produced by hashint4 and hashint2 for logically equal inputs; this is
	 * necessary to support cross-type hash joins across these input types.
	 * Since all three types are signed, we can xor the high half of the int8
	 * value if the sign is positive, or the complement of the high half when
	 * the sign is negative.
	 */
#ifndef INT64_IS_BUSTED
	int64		val = PG_GETARG_INT64(0);
	uint32		lohalf = (uint32) val;
	uint32		hihalf = (uint32) (val >> 32);

	lohalf ^= (val >= 0) ? hihalf : ~hihalf;

	return hash_uint32(lohalf);
#else
	/* here if we can't count on "x >> 32" to work sanely */
	return hash_uint32((int32) PG_GETARG_INT64(0));
#endif
}

Datum
hashoid(PG_FUNCTION_ARGS)
{
	return hash_uint32((uint32) PG_GETARG_OID(0));
}

Datum
hashenum(PG_FUNCTION_ARGS)
{
	return hash_uint32((uint32) PG_GETARG_OID(0));
}

Datum
hashfloat4(PG_FUNCTION_ARGS)
{
	float4		key = PG_GETARG_FLOAT4(0);
	float8		key8;

	/*
	 * On IEEE-float machines, minus zero and zero have different bit patterns
	 * but should compare as equal.  We must ensure that they have the same
	 * hash value, which is most reliably done this way:
	 */
	if (key == (float4) 0)
		PG_RETURN_UINT32(0);

	/*
	 * To support cross-type hashing of float8 and float4, we want to return
	 * the same hash value hashfloat8 would produce for an equal float8 value.
	 * So, widen the value to float8 and hash that.  (We must do this rather
	 * than have hashfloat8 try to narrow its value to float4; that could fail
	 * on overflow.)
	 */
	key8 = key;

	return hash_any((unsigned char *) &key8, sizeof(key8));
}

Datum
hashfloat8(PG_FUNCTION_ARGS)
{
	float8		key = PG_GETARG_FLOAT8(0);

	/*
	 * On IEEE-float machines, minus zero and zero have different bit patterns
	 * but should compare as equal.  We must ensure that they have the same
	 * hash value, which is most reliably done this way:
	 */
	if (key == (float8) 0)
		PG_RETURN_UINT32(0);

	return hash_any((unsigned char *) &key, sizeof(key));
}

Datum
hashoidvector(PG_FUNCTION_ARGS)
{
	oidvector  *key = (oidvector *) PG_GETARG_POINTER(0);

	return hash_any((unsigned char *) key->values, key->dim1 * sizeof(Oid));
}

Datum
hashint2vector(PG_FUNCTION_ARGS)
{
	int2vector *key = (int2vector *) PG_GETARG_POINTER(0);

	return hash_any((unsigned char *) key->values, key->dim1 * sizeof(int2));
}

Datum
hashname(PG_FUNCTION_ARGS)
{
	char	   *key = NameStr(*PG_GETARG_NAME(0));
	int			keylen = strlen(key);

	Assert(keylen < NAMEDATALEN);		/* else it's not truncated correctly */

	return hash_any((unsigned char *) key, keylen);
}


extern void varattrib_untoast_ptr_len(Datum d, char **datastart, int *len, void **tofree);
Datum
hashtext(PG_FUNCTION_ARGS)
{
	text	   *key = PG_GETARG_TEXT_PP(0);
	Datum		result;

	/*
	 * Note: this is currently identical in behavior to hashvarlena, but keep
	 * it as a separate function in case we someday want to do something
	 * different in non-C locales.	(See also hashbpchar, if so.)
	 */
	result = hash_any((unsigned char *) VARDATA_ANY(key),
					  VARSIZE_ANY_EXHDR(key));

	return result;
}

/*
 * hashvarlena() can be used for any varlena datatype in which there are
 * no non-significant bits, ie, distinct bitpatterns never compare as equal.
 */
Datum
hashvarlena(PG_FUNCTION_ARGS)
{
	struct varlena *key = PG_GETARG_VARLENA_PP(0);
	Datum		result;

	result = hash_any((unsigned char *) VARDATA_ANY(key),
					  VARSIZE_ANY_EXHDR(key));

	return result;
}

/*
 * This hash function was written by Bob Jenkins
 * (bob_jenkins@burtleburtle.net), and superficially adapted
 * for PostgreSQL by Neil Conway. For more information on this
 * hash function, see http://burtleburtle.net/bob/hash/#lookup
 * and http://burtleburtle.net/bob/hash/lookup3.txt. Further
 * information on the original version of the hash function can
 * be found in Bob's article in Dr. Dobb's Journal, Sept. 1997.
 */

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#ifndef WORS_BIGENDIAN
#define HASH_LITTLE_ENDIAN 1
#define HASH_BIG_ENDIAN 0
#else
#define HASH_LITTLE_ENDIAN 0
#define HASH_BIG_ENDIAN 1
#endif

/*----------
 * mix -- mix 3 32-bit values reversibly.
 * 
 * This is reversible, so any information in (a,b,c) before mix() is
 * still in (a,b,c) after mix().
 * 
 * If four pairs of (a,b,c) inputs are run through mix(), or through
 * mix() in reverse, there are at least 32 bits of the output that
 * are sometimes the same for one pair and different for another pair.
 * This was tested for:
 * * pairs that differed by one bit, by two bits, in any combination
 *   of top bits of (a,b,c), or in any combination of bottom bits of
 *   (a,b,c).
 * * "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
 *   the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 *   is commonly produced by subtraction) look like a single 1-bit
 *   difference.
 * * the base values were pseudorandom, all zero but one bit set, or
 *   all zero plus a counter that starts at zero.
 * 
 * Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
 * satisfy this are
 *     4  6  8 16 19  4
 *     9 15  3 18 27 15
 *    14  9  3  7 17  3
 * Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
 * for "differ" defined as + with a one-bit base and a two-bit delta.  I
 * used http://burtleburtle.net/bob/hash/avalanche.html to choose
 * the operations, constants, and arrangements of the variables.
 * 
 * This does not achieve avalanche.  There are input bits of (a,b,c)
 * that fail to affect some output bits of (a,b,c), especially of a.  The
 * most thoroughly mixed value is c, but it doesn't really even achieve
 * avalanche in c.
 * 
 * This allows some parallelism.  Read-after-writes are good at doubling
 * the number of bits affected, so the goal of mixing pulls in the opposite
 * direction as the goal of parallelism.  I did what I could.  Rotates
 * seem to cost as much as shifts on every machine I could lay my hands
 * on, and rotates are much kinder to the top and bottom bits, so I used
 * rotates.
 *----------
 */
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*----------
 * final -- final mixing of 3 32-bit values (a,b,c) into c
 * 
 * Pairs of (a,b,c) values differing in only a few bits will usually
 * produce values of c that look totally different.  This was tested for
 * - pairs that differed by one bit, by two bits, in any combination
 *   of top bits of (a,b,c), or in any combination of bottom bits of
 *   (a,b,c).
 * - "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
 *   the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 *   is commonly produced by subtraction) look like a single 1-bit
 *   difference.
 * - the base values were pseudorandom, all zero but one bit set, or
 *   all zero plus a counter that starts at zero.
 * 
 * These constants passed:
 *  14 11 25 16 4 14 24
 *  12 14 25 16 4 14 24
 * and these came close:
 *   4  8 15 26 3 22 24
 *  10  8 15 26 3 22 24
 *  11  8 15 26 3 22 24
 *----------
 */
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*----------
 *  This works on all machines.  To be useful, it requires
 *  -- that the key be an array of uint32's, and
 *  -- that the length be the number of uint32's in the key
 * 
 *  The function hashword() is identical to hashlittle() on little-endian
 *  machines, and identical to hashbig() on big-endian machines,
 *  except that the length has to be measured in uint32s rather than in
 *  bytes.  hashlittle() is more complicated than hashword() only because
 *  hashlittle() has to dance around fitting the key bytes into registers.
 *----------
 */
extern Datum
hashword(
const uint32 *k,                   /* the key, an array of uint32 values */
size_t          length,               /* the length of the key, in uint32s */
uint32        initval);         /* the previous hash, or an arbitrary value */
Datum
hashword(
const uint32 *k,                   /* the key, an array of uint32 values */
size_t          length,               /* the length of the key, in uint32s */
uint32        initval)         /* the previous hash, or an arbitrary value */
{
  uint32 a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((uint32)length)<<2) + initval;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32's */
  switch(length)                     /* all the case statements fall through */
  { 
  case 3 : c+=k[2];
  case 2 : b+=k[1];
  case 1 : a+=k[0];
    final(a,b,c);
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  return UInt32GetDatum(c);
}


/*----------
 * hashword2() -- same as hashword(), but take two seeds and return two
 * 32-bit values.  pc and pb must both be nonnull, and *pc and *pb must
 * both be initialized with seeds.  If you pass in (*pb)==0, the output 
 * (*pc) will be the same as the return value from hashword().
 *----------
 */
extern void hashword2 (
const uint32 *k,                   /* the key, an array of uint32 values */
size_t          length,               /* the length of the key, in uint32s */
uint32       *pc,                      /* IN: seed OUT: primary hash value */
uint32       *pb);               /* IN: more seed OUT: secondary hash value */
void hashword2 (
const uint32 *k,                   /* the key, an array of uint32 values */
size_t          length,               /* the length of the key, in uint32s */
uint32       *pc,                      /* IN: seed OUT: primary hash value */
uint32       *pb)               /* IN: more seed OUT: secondary hash value */
{
  uint32 a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)(length<<2)) + *pc;
  c += *pb;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32's */
  switch(length)                     /* all the case statements fall through */
  { 
  case 3 : c+=k[2];
  case 2 : b+=k[1];
  case 1 : a+=k[0];
    final(a,b,c);
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  *pc=c; *pb=b;
}


/*----------
 * hashlittle() -- hash a variable-length key into a 32-bit value
 *   k       : the key (the unaligned variable-length array of bytes)
 *   length  : the length of the key, counting by bytes
 *   initval : can be any 4-byte value
 * Returns a 32-bit value.  Every bit of the key affects every bit of
 * the return value.  Two keys differing by one or two bits will have
 * totally different hash values.
 * 
 * The best hash table sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *   h = (h & hashmask(10));
 * In which case, the hash table should have hashsize(10) elements.
 * 
 * If you are hashing n strings (uint8 **)k, do it like this:
 *   for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);
 * 
 * By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
 * code any way you wish, private, educational, or commercial.  It's free.
 * 
 * Use for hash table lookup, or anything where one collision in 2^^32 is
 * acceptable.  Do NOT use for cryptographic purposes.
 *----------
 */

extern Datum
hashlittle( const void *key, size_t length, uint32 initval);
Datum
hashlittle( const void *key, size_t length, uint32 initval)
{
  uint32 a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32 *k = (const uint32 *)key;         /* read 32-bit chunks */
#ifdef VALGRIND
    const uint8  *k8;
#endif

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return UInt32GetDatum(c); /* zero length requires no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8 *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return UInt32GetDatum(c);
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16 *k = (const uint16 *)key;         /* read 16-bit chunks */
    const uint8  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32)k[1])<<16);
      b += k[2] + (((uint32)k[3])<<16);
      c += k[4] + (((uint32)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8 *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32)k[5])<<16);
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 11: c+=((uint32)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 7 : b+=((uint32)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 3 : a+=((uint32)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return UInt32GetDatum(c);     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8 *k = (const uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32)k[1])<<8;
      a += ((uint32)k[2])<<16;
      a += ((uint32)k[3])<<24;
      b += k[4];
      b += ((uint32)k[5])<<8;
      b += ((uint32)k[6])<<16;
      b += ((uint32)k[7])<<24;
      c += k[8];
      c += ((uint32)k[9])<<8;
      c += ((uint32)k[10])<<16;
      c += ((uint32)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32)k[11])<<24;
    case 11: c+=((uint32)k[10])<<16;
    case 10: c+=((uint32)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((uint32)k[7])<<24;
    case 7 : b+=((uint32)k[6])<<16;
    case 6 : b+=((uint32)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((uint32)k[3])<<24;
    case 3 : a+=((uint32)k[2])<<16;
    case 2 : a+=((uint32)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return UInt32GetDatum(c);
    }
  }

  final(a,b,c);
  return UInt32GetDatum(c);
}


/*----------
 * hashlittle2: return 2 32-bit hash values
 *
 * This is identical to hashlittle(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 *----------
 */
extern void hashlittle2( 
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  uint32   *pc,        /* IN: primary initval, OUT: primary hash */
  uint32   *pb);        /* IN: secondary initval, OUT: secondary hash */
void hashlittle2( 
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  uint32   *pc,        /* IN: primary initval, OUT: primary hash */
  uint32   *pb)        /* IN: secondary initval, OUT: secondary hash */
{
  uint32 a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + *pc;
  c += *pb;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32 *k = (const uint32 *)key;         /* read 32-bit chunks */
#ifdef VALGRIND
    const uint8  *k8;
#endif

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8 *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16 *k = (const uint16 *)key;         /* read 16-bit chunks */
    const uint8  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32)k[1])<<16);
      b += k[2] + (((uint32)k[3])<<16);
      c += k[4] + (((uint32)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8 *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32)k[5])<<16);
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 11: c+=((uint32)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32)k[3])<<16);
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 7 : b+=((uint32)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32)k[1])<<16);
             break;
    case 3 : a+=((uint32)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8 *k = (const uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32)k[1])<<8;
      a += ((uint32)k[2])<<16;
      a += ((uint32)k[3])<<24;
      b += k[4];
      b += ((uint32)k[5])<<8;
      b += ((uint32)k[6])<<16;
      b += ((uint32)k[7])<<24;
      c += k[8];
      c += ((uint32)k[9])<<8;
      c += ((uint32)k[10])<<16;
      c += ((uint32)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32)k[11])<<24;
    case 11: c+=((uint32)k[10])<<16;
    case 10: c+=((uint32)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((uint32)k[7])<<24;
    case 7 : b+=((uint32)k[6])<<16;
    case 6 : b+=((uint32)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((uint32)k[3])<<24;
    case 3 : a+=((uint32)k[2])<<16;
    case 2 : a+=((uint32)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }
  }

  final(a,b,c);
  *pc=c; *pb=b;
}



/*----------
 * hashbig():
 * This is the same as hashword() on big-endian machines.  It is different
 * from hashlittle() on all machines.  hashbig() takes advantage of
 * big-endian byte ordering. 
 *----------
 */
extern Datum
hashbig( const void *key, size_t length, uint32 initval);
Datum
hashbig( const void *key, size_t length, uint32 initval)
{
  uint32 a,b,c;
  union { const void *ptr; size_t i; } u; /* to cast key to (size_t) happily */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + initval;

  u.ptr = key;
  if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32 *k = (const uint32 *)key;         /* read 32-bit chunks */
#ifdef VALGRIND
    const uint8  *k8;
#endif

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]<<8" actually reads beyond the end of the string, but
     * then shifts out the part it's not allowed to read.  Because the
     * string is aligned, the illegal read is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff00; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff0000; a+=k[0]; break;
    case 5 : b+=k[1]&0xff000000; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff00; break;
    case 2 : a+=k[0]&0xffff0000; break;
    case 1 : a+=k[0]&0xff000000; break;
    case 0 : return UInt32GetDatum(c);     /* zero length requires no mixing */
    }

#else  /* make valgrind happy */

    k8 = (const uint8 *)k;
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32)k8[10])<<8;  /* fall through */
    case 10: c+=((uint32)k8[9])<<16;  /* fall through */
    case 9 : c+=((uint32)k8[8])<<24;  /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32)k8[6])<<8;   /* fall through */
    case 6 : b+=((uint32)k8[5])<<16;  /* fall through */
    case 5 : b+=((uint32)k8[4])<<24;  /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32)k8[2])<<8;   /* fall through */
    case 2 : a+=((uint32)k8[1])<<16;  /* fall through */
    case 1 : a+=((uint32)k8[0])<<24; break;
    case 0 : return UInt32GetDatum(c);
    }

#endif /* !VALGRIND */

  } else {                        /* need to read the key one byte at a time */
    const uint8 *k = (const uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += ((uint32)k[0])<<24;
      a += ((uint32)k[1])<<16;
      a += ((uint32)k[2])<<8;
      a += ((uint32)k[3]);
      b += ((uint32)k[4])<<24;
      b += ((uint32)k[5])<<16;
      b += ((uint32)k[6])<<8;
      b += ((uint32)k[7]);
      c += ((uint32)k[8])<<24;
      c += ((uint32)k[9])<<16;
      c += ((uint32)k[10])<<8;
      c += ((uint32)k[11]);
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[11];
    case 11: c+=((uint32)k[10])<<8;
    case 10: c+=((uint32)k[9])<<16;
    case 9 : c+=((uint32)k[8])<<24;
    case 8 : b+=k[7];
    case 7 : b+=((uint32)k[6])<<8;
    case 6 : b+=((uint32)k[5])<<16;
    case 5 : b+=((uint32)k[4])<<24;
    case 4 : a+=k[3];
    case 3 : a+=((uint32)k[2])<<8;
    case 2 : a+=((uint32)k[1])<<16;
    case 1 : a+=((uint32)k[0])<<24;
             break;
    case 0 : return UInt32GetDatum(c);
    }
  }

  final(a,b,c);
  return UInt32GetDatum(c);
}

/*----------
 * hashbig2: return 2 32-bit hash values
 *
 * This is identical to hashbig(), except it returns two 32-bit hash
 * values instead of just one.  This is good enough for hash table
 * lookup with 2^^64 buckets, or if you want a second hash if you're not
 * happy with the first, or if you want a probably-unique 64-bit ID for
 * the key.  *pc is better mixed than *pb, so use *pc first.  If you want
 * a 64-bit value do something like "*pc + (((uint64_t)*pb)<<32)".
 *----------
 */
extern void hashbig2( 
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  uint32   *pc,        /* IN: primary initval, OUT: primary hash */
  uint32   *pb);        /* IN: secondary initval, OUT: secondary hash */
void hashbig2( 
  const void *key,       /* the key to hash */
  size_t      length,    /* length of the key */
  uint32   *pc,        /* IN: primary initval, OUT: primary hash */
  uint32   *pb)        /* IN: secondary initval, OUT: secondary hash */
{
  uint32 a,b,c;
  union { const void *ptr; size_t i; } u; /* to cast key to (size_t) happily */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32)length) + *pc;
  c += *pb;

  u.ptr = key;
  if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32 *k = (const uint32 *)key;         /* read 32-bit chunks */
#ifdef VALGRIND
    const uint8  *k8;
#endif

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]<<8" actually reads beyond the end of the string, but
     * then shifts out the part it's not allowed to read.  Because the
     * string is aligned, the illegal read is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff00; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff0000; a+=k[0]; break;
    case 5 : b+=k[1]&0xff000000; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff00; break;
    case 2 : a+=k[0]&0xffff0000; break;
    case 1 : a+=k[0]&0xff000000; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#else  /* make valgrind happy */

    k8 = (const uint8 *)k;
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32)k8[10])<<8;  /* fall through */
    case 10: c+=((uint32)k8[9])<<16;  /* fall through */
    case 9 : c+=((uint32)k8[8])<<24;  /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32)k8[6])<<8;   /* fall through */
    case 6 : b+=((uint32)k8[5])<<16;  /* fall through */
    case 5 : b+=((uint32)k8[4])<<24;  /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32)k8[2])<<8;   /* fall through */
    case 2 : a+=((uint32)k8[1])<<16;  /* fall through */
    case 1 : a+=((uint32)k8[0])<<24; break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }

#endif /* !VALGRIND */

  } else {                        /* need to read the key one byte at a time */
    const uint8 *k = (const uint8 *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += ((uint32)k[0])<<24;
      a += ((uint32)k[1])<<16;
      a += ((uint32)k[2])<<8;
      a += ((uint32)k[3]);
      b += ((uint32)k[4])<<24;
      b += ((uint32)k[5])<<16;
      b += ((uint32)k[6])<<8;
      b += ((uint32)k[7]);
      c += ((uint32)k[8])<<24;
      c += ((uint32)k[9])<<16;
      c += ((uint32)k[10])<<8;
      c += ((uint32)k[11]);
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[11];
    case 11: c+=((uint32)k[10])<<8;
    case 10: c+=((uint32)k[9])<<16;
    case 9 : c+=((uint32)k[8])<<24;
    case 8 : b+=k[7];
    case 7 : b+=((uint32)k[6])<<8;
    case 6 : b+=((uint32)k[5])<<16;
    case 5 : b+=((uint32)k[4])<<24;
    case 4 : a+=k[3];
    case 3 : a+=((uint32)k[2])<<8;
    case 2 : a+=((uint32)k[1])<<16;
    case 1 : a+=((uint32)k[0])<<24;
             break;
    case 0 : *pc=c; *pb=b; return;  /* zero length strings require no mixing */
    }
  }

  final(a,b,c);
  *pc=c; *pb=b;
}

/*
 * hash_any() -- hash a variable-length key into a 32-bit value
 *		k		: the key (the unaligned variable-length array of bytes)
 *		len		: the length of the key, counting by bytes
 *
 * Returns a uint32 value.	Every bit of the key affects every bit of
 * the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 * About 6*len+35 instructions. The best hash table sizes are powers
 * of 2.  There is no need to do mod a prime (mod is sooo slow!).
 * If you need less than 32 bits, use a bitmask.
 */
Datum
hash_any(register const unsigned char *k, register int keylen)
{
#ifndef WORDS_BIGENDIAN
	return hashlittle(k, keylen, 3923095);
#else
	return hashbig(k, keylen, 3923095);
#endif
}

/*
 * hash_uint32() -- hash a 32-bit value
 *
 * This has the same result (at least on little-endian machines) as
 *		hash_any(&k, sizeof(uint32))
 * but is faster and doesn't force the caller to store k into memory.
 */
Datum
hash_uint32(uint32 k)
{
	register uint32 a,
				b,
				c;

	a = 0xdeadbeef + k;
	b = 0xdeadbeef;
	c = 3923095 + (uint32) sizeof(uint32);

	mix(a, b, c);

	/* report the result */
	return UInt32GetDatum(c);
}
