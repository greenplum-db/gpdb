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

void
test_xactdescprepareCommit(void **state)
{
	StringInfo buf = makeStringInfo();
	XLogRecord *rec = palloc(SizeOfXLogRecord + MAXALIGN(sizeof(TwoPhaseFileHeader)));

	rec->xl_info = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(rec);

	tpfh->prepared_at = 617826371830030;
	strcpy(tpfh->gid, "4242424242-0000000042");
	tpfh->tablespace_oid_to_delete_on_commit = 42;
	tpfh->tablespace_oid_to_delete_on_abort = InvalidOid;

	xact_desc_prepare(buf, rec);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042; tablespace_oid_to_delete_on_commit = 42", buf->data);
}

void
test_xactdescprepareAbort(void **state)
{
	StringInfo buf = makeStringInfo();
	XLogRecord *rec = palloc(SizeOfXLogRecord + MAXALIGN(sizeof(TwoPhaseFileHeader)));

	rec->xl_info = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(rec);

	tpfh->prepared_at = 617826371830030;
	strcpy(tpfh->gid, "4242424242-0000000042");
	tpfh->tablespace_oid_to_delete_on_commit = InvalidOid;
	tpfh->tablespace_oid_to_delete_on_abort = 42;

	xact_desc_prepare(buf, rec);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042; tablespace_oid_to_delete_on_abort = 42", buf->data);
}

void
test_xactdescprepareNone(void **state)
{
	StringInfo buf = makeStringInfo();
	XLogRecord *rec = palloc(SizeOfXLogRecord + MAXALIGN(sizeof(TwoPhaseFileHeader)));

	rec->xl_info = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(rec);

	tpfh->prepared_at = 617826371830030;
	strcpy(tpfh->gid, "4242424242-0000000042");
	tpfh->tablespace_oid_to_delete_on_commit = InvalidOid;
	tpfh->tablespace_oid_to_delete_on_abort = InvalidOid;

	xact_desc_prepare(buf, rec);

	assert_string_equal("at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042", buf->data);
}

void
test_xactdescPrepare(void **state)
{
	StringInfo buf = makeStringInfo();
	XLogRecord *rec = palloc(SizeOfXLogRecord + MAXALIGN(sizeof(TwoPhaseFileHeader)));

	rec->xl_info = XLOG_XACT_PREPARE;
	TwoPhaseFileHeader* tpfh = (TwoPhaseFileHeader*) XLogRecGetData(rec);

	tpfh->prepared_at = 617826371830030;
	strcpy(tpfh->gid, "4242424242-0000000042");
	tpfh->tablespace_oid_to_delete_on_commit = 42;
	tpfh->tablespace_oid_to_delete_on_abort = InvalidOid;

	xact_desc(buf, rec);

	assert_string_equal("prepare: at = 2019-07-30 18:26:11.83003+00; gid = 4242424242-0000000042; tablespace_oid_to_delete_on_commit = 42", buf->data);
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
		unit_test(test_xactdescprepareNone),
		unit_test(test_xactdescPrepare)
	};

	return run_tests(tests);
}
