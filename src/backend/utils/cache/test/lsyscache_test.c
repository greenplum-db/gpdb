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

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_get_func_arg_types_can_correctly_return_more_than_one_argtype)
	};
	MemoryContextInit();
	return run_tests(tests);
}
