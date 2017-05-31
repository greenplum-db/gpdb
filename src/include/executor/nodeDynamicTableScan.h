/*
 * nodeDynamicTableScan.h
 *
 * Copyright (c) 2012 - present, EMC/Greenplum
 */
#ifndef NODEDYNAMICTABLESCAN_H
#define NODEDYNAMICTABLESCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsDynamicTableScan(DynamicTableScan *node);
extern DynamicTableScanState *ExecInitDynamicTableScan(DynamicTableScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecDynamicTableScan(DynamicTableScanState *node);
extern void ExecEndDynamicTableScan(DynamicTableScanState *node);
extern void ExecDynamicTableMarkPos(DynamicTableScanState *node);
extern void ExecDynamicTableRestrPos(DynamicTableScanState *node);
extern void ExecDynamicTableReScan(DynamicTableScanState *node, ExprContext *exprCtxt);

static inline gpmon_packet_t * GpmonPktFromDynamicTableScanState(DynamicTableScanState *node)
{
	return &node->tableScanState.ss.ps.gpmon_pkt;
}
#endif
