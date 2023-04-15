#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "access/htup_details.h"
#include "utils/memutils.h"
#include "catalog/pg_proc.h"

#include "../lsyscache.c"

static void
test_get_func_arg_types_can_correctly_return_more_than_one_argtype(void **state)
{
	HeapTuple	tp;
	tp = malloc(sizeof(struct HeapTupleData));
	struct HeapTupleHeaderData *data;

	/* allocate enough space to hold 2 oids in proargtypes oidvector */
	data = malloc(sizeof(struct HeapTupleHeaderData) + sizeof(struct FormData_pg_proc) + sizeof(Oid) * 2);
	tp->t_data = data;

	/* setup tuple offset to data */
	data->t_hoff = (uint8)sizeof(struct HeapTupleHeaderData);
	Form_pg_proc pgproc = (Form_pg_proc)((char *)data + sizeof(struct HeapTupleHeaderData));

	/* setup oidvector with 2 oids */
	pgproc->proargtypes.dim1 = 2;
	pgproc->proargtypes.values[0] = 11;
	pgproc->proargtypes.values[1] = 22;

	will_return(SearchSysCache, tp);
	expect_value(SearchSysCache, cacheId, PROCOID);
	expect_value(SearchSysCache, key1, 123);
	expect_any(SearchSysCache, key2);
	expect_any(SearchSysCache, key3);
	expect_any(SearchSysCache, key4);

	will_be_called(ReleaseSysCache);
	expect_value(ReleaseSysCache, tuple, tp);

	List *result = get_func_arg_types(123);
	assert_int_equal(2, result->length);

	assert_int_equal(11, lfirst_oid(list_head(result)));
	assert_int_equal(22, lfirst_oid(lnext(list_head(result))));
}

static void
test_default_partition_opfamily_for_type(void **state)
{
    TypeCacheEntry *tcache;
    tcache = malloc(sizeof(struct TypeCacheEntry));
    tcache->btree_opf = 111;
    tcache->cmp_proc = 0;
    tcache->eq_opr = 0;
    tcache->lt_opr = 0;
    tcache->gt_opr = 0;
    will_return(lookup_type_cache, tcache);
    expect_value(lookup_type_cache, type_id, 23);
    expect_value(lookup_type_cache, flags, 623);

    // if btree_opf has a non-zero value,
    // but cmp_proc, eq_opr, lt_opr, and gt_opr are all zero,
    // return invalid oid
    Oid result1 = default_partition_opfamily_for_type(23);
    assert_int_equal(0, result1);

    tcache->cmp_proc = 222;
    will_return(lookup_type_cache, tcache);
    expect_value(lookup_type_cache, type_id, 23);
    expect_value(lookup_type_cache, flags, 623);

    // if btree_opf and cmp_proc have non-zero values,
    // but eq_opr, lt_opr, and gt_opr are all zero,
    // return invalid oid
    Oid result2 = default_partition_opfamily_for_type(23);
    assert_int_equal(0, result2);

    tcache->gt_opr = 333;
    will_return(lookup_type_cache, tcache);
    expect_value(lookup_type_cache, type_id, 23);
    expect_value(lookup_type_cache, flags, 623);

    // if btree_opf and cmp_proc have non-zero values,
    // and at least eq/lt/gt_opr has a non-zero value,
    // return btree opfamily
    Oid result3 = default_partition_opfamily_for_type(23);
    assert_int_equal(111, result3);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_get_func_arg_types_can_correctly_return_more_than_one_argtype),
            unit_test(test_default_partition_opfamily_for_type)
	};
	MemoryContextInit();
	return run_tests(tests);
}
