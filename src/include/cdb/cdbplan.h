/*-------------------------------------------------------------------------
 *
 * cdbplan.h
 *	  definitions for cdbplan.c utilities
 *
 * Copyright (c) 2004-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBPLAN_H
#define CDBPLAN_H

#include "optimizer/walkers.h"

extern Node * plan_tree_mutator(Node *node, Node *(*mutator) (), void *context);

#endif   /* CDBPLAN_H */
