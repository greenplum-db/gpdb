#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbmirroredappendonly.c"

static int
check_relfilenode_function(const RelFileNode *value, const RelFileNode *check_value)
{
	return RelFileNodeEquals(*value, *check_value);
}

/*
 * Test that XLogAOSegmentFile will be called when we cannot find the AO
 * segment file.
 */
static void
ao_invalid_segment_file_test(uint8 xl_info)
{
	RelFileNode relfilenode;
	XLogRecord record;
	XLogRecord *mockrecord;
	xl_ao_target xlaotarget;
	char *buffer = NULL;

	/* create mock transaction log */
	relfilenode.spcNode = DEFAULTTABLESPACE_OID;
	relfilenode.dbNode = 12087 /* postgres database */;
	relfilenode.relNode = FirstNormalObjectId;

	xlaotarget.node = relfilenode;
	xlaotarget.segment_filenum = 2;
	xlaotarget.offset = 12345;

	record.xl_info = xl_info;
	record.xl_rmid = RM_APPEND_ONLY_ID;
	record.xl_len = 0;

	if (xl_info == XLOG_APPENDONLY_INSERT)
	{
		xl_ao_insert xlaoinsert;

		xlaoinsert.target = xlaotarget;
		buffer = (char *) malloc(SizeOfXLogRecord + SizeOfAOInsert);
		memcpy(&buffer[SizeOfXLogRecord], &xlaoinsert, SizeOfAOInsert);
	}
	else if (xl_info == XLOG_APPENDONLY_TRUNCATE)
	{
		xl_ao_truncate xlaotruncate;

		xlaotruncate.target = xlaotarget;
		buffer = (char *) malloc(SizeOfXLogRecord + sizeof(xl_ao_truncate));
		memcpy(&buffer[SizeOfXLogRecord], &xlaotruncate, sizeof(xl_ao_truncate));
	}

	memcpy(buffer, &record, SizeOfXLogRecord);
	mockrecord = (XLogRecord *) buffer;

	/* mock to not find AO segment file */
	expect_any(PathNameOpenFile, fileName);
	expect_any(PathNameOpenFile, fileFlags);
	expect_any(PathNameOpenFile, fileMode);
	will_return(PathNameOpenFile, -1);

	/* XLogAOSegmentFile should be called with our mock relfilenode and segment file number */
	expect_check(XLogAOSegmentFile, &rnode, check_relfilenode_function, &relfilenode);
	expect_value(XLogAOSegmentFile, segmentFileNum, xlaotarget.segment_filenum);
	will_be_called(XLogAOSegmentFile);

	/* run test */
	if (xl_info == XLOG_APPENDONLY_INSERT)
		ao_insert_replay(mockrecord);
	else if (xl_info == XLOG_APPENDONLY_TRUNCATE)
		ao_truncate_replay(mockrecord);
}

/*
 * Test that ao_insert_replay will call XLogAOSegmentFile when we cannot find
 * the AO segment file.
 */
void
test_ao_insert_replay_invalid_segment_file(void **state)
{
	ao_invalid_segment_file_test(XLOG_APPENDONLY_INSERT);
}

/*
 * Test that ao_truncate_replay will call XLogAOSegmentFile when we cannot find
 * the AO segment file.
 */
void
test_ao_truncate_replay_invalid_segment_file(void **state)
{
	ao_invalid_segment_file_test(XLOG_APPENDONLY_TRUNCATE);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_ao_insert_replay_invalid_segment_file),
		unit_test(test_ao_truncate_replay_invalid_segment_file)
	};

	return run_tests(tests);
}
