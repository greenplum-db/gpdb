#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../guc.c"

#define assert_null(c) assert_true(c == NULL)
#define assert_not_null(c) assert_true(c != NULL)

#define VARLEN(d) VARSIZE(d)-VARHDRSZ

/* Values for text datum */
#define TEXT_TYPLEN		-1	
#define TEXT_TYPBYVAL	false
#define TEXT_TYPALIGN	'i'

/* Helper function */
ArrayType *create_guc_array(List *guc_list, int elems);
ArrayType *create_md_guc_array(List *guc_list, int elems, int ndims);
Datum *create_guc_datum_array(List *guc_list, int num);

/*
 * NULL array input: return NULL
 */
void 
test__GUCArrayReset__NULL_array(void **state) 
{
	assert_null(GUCArrayReset(NULL));
}

/*
 * Test GUCArrayReset
 *
 * superuser: return NULL
 */
void
test__GUCArrayReset__superuser(void **state)
{
	ArrayType	*in;
	ArrayType	*out;

	will_return(superuser, true);
	in = construct_empty_array(TEXTOID);
	assert_not_null(in);

	out = GUCArrayReset(in);
	assert_null(out);

	pfree(in);
}

/*
 * GUC array: all PGC_USERSET, return NULL
 */
void
test__GUCArrayReset__all_userset_guc(void **state)
{
	ArrayType  *in;
	List 	   *guc_list;
	int			elems;

	build_guc_variables();
	will_return(superuser, false);

	/* construct text array */
	elems = 3;
	guc_list = list_make3("application_name=unittest", "password_encryption=off", "backslash_quote=off");
	in = create_guc_array(guc_list, elems);

	assert_null(GUCArrayReset(in));

	list_free(guc_list);
	pfree(in);
}

/*
 * GUC array: all non-PGC_USERSET, return the same array
 */
void
test__GUCArrayReset__all_non_userset_guc(void **state)
{
	ArrayType  *in;
	ArrayType  *out;
	Datum		d;
	List	   *guc_list;
	bool		isnull;
	int			i;
	int			elems;

	build_guc_variables();
	will_return(superuser, false);

	/* construct text array */
	elems = 3;
	guc_list = list_make3("log_error_verbosity=terse", "gp_log_format=csv", "maintenance_mode=true");
	in = create_guc_array(guc_list, elems);

	out = GUCArrayReset(in);
	assert_not_null(out);
	assert_int_equal(ARR_DIMS(out)[0], elems);

	/* check element 1 */
	i = 1;
	d = array_ref(out, 1, &i, -1, TEXT_TYPLEN, TEXT_TYPBYVAL, TEXT_TYPALIGN, &isnull);
	assert_false(isnull);
	assert_int_equal(strlen("log_error_verbosity=terse"), VARLEN(d));
	assert_memory_equal(VARDATA(d), "log_error_verbosity=terse", VARLEN(d));

	/* check element 2 */
	i = 2;
	d = array_ref(out, 1, &i, -1, TEXT_TYPLEN, TEXT_TYPBYVAL, TEXT_TYPALIGN, &isnull);
	assert_false(isnull);
	assert_int_equal(strlen("gp_log_format=csv"), VARLEN(d));
	assert_memory_equal(VARDATA(d), "gp_log_format=csv", VARLEN(d));

	/* check element 3 */
	i = 3;
	d = array_ref(out, 1, &i, -1, TEXT_TYPLEN, TEXT_TYPBYVAL, TEXT_TYPALIGN, &isnull);
	assert_false(isnull);
	assert_int_equal(strlen("maintenance_mode=true"), VARLEN(d));
	assert_memory_equal(VARDATA(d), "maintenance_mode=true", VARLEN(d));

	list_free(guc_list);
	pfree(in);
	pfree(out);
}

/*
 * GUC array: mix of PGC_USERSET, PGC_POSTMASTER, PGC_SUSET
 *		return ArrayType contains non-PGC_USERSET
 */
void
test__GUCArrayReset__mix_guc(void **state)
{
	ArrayType  *in;
	ArrayType  *out;
	Datum		d;
	List	   *guc_list;
	int			elems;

	build_guc_variables();
	will_return(superuser, false);

	/* construct text array */
	elems = 4;
	guc_list = list_make4("password_encryption=on", "log_error_verbosity=verbose", "application_name=mixtest", "allow_system_table_mods=dml");
	in = create_guc_array(guc_list, elems);

	out = GUCArrayReset(in);
	assert_not_null(out);
	assert_int_equal(ARR_DIMS(out)[0], 1);
	d = PointerGetDatum(ARR_DATA_PTR(out));
	assert_int_equal(strlen("log_error_verbosity=verbose"), VARLEN(d));
	assert_memory_equal(VARDATA(d), "log_error_verbosity=verbose", VARLEN(d));

	list_free(guc_list);
	pfree(in);
	pfree(out);
}

/*
 * GUC array: one invalid guc + non-userset guc
 *		return ArrayType contain non-userset guc, ignore invalid guc
 */
void
test__GUCArrayReset__invalid_guc(void **state) 
{
	ArrayType  *in;
	ArrayType  *out;
	Datum		d;
	List       *guc_list;
	int         elems;

	build_guc_variables();
	will_return(superuser, false);

	/* construct text array */
	elems = 2;
	guc_list = list_make2("invalid_guc=true", "gp_log_format=text");
	in = create_guc_array(guc_list, elems);

	out = GUCArrayReset(in);
	assert_not_null(out);
	assert_int_equal(ARR_DIMS(out)[0], 1);

	d = PointerGetDatum(ARR_DATA_PTR(out));
	assert_int_equal(strlen("gp_log_format=text"), VARLEN(d));
	assert_memory_equal(VARDATA(d), "gp_log_format=text", VARLEN(d));

	list_free(guc_list);
	pfree(in);
	pfree(out);
}

/*
 * GUC array: 2-dim array
 *		return NULL
 */
void 
test__GUCArrayReset__md_array(void **state) 
{
	ArrayType  *in;
	List       *guc_list;
	int         elems;
	int			ndims;

	build_guc_variables();
	will_return(superuser, false);

	/* construct 2-dimension text array */
	elems = 2;
	ndims = 2;
	guc_list = list_make4("gp_log_format=text", "allow_system_table_mods=ddl", "password_encryption=on", "log_error_verbosity=verbose");
	in = create_md_guc_array(guc_list, elems, ndims);

	assert_null(GUCArrayReset(in));

	list_free(guc_list);
	pfree(in);
}


/*
 * Test set_config_option
 */
void
test__set_config_option(void **state) 
{
	build_guc_variables();

	bool ret;
	ret = set_config_option("password_encryption", "off", PGC_POSTMASTER, PGC_S_SESSION, false, false);
	assert_true(ret);
}

/*
 * Test find_option
 */
void
test__find_option(void **state) 
{
	build_guc_variables();

	struct config_generic *config;
	config = find_option("unknown_name", false, LOG);
	assert_null(config);

	config = find_option("password_encryption", false, LOG);
	assert_not_null(config);
	config = find_option("gp_resqueue_priority_cpucores_per_segment", false, LOG);
	assert_not_null(config);

	/* supported obsolete guc name */
	config = find_option("work_mem", false, LOG);
	assert_not_null(config);
}


/*
 * Helper function
 */
Datum *
create_guc_datum_array(List *guc_list, int num)
{
	Datum      *darray;
	ListCell   *item;
	int         i;

	darray = (Datum *) palloc0(num * sizeof(Datum));

	i = 0;
	foreach(item, guc_list)
		darray[i++] = CStringGetTextDatum((char *) lfirst(item));

	return darray;
}

ArrayType *
create_guc_array(List *guc_list, int elems)
{
	ArrayType  *array;
	Datum 	   *darray;

	darray = create_guc_datum_array(guc_list, elems);
	array = construct_array(darray, elems, TEXTOID, TEXT_TYPLEN, TEXT_TYPBYVAL, TEXT_TYPALIGN);

	pfree(darray);
	return array;
}

ArrayType *
create_md_guc_array(List *guc_list, int elems, int ndims)
{
	ArrayType  *array;
	Datum	   *darray;
	int			dims[ndims];
	int			lbs[1];

	darray = create_guc_datum_array(guc_list, elems * ndims);

	dims[0] = elems;
	dims[1] = elems;
	lbs[0] = 1;
	array = construct_md_array(darray, NULL, ndims, dims, lbs,
							   TEXTOID, TEXT_TYPLEN, TEXT_TYPBYVAL, TEXT_TYPALIGN);
	pfree(darray);
	return array;
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__GUCArrayReset__superuser),
		unit_test(test__GUCArrayReset__NULL_array),
		unit_test(test__GUCArrayReset__all_userset_guc),
		unit_test(test__GUCArrayReset__all_non_userset_guc),
		unit_test(test__GUCArrayReset__mix_guc),
		unit_test(test__GUCArrayReset__invalid_guc),
		unit_test(test__GUCArrayReset__md_array),
		unit_test(test__set_config_option),
		unit_test(test__find_option)
	};

	MemoryContextInit();

	return run_tests(tests);
}
