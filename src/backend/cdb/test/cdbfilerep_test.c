#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbfilerep.c"

/*
 * Test validates if forking a filerep process fails,
 * it doesn't set the filerep subprocess slot to -1.
 * As if kill is called against this pid, and if it is -1,
 * disastrously sends signal to unexpected processes in the system.
 * Also we expect 0 as the pid of dead process everywhere in the code.
 */
void
test__FileRep_StartChildProcess(void **state)
{
	pid_t		pid;
	FileRepSubProc FileRepSubProcListLocal[FileRepProcessType__EnumerationCount];

	FileRepSubProcList = &FileRepSubProcListLocal;

	assert_int_equal(sizeof(FileRepSubProcListInitial), sizeof(FileRepSubProcListLocal));
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
