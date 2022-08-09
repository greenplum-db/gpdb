#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../tablecmds.c"

/*
 * Check that two segno lists have the same values.
 */
static int check_segno_list(const LargestIntegralType arg1, const LargestIntegralType arg2)
{
	const List *value = (const List *)arg1; 
	const List *check_value = (const List *)arg2;
	List *compare;

	if (list_length(value) != list_length(check_value))
		return false;

	compare = list_union_int(value, check_value);
	return list_length(compare) == list_length(value);
}

/*
 * Ensure that the column having the smallest on-disk segfile is
 * chosen for headerscan during ALTER TABLE ADD COLUMN operation.
 */
static void
test__column_to_scan(void **state)
{
	List *drop_segno_list = NIL;
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

	/* AOCSDrop should be called with segno 3 to drop */
	drop_segno_list = lappend_int(drop_segno_list, 3);
	Gp_role = GP_ROLE_EXECUTE;
	expect_value(AOCSDrop, aorel, &reldata);
	expect_check(AOCSDrop, compaction_segno, check_segno_list, drop_segno_list);
	will_be_called(AOCSDrop);

	/* Column 1 (vpe index 1) has the smallest eof */
	col = column_to_scan(segInfos, 4, numcols, &reldata);
	assert_int_equal(col, 1);
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__column_to_scan)
	};

	MemoryContextInit();

	return run_tests(tests);
}
