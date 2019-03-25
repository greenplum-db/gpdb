#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"


#include "postgres.h"


#include "access/gin_private.h"


#include <stdlib.h>


/*
 * 
 * ginpostlist Fakes
 * 
 */
bool assert_enabled;
void *palloc(Size size) { /* something */ };
void *repalloc(void *pointer, Size size) { /* something */};
void ExceptionalCondition(const char *conditionName, const char *errorType, const char *fileName, int lineNumber) {	exit(1); }
void tbm_add_tuples(TIDBitmap *tbm,	const ItemPointer tids, int ntids, bool recheck) {	/* something */ };
void pfree(void *pointer) { /* something */ };


// the first position in the array stores 127
// a = 127 == 01111111 (0x7f)
// b = 128 == 10000000 (0x80)
// c = 129 == 10000001 (0x81)
// d = c&a == 00000001 (0x01)
// e = d|b == 10000001
// f = 257 == 100000001 (0x101)

// 10000001 >> 7 == 00000001
// 100000001 >> 7 == 000000010
// 1000000000000001 == 32769


void test__encode_varbyte_less_than_127(void **state) {
    uint64 val;
	unsigned char **list;
	unsigned char *result_list;

	// 1 is not encoded
	val = 1;
	list = calloc(1, sizeof(unsigned char *));
    result_list = *list = calloc(1, sizeof(unsigned char));
		
	encode_varbyte(val, list);
	
 	assert_int_equal(*result_list, 1);
 	assert_int_equal(*result_list, 0x01);

	// 126 is not encoded
	val = 126;
	list = calloc(1, sizeof(unsigned char *));
	result_list = *list = calloc(1, sizeof(unsigned char));
	encode_varbyte(val, list);
 	assert_int_equal(*result_list, 0x7e);
 	assert_int_equal(*result_list, 126);	
}


void test__encode_varbyte_just_larger_than_127(void **state) {
    uint64 val;
	unsigned char **list;
	unsigned char *result_list;

	val = 129;
	list = calloc(2, sizeof(unsigned char *));
	result_list = *list = calloc(1, sizeof(unsigned char));
	encode_varbyte(val, list);

 	assert_int_equal(*result_list, 0x81);
 	assert_int_equal(*result_list, 129);	
}

void test__encode_varbyte_just_larger_than_255(void **state) {
    uint64 val;
	unsigned char **list;
	unsigned char *result_list;

	val = 256;
	list = calloc(2, sizeof(unsigned char));
	result_list = *list = calloc(1, sizeof(unsigned char));
	encode_varbyte(val, list);

 	assert_int_equal(*(result_list), 0x80);
 	assert_int_equal(*(result_list), 128);
	
 	assert_int_equal(*(result_list+1), 0x02);
 	assert_int_equal(*(result_list+1), 2);
}


void test__decode_varbyte_can_decode_1(void **state) {
	uint64 value;
	unsigned char **list;
	unsigned char *original_list;
	unsigned char *result_list;

	list = calloc(17, sizeof(unsigned char *));
	original_list = result_list = *list = calloc(1, sizeof(unsigned char));
	encode_varbyte(1, list);
	decode_varbyte(&original_list);
 	assert_int_equal(*result_list, 1);
}


void test__decode_varbyte_can_decode_128(void **state) {
	uint64 value;
	unsigned char **list;
	unsigned char **original_list;
	unsigned char *result_list;

	original_list = list = calloc(17, sizeof(unsigned char *));
    result_list = *list = calloc(1, sizeof(unsigned char));

	value = 128;
	encode_varbyte(value, list);
	decode_varbyte(original_list);
	
 	assert_int_equal(*result_list, 0x80);
}


void test__decode_varbyte_can_decode_256(void **state) {
	uint64 value;
	unsigned char **list;
	unsigned char **original_list;
	unsigned char *result_list;

	original_list = list = calloc(17, sizeof(unsigned char *));
	result_list = *list = calloc(1, sizeof(unsigned char));
	
	value = 257;
	encode_varbyte(value, list);
	decode_varbyte(original_list);

 	assert_int_equal(*result_list, 0x81);
 	assert_int_equal(*(result_list+1), 0x02);
}


void test__decode_varbyte_can_decode_x(void **state) {
	uint64 value;
	uint64 return_value;
	unsigned char **list;
	unsigned char **original_list;
	unsigned char *result_list;

	list = calloc(17, sizeof(unsigned char *));
	original_list = list;
	
	
	for( int i = 0; i < 17; i++)
		list[i] = calloc(1, sizeof(unsigned char));

	result_list = list[0];

	value = 1;
	
	debugger();
	
	encode_varbyte(value, list);

	debugger();
	
	assert_int_equal(*result_list, value);
	
	return_value = decode_varbyte(original_list);

	debugger();

 	assert_int_equal(return_value, value);
}

void debugger() {};

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__encode_varbyte_less_than_127),
		unit_test(test__encode_varbyte_just_larger_than_127),
		unit_test(test__encode_varbyte_just_larger_than_255),
		/*
		unit_test(test__decode_varbyte_can_decode_1),
		unit_test(test__decode_varbyte_can_decode_128),
		unit_test(test__decode_varbyte_can_decode_256),
		*/
		unit_test(test__decode_varbyte_can_decode_x)
	};
	
	return run_tests(tests);
}

