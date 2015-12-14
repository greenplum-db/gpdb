/*
 * gp_atomic.c
 *    routines for atomic operations.
 *
 * Copyright (c) 2011 - present, EMC DCD (Greenplum)
 */

#include "postgres.h"
#include "utils/gp_atomic.h"

/*
 * atomic_incmod_value
 *
 * Atomically adds 1 to a value, modulo 'mod'
 *
 * Algorithm:
 *   *loc = (*loc + 1) % mod
 *
 */
int32
gp_atomic_incmod_32(volatile int32 *loc, int32 mod)
{
	Assert(NULL != loc);
	int32 oldval = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)loc, 1);
	if (oldval >= mod)
	{
		/* we have overflow */
		if (oldval == mod)
		{
			/* exactly at overflow, reduce by one cycle */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)loc, mod);
		}
		/* must reduce result */
		oldval %= mod;
	}
	return(oldval);
}


/*
 * gp_atomic_dec_positive_32
 *
 * Atomically decrements a value by a positive amount dec. If result was negative,
 * sets value to 0
 *
 * Returns new decremented value, which is never negative
 */
uint32
gp_atomic_dec_positive_32(volatile uint32 *loc, uint32 dec)
{
	uint32 newVal = 0;
	while (true)
	{
		uint32 oldVal = (uint32) *loc;
		newVal = 0;
		if (oldVal > dec)
		{
			newVal = oldVal - dec;
		}
		bool casResult = pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)loc, &oldVal, newVal);
		if (true == casResult)
		{
			break;
		}
	}
	return newVal;
}

/*
 * gp_atomic_inc_ceiling_32
 *
 * Atomically increments a value by a positive amount inc. If result is over
 * a ceiling ceil, set value to ceil.
 *
 * Returns new incremented value, which is <= ceil.
 */
uint32
gp_atomic_inc_ceiling_32(volatile uint32 *loc, uint32 inc, uint32 ceil)
{
	uint32 newVal = 0;
	while (true)
	{
		uint32 oldVal = (uint32) *loc;
		newVal = oldVal + inc;
		if (newVal > ceil)
		{
			newVal = ceil;
		}
		bool casResult = pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)loc, &oldVal, newVal);
		if (true == casResult)
		{
			break;
		}
	}
	return newVal;
}
