#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#include "utils/memutils.h"
#include "access/xlog.h"
#include "pgtime.h"

/* Actual function body */
#include "../xactdesc.c"

static void
test_xactdescprepareCommit(void **state)
{
	StringInfo buf = makeStringInfo();

	XLogReaderState *record = palloc(sizeof(XLogReaderState));
	XLogRecGetData(record) = palloc(sizeof(TwoPhaseFileHeader));
	record->decoded_record = palloc(sizeof(TwoPhaseFileHeader));

	XLogRecGetInfo(record) = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(record);

	tpfh->prepared_at = 617826371830030;
	tpfh->tablespace_oid_to_delete_on_commit = 42;
	tpfh->tablespace_oid_to_delete_on_abort = InvalidOid;

	/* Can not use save_state() here, so emulate it */
	tpfh->gidlen = strlen("4242424242-0000000042") + 1;
	strcpy((char *)tpfh + sizeof(*tpfh), "4242424242-0000000042");

	xact_desc(buf, record);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042; tablespace_oid_to_delete_on_commit = 42", buf->data);
}

static void
test_xactdescprepareAbort(void **state)
{
	StringInfo buf = makeStringInfo();

	XLogReaderState *record = palloc(sizeof(XLogReaderState));
	XLogRecGetData(record) = palloc(sizeof(TwoPhaseFileHeader));
	record->decoded_record = palloc(sizeof(TwoPhaseFileHeader));

	XLogRecGetInfo(record) = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(record);

	tpfh->prepared_at = 617826371830030;
	tpfh->tablespace_oid_to_delete_on_commit = InvalidOid;
	tpfh->tablespace_oid_to_delete_on_abort = 42;

	/* Can not use save_state() here, so emulate it */
	tpfh->gidlen = strlen("4242424242-0000000042") + 1;
	strcpy((char *)tpfh + sizeof(*tpfh), "4242424242-0000000042");

	xact_desc(buf, record);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042; tablespace_oid_to_delete_on_abort = 42", buf->data);
}

static void
test_xactdescprepareNone(void **state)
{
	StringInfo buf = makeStringInfo();

	XLogReaderState *record = palloc(sizeof(XLogReaderState));
	XLogRecGetData(record) = palloc(sizeof(TwoPhaseFileHeader));
	record->decoded_record = palloc(sizeof(TwoPhaseFileHeader));

	XLogRecGetInfo(record) = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(record);

	tpfh->prepared_at = 617826371830030;
	tpfh->tablespace_oid_to_delete_on_commit = InvalidOid;
	tpfh->tablespace_oid_to_delete_on_abort = InvalidOid;

	/* Can not use save_state() here, so emulate it */
	tpfh->gidlen = strlen("4242424242-0000000042") + 1;
	strcpy((char *)tpfh + sizeof(*tpfh), "4242424242-0000000042");

	xact_desc(buf, record);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042", buf->data);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	MemoryContextInit();
	pg_timezone_initialize();

	const UnitTest tests[] = {
		unit_test(test_xactdescprepareCommit),
		unit_test(test_xactdescprepareAbort),
		unit_test(test_xactdescprepareNone)
	};

	return run_tests(tests);
}
