/*-------------------------------------------------------------------------
 *
 * ext_alloc.h
 *	  This file contains declarations for external memory management
 *	  functions.
 *
 * Portions Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXT_ALLOC_H
#define EXT_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

extern uint64 OptimizerOustandingMemoryBalance;

extern void
Ext_OptimizerFree(void *ptr);

extern void*
Ext_OptimizerAlloc(size_t size);

extern uint64
GetOptimizerOutstandingMemoryBalance(void);


#ifdef __cplusplus
}
#endif

#endif
