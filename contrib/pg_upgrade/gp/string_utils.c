#include "string_utils.h"


/*
 * Takes an array and prints it using hardcoded separator and quote.
 * It is caller's responsibility to call pfree() on return value
 */
char *
array_to_string(const char *const array[], size_t length)
{
	const char *const SEPARATOR = ", ";
	const char *const QUOTE     = "'";

	char   *result;
	char   *result_current_pointer;
	size_t length_total         = 1;
	size_t i;

	if (length == 0)
	{
		result = palloc0(1);
		return result;
	}

	for (i = 0; i < length; i++)
	{
		length_total += strlen(array[i]);
		length_total += strlen(QUOTE) * 2;
		length_total += strlen(SEPARATOR);
	}

	length_total -= strlen(SEPARATOR);

	result                 = palloc(length_total);
	result_current_pointer = result;

	for (i = 0; i < length; i++)
	{
		int sprintf_status;

		if (i > 0)
		{
			if ((sprintf_status =
				     sprintf(result_current_pointer, "%s", SEPARATOR)) < 0)
				pg_fatal("sprintf() failed\n");
			result_current_pointer += sprintf_status;
		}

		if ((sprintf_status = sprintf(result_current_pointer,
		                              "%s%s%s",
		                              QUOTE,
		                              array[i],
		                              QUOTE)) < 0)
			pg_fatal("sprintf() failed\n");
		result_current_pointer += sprintf_status;
	}

	return result;
}