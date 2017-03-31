/*-------------------------------------------------------------------------
 *
 * planpartition.h
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 *-------------------------------------------------------------------------
 */

#ifndef PLANPARTITION_H
#define PLANPARTITION_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern bool inject_partition_selectors_for_join(PlannerInfo *root, JoinPath *join_path, Plan **inner_plan_p);

extern RestrictInfo *make_mergeclause(Node *outer, Node *inner);

#endif /* PLANPARTITION_H */
