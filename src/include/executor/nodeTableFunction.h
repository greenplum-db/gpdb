/*-------------------------------------------------------------------------
 *
 * nodeTableFunction.h
 *
 * Portions Copyright (c) 2011 - present, EMC
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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

extern TableFunctionState *ExecInitTableFunction(TableFunctionScan *node, 
												 EState *estate, 
												 int eflags);
extern void ExecEndTableFunction(TableFunctionState *node);
extern void ExecReScanTableFunction(TableFunctionState *node);

#endif /* NODE_TABLE_FUNCTION_H */
