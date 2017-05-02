#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../tablecmds.c"

/*
 * Check that two segno lists have the same values.
 */
static int check_segno_list(const List* value, const List* check_value)
{
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
void
test__column_to_scan(void **state)
{
	AOCSFileSegInfo *segInfos[4];
	int numcols = 3;
	int col;

	/* Valid segment, col=0 is the smallest */
	segInfos[0] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[0]->segno = 2;
	segInfos[0]->total_tupcount = 51;
	segInfos[0]->state = AOSEG_STATE_DEFAULT;
	segInfos[0]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[0]->vpinfo.entry[0].eof = 120;
	segInfos[0]->vpinfo.entry[0].eof_uncompressed = 200;
	segInfos[0]->vpinfo.entry[1].eof = 100;
	segInfos[0]->vpinfo.entry[1].eof_uncompressed = 100;
	segInfos[0]->vpinfo.entry[2].eof = 320;
	segInfos[0]->vpinfo.entry[2].eof_uncompressed = 400;

	/* Valid segment, col=2 is the smallest */
	segInfos[1] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[1]->segno = 1;
	segInfos[1]->state = AOSEG_STATE_USECURRENT;
	segInfos[1]->total_tupcount = 135;
	segInfos[1]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[1]->vpinfo.entry[0].eof = 200;
	segInfos[1]->vpinfo.entry[0].eof_uncompressed = 250;
	segInfos[1]->vpinfo.entry[1].eof = 500;
	segInfos[1]->vpinfo.entry[1].eof_uncompressed = 650;
	segInfos[1]->vpinfo.entry[2].eof = 100;
	segInfos[1]->vpinfo.entry[2].eof_uncompressed = 120;

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

	List *drop_segno_list = NIL;
	drop_segno_list = lappend_int(drop_segno_list, 3);
	Gp_role = GP_ROLE_EXECUTE;
	RelationData reldata;
	expect_value(AOCSDrop, aorel, &reldata);
	expect_check(AOCSDrop, compaction_segno, check_segno_list, drop_segno_list);
	will_be_called(AOCSDrop);

	/* Empty segment, should be skipped over */
	segInfos[3] = (AOCSFileSegInfo *)
			malloc(sizeof(AOCSFileSegInfo) + sizeof(AOCSVPInfoEntry)*numcols);
	segInfos[3]->segno = 3;
	segInfos[3]->state = AOSEG_STATE_DEFAULT;
	segInfos[3]->total_tupcount = 0;
	segInfos[3]->vpinfo.nEntry = 3; /* number of columns */
	segInfos[3]->vpinfo.entry[0].eof = 200;
	segInfos[3]->vpinfo.entry[0].eof_uncompressed = 200;
	segInfos[3]->vpinfo.entry[1].eof = 100;
	segInfos[3]->vpinfo.entry[1].eof_uncompressed = 165;
	segInfos[3]->vpinfo.entry[2].eof = 50;
	segInfos[3]->vpinfo.entry[2].eof_uncompressed = 85;

	/* Column 0 (vpe index 0) is the smallest (total eof = 120 + 200) */
	col = column_to_scan(segInfos, 4, numcols, &reldata);
	assert_int_equal(col, 0);
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
