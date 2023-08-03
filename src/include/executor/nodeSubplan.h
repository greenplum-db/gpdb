/*-------------------------------------------------------------------------
 *
 * nodeSubplan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeSubplan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESUBPLAN_H
#define NODESUBPLAN_H

#include "nodes/execnodes.h"
#include "executor/execdesc.h"

extern SubPlanState *ExecInitSubPlan(SubPlan *subplan, PlanState *parent);

extern Datum ExecSubPlan(SubPlanState *node, ExprContext *econtext, bool *isNull);

extern void ExecReScanSetParamPlan(SubPlanState *node, PlanState *parent);

/*
 * MPP Change: Added a parameter (ParamListInfo p) to this function.  See comments in nodeSubplan.cpp
 */
extern void ExecSetParamPlan(SubPlanState *node, ExprContext *econtext, QueryDesc *gbl_queryDesc);

extern void ExecSetParamPlanMulti(const Bitmapset *params, ExprContext *econtext, QueryDesc *gbl_queryDesc);

extern void PrefetchbuildSubPlanHash(SubPlanState *node);

#endif							/* NODESUBPLAN_H */
