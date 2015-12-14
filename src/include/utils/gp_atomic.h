/*
 * gp_atomic.h
 *    Header file for atomic operations.
 *
 * Copyright (c) 2011 - present, EMC DCD (Greenplum).
 */
#ifndef GP_ATOMIC_H
#define GP_ATOMIC_H

#include "port/atomics.h"

extern int32 gp_atomic_incmod_32(volatile int32 *loc, int32 mod);
extern uint32 gp_atomic_dec_positive_32(volatile uint32 *loc, uint32 dec);
extern uint32 gp_atomic_inc_ceiling_32(volatile uint32 *loc, uint32 inc, uint32 ceil);

#endif   /* GP_ATOMIC_H */
