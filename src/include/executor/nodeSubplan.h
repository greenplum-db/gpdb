/*-------------------------------------------------------------------------
 *
 * nodeSubplan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeSubplan.h,v 1.27 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESUBPLAN_H
#define NODESUBPLAN_H

#include "nodes/execnodes.h"
#include "executor/execdesc.h"

extern SubPlanState *ExecInitSubPlan(SubPlan *subplan, PlanState *parent);
extern Datum ExecSubPlan(SubPlanState *node,
			ExprContext *econtext,
			bool *isNull,
			ExprDoneCond *isDone);

extern void ExecReScanSetParamPlan(SubPlanState *node, PlanState *parent);
extern void ExecEagerFreeSubPlan(SubPlanState *node);

/*
 * MPP Change: Added a parameter (ParamListInfo p) to this function.  See comments in nodeSubplan.cpp
 */
extern void ExecSetParamPlan(SubPlanState *node, ExprContext *econtext, QueryDesc *gbl_queryDesc);

/* Gpmon: It seems does not make any sense to gpmon this node. */
#endif   /* NODESUBPLAN_H */
