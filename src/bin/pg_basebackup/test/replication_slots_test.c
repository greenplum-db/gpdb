#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres_fe.h"
#include "../replicationslot.h"

const char *sent_replication_slot_name = "";

void create_physical_replication_slot_spy(const char *name) {
	sent_replication_slot_name = name;
}

void test__create_slot(void **state) {
    char *replication_slot = "my_replication_slot_name";

    CreateReplicationSlot(replication_slot,
                          create_physical_replication_slot_spy);

    assert_string_equal(
	    sent_replication_slot_name,
    	"my_replication_slot_name");

}

int
main(int argc, char *argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test__create_slot),
    };

    return run_tests(tests);
}
