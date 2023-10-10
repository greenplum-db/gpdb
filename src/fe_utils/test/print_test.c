#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include "cmockery.h"

#include "../print.c" // the file which we want to test

// wrap function for exit() of printTableAddCell
void __wrap_exit(int code);
void
__wrap_exit(int code)
{
	check_expected(code);
	mock();
}

static void
test_printTableAddCell(void **state)
{
    struct printTableContent content;
    char cell[] = "cell";

    // normal case
    content.ncolumns = 26;
    content.nrows = 10;
    content.cellsadded = 100;
    printTableAddCell(&content, cell, false, false);
    assert_true( content.cellsadded == 101 );

    // bigger than int range (2147483647) case
    const long LONG_VAL = 4000000000L;
    content.ncolumns = 26;
    content.nrows = 200000000;
    content.cellsadded = LONG_VAL;
    printTableAddCell(&content, cell, false, false);
    assert_true( content.cellsadded == LONG_VAL+1 );

    // error case (cellsadded > ncolumns*nrows)
    content.ncolumns = 26;
    content.nrows = 200000000;
    content.cellsadded = 6000000000L;
    expect_value(__wrap_exit, code, 1);
    will_be_called(__wrap_exit);
    printTableAddCell(&content, cell, false, false); 
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_printTableAddCell)
	};

	return run_tests(tests);
}
