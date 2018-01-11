#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../aocsam.c"

/*
 * aocs_begin_headerscan()
 *
 * Verify that we are setting correct storage attributes (no
 * compression, no checksum) for scanning an existing column in ALTER
 * TABLE ADD COLUMN case.
 */
void
test__aocs_begin_headerscan(void **state)
{
	AOCSHeaderScanDesc desc;
	RelationData reldata;
	FormData_pg_appendonly pgappendonly;

	pgappendonly.checksum = true;
	reldata.rd_appendonly = &pgappendonly;
	FormData_pg_class pgclass;

	reldata.rd_rel = &pgclass;
	StdRdOptions opt;

	opt.blocksize = 8192 * 5;
	StdRdOptions *opts[1];

	opts[0] = &opt;

	strncpy(&pgclass.relname.data[0], "mock_relation", 13);
	expect_value(RelationGetAttributeOptions, rel, &reldata);
	will_return(RelationGetAttributeOptions, &opts);
	expect_any(AppendOnlyStorageRead_Init, storageRead);
	expect_any(AppendOnlyStorageRead_Init, memoryContext);
	expect_any(AppendOnlyStorageRead_Init, maxBufferLen);
	expect_any(AppendOnlyStorageRead_Init, relationName);
	expect_any(AppendOnlyStorageRead_Init, title);
	expect_any(AppendOnlyStorageRead_Init, storageAttributes);

	/*
	 * AppendOnlyStorageRead_Init assigns storageRead->storageAttributes.
	 * will_assign_*() functions mandate a paramter as an argument.  Here we
	 * want to set selective members of a parameter.  I don't know how this
	 * can be achieved using cmockery.  This test will be meaningful only when
	 * we are able to set storageAttributes member of desc.ao_read.
	 */
	will_be_called(AppendOnlyStorageRead_Init);
	desc = aocs_begin_headerscan(&reldata, 0);
	assert_false(desc->ao_read.storageAttributes.compress);
	assert_int_equal(desc->colno, 0);
}


void
test__aocs_addcol_init(void **state)
{
	AOCSAddColumnDesc desc;
	RelationData reldata;
	FormData_pg_appendonly pgappendonly;
	int			nattr = 5;
	StdRdOptions **opts =
	(StdRdOptions **) malloc(sizeof(StdRdOptions *) * nattr);

	/* 3 existing columns */
	opts[0] = opts[1] = opts[2] = (StdRdOptions *) NULL;

	/* 2 newly added columns */
	opts[3] = (StdRdOptions *) malloc(sizeof(StdRdOptions));
	opts[3]->compresstype = "rle_type";
	opts[3]->compresslevel = 2;
	opts[3]->blocksize = 8192;
	opts[4] = (StdRdOptions *) malloc(sizeof(StdRdOptions));
	opts[4]->compresstype = "none";
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
	expect_value(create_datumstreamwrite, checksum, true);
	expect_value(create_datumstreamwrite, checksum, true);
	expect_value(create_datumstreamwrite, safeFSWriteSize, 0);
	expect_value(create_datumstreamwrite, safeFSWriteSize, 0);
	expect_value(create_datumstreamwrite, maxsz, 8192);
	expect_value(create_datumstreamwrite, maxsz, 8192 * 2);
	expect_any(create_datumstreamwrite, attr);
	expect_any(create_datumstreamwrite, attr);
	expect_any(create_datumstreamwrite, relname);
	expect_any(create_datumstreamwrite, relname);
	expect_any(create_datumstreamwrite, title);
	expect_any(create_datumstreamwrite, title);
	will_return_count(create_datumstreamwrite, NULL, 2);

	pgappendonly.checksum = true;
	reldata.rd_appendonly = &pgappendonly;
	reldata.rd_att = (TupleDesc) malloc(sizeof(struct tupleDesc));
	reldata.rd_att->attrs =
		(Form_pg_attribute *) malloc(sizeof(Form_pg_attribute *) * nattr);
	memset(reldata.rd_att->attrs, 0, sizeof(Form_pg_attribute *) * nattr);
	reldata.rd_att->natts = 5;
	/* 3 existing columns, 2 new columns */
	desc = aocs_addcol_init(&reldata, 2);
	assert_int_equal(desc->num_newcols, 2);
	assert_int_equal(desc->cur_segno, -1);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__aocs_begin_headerscan),
		unit_test(test__aocs_addcol_init)
	};

	MemoryContextInit();

	return run_tests(tests);
}
