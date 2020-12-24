/*-------------------------------------------------------------------------
 * nodeTupleSplit.h
 *	  prototypes for nodeTupleSplit.
 *
 * Portions Copyright (c) 2019-Present VMware, Inc. or its affiliates.
 *
 * src/include/executor/nodeTupleSplit.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GPDB_NODETUPLESPLIT_H
#define GPDB_NODETUPLESPLIT_H

#include "fmgr.h"
#include "executor/tuptable.h"
#include "nodes/execnodes.h"
#include "nodes/primnodes.h"
#include "utils/tuplesort.h"

extern TupleSplitState *ExecInitTupleSplit(TupleSplit *node, EState *estate, int eflags);
extern void ExecEndTupleSplit(TupleSplitState *node);
extern void ExecReScanTupleSplit(TupleSplitState *node);

extern void ExecSquelchTupleSplit(TupleSplitState *node);
#endif /* NODETUPLESPLIT_H */
