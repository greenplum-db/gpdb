#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/guc.h"
static char *sync_guc[] = 
{
	#include "utils/sync_guc_name.h"
};

static char *unsync_guc[] = 
{
	#include "utils/unsync_guc_name.h"
};

int sync_length = sizeof(sync_guc) / sizeof(char *);
int unsync_length = sizeof(unsync_length) / sizeof(char *);

static int guc_array_compare(const void *a, const void *b)
{
	const char *namea = *(const char **)a;
	const char *nameb = *(const char **)b;

	return guc_name_compare(namea, nameb);
}

/*
 * case 1: promise sync_guc_name.h guc name place order by alphabets
 * same as qsort ranking result.
 */
static void
test_sync_guc_name_ordering(void **state)
{
	const char *guc_names_array[] =
	{
		#include "utils/sync_guc_name.h"
	};
	qsort((void *) guc_names_array, sync_length, sizeof(char *), guc_array_compare);

	for(int i = 1; i < sync_length; i ++)
		assert_true(strcmp(sync_guc[i], guc_names_array[i]) == 0);
}

/*
 * case 2: promise unsync_guc_name.h guc name place order by alphabets
 * same as qsort ranking result.
 */
static void
test_unsync_guc_name_ordering(void **state)
{
	const char *guc_names_array[] =
	{
		#include "utils/unsync_guc_name.h"
	};
	qsort((void *) guc_names_array, unsync_length, sizeof(char *), guc_array_compare);

	for(int i = 1; i < unsync_length; i ++)
		assert_true(strcmp(unsync_guc[i], guc_names_array[i]) == 0);
}

int main(int argc, char* argv[]) {
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_sync_guc_name_ordering),
		unit_test(test_unsync_guc_name_ordering)
	};

	return run_tests(tests);
}
