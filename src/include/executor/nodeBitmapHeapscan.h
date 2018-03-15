/*-------------------------------------------------------------------------
 *
 * nodeBitmapHeapscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeBitmapHeapscan.h,v 1.8 2010/01/02 16:58:03 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEBITMAPHEAPSCAN_H
#define NODEBITMAPHEAPSCAN_H

#include "nodes/execnodes.h"

extern BitmapHeapScanState *ExecInitBitmapHeapScan(BitmapHeapScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecBitmapHeapScan(BitmapHeapScanState *node);
extern void ExecEndBitmapHeapScan(BitmapHeapScanState *node);
extern void ExecBitmapHeapReScan(BitmapHeapScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeBitmapHeapScan(BitmapHeapScanState *node);

extern void bitgetpage(HeapScanDesc scan, TBMIterateResult *tbmres);

#endif   /* NODEBITMAPHEAPSCAN_H */
