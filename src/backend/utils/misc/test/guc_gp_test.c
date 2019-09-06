#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#include "../guc_gp.c"

void init(void );

void init()
{
	sync_guc_num = sizeof(sync_guc_names_array)/ sizeof(char *);
	unsync_guc_num = sizeof(unsync_guc_names_array)/ sizeof(char *);
	qsort((void *) sync_guc_names_array, sync_guc_num,
			sizeof(char *), guc_array_compare);

	qsort((void *) unsync_guc_names_array, unsync_guc_num,
			sizeof(char *), guc_array_compare);
}

static void assert_guc(struct config_generic *conf)
{
	char *res = (char *) bsearch((void *) &conf->name,
			(void *) sync_guc_names_array,
			sync_guc_num,
			sizeof(char *),
			guc_array_compare);
	if (!res)
	{
		char *res = (char *) bsearch((void *) &conf->name,
				(void *) unsync_guc_names_array,
				unsync_guc_num,
				sizeof(char *),
				guc_array_compare);

		if ( res == NULL)
			printf("GUC: '%s' does not exist in both list.\n", conf->name);

		assert_true(res);
	}
}

static void
test_bool_guc_gp_coverage(void **state)
{
	for (int i = 0; ConfigureNamesBool_gp[i].gen.name; i++)
	{
		struct config_generic *conf = (struct config_generic *)&ConfigureNamesBool_gp[i];
		assert_guc(conf);
	}

}

static void
test_int_guc_gp_coverage(void **state)
{
	for (int i = 0; ConfigureNamesInt_gp[i].gen.name; i++)
	{
		struct config_generic *conf = (struct config_generic *)&ConfigureNamesInt_gp[i];
		assert_guc(conf);
	}

}

static void
test_string_guc_gp_coverage(void **state)
{
	for (int i = 0; ConfigureNamesString_gp[i].gen.name; i++)
	{
		struct config_generic *conf = (struct config_generic *)&ConfigureNamesString_gp[i];
		assert_guc(conf);
	}

}

static void
test_real_guc_gp_coverage(void **state)
{
	for (int i = 0; ConfigureNamesReal_gp[i].gen.name; i++)
	{
		struct config_generic *conf = (struct config_generic *)&ConfigureNamesReal_gp[i];
		assert_guc(conf);
	}

}

static void
test_enum_guc_gp_coverage(void **state)
{
	for (int i = 0; ConfigureNamesEnum_gp[i].gen.name; i++)
	{
		struct config_generic *conf = (struct config_generic *)&ConfigureNamesEnum_gp[i];
		assert_guc(conf);
	}

}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);
	init();

	const UnitTest tests[] = {
		unit_test(test_bool_guc_gp_coverage),
		unit_test(test_int_guc_gp_coverage),
		unit_test(test_real_guc_gp_coverage),
		unit_test(test_string_guc_gp_coverage),
		unit_test(test_enum_guc_gp_coverage)
	};

	return run_tests(tests);
}
