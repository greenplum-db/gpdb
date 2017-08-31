/*-------------------------------------------------------------------------
 *
 * cdbexplain.h
 *    Functions supporting the Greenplum EXPLAIN ANALYZE command
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbexplain.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBEXPLAIN_H
#define CDBEXPLAIN_H

#include "executor/instrument.h"        /* instr_time */
#include "cdb/cdbpublic.h"              /* CdbExplain_Agg */

struct CdbDispatchResults;              /* #include "cdb/cdbdispatchresult.h" */
struct EState;                          /* #include "nodes/execnodes.h" */
struct PlanState;                       /* #include "nodes/execnodes.h" */
struct QueryDesc;                       /* #include "executor/execdesc.h" */
struct StringInfoData;                  /* #include "lib/stringinfo.h" */

struct CdbExplain_ShowStatCtx;          /* private, in "cdb/cdbexplain.c" */
struct PlannedStmt;

static inline void
cdbexplain_agg_init0(CdbExplain_Agg *agg)
{
    agg->vmax = 0;
    agg->vsum = 0;
    agg->vcnt = 0;
    agg->imax = 0;
}

static inline bool
cdbexplain_agg_upd(CdbExplain_Agg *agg, double v, int id)
{
    if (v > 0)
    {
        agg->vsum += v;
        agg->vcnt++;

        if (v > agg->vmax ||
            agg->vcnt == 0)
        {
            agg->vmax = v;
            agg->imax = id;
            return true;
        }
    }
    return false;
}

static inline double
cdbexplain_agg_avg(CdbExplain_Agg *agg)
{
    return (agg->vcnt > 0) ? agg->vsum / agg->vcnt
                           : 0;
}


/*
 * cdbexplain_localExecStats
 *    Called by qDisp to build NodeSummary and SliceSummary blocks
 *    containing EXPLAIN ANALYZE statistics for a root slice that
 *    has been executed locally in the qDisp process.  Attaches these
 *    structures to the PlanState nodes' Instrumentation objects for
 *    later use by cdbexplain_showExecStats().
 *
 * 'planstate' is the top PlanState node of the slice.
 * 'showstatctx' is a CdbExplain_ShowStatCtx object which was created by
 *      calling cdbexplain_showExecStatsBegin().
 */
void
cdbexplain_localExecStats(struct PlanState                 *planstate,
                          struct CdbExplain_ShowStatCtx    *showstatctx);

/*
 * cdbexplain_sendExecStats
 *    Called by qExec process to send EXPLAIN ANALYZE statistics to qDisp.
 *    On the qDisp, libpq will attach this data to the PGresult object.
 */
void
cdbexplain_sendExecStats(struct QueryDesc *queryDesc);

/*
 * cdbexplain_recvExecStats
 *    Called by qDisp to transfer a slice's EXPLAIN ANALYZE statistics
 *    from the CdbDispatchResults structures to the PlanState tree.
 *    Recursively does the same for slices that are descendants of the
 *    one specified.
 *
 * 'showstatctx' is a CdbExplain_ShowStatCtx object which was created by
 *      calling cdbexplain_showExecStatsBegin().
 */
void
cdbexplain_recvExecStats(struct PlanState              *planstate,
                         struct CdbDispatchResults     *dispatchResults,
                         int                            sliceIndex,
                         struct CdbExplain_ShowStatCtx *showstatctx);

/*
 * cdbexplain_showExecStatsBegin
 *    Called by qDisp process to create a CdbExplain_ShowStatCtx structure
 *    in which to accumulate overall statistics for a query.
 *
 * 'explaincxt' is a MemoryContext from which to allocate the ShowStatCtx as
 *      well as any needed buffers and the like.  The explaincxt ptr is saved
 *      in the ShowStatCtx.  The caller is expected to reset or destroy the
 *      explaincxt not too long after calling cdbexplain_showExecStatsEnd(); so
 *      we don't bother to pfree() memory that we allocate from this context.
 * 'querystarttime' is the timestamp of the start of the query, in a
 *      platform-dependent format.
 */
struct CdbExplain_ShowStatCtx *
cdbexplain_showExecStatsBegin(struct QueryDesc *queryDesc,
                              instr_time        querystarttime);

/*
 * cdbexplain_showExecStats
 *    Called by qDisp process to format a node's EXPLAIN ANALYZE statistics.
 *
 * 'planstate' is the node whose statistics are to be displayed.
 * 'str' is the output buffer.
 * 'ctx' is a CdbExplain_ShowStatCtx object which was created by
 *      calling cdbexplain_showExecStatsBegin().
 */
void
cdbexplain_showExecStats(struct PlanState              *planstate,
                         struct StringInfoData         *str,
                         int                            indent,
                         struct CdbExplain_ShowStatCtx *ctx);


/*
 * cdbexplain_showExecStatsEnd
 *    Called by qDisp process to format the overall statistics for a query
 *    into the caller's buffer.
 *
 * 'ctx' is the CdbExplain_ShowStatCtx object which was created by a call to
 *      cdbexplain_showExecStatsBegin() and contains statistics which have
 *      been accumulated over a series of calls to cdbexplain_showExecStats().
 *      Invalid on return (it is freed).
 * 'str' is the output buffer.
 *
 * This doesn't free the CdbExplain_ShowStatCtx object or buffers, because
 * shortly afterwards the caller is expected to destroy the 'explaincxt'
 * MemoryContext which was passed to cdbexplain_showExecStatsBegin(), thus
 * freeing all at once.
 */
void
cdbexplain_showExecStatsEnd(struct PlannedStmt *stmt,
							struct CdbExplain_ShowStatCtx  *showstatctx,
                            struct StringInfoData          *str,
                            struct EState                  *estate);


#endif   /* CDBEXPLAIN_H */
