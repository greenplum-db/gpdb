#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../aocsam.c"

/*
 * aocs_begin_headerscan()
 *
 * Verify that we are setting correct storage attributes (no compression) for
 * scanning an existing column in ALTER TABLE ADD COLUMN case.
 */
static void
test__aocs_begin_headerscan(void **state)
{
	AOCSHeaderScanDesc desc;
	RelationData reldata;
	FormData_pg_class pgclass;
	int nattr = 1;

	reldata.rd_rel = &pgclass;
	reldata.rd_id = 12345;
	reldata.rd_rel->relnatts = nattr;
	reldata.rd_att = (TupleDesc) palloc(sizeof(TupleDescData) +
										(sizeof(Form_pg_attribute *) * nattr));
	memset(reldata.rd_att->attrs, 0, sizeof(Form_pg_attribute *) * nattr);
	reldata.rd_att->natts = nattr;

	/* opts and opt will be freed by aocs_begin_headerscan */
	StdRdOptions **opts =
			(StdRdOptions **) palloc(sizeof(StdRdOptions *) * nattr);
	opts[0] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	opts[0]->blocksize = 8192 * 5;

	strncpy(&pgclass.relname.data[0], "mock_relation", 13);
	expect_value(RelationGetAttributeOptions, rel, &reldata);
	will_return(RelationGetAttributeOptions, opts);

	expect_value(GetAppendOnlyEntryAttributes, relid, 12345);
	expect_any(GetAppendOnlyEntryAttributes, blocksize);
	expect_any(GetAppendOnlyEntryAttributes, compresslevel);
	expect_any(GetAppendOnlyEntryAttributes, checksum);
	expect_any(GetAppendOnlyEntryAttributes, compresstype);
	will_be_called(GetAppendOnlyEntryAttributes);

	/*
	 * We used to mock AppendOnlyStorageRead_Init() here, however as the mocked
	 * one does not initialize desc->ao_read.storageAttributes at all, it makes
	 * the following assertion flaky.
	 *
	 * On the other hand aocs_begin_headerscan() itself does not do many useful
	 * things, the actual job is done inside AppendOnlyStorageRead_Init(), so
	 * to make the test more useful we removed the mocking and test against the
	 * real AppendOnlyStorageRead_Init() now.
	 */
	desc = aocs_begin_headerscan(&reldata, 0);
	assert_false(desc->ao_read.storageAttributes.compress);
	assert_int_equal(desc->colno, 0);
}


static void
test__aocs_writecol_init(void **state)
{
	AOCSWriteColumnDesc desc;
	NewColumnValue *newval1 = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
	NewColumnValue *newval2 = (NewColumnValue *) palloc0(sizeof(NewColumnValue));
	RelationData reldata;
	int			nattr = 5;
	StdRdOptions **opts =
	(StdRdOptions **) palloc(sizeof(StdRdOptions *) * nattr);
	wal_level = WAL_LEVEL_REPLICA;
	List *newvals = NIL;

	newval1->attnum = 4;
	newval1->op = AOCSADDCOLUMN;
	newval2->attnum = 5;
	newval2->op = AOCSADDCOLUMN;
	newvals = lappend(newvals, newval1);
	newvals = lappend(newvals, newval2);

	/* 3 existing columns */
	opts[0] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	opts[1] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	opts[2] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	/* 2 newly added columns */
	opts[3] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	strcpy(opts[3]->compresstype, "rle_type");
	opts[3]->compresslevel = 2;
	opts[3]->blocksize = 8192;
	opts[4] = (StdRdOptions *) palloc(sizeof(StdRdOptions));
	strcpy(opts[4]->compresstype, "none");
	opts[4]->compresslevel = 0;
	opts[4]->blocksize = 8192 * 2;

	/* One call to RelationGetAttributeOptions() */
	expect_any(RelationGetAttributeOptions, rel);
	will_return(RelationGetAttributeOptions, opts);

	/* Two calls to create_datumstreamwrite() */
	expect_string(create_datumstreamwrite, compName, "rle_type");
	expect_string(create_datumstreamwrite, compName, "none");
	expect_value(create_datumstreamwrite, compLevel, 2);
	expect_value(create_datumstreamwrite, compLevel, 0);
	expect_any_count(create_datumstreamwrite, checksum, 2);
	expect_value(create_datumstreamwrite, maxsz, 8192);
	expect_value(create_datumstreamwrite, maxsz, 8192 * 2);
	expect_value(create_datumstreamwrite, needsWAL, true);
	expect_value(create_datumstreamwrite, needsWAL, true);
	expect_any_count(create_datumstreamwrite, attr, 2);
	expect_any_count(create_datumstreamwrite, relname, 2);
	expect_any_count(create_datumstreamwrite, title, 2);
	will_return_count(create_datumstreamwrite, NULL, 2);

	FormData_pg_class rel;
	rel.relpersistence = RELPERSISTENCE_PERMANENT;
	reldata.rd_id = 12345;
	reldata.rd_rel = &rel;

	reldata.rd_rel->relnatts = 5;
	reldata.rd_att = (TupleDesc) palloc(sizeof(TupleDescData) +
										(sizeof(Form_pg_attribute *) * nattr));
	memset(reldata.rd_att->attrs, 0, sizeof(Form_pg_attribute *) * nattr);
	reldata.rd_att->natts = 5;

	expect_value(GetAppendOnlyEntryAttributes, relid, 12345);
	expect_any(GetAppendOnlyEntryAttributes, blocksize);
	expect_any(GetAppendOnlyEntryAttributes, compresslevel);
	expect_any(GetAppendOnlyEntryAttributes, checksum);
	expect_any(GetAppendOnlyEntryAttributes, compresstype);
	will_be_called(GetAppendOnlyEntryAttributes);

	/* 3 existing columns, 2 new columns */
	desc = aocs_writecol_init(&reldata, newvals, AOCSADDCOLUMN);
	assert_int_equal(desc->num_cols_to_write, 2);
	assert_int_equal(desc->cur_segno, -1);
}

/*
 * Ensure that the column having the smallest on-disk segfile is
 * chosen for headerscan during ALTER TABLE ADD COLUMN operation.
 */
static void
test__column_to_scan(void **state)
{
	RelationData reldata;
	AOCSFileSegInfo *segInfos[4];
	int numcols = 3;
	int col;

	/* Empty segment, should be skipped over */
	segInfos[0] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[0]->segno = 3;
	segInfos[0]->state = AOSEG_STATE_DEFAULT;
	segInfos[0]->total_tupcount = 0;
	segInfos[0]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[0]->vpinfo.entry[0].eof = 200;
	segInfos[0]->vpinfo.entry[0].eof_uncompressed = 200;
	segInfos[0]->vpinfo.entry[1].eof = 100;
	segInfos[0]->vpinfo.entry[1].eof_uncompressed = 165;
	segInfos[0]->vpinfo.entry[2].eof = 50;
	segInfos[0]->vpinfo.entry[2].eof_uncompressed = 85;

	/* Valid segment, col=1 is the smallest */
	segInfos[1] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[1]->segno = 2;
	segInfos[1]->total_tupcount = 51;
	segInfos[1]->state = AOSEG_STATE_DEFAULT;
	segInfos[1]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[1]->vpinfo.entry[0].eof = 120;
	segInfos[1]->vpinfo.entry[0].eof_uncompressed = 200;
	segInfos[1]->vpinfo.entry[1].eof = 100;
	segInfos[1]->vpinfo.entry[1].eof_uncompressed = 100;
	segInfos[1]->vpinfo.entry[2].eof = 320;
	segInfos[1]->vpinfo.entry[2].eof_uncompressed = 400;

	/* AWATING_DROP segment, should be skipped over */
	segInfos[2] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[2]->segno = 3;
	segInfos[2]->state = AOSEG_STATE_AWAITING_DROP;
	segInfos[2]->total_tupcount = 15;
	segInfos[2]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[2]->vpinfo.entry[0].eof = 141;
	segInfos[2]->vpinfo.entry[0].eof_uncompressed = 200;
	segInfos[2]->vpinfo.entry[1].eof = 51;
	segInfos[2]->vpinfo.entry[1].eof_uncompressed = 65;
	segInfos[2]->vpinfo.entry[2].eof = 20;
	segInfos[2]->vpinfo.entry[2].eof_uncompressed = 80;

	/* Valid segment, col=0 is the smallest */
	segInfos[3] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[3]->segno = 1;
	segInfos[3]->state = AOSEG_STATE_USECURRENT;
	segInfos[3]->total_tupcount = 135;
	segInfos[3]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[3]->vpinfo.entry[0].eof = 60;
	segInfos[3]->vpinfo.entry[0].eof_uncompressed = 80;
	segInfos[3]->vpinfo.entry[1].eof = 500;
	segInfos[3]->vpinfo.entry[1].eof_uncompressed = 650;
	segInfos[3]->vpinfo.entry[2].eof = 100;
	segInfos[3]->vpinfo.entry[2].eof_uncompressed = 120;

	/* Column 1 (vpe index 1) has the smallest eof */
	col = column_to_scan(segInfos, 4, numcols, &reldata);
	assert_int_equal(col, 1);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__aocs_begin_headerscan),
		unit_test(test__aocs_writecol_init),
		unit_test(test__column_to_scan)
	};

	MemoryContextInit();

	return run_tests(tests);
}

