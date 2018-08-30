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

/*
 * For Hyperloglog, we define an error margin of 0.3%. If the number of
 * distinct values estimated by hyperloglog is within an error of 0.3%,
 * we consider everything as distinct.
 */
#define HLL_ERROR_MARGIN  0.03

typedef struct TypInfo
{
	Oid typOid;
	bool typbyval;
	int16 typlen;
	Oid ltFuncOp; /* oid of 'less than' operator function id of this type */
	Oid eqFuncOp; /* oid of equality operator function id of this type */
} TypInfo;

/* Functions and structures used for aggregating leaf partition stats */
typedef struct MCVFreqPair
{
	Datum mcv;
	float4 count;
	TypInfo *typinfo; /* type information of datum type */
} MCVFreqPair;

/*
 * Extra information used by the default analysis routines
 */
typedef struct
{
	Oid			eqopr;			/* '=' operator for datatype, if any */
	Oid			eqfunc;			/* and associated function */
	Oid			ltopr;			/* '<' operator for datatype, if any */
} StdAnalyzeData;

/* extern functions called by commands/analyze.c */
extern MCVFreqPair **aggregate_leaf_partition_MCVs(Oid relationOid,
												   AttrNumber attnum,
												   HeapTuple *heaptupleStats,
												   float4 *relTuples,
												   unsigned int nEntries,
												   double ndistinct,
												   int *num_mcv,
												   int *rem_mcv,
												   void **result);
extern bool datumCompare(Datum d1, Datum d2, Oid opFuncOid);
extern float4 get_rel_reltuples(Oid relid);
extern int4 get_rel_relpages(Oid relid);
extern int aggregate_leaf_partition_histograms(Oid relationOid,
											   AttrNumber attnum,
											   HeapTuple *heaptupleStats,
											   float4 *relTuples,
											   unsigned int nEntries,
											   MCVFreqPair **mcvpairArray,
											   int rem_mcv,
											   void **result);
extern bool needs_sample(VacAttrStats **vacattrstats, int attr_cnt);
extern bool leaf_parts_analyzed(Oid attrelid, Oid relid_exclude, List *va_cols);
extern void examine_attribute_for_hll(Relation onerel, VacAttrStats *stat, int attnum);
extern void merge_leaf_stats(VacAttrStatsP stats,
							 AnalyzeAttrFetchFunc fetchfunc,
							 int samplerows,
							 double totalrows);
#endif  /* ANALYZEUTILS_H */
