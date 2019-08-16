#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/guc.h"
#include "utils/guc_tables.h"

static bool debuginfo = false;

extern struct config_bool ConfigureNamesBool[];
extern struct config_int ConfigureNamesInt[];
extern struct config_real ConfigureNamesReal[];
extern struct config_enum ConfigureNamesEnum[];
extern struct config_string ConfigureNamesString[];

static int guc_array_compare(const void *a, const void *b)
{
	const char *namea = *(const char **)a;
	const char *nameb = *(const char **)b;

	return guc_name_compare(namea, nameb);
}

static void
test_guc_name_unique(void **state)
{
	int i;
	static const char *guc_names_array[] =
	{
		#include "utils/sync_guc_name.h"
		#include "utils/unsync_guc_name.h"
	};


	int guc_num = sizeof(guc_names_array) / sizeof(char *);
	qsort((void *) guc_names_array, guc_num, sizeof(char *), guc_array_compare);

	for(i = 1; i < guc_num;i ++)
	{
		assert_true(strcmp(guc_names_array[i - 1], guc_names_array[i]));
	}
}

/*
 * case 1: test guc list fully cover predefined guc name
 *
 */
static void
test_guc_name_coverage(void **state)
{
	int i;
	static const char *guc_names_array[] =
	{
		#include "utils/sync_guc_name.h"
		#include "utils/unsync_guc_name.h"
	};


	int guc_num = sizeof(guc_names_array) / sizeof(char *);
	qsort((void *) guc_names_array, guc_num, sizeof(char *), guc_array_compare);

	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesBool[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesInt[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesReal[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesReal[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}

	for (i = 0; ConfigureNamesString[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesString[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}

	for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesEnum[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesBool_gp[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesBool_gp[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesInt_gp[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesInt_gp[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesReal_gp[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesReal_gp[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesString_gp[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesString_gp[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}
	for (i = 0; ConfigureNamesEnum_gp[i].gen.name; i++)
	{
		char *res = (char *) bsearch((void *) &(ConfigureNamesEnum_gp[i].gen.name),
				(void *) guc_names_array,
				guc_num,
				sizeof(char *),
				guc_array_compare);
		assert_true(res != NULL);
	}

}

/*
 * case 2: test guc name list's elements number is same as predefined guc number
 */
static void 
test_guc_name_number(void **state)
{
	static const char *guc_names_array[] =
	{
		#include "utils/sync_guc_name.h"
		#include "utils/unsync_guc_name.h"
	};


	int guc_num = sizeof(guc_names_array) / sizeof(char *);

	int i;
	int guc_name_num = 0;
	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesReal[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesString[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesEnum[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesBool_gp[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesInt_gp[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesReal_gp[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesString_gp[i].gen.name; i++)
		guc_name_num ++;
	for (i = 0; ConfigureNamesEnum_gp[i].gen.name; i++)
		guc_name_num ++;

	assert_true(guc_name_num == guc_num);
}

/*
 * case 3: two guc lists elements are fully mutual exclusion
 */
static void
test_guc_name_list_mutual_exclusion(void **state)
{
	static const char *guc_sync[] =
	{
		#include "utils/sync_guc_name.h"
	};

	static const char *guc_unsync[] =
	{
		#include "utils/unsync_guc_name.h"
	};

	int guc_sync_num = sizeof(guc_sync) / sizeof(char *);
	int guc_unsync_num = sizeof(guc_unsync) / sizeof(char *);

	for(int i = 0; i < guc_sync_num; i++)
	{
		char *res = (char *) bsearch((void *) &(guc_sync[i]),
				(void *) guc_unsync,
				guc_unsync_num,
				sizeof(char *),
				guc_array_compare);

		assert_true(res == NULL);
	}
}

/*
 * case 4: sync guc should not contain PGC_INTERNAL, PGC_SIGUP or PGC_POSTMASTER
 */
static void
test_guc_flag_check(void **state)
{
	static const char *guc_sync[] =
	{
		#include "utils/sync_guc_name.h"
	};
	int guc_sync_num = sizeof(guc_sync) / sizeof(char *);
	build_guc_variables();
	for(int i = 0; i < guc_sync_num; i++)
	{
		struct config_generic *record = find_option(guc_sync[i], false, 10);
		assert_true(record->context != PGC_INTERNAL);
		assert_true(record->context != PGC_POSTMASTER);
		assert_true(record->context != PGC_SIGHUP);
	}
}

int main(int argc, char* argv[]) {
	const UnitTest tests[] = {
		unit_test(test_guc_name_unique),
		unit_test(test_guc_name_coverage),
		unit_test(test_guc_name_number),
		unit_test(test_guc_name_list_mutual_exclusion),
		unit_test(test_guc_flag_check)
	};

	return run_tests(tests);
}
