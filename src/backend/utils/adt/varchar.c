/*-------------------------------------------------------------------------
 *
 * varchar.c
 *	  Functions for the built-in types char(n) and varchar(n).
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/adt/varchar.c,v 1.120 2006/12/30 21:21:54 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"


#include "access/hash.h"
#include "access/tuptoaster.h"
#include "libpq/pqformat.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "mb/pg_wchar.h"


/* common code for bpchartypmodin and varchartypmodin */
static int32
anychar_typmodin(ArrayType *ta, const char *typename)
{
	int32		typmod;
	int32	   *tl;
	int			n;

	tl = ArrayGetIntegerTypmods(ta, &n);

	/*
	 * we're not too tense about good error message here because grammar
	 * shouldn't allow wrong number of modifiers for CHAR
	 */
	if (n != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid type modifier")));

	if (*tl < 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("length for type %s must be at least 1", typename)));
	if (*tl > MaxAttrSize)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("length for type %s cannot exceed %d",
						typename, MaxAttrSize)));

	/*
	 * For largely historical reasons, the typmod is VARHDRSZ plus the number
	 * of characters; there is enough client-side code that knows about that
	 * that we'd better not change it.
	 */
	typmod = VARHDRSZ + *tl;

	return typmod;
}

/* common code for bpchartypmodout and varchartypmodout */
static char *
anychar_typmodout(int32 typmod)
{
	char	   *res = (char *) palloc(64);

	if (typmod > VARHDRSZ)
		snprintf(res, 64, "(%d)", (int) (typmod - VARHDRSZ));
	else
		*res = '\0';

	return res;
}


/*
 * CHAR() and VARCHAR() types are part of the ANSI SQL standard. CHAR()
 * is for blank-padded string whose length is specified in CREATE TABLE.
 * VARCHAR is for storing string whose length is at most the length specified
 * at CREATE TABLE time.
 *
 * It's hard to implement these types because we cannot figure out
 * the length of the type from the type itself. I changed (hopefully all) the
 * fmgr calls that invoke input functions of a data type to supply the
 * length also. (eg. in INSERTs, we have the tupleDescriptor which contains
 * the length of the attributes and hence the exact length of the char() or
 * varchar(). We pass this to bpcharin() or varcharin().) In the case where
 * we cannot determine the length, we pass in -1 instead and the input
 * converter does not enforce any length check.
 *
 * We actually implement this as a varlena so that we don't have to pass in
 * the length for the comparison functions. (The difference between these
 * types and "text" is that we truncate and possibly blank-pad the string
 * at insertion time.)
 *
 *															  - ay 6/95
 */


/*****************************************************************************
 *	 bpchar - char()														 *
 *****************************************************************************/

/*
 * bpchar_input -- common guts of bpcharin and bpcharrecv
 *
 * s is the input text of length len (may not be null-terminated)
 * atttypmod is the typmod value to apply
 *
 * Note that atttypmod is measured in characters, which
 * is not necessarily the same as the number of bytes.
 *
 * If the input string is too long, raise an error, unless the extra
 * characters are spaces, in which case they're truncated.  (per SQL)
 */
static BpChar *
bpchar_input(const char *s, size_t len, int32 atttypmod)
{
	BpChar	   *result;
	char	   *r;
	size_t		maxlen;

	/* If typmod is -1 (or invalid), use the actual string length */
	if (atttypmod < (int32) VARHDRSZ)
		maxlen = len;
	else
	{
		size_t		charlen;	/* number of CHARACTERS in the input */

		maxlen = atttypmod - VARHDRSZ;
		charlen = pg_mbstrlen_with_len(s, len);
		if (charlen > maxlen)
		{
			/* Verify that extra characters are spaces, and clip them off */
			size_t		mbmaxlen = pg_mbcharcliplen(s, len, maxlen);
			size_t		j;

			/*
			 * at this point, len is the actual BYTE length of the input
			 * string, maxlen is the max number of CHARACTERS allowed for this
			 * bpchar type, mbmaxlen is the length in BYTES of those chars.
			 */
			for (j = mbmaxlen; j < len; j++)
			{
				if (s[j] != ' ')
					ereport(ERROR,
							(errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
							 errmsg("value too long for type character(%d)",
									(int) maxlen)));
			}

			/*
			 * Now we set maxlen to the necessary byte length, not the number
			 * of CHARACTERS!
			 */
			maxlen = len = mbmaxlen;
		}
		else
		{
			/*
			 * Now we set maxlen to the necessary byte length, not the number
			 * of CHARACTERS!
			 */
			maxlen = len + (maxlen - charlen);
		}
	}

	result = (BpChar *) palloc(maxlen + VARHDRSZ);
	SET_VARSIZE(result, maxlen + VARHDRSZ);
	r = VARDATA(result);
	memcpy(r, s, len);

	/* blank pad the string if necessary */
	if (maxlen > len)
		memset(r + len, ' ', maxlen - len);

	return result;
}

/*
 * Convert a C string to CHARACTER internal representation.  atttypmod
 * is the declared length of the type plus VARHDRSZ.
 */
Datum
bpcharin(PG_FUNCTION_ARGS)
{
	char	   *s = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
	Oid			typelem = PG_GETARG_OID(1);
#endif
	int32		atttypmod = PG_GETARG_INT32(2);
	BpChar	   *result;

	result = bpchar_input(s, strlen(s), atttypmod);
	PG_RETURN_BPCHAR_P(result);
}


/*
 * Convert a CHARACTER value to a C string.
 *
 * Uses the text conversion functions, which is only appropriate if BpChar
 * and text are equivalent types.
 */
Datum
bpcharout(PG_FUNCTION_ARGS)
{
	Datum d = PG_GETARG_DATUM(0);
	char *s; void *tofree; int len; 
	char *result;

	varattrib_untoast_ptr_len(d, &s, &len, &tofree); 

	/* copy and add null term */
	result = (char *) palloc(len + 1);
	memcpy(result, s, len);
	result[len] = '\0';

	if(tofree)
		pfree(tofree);

	PG_RETURN_CSTRING(result);
}

/*
 *		bpcharrecv			- converts external binary format to bpchar
 */
Datum
bpcharrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);

#ifdef NOT_USED
	Oid			typelem = PG_GETARG_OID(1);
#endif
	int32		atttypmod = PG_GETARG_INT32(2);
	BpChar	   *result;
	char	   *str;
	int			nbytes;

	str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
	result = bpchar_input(str, nbytes, atttypmod);
	pfree(str);
	PG_RETURN_BPCHAR_P(result);
}

/*
 *		bpcharsend			- converts bpchar to binary format
 */
Datum
bpcharsend(PG_FUNCTION_ARGS)
{
	/* Exactly the same as textsend, so share code */
	return textsend(fcinfo);
}


/*
 * Converts a CHARACTER type to the specified size.
 *
 * maxlen is the typmod, ie, declared length plus VARHDRSZ bytes.
 * isExplicit is true if this is for an explicit cast to char(N).
 *
 * Truncation rules: for an explicit cast, silently truncate to the given
 * length; for an implicit cast, raise error unless extra characters are
 * all spaces.	(This is sort-of per SQL: the spec would actually have us
 * raise a "completion condition" for the explicit cast case, but Postgres
 * hasn't got such a concept.)
 */
Datum
bpchar(PG_FUNCTION_ARGS)
{
	Datum 		d = PG_GETARG_DATUM(0);
	int32		maxlen = PG_GETARG_INT32(1);
	bool		isExplicit = PG_GETARG_BOOL(2);
	char *p; void *tofree; int plen; 
	int len;
	BpChar	   *result;
	char	   *r;
	char	   *s;
	int			i;
	int			charlen;		/* number of characters in the input string +
								 * VARHDRSZ */

	/* No work if typmod is invalid */
	if (maxlen < (int32) VARHDRSZ)
		return d;

	varattrib_untoast_ptr_len(d, &p, &plen, &tofree);

	charlen = pg_mbstrlen_with_len(p, plen) + VARHDRSZ;

	/* No work if supplied data matches typmod already */
	if (charlen == maxlen)
	{
		if(tofree)
			pfree(tofree);
		return d;
	}

	len = plen + VARHDRSZ;

	if (charlen > maxlen)
	{
		/* Verify that extra characters are spaces, and clip them off */
		size_t		maxmblen;

		maxmblen = pg_mbcharcliplen(p, plen, maxlen - VARHDRSZ) + VARHDRSZ;

		if (!isExplicit)
		{
			for (i = maxmblen - VARHDRSZ; i < plen; i++)
				if (*(p + i) != ' ')
					ereport(ERROR,
							(errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
							 errmsg("value too long for type character(%d)",
									maxlen - VARHDRSZ)));
		}

		len = maxmblen;

		/*
		 * XXX: at this point, maxlen is the necessary byte length+VARHDRSZ,
		 * not the number of CHARACTERS!
		 */
		maxlen = len;
	}
	else
	{
		/*
		 * XXX: at this point, maxlen is the necessary byte length+VARHDRSZ,
		 * not the number of CHARACTERS!
		 */
		maxlen = len + (maxlen - charlen);
	}

	s = p;

	result = palloc(maxlen);
	SET_VARSIZE(result, maxlen);
	r = VARDATA(result);

	memcpy(r, s, len - VARHDRSZ);

	/* blank pad the string if necessary */
	if (maxlen > len)
		memset(r + len - VARHDRSZ, ' ', maxlen - len);

	if(tofree)
		pfree(tofree);

	PG_RETURN_BPCHAR_P(result);
}


/* char_bpchar()
 * Convert char to bpchar(1).
 */
Datum
char_bpchar(PG_FUNCTION_ARGS)
{
	char		c = PG_GETARG_CHAR(0);
	BpChar	   *result;

	result = (BpChar *) palloc(VARHDRSZ + 1);

	SET_VARSIZE(result, VARHDRSZ + 1);
	*(VARDATA(result)) = c;

	PG_RETURN_BPCHAR_P(result);
}


/* bpchar_name()
 * Converts a bpchar() type to a NameData type.
 */
Datum
bpchar_name(PG_FUNCTION_ARGS)
{
	Name		result;
	Datum d = PG_GETARG_DATUM(0);
	char *s; void *tofree; int len;

	varattrib_untoast_ptr_len(d, &s, &len, &tofree);

	/* Truncate to max length for a Name */
	if (len >= NAMEDATALEN)
		len = NAMEDATALEN - 1;

	/* Remove trailing blanks */
	while (len > 0)
	{
		if (*(s + len - 1) != ' ')
			break;
		len--;
	}

	result = (NameData *) palloc(NAMEDATALEN);
	memcpy(NameStr(*result), s, len);

	/* Now null pad to full length... */
	while (len < NAMEDATALEN)
	{
		*(NameStr(*result) + len) = '\0';
		len++;
	}

	if(tofree)
		pfree(tofree);

	PG_RETURN_NAME(result);
}

/* name_bpchar()
 * Converts a NameData type to a bpchar type.
 *
 * Uses the text conversion functions, which is only appropriate if BpChar
 * and text are equivalent types.
 */
Datum
name_bpchar(PG_FUNCTION_ARGS)
{
	Name		s = PG_GETARG_NAME(0);
	BpChar	   *result;

	result = (BpChar *) cstring_to_text(NameStr(*s));
	PG_RETURN_BPCHAR_P(result);
}

Datum
bpchartypmodin(PG_FUNCTION_ARGS)
{
	ArrayType  *ta = PG_GETARG_ARRAYTYPE_P(0);

	PG_RETURN_INT32(anychar_typmodin(ta, "char"));
}

Datum
bpchartypmodout(PG_FUNCTION_ARGS)
{
	int32		typmod = PG_GETARG_INT32(0);

	PG_RETURN_CSTRING(anychar_typmodout(typmod));
}


/*****************************************************************************
 *	 varchar - varchar(n)
 *
 * Note: varchar piggybacks on type text for most operations, and so has no
 * C-coded functions except for I/O and typmod checking.
 *****************************************************************************/

/*
 * varchar_input -- common guts of varcharin and varcharrecv
 *
 * s is the input text of length len (may not be null-terminated)
 * atttypmod is the typmod value to apply
 *
 * Note that atttypmod is measured in characters, which
 * is not necessarily the same as the number of bytes.
 *
 * If the input string is too long, raise an error, unless the extra
 * characters are spaces, in which case they're truncated.  (per SQL)
 *
 * Uses the C string to text conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */
static VarChar *
varchar_input(const char *s, size_t len, int32 atttypmod)
{
	VarChar    *result;
	size_t		maxlen;

	maxlen = atttypmod - VARHDRSZ;

	if (atttypmod >= (int32) VARHDRSZ && len > maxlen)
	{
		/* Verify that extra characters are spaces, and clip them off */
		size_t		mbmaxlen = pg_mbcharcliplen(s, len, maxlen);
		size_t		j;

		for (j = mbmaxlen; j < len; j++)
		{
			if (s[j] != ' ')
				ereport(ERROR,
						(errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
					  errmsg("value too long for type character varying(%d)",
							 (int) maxlen)));
		}

		len = mbmaxlen;
	}

	result = (VarChar *) cstring_to_text_with_len(s, len);
	return result;
}

/*
 * Convert a C string to VARCHAR internal representation.  atttypmod
 * is the declared length of the type plus VARHDRSZ.
 */
Datum
varcharin(PG_FUNCTION_ARGS)
{
	char	   *s = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
	Oid			typelem = PG_GETARG_OID(1);
#endif
	int32		atttypmod = PG_GETARG_INT32(2);
	VarChar    *result;

	result = varchar_input(s, strlen(s), atttypmod);
	PG_RETURN_VARCHAR_P(result);
}


/*
 * Convert a VARCHAR value to a C string.
 *
 * Uses the text to C string conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */
Datum
varcharout(PG_FUNCTION_ARGS)
{
	char	   *result;

	Datum d = PG_GETARG_DATUM(0);
	char *s; void *tofree; int len;

	varattrib_untoast_ptr_len(d, &s, &len, &tofree); 

	/* copy and add null term */
	result = palloc(len + 1);
	memcpy(result, s, len);
	result[len] = '\0';

	if(tofree)
		pfree(tofree);

	PG_RETURN_CSTRING(result);
}

/*
 *		varcharrecv			- converts external binary format to varchar
 */
Datum
varcharrecv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);

#ifdef NOT_USED
	Oid			typelem = PG_GETARG_OID(1);
#endif
	int32		atttypmod = PG_GETARG_INT32(2);
	VarChar    *result;
	char	   *str;
	int			nbytes;

	str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
	result = varchar_input(str, nbytes, atttypmod);
	pfree(str);
	PG_RETURN_VARCHAR_P(result);
}

/*
 *		varcharsend			- converts varchar to binary format
 */
Datum
varcharsend(PG_FUNCTION_ARGS)
{
	/* Exactly the same as textsend, so share code */
	return textsend(fcinfo);
}


/*
 * Converts a VARCHAR type to the specified size.
 *
 * maxlen is the typmod, ie, declared length plus VARHDRSZ bytes.
 * isExplicit is true if this is for an explicit cast to varchar(N).
 *
 * Truncation rules: for an explicit cast, silently truncate to the given
 * length; for an implicit cast, raise error unless extra characters are
 * all spaces.	(This is sort-of per SQL: the spec would actually have us
 * raise a "completion condition" for the explicit cast case, but Postgres
 * hasn't got such a concept.)
 */
Datum
varchar(PG_FUNCTION_ARGS)
{
	Datum 		d = PG_GETARG_DATUM(0);
	int32		maxlen = PG_GETARG_INT32(1);
	bool		isExplicit = PG_GETARG_BOOL(2);
	VarChar    *result;
	size_t		maxmblen;
	int			i;

	char *p; void *tofree; int len;

	varattrib_untoast_ptr_len(d, &p, &len, &tofree);

	/* No work if typmod is invalid or supplied data fits it already */
	if (maxlen < (int32) VARHDRSZ || (len + VARHDRSZ) <= maxlen)
		return d;

	/* only reach here if string is too long... */
	/* truncate multibyte string preserving multibyte boundary */
	maxmblen = pg_mbcharcliplen(p, len, maxlen - VARHDRSZ);

	if (!isExplicit)
	{
		for (i = maxmblen; i < len; i++)
			if (*(p + i) != ' ')
				ereport(ERROR,
						(errcode(ERRCODE_STRING_DATA_RIGHT_TRUNCATION),
					  errmsg("value too long for type character varying(%d)",
							 maxlen - VARHDRSZ)));
	}

	len = maxmblen + VARHDRSZ;
	result = palloc(len);
	SET_VARSIZE(result, len);
	memcpy(VARDATA(result), p, maxmblen); 

	if(tofree)
		pfree(tofree);

	PG_RETURN_VARCHAR_P(result);
}

Datum
varchartypmodin(PG_FUNCTION_ARGS)
{
	ArrayType  *ta = PG_GETARG_ARRAYTYPE_P(0);

	PG_RETURN_INT32(anychar_typmodin(ta, "varchar"));
}

Datum
varchartypmodout(PG_FUNCTION_ARGS)
{
	int32		typmod = PG_GETARG_INT32(0);

	PG_RETURN_CSTRING(anychar_typmodout(typmod));
}


/*****************************************************************************
 * Exported functions
 *****************************************************************************/

/* "True" length (not counting trailing blanks) of a BpChar */
static inline int
bcTruelen(char *p, int len)
{
	int			i;

	for (i = len - 1; i >= 0; i--)
	{
		if (p[i] != ' ')
			break;
	}
	return i + 1;
}

Datum
bpcharlen(PG_FUNCTION_ARGS)
{
	Datum d = PG_GETARG_DATUM(0);
	char* p; void *tofree; int len;

	varattrib_untoast_ptr_len(d, &p, &len, &tofree);

	/* get number of bytes, ignoring trailing spaces */
	len = bcTruelen(p, len);

	/* in multibyte encoding, convert to number of characters */
	if (pg_database_encoding_max_length() != 1)
		len = pg_mbstrlen_with_len(p, len);

	if(tofree)
		pfree(tofree);

	PG_RETURN_INT32(len);
}

Datum
bpcharoctetlen(PG_FUNCTION_ARGS)
{
	Datum d = PG_GETARG_DATUM(0);
	char* p; void *tofree; int len;

	varattrib_untoast_ptr_len(d, &p, &len, &tofree);

	if(tofree)
		pfree(tofree);

	PG_RETURN_INT32(len);
}


/*****************************************************************************
 *	Comparison Functions used for bpchar
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 *****************************************************************************/
static inline bool bpeq(Datum d0, Datum d1)
{
	char* p0; void *tofree0; int len0;
	char* p1; void *tofree1; int len1;

	bool result;

	varattrib_untoast_ptr_len(d0, &p0, &len0, &tofree0);
	varattrib_untoast_ptr_len(d1, &p1, &len1, &tofree1);

	len0 = bcTruelen(p0, len0);
	len1 = bcTruelen(p1, len1);

	/*
	 * Since we only care about equality or not-equality, we can avoid all the
	 * expense of strcoll() here, and just do bitwise comparison.
	 */
	if (len0 != len1)
		result = false;
	else
		result = (memcmp(p0, p1, len0) == 0); 

	if(tofree0)
		pfree(tofree0);
	if(tofree1)
		pfree(tofree1);

	return result;
}


Datum
bpchareq(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL(bpeq(d0, d1)); 
}

Datum
bpcharne(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL((!bpeq(d0, d1))); 
}

static inline int
bpcmp(Datum d0, Datum d1)
{
	char *p0; void *tofree0; int len0;
	char *p1; void *tofree1; int len1;
	int result;

	varattrib_untoast_ptr_len(d0, &p0, &len0, &tofree0);
	varattrib_untoast_ptr_len(d1, &p1, &len1, &tofree1);

	len0 = bcTruelen(p0, len0);
	len1 = bcTruelen(p1, len1);

	result = varstr_cmp(p0, len0, p1, len1);

	if(tofree0)
		pfree(tofree0);
	if(tofree1)
		pfree(tofree1);

	return result;
}

Datum
bpcharlt(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL(bpcmp(d0, d1) < 0);
}

Datum
bpcharle(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL(bpcmp(d0, d1) <= 0);
}

Datum
bpchargt(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL(bpcmp(d0, d1) > 0);
}

Datum
bpcharge(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_BOOL(bpcmp(d0, d1) >= 0);
}

Datum
bpcharcmp(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	PG_RETURN_INT32(bpcmp(d0, d1));
}

Datum
bpchar_larger(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	int cmp = bpcmp(d0, d1);

	return cmp >= 0 ? d0 : d1;
}

Datum
bpchar_smaller(PG_FUNCTION_ARGS)
{
	Datum d0 = PG_GETARG_DATUM(0);
	Datum d1 = PG_GETARG_DATUM(1);

	int cmp = bpcmp(d0, d1);

	return cmp <= 0 ? d0 : d1;
}


/*
 * bpchar needs a specialized hash function because we want to ignore
 * trailing blanks in comparisons.
 *
 * Note: currently there is no need for locale-specific behavior here,
 * but if we ever change the semantics of bpchar comparison to trust
 * strcoll() completely, we'd need to do something different in non-C locales.
 */
Datum
hashbpchar(PG_FUNCTION_ARGS)
{
	Datum d = PG_GETARG_DATUM(0);
	char* p; void *tofree; int len;
	Datum result;

	varattrib_untoast_ptr_len(d, &p, &len, &tofree);

	len = bcTruelen(p, len);

	result = hash_any((unsigned char *) p, len); 

	if(tofree)
		pfree(tofree);

	return result;
}


/*
 * The following operators support character-by-character comparison
 * of bpchar datums, to allow building indexes suitable for LIKE clauses.
 * Note that the regular bpchareq/bpcharne comparison operators are assumed
 * to be compatible with these!
 */

static int
internal_bpchar_pattern_compare(BpChar *arg1, BpChar *arg2)
{
	int			result;
	int			len1,
				len2;

	len1 = VARSIZE_ANY_EXHDR(arg1);
	len2 = VARSIZE_ANY_EXHDR(arg2);

	//len1 = bcTruelen(arg1);
	//len2 = bcTruelen(arg2);

	result = strncmp(VARDATA_ANY(arg1), VARDATA_ANY(arg2), Min(len1, len2));
	if (result != 0)
		return result;
	else if (len1 < len2)
		return -1;
	else if (len1 > len2)
		return 1;
	else
		return 0;
}


Datum
bpchar_pattern_lt(PG_FUNCTION_ARGS)
{
	BpChar	   *arg1 = PG_GETARG_BPCHAR_PP(0);
	BpChar	   *arg2 = PG_GETARG_BPCHAR_PP(1);
	int			result;

	result = internal_bpchar_pattern_compare(arg1, arg2);

	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);

	PG_RETURN_BOOL(result < 0);
}


Datum
bpchar_pattern_le(PG_FUNCTION_ARGS)
{
	BpChar	   *arg1 = PG_GETARG_BPCHAR_PP(0);
	BpChar	   *arg2 = PG_GETARG_BPCHAR_PP(1);
	int			result;

	result = internal_bpchar_pattern_compare(arg1, arg2);

	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);

	PG_RETURN_BOOL(result <= 0);
}


Datum
bpchar_pattern_ge(PG_FUNCTION_ARGS)
{
	BpChar	   *arg1 = PG_GETARG_BPCHAR_PP(0);
	BpChar	   *arg2 = PG_GETARG_BPCHAR_PP(1);
	int			result;

	result = internal_bpchar_pattern_compare(arg1, arg2);

	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);

	PG_RETURN_BOOL(result >= 0);
}


Datum
bpchar_pattern_gt(PG_FUNCTION_ARGS)
{
	BpChar	   *arg1 = PG_GETARG_BPCHAR_PP(0);
	BpChar	   *arg2 = PG_GETARG_BPCHAR_PP(1);
	int			result;

	result = internal_bpchar_pattern_compare(arg1, arg2);

	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);

	PG_RETURN_BOOL(result > 0);
}


Datum
btbpchar_pattern_cmp(PG_FUNCTION_ARGS)
{
	BpChar	   *arg1 = PG_GETARG_BPCHAR_PP(0);
	BpChar	   *arg2 = PG_GETARG_BPCHAR_PP(1);
	int			result;

	result = internal_bpchar_pattern_compare(arg1, arg2);

	PG_FREE_IF_COPY(arg1, 0);
	PG_FREE_IF_COPY(arg2, 1);

	PG_RETURN_INT32(result);
}
