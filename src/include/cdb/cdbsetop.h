/*-------------------------------------------------------------------------
 *
 *  cdbsetop.h
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbsetop.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBSETOP_H
#define CDBSETOP_H

#include "nodes/pg_list.h"
#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"

/*
 * GpSetOpType represents a strategy by which to construct a parallel
 * execution plan for a set operation.
 *
 * PSETOP_PARALLEL_PARTITIONED
 *    The plans input to the Append node are (or are coerced to) partitioned 
 *    loci (hashed, scattered, or single QE).  The result of the Append is
 *    assumed to be scattered and unordered is redistributed (if necessary) 
 *    to suit the particular set operation. 
 *
 * PSETOP_SEQUENTIAL_QD
 *    The plans input to the Append node are (or are coerced to) root loci. 
 *    The result of the Append is, therefore, root and unordered.  The set
 *    operation is performed on the QD as if it were sequential.
 *
 * PSETOP_SEQUENTIAL_QE
 *    The plans input to the Append node are (or are coerced to) single QE
 *    loci.  The result of the Append is, therefore, single QE and assumed
 *    unordered.  The set operation is performed on the QE as if it were 
 *    sequential.
 *
 * PSETOP_SEQUENTIAL_QE
 *    Similar to SEQUENTIAL_QD/QE, but the output must be made available
 *    to the outer query's locus. We don't know the outer query's locus yet,
 *    but we treat it sequential.
 *
 * PSETOP_GENERAL
 *    The plans input to the Append node are all general loci.  The result
 *    of the Append is, therefore general as well.
 */
typedef enum GpSetOpType
{
	PSETOP_NONE = 0,
	PSETOP_PARALLEL_PARTITIONED,
	PSETOP_SEQUENTIAL_QD,
	PSETOP_SEQUENTIAL_QE,
	PSETOP_SEQUENTIAL_OUTERQUERY,
	PSETOP_GENERAL
} GpSetOpType;

extern 
GpSetOpType choose_setop_type(List *pathlist);

extern
void adjust_setop_arguments(PlannerInfo *root, List *pathlist, List *tlist_list, GpSetOpType setop_type);


extern Path *make_motion_hash_all_targets(PlannerInfo *root, Path *subpath, List *tlist);

extern
void mark_append_locus(Path *path, GpSetOpType optype);

#endif   /* CDBSETOP_H */
