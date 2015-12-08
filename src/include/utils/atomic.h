/*
 * atomic.h
 *    Header file for atomic operations.
 *
 * Copyright (c) 2011 - present, EMC DCD (Greenplum).
 */
extern int32 compare_and_swap_32(uint32 *dest, uint32 old, uint32 new);
extern int32 compare_and_swap_64(uint64 *dest, uint64 old, uint64 new);
extern int32 compare_and_swap_ulong(unsigned long *dest,
									unsigned long oldval,
									unsigned long newval);
extern int32 gp_atomic_add_32(volatile int32 *ptr, int32 inc);
extern int64 gp_atomic_add_int64(int64 *ptr, int64 inc);
extern uint64 gp_atomic_add_uint64(uint64 *ptr, int64 inc);

extern int32 gp_atomic_incmod_32(volatile int32 *loc, int32 mod);
extern uint32 gp_atomic_dec_positive_32(volatile uint32 *loc, uint32 dec);
extern uint32 gp_atomic_inc_ceiling_32(volatile uint32 *loc, uint32 inc, uint32 ceil);
