/*-------------------------------------------------------------------------
 *
 * complex_type.c
 *	  Core I/O functions for the built-in complex type.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2016-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	src/backend/utils/adt/complex_type.c
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#include "access/hash.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "libpq/pqformat.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/complex_type.h"
#include "utils/float.h"
#include "utils/float_utils.h"
#include "utils/geo_decls.h"
#include "utils/numeric.h"

#define dp_pl(a, b) DatumGetFloat8(DirectFunctionCall2(float8pl, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_mi(a, b) DatumGetFloat8(DirectFunctionCall2(float8mi, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_mul(a, b) DatumGetFloat8(DirectFunctionCall2(float8mul, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_div(a, b) DatumGetFloat8(DirectFunctionCall2(float8div, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_pow(a, b) DatumGetFloat8(DirectFunctionCall2(dpow, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_atan2(a, b) DatumGetFloat8(DirectFunctionCall2(datan2, Float8GetDatum((a)), Float8GetDatum((b))))
#define dp_exp(a) DatumGetFloat8(DirectFunctionCall1(dexp, Float8GetDatum((a))))
#define dp_log1(a) DatumGetFloat8(DirectFunctionCall1(dlog1, Float8GetDatum((a))))
#define dp_sin(a) DatumGetFloat8(DirectFunctionCall1(dsin, Float8GetDatum((a))))
#define dp_cos(a) DatumGetFloat8(DirectFunctionCall1(dcos, Float8GetDatum((a))))
#define dp_sinh(a) DatumGetFloat8(DirectFunctionCall1(dsinh, Float8GetDatum((a))))
#define dp_cosh(a) DatumGetFloat8(DirectFunctionCall1(dcosh, Float8GetDatum((a))))
#define dp_asin(a) DatumGetFloat8(DirectFunctionCall1(dasin, Float8GetDatum((a))))

/*
 * decode a double precision from a cstring
 *
 * This is copy-pasted from float8in. The difference is that this returns a pointer
 * to the end of valid float, so that the caller can continue processing.
 */
static double
complex_decode_double(char **num_p)
{
	char	   *num = *num_p;
	char	   *orig_num;
	double		val;
	char	   *endptr;

	/*
	 * endptr points to the first character _after_ the sequence we recognized
	 * as a valid floating point number. orig_num points to the original input
	 * string.
	 */
	orig_num = num;

	/* skip leading whitespace */
	while (*num != '\0' && isspace((unsigned char) *num))
		num++;

	/*
	 * Check for an empty-string input to begin with, to avoid the vagaries of
	 * strtod() on different platforms.
	 */
	if (*num == '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			 errmsg("invalid input syntax for type complex: \"%s\"",
					orig_num)));

	errno = 0;
	val = strtod(num, &endptr);

	/* did we not see anything that looks like a double? */
	if (endptr == num || errno != 0)
	{
		int			save_errno = errno;

		/*
		 * C99 requires that strtod() accept NaN, [+-]Infinity, and [+-]Inf,
		 * but not all platforms support all of these (and some accept them
		 * but set ERANGE anyway...)  Therefore, we check for these inputs
		 * ourselves if strtod() fails.
		 *
		 * Note: C99 also requires hexadecimal input as well as some extended
		 * forms of NaN, but we consider these forms unportable and don't try
		 * to support them.  You can use 'em if your strtod() takes 'em.
		 */
		if (pg_strncasecmp(num, "NaN", 3) == 0)
		{
			val = get_float8_nan();
			endptr = num + 3;
		}
		else if (pg_strncasecmp(num, "Infinity", 8) == 0)
		{
			val = get_float8_infinity();
			endptr = num + 8;
		}
		else if (pg_strncasecmp(num, "+Infinity", 9) == 0)
		{
			val = get_float8_infinity();
			endptr = num + 9;
		}
		else if (pg_strncasecmp(num, "-Infinity", 9) == 0)
		{
			val = -get_float8_infinity();
			endptr = num + 9;
		}
		else if (pg_strncasecmp(num, "inf", 3) == 0)
		{
			val = get_float8_infinity();
			endptr = num + 3;
		}
		else if (pg_strncasecmp(num, "+inf", 4) == 0)
		{
			val = get_float8_infinity();
			endptr = num + 4;
		}
		else if (pg_strncasecmp(num, "-inf", 4) == 0)
		{
			val = -get_float8_infinity();
			endptr = num + 4;
		}
		else if (save_errno == ERANGE)
		{
			/*
			 * Some platforms return ERANGE for denormalized numbers (those
			 * that are not zero, but are too close to zero to have full
			 * precision).  We'd prefer not to throw error for that, so try to
			 * detect whether it's a "real" out-of-range condition by checking
			 * to see if the result is zero or huge.
			 */
			if (val == 0.0 || val >= HUGE_VAL || val <= -HUGE_VAL)
				ereport(ERROR,
						(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
				   errmsg("\"%s\" is out of range for type double precision",
						  orig_num)));
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			 errmsg("invalid input syntax for type complex: \"%s\"",
					orig_num)));
	}
#ifdef HAVE_BUGGY_SOLARIS_STRTOD
	else
	{
		/*
		 * Many versions of Solaris have a bug wherein strtod sets endptr to
		 * point one byte beyond the end of the string when given "inf" or
		 * "infinity".
		 */
		if (endptr != num && endptr[-1] == '\0')
			endptr--;
	}
#endif   /* HAVE_BUGGY_SOLARIS_STRTOD */

	/*** end of float8in ***/

	/*
	 * Instead of checking for garbage at end of the string, return pointer
	 * to where we stopped.
	 */

	*num_p = endptr;
	return val;
}

/*
 *	complex_in	- converts "num" to complex
 *				  restricted syntax are:
 *				  {<sp>} [+|-] {digit} [.{digit}] [<exp>] {<sp>} [+|-] {<sp>} {digit} [.{digit}] [<exp>]i
 *				  or
 *				  {<sp>} [+|-] {digit} [.{digit}] [<exp>]
 *				  or
 *				  {<sp>} [+|-] {digit} [.{digit}] [<exp>]i
 *				  where <sp> is a space, digit is 0-9,
 *				  <exp> is "e" or "E" followed by an integer.
 */
Datum
complex_in(PG_FUNCTION_ARGS)
{
	char	   *num = PG_GETARG_CSTRING(0);
	double		val = 0;
	double		re = 0;
	double		im = 0;
	int			im_sign = 1;
	char	   *orig_num = num;
	Complex    *comp = (Complex *) palloc(sizeof(Complex));

	if ('\0' == *num)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
						errmsg("invalid input syntax for type complex: \"%s\"", orig_num)));
	}

	val = complex_decode_double(&num);
	/* first part is the imaginary part, so there is only imaginary */
	if ('i' == *num)
	{
		im = val;
		num++;
	}
	/* first part is the real part */
	else
	{
		re = val;
		/* skip spaces after the real part */
		while ('\0' != *num && isspace(*num))
		{
			num++;
		}
		/* there is the imaginary part */
		if ('-' == *num || '+' == *num)
		{
			if ('-' == *num)
			{
				im_sign = -1;
			}
			num++;

			/*
			 * skip whitespaces after the plus/minus operator between the real part
			 * and the imaginary part
			 */
			while ('\0' != *num && isspace(*num))
			{
				num++;
			}
			/* get the imaginary part */
			im = complex_decode_double(&num);
			if ('i' == *num)
			{
				/* store the sign of the imaginary with the imaginary part */
				im = im * im_sign;
				num++;
			}
			else
			{
				ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
								errmsg("invalid input syntax for type complex: \"%s\"", orig_num)));
			}
		}
	}

	/* skip trailing whitespaces */
	while ('\0' != *num && isspace(*num))
	{
		num++;
	}

	if ('\0' != *num)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
						errmsg("invalid input syntax for type complex: \"%s\"", orig_num)));
	}

	INIT_COMPLEX(comp, re, im);
	PG_RETURN_COMPLEX_P(comp);
}

/* max size of the representation of a complex number */
#define MAXCOMPLEXWIDTH 320
/*
 * outputs the real/imag part of a complex number
 */
static void
complex_part_out(char **p, char *limit, double part)
{
	switch (is_infinite(part))
	{
		case 1:
			(*p) += snprintf((*p), limit - (*p), "%s", "Infinity");
			break;
		case -1:
			(*p) += snprintf((*p), limit - (*p), "%s", "-Infinity");
			break;
		default:
			if (isnan(part))
			{
				(*p) += snprintf((*p), limit - (*p), "%s", "NaN");
			}
			else
			{
				int	ndig = DBL_DIG + extra_float_digits;

				if (ndig < 1)
				{
					ndig = 1;
				}
				(*p) += snprintf((*p), limit - (*p), "%.*g", ndig, part);
			}
	}
}

/*
 * complex_out		- converts complex number to a string
 *					  with format x +/- yi
 */
Datum
complex_out(PG_FUNCTION_ARGS)
{
	Complex    *num = PG_GETARG_COMPLEX_P(0);
	char	   *ascii = (char *) palloc(MAXCOMPLEXWIDTH + 1);
	char	   *limit = ascii + MAXCOMPLEXWIDTH + 1;
	char	   *p = ascii;

	*p = '\0';
	complex_part_out(&p, limit, re(num));
	p += snprintf(p, limit - p, " %c ", (im(num) >= 0 || isnan(im(num))) ? '+' : '-');
	
	/*
	 * Complex numbers with 0 imaginary part have different meaning whether the imaginary part
	 * is -0 or 0. E.x. sqrt(-1 + 0i) = 0 + 1i V.S. sqrt(-1 - 0i) = 0 - 1i. So we must output
	 * the sign of the imaginary part if it equals 0.
	 */
	complex_part_out(&p, limit, (im(num) != 0) ? fabs(im(num)) : im(num));
	snprintf(p, limit - p, "%c", 'i');
	PG_RETURN_CSTRING(ascii);
}

/*
 *		complex_recv			- converts external binary format to complex
 *
 *		We represent a complex number by two double numbers
 *		(one for the real part, the other one for the imaginary part).
 *		This rule is not perfect but it gives us portability across
 *		most IEEE-float-using architectures.
 */
Datum
complex_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	Complex    *num = (Complex *) palloc(sizeof(Complex));
	double		real = pq_getmsgfloat8(buf);
	double		imag = pq_getmsgfloat8(buf);

	INIT_COMPLEX(num, real, imag);

	PG_RETURN_COMPLEX_P(num);
}

/*
 *		complex_send			- converts complex to binary format
 */
Datum
complex_send(PG_FUNCTION_ARGS)
{
	StringInfoData buf;
	Complex    *num = PG_GETARG_COMPLEX_P(0);

	pq_begintypsend(&buf);
	pq_sendfloat8(&buf, re(num));
	pq_sendfloat8(&buf, im(num));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

static Complex *
construct_complex_in(double re, double im)
{
	Complex    *num = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(num, re, im);

	return num;
}

/*
 *		construct_complex		- construct a complex number with two double,
 *								  returns arg1 + arg2i
 */
Datum
construct_complex(PG_FUNCTION_ARGS)
{
	double		re = PG_GETARG_FLOAT8(0);
	double		im = PG_GETARG_FLOAT8(1);

	PG_RETURN_COMPLEX_P(construct_complex_in(re, im));
}

/*
 *		construct_complex_trig	- returns arg1*cos(arg2) + arg1*sin(arg2)i
 */
extern Datum
construct_complex_trig(PG_FUNCTION_ARGS)
{
	double		mag = PG_GETARG_FLOAT8(0);
	double		arg = PG_GETARG_FLOAT8(1);
	Complex    *result = construct_complex_in(mag * dp_cos(arg), mag * dp_sin(arg));

	CHECKFLOATVAL(re(result), isinf(mag), true);
	CHECKFLOATVAL(im(result), isinf(mag), true);
	PG_RETURN_COMPLEX_P(result);
}

/*
 *		complex_re				- get the real part of arg1
 */
Datum
complex_re(PG_FUNCTION_ARGS)
{
	Complex    *num = PG_GETARG_COMPLEX_P(0);

	PG_RETURN_FLOAT8(re(num));
}

/*
 *		complex_im				- get the imaginary part of arg1
 */
Datum
complex_im(PG_FUNCTION_ARGS)
{
	Complex    *num = PG_GETARG_COMPLEX_P(0);

	PG_RETURN_FLOAT8(im(num));
}

/*
 *		complex_arg				- returns the phase of arg1
 */
Datum
complex_arg(PG_FUNCTION_ARGS)
{
	Complex    *comp = PG_GETARG_COMPLEX_P(0);;
	double		result;

	result = dp_atan2(im(comp), re(comp));

	PG_RETURN_FLOAT8(result);
}

/*
 *		complex_mag				- returns magnitude of arg1
 */
Datum
complex_mag(PG_FUNCTION_ARGS)
{
	Complex    *comp = PG_GETARG_COMPLEX_P(0);
	double		result = 0;

	result = hypot(re(comp), im(comp));

	CHECKFLOATVAL(result, isinf(re(comp)) || isinf(im(comp)),
				  0 == re(comp) && 0 == im(comp));
	PG_RETURN_FLOAT8(result);
}

/*
 *		complex_conj			- return conjunction of arg1
 */
Datum
complex_conj(PG_FUNCTION_ARGS)
{
	Complex    *comp = PG_GETARG_COMPLEX_P(0);

	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, re(comp), -1 * im(comp));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_hash			- return hash value of arg1
 */
Datum
complex_hash(PG_FUNCTION_ARGS)
{

	Complex    *num = PG_GETARG_COMPLEX_P(0);
	struct
	{
		double		x,
					y;
	}			key;

	key.x = re(num);
	key.y = im(num);

	/*
	 * On IEEE-float machines, minus zero and zero have different bit patterns
	 * but should compare as equal.  We must ensure that they have the same
	 * hash value, which is most easily done this way:
	 */
	if (0 == key.x)
	{
		key.x = (float8) 0;
	}
	if (0 == key.y)
	{
		key.y = (float8) 0;
	}

	return hash_any((unsigned char *) &key, sizeof(key));
}

static int
complex_abs_cmp_internal(Complex *a, Complex *b)
{
	double a_x = re(a);
	double a_y = im(a);
	double b_x = re(b);
	double b_y = im(b);
	
	
	/* We can only define a partial order when comparing complex numbers so
	 * we compare real parts first and if they are equal, we compare imaginary
	 * parts
	 */
	if (a_x > b_x)
	{
		return 1;
	}
	else if (a_x == b_x)
	{
		if (a_y > b_y)
		{
			return 1;
		}
		else if (a_y < b_y)
		{
			return -1;
		}
	}
	else if (a_x < b_x)
	{
		return -1;
	}
	
	return 0;
}
	
/*
 *		complex_lt				- checks whether arg1 is less than arg2
 */
Datum
complex_lt(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) < 0);
}

/*
 *		complex_gt				- checks whether arg1 is greater than arg2
 */
Datum
complex_gt(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) > 0);
}

/*
 *		complex_lte				- checks whether arg1 is less than or equal to arg2
 */
Datum
complex_lte(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) <= 0);
}

/*
 *		complex_gte				- checks whether arg1 is greater than or equal to arg2
 */
Datum
complex_gte(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) >= 0);
}

/*
 *		complex_eq				- checks whether arg1 equals arg2
 */
Datum
complex_eq(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) == 0);
}

/*
 *		complex_ne				- checks whether arg1 not equals arg2
 */
Datum
complex_ne(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) != 0);
}

Datum
complex_cmp(PG_FUNCTION_ARGS)
{
	Complex	*num1 = PG_GETARG_COMPLEX_P(0);
	Complex	*num2 = PG_GETARG_COMPLEX_P(1);
	int			result;

	result = complex_abs_cmp_internal(num1, num2);

	PG_FREE_IF_COPY(num1, 0);
	PG_FREE_IF_COPY(num2, 1);

	PG_RETURN_INT32(result);
}

/*
 *		complex_pl				- returns arg1 + arg2
 */
Datum
complex_pl(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, re(a) + re(b), im(a) + im(b));

	CHECKFLOATVAL(re(c), isinf(re(a)) || isinf(re(b)), true);
	CHECKFLOATVAL(im(c), isinf(im(a)) || isinf(im(b)), true);
	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_mi				- returns arg1 - arg2
 */
Datum
complex_mi(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);

	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, re(a) - re(b), im(a) - im(b));

	CHECKFLOATVAL(re(c), isinf(re(a)) || isinf(re(b)), true);
	CHECKFLOATVAL(im(c), isinf(im(a)) || isinf(im(b)), true);
	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_mul				- returns arg1 * arg2
 */
Datum
complex_mul(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	c->x = re(a) * re(b) - im(a) * im(b);
	c->y = re(a) * im(b) + im(a) * re(b);

	CHECKFLOATVAL(re(c),
				isinf(re(a)) || isinf(re(b)) || isinf(im(a)) || isinf(im(b)),
				  true);
	CHECKFLOATVAL(im(c),
				isinf(re(a)) || isinf(re(b)) || isinf(im(a)) || isinf(im(b)),
				  true);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_div				- returns arg1/arg2
 */
Datum
complex_div(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);
	Complex    *c = (Complex *) palloc(sizeof(Complex));
	double		div = re(b) * re(b) + im(b) * im(b);

	c->x = (re(a) * re(b) + im(a) * im(b)) / div;
	c->y = (im(a) * re(b) - re(a) * im(b)) / div;

	CHECKFLOATVAL(re(c),
				  isinf(re(a)) || isinf(re(b)) || isinf(im(a)) || isinf(im(b))
				  || (0 == re(b) && 0 == im(b)),
				  true);
	CHECKFLOATVAL(im(c),
				  isinf(re(a)) || isinf(re(b)) || isinf(im(a)) || isinf(im(b))
				  || (0 == re(b) && 0 == im(b)),
				  true);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_up				- returns +arg1
 */
Datum
complex_up(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	*c = *a;

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_um				- returns -arg1
 */
Datum
complex_um(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, -re(a), -im(a));

	PG_RETURN_COMPLEX_P(c);
}

/* return x^k */
static Complex
pg_cpow_n(Complex x, int k)
{
	Complex		res;

	if (k == 0)
	{
		INIT_COMPLEX(&res, 1, 0);
		return res;
	}
	else if (k == 1)
		return x;
	else if (k < 0)
	{
		Complex		one = {1, 0};
		Complex		pow_mi_n = pg_cpow_n(x, -k);

		return *DatumGetComplexP(DirectFunctionCall2(complex_div,
					   ComplexPGetDatum(&one), ComplexPGetDatum(&pow_mi_n)));
	}
	else
	{							/* k > 0 */
		Complex		z = {1, 0};
		double		re_x = 0;
		double		re_z = 0;

		/*
		 *	Euclidean method to calculate power.
		 *	Target: calculating r = a^c, where c is an integer.
		 *	
		 *	Initial: z = 1, x = a, k = c, r = z*x^k
		 *	Loop invariant: r = z*x^k
		 *			 x is the base
		 *			 k is the power
		 *			 z is the remaining which makes the loop invariant valid
		 *	End condition: k == 0, r = z*x^0, so r = z
		 *
		 *	while k > 1:
		 *		if k is odd:
					/// z*x^k = (z*x)*x^(k-1), the following two steps keep loop invariant
		 *			k = k - 1		/// make the power k even, then r = z*x*x^k
		 *			z = z * x		/// to keep loop invariant, combining z and x
		 *		if k == 0:			/// this step don't make any changing, so the loop invariant was kept
		 *			break
		 *		/// now k is even and bigger than 0, so x^k = (x*x)^(k/2),
		 *		/// the following two steps keep loop invariant
		 *		k = k/2
		 *		x = x*x
		 *	endwhile
		 *	r = z
		 */	
		while (k > 0)
		{
			re_x = re(&x);
			re_z = re(&z);
			if (k & 1)
			{
				/* Let z = z*x; */
				{
					z.x = re_z * re_x - im(&z) * im(&x);
					z.y = re_z * im(&x) + im(&z) * re_x;
				}
			}
			if (k == 1)
				break;
			k >>= 1;			/* efficient division by 2; now have k >= 1 */

			/* Let x = x*x; */
			{
				x.x = re_x * re_x - im(&x) * im(&x);
				x.y = re_x * im(&x) + im(&x) * re_x;
			}
		}

		/*
		 * According to pg_cpow, x should not be zero, but the result can be
		 * zero. Let x = r(cos(a) + sin(a)*i) where a is the phase of x, then
		 * x^k equals (r^k)(cos(k*a) + sin(k*a)*i). If k*a = 2*m*pi + pi,
		 * or k*a = 2*m*pi + pi, or k*a = 2*m*pi + pi/2, or k*a = 2*m*pi + 3*pi/2,
		 * we will get a result with 0 real part or with 0 imaginary part.
		 */
		CHECKFLOATVAL(re(&z), isinf(re(&x)) || isinf(im(&x)), true);
		CHECKFLOATVAL(im(&z), isinf(re(&x)) || isinf(im(&x)), true);
		return z;
	}
}

/* reason for this:
 * 1) x^n  (e.g. for n = +/- 2, 3) is unnecessarily inaccurate in glibc;
 *	  cut-off 65536 : guided from empirical speed measurements
 *
 * 2) On Mingw (but not Mingw-w64) the system cpow is explicitly linked
 *	  against the (slow) MSVCRT pow, and gets (0+0i)^y as 0+0i for all y.
 *
 * 3) PPC Mac OS x crashes on powers of 0+0i (at least under Rosetta).
 *	  Really 0i^-1 should by Inf+NaNi, but getting that portably seems too hard.
 *	  (C1x's CMPLX will eventually be possible.)
 */

static Complex
pg_cpow(Complex x, Complex y)
{
	Complex		z;
	double		yr = re(&y),
				yi = im(&y);
	int			k;

	if (re(&x) == 0.0 && im(&x) == 0.0)
	{
		if (yi == 0.0)
		{
			INIT_COMPLEX(&z, dp_pow(0.0, yr), 0);
		}
		else
		{
			INIT_COMPLEX(&z, get_float8_nan(), get_float8_nan());
		}
	}
	else if ((yi == 0.0) && (yr == (k = (int) yr)) && (abs(k) <= 65536))
	{
		z = pg_cpow_n(x, k);
	}
	else
	{
		/* We must check overflow each step, so we can't use cpow directly */
		double		rho,
					r,
					i,
					theta,
					a,
					b;

		/* Let r = hypot(re(&x), im(&x)); */
		r = DatumGetFloat8(DirectFunctionCall1(complex_mag, ComplexPGetDatum(&x)));
		/* Let i = atan2(im(&x), re(&x)); */
		i = DatumGetFloat8(DirectFunctionCall1(complex_arg, ComplexPGetDatum(&x)));
		/* Let theta = i * yr; */
		theta = dp_mul(i, yr);
		if (yi == 0.0)
		{
			/* Let rho = pow(r, yr); */
			rho = dp_pow(r, yr);
		}
		else
		{
			/* rearrangement of cexp(x * clog(y)) */
			/* Let r = log(r); */
			r = dp_log1(r);
			/* Let a = r * yi; */
			a = dp_mul(r, yi);
			/* Let theta += a; */
			theta = dp_pl(a, theta);

			/* Let a = r * yr; */
			a = dp_mul(r, yr);
			/* Let b = i * yi; */
			b = dp_mul(i, yi);
			/* Let a = a - b; */
			a = dp_mi(a, b);
			/* Let rho = exp(a); i.e. exp(r*yr - i*yi) */
			rho = dp_exp(a);
		}
		INIT_COMPLEX(&z, dp_mul(rho, dp_cos(theta)), dp_mul(rho, dp_sin(theta)));
	}
	return z;
}

/*
 *		complex_pow				- return arg1^arg2
 */
Datum
complex_pow(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *b = PG_GETARG_COMPLEX_P(1);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	/* We must check overflow each step, so we can't use cpow directly */
	*c = pg_cpow(*a, *b);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_sqrt			- returns sqrt(arg1)
 */
Datum
complex_sqrt(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex		b = {0.5, 0};
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	*c = pg_cpow(*a, b);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_cbrt			- returns cbrt(arg1)
 */
Datum
complex_cbrt(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex		b = {1.0 / 3, 0};
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	*c = pg_cpow(*a, b);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_degrees			- returns degrees(arg(arg1))
 */
Datum
complex_degrees(PG_FUNCTION_ARGS)
{
	float8		arg_a = 0;
	float8		result = 0;

	arg_a = DatumGetFloat8(DirectFunctionCall1(complex_arg, PG_GETARG_DATUM(0)));
	result = arg_a * (180.0 / M_PI);

	CHECKFLOATVAL(result, isinf(arg_a), arg_a == 0);
	PG_RETURN_FLOAT8(result);
}

/*
 *		complex_exp				- returns exp(arg1)
 */
Datum
complex_exp(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));
	double		exp_ar = 0;
	double		ai = im(a);
	double		cr,
				ci;

	exp_ar = dp_exp(re(a));

	cr = dp_mul(exp_ar, dp_cos(ai));
	ci = dp_mul(exp_ar, dp_sin(ai));
	INIT_COMPLEX(c, cr, ci);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_ln				- returns ln(arg1)
 */
Datum
complex_ln(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));
	double		mag,
				ci,
				cr;

	if (0 == re(a) && 0 == im(a))
	{
		cr = -get_float8_infinity();
		ci = 0;
	}
	else
	{
		mag = DatumGetFloat8(DirectFunctionCall1(complex_mag, PG_GETARG_DATUM(0)));
		ci = DatumGetFloat8(DirectFunctionCall1(complex_arg, PG_GETARG_DATUM(0)));
		cr = dp_log1(mag);
	}
	INIT_COMPLEX(c, cr, ci);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_log10			- returns log10(arg1)
 */
Datum
complex_log10(PG_FUNCTION_ARGS)
{
	Complex    *c = NULL;
	Complex    *ln_a = NULL;
	Complex		ln_10 = {log(10), 0};

	ln_a = DatumGetComplexP(DirectFunctionCall1(complex_ln, PG_GETARG_DATUM(0)));
	c = DatumGetComplexP(DirectFunctionCall2(complex_div, ComplexPGetDatum(ln_a),
											 ComplexPGetDatum(&ln_10)));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_log				- returns log(arg1, arg2), where arg1 is the base
 */
Datum
complex_log(PG_FUNCTION_ARGS)
{
	Complex    *c,
			   *ln_a,
			   *ln_b;

	ln_a = DatumGetComplexP(DirectFunctionCall1(complex_ln, PG_GETARG_DATUM(0)));
	ln_b = DatumGetComplexP(DirectFunctionCall1(complex_ln, PG_GETARG_DATUM(1)));
	c = DatumGetComplexP(DirectFunctionCall2(complex_div, ComplexPGetDatum(ln_b),
											 ComplexPGetDatum(ln_a)));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_acos			- returns acos(arg1)
 */
Datum
complex_acos(PG_FUNCTION_ARGS)
{
	Complex    *c = NULL;
	Complex    *asin_a = NULL;
	Complex		C_M_PI_2 = {M_PI_2, 0};

	asin_a = DatumGetComplexP(DirectFunctionCall1(complex_asin, PG_GETARG_DATUM(0)));
	c = DatumGetComplexP(DirectFunctionCall2(complex_mi, ComplexPGetDatum(&C_M_PI_2),
											 ComplexPGetDatum(asin_a)));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_asin			- returns asin(arg1)
 */
Datum
complex_asin(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));
	double		ar = re(a);
	double		ai = im(a);
	double		cr,
				ci,
				alpha,
				t1,
				t2,
				tmp;

	/* Let t1 = 0.5*hypot(ar+1, ai); */
	tmp = dp_pl(ar, 1);
	t1 = hypot(tmp, ai);
	CHECKFLOATVAL(t1, isinf(tmp) || isinf(ai), 0 == tmp && 0 == ai);
	t1 = dp_mul(t1, 0.5);

	/* Let t2 = 0.5*hypot(ar-1, ai) */
	tmp = dp_mi(ar, 1);
	t2 = hypot(tmp, ai);
	CHECKFLOATVAL(t2, isinf(tmp) || isinf(ai), 0 == tmp && 0 == ai);
	t2 = dp_mul(t2, 0.5);

	/* Let alpha = t1 + t2; */
	alpha = dp_pl(t1, t2);

	/* Let tmp = alpha + sqrt(alpha*alpha -1); */
	tmp = dp_mul(alpha, alpha);
	tmp = dp_mi(tmp, 1);
	tmp = dp_pow(tmp, 0.5);
	tmp = dp_pl(tmp, alpha);

	/* Let ci = log(tmp); */
	ci = dp_log1(tmp);

	/*
	 * This comes from 'z_asin() is continuous from below if x >= 1 and
	 * continuous from above if x <= -1.'
	 */
	if (ai < 0 || (ai == 0 && ar > 1))
		ci *= -1;

	/* Let cr = asin(t1 - t2); */
	tmp = dp_mi(t1, t2);
	cr = dp_asin(tmp);

	INIT_COMPLEX(c, cr, ci);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_atan			- returns atan(arg1)
 */
Datum
complex_atan(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));
	double		ar,
				square_ar,
				ai,
				square_ai,
				cr,
				ci,
				tmp,
				t1,
				t2;

	ar = re(a);
	ai = im(a);
	square_ar = dp_pow(ar, 2);
	square_ai = dp_pow(ai, 2);

	/* Let cr = 0.5*atan2(2*ar, (1 - ar*ar - ai*ai)); */
	t1 = dp_pl(square_ai, square_ar);
	tmp = dp_mi(1, t1);
	cr = dp_mul(2, ar);
	cr = dp_atan2(cr, tmp);
	cr = dp_mul(cr, 0.5);

	/* Let ci = 0.25*log((ar*ar + ai*ai + 2*ai + 1)/(ar*ar + ai*ai - 2*ai + 1)); */
	/* Let t1 = ar*ar + ai*ai + 1; */
	t1 = dp_pl(t1, 1);
	/* Let tmp = 2*ai; */
	tmp = dp_mul(ai, 2);
	/* Let t2 =  (ar*ar + ai*ai - 2*ai + 1); */
	t2 = dp_mi(t1, tmp);
	/* Let t1 =  (ar*ar + ai*ai + 2*ai + 1); */
	t1 = dp_pl(t1, tmp);
	/* Let t2 = t1/t2; */
	ci = dp_div(t1, t2);
	ci = dp_log1(ci);
	ci = dp_mul(ci, 0.25);

	INIT_COMPLEX(c, cr, ci);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_cos				- returns cos(arg1)
 */
Datum
complex_cos(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, dp_mul(dp_cos(re(a)), dp_cosh(im(a))),
				 -dp_mul(dp_sin(re(a)), dp_sinh(im(a))));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_cot				- returns cot(arg1)
 */
Datum
complex_cot(PG_FUNCTION_ARGS)
{
	Complex    *c = NULL;
	Complex		one = {1, 0};
	Complex    *tan_a = NULL;

	tan_a = DatumGetComplexP(DirectFunctionCall1(complex_tan, PG_GETARG_DATUM(0)));
	c = DatumGetComplexP(DirectFunctionCall2(complex_div, ComplexPGetDatum(&one),
											 ComplexPGetDatum(tan_a)));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_sin				- returns sin(arg1)
 */
Datum
complex_sin(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	INIT_COMPLEX(c, dp_mul(dp_sin(re(a)), dp_cosh(im(a))),
				 dp_mul(dp_cos(re(a)), dp_sinh(im(a))));

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		complex_tan				- returns tan(arg1)
 */
Datum
complex_tan(PG_FUNCTION_ARGS)
{
	Complex    *a = PG_GETARG_COMPLEX_P(0);
	Complex    *c = (Complex *) palloc(sizeof(Complex));

	double		x2,
				y2,
				den,
				ci;

	x2 = dp_mul(2.0, re(a));
	y2 = dp_mul(2.0, im(a));
	den = dp_pl(dp_cos(x2), dp_cosh(y2));
	/* any threshold between -log(DBL_EPSILON) and log(DBL_XMAX) will do */
	if (isnan(y2) || fabs(y2) < 50.0)
		ci = dp_div(dp_sinh(y2), den);
	else
		ci = (y2 < 0 ? -1.0 : 1.0);

	INIT_COMPLEX(c, dp_div(dp_sin(x2), den), ci);

	PG_RETURN_COMPLEX_P(c);
}

/*
 *		float2complex			- returns complex(arg1, 0)
 *
 *		Note: expression -4::COMPLEX will get -4 - 0i, not -4 + 0i,
 *		because operator :: has higher precedence than unary minus.
 *		So sqrt(-4::complex) will get 0 - 2i, and sqrt(complex(-4,0))
 *		will get 0 + 2i.
 */
Datum
float82complex(PG_FUNCTION_ARGS)
{
	float8		a = PG_GETARG_FLOAT8(0);

	PG_RETURN_COMPLEX_P(construct_complex_in(a, 0));
}

Datum
float42complex(PG_FUNCTION_ARGS)
{
	float4		a = PG_GETARG_FLOAT4(0);

	PG_RETURN_COMPLEX_P(construct_complex_in(a, 0));
}

/*
 *		float2complex			- returns complex(arg1, 0)
 */
Datum
int82complex(PG_FUNCTION_ARGS)
{
	int8		a = PG_GETARG_INT64(0);

	PG_RETURN_COMPLEX_P(construct_complex_in(a, 0));
}

Datum
int42complex(PG_FUNCTION_ARGS)
{
	int32		a = PG_GETARG_INT32(0);

	PG_RETURN_COMPLEX_P(construct_complex_in(a, 0));
}

Datum
int22complex(PG_FUNCTION_ARGS)
{
	int32		a = PG_GETARG_INT16(0);

	PG_RETURN_COMPLEX_P(construct_complex_in(a, 0));
}

Datum
numeric2complex(PG_FUNCTION_ARGS)
{
	Numeric		num = PG_GETARG_NUMERIC(0);
	double		float_num;

	if (numeric_is_nan(num))
	{
		float_num = get_float8_nan();
	}
	else
	{
		float_num = DatumGetFloat8(DirectFunctionCall1(numeric_float8, NumericGetDatum(num)));
	}

	PG_RETURN_COMPLEX_P(construct_complex_in(float_num, 0));
}

/*
 *		complex_dot_product		- returns dot product of two 1-dim array with the same size
 */
Datum
complex_dot_product(PG_FUNCTION_ARGS)
{
	ArrayType  *la = NULL;
	bool		lnull = false;
	ArrayType  *ra = NULL;
	bool		rnull = false;
	Complex    *num = NULL;
	bool		inf_valid = false;

	int			i = 0;

	num = (Complex *) palloc(sizeof(Complex));
	la = PG_GETARG_ARRAYTYPE_P(0);
	ra = PG_GETARG_ARRAYTYPE_P(1);

	if (1 != ARR_NDIM(la) || 1 != ARR_NDIM(ra))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot dot product two multi-dimentsion arrays"),
				 errdetail("The dimension of the left argument is %d, the dimension of the right argument is %d.",
						   ARR_NDIM(la), ARR_NDIM(ra))));
	}

	if (ARR_DIMS(la)[0] != ARR_DIMS(ra)[0])
	{
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot dot product two 1-dim arrays with different number of elements"),
			  errdetail("The number of elements of the left argument is %d, "
						"the number of elements of the right argument is %d.",
						ARR_DIMS(la)[0],
						ARR_DIMS(ra)[0])));
	}


	INIT_COMPLEX(num, 0, 0);
	for (i = 1; i <= ARR_DIMS(la)[0]; i++)
	{
		int			indx = i;
		Complex    *lc = DatumGetComplexP(array_ref(la, 1, &indx, -1,
									   sizeof(Complex), false, 'd', &lnull));
		Complex    *rc = DatumGetComplexP(array_ref(ra, 1, &indx, -1,
									   sizeof(Complex), false, 'd', &rnull));

		if (lnull || rnull)
		{
			break;
		}
		else
		{
			double		mul_re = re(lc) * re(rc) - im(lc) * im(rc);
			double		mul_im = re(lc) * im(rc) + im(lc) * re(rc);

			inf_valid = (inf_valid
						 || (isinf(re(lc)) || isinf(re(rc)) || isinf(im(lc)) || isinf(im(rc))));

			num->x += mul_re;
			num->y += mul_im;
		}
	}

	if (lnull || rnull)
	{
		PG_RETURN_NULL();
	}
	else
	{
		CHECKFLOATVAL(re(num), inf_valid, true);
		CHECKFLOATVAL(im(num), inf_valid, true);
		PG_RETURN_COMPLEX_P(num);
	}
}
