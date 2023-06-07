/*-------------------------------------------------------------------------
 *
 * orca.h
 *	  prototypes for the ORCA query planner
 *
 *
 * Portions Copyright (c) 2010-Present, VMware, Inc. or its affiliates
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/include/optimizer/orca.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef ORCA_H
#define ORCA_H

#include "pg_config.h"

#ifdef USE_ORCA

extern PlannedStmt * optimize_query(Query *parse, int cursorOptions, ParamListInfo boundParams);
extern Node *transformGroupedWindows(Node *node, void *context);

#endif

#endif /* ORCA_H */
