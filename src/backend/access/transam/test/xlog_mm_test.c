#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlog_mm.c"

static const XLogRecPtr InvalidXLogRecPtr = {0, 0};

static int
check_relfilenode_function(const RelFileNode *value, const RelFileNode *check_value)
{
	return RelFileNodeEquals(*value, *check_value);
}

/*
 * Test that an MMXLOG_REMOVE_APPENDONLY_FILE xlog record will call
 * XLogAODropSegmentFile.
 */
void
test_mmxlog_redo_mmxlog_remove_file_ao(void **state)
{
	RelFileNode relfilenode;
	XLogRecord record;
	XLogRecord *mockrecord;
	xl_mm_fs_obj xlmmfsobj;
	char *buffer;

	/* Set our dbid */
	GpIdentity.dbid = 4;

	/* create mock relfilenode */
	relfilenode.spcNode = DEFAULTTABLESPACE_OID;
	relfilenode.dbNode = 12087 /* postgres database */;
	relfilenode.relNode = FirstNormalObjectId;

	/* create mock transaction log */
	xlmmfsobj.objtype = MM_OBJ_RELFILENODE;
	xlmmfsobj.filespace = SYSTEMFILESPACE_OID;
	xlmmfsobj.tablespace = relfilenode.spcNode;
	xlmmfsobj.database = relfilenode.dbNode;
	xlmmfsobj.relfilenode = relfilenode.relNode;
	xlmmfsobj.segnum = 2;

	xlmmfsobj.u.dbid.master = 1;
	xlmmfsobj.u.dbid.mirror = GpIdentity.dbid;

	sprintf(xlmmfsobj.master_path, "./base/%d/%d", xlmmfsobj.database, xlmmfsobj.relfilenode);
	sprintf(xlmmfsobj.mirror_path, "./base/%d/%d", xlmmfsobj.database, xlmmfsobj.relfilenode);

	record.xl_info = MMXLOG_REMOVE_APPENDONLY_FILE;
	record.xl_rmid = RM_MMXLOG_ID;
	record.xl_len = 0;

	buffer = (char *) malloc(SizeOfXLogRecord + sizeof(xl_mm_fs_obj));
	memcpy(buffer, &record, SizeOfXLogRecord);
	memcpy(&buffer[SizeOfXLogRecord], &xlmmfsobj, sizeof(xl_mm_fs_obj));
	mockrecord = (XLogRecord *) buffer;

	/* Make sure we are in standby mode for any AO xlog replays */
	will_return(IsStandbyMode, true);

	/* XLogAODropSegmentFile should be called with our mock relfilenode and segment file number */
	expect_check(XLogAODropSegmentFile, &rnode, check_relfilenode_function, &relfilenode);
	expect_value(XLogAODropSegmentFile, segmentFileNum, xlmmfsobj.segnum);
	will_be_called(XLogAODropSegmentFile);

	/* mock MirroredAppendOnly_Drop to skip */
	expect_check(MirroredAppendOnly_Drop, relFileNode, check_relfilenode_function, &relfilenode);
	expect_value(MirroredAppendOnly_Drop, segmentFileNum, xlmmfsobj.segnum);
	expect_value(MirroredAppendOnly_Drop, relationName, NULL);
	expect_value(MirroredAppendOnly_Drop, primaryOnly, true);
	expect_not_value(MirroredAppendOnly_Drop, primaryError, NULL);
	expect_not_value(MirroredAppendOnly_Drop, mirrorDataLossOccurred, NULL);
	will_be_called(MirroredAppendOnly_Drop);

	/* test that mmxlog_redo properly calls XLogAODropSegmentFile */
	mmxlog_redo(InvalidXLogRecPtr, InvalidXLogRecPtr, mockrecord);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_mmxlog_redo_mmxlog_remove_file_ao)
	};
	return run_tests(tests);
}
