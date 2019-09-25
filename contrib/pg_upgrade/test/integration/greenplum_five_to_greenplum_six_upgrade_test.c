#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmockery.h"

#include "scenarios/heap_table.h"
#include "scenarios/ao_table.h"

static void
resetGpdbFiveDataDirectories(void)
{
	printf("\nMaking a copy of gpdb5 data directories.\n");
	system("rsync -a --delete ./gpdb5-data-copy/ ./gpdb5-data");
}

static void
resetGpdbSixDataDirectories(void)
{
	printf("\nMaking a copy of gpdb6 data directories.\n");
	system("rsync -a --delete ./gpdb6-data-copy/ ./gpdb6-data");
}

static void
setup(void **state)
{
	resetGpdbFiveDataDirectories();
	resetGpdbSixDataDirectories();
}

static void
teardown(void **state)
{
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test_setup_teardown(test_an_ao_table_with_data_can_be_upgraded, setup, teardown),
		unit_test_setup_teardown(test_a_heap_table_with_data_can_be_upgraded, setup, teardown),
	};

	return run_tests(tests);
}
