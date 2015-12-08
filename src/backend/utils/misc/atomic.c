/*
 * atomic.c
 *    routines for atomic operations.
 *
 * Copyright (c) 2011 - present, EMC DCD (Greenplum)
 */

#include "postgres.h"
#include "utils/atomic.h"

#if defined(_AIX)
#include <sys/atomic_op.h>
#elif defined(__sparc__)
#include <atomic.h>
#endif

/*
 * compare_and_swap_32
 *   Atomic compare and swap operation in a 32-bit address.
 *
 * Algorithm:
 * 		if (*dest == old)
 * 		{
 * 			*dest = new;
 * 			return 1;
 * 		}
 * 		return 0;
 *
 */
int32 compare_and_swap_32(uint32 *dest, uint32 old, uint32 new)
{
	if (old == new)
		return true;

	volatile int32 res = 0;

#if defined(__i386)
	__asm__ __volatile__
		(
		"MOVL 0x8(%%ebp), %%edx 						\n\t"
		"MOVL 0xc(%%ebp), %%eax			 				\n\t"
		"MOVL 0x10(%%ebp), %%ecx 						\n\t"
		"LOCK CMPXCHG %%ecx, (%%edx)			\n\t"
		"SETE %0								\n\t"
		:"=g"(res)
		:
		:"%eax","%ecx","%edx"
		);

#elif defined(__x86_64__)
	res = __sync_bool_compare_and_swap(dest, old, new);

#elif defined(__sparc__)
	uint32 returnValue = atomic_cas_32(dest, old, new);
	res = !(returnValue == *dest);

#else
#error unsupported platform: requires atomic compare_and_swap_32 operation
#endif

	return res;
}

/*
 * compare_and_swap_64
 *   Atomic compare and swap operation in a 64-bit address.
 *
 * The algorithm is similar to compare_and_swap_32.
 */
int32 compare_and_swap_64(uint64 *dest, uint64 old, uint64 new)
{
	if (old == new)
		return true;

	volatile int32 res = 0;
	
#if defined(__x86_64__)
	res = __sync_bool_compare_and_swap(dest, old, new);

#elif defined(__sparc__)
	uint64 returnValue = atomic_cas_64(dest, old, new);
	res = !(returnValue == *dest);

#elif defined(_AIX)
	res = compare_and_swap(dest, &old, new);

#elif !defined(__i386)
#error unsupported platform: requires atomic compare_and_swap_64 operation
#endif

	/* All others, return 0 */
	return res;
}

/*
 * compare_and_swap_ulong
 *   a generic cas operation for an unsigned long value in both
 *   32-bit and 64-bit platform.
 */
int32 compare_and_swap_ulong(unsigned long *dest,
							 unsigned long oldval,
							 unsigned long newval)
{
	int32 casResult = false;

#if defined(__i386)
	Assert(sizeof(unsigned long) == sizeof(uint32));
	casResult = compare_and_swap_32((uint32*)dest,
									(uint32)oldval,
									(uint32)newval);
#else
	if (sizeof(unsigned long) == sizeof(uint64))
	{
		casResult = compare_and_swap_64((uint64*)dest,
										(uint64)oldval,
										(uint64)newval);
	}
	
	else
	{
		Assert(sizeof(unsigned long) == sizeof(uint32));
		casResult = compare_and_swap_32((uint32*)dest,
										(uint32)oldval,
										(uint32)newval);
	}
#endif

	return casResult;
}
	
/* 
 * gp_atomic_add_32
 *    Atomic increment in a 32-bit address, and return the incremented value.
 */
int32 gp_atomic_add_32(volatile int32 *ptr, int32 inc)
{
	volatile int32 newValue = 0;
	
#if defined(__x86_64__)
	int32 oldValue = __sync_fetch_and_add(ptr, inc);
	newValue = oldValue + inc;

#elif defined(__i386)
	__asm__ __volatile__ (
		"lock; xaddl %0, %1"
		: "=r" (newValue), "=m" (*ptr)
		: "0" (inc), "m" (*ptr)
		: "memory");

	newValue += inc;

#elif defined(__sparc__)
	newValue = atomic_add_32_nv((volatile uint32_t *)ptr, inc);

#else
#error unsupported platform: requires atomic_add_32 operation
#endif

	return newValue;
}

/*
 * gp_atomic_add_int64
 *   Atomic increment a 64-bit address, and return the incremented value
 *   inc can be a positive or negative quantity
 */
int64
gp_atomic_add_int64(int64 *ptr, int64 inc)
{
	Assert(NULL != ptr);

	volatile int64 newValue = 0;

#if defined(__x86_64__)
	int64 oldValue = __sync_fetch_and_add(ptr, inc);
	newValue = oldValue + inc;

#elif defined(__i386)

	volatile int64* newValuePtr = &newValue;
	int addValueLow = (int) inc;
	int addValueHigh = (int) (inc>>32);
	__asm__ __volatile__ (
		  "PUSHL %%ebx; \n\t" /* Save ebx, it's a special register we can't clobber */

	      "MOVL %0, %%edi; \n\t" /* Load ptr */
	      "MOVL (%%edi), %%eax; \n\t" /* Load first word from *ptr */
	      "MOVL 0x4(%%edi), %%edx; \n\t" /* Load second word from *ptr + 4 */
	   "tryAgain_add_64: \n\t"
	      "MOVL %1, %%ebx; \n\t"   /* Load addValueLow */
	      "MOVL %2, %%ecx; \n\t"   /* Load addValueHigh */
	      "ADDL %%eax, %%ebx; \n\t"  /* Add first word */
	      "ADCL %%edx, %%ecx; \n\t"  /* Add second word */
	      "lock cmpxchg8b (%%edi); \n\t"  /* Compare and exchange 8 bytes atomically */
	      "jnz tryAgain_add_64; \n\t"  /* If ptr has changed, try again with new value */

		  "MOVL %3, %%edi; \n\t"  /* Put result in *newValuePtr */
		  "MOVL %%ebx, (%%edi); \n\t"  /* first word */
		  "MOVL %%ecx, 0x4(%%edi); \n\t" /* second word */
		  "POPL %%ebx; \n\t" /* Restore ebx */

		: /* no output registers */
		: "m" (ptr), "m" (addValueLow), "m" (addValueHigh), "m" (newValuePtr) /* input registers */
		: "memory", "%edi", "%edx", "%ecx", "%eax" /* clobber list */
	);

#elif defined(__sparc__)
#error unsupported platform sparc: requires atomic_add_64 operation
	/* For sparc we can probably use __sync_fetch_and_add as well */
#else
#error unsupported/unknown platform: requires atomic_add_64 operation
#endif

	return newValue;
}

/*
 * gp_atomic_add_uint64
 * 		Atomic increment an unsigned 64-bit address, and return the incremented value
 * 		inc can be a positive or negative quantity
 * 		ptr : uint64 pointer that we need to inc atomically
 * 		inc : values that needs to be added
 */

uint64
gp_atomic_add_uint64(uint64 *ptr, int64 inc)
{
	Assert(inc >= 0);
	Assert(NULL != ptr);
	
	volatile uint64 newValue = 0;
	uint64 uInc = (uint64) inc;

#if defined(__x86_64__)
	uint64 oldValue = __sync_fetch_and_add(ptr, uInc);
	newValue = oldValue + uInc;
#elif defined(__i386)

	volatile uint64* newValuePtr = &newValue;
	int addValueLow = (int) uInc;
	int addValueHigh = (int) (uInc>>32);

	__asm__ __volatile__ (
		  "PUSHL %%ebx; \n\t" /* Save ebx, it's a special register we can't clobber */
		  "MOVL %0, %%edi; \n\t" /* Load ptr */
		  "MOVL (%%edi), %%eax; \n\t" /* Load first word from *ptr */
          "MOVL 0x4(%%edi), %%edx; \n\t" /* Load second word from *ptr + 4 */
	   "tryAgain_add_uint64: \n\t"
          "MOVL %1, %%ebx; \n\t"   /* Load addValueLow */
          "MOVL %2, %%ecx; \n\t"   /* Load addValueHigh */
          "ADDL %%eax, %%ebx; \n\t"  /* Add first word */
          "ADCL %%edx, %%ecx; \n\t"  /* Add second word */
          "lock cmpxchg8b (%%edi); \n\t"  /* Compare and exchange 8 bytes atomically */
          "jnz tryAgain_add_uint64; \n\t"  /* If ptr has changed, try again with new value */

          "MOVL %3, %%edi; \n\t"  /* Put result in *newValuePtr */
          "MOVL %%ebx, (%%edi); \n\t"  /* first word */
          "MOVL %%ecx, 0x4(%%edi); \n\t" /* second word */
          "POPL %%ebx; \n\t" /* Restore ebx */

		: /* no output registers */
		: "m" (ptr), "m" (addValueLow), "m" (addValueHigh), "m" (newValuePtr) /* input registers */
        : "memory", "%edi", "%edx", "%ecx", "%eax" /* clobber list */
    );

#elif defined(__sparc__)
#error unsupported platform sparc: requires atomic_add_64 operation
        /* For sparc we can probably use __sync_fetch_and_add as well */
#else
#error unsupported/unknown platform: requires atomic_add_64 operation
#endif

    return newValue;
}

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
	int32 oldval = gp_atomic_add_32(loc, 1);
	if (oldval >= mod)
	{
		/* we have overflow */
		if (oldval == mod)
		{
			/* exactly at overflow, reduce by one cycle */
			gp_atomic_add_32(loc, -mod);
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
		bool casResult = compare_and_swap_32((uint32 *)loc, oldVal, newVal);
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
		bool casResult = compare_and_swap_32((uint32 *)loc, oldVal, newVal);
		if (true == casResult)
		{
			break;
		}
	}
	return newVal;
}
