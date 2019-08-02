#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../bitstream.c"

static void
test__Bitstream__Simple(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	uint32 tmp;

	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 32);
	assert_false(Bitstream_HasError(&bitstream));

	Bitstream_Put(&bitstream, 0x1, 1);
	assert_int_equal(data[0], 0x80);
	Bitstream_Put(&bitstream, 0x1, 1);
	assert_int_equal(data[0], 0xC0);
	Bitstream_Put(&bitstream, 0x1, 1);
	assert_int_equal(data[0], 0xE0);

	for (int i = 3; i < 8; i++)
	{
		Bitstream_Put(&bitstream, 0x1, 1);
	}
	assert_false(Bitstream_HasError(&bitstream));
	assert_int_equal(data[0], 0xFF);

	Bitstream_Put(&bitstream, 0x01FF, 16);
	assert_int_equal(data[1], 0x01);
	assert_int_equal(data[2], 0xFF);

	Bitstream_Put(&bitstream, 0x1FF, 9);
	assert_int_equal(data[3], 0xFF);
	assert_int_equal(data[4], 0x80);
	assert_false(Bitstream_HasError(&bitstream));

	Bitstream bitstream2;
	Bitstream_Init(&bitstream2, data, 32);
	assert_false(Bitstream_HasError(&bitstream2));

	assert_true(Bitstream_Get(&bitstream2, 8, &tmp));
	assert_int_equal(tmp, 0xFF);
	assert_true(Bitstream_Get(&bitstream2, 16, &tmp));
	assert_int_equal(tmp, 0x1FF);
	assert_true(Bitstream_Get(&bitstream2, 9, &tmp));
	assert_int_equal(tmp, 0x1FF);
}

static void
test__Bitstream_ErrorFlag(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 4);
	assert_false(Bitstream_HasError(&bitstream));

	for (int i = 0; i < 8; i++)
	{
		Bitstream_Put(&bitstream, 0x1, 8);
	}
	assert_true(Bitstream_HasError(&bitstream));

	Bitstream bitstream2;
	Bitstream_Init(&bitstream2, data, 4);
	for (int i = 0; i < 8; i++)
	{
		Bitstream_Put(&bitstream2, 0x1, 8);
	}
	assert_true(Bitstream_HasError(&bitstream2));
}

static void
test__Bitstream__Skip(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	uint32 tmp;

	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 32);

	Bitstream_Put(&bitstream, 0x2, 2);
	Bitstream_Skip(&bitstream, 9);
	Bitstream_Put(&bitstream, 0x3, 2);
	assert_false(Bitstream_HasError(&bitstream));
	assert_int_equal(Bitstream_GetOffset(&bitstream), 13);

	Bitstream bitstream2;
	Bitstream_Init(&bitstream2, data, 32);
	assert_false(Bitstream_HasError(&bitstream2));
	assert_true(Bitstream_Get(&bitstream2, 2, &tmp));
	assert_int_equal(tmp, 0x02);
	assert_true(Bitstream_Skip(&bitstream2, 9));
	assert_true(Bitstream_Get(&bitstream2, 2, &tmp));
	assert_int_equal(tmp, 0x3);
	assert_int_equal(Bitstream_GetOffset(&bitstream2), 13);
}

static void
test__Bitstream__Align(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 32);

	Bitstream_Put(&bitstream, 0x2, 2);
	Bitstream_Align(&bitstream, 8);
	assert_int_equal(Bitstream_GetOffset(&bitstream), 8);
	Bitstream_Put(&bitstream, 0x2, 2);
	Bitstream_Align(&bitstream, 32);
	assert_int_equal(Bitstream_GetOffset(&bitstream), 32);
}

static void
test__Bitstream__GetAlignedData(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 32);

	Bitstream_Put(&bitstream, 0x2, 2);
	unsigned char *ad = Bitstream_GetAlignedData(&bitstream, 8);
	assert_int_equal(data + 1, ad);
	assert_int_equal(Bitstream_GetOffset(&bitstream), 8);
	ad = Bitstream_GetAlignedData(&bitstream, 32);
	assert_int_equal(data + 4, ad);
	assert_int_equal(Bitstream_GetOffset(&bitstream), 32);
}

static void
test__Bitstream__GetRemaining(void **state)
{
	Bitstream bitstream;
	unsigned char data[4];
	memset(data, 0, 4);
	Bitstream_Init(&bitstream, data, 4);

	Bitstream_Put(&bitstream, 0x2, 2);
	assert_int_equal(30, Bitstream_GetRemaining(&bitstream));
	assert_false(Bitstream_HasError(&bitstream));

	Bitstream_Put(&bitstream, 0x2, 30);
	assert_false(Bitstream_HasError(&bitstream));

	assert_int_equal(0, Bitstream_GetRemaining(&bitstream));
}

static void
test__Bitstream__GetLength(void **state)
{
	Bitstream bitstream;
	unsigned char data[32];
	memset(data, 0, 32);
	Bitstream_Init(&bitstream, data, 4);

	assert_int_equal(0, Bitstream_GetLength(&bitstream));
	Bitstream_Put(&bitstream, 0x2, 2);
	assert_int_equal(1, Bitstream_GetLength(&bitstream));
	Bitstream_Put(&bitstream, 0x2, 6);
	assert_int_equal(1, Bitstream_GetLength(&bitstream));
	Bitstream_Put(&bitstream, 0x2, 1);
	assert_int_equal(2, Bitstream_GetLength(&bitstream));
	Bitstream_Put(&bitstream, 0x2, 16);
	assert_int_equal(4, Bitstream_GetLength(&bitstream));
}

int main(int argc, char* argv[]) {
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__Bitstream__Simple),
			unit_test(test__Bitstream_ErrorFlag),
			unit_test(test__Bitstream__Skip),
			unit_test(test__Bitstream__Align),
			unit_test(test__Bitstream__GetAlignedData),
			unit_test(test__Bitstream__GetRemaining),
			unit_test(test__Bitstream__GetLength)
	};
	return run_tests(tests);
}
