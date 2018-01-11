/*-------------------------------------------------------------------------
 *
 * nodeBitmapAppendOnlyscan.h
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeBitmapAppendOnlyscan.h
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEBITMAPAPPENDONLYSCAN_H
#define NODEBITMAPAPPENDONLYSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsBitmapAppendOnlyScan(BitmapAppendOnlyScan *node);
extern BitmapAppendOnlyScanState *ExecInitBitmapAppendOnlyScan(BitmapAppendOnlyScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);
extern void ExecEndBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);
extern void ExecBitmapAppendOnlyReScan(BitmapAppendOnlyScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);

#endif   /* NODEBITMAPAPPENDONLYSCAN_H */

