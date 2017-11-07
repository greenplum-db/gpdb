/*-------------------------------------------------------------------------
 *
 * autostats.c
 *
 * Greenplum auto-analyze code
 *
 *
 * Portions Copyright (c) 2005-2015, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/autostats.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/catalog.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpartition.h"
#include "commands/vacuum.h"
#include "executor/execdesc.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "postmaster/autostats.h"
#include "utils/acl.h"
#include "miscadmin.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/*
 * Forward declarations.
 */
static void autostats_issue_analyze(Oid relationOid);
static bool autostats_on_change_check(AutoStatsCmdType cmdType, uint64 ntuples);
static bool autostats_on_no_stats_check(AutoStatsCmdType cmdType, Oid relationOid);

/*
 * Auto-stats employs this sub-routine to issue an analyze on a specific relation.
 */
static void
autostats_issue_analyze(Oid relationOid)
{
	VacuumStmt *analyzeStmt = NULL;
	RangeVar   *relation = NULL;

	/*
	 * If this user does not own the table, then auto-stats will not issue the
	 * analyze.
	 */
	if (!(pg_class_ownercheck(relationOid, GetUserId()) ||
		  (pg_database_ownercheck(MyDatabaseId, GetUserId()) && !IsSharedRelation(relationOid))))
	{
		elog(DEBUG3, "Auto-stats did not issue ANALYZE on tableoid %d since the user does not have table-owner level permissions.",
			 relationOid);

		return;
	}

	relation = makeRangeVar(get_namespace_name(get_rel_namespace(relationOid)), get_rel_name(relationOid), -1);
	analyzeStmt = makeNode(VacuumStmt);
	/* Set up command parameters */
	analyzeStmt->vacuum = false;
	analyzeStmt->full = false;
	analyzeStmt->analyze = true;
	analyzeStmt->freeze_min_age = -1;
	analyzeStmt->verbose = false;
	analyzeStmt->rootonly = false;
	analyzeStmt->relation = relation;	/* not used since we pass relids list */
	analyzeStmt->va_cols = NIL;
	vacuum(analyzeStmt, InvalidOid, NULL, false, false);
	pfree(analyzeStmt);
}

/*
 * Method determines if auto-stats should run as per onchange auto-stats policy. This policy
 * enables auto-analyze if the command was a CTAS, INSERT, DELETE, UPDATE or COPY
 * and the number of tuples is greater than a threshold.
 */
static bool
autostats_on_change_check(AutoStatsCmdType cmdType, uint64 ntuples)
{
	bool		result = false;

	switch (cmdType)
	{
		case AUTOSTATS_CMDTYPE_CTAS:
		case AUTOSTATS_CMDTYPE_INSERT:
		case AUTOSTATS_CMDTYPE_DELETE:
		case AUTOSTATS_CMDTYPE_UPDATE:
		case AUTOSTATS_CMDTYPE_COPY:
			result = true;
			break;
		default:
			break;
	}

	result = result && (ntuples > gp_autostats_on_change_threshold);
	return result;
}

/*
 * Method determines if auto-stats should run as per onnostats auto-stats policy. This policy
 * enables auto-analyze if :
 * (1) CTAS
 * (2) I-S or COPY if there are no statistics present
 */
static bool
autostats_on_no_stats_check(AutoStatsCmdType cmdType, Oid relationOid)
{
	if (cmdType == AUTOSTATS_CMDTYPE_CTAS)
		return true;

	if (!(cmdType == AUTOSTATS_CMDTYPE_INSERT
		  || cmdType == AUTOSTATS_CMDTYPE_COPY))
		return false;

	/*
	 * a relation has no stats if the corresponding row in pg_class has
	 * relpages=0, reltuples=0
	 */
	{
		HeapTuple	tuple;
		Form_pg_class classForm;
		bool		result = false;

		/*
		 * Must get the relation's tuple from pg_class
		 */
		tuple = SearchSysCache(RELOID,
							   ObjectIdGetDatum(relationOid),
							   0, 0, 0);
		if (!HeapTupleIsValid(tuple))
		{
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_TABLE),
					 errmsg("relation with OID %u does not exist",
							relationOid)));
			return false;
		}
		classForm = (Form_pg_class) GETSTRUCT(tuple);
		elog(DEBUG5, "Auto-stats ONNOSTATS check on tableoid %d has relpages = %d reltuples = %.0f.",
			 relationOid,
			 classForm->relpages,
			 classForm->reltuples);

		result = (classForm->relpages == 0 && classForm->reltuples < 1);
		ReleaseSysCache(tuple);
		return result;
	}

	/* we should not get here at all */
}

/*
 * Convert command type to string for logging purposes.
 */
const char *
autostats_cmdtype_to_string(AutoStatsCmdType cmdType)
{
	switch (cmdType)
	{
		case AUTOSTATS_CMDTYPE_CTAS:
			return "CTAS";
		case AUTOSTATS_CMDTYPE_INSERT:
			return "INSERT";
		case AUTOSTATS_CMDTYPE_DELETE:
			return "DELETE";
		case AUTOSTATS_CMDTYPE_UPDATE:
			return "UPDATE";
		case AUTOSTATS_CMDTYPE_COPY:
			return "COPY";
		default:

			/*
			 * we should not reach here .. but this method should probably not
			 * throw an error
			 */
			break;
	}
	return "UNKNOWN";
}

/*
 * This function extracts the command type and id of the modified relation from a
 * a PlannedStmt. This is done in preparation to call auto_stats()
 */
void
autostats_get_cmdtype(QueryDesc *queryDesc, AutoStatsCmdType * pcmdType, Oid *prelationOid)
{
	PlannedStmt *stmt = queryDesc->plannedstmt;
	Oid			relationOid = InvalidOid;		/* relation that is modified */
	AutoStatsCmdType cmdType = AUTOSTATS_CMDTYPE_SENTINEL;		/* command type */
	RangeTblEntry *rte = NULL;

	switch (stmt->commandType)
	{
		case CMD_SELECT:
			if (stmt->intoClause != NULL)
			{
				/* CTAS */
				relationOid = GetIntoRelOid(queryDesc);
				cmdType = AUTOSTATS_CMDTYPE_CTAS;
			}
			break;
		case CMD_INSERT:
			rte = rt_fetch(lfirst_int(list_head(stmt->resultRelations)), stmt->rtable);
			relationOid = rte->relid;
			cmdType = AUTOSTATS_CMDTYPE_INSERT;
			break;
		case CMD_UPDATE:
			rte = rt_fetch(lfirst_int(list_head(stmt->resultRelations)), stmt->rtable);
			relationOid = rte->relid;
			cmdType = AUTOSTATS_CMDTYPE_UPDATE;
			break;
		case CMD_DELETE:
			rte = rt_fetch(lfirst_int(list_head(stmt->resultRelations)), stmt->rtable);
			relationOid = rte->relid;
			cmdType = AUTOSTATS_CMDTYPE_DELETE;
			break;
		case CMD_UTILITY:
		case CMD_UNKNOWN:
		case CMD_NOTHING:
			break;
		default:
			Assert(false);
			break;
	}

	Assert(cmdType >= 0 && cmdType <= AUTOSTATS_CMDTYPE_SENTINEL);
	*pcmdType = cmdType;
	*prelationOid = relationOid;
}

/*
 * This method takes a decision to run analyze based on the query and the number of modified tuples based
 * on the policy set via gp_autostats_mode. The following modes are currently supported:
 * none			:	no automatic analyzes are issued. simply return.
 * on_change	:	if the number of modified tuples > gp_onchange_threshold, then an automatic analyze is issued.
 * on_no_stats	:	if the operation is a ctas/insert-select and there are no stats on the modified table,
 *					an automatic analyze is issued.
 */
void
auto_stats(AutoStatsCmdType cmdType, Oid relationOid, uint64 ntuples, bool inFunction)
{
	TimestampTz start;
	bool		policyCheck = false;

	start = GetCurrentTimestamp();

	if (Gp_role != GP_ROLE_DISPATCH || relationOid == InvalidOid || rel_is_partitioned(relationOid))
	{
		return;
	}

	Assert(relationOid != InvalidOid);
	Assert(cmdType >= 0 && cmdType <= AUTOSTATS_CMDTYPE_SENTINEL);		/* it is a valid command
																		 * as per auto-stats */

	GpAutoStatsModeValue actual_gp_autostats_mode;

	if (inFunction)
	{
		actual_gp_autostats_mode = gp_autostats_mode_in_functions;
	}
	else
	{
		actual_gp_autostats_mode = gp_autostats_mode;
	}

	switch (actual_gp_autostats_mode)
	{
		case GP_AUTOSTATS_ON_CHANGE:
			policyCheck = autostats_on_change_check(cmdType, ntuples);
			break;
		case GP_AUTOSTATS_ON_NO_STATS:
			policyCheck = autostats_on_no_stats_check(cmdType, relationOid);
			break;
		default:
			Assert(actual_gp_autostats_mode == GP_AUTOSTATS_NONE);
			policyCheck = false;
			break;
	}

	if (!policyCheck)
	{
		elog(DEBUG3, "In mode %s, command %s on (dboid,tableoid)=(%d,%d) modifying " UINT64_FORMAT " tuples did not issue Auto-ANALYZE.",
			 gpvars_show_gp_autostats_mode(),
			 autostats_cmdtype_to_string(cmdType),
			 MyDatabaseId,
			 relationOid,
			 ntuples);

		return;
	}

	if (log_autostats)
	{
		const char *autostats_mode;

		if (inFunction)
		{
			autostats_mode = gpvars_show_gp_autostats_mode_in_functions();
		}
		else
		{
			autostats_mode = gpvars_show_gp_autostats_mode();
		}
		elog(LOG, "In mode %s, command %s on (dboid,tableoid)=(%d,%d) modifying " UINT64_FORMAT " tuples caused Auto-ANALYZE.",
			 autostats_mode,
			 autostats_cmdtype_to_string(cmdType),
			 MyDatabaseId,
			 relationOid,
			 ntuples);
	}

	autostats_issue_analyze(relationOid);

	if (log_duration)
	{
		long		secs;
		int			usecs;
		int			msecs;

		TimestampDifference(start, GetCurrentTimestamp(), &secs, &usecs);
		msecs = usecs / 1000;
		elog(LOG, "duration: %ld.%03d ms Auto-ANALYZE", secs * 1000 + msecs, usecs % 1000);
	}
}
