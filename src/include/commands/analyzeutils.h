/*-------------------------------------------------------------------------
 *
 * analyzeutils.h
 *
 *	  Header file for utils functions in analyzeutils.c
 *
 * Copyright (c) 2015, Pivotal Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef ANALYZEUTILS_H
#define ANALYZEUTILS_H

#include "commands/vacuum.h"

/* extern functions called by commands/analyze.c */
extern int aggregate_leaf_partition_MCVs(Oid relationOid,
										 AttrNumber attnum,
										 HeapTuple *heaptupleStats,
										 float4 *relTuples,
										 unsigned int nEntries,
										 void **result);
extern bool datumCompare(Datum d1, Datum d2, Oid opFuncOid);
extern float4 get_rel_reltuples(Oid relid);
extern int aggregate_leaf_partition_histograms(Oid relationOid,
											   AttrNumber attnum,
											   HeapTuple *heaptupleStats,
											   float4 *relTuples,
											   unsigned int nEntries,
											   void **result);
extern bool needs_sample(VacAttrStats **vacattrstats, int attr_cnt);
extern bool leaf_parts_analyzed(VacAttrStats *stats);

#endif  /* ANALYZEUTILS_H */
