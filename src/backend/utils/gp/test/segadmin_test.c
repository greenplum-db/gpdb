#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../segadmin.c"
#include "elog_helper.h"
#include "access/relscan.h"

void
_setup__access_to_gp_segment_configuration_table(const HeapTupleData *tuple);
void
_LocalExceptionalCondition();

void
test__remove_segment__when_mirror_does_not_exist(void)
{
	expect_elog(ERROR);

	expect_value(does_segment_exist, dbid, 10);
	will_return(does_segment_exist, false);

	PG_TRY();
			{
				remove_segment(1, 10);
				fail_msg("Should have raised");
			}
		PG_CATCH();
			{

			}
	PG_END_TRY();
}

void
test__remove_segment__when_mirror_does_exist__error_deleting_mirror(void)
{
	HeapTupleData tuple;
	expect_elog(ERROR);

	expect_value(does_segment_exist, dbid, 10);
	will_return(does_segment_exist, true);
	_setup__access_to_gp_segment_configuration_table(&tuple);

	/*
	 * Execute elog when simple_heap_delete is called
	 */
	expect_any(simple_heap_delete, relation);
	expect_value(simple_heap_delete, tid, &(&tuple)->t_self);
	will_be_called_with_sideeffect(simple_heap_delete,
	                               &_LocalExceptionalCondition,
	                               NULL);

	PG_TRY();
			{
				remove_segment(1, 10);
				fail_msg("Should have raised");
			}
		PG_CATCH();
			{}
	PG_END_TRY();
}

void
test__remove_segment__when_mirror_does_exist_can_delete_the_mirror(void)
{
	HeapTupleData tuple;

	expect_value(does_segment_exist, dbid, 10);
	will_return(does_segment_exist, true);
	_setup__access_to_gp_segment_configuration_table(&tuple);

	/*
	 * Successfully deletes a segment
	 */
	expect_any(simple_heap_delete, relation);
	expect_value(simple_heap_delete, tid, &(&tuple)->t_self);
	will_be_called(simple_heap_delete);

	/*
	 * Second time that the systable_getnext is called
	 */
	expect_any(systable_getnext, sysscan);
	will_return(systable_getnext, NULL);

	/*
	 * Release ScanKeyData and the heap relation lock
	 */
	expect_any(systable_endscan, sysscan);
	will_be_called(systable_endscan);

	expect_any(relation_close, relation);
	expect_value(relation_close, lockmode, NoLock);
	will_be_called(relation_close);

	remove_segment(1, 10);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__remove_segment__when_mirror_does_not_exist),
		unit_test(
			test__remove_segment__when_mirror_does_exist_can_delete_the_mirror),
		unit_test(
			test__remove_segment__when_mirror_does_exist__error_deleting_mirror),
	};
	return run_tests(tests);
}

void
_LocalExceptionalCondition()
{
	elog(ERROR, "Failed deleting the segment");
}

void
_setup__access_to_gp_segment_configuration_table(const HeapTupleData *tuple)
{
	RelationData    heap;
	SysScanDescData scan;

	/*
	 * Ensure that heap_open is Locked with Row Exclusivity access
	 * to the table gp_segment_configuration
	 */
	expect_value(heap_open, relationId, GpSegmentConfigRelationId);
	expect_value(heap_open, lockmode, RowExclusiveLock);
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
	expect_value(ScanKeyInit, argument, Int16GetDatum(10));
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
}
