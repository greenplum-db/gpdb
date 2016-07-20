#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "../cdb_lockbox.c"

void
test__obfuscate(void **state)
{
	char		*str;
	char		*back;

	str = lb_obfuscate("foobar");
	back = lb_deobfuscate(str);
	assert_string_equal("foobar", back);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__obfuscate),
	};
	return run_tests(tests);
}
