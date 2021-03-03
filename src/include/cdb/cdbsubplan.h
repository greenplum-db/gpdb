/*-------------------------------------------------------------------------
 *
 * cdbsubplan.h
 *	  Routines for preprocessing initPlan subplans	and executing queries
 *	  with initPlans
 *
 * Portions Copyright (c) 2003-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbsubplan.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBSUBPLAN_H
#define CDBSUBPLAN_H

#include "executor/execdesc.h"
#include "nodes/params.h"
#include "nodes/plannodes.h"

extern void preprocess_initplans(QueryDesc *queryDesc);
extern void postprocess_initplans(QueryDesc *queryDesc);

#endif   /* CDBSUBPLAN_H */
