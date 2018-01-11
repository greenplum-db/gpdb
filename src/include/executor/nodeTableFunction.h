/*-------------------------------------------------------------------------
 *
 * nodeTableFunction.h
 *
 * Portions Copyright (c) 2011 - present, EMC
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeTableFunction.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODE_TABLE_FUNCTION_H
#define NODE_TABLE_FUNCTION_H

#include "nodes/execnodes.h"

extern TupleTableSlot*         ExecTableFunction(TableFunctionState *repeatstate);
extern TableFunctionState *ExecInitTableFunction(TableFunctionScan *node, 
												 EState *estate, 
												 int eflags);
extern int           ExecCountSlotsTableFunction(TableFunctionScan *node);
extern void                 ExecEndTableFunction(TableFunctionState *node);
extern void              ExecReScanTableFunction(TableFunctionState *node, 
												 ExprContext *exprCtxt);

#endif /* NODE_TABLE_FUNCTION_H */
