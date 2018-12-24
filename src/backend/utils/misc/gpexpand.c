/*-------------------------------------------------------------------------
 *
 * gpexpand.c
 *	  Helper functions for gpexpand.
 *
 *
 * Copyright (c) 2018-Present Pivotal Software, Inc.
 *
 * src/backend/utils/misc/gpexpand.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "../interfaces/libpq/libpq-fe.h"

#include "catalog/gp_configuration_history.h"
#include "catalog/pg_auth_time_constraint.h"
#include "catalog/pg_description.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_encoding.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_shdescription.h"
#include "catalog/pg_stat_last_operation.h"
#include "catalog/pg_stat_last_shoperation.h"
#include "catalog/pg_statistic.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "postmaster/fts.h"
#include "storage/lock.h"
#include "utils/builtins.h"
#include "utils/gpexpand.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/timestamp.h"

#define foreach_phase1_status_code(code) \
	for ((code) = GP_EXPAND_STATUS_PHASE1 + 1; \
		 (code) < GP_EXPAND_STATUS_PHASE1_UNKNOWN; \
		 (code)++)

#define foreach_phase2_status_code(code) \
	for ((code) = GP_EXPAND_STATUS_PHASE2 + 1; \
		 (code) < GP_EXPAND_STATUS_PHASE2_UNKNOWN; \
		 (code)++)

enum
{
	GP_EXPAND_STATUS_PHASE0 = 0,

	GP_EXPAND_STATUS_PHASE1 = 100,

	GP_EXPAND_STATUS_PHASE1_UNINITIALIZED,
	GP_EXPAND_STATUS_PHASE1_EXPANSION_PREPARE_STARTED,
	GP_EXPAND_STATUS_PHASE1_BUILD_SEGMENT_TEMPLATE_STARTED,
	GP_EXPAND_STATUS_PHASE1_BUILD_SEGMENT_TEMPLATE_DONE,
	GP_EXPAND_STATUS_PHASE1_BUILD_SEGMENTS_STARTED,
	GP_EXPAND_STATUS_PHASE1_BUILD_SEGMENTS_DONE,
	GP_EXPAND_STATUS_PHASE1_UPDATE_CATALOG_STARTED,
	GP_EXPAND_STATUS_PHASE1_UPDATE_CATALOG_DONE,
	GP_EXPAND_STATUS_PHASE1_SETUP_EXPANSION_SCHEMA_STARTED,
	GP_EXPAND_STATUS_PHASE1_SETUP_EXPANSION_SCHEMA_DONE,
	GP_EXPAND_STATUS_PHASE1_PREPARE_EXPANSION_SCHEMA_STARTED,
	GP_EXPAND_STATUS_PHASE1_PREPARE_EXPANSION_SCHEMA_DONE,
	GP_EXPAND_STATUS_PHASE1_EXPANSION_PREPARE_DONE,

	GP_EXPAND_STATUS_PHASE1_UNKNOWN,

	GP_EXPAND_STATUS_PHASE2 = 200,

	GP_EXPAND_STATUS_PHASE2_SETUP_DONE,
	GP_EXPAND_STATUS_PHASE2_EXPANSION_STOPPED,
	GP_EXPAND_STATUS_PHASE2_EXPANSION_STARTED,
	GP_EXPAND_STATUS_PHASE2_EXPANSION_COMPLETE,

	GP_EXPAND_STATUS_PHASE2_UNKNOWN,
};

/* phase1 status are collected from gpexpand GpExpandStatus() */
static const char *(phase1_status[]) =
{
	"UNINITIALIZED",
	"EXPANSION_PREPARE_STARTED",
	"BUILD_SEGMENT_TEMPLATE_STARTED",
	"BUILD_SEGMENT_TEMPLATE_DONE",
	"BUILD_SEGMENTS_STARTED",
	"BUILD_SEGMENTS_DONE",
	"UPDATE_CATALOG_STARTED",
	"UPDATE_CATALOG_DONE",
	"SETUP_EXPANSION_SCHEMA_STARTED",
	"SETUP_EXPANSION_SCHEMA_DONE",
	"PREPARE_EXPANSION_SCHEMA_STARTED",
	"PREPARE_EXPANSION_SCHEMA_DONE",
	"EXPANSION_PREPARE_DONE",

	"UNKNOWN_PHASE1_STATUS",
};

/* phase2 status are collected from gpexpand main() */
static const char *(phase2_status[]) =
{
	"SETUP DONE",
	"EXPANSION STOPPED",
	"EXPANSION STARTED",
	"EXPANSION COMPLETE",

	"UNKNOWN PHASE2 STATUS",
};

typedef struct _ExpandStatus
{
	int32		code;		/* status code in integer */
	const char *status;		/* status name in text */
	const char *detail;		/* status detail in text */
} _ExpandStatus;

static const char *phase0_status_from_code(int code);
static const char *phase1_status_from_code(int code);
static const char *phase2_status_from_code(int code);
static int phase1_status_to_code(const char *status);
static int phase2_status_to_code(const char *status);
static void parse_phase1_status_line(_ExpandStatus *status, char *line);
static void get_phase1_status(_ExpandStatus *status);
static void get_phase2_status(_ExpandStatus *status);
static void get_phase1_status_from_file(_ExpandStatus *status, const char *filename);
static void get_phase2_status_from_file(_ExpandStatus *status);
static void get_phase2_status_from_schema(_ExpandStatus *status);
static void get_phase2_progress_from_schema(_ExpandStatus *status);

static volatile int *gp_expand_version;

/*
 * Catalog lock.
 */
static LOCKTAG gp_expand_locktag =
{
	/* FIXME: how to fill the locktag? */
	.locktag_field1 = 0xdead,
	.locktag_field2 = 0xdead,
	.locktag_field3 = 0xdead,
	.locktag_field4 = 0xdd,
	.locktag_type = LOCKTAG_USERLOCK,
	.locktag_lockmethodid = USER_LOCKMETHOD,
};

int
GpExpandVersionShmemSize(void)
{
	return sizeof(*gp_expand_version);
}

void
GpExpandVersionShmemInit(void)
{
	if (IsUnderPostmaster)
		return;

	/* only postmaster initialize it */
	gp_expand_version = (volatile int*)ShmemAlloc(GpExpandVersionShmemSize());  
	*gp_expand_version = 0;
}

int
GetGpExpandVersion(void)
{
	return *gp_expand_version;
}

/*
 * Used by gpexpand to bump the gpexpand version once gpexpand started up
 * new segments and updated the gp_segment_configuration.
 *
 * a gpexpand version change also prevent concurrent changes to catalog
 * during gpexpand (see gp_expand_lock_catalog)
 *
 */
Datum
gp_expand_bump_version(PG_FUNCTION_ARGS)
{
	*gp_expand_version += 1;
	PG_RETURN_VOID();
}

/*
 * Lock the catalog lock in exclusive mode.
 *
 * This should only be called by gpexpand.
 */
Datum
gp_expand_lock_catalog(PG_FUNCTION_ARGS)
{
	LockAcquire(&gp_expand_locktag, AccessExclusiveLock, false, false);

	PG_RETURN_VOID();
}

/*
 * Get the gpexpand status.
 *
 * The status is determined from gpexpand status file and schema tables.  The
 * output contains three columns: code, status and detail.
 *
 * - code: an integer code of the status.
 *   - 0: expansion is not in progress;
 *   - 1xx: the initialization phase;
 *   - 2xx: the redistribution phase;
 * - status: the status name in text;
 * - detail: the detail name in text;
 *
 * Both status and detail are loaded from the gpexpand status file and schema
 * tables, they are not always meaningful.  When we only care about whether
 * gpexpand is in progress we could simply query like this:
 *
 *     SELECT code > 0 as expansion FROM gp_expand_get_status();
 *
 * During redistribution phase the progress is displayed as detail, but only
 * when queried with the right database.
 *
 * Note: this function only reports the CURRENT status, the status can be
 * different at a second call.  Even if this function reports that expansion is
 * not in progress it does not mean that an expansion can not be started next
 * second.  To ensure that, call this function while holding the gpexpand
 * catalog protection lock in shared mode:
 *
 *     begin;
 *       -- any DDL can be used to acquire the lock in shared mode
 *       create temp table tmp_table; drop table tmp_table;
 *       -- expansion will not happen until end of transaction
 *       select gp_expand_get_status();
 *     end;
 */
Datum
gp_expand_get_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		int			nattr = 3;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "code", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "status", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "detail", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);
		funcctx->max_calls = 1;

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		_ExpandStatus status;
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;

		/* first check if it is initialization phase */
		get_phase1_status(&status);

		/* if not, further check if it is redistribution phase */
		if (status.code == GP_EXPAND_STATUS_PHASE0)
			get_phase2_status(&status);

		MemSet(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum(status.code);
		values[1] = CStringGetTextDatum(status.status);
		values[2] = CStringGetTextDatum(status.detail);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * Prevent catalog being changed during gpexpand.
 *
 * This should be called before any catalog changes.
 *
 * Will raise an error if gpexpand already in progress or complete.
 */
void
gp_expand_protect_catalog_changes(Relation relation)
{
	LockAcquireResult	acquired;
	int					oldVersion;
	int					newVersion;

	if (Gp_role != GP_ROLE_DISPATCH)
		/* only lock catalog updates on qd */
		return;

	if (RelationGetNamespace(relation) != PG_CATALOG_NAMESPACE)
		/* not catalog relations */
		return;

	switch (RelationGetRelid(relation))
	{
		case GpSegmentConfigRelationId:
		case GpConfigHistoryRelationId:
		case DescriptionRelationId:
		case PartitionRelationId:
		case PartitionRuleRelationId:
		case SharedDescriptionRelationId:
		case StatLastOpRelationId:
		case StatLastShOpRelationId:
		case StatisticRelationId:
		case PartitionEncodingRelationId:
		case AuthTimeConstraintRelationId:
			/* these catalog tables are only meaningful on qd */
			return;
	}

	/*
	 * The online expand util will hold this lock in AccessExclusiveLock mode.
	 * Acquire expand lock in dontWait mode. If the lock is not available,
	 * report error. Because online expand must be running, after that, the
	 * cluster size has been changed, and the catalog data has been copied
	 * to new segments, but this transaction gangs are still running on old
	 * segments. Any catalog changes won't be copied to new segment.
	 */
	/* FIXME: do not re-acquire the lock */
	acquired = LockAcquire(&gp_expand_locktag, AccessShareLock, false, true);
	if (acquired == LOCKACQUIRE_NOT_AVAIL)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("gpexpand in progress, catalog changes are disallowed.")));

	oldVersion = cdbcomponent_getCdbComponents(true)->expand_version;
	newVersion = GetGpExpandVersion();
	if (oldVersion != newVersion)
		ereport(FATAL,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("cluster is expaneded from version %d to %d, "
						"catalog changes are disallowed",
						oldVersion, newVersion)));
}

static const char *
phase0_status_from_code(int code)
{
	Assert(code == GP_EXPAND_STATUS_PHASE0);

	return "NOT EXPANSION";
}

static const char *
phase1_status_from_code(int code)
{
	Assert(code > GP_EXPAND_STATUS_PHASE1);
	Assert(code <= GP_EXPAND_STATUS_PHASE1_UNKNOWN);

	return phase1_status[code - GP_EXPAND_STATUS_PHASE1 - 1];
}

static const char *
phase2_status_from_code(int code)
{
	Assert(code > GP_EXPAND_STATUS_PHASE2);
	Assert(code <= GP_EXPAND_STATUS_PHASE2_UNKNOWN);

	return phase2_status[code - GP_EXPAND_STATUS_PHASE2 - 1];
}

static int
phase1_status_to_code(const char *status)
{
	int			code;

	foreach_phase1_status_code(code)
	{
		if (strcmp(status, phase1_status_from_code(code)) == 0)
			return code;
	}

	return GP_EXPAND_STATUS_PHASE1_UNKNOWN;
}

static int
phase2_status_to_code(const char *status)
{
	int			code;

	foreach_phase2_status_code(code)
	{
		if (strcmp(status, phase2_status_from_code(code)) == 0)
			return code;
	}

	return GP_EXPAND_STATUS_PHASE2_UNKNOWN;
}

/*
 * Parse one line from gpexpand status file.
 *
 * Note that the line argument will be modified during the parsing.
 */
static void
parse_phase1_status_line(_ExpandStatus *status, char *line)
{
	char	   *sep;

	/* The format of status line is "KEY:VALUE\n" */
	sep = strchr(line, ':');
	if (!sep)
	{
		/* ':' is not found, not a valid status line */

		status->code = GP_EXPAND_STATUS_PHASE1_UNKNOWN;
		status->status = phase1_status_from_code(status->code);
		status->detail = "invalid status file";
		return;
	}

	/* Separate the KEY and VALUE parts */
	*sep++ = '\0';

	/* line points to KEY */
	status->code = phase1_status_to_code(line);
	status->status = phase1_status_from_code(status->code);

	/* Strip trailing \r or \n */
	line = sep;
	sep = strpbrk(sep, "\r\n");
	if (sep)
		*sep = '\0';

	/* line points to VALUE */
	status->detail = pstrdup(line);
}

static void
get_phase1_status(_ExpandStatus *status)
{
	get_phase1_status_from_file(status, "gpexpand.status");
}

static void
get_phase2_status(_ExpandStatus *status)
{
	RangeVar   *rv;
	Relation	rel;

	/*
	 * Must check for existance of the schema table and acquire lock on it
	 * before querying.
	 */
	rv = makeRangeVar("gpexpand", "status", -1);
	rel = heap_openrv_extended(rv, AccessShareLock, true);

	if (RelationIsValid(rel))
	{
		/*
		 * The schema table exists in current database, we could load detailed
		 * status information from it.
		 */
		get_phase2_status_from_schema(status);
		heap_close(rel, NoLock);
	}
	else
	{
		/*
		 * The schema table does not exist in current database, but we have to
		 * further distinct two situations:
		 *
		 * - gpexpand is not started or already finished;
		 * - the table is created in other database;
		 *
		 * We can distinct it by checking the existance of the status file.
		 */
		get_phase2_status_from_file(status);
	}
}

static void
get_phase1_status_from_file(_ExpandStatus *status, const char *filename)
{
	StringInfo	fullname = makeStringInfo();
	FILE	   *fstatus;

	appendStringInfo(fullname, "%s/%s", data_directory, filename);

	while (!(fstatus = fopen(fullname->data, "r")) && errno == EINTR) ;

	if (fstatus)
	{
		char		line[MAXPGPATH * 2];

		/* status file exists, parse the information from it */

		/* find the last line */
		while (fgets(line, sizeof(line), fstatus)) ;
		fclose(fstatus);

		parse_phase1_status_line(status, line);
	}
	else
	{
		/* status file does not exist */

		status->code = GP_EXPAND_STATUS_PHASE0;
		status->status = phase0_status_from_code(status->code);
		status->detail = "";
	}
}

static void
get_phase2_status_from_file(_ExpandStatus *status)
{
	/*
	 * Both phases share the same status file format, so reuse the parse logic.
	 */
	get_phase1_status_from_file(status, "gpexpand.phase2.status");

	/*
	 * The last step of initialization phase is "EXPANSION_PREPARE_DONE", it
	 * also contains the database name where the schema tables are created.
	 */
	if (status->code == GP_EXPAND_STATUS_PHASE1_EXPANSION_PREPARE_DONE)
	{
		StringInfo	detail = makeStringInfo();

		appendStringInfo(detail,
						 "detailed phase2 information are only available in database \"%s\"",
						 status->detail);

		status->detail = detail->data;
	}

	/*
	 * As long as the status file exists we know it is redistribution phase,
	 * but nothing more.
	 */
	if (status->code != GP_EXPAND_STATUS_PHASE0)
	{
		status->code = GP_EXPAND_STATUS_PHASE2_UNKNOWN;
		status->status = phase2_status_from_code(status->code);
	}
}

static void
get_phase2_status_from_schema(_ExpandStatus *status)
{
	CdbPgResults results = { NULL, 0 };
	Datum		recent_updated_datum;
	const char *recent_status_str = NULL;
	int			i;

	/*
	 * Dispatch the query to segments to collect the progress information.  The
	 * schema table must be locked before this.
	 */
	CdbDispatchCommand("SELECT status, updated "
					   "  FROM gpexpand.status "
					   " ORDER BY updated DESC "
					   " LIMIT 1",
					   DF_WITH_SNAPSHOT, &results);

	for (i = 0; i < results.numResults; i++)
	{
		struct pg_result *result = results.pg_results[i];
		Datum		updated_datum;
		const char *updated_str;
		const char *status_str;

		Assert(PQresultStatus(result) == PGRES_TUPLES_OK);

		if (PQntuples(result) < 1)
			continue;

		Assert(PQntuples(result) == 1);

		status_str = PQgetvalue(result, 0, 0);
		updated_str = PQgetvalue(result, 0, 1);

		updated_datum = DirectFunctionCall3(timestamp_in,
											CStringGetDatum(updated_str),
											ObjectIdGetDatum(InvalidOid),
											Int32GetDatum(-1));

		/* We need to find out the most recent status from all the segments */
		if (!recent_status_str ||
			DatumGetBool(DirectFunctionCall2(timestamp_lt,
											 recent_updated_datum,
											 updated_datum)))
		{
			recent_updated_datum = updated_datum;
			recent_status_str = status_str;

			Assert(recent_status_str);
		}
	}

	if (recent_status_str)
	{
		/* Found the most recent status */
		status->status = pstrdup(recent_status_str);
		status->code = phase2_status_to_code(status->status);

		/* Now try to load the progress information */
		get_phase2_progress_from_schema(status);
	}
	else
	{
		/*
		 * No status is found, this should not happend, but anyway mark it as
		 * unknown redistribution phase.
		 */
		status->code = GP_EXPAND_STATUS_PHASE2_UNKNOWN;
		status->status = phase2_status_from_code(status->code);
		status->detail = "";
	}

	cdbdisp_clearCdbPgResults(&results);
}

static void
get_phase2_progress_from_schema(_ExpandStatus *status)
{
	RangeVar   *rv;
	Relation	rel;
	CdbPgResults results = { NULL, 0 };
	StringInfo	progress = makeStringInfo();
	int			total = 0;
	int			completed = 0;
	int			i;
	int			j;

	/*
	 * We have checked for the gpexpand.status schema table, for
	 * gpexpand.status_detail we must also check and lock it.
	 */
	rv = makeRangeVar("gpexpand", "status_detail", -1);
	rel = heap_openrv_extended(rv, AccessShareLock, true);

	if (!RelationIsValid(rel))
	{
		status->detail = "progress: unknown";
		return;
	}

	CdbDispatchCommand("SELECT status, count(status) "
					   "  FROM gpexpand.status_detail "
					   " GROUP BY status",
					   DF_WITH_SNAPSHOT, &results);

	for (i = 0; i < results.numResults; i++)
	{
		struct pg_result *result = results.pg_results[i];

		Assert(PQresultStatus(result) == PGRES_TUPLES_OK);

		for (j = 0; j < PQntuples(result); j++)
		{
			const char *status_str = PQgetvalue(result, j, 0);
			int			count;

			Assert(!PQgetisnull(result, j, 1));
			count = atoi(PQgetvalue(result, j, 1));

			/* Count the total and completed tables */
			total += count;
			if (strcmp(status_str, "COMPLETED") == 0)
				completed += count;
		}
	}

	cdbdisp_clearCdbPgResults(&results);

	appendStringInfo(progress,
					 "progress: %d of %d completed",
					 completed, total);
	status->detail = progress->data;
}
