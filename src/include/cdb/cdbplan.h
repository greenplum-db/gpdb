/*-------------------------------------------------------------------------
 *
 * cdbplan.h
 *	  definitions for cdbplan.c utilities
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbplan.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBPLAN_H
#define CDBPLAN_H

#include "optimizer/walkers.h"

extern Node * plan_tree_mutator(Node *node, Node *(*mutator) (), void *context);

#endif   /* CDBPLAN_H */
