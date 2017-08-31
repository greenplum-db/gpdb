/*-------------------------------------------------------------------------
 *
 * debugutils.h
 *	  Debugging tools and routines
 * 
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/debugutils.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef _DEBUGUTILS_H_
#define _DEBUGUTILS_H_

#include "executor/tuptable.h"

extern void dotnode(void *, const char*);
extern char *tup2str(TupleTableSlot *slot);
extern void dump_tupdesc(TupleDesc tupdesc, const char *fname);
extern void dump_mt_bind(MemTupleBinding *mt_bind, const char *fname);
#ifdef USE_ASSERT_CHECKING
extern int debug_write(const char *filename, const char *output_string);
#endif
#endif /* _DEBUGUTILS_H_ */


