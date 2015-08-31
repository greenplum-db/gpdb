#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbfilerep.c"

/*
 * Test for MPP-24515
 */
void
test__FileRep_StartChildProcess(void **state)
{
	pid_t		pid;
	FileRepSubProc FileRepSubProcListLocal[FileRepProcessTypeMirrorVerification+1];

	FileRepSubProcList = &FileRepSubProcListLocal;
	memcpy(FileRepSubProcList, FileRepSubProcListInitial,
			sizeof(FileRepSubProcListInitial));

	will_return(fork_process, -1);
	will_be_called(RedZoneHandler_DetectRunawaySession);

	pid = FileRep_StartChildProcess(FileRepProcessTypeMirrorConsumerAppendOnly1);

	assert_int_equal(pid, -1);
	assert_int_equal(
			FileRepSubProcList[FileRepProcessTypeMirrorConsumerAppendOnly1].pid, 0);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__FileRep_StartChildProcess)
	};

	MemoryContextInit();

	return run_tests(tests);
}
