#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"

#include "postgres.h"
#include "utils/relcache.h"
#include "cdb/cdbutil.h"
#include "access/genam.h"
#include "access/relscan.h"
#include "cdb/cdbvars.h"
#include "utils/fmgroids.h"
#include "catalog/indexing.h"
#include "utils/tqual.h"
#include "elog_helper.h"

void
execute_on_master(void);
void
execute_on_segment(void);
void
revert_to_initial_database(void);
void
_setup__access_to_gp_segment_configuration_table(const HeapTupleData *tuple);

void
test__does_segment_exist__when_running_code_in_segment__raises_exception(void)
{
	execute_on_segment();
	expect_elog(ERROR);
	PG_TRY();
			{
				does_segment_exist(123);
				fail_msg("Should not have reached this location");
			}
		PG_CATCH();
			{

			}
	PG_END_TRY();

	revert_to_initial_database();
}

void
test__does_segment_exist__when_segment_does_not_exist__return_false(void)
{
	execute_on_master();
	HeapTuple tuple = NULL;
	_setup__access_to_gp_segment_configuration_table(tuple);

	bool result = does_segment_exist(123);
	assert_false(result);

	revert_to_initial_database();
}

void
test__does_segment_exist__when_segment_does_exists__return_true(void)
{
	execute_on_master();
	HeapTupleData tuple;
	_setup__access_to_gp_segment_configuration_table(&tuple);

	bool result = does_segment_exist(123);
	assert_true(result);

	revert_to_initial_database();
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
		               {
			               unit_test(
				               test__does_segment_exist__when_segment_does_not_exist__return_false),
			               unit_test(
				               test__does_segment_exist__when_segment_does_exists__return_true),
			               unit_test(
				               test__does_segment_exist__when_running_code_in_segment__raises_exception)
		               };

	return run_tests(tests);
}

static int savedSegindex = MASTER_CONTENT_ID;

void
execute_on_master(void)
{
	savedSegindex = GpIdentity.segindex;
	GpIdentity.segindex = MASTER_CONTENT_ID;
}

void
execute_on_segment(void)
{
	savedSegindex = GpIdentity.segindex;
	GpIdentity.segindex = 1;
}

void
revert_to_initial_database(void)
{
	GpIdentity.segindex = savedSegindex;
}

void
_setup__access_to_gp_segment_configuration_table(const HeapTupleData *tuple)
{
	RelationData    heap;
	SysScanDescData scan;

	/* 
	 * Ensure that heap_open is Locked with Share access 
	 * to the table gp_segment_configuration
	 */
	expect_value(heap_open, relationId, GpSegmentConfigRelationId);
	expect_value(heap_open, lockmode, AccessShareLock);
	will_return(heap_open, &heap);

	/*
	 * The next setup will prepare a Scan Key Data that will result in the following
	 * SQL to be executed:
	 * SELECT * FROM gp_segment_configuration WHERE dbid = 123
	 *
	 * Where 123 is the value that is passed to the does_segment_exist function
	 */
	expect_any(ScanKeyInit, entry);
	expect_value(ScanKeyInit,
	             attributeNumber,
	             Anum_gp_segment_configuration_dbid);
	expect_value(ScanKeyInit, strategy, BTEqualStrategyNumber);
	expect_value(ScanKeyInit, procedure, F_INT2EQ);
	expect_value(ScanKeyInit, argument, Int16GetDatum(123));
	will_be_called(ScanKeyInit);

	/*
	 * Setup the index scan on the gp_segment_configuration table that was
	 * prepared using the ScanKeyInit
	 */
	expect_value(systable_beginscan, heapRelation, &heap);
	expect_value(systable_beginscan, indexId, GpSegmentConfigDbidIndexId);
	expect_value(systable_beginscan, indexOK, true);
	expect_value(systable_beginscan, snapshot, SnapshotNow);
	expect_value(systable_beginscan, nkeys, 1);
	expect_any(systable_beginscan, key);
	will_return(systable_beginscan, &scan);

	expect_value(systable_getnext, sysscan, &scan);
	will_return(systable_getnext, tuple);

	expect_value(systable_endscan, sysscan, &scan);
	will_be_called(systable_endscan);

	expect_value(relation_close, relation, &heap);
	expect_value(relation_close, lockmode, NoLock);
	will_be_called(relation_close);
}
