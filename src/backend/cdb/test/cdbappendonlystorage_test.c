#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbappendonlystorage.c"

static const XLogRecPtr InvalidXLogRecPtr = {0, 0};

/*
 * Test that calling appendonly_redo with XLOG_APPENDONLY_INSERT record will
 * call ao_insert_replay.
 */
void
test_appendonly_redo_insert_replay(void **state)
{
	RelFileNode relfilenode;
	XLogRecord record;
	XLogRecord *mockrecord;
	xl_ao_insert xlaoinsert;
	char *buffer;

	/* create mock transaction log */
	relfilenode.spcNode = DEFAULTTABLESPACE_OID;
	relfilenode.dbNode = 12087 /* postgres database */;
	relfilenode.relNode = FirstNormalObjectId;

	xlaoinsert.target.node = relfilenode;
	xlaoinsert.target.segment_filenum = 2;
	xlaoinsert.target.offset = 12345;

	record.xl_info = XLOG_APPENDONLY_INSERT;
	record.xl_rmid = RM_APPEND_ONLY_ID;
	record.xl_len = 0;

	buffer = (char *) malloc(SizeOfXLogRecord + SizeOfAOInsert);
	memcpy(buffer, &record, SizeOfXLogRecord);
	memcpy(&buffer[SizeOfXLogRecord], &xlaoinsert, SizeOfAOInsert);
	mockrecord = (XLogRecord *) buffer;

	/* pretend we are a standby */
	will_return(IsStandbyMode, true);

	/* we expect ao_insert_replay to be called with XLOG_APPENDONLY_INSERT record type */
	expect_any(ao_insert_replay, record);
	will_be_called(ao_insert_replay);

	/* run test */
	appendonly_redo(InvalidXLogRecPtr, InvalidXLogRecPtr, mockrecord);
}

/*
 * Test that calling appendonly_redo with XLOG_APPENDONLY_TRUNCATE record will
 * call ao_truncate_replay.
 */
void
test_appendonly_redo_truncate_replay(void **state)
{
	RelFileNode relfilenode;
	XLogRecord record;
	XLogRecord *mockrecord;
	xl_ao_truncate xlaotruncate;
	char *buffer;

	/* create mock transaction log */
	relfilenode.spcNode = DEFAULTTABLESPACE_OID;
	relfilenode.dbNode = 12087 /* postgres database */;
	relfilenode.relNode = FirstNormalObjectId;

	xlaotruncate.target.node = relfilenode;
	xlaotruncate.target.segment_filenum = 2;
	xlaotruncate.target.offset = 12345;

	record.xl_info = XLOG_APPENDONLY_TRUNCATE;
	record.xl_rmid = RM_APPEND_ONLY_ID;
	record.xl_len = 0;

	buffer = (char *) malloc(SizeOfXLogRecord + sizeof(xl_ao_truncate));
	memcpy(buffer, &record, SizeOfXLogRecord);
	memcpy(&buffer[SizeOfXLogRecord], &xlaotruncate, sizeof(xl_ao_truncate));
	mockrecord = (XLogRecord *) buffer;

	/* pretend we are a standby */
	will_return(IsStandbyMode, true);

	/* we expect ao_truncate_replay to be called with XLOG_APPENDONLY_TRUNCATE record type */
	expect_any(ao_truncate_replay, record);
	will_be_called(ao_truncate_replay);

	/* run test */
	appendonly_redo(InvalidXLogRecPtr, InvalidXLogRecPtr, mockrecord);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_appendonly_redo_insert_replay),
		unit_test(test_appendonly_redo_truncate_replay)
	};
	return run_tests(tests);
}
