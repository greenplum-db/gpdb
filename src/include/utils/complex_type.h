/*-------------------------------------------------------------------------
 *
 * complex_type.h - Declarations for complex data type
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2016-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/complex_type.h
 *
 * NOTE
 *	  These routines do *not* use the float types from adt/.
 *
 *	  XXX These routines were not written by a numerical analyst.
 *
 *-------------------------------------------------------------------------
 */
#ifndef COMPLEX_TYPE_H
#define COMPLEX_TYPE_H
#include "fmgr.h"
/*
 * GCC has problems with header files on e.g. Solaris.
 * That OS defines the imaginary type, but GCC does not.
 * Probably needed elsewhere, e.g. AIX.
 * And use on Win32/64 suppresses warnings.
 * The warning is also seen on Mac OS 10.5.
 */
#if defined(__GNUC__) && (defined(__sun__) || defined(Win32))
#undef	I
#define I (__extension__ 1.0iF)
#endif

/*
 * represents the complex number x + yi
 *
 * We don't choose to use double complex defined in c99 because the
 * reasons list below:
 *
 * 1. Functions defined in /usr/include/complex.h is not portable enough.
 * 2. We can't use functions provided in /usr/include/complex.h directly,
 * because we need to check double overflow/underflow during calculation.
 * 3. For most portability, we store a complex number with two double numbers,
 * one for the real part, another one for the imaginary part. And all these
 * functions implemented by ourselves operate directly on the two double numbers,
 * so there is no need to do extra transformation between double complex and
 * internal representation.
 *
 * Note: When related with Infinity and NaN, the behaviours may not conform
 * the standard C99. But postgres' float8 doesn't conform C standard, because
 * we need more strict check. E.x. 5.0/0.0 in C will get Infinity, but in postgres
 * it will get a divided by zero error. Likely, (5.0 + 3.0i)/(0 + 0i) in C will get
 * (Infinity + Infinityi), but in our implementatioin it will get a divided by zero
 * error.
 */
typedef struct
{
	double		x,
				y;
} Complex;

#define INIT_COMPLEX(_c, _re, _im)\
do\
{\
	(_c)->x = (_re);\
	(_c)->y = (_im);\
} while (0)

#define re(z) ((z)->x)
#define im(z) ((z)->y)

/*
 * fmgr interface macros
 *
 * Complex is a fixed-size pass-by-reference type
 */
#define DatumGetComplexP(X)			((Complex*) DatumGetPointer(X))
#define ComplexPGetDatum(X)			PointerGetDatum(X)
#define PG_GETARG_COMPLEX_P(n)		DatumGetComplexP(PG_GETARG_DATUM(n))
#define PG_RETURN_COMPLEX_P(X)		return ComplexPGetDatum(X)

/* public complex routines */
extern Datum complex_in(PG_FUNCTION_ARGS);
extern Datum complex_out(PG_FUNCTION_ARGS);
extern Datum complex_recv(PG_FUNCTION_ARGS);
extern Datum complex_send(PG_FUNCTION_ARGS);

/*
 * constructs a complex number with two real numbers,
 * arg1 as the real part, arg2 as the imaginary part
 */
extern Datum construct_complex(PG_FUNCTION_ARGS);

/*
 * returns arg1*cos(arg2) + arg1*sin(arg2)i
 */
extern Datum construct_complex_trig(PG_FUNCTION_ARGS);

/* returns real part of arg1 */
extern Datum complex_re(PG_FUNCTION_ARGS);

/* returns imaginary part of arg1 */
extern Datum complex_im(PG_FUNCTION_ARGS);

/* returns phase of arg1 */
extern Datum complex_arg(PG_FUNCTION_ARGS);

/* returns magnitude of arg1 */
extern Datum complex_mag(PG_FUNCTION_ARGS);

/* returns conjunction of arg1 */
extern Datum complex_conj(PG_FUNCTION_ARGS);

/* returns hash value of arg1 */
extern Datum complex_hash(PG_FUNCTION_ARGS);

/* checks whether arg1 equals arg2 */
extern Datum complex_eq(PG_FUNCTION_ARGS);

/* checks whether arg1 not equals arg2 */
extern Datum complex_ne(PG_FUNCTION_ARGS);

/* returns arg1 + arg2 */
extern Datum complex_pl(PG_FUNCTION_ARGS);

/* returns +arg1 */
extern Datum complex_up(PG_FUNCTION_ARGS);

/* returns arg1 - arg2 */
extern Datum complex_mi(PG_FUNCTION_ARGS);

/* returns -arg1 */
extern Datum complex_um(PG_FUNCTION_ARGS);

/* returns arg1 * arg2 */
extern Datum complex_mul(PG_FUNCTION_ARGS);

/* returns arg1 / arg2 */
extern Datum complex_div(PG_FUNCTION_ARGS);

/* returns arg1 ^ arg2, where arg2 is not an integer */
extern Datum complex_pow(PG_FUNCTION_ARGS);

/* returns sqrt(arg1) */
extern Datum complex_sqrt(PG_FUNCTION_ARGS);

/* returns cbrt(arg1) */
extern Datum complex_cbrt(PG_FUNCTION_ARGS);

/* return degrees(arg(arg1)) */
extern Datum complex_degrees(PG_FUNCTION_ARGS);

/* returns exp(arg1) */
extern Datum complex_exp(PG_FUNCTION_ARGS);

/* returns ln(arg1) */
extern Datum complex_ln(PG_FUNCTION_ARGS);

/* returns log10(arg1) */
extern Datum complex_log10(PG_FUNCTION_ARGS);

/* returns log(arg1,arg2), arg1 based logarithm */
extern Datum complex_log(PG_FUNCTION_ARGS);

/* returns acos(arg1) */
extern Datum complex_acos(PG_FUNCTION_ARGS);

/* returns asin(arg1) */
extern Datum complex_asin(PG_FUNCTION_ARGS);

/* returns atan(arg1) */
extern Datum complex_atan(PG_FUNCTION_ARGS);

/* returns cos(arg1) */
extern Datum complex_cos(PG_FUNCTION_ARGS);

/* returns cot(arg1) */
extern Datum complex_cot(PG_FUNCTION_ARGS);

/* returns sin(arg1) */
extern Datum complex_sin(PG_FUNCTION_ARGS);

/* returns tan(arg1) */
extern Datum complex_tan(PG_FUNCTION_ARGS);

/* returns dot product of two 1-dim array with the same number of elements */
extern Datum complex_dot_product(PG_FUNCTION_ARGS);

/* returns arg1 + 0i */
extern Datum float82complex(PG_FUNCTION_ARGS);
extern Datum float42complex(PG_FUNCTION_ARGS);
extern Datum int82complex(PG_FUNCTION_ARGS);
extern Datum int42complex(PG_FUNCTION_ARGS);
extern Datum int22complex(PG_FUNCTION_ARGS);
extern Datum numeric2complex(PG_FUNCTION_ARGS);

#endif   /* COMPLEX_TYPE_H */
