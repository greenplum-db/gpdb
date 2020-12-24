/*--------------------------------------------------------------------------
 *
 * execDynamicScan.h
 *	 Definitions and API functions for execDynamicScan.c
 *
 * Copyright (c) 2014-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execDynamicScan.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef EXECDYNAMICSCAN_H
#define EXECDYNAMICSCAN_H

#include "access/attnum.h"
#include "nodes/execnodes.h"

extern bool isDynamicScan(const Plan *p);
extern int DynamicScan_GetDynamicScanIdPrintable(Plan *plan);

extern Oid DynamicScan_GetTableOid(ScanState *scanState);
extern void DynamicScan_SetTableOid(ScanState *scanState, Oid curRelOid);

extern bool DynamicScan_RemapExpression(ScanState *scanState, AttrNumber *attMap, Node *expr);

#endif   /* EXECDYNAMICSCAN_H */
