#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>


#include "cmockery.h"
#include "debugger.h"


#include "postgres.h"
#include "access/gin_private.h"


static BlockId
make_block_id(uint16 high, uint16 low)
{
	BlockId block_id;
	block_id = (BlockId) calloc(1, sizeof(BlockIdData));
	block_id->bi_hi = high;
	block_id->bi_lo = low;
	return block_id;
}

static ItemPointer
make_item_pointer(BlockId block_id, OffsetNumber offset_number)
{
	ItemPointer item_pointer;
	item_pointer = (ItemPointer) calloc(1, sizeof(ItemPointerData));
	item_pointer->ip_blkid = *block_id;
	item_pointer->ip_posid = offset_number;
	return item_pointer;
}

/*
 * Postgres expects item pointers' offsets to be less than 11 bits.
 * Greenplum append-only tables allow for the full 16 bits of OffsetNumber
 */
static void
test_compress_gin_posting_list_with_item_pointer_with_offset_larger_than_eleven_bits()
{
	OffsetNumber offset_number_larger_than_11_bits = 3000;
	int number_of_item_pointers = 1;
	int max_size = 100 * sizeof(ItemPointerData);

	ItemPointer item_pointer = make_item_pointer(
		make_block_id(0, 0),
		offset_number_larger_than_11_bits);

	int *number_written = calloc(1, sizeof(int));
	
	GinPostingList *gin_posting_list = ginCompressPostingList(
		item_pointer,
		number_of_item_pointers,
		max_size,
		number_written);

	assert_int_equal(*gin_posting_list->bytes, 0);
	assert_int_equal(gin_posting_list->nbytes, 0);
	assert_int_equal(*number_written, 1);
	assert_int_equal(gin_posting_list->first.ip_posid, item_pointer->ip_posid);
}

static void
test_compress_gin_posting_list_with_multiple_item_pointers()
{
	OffsetNumber offset_number_with_all_bits_on = 65535;
	OffsetNumber offset_number_larger_than_11_bits = 5000;
	OffsetNumber other_offset_number_larger_than_11_bits = 5000;

	ItemPointer first_item_pointer = make_item_pointer(
		make_block_id(0, 0),
		offset_number_larger_than_11_bits);

	ItemPointer second_item_pointer = make_item_pointer(
		make_block_id(65534, 65535),
		offset_number_with_all_bits_on);

	/*
	 * Last itempointer in the list does not get encoded or decoded 
	 */
	ItemPointer third_item_pointer = make_item_pointer(
		make_block_id(65535, 65535),
		other_offset_number_larger_than_11_bits);

	ItemPointerData item_pointer_datas[3];
	ItemPointerCopy(first_item_pointer, &item_pointer_datas[0]);
	ItemPointerCopy(second_item_pointer, &item_pointer_datas[1]);
	ItemPointerCopy(third_item_pointer, &item_pointer_datas[2]);
	
	int number_of_item_pointers = 3;
	int max_size = 100 * sizeof(ItemPointerData);
	int *number_written = calloc(1, sizeof(int));

	GinPostingList *gin_posting_list = ginCompressPostingList(
		&item_pointer_datas[0],
		number_of_item_pointers,
		max_size,
		number_written);

	int number_of_decoded_item_pointers = 0; 
	ItemPointer decoded_item_pointers = ginPostingListDecode(
		gin_posting_list,
		&number_of_decoded_item_pointers);

	// Number of items compressed successfully
	assert_int_equal(*number_written, 3);

	// Number of items are decoded successfully
	assert_int_equal(number_of_decoded_item_pointers, 3);
	
	// Block ids can be decoded successfuly
	assert_int_equal(decoded_item_pointers[0].ip_blkid.bi_hi, 0);
	assert_int_equal(decoded_item_pointers[0].ip_blkid.bi_lo, 0);
	
	assert_int_equal(decoded_item_pointers[1].ip_blkid.bi_hi, 65534);
	assert_int_equal(decoded_item_pointers[1].ip_blkid.bi_lo, 65535);

	// Offsets are decoded successfully
	assert_int_equal(
		decoded_item_pointers[0].ip_posid,
		5000);

	assert_int_equal(
		decoded_item_pointers[1].ip_posid,
		65535);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_compress_gin_posting_list_with_item_pointer_with_offset_larger_than_eleven_bits),
		unit_test(test_compress_gin_posting_list_with_multiple_item_pointers)
	};
	
	return run_tests(tests);
}
