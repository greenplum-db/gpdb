/*-------------------------------------------------------------------------
 *
 * planpartition.h
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Author: Siva
 *-------------------------------------------------------------------------
 */

#ifndef PLANPARTITION_H
#define PLANPARTITION_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

/**
 * Transform a plan to take advantage of dynamic partition elimination, if possible.
 * TODO siva - improve comments.
 */
extern Plan *apply_dyn_partition_transforms(PlannerInfo *root, Plan *plan);

extern RestrictInfo *make_mergeclause(Node *outer, Node *inner);

#endif /* PLANPARTITION_H */
