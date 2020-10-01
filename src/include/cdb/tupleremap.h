/*-------------------------------------------------------------------------
 *
 * tupleremap.h
 *
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/cdb/tupleremap.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TUPLEREMAP_H
#define TUPLEREMAP_H

#include "tcop/dest.h"

/* Opaque struct, only known inside tupleremap.c. */
typedef struct TupleRemapper TupleRemapper;

extern TupleRemapper *CreateTupleRemapper(void);
extern void DestroyTupleRemapper(TupleRemapper *remapper);
extern MinimalTuple TRCheckAndRemap(TupleRemapper *remapper, TupleDesc tupledesc, MinimalTuple tuple);
extern void TRHandleTypeLists(TupleRemapper *remapper, List *typelist);
extern Datum TRRemapDatum(TupleRemapper *remapper, Oid typeid, Datum value);

#endif   /* TUPLEREMAP_H */
