/*-------------------------------------------------------------------------
 *
 * complex.h - Declarations for complex data type
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2016, Pivotal Software Inc
=======
 *
 *
 * NOTE
 *	  These routines do *not* use the float types from adt/.
 *
 *	  XXX These routines were not written by a numerical analyst.
 *
 *-------------------------------------------------------------------------
 */
#ifndef FLOAT_UTILS_H 
#define FLOAT_UTILS_H
/*
 * check to see if a float4/8 val has underflowed or overflowed
 */
#define CHECKFLOATVAL(val, inf_is_valid, zero_is_valid)			\
do {															\
	if (isinf(val) && !(inf_is_valid))							\
		ereport(ERROR,											\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),	\
		  errmsg("value out of range: overflow"),errOmitLocation(true)));				\
																\
	if ((val) == 0.0 && !(zero_is_valid))						\
		ereport(ERROR,											\
				(errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),	\
		 errmsg("value out of range: underflow"),errOmitLocation(true)));				\
} while(0)

#endif /* FLOAT_UTILS_H */
