/*-------------------------------------------------------------------------
 *
 * nodeDynamicBitmapIndexscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeDynamicBitmapIndexscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEDYNAMICBITMAPINDEXSCAN_H
#define NODEDYNAMICBITMAPINDEXSCAN_H

#include "nodes/execnodes.h"

/* GPDB_12_MERGE_FIXME */
#if 0
extern DynamicBitmapIndexScanState *ExecInitDynamicBitmapIndexScan(DynamicBitmapIndexScan *node, EState *estate, int eflags);
extern Node *MultiExecDynamicBitmapIndexScan(DynamicBitmapIndexScanState *node);
extern void ExecEndDynamicBitmapIndexScan(DynamicBitmapIndexScanState *node);
extern void ExecReScanDynamicBitmapIndex(DynamicBitmapIndexScanState *node);
#endif

#endif   /* NODEDYNAMICBITMAPINDEXSCAN_H */
