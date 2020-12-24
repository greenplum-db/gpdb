/*-------------------------------------------------------------------------
 *
 * autostats.c
 *
 * Greenplum auto-analyze code
 *
 *
 * Portions Copyright (c) 2005-2015, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "commands/vacuum.h"
#include "executor/execdesc.h"
#include "executor/executor.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "postmaster/autostats.h"
#include "postmaster/autovacuum.h"
#include "utils/acl.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

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
	VacuumStmt *analyzeStmt;
	VacuumRelation *relation;
	ParseState *pstate;

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

	/* Set up an ANALYZE command */
	relation = makeVacuumRelation(NULL, relationOid, NIL);
	analyzeStmt = makeNode(VacuumStmt);
	analyzeStmt->options = NIL;
	analyzeStmt->rels = list_make1(relation);
	analyzeStmt->is_vacuumcmd = false;

	pstate = make_parsestate(NULL);
	pstate->p_sourcetext = NULL;

	ExecVacuum(pstate, analyzeStmt, false);

	free_parsestate(pstate);
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

		if (classForm->relkind == RELKIND_FOREIGN_TABLE &&
			rel_is_external_table(relationOid))
		{
			/*
			 * To keep the behaviour the same as in GPDB 6, don't try to
			 * auto-analyze external tables. In GPDB 6, we used to populate
			 * pg_class.relpages with a constant at CREATE EXTERNAL TABLE.
			 * We don't do that anymore, for consistency with foreign tables,
			 * but without this special case here, we would then try to
			 * auto-analyze external tables. External tables don't have
			 * an ANALYZE callback, so it wouldn't do anything, but it would
			 * print an annoying "cannot analyze this foreign table" warning
			 * every time you inserted to an external table.
			 *
			 * All foreign tables without an analyze callback have the same
			 * problem, really, but we're not concerned about that right now.
			 */
			result = false;
		}
		else
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

	switch (stmt->commandType)
	{
		case CMD_SELECT:
			if (stmt->intoClause != NULL)
			{
				/* CTAS */
				relationOid = GetIntoRelOid(queryDesc);
				cmdType = AUTOSTATS_CMDTYPE_CTAS;
			}
			else if (stmt->copyIntoClause != NULL)
			{
				cmdType = AUTOSTATS_CMDTYPE_COPY;
			}
			break;

		case CMD_INSERT:
		case CMD_UPDATE:
		case CMD_DELETE:
			{
				RangeTblEntry *rte;

				if (stmt->resultRelations)
				{
					rte = rt_fetch(lfirst_int(list_head(stmt->resultRelations)), stmt->rtable);
					relationOid = rte->relid;
				}

				if (stmt->commandType == CMD_INSERT)
					cmdType = AUTOSTATS_CMDTYPE_INSERT;
				else if (stmt->commandType == CMD_UPDATE)
					cmdType = AUTOSTATS_CMDTYPE_UPDATE;
				else
					cmdType = AUTOSTATS_CMDTYPE_DELETE;
			}
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
	char		relkind;

	TimestampTz start;
	bool		policyCheck = false;

	start = GetCurrentTimestamp();

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	if (AutoVacuumingActive())
		return;

	if (relationOid == InvalidOid)
		return;

	relkind = get_rel_relkind(relationOid);
	if (relkind == '\0')
		return; /* relation not found */

	/* Updates on views are possible via triggers, but we can't analyze views. */
	if (relkind == RELKIND_VIEW)
		return;

	if (relkind == RELKIND_PARTITIONED_TABLE)
		return;

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
			 lookup_autostats_mode_by_value(actual_gp_autostats_mode),
			 autostats_cmdtype_to_string(cmdType),
			 MyDatabaseId,
			 relationOid,
			 ntuples);

		return;
	}

	if (log_autostats)
	{
		elog(LOG, "In mode %s, command %s on (dboid,tableoid)=(%d,%d) modifying " UINT64_FORMAT " tuples caused Auto-ANALYZE.",
			 lookup_autostats_mode_by_value(actual_gp_autostats_mode),
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
