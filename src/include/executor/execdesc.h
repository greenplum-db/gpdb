/*-------------------------------------------------------------------------
 *
 * execdesc.h
 *	  plan and query descriptor accessor macros used by the executor
 *	  and related modules.
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/execdesc.h,v 1.37 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECDESC_H
#define EXECDESC_H

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "tcop/dest.h"
#include "gpmon/gpmon.h"

struct CdbExplain_ShowStatCtx;  /* private, in "cdb/cdbexplain.c" */


/* GangType enumeration is used in several structures related to CDB
 * slice plan support.
 */
typedef enum GangType
{
	GANGTYPE_UNALLOCATED,       /* a root slice executed by the qDisp */
	GANGTYPE_ENTRYDB_READER,    /* a 1-gang with read access to the entry db */
	GANGTYPE_SINGLETON_READER,	/* a 1-gang to read the segment dbs */
	GANGTYPE_PRIMARY_READER,    /* a 1-gang or N-gang to read the segment dbs */
	GANGTYPE_PRIMARY_WRITER		/* the N-gang that can update the segment dbs */
} GangType;

/*
 * MPP Plan Slice information
 *
 * These structures summarize how a plan tree is sliced up into separate
 * units of execution or slices. A slice will execute on a each worker within
 * a gang of processes. Some gangs have a worker process on each of several
 * databases, others have a single worker.
 */
typedef struct Slice
{
	NodeTag		type;

	/*
	 * The index in the global slice table of this slice. The root slice of
	 * the main plan is always 0. Slices that have senders at their local
	 * root have a sliceIndex equal to the motionID of their sender Motion.
	 *
	 * Undefined slices should have this set to -1.
	 */
	int			sliceIndex;

	/*
	 * The root slice of the slice tree of which this slice is a part.
	 */
	int			rootIndex;

	/*
	 * the index of parent in global slice table (origin 0) or -1 if
	 * this is root slice.
	 */
	int			parentIndex;

	/*
	 * An integer list of indices in the global slice table (origin  0)
	 * of the child slices of this slice, or -1 if this is a leaf slice.
	 * A child slice corresponds to a receiving motion in this slice.
	 */
	List	   *children;

	/* What kind of gang does this slice need? */
	GangType	gangType;

	/*
	 * How many gang members needed?
	 *
	 * It is set before the process lists below and used to decide how
	 * to initialize them.
	 */
	int			gangSize;

	/*
	 * How many of the gang members will actually be used? This takes into
	 * account directDispatch information.
	 */
	int			numGangMembersToBeActive;

	/*
	 * directDispatch->isDirectDispatch should ONLY be set for a slice
	 * when it requires an n-gang.
	 */
	DirectDispatchInfo directDispatch;

	struct Gang *primaryGang;

	/*
	 * A list of CDBProcess nodes corresponding to the worker processes
	 * allocated to implement this plan slice.
	 *
	 * The number of processes must agree with the the plan slice to be
	 * implemented.
	 */
	List	   *primaryProcesses;
} Slice;

/*
 * The SliceTable is a list of Slice structures organized into root slices
 * and motion slices as follows:
 *
 * Slice 0 is the root slice of plan as a whole.
 * Slices 1 through nMotion are motion slices with a sending motion at
 *  the root of the slice.
 * Slices nMotion+1 and on are root slices of initPlans.
 *
 * There may be unused slices in case the plan contains subplans that
 * are  not initPlans.  (This won't happen unless MPP decides to support
 * subplans similarly to PostgreSQL, which isn't the current plan.)
 */
typedef struct SliceTable
{
	NodeTag		type;

	int			nMotions;		/* The number Motion nodes in the entire plan */
	int			nInitPlans;		/* The number of initplan slices allocated */
	int			localSlice;		/* Index of the slice to execute. */
	List	   *slices;			/* List of slices */
	int			instrument_options;	/* OR of InstrumentOption flags */
	uint32		ic_instance_id;
} SliceTable;

/*
 * Holds information about a cursor's current position.
 */
typedef struct CursorPosInfo
{
	NodeTag type;

	char	   *cursor_name;
	int		 	gp_segment_id;
	ItemPointerData	ctid;
	Oid			table_oid;
} CursorPosInfo;

/* ----------------
 *		query dispatch information:
 *
 * a QueryDispatchDesc encapsulates extra information that need to be
 * dispatched from QD to QEs.
 *
 * A QueryDesc is created separately on each segment, but QueryDispatchDesc
 * is created in the QD, and passed to each segment.
 * ---------------------
 */
typedef struct QueryDispatchDesc
{
	NodeTag		type;

	/*
	 * For a SELECT INTO statement, this stores the tablespace to use for the
	 * new table and related auxiliary tables.
	 */
	char		*intoTableSpaceName;

	/*
	 * Oids to use, for new objects created in a CREATE command.
	 */
	List	   *oidAssignments;

	/*
	 * This allows the slice table to accompany the plan as it moves
	 * around the executor.
	 *
	 * Currently, the slice table should not be installed on the QD.
	 * Rather is it shipped to QEs as a separate parameter to MPPEXEC.
	 * The implementation of MPPEXEC, which runs on the QEs, installs
	 * the slice table in the plan as required there.
	 */
	SliceTable *sliceTable;

	List	   *cursorPositions;

	/*
	 * Set to true for CTAS and SELECT INTO. Set to false for ALTER TABLE
	 * REORGANIZE. This is mainly used to track if the dispatched query is
	 * meant for internal rewrite on QE segments or just for holding data from
	 * a SELECT for a new relation. If DestIntoRel is set in the QD's
	 * queryDesc->dest, use the original table's reloptions. If DestRemote is
	 * set, use default reloptions + gp_default_storage_options.
	 */
	bool validate_reloptions;
} QueryDispatchDesc;

/*
 * When a CREATE command is dispatched to segments, the OIDs used for the
 * new objects are sent in a list of OidAssignments.
 */
typedef struct
{
	NodeTag		type;

	/*
	 * Key data. Depending on the catalog table, different fields are used.
	 * See CreateKeyFromCatalogTuple().
	 */
	Oid			catalog;		/* OID of the catalog table, e.g. pg_class */
	Oid			namespaceOid;	/* namespace OID for most objects */
	char	   *objname;		/* object name (e.g. relation name) */
	Oid			keyOid1;		/* generic OID field, meaning depends on object type */
	Oid			keyOid2;		/* 2nd generic OID field, meaning depends on object type */

	Oid			oid;			/* OID to assign */

} OidAssignment;

/* ----------------
 *		query descriptor:
 *
 *	a QueryDesc encapsulates everything that the executor
 *	needs to execute the query.
 *
 *	For the convenience of SQL-language functions, we also support QueryDescs
 *	containing utility statements; these must not be passed to the executor
 *	however.
 * ---------------------
 */
typedef struct QueryDesc
{
	/* These fields are provided by CreateQueryDesc */
	CmdType		operation;		/* CMD_SELECT, CMD_UPDATE, etc. */
	PlannedStmt *plannedstmt;	/* planner's output, or null if utility */
	Node	   *utilitystmt;	/* utility statement, or null */
	const char *sourceText;		/* source text of the query */
	Snapshot	snapshot;		/* snapshot to use for query */
	Snapshot	crosscheck_snapshot;	/* crosscheck for RI update/delete */
	DestReceiver *dest;			/* the destination for tuple output */
	ParamListInfo params;		/* param values being passed in */
	int			instrument_options;		/* OR of InstrumentOption flags */

	/* These fields are set by ExecutorStart */
	TupleDesc	tupDesc;		/* descriptor for result tuples */
	EState	   *estate;			/* executor's query-wide state */
	PlanState  *planstate;		/* tree of per-plan-node state */

	/* This field is set by ExecutorEnd after collecting cdbdisp results */
	uint64		es_processed;	/* # of tuples processed */
	Oid			es_lastoid;		/* oid of row inserted */
	bool		extended_query;   /* simple or extended query protocol? */
	char		*portal_name;	/* NULL for unnamed portal */

	QueryDispatchDesc *ddesc;

	/* CDB: EXPLAIN ANALYZE statistics */
	struct CdbExplain_ShowStatCtx  *showstatctx;

	/* Gpmon */
	gpmon_packet_t *gpmon_pkt;

	/* This is always set NULL by the core system, but plugins can change it */
	struct Instrumentation *totaltime;	/* total time spent in ExecutorRun */

} QueryDesc;

/* in pquery.c */
extern QueryDesc *CreateQueryDesc(PlannedStmt *plannedstmt,
								  const char *sourceText,
				Snapshot snapshot,
				Snapshot crosscheck_snapshot,
				DestReceiver *dest,
				ParamListInfo params,
				int instrument_options);

extern QueryDesc *CreateUtilityQueryDesc(Node *utilitystmt,
										 const char *sourceText,
					   Snapshot snapshot,
					   DestReceiver *dest,
					   ParamListInfo params);

extern void FreeQueryDesc(QueryDesc *qdesc);

#endif   /* EXECDESC_H  */
