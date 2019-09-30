#include "stdbool.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#include "cmockery.h"
#include "row-assertions.h"

bool (*matcher)(void *expected, void*actual);
void (*match_failed)(void *expected);

static bool
row_in(void *expected_user, Rows *actual_rows)
{
	for (int i = 0; i < actual_rows->size; i++)
	{
		void *current_user = actual_rows->rows[i];

		if (matcher(current_user, expected_user))
			return true;
	}

	return false;
}

static void
assert_row_in(void *expected_row, Rows *actual_rows)
{
	bool found = row_in(expected_row, actual_rows);

	if (!found)
		match_failed(expected_row);

	assert_true(found);
}

void
assert_rows(Rows *actual_rows, Rows expected_rows)
{
	if (matcher == NULL)
		printf("expected matcher() function to be configured, was NULL");

	if (match_failed == NULL)
		printf("expected match_failed() function to be configured, was NULL");

	for (int i = 0; i < expected_rows.size; i++)
		assert_row_in(expected_rows.rows[i], actual_rows);
}

