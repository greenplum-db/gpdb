/*
 * src/test/fdw/extended_protocol_commit_test_fdw.c
 *
 *
 * extended_protocol_commit_test_fdw.c
 *
 *		A foreign data wrapper for testing of its interaction with FDW.
 */

#include "postgres.h"

#include "executor/spi.h"
#include "foreign/fdwapi.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "utils/snapmgr.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


#define TEST_TABLE_NAME "extended_protocol_commit_test_table_for_create_drop"


/*
 * Execute the given query via SPI
 */
static void
test_execute_spi_expression(const char *query)
{
	int			r;

	PushActiveSnapshot(GetTransactionSnapshot());

	PG_TRY();
	{
		if (SPI_connect() != SPI_OK_CONNECT)
			elog(ERROR, "Failed to connect to SPI");

		r = SPI_execute(query, false, 0);
		if (r < 0)
			elog(ERROR, "Failed to execute '%s' via SPI: %s [%d]", query, SPI_result_code_string(r), r);
	}
	PG_CATCH();
	{
		SPI_finish();

		PopActiveSnapshot();

		PG_RE_THROW();
	}
	PG_END_TRY();

	SPI_finish();

	PopActiveSnapshot();
}

static void
test_create_table_via_spi(void)
{
	StringInfo	query;

	query = makeStringInfo();
	appendStringInfo(query, "CREATE TABLE %s(i INT, t TEXT) DISTRIBUTED RANDOMLY;", TEST_TABLE_NAME);

	test_execute_spi_expression(query->data);

	pfree(query->data);
	pfree(query);
}

static void
test_drop_table_via_spi(void)
{
	StringInfo	query;

	query = makeStringInfo();
	appendStringInfo(query, "DROP TABLE %s;", TEST_TABLE_NAME);

	test_execute_spi_expression(query->data);

	pfree(query->data);
	pfree(query);
}

static void
extended_protocol_commit_test_fdw_GetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	test_create_table_via_spi();

	baserel->rows = 1;
	baserel->fdw_private = NULL;
}

static void
extended_protocol_commit_test_fdw_GetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	add_path(baserel, (Path *) create_foreignscan_path(
													   root,
													   baserel,
													   NULL,
													   0,
													   0,
													   0,
													   NIL,
													   NULL,
													   NULL,
													   NIL
													   ));
}

static ForeignScan *
extended_protocol_commit_test_fdw_GetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan)
{
	return make_foreignscan(
							tlist,
							extract_actual_clauses(scan_clauses, false),
							baserel->relid,
							NIL,
							NULL,
							NIL,
							NIL,
							NULL
		);
}

static void
extended_protocol_commit_test_fdw_BeginForeignScan(ForeignScanState *node, int eflags)
{
}

static TupleTableSlot *
extended_protocol_commit_test_fdw_IterateForeignScan(ForeignScanState *node)
{
	return ExecClearTuple(node->ss.ss_ScanTupleSlot);
}

static void
extended_protocol_commit_test_fdw_ReScanForeignScan(ForeignScanState *node)
{
}

static void
extended_protocol_commit_test_fdw_EndForeignScan(ForeignScanState *node)
{
	test_drop_table_via_spi();
}

PG_FUNCTION_INFO_V1(extended_protocol_commit_test_fdw_handler);
Datum
extended_protocol_commit_test_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *result = makeNode(FdwRoutine);

	result->GetForeignRelSize = extended_protocol_commit_test_fdw_GetForeignRelSize;
	result->GetForeignPaths = extended_protocol_commit_test_fdw_GetForeignPaths;
	result->GetForeignPlan = extended_protocol_commit_test_fdw_GetForeignPlan;
	result->BeginForeignScan = extended_protocol_commit_test_fdw_BeginForeignScan;
	result->IterateForeignScan = extended_protocol_commit_test_fdw_IterateForeignScan;
	result->ReScanForeignScan = extended_protocol_commit_test_fdw_ReScanForeignScan;
	result->EndForeignScan = extended_protocol_commit_test_fdw_EndForeignScan;

	PG_RETURN_POINTER(result);
}
