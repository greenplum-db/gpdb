/*-------------------------------------------------------------------------
 *
 * planpartition.h
 *	  Transforms to produce plans that achieve dynamic partition elimination.
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/optimizer/planpartition.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef PLANPARTITION_H
#define PLANPARTITION_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern bool inject_partition_selectors_for_join(PlannerInfo *root,
									JoinPath *join_path,
									List **partSelectors_p);

extern Plan *create_partition_selector_plan(PlannerInfo *root, PartitionSelectorPath *pspath);

extern RestrictInfo *make_mergeclause(Node *outer, Node *inner);

#endif /* PLANPARTITION_H */
