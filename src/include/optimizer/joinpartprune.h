/*-------------------------------------------------------------------------
 *
 * joinpartprune.h
 *	  Transforms to produce plans that achieve join partition pruning
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/optimizer/joinpartprune.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef JOINPARTPRUNE_H
#define JOINPARTPRUNE_H

#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"

extern void push_partition_selector_candidate_for_join(PlannerInfo *root,
														JoinPath *join_path);

extern bool pop_and_inject_partition_selectors(PlannerInfo *root,
											   JoinPath *jpath);

extern List *make_partition_join_pruneinfos(struct PlannerInfo *root,
											struct RelOptInfo *parentrel,
											List *subpaths,
											List *partitioned_rels);

extern Plan *create_partition_selector_plan(PlannerInfo *root, PartitionSelectorPath *pspath);

#endif /* JOINPARTPRUNE_H */
