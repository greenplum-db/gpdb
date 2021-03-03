/*-------------------------------------------------------------------------
 *
 * nodeShareInputScan.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeShareInputScan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESHAREINPUTSCAN_H
#define NODESHAREINPUTSCAN_H

#include "nodes/execnodes.h"
#include "storage/sharedfileset.h"

extern ShareInputScanState *ExecInitShareInputScan(ShareInputScan *node, EState *estate, int eflags);
extern void ExecEndShareInputScan(ShareInputScanState *node);
extern void ExecReScanShareInputScan(ShareInputScanState *node);
extern void ExecSquelchShareInputScan(ShareInputScanState *node);

extern Size ShareInputShmemSize(void);
extern void ShareInputShmemInit(void);

extern SharedFileSet *get_shareinput_fileset(void);

#endif   /* NODESHAREINPUTSCAN_H */
