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

typedef struct TypInfo
{
	Oid typOid;
	bool typbyval;
	int16 typlen;
	Oid ltFuncOp; /* oid of 'less than' operator function id of this type */
	Oid eqFuncOp; /* oid of equality operator function id of this type */
	FmgrInfo hashfunc;		/* hash function */
} TypInfo;

/* Functions and structures used for aggregating leaf partition stats */
typedef struct MCVFreqPair
{
	Datum mcv;
	float4 count;
	TypInfo *typinfo; /* type information of datum type */
} MCVFreqPair;

/* extern functions called by commands/analyze.c */
extern MCVFreqPair **aggregate_leaf_partition_MCVs(Oid relationOid,
												   AttrNumber attnum,
												   int numPartitions,
												   HeapTuple *heaptupleStats,
												   float4 *relTuples,
												   unsigned int nEntries,
												   double ndistinct,
												   int *num_mcv,
												   int *rem_mcv,
												   void **result);
extern bool datumCompare(Datum d1, Datum d2, Oid opFuncOid);
extern float4 get_rel_reltuples(Oid relid);
extern int32 get_rel_relpages(Oid relid);
extern int aggregate_leaf_partition_histograms(Oid relationOid,
											   AttrNumber attnum,
											   int nParts,
											   HeapTuple *heaptupleStats,
											   float4 *relTuples,
											   unsigned int nEntries,
											   MCVFreqPair **mcvpairArray,
											   int rem_mcv,
											   void **result);
extern bool needs_sample(VacAttrStats **vacattrstats, int attr_cnt);
extern AttrNumber fetch_leaf_attnum(Oid leafRelid, const char* attname);
extern HeapTuple fetch_leaf_att_stats(Oid leafRelid, AttrNumber leafAttNum);
extern bool leaf_parts_analyzed(Oid attrelid, Oid relid_exclude, List *va_cols, int elevel);

#endif  /* ANALYZEUTILS_H */
