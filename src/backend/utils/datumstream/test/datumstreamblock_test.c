#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../datumstreamblock.c"

/* 
 * Unit test function to test the routines added for
 * Delta Compression
 */
static void 
test__DeltaCompression__Core(void **state) 
{
	Delta_Compression_status status = DELTA_COMPRESSION_NOT_APPLIED;
	DatumStreamTypeInfo typeInfo;

	DatumStreamBlockWrite* dsw = malloc(sizeof(DatumStreamBlockWrite));
	memset(dsw, 0, sizeof(DatumStreamBlockWrite));

	/* Creating the DatumStreamBlockWrite structure to be used for below iterations */
	strncpy(dsw->eyecatcher, DatumStreamBlockWrite_Eyecatcher, DatumStreamBlockWrite_EyecatcherLen);
	dsw->datumStreamVersion = DatumStreamVersion_Dense_Enhanced;
	dsw->rle_want_compression = true;
	dsw->delta_want_compression = true;
	dsw->typeInfo = &typeInfo;
	dsw->maxDataBlockSize = 32768;
	dsw->maxDatumPerBlock = 32768 / 64;
	dsw->datum_buffer_size = dsw->maxDataBlockSize;
	dsw->datum_buffer = malloc(dsw->datum_buffer_size);
	dsw->datum_afterp = dsw->datum_buffer + dsw->datum_buffer_size;
	dsw->null_bitmap_buffer_size = 64;
	dsw->null_bitmap_buffer = malloc(dsw->null_bitmap_buffer_size);
	dsw->rle_compress_bitmap_buffer_size = 16;
	dsw->rle_compress_bitmap_buffer = malloc(dsw->rle_compress_bitmap_buffer_size);
	dsw->rle_repeatcounts_maxcount = 16;
	dsw->rle_repeatcounts = malloc(dsw->rle_repeatcounts_maxcount * Int32Compress_MaxByteLen);
	dsw->delta_bitmap_buffer_size = 16;
	dsw->delta_bitmap_buffer = malloc(dsw->delta_bitmap_buffer_size);
	dsw->deltas_maxcount = 16;
	dsw->deltas = malloc(dsw->deltas_maxcount * Int32Compress_MaxByteLen);
	dsw->delta_sign = malloc(dsw->deltas_maxcount * sizeof(bool));
	DatumStreamBlockWrite_GetReady(dsw);

	/* For unit testing using this type object */
	typeInfo.datumlen = 4;
	typeInfo.typid = INT4OID;
	typeInfo.byval = true;

	/* 
	 * First value will always be physically stored 
	 */
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(32));
	assert_int_equal(status, DELTA_COMPRESSION_NOT_APPLIED);
	assert_false(dsw->delta_has_compression);
	assert_true(dsw->not_first_datum);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 0);
	assert_int_equal(dsw->delta_bitmap.bitCount, 0);

	/* Since physical datum, test the the routines for processing the same */
	DatumStreamBlockWrite_DenseIncrItem(dsw, 0, 4);
	DatumStreamBlockWrite_DeltaMaintain(dsw, UInt32GetDatum(32));
	assert_int_equal(dsw->physical_datum_count, 1);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 0);
	assert_int_equal(dsw->delta_bitmap.bitCount, 0);

	/* 
	 * Second value, this must be processed as delta 
	 */
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(33));
	assert_int_equal(status, DELTA_COMPRESSION_OK);
	assert_true(dsw->delta_has_compression);
	assert_int_equal((*(uint32 *) (&dsw->compare_item)), 33);
	/* delta value checks */
	assert_int_equal(dsw->deltas_count, 1);
	assert_int_equal(dsw->delta_sign[dsw->deltas_count-1], true);
	assert_int_equal(dsw->deltas[dsw->deltas_count-1], 1);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 1);
	assert_int_equal(dsw->delta_bitmap.bitCount,2);
	assert_true(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));

	/* 
	 * Third value, this must be processed as positive delta case 
	 */
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(53));
	assert_int_equal(status, DELTA_COMPRESSION_OK);
	assert_true(dsw->delta_has_compression);
	/* delta value checks */
	assert_int_equal((*(uint32 *) (&dsw->compare_item)), 53);
	assert_int_equal(dsw->deltas_count, 2);
	assert_int_equal(dsw->delta_sign[dsw->deltas_count-1], true);
	assert_int_equal(dsw->deltas[dsw->deltas_count-1], 53 - 33);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 2);
	assert_int_equal(dsw->delta_bitmap.bitCount,3);
	assert_true(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));

	/* 
	 * Forth value, this must be processed as negative delta case 
	 */
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(23));
	assert_int_equal(status, DELTA_COMPRESSION_OK);
	assert_true(dsw->delta_has_compression);
	/* delta value checks */
	assert_int_equal((*(uint32 *) (&dsw->compare_item)), 23);
	assert_int_equal(dsw->deltas_count, 3);
	assert_int_equal(dsw->delta_sign[dsw->deltas_count-1], false);
	assert_int_equal(dsw->deltas[dsw->deltas_count-1], 53 - 23);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 3);
	assert_int_equal(dsw->delta_bitmap.bitCount,4);
	assert_true(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));

	/* 
	 * Fifth value, this must be processed as delta exceeding case 
	 */
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(23 + MAX_DELTA_SUPPORTED_DELTA_COMPRESSION + 1));
	assert_int_equal(status, DELTA_COMPRESSION_NOT_APPLIED);
	assert_true(dsw->delta_has_compression);
	/* delta value checks */
	assert_int_equal((*(uint32 *) (&dsw->compare_item)), 23);
	assert_int_equal(dsw->deltas_count, 3);
	assert_int_equal(dsw->delta_sign[dsw->deltas_count-1], false);
	assert_int_equal(dsw->deltas[dsw->deltas_count-1], 53 - 23);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 3);
	assert_int_equal(dsw->delta_bitmap.bitCount,4);
	assert_true(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));
	assert_true(dsw->not_first_datum);

	/* Again since physical datum, test the the routines for processing the same */
	DatumStreamBlockWrite_DenseIncrItem(dsw, 0, 4);
	DatumStreamBlockWrite_DeltaMaintain(dsw, UInt32GetDatum(23 + MAX_DELTA_SUPPORTED_DELTA_COMPRESSION + 1));
	assert_int_equal(dsw->physical_datum_count, 2);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 3);
	assert_int_equal(dsw->delta_bitmap.bitCount, 5);
	assert_false(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));

	/* 
	 * Sixth value, this must be processed as delta exceeding case 
	*/
	status = DatumStreamBlockWrite_PerformDeltaCompression(dsw, UInt32GetDatum(63));
	assert_int_equal(status, DELTA_COMPRESSION_OK);
	assert_true(dsw->delta_has_compression);
	/* delta value checks */
	assert_int_equal((*(uint32 *) (&dsw->compare_item)), 63);
	assert_int_equal(dsw->deltas_count, 4);
	assert_int_equal(dsw->delta_sign[dsw->deltas_count-1], false);
	assert_int_equal(dsw->deltas[dsw->deltas_count-1], 23 + MAX_DELTA_SUPPORTED_DELTA_COMPRESSION + 1 - 63);
	/* corresponding bitmap checks */
	assert_int_equal(dsw->delta_bitmap.bitOnCount, 4);
	assert_int_equal(dsw->delta_bitmap.bitCount,6);
	assert_true(DatumStreamBitMapWrite_CurrentIsOn(&dsw->delta_bitmap));

	free(dsw->datum_buffer);
	free(dsw->null_bitmap_buffer);
	free(dsw->rle_compress_bitmap_buffer);
	free(dsw->rle_repeatcounts);
	free(dsw->delta_bitmap_buffer);
	free(dsw->deltas);
	free(dsw->delta_sign);
	free(dsw);
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__DeltaCompression__Core)
	};
	return run_tests(tests);
}
