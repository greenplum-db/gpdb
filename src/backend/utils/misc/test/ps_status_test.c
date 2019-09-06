#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../ps_status.c"

/*
 * Check it won't crash in case the ps_buffer overflows.
 */
static void
test__set_ps_display(void **state)
{
	ps_buffer = (char *) malloc(64 * sizeof(char));
	memset(ps_buffer, 0x7F, 64 * sizeof(char));
	ps_buffer_fixed_size = 25;
	ps_buffer_size = 32;
	last_status_len = 0;
	IsUnderPostmaster = true;

	gp_session_id = 1024;
	Gp_role = GP_ROLE_DISPATCH;
	GpIdentity.segindex = 24;
	gp_command_count = 1024;
	currentSliceId = 40;

	set_ps_display("testing activity", true);
	set_ps_display("testing activity", false);

	assert_true(ps_buffer[32] == 0x7f);
	free(ps_buffer);
}

/*
 * MPP-220077: real_act_prefix_size should not go beyond ps_buffer_size
 */
static void
test__set_ps_display__real_act_prefix_size_overflow(void **state)
{
	int		len;

	last_status_len = 0;
	ps_buffer_size = 10;
	ps_buffer = (char *) malloc(ps_buffer_size * sizeof(char));
	ps_buffer_fixed_size = 6;
	memset(ps_buffer, 'x', ps_buffer_fixed_size * sizeof(char));

	IsUnderPostmaster = true;

	gp_session_id = 26351;
	Gp_role = GP_ROLE_DISPATCH;
	GpIdentity.segindex = -1;
	gp_command_count = 964;
	currentSliceId = -1;

	set_ps_display("testing activity", true);
	assert_true(real_act_prefix_size <= ps_buffer_size);

	get_real_act_ps_display(&len);
	assert_true(len == 0);
	free(ps_buffer);
}

/*
 * Positive case to validate correctly getting the position and length for
 * activity string.
 */
static void
test__set_ps_display__real_act_prefix_size(void **state)
{
	int		len;
	char* activity = "testing activity";

	last_status_len = 0;
	ps_buffer_size = 100;
	ps_buffer = (char *) malloc(ps_buffer_size * sizeof(char));
	ps_buffer_fixed_size = 6;
	memset(ps_buffer, 'x', ps_buffer_fixed_size * sizeof(char));

	IsUnderPostmaster = true;

	gp_session_id = 26351;
	Gp_role = GP_ROLE_DISPATCH;
	GpIdentity.segindex = -1;
	gp_command_count = 964;
	currentSliceId = -1;

	set_ps_display(activity, true);
	assert_true(real_act_prefix_size <= ps_buffer_size);

	assert_true(strcmp(activity, get_real_act_ps_display(&len)) == 0);
	assert_true(len == strlen(activity));
	free(ps_buffer);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__set_ps_display),
			unit_test(test__set_ps_display__real_act_prefix_size_overflow),
			unit_test(test__set_ps_display__real_act_prefix_size)
	};
	return run_tests(tests);
}
