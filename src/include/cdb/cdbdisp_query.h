/*-------------------------------------------------------------------------
 *
 * cdbdisp_query.h
 * routines for dispatching command string or plan to the qExec processes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdisp_query.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_QUERY_H
#define CDBDISP_QUERY_H

#include "lib/stringinfo.h" /* StringInfo */
#include "cdb/cdbtm.h"

#define DF_NONE 0x0

/*
 * indicate whether an error occurring on one of the qExec segdbs should cause all still-executing
 * commands to cancel on other qExecs, normally this would be true.
 */
#define DF_CANCEL_ON_ERROR 0x1
/*
 * indicate whether the command to be dispatched should be done inside of a global transaction.
 */
#define DF_NEED_TWO_PHASE 0x2
/*
 * indicate whether the command should be dispatched to qExecs along with a snapshot.
 */
#define DF_WITH_SNAPSHOT  0x4

struct QueryDesc;
struct CdbDispatcherState;
struct CdbPgResults;

/* Compose and dispatch the MPPEXEC commands corresponding to a plan tree
 * within a complete parallel plan.
 *
 * The CdbDispatchResults objects allocated for the plan are
 * returned in *pPrimaryResults
 * The caller, after calling CdbCheckDispatchResult(), can
 * examine the CdbDispatchResults objects, can keep them as
 * long as needed, and ultimately must free them with
 * cdbdisp_destroyDispatchState() prior to deallocation
 * of the caller's memory context.
 *
 * NB: Callers should use PG_TRY()/PG_CATCH() if needed to make
 * certain that the CdbDispatchResults objects are destroyed by
 * cdbdisp_destroyDispatchState() in case of error.
 * To wait for completion, check for errors, and clean up, it is
 * suggested that the caller use cdbdisp_finishCommand().
 */
void
CdbDispatchPlan(struct QueryDesc *queryDesc,
					 bool planRequiresTxn,
					 bool cancelOnError,
					 struct CdbDispatcherState *ds);

/*
 * Special for sending SET commands that change GUC variables, so they go to all
 * gangs, both reader and writer
 */
void
CdbDispatchSetCommand(const char *strCommand, bool cancelOnError);

/*
 * CdbDispatchCommand
 *
 * Dispatch an plain command to all primary writer QEs, wait until
 * all QEs finished successfully. If one or more QEs got error,
 * throw an Error.
 *
 * -flags:
 * 	Is the combination of EUS_NEED_TWO_PHASE, EUS_WITH_SNAPSHOT,EUS_CANCEL_ON_ERROR
 *
 */
void
CdbDispatchCommand(const char* strCommand,
                    int flags,
                    struct CdbPgResults* cdb_pgresults);

/*
 * CdbDispatchUtilityStatement
 *
 * Dispatch an already parsed statement to all primary writer QEs, wait until
 * all QEs finished successfully. If one or more QEs got error,
 * throw an Error.
 *
 * -flags:
 * 	Is the combination of DF_NEED_TWO_PHASE, DF_WITH_SNAPSHOT,DF_CANCEL_ON_ERROR
 *
 * -cdb_pgresults:
 * 	Indicate whether return the pg_result for each QE connection.
 *
 * If returnPgResults is true, caller need to call cdbdisp_freeCdbPgResults() to
 * clear pg_results.
 */
void
CdbDispatchUtilityStatement(struct Node *stmt,
							int flags,
							List *oid_assignments,
							struct CdbPgResults* cdb_pgresults);

#endif   /* CDBDISP_QUERY_H */
