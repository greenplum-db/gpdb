/******************************************************************
 * Code heavily borrowed from the below author. It is primarily a  
 * conversion to C from C++. It also adds header funcs to encode
 * and decode arbitrarily sized arrays. All Chinese characters were
 * also removed.
 *
 * The original group varint encoding was developed by Google, Inc.
 * under the Create Commons Attribution 3.0 License and the Apache 2.0
 * License.
 *
 * ORIGINAL AUTHOR INFO BELOW
 * -----------------------------------------------------------------
 * Created on: 2011-4-26
 *      Author: yewang@taobao.com clm971910@gmail.com
 *
 ******************************************************************/

#ifndef VARINT_H_
#define VARINT_H_

#include <stdint.h>
#include <stddef.h>

#define   MAX_VARINT32_BYTES   5
#define   MAX_UINT8            256
#define   MAX_UINT16           65536
#define   MAX_UINT24           16777216
#define   MAX_UINT32           4294967296

#define   GROUP_VARINT_DECODE(idx)              \
    valueArr[0] = ((struct GROUP_VARINT_TYPE_##idx *) star)[0].u0; \
    valueArr[1] = ((struct GROUP_VARINT_TYPE_##idx *) star)[0].u1; \
    valueArr[2] = ((struct GROUP_VARINT_TYPE_##idx *) star)[0].u2; \
    valueArr[3] = ((struct GROUP_VARINT_TYPE_##idx *) star)[0].u3

/* ---------------------- function declarations ------------------------ */
uint8_t * varint_encode_uint32 ( uint32_t value, uint8_t * target);
const uint8_t * varint_decode_uint32(const uint8_t * buffer, uint32_t * value);
uint8_t * group_varint_encode_uint32 ( uint32_t * valueArr, uint8_t * target);
const uint8_t * group_varint_decode_uint32 ( const uint8_t * buf, uint32_t * valueArr);

uint32_t group_encode_sorted(uint32_t * input, int32_t length, uint8_t * output);
void group_decode_sorted(const uint8_t * input, int32_t length, uint32_t * output);

uint32_t zigZag_encode32(int32_t  n);
inline uint64_t zigZag_encode64(int64_t  n);
int32_t  zigZag_decode32(uint32_t n);
inline int64_t  zigZag_decode64(uint64_t n);

/* ---------------------- function definitions --------------------------- */
/** offset lookup table for decoding
 */
static const int GROUP_VARINT_IDX_ARR[256][5] =
{
    /* 00 00 00 00 */ {1, 2, 3, 4, 5},
    /* 00 00 00 01 */ {1, 2, 3, 4, 6},
    /* 00 00 00 10 */ {1, 2, 3, 4, 7},
    /* 00 00 00 11 */ {1, 2, 3, 4, 8},

    /* 00 00 01 00 */ {1, 2, 3, 5, 6},
    /* 00 00 01 01 */ {1, 2, 3, 5, 7},
    /* 00 00 01 10 */ {1, 2, 3, 5, 8},
    /* 00 00 01 11 */ {1, 2, 3, 5, 9},

    /* 00 00 10 00 */ {1, 2, 3, 6, 7},
    /* 00 00 10 01 */ {1, 2, 3, 6, 8},
    /* 00 00 10 10 */ {1, 2, 3, 6, 9},
    /* 00 00 10 11 */ {1, 2, 3, 6, 10},

    /* 00 00 11 00 */ {1, 2, 3, 7, 8},
    /* 00 00 11 01 */ {1, 2, 3, 7, 9},
    /* 00 00 11 10 */ {1, 2, 3, 7, 10},
    /* 00 00 11 11 */ {1, 2, 3, 7, 11},

    /* 00 01 00 00 */ {1, 2, 4, 5, 6},
    /* 00 01 00 01 */ {1, 2, 4, 5, 7},
    /* 00 01 00 10 */ {1, 2, 4, 5, 8},
    /* 00 01 00 11 */ {1, 2, 4, 5, 9},

    /* 00 01 01 00 */ {1, 2, 4, 6, 7},
    /* 00 01 01 01 */ {1, 2, 4, 6, 8},
    /* 00 01 01 10 */ {1, 2, 4, 6, 9},
    /* 00 01 01 11 */ {1, 2, 4, 6, 10},

    /* 00 01 10 00 */ {1, 2, 4, 7, 8},
    /* 00 01 10 01 */ {1, 2, 4, 7, 9},
    /* 00 01 10 10 */ {1, 2, 4, 7, 10},
    /* 00 01 10 11 */ {1, 2, 4, 7, 11},

    /* 00 01 11 00 */ {1, 2, 4, 8, 9},
    /* 00 01 11 01 */ {1, 2, 4, 8, 10},
    /* 00 01 11 10 */ {1, 2, 4, 8, 11},
    /* 00 01 11 11 */ {1, 2, 4, 8, 12},

    /* 00 10 00 00 */ {1, 2, 5, 6, 7},
    /* 00 10 00 01 */ {1, 2, 5, 6, 8},
    /* 00 10 00 10 */ {1, 2, 5, 6, 9},
    /* 00 10 00 11 */ {1, 2, 5, 6, 10},

    /* 00 10 01 00 */ {1, 2, 5, 7, 8},
    /* 00 10 01 01 */ {1, 2, 5, 7, 9},
    /* 00 10 01 10 */ {1, 2, 5, 7, 10},
    /* 00 10 01 11 */ {1, 2, 5, 7, 11},

    /* 00 10 10 00 */ {1, 2, 5, 8, 9},
    /* 00 10 10 01 */ {1, 2, 5, 8, 10},
    /* 00 10 10 10 */ {1, 2, 5, 8, 11},
    /* 00 10 10 11 */ {1, 2, 5, 8, 12},

    /* 00 10 11 00 */ {1, 2, 5, 9, 10},
    /* 00 10 11 01 */ {1, 2, 5, 9, 11},
    /* 00 10 11 10 */ {1, 2, 5, 9, 12},
    /* 00 10 11 11 */ {1, 2, 5, 9, 13},

    /* 00 11 00 00 */ {1, 2, 6, 7, 8},
    /* 00 11 00 01 */ {1, 2, 6, 7, 9},
    /* 00 11 00 10 */ {1, 2, 6, 7, 10},
    /* 00 11 00 11 */ {1, 2, 6, 7, 11},

    /* 00 11 01 00 */ {1, 2, 6, 8, 9},
    /* 00 11 01 01 */ {1, 2, 6, 8, 10},
    /* 00 11 01 10 */ {1, 2, 6, 8, 11},
    /* 00 11 01 11 */ {1, 2, 6, 8, 12},

    /* 00 11 10 00 */ {1, 2, 6, 9, 10},
    /* 00 11 10 01 */ {1, 2, 6, 9, 11},
    /* 00 11 10 10 */ {1, 2, 6, 9, 12},
    /* 00 11 10 11 */ {1, 2, 6, 9, 13},

    /* 00 11 11 00 */ {1, 2, 6, 10, 11},
    /* 00 11 11 01 */ {1, 2, 6, 10, 12},
    /* 00 11 11 10 */ {1, 2, 6, 10, 13},
    /* 00 11 11 11 */ {1, 2, 6, 10, 14},

    /* 01 00 00 00 */ {1, 3, 4, 5, 6},
    /* 01 00 00 01 */ {1, 3, 4, 5, 7},
    /* 01 00 00 10 */ {1, 3, 4, 5, 8},
    /* 01 00 00 11 */ {1, 3, 4, 5, 9},

    /* 01 00 01 00 */ {1, 3, 4, 6, 7},
    /* 01 00 01 01 */ {1, 3, 4, 6, 8},
    /* 01 00 01 10 */ {1, 3, 4, 6, 9},
    /* 01 00 01 11 */ {1, 3, 4, 6, 10},

    /* 01 00 10 00 */ {1, 3, 4, 7, 8},
    /* 01 00 10 01 */ {1, 3, 4, 7, 9},
    /* 01 00 10 10 */ {1, 3, 4, 7, 10},
    /* 01 00 10 11 */ {1, 3, 4, 7, 11},

    /* 01 00 11 00 */ {1, 3, 4, 8, 9},
    /* 01 00 11 01 */ {1, 3, 4, 8, 10},
    /* 01 00 11 10 */ {1, 3, 4, 8, 11},
    /* 01 00 11 11 */ {1, 3, 4, 8, 12},

    /* 01 01 00 00 */ {1, 3, 5, 6, 7},
    /* 01 01 00 01 */ {1, 3, 5, 6, 8},
    /* 01 01 00 10 */ {1, 3, 5, 6, 9},
    /* 01 01 00 11 */ {1, 3, 5, 6, 10},

    /* 01 01 01 00 */ {1, 3, 5, 7, 8},
    /* 01 01 01 01 */ {1, 3, 5, 7, 9},
    /* 01 01 01 10 */ {1, 3, 5, 7, 10},
    /* 01 01 01 11 */ {1, 3, 5, 7, 11},

    /* 01 01 10 00 */ {1, 3, 5, 8, 9},
    /* 01 01 10 01 */ {1, 3, 5, 8, 10},
    /* 01 01 10 10 */ {1, 3, 5, 8, 11},
    /* 01 01 10 11 */ {1, 3, 5, 8, 12},

    /* 01 01 11 00 */ {1, 3, 5, 9, 10},
    /* 01 01 11 01 */ {1, 3, 5, 9, 11},
    /* 01 01 11 10 */ {1, 3, 5, 9, 12},
    /* 01 01 11 11 */ {1, 3, 5, 9, 13},

    /* 01 10 00 00 */ {1, 3, 6, 7, 8},
    /* 01 10 00 01 */ {1, 3, 6, 7, 9},
    /* 01 10 00 10 */ {1, 3, 6, 7, 10},
    /* 01 10 00 11 */ {1, 3, 6, 7, 11},

    /* 01 10 01 00 */ {1, 3, 6, 8, 9},
    /* 01 10 01 01 */ {1, 3, 6, 8, 10},
    /* 01 10 01 10 */ {1, 3, 6, 8, 11},
    /* 01 10 01 11 */ {1, 3, 6, 8, 12},

    /* 01 10 10 00 */ {1, 3, 6, 9, 10},
    /* 01 10 10 01 */ {1, 3, 6, 9, 11},
    /* 01 10 10 10 */ {1, 3, 6, 9, 12},
    /* 01 10 10 11 */ {1, 3, 6, 9, 13},

    /* 01 10 11 00 */ {1, 3, 6, 10, 11},
    /* 01 10 11 01 */ {1, 3, 6, 10, 12},
    /* 01 10 11 10 */ {1, 3, 6, 10, 13},
    /* 01 10 11 11 */ {1, 3, 6, 10, 14},

    /* 01 11 00 00 */ {1, 3, 7, 8, 9},
    /* 01 11 00 01 */ {1, 3, 7, 8, 10},
    /* 01 11 00 10 */ {1, 3, 7, 8, 11},
    /* 01 11 00 11 */ {1, 3, 7, 8, 12},

    /* 01 11 01 00 */ {1, 3, 7, 9, 10},
    /* 01 11 01 01 */ {1, 3, 7, 9, 11},
    /* 01 11 01 10 */ {1, 3, 7, 9, 12},
    /* 01 11 01 11 */ {1, 3, 7, 9, 13},

    /* 01 11 10 00 */ {1, 3, 7, 10, 11},
    /* 01 11 10 01 */ {1, 3, 7, 10, 12},
    /* 01 11 10 10 */ {1, 3, 7, 10, 13},
    /* 01 11 10 11 */ {1, 3, 7, 10, 14},

    /* 01 11 11 00 */ {1, 3, 7, 11, 12},
    /* 01 11 11 01 */ {1, 3, 7, 11, 13},
    /* 01 11 11 10 */ {1, 3, 7, 11, 14},
    /* 01 11 11 11 */ {1, 3, 7, 11, 15},

    /* 10 00 00 00 */ {1, 4, 5, 6, 7},
    /* 10 00 00 01 */ {1, 4, 5, 6, 8},
    /* 10 00 00 10 */ {1, 4, 5, 6, 9},
    /* 10 00 00 11 */ {1, 4, 5, 6, 10},

    /* 10 00 01 00 */ {1, 4, 5, 7, 8},
    /* 10 00 01 01 */ {1, 4, 5, 7, 9},
    /* 10 00 01 10 */ {1, 4, 5, 7, 10},
    /* 10 00 01 11 */ {1, 4, 5, 7, 11},

    /* 10 00 10 00 */ {1, 4, 5, 8, 9},
    /* 10 00 10 01 */ {1, 4, 5, 8, 10},
    /* 10 00 10 10 */ {1, 4, 5, 8, 11},
    /* 10 00 10 11 */ {1, 4, 5, 8, 12},

    /* 10 00 11 00 */ {1, 4, 5, 9, 10},
    /* 10 00 11 01 */ {1, 4, 5, 9, 11},
    /* 10 00 11 10 */ {1, 4, 5, 9, 12},
    /* 10 00 11 11 */ {1, 4, 5, 9, 13},

    /* 10 01 00 00 */ {1, 4, 6, 7, 8},
    /* 10 01 00 01 */ {1, 4, 6, 7, 9},
    /* 10 01 00 10 */ {1, 4, 6, 7, 10},
    /* 10 01 00 11 */ {1, 4, 6, 7, 11},

    /* 10 01 01 00 */ {1, 4, 6, 8, 9},
    /* 10 01 01 01 */ {1, 4, 6, 8, 10},
    /* 10 01 01 10 */ {1, 4, 6, 8, 11},
    /* 10 01 01 11 */ {1, 4, 6, 8, 12},

    /* 10 01 10 00 */ {1, 4, 6, 9, 10},
    /* 10 01 10 01 */ {1, 4, 6, 9, 11},
    /* 10 01 10 10 */ {1, 4, 6, 9, 12},
    /* 10 01 10 11 */ {1, 4, 6, 9, 13},

    /* 10 01 11 00 */ {1, 4, 6, 10, 11},
    /* 10 01 11 01 */ {1, 4, 6, 10, 12},
    /* 10 01 11 10 */ {1, 4, 6, 10, 13},
    /* 10 01 11 11 */ {1, 4, 6, 10, 14},

    /* 10 10 00 00 */ {1, 4, 7, 8, 9},
    /* 10 10 00 01 */ {1, 4, 7, 8, 10},
    /* 10 10 00 10 */ {1, 4, 7, 8, 11},
    /* 10 10 00 11 */ {1, 4, 7, 8, 12},

    /* 10 10 01 00 */ {1, 4, 7, 9, 10},
    /* 10 10 01 01 */ {1, 4, 7, 9, 11},
    /* 10 10 01 10 */ {1, 4, 7, 9, 12},
    /* 10 10 01 11 */ {1, 4, 7, 9, 13},

    /* 10 10 10 00 */ {1, 4, 7, 10, 11},
    /* 10 10 10 01 */ {1, 4, 7, 10, 12},
    /* 10 10 10 10 */ {1, 4, 7, 10, 13},
    /* 10 10 10 11 */ {1, 4, 7, 10, 14},

    /* 10 10 11 00 */ {1, 4, 7, 11, 12},
    /* 10 10 11 01 */ {1, 4, 7, 11, 13},
    /* 10 10 11 10 */ {1, 4, 7, 11, 14},
    /* 10 10 11 11 */ {1, 4, 7, 11, 15},

    /* 10 11 00 00 */ {1, 4, 8, 9, 10},
    /* 10 11 00 01 */ {1, 4, 8, 9, 11},
    /* 10 11 00 10 */ {1, 4, 8, 9, 12},
    /* 10 11 00 11 */ {1, 4, 8, 9, 13},

    /* 10 11 01 00 */ {1, 4, 8, 10, 11},
    /* 10 11 01 01 */ {1, 4, 8, 10, 12},
    /* 10 11 01 10 */ {1, 4, 8, 10, 13},
    /* 10 11 01 11 */ {1, 4, 8, 10, 14},

    /* 10 11 10 00 */ {1, 4, 8, 11, 12},
    /* 10 11 10 01 */ {1, 4, 8, 11, 13},
    /* 10 11 10 10 */ {1, 4, 8, 11, 14},
    /* 10 11 10 11 */ {1, 4, 8, 11, 15},

    /* 10 11 11 00 */ {1, 4, 8, 12, 13},
    /* 10 11 11 01 */ {1, 4, 8, 12, 14},
    /* 10 11 11 10 */ {1, 4, 8, 12, 15},
    /* 10 11 11 11 */ {1, 4, 8, 12, 16},

    /* 11 00 00 00 */ {1, 5, 6, 7, 8},
    /* 11 00 00 01 */ {1, 5, 6, 7, 9},
    /* 11 00 00 10 */ {1, 5, 6, 7, 10},
    /* 11 00 00 11 */ {1, 5, 6, 7, 11},

    /* 11 00 01 00 */ {1, 5, 6, 8, 9},
    /* 11 00 01 01 */ {1, 5, 6, 8, 10},
    /* 11 00 01 10 */ {1, 5, 6, 8, 11},
    /* 11 00 01 11 */ {1, 5, 6, 8, 12},

    /* 11 00 10 00 */ {1, 5, 6, 9, 10},
    /* 11 00 10 01 */ {1, 5, 6, 9, 11},
    /* 11 00 10 10 */ {1, 5, 6, 9, 12},
    /* 11 00 10 11 */ {1, 5, 6, 9, 13},

    /* 11 00 11 00 */ {1, 5, 6, 10, 11},
    /* 11 00 11 01 */ {1, 5, 6, 10, 12},
    /* 11 00 11 10 */ {1, 5, 6, 10, 13},
    /* 11 00 11 11 */ {1, 5, 6, 10, 14},

    /* 11 01 00 00 */ {1, 5, 7, 8, 9},
    /* 11 01 00 01 */ {1, 5, 7, 8, 10},
    /* 11 01 00 10 */ {1, 5, 7, 8, 11},
    /* 11 01 00 11 */ {1, 5, 7, 8, 12},

    /* 11 01 01 00 */ {1, 5, 7, 9, 10},
    /* 11 01 01 01 */ {1, 5, 7, 9, 11},
    /* 11 01 01 10 */ {1, 5, 7, 9, 12},
    /* 11 01 01 11 */ {1, 5, 7, 9, 13},

    /* 11 01 10 00 */ {1, 5, 7, 10, 11},
    /* 11 01 10 01 */ {1, 5, 7, 10, 12},
    /* 11 01 10 10 */ {1, 5, 7, 10, 13},
    /* 11 01 10 11 */ {1, 5, 7, 10, 14},

    /* 11 01 11 00 */ {1, 5, 7, 11, 12},
    /* 11 01 11 01 */ {1, 5, 7, 11, 13},
    /* 11 01 11 10 */ {1, 5, 7, 11, 14},
    /* 11 01 11 11 */ {1, 5, 7, 11, 15},

    /* 11 10 00 00 */ {1, 5, 8, 9, 10},
    /* 11 10 00 01 */ {1, 5, 8, 9, 11},
    /* 11 10 00 10 */ {1, 5, 8, 9, 12},
    /* 11 10 00 11 */ {1, 5, 8, 9, 13},

    /* 11 10 01 00 */ {1, 5, 8, 10, 11},
    /* 11 10 01 01 */ {1, 5, 8, 10, 12},
    /* 11 10 01 10 */ {1, 5, 8, 10, 13},
    /* 11 10 01 11 */ {1, 5, 8, 10, 14},

    /* 11 10 10 00 */ {1, 5, 8, 11, 12},
    /* 11 10 10 01 */ {1, 5, 8, 11, 13},
    /* 11 10 10 10 */ {1, 5, 8, 11, 14},
    /* 11 10 10 11 */ {1, 5, 8, 11, 15},

    /* 11 10 11 00 */ {1, 5, 8, 12, 13},
    /* 11 10 11 01 */ {1, 5, 8, 12, 14},
    /* 11 10 11 10 */ {1, 5, 8, 12, 15},
    /* 11 10 11 11 */ {1, 5, 8, 12, 16},

    /* 11 11 00 00 */ {1, 5, 9, 10, 11},
    /* 11 11 00 01 */ {1, 5, 9, 10, 12},
    /* 11 11 00 10 */ {1, 5, 9, 10, 13},
    /* 11 11 00 11 */ {1, 5, 9, 10, 14},

    /* 11 11 01 00 */ {1, 5, 9, 11, 12},
    /* 11 11 01 01 */ {1, 5, 9, 11, 13},
    /* 11 11 01 10 */ {1, 5, 9, 11, 14},
    /* 11 11 01 11 */ {1, 5, 9, 11, 15},

    /* 11 11 10 00 */ {1, 5, 9, 12, 13},
    /* 11 11 10 01 */ {1, 5, 9, 12, 14},
    /* 11 11 10 10 */ {1, 5, 9, 12, 15},
    /* 11 11 10 11 */ {1, 5, 9, 12, 16},

    /* 11 11 11 00 */ {1, 5, 9, 13, 14},
    /* 11 11 11 01 */ {1, 5, 9, 13, 15},
    /* 11 11 11 10 */ {1, 5, 9, 13, 16},
    /* 11 11 11 11 */ {1, 5, 9, 13, 17}
};

/* Bitpacked structs used to dissasemble encoded data 
 * Corresponds to the above offsets*/
#pragma pack(1)
struct GROUP_VARINT_TYPE_0   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_1   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_2   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_3   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_4   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_5   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_6   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_7   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_8   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_9   { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_10  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_11  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_12  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_13  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_14  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_15  { uint32_t u0:8;   uint32_t u1:8;   uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_16  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_17  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_18  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_19  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_20  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_21  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_22  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_23  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_24  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_25  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_26  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_27  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_28  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_29  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_30  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_31  { uint32_t u0:8;   uint32_t u1:16;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_32  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_33  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_34  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_35  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_36  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_37  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_38  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_39  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_40  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_41  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_42  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_43  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_44  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_45  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_46  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_47  { uint32_t u0:8;   uint32_t u1:24;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_48  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_49  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_50  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_51  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_52  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_53  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_54  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_55  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_56  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_57  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_58  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_59  { uint32_t u0:8;   uint32_t u1;     uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_60  { uint32_t u0:8;   uint32_t u1;     uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_61  { uint32_t u0:8;   uint32_t u1;     uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_62  { uint32_t u0:8;   uint32_t u1;     uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_63  { uint32_t u0:8;   uint32_t u1;     uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_64  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_65  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_66  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_67  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_68  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_69  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_70  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_71  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_72  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_73  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_74  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_75  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_76  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_77  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_78  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_79  { uint32_t u0:16;   uint32_t u1:8;   uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_80  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_81  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_82  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_83  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_84  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_85  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_86  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_87  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_88  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_89  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_90  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_91  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_92  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_93  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_94  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_95  { uint32_t u0:16;   uint32_t u1:16;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_96  { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_97  { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_98  { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_99  { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_100 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_101 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_102 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_103 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_104 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_105 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_106 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_107 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_108 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_109 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_110 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_111 { uint32_t u0:16;   uint32_t u1:24;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_112 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_113 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_114 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_115 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_116 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_117 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_118 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_119 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_120 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_121 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_122 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_123 { uint32_t u0:16;   uint32_t u1;     uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_124 { uint32_t u0:16;   uint32_t u1;     uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_125 { uint32_t u0:16;   uint32_t u1;     uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_126 { uint32_t u0:16;   uint32_t u1;     uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_127 { uint32_t u0:16;   uint32_t u1;     uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_128 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_129 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_130 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_131 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_132 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_133 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_134 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_135 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_136 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_137 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_138 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_139 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_140 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_141 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_142 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_143 { uint32_t u0:24;   uint32_t u1:8;   uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_144 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_145 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_146 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_147 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_148 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_149 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_150 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_151 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_152 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_153 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_154 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_155 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_156 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_157 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_158 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_159 { uint32_t u0:24;   uint32_t u1:16;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_160 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_161 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_162 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_163 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_164 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_165 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_166 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_167 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_168 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_169 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_170 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_171 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_172 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_173 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_174 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_175 { uint32_t u0:24;   uint32_t u1:24;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_176 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_177 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_178 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_179 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_180 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_181 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_182 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_183 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_184 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_185 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_186 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_187 { uint32_t u0:24;   uint32_t u1;     uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_188 { uint32_t u0:24;   uint32_t u1;     uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_189 { uint32_t u0:24;   uint32_t u1;     uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_190 { uint32_t u0:24;   uint32_t u1;     uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_191 { uint32_t u0:24;   uint32_t u1;     uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_192 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_193 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_194 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_195 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_196 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_197 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_198 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_199 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_200 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_201 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_202 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_203 { uint32_t u0;      uint32_t u1:8;   uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_204 { uint32_t u0;      uint32_t u1:8;   uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_205 { uint32_t u0;      uint32_t u1:8;   uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_206 { uint32_t u0;      uint32_t u1:8;   uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_207 { uint32_t u0;      uint32_t u1:8;   uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_208 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_209 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_210 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_211 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_212 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_213 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_214 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_215 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_216 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_217 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_218 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_219 { uint32_t u0;      uint32_t u1:16;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_220 { uint32_t u0;      uint32_t u1:16;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_221 { uint32_t u0;      uint32_t u1:16;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_222 { uint32_t u0;      uint32_t u1:16;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_223 { uint32_t u0;      uint32_t u1:16;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_224 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_225 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_226 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_227 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_228 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_229 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_230 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_231 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_232 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_233 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_234 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_235 { uint32_t u0;      uint32_t u1:24;  uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_236 { uint32_t u0;      uint32_t u1:24;  uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_237 { uint32_t u0;      uint32_t u1:24;  uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_238 { uint32_t u0;      uint32_t u1:24;  uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_239 { uint32_t u0;      uint32_t u1:24;  uint32_t u2;    uint32_t u3; };

struct GROUP_VARINT_TYPE_240 { uint32_t u0;      uint32_t u1;     uint32_t u2:8;  uint32_t u3:8; };
struct GROUP_VARINT_TYPE_241 { uint32_t u0;      uint32_t u1;     uint32_t u2:8;  uint32_t u3:16; };
struct GROUP_VARINT_TYPE_242 { uint32_t u0;      uint32_t u1;     uint32_t u2:8;  uint32_t u3:24; };
struct GROUP_VARINT_TYPE_243 { uint32_t u0;      uint32_t u1;     uint32_t u2:8;  uint32_t u3; };

struct GROUP_VARINT_TYPE_244 { uint32_t u0;      uint32_t u1;     uint32_t u2:16; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_245 { uint32_t u0;      uint32_t u1;     uint32_t u2:16; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_246 { uint32_t u0;      uint32_t u1;     uint32_t u2:16; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_247 { uint32_t u0;      uint32_t u1;     uint32_t u2:16; uint32_t u3; };

struct GROUP_VARINT_TYPE_248 { uint32_t u0;      uint32_t u1;     uint32_t u2:24; uint32_t u3:8; };
struct GROUP_VARINT_TYPE_249 { uint32_t u0;      uint32_t u1;     uint32_t u2:24; uint32_t u3:16; };
struct GROUP_VARINT_TYPE_250 { uint32_t u0;      uint32_t u1;     uint32_t u2:24; uint32_t u3:24; };
struct GROUP_VARINT_TYPE_251 { uint32_t u0;      uint32_t u1;     uint32_t u2:24; uint32_t u3; };

struct GROUP_VARINT_TYPE_252 { uint32_t u0;      uint32_t u1;     uint32_t u2;    uint32_t u3:8; };
struct GROUP_VARINT_TYPE_253 { uint32_t u0;      uint32_t u1;     uint32_t u2;    uint32_t u3:16; };
struct GROUP_VARINT_TYPE_254 { uint32_t u0;      uint32_t u1;     uint32_t u2;    uint32_t u3:24; };
struct GROUP_VARINT_TYPE_255 { uint32_t u0;      uint32_t u1;     uint32_t u2;    uint32_t u3; };
#pragma pack()

/** Basic varint encoding for uint32 values
 *  Encodes one value using 7-bits per byte and 1 continuation bit
 */
inline uint8_t *
varint_encode_uint32 ( uint32_t value, uint8_t * target)
{
    target[0] = (uint8_t)(value | 0x80);

    if ( value >= (1 << 7) )
    {
        target[1] = (uint8_t)( (value >>  7) | 0x80 );

        if ( value >= (1 << 14) )
        {
            target[2] = (uint8_t)( (value >> 14) | 0x80 );

            if ( value >= (1 << 21) )
            {
                target[3] = (uint8_t)((value >> 21) | 0x80);

                if ( value >= (1 << 28) )
                {
                    target[4] = (uint8_t)(value >> 28);
                    return target + 5;
                }
                else
                {
                    target[3] &= 0x7F;
                    return target + 4;
                }
            }
            else
            {
                target[2] &= 0x7F;
                return target + 3;
            }
        }
        else
        {
            target[1] &= 0x7F;
            return target + 2;
        }
    }
    else
    {
        target[0] &= 0x7F;
        return target + 1;
    }
}

/** Decodes uint32s from an array of uint8_t bytes
 *  Decodes uint32a encoded with 7 bits/byte and 1 continuation bit
 */
inline const uint8_t *
varint_decode_uint32(const uint8_t * buffer, uint32_t * value)
{
    const uint8_t * ptr = buffer;

    uint32_t  i = 0;
    uint32_t  b;
    uint32_t  result;

    b = *(ptr++); result  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

    for (; i < MAX_VARINT32_BYTES; i++)
    {
        b = *(ptr++); if (!(b & 0x80)) goto done;
    }

    return NULL;
done:
    *value = result;
    return ptr;
}

/* Encodes groups of 4 uint32's using group varint encoding
 * The first byte consists of 4 2-bit codes used to represent
 * the lengths (in bytes) of the 4 uints.
 * 
 * Tag byte structure:
 * [ 00 | 00 | 01 | 10 ]
 * 00 => 1-byte number
 * 00 => 1-byte number
 * 01 => 2-byte number 
 * 10 => 3-byte number
 * 11 => 4-byte number (NOTE: Not included in the tag byte example)
 *
 * Encode example (using above tag byte):
 * The four uint's below:
 * 1) [00000000] [00000000] [00000000] [00000001]
 * 2) [00000000] [00000000] [00000000] [00001111]
 * 3) [00000000] [00000000] [11111111] [00000001]
 * 4) [00000000] [11110000] [00001111] [10101010]
 * 
 * encode into the following:
 *    tag     |     #1     |     #2     |     #3                | =>
 * [00000110] | [00000001] | [00001111] | [11111111] [00000001] | =>
 *
 * |                #4                |
 * | [11110000] [00001111] [10101010] |
 * 
 * This takes 16-Bytes and encodes it into 8-bytes. Obviously ratios are
 * specific to numbers encoded.
 */
inline uint8_t *
group_varint_encode_uint32 ( uint32_t * valueArr, uint8_t * target)
{
    uint8_t len1 = 0;
    uint8_t len2 = 0;
    uint8_t len3 = 0;
    uint8_t len4 = 0;
    uint8_t len  = 0;

    uint8_t * buf = target + 1;

    if ( valueArr[0] < MAX_UINT8 )
    {
        len1   = 1;
        buf[0] = (uint8_t) valueArr[0];
    }
    else if ( valueArr[0] < MAX_UINT16 )
    {
        len1 = 2;
        ((uint16_t *)(buf))[0] = (uint16_t) valueArr[0];
    }
    else if ( valueArr[0] < MAX_UINT24 )
    {
        len1 = 3;
        ((uint32_t *)(buf))[0] = ( valueArr[0] );
    } else  {
        len1 = 4;
        ((uint32_t *)(buf))[0] = valueArr[0];
    }
    len = len1;

    if ( valueArr[1] < MAX_UINT8 )
    {
        len2 = 1;
        buf[len] = (uint8_t) valueArr[1];
    }
    else if ( valueArr[1] < MAX_UINT16 )
    {
        len2 = 2;
        ((uint16_t *)(buf + len))[0] = (uint16_t) valueArr[1];
    }
    else if ( valueArr[1] < MAX_UINT24 )
    {
        len2 = 3;
        ((uint32_t *)(buf + len))[0] = ( valueArr[1] );
    } else  {
        len2 = 4;
        ((uint32_t *)(buf + len))[0] = valueArr[1];
    }
    len = len + len2;

    if ( valueArr[2] < MAX_UINT8 )
    {
        len3 = 1;
        buf[len] = (uint8_t) valueArr[2];
    }
    else if ( valueArr[2] < MAX_UINT16 )
    {
        len3 = 2;
        ((uint16_t *)(buf + len))[0] = (uint16_t) valueArr[2];
    }
    else if ( valueArr[2] < MAX_UINT24 )
    {
        len3 = 3;
        ((uint32_t *)(buf + len))[0] = ( valueArr[2] );
    } else  {
        len3 = 4;
        ((uint32_t *)(buf + len))[0] = valueArr[2];
    }
    len = len + len3;

    if ( valueArr[3] < MAX_UINT8 )
    {
        len4 = 1;
        buf[len] = (uint8_t) valueArr[3];
    }
    else if ( valueArr[3] < MAX_UINT16 )
    {
        len4 = 2;
        ((uint16_t *)(buf + len))[0] = (uint16_t) valueArr[3];
    }
    else if ( valueArr[3] < MAX_UINT24 )
    {
        len4 = 3;
        ((uint32_t *)(buf + len))[0] = ( valueArr[3] );
    } else  {
        len4 = 4;
        ((uint32_t *)(buf + len))[0] = valueArr[3];
    }
    len = len + len4;

    target[0] = ((len1 - 1) << 6) | ((len2 - 1) << 4) | ((len3 - 1) << 2) | (len4 - 1);

    return buf + len;
}

/* Takes an arbitrarily sized array of sorted uint32's and group varint
 * encodes the deltas. Remainder values (if length of array isn't 
 * divisible by 4) are encoded with continuation bit varint encoding.
 */
uint32_t
group_encode_sorted(uint32_t * input,int32_t length ,uint8_t * output)
{
    int i,offset=0;
    uint8_t *  buffer = output;
    uint32_t set[4] = {0};

    /* encode each group of 4 based on deltas */
    if (length > 3){
        for(i=0; i*sizeof(uint32_t) < length - 3; i++){
            set[0] = input[i*sizeof(uint32_t)] - offset;
            set[1] = input[i*sizeof(uint32_t) + 1] - input[i*sizeof(uint32_t)];
            set[2] = input[i*sizeof(uint32_t) + 2] - input[i*sizeof(uint32_t) + 1];
            set[3] = input[i*sizeof(uint32_t) + 3] - input[i*sizeof(uint32_t) + 2];
            offset = input[i*sizeof(uint32_t) + 3];
            buffer = group_varint_encode_uint32(set,buffer);
        }
    }

    /* encode remainders by using continuation bit encoding*/
    if (length % 4 == 1){
        buffer = varint_encode_uint32(input[length - 1] - offset,buffer);
    }  else if (length % 4 == 2){
        buffer = varint_encode_uint32(input[length - 2] - offset,buffer);
        buffer = varint_encode_uint32(input[length - 1] - input[length - 2],buffer);
    } else if (length % 4 == 3){
        buffer = varint_encode_uint32(input[length - 3] - offset,buffer);
        buffer = varint_encode_uint32(input[length - 2] - input[length - 3],buffer);
        buffer = varint_encode_uint32(input[length - 1] - input[length - 2],buffer);
    }

    return (char *)buffer - (char *)output;
}

/* Decode bytes that represent group-varint-encoded values into 4 uint32s
 */
inline const uint8_t *
group_varint_decode_uint32 ( const uint8_t * buf, uint32_t * valueArr)
{
    const uint8_t * star = buf + 1;

    switch ( buf[0] )
    {
        case 0  :  GROUP_VARINT_DECODE(0);    break;
        case 1  :  GROUP_VARINT_DECODE(1);    break;
        case 2  :  GROUP_VARINT_DECODE(2);    break;
        case 3  :  GROUP_VARINT_DECODE(3);    break;
        case 4  :  GROUP_VARINT_DECODE(4);    break;
        case 5  :  GROUP_VARINT_DECODE(5);    break;
        case 6  :  GROUP_VARINT_DECODE(6);    break;
        case 7  :  GROUP_VARINT_DECODE(7);    break;
        case 8  :  GROUP_VARINT_DECODE(8);    break;
        case 9  :  GROUP_VARINT_DECODE(9);    break;
        case 10 :  GROUP_VARINT_DECODE(10);   break;
        case 11 :  GROUP_VARINT_DECODE(11);   break;
        case 12 :  GROUP_VARINT_DECODE(12);   break;
        case 13 :  GROUP_VARINT_DECODE(13);   break;
        case 14 :  GROUP_VARINT_DECODE(14);   break;
        case 15 :  GROUP_VARINT_DECODE(15);   break;
        case 16 :  GROUP_VARINT_DECODE(16);   break;
        case 17 :  GROUP_VARINT_DECODE(17);   break;
        case 18 :  GROUP_VARINT_DECODE(18);   break;
        case 19 :  GROUP_VARINT_DECODE(19);   break;
        case 20 :  GROUP_VARINT_DECODE(20);   break;
        case 21 :  GROUP_VARINT_DECODE(21);   break;
        case 22 :  GROUP_VARINT_DECODE(22);   break;
        case 23 :  GROUP_VARINT_DECODE(23);   break;
        case 24 :  GROUP_VARINT_DECODE(24);   break;
        case 25 :  GROUP_VARINT_DECODE(25);   break;
        case 26 :  GROUP_VARINT_DECODE(26);   break;
        case 27 :  GROUP_VARINT_DECODE(27);   break;
        case 28 :  GROUP_VARINT_DECODE(28);   break;
        case 29 :  GROUP_VARINT_DECODE(29);   break;
        case 30 :  GROUP_VARINT_DECODE(30);   break;
        case 31 :  GROUP_VARINT_DECODE(31);   break;
        case 32 :  GROUP_VARINT_DECODE(32);   break;
        case 33 :  GROUP_VARINT_DECODE(33);   break;
        case 34 :  GROUP_VARINT_DECODE(34);   break;
        case 35 :  GROUP_VARINT_DECODE(35);   break;
        case 36 :  GROUP_VARINT_DECODE(36);   break;
        case 37 :  GROUP_VARINT_DECODE(37);   break;
        case 38 :  GROUP_VARINT_DECODE(38);   break;
        case 39 :  GROUP_VARINT_DECODE(39);   break;
        case 40 :  GROUP_VARINT_DECODE(40);   break;
        case 41 :  GROUP_VARINT_DECODE(41);   break;
        case 42 :  GROUP_VARINT_DECODE(42);   break;
        case 43 :  GROUP_VARINT_DECODE(43);   break;
        case 44 :  GROUP_VARINT_DECODE(44);   break;
        case 45 :  GROUP_VARINT_DECODE(45);   break;
        case 46 :  GROUP_VARINT_DECODE(46);   break;
        case 47 :  GROUP_VARINT_DECODE(47);   break;
        case 48 :  GROUP_VARINT_DECODE(48);   break;
        case 49 :  GROUP_VARINT_DECODE(49);   break;
        case 50 :  GROUP_VARINT_DECODE(50);   break;
        case 51 :  GROUP_VARINT_DECODE(51);   break;
        case 52 :  GROUP_VARINT_DECODE(52);   break;
        case 53 :  GROUP_VARINT_DECODE(53);   break;
        case 54 :  GROUP_VARINT_DECODE(54);   break;
        case 55 :  GROUP_VARINT_DECODE(55);   break;
        case 56 :  GROUP_VARINT_DECODE(56);   break;
        case 57 :  GROUP_VARINT_DECODE(57);   break;
        case 58 :  GROUP_VARINT_DECODE(58);   break;
        case 59 :  GROUP_VARINT_DECODE(59);   break;
        case 60 :  GROUP_VARINT_DECODE(60);   break;
        case 61 :  GROUP_VARINT_DECODE(61);   break;
        case 62 :  GROUP_VARINT_DECODE(62);   break;
        case 63 :  GROUP_VARINT_DECODE(63);   break;
        case 64 :  GROUP_VARINT_DECODE(64);   break;
        case 65 :  GROUP_VARINT_DECODE(65);   break;
        case 66 :  GROUP_VARINT_DECODE(66);   break;
        case 67 :  GROUP_VARINT_DECODE(67);   break;
        case 68 :  GROUP_VARINT_DECODE(68);   break;
        case 69 :  GROUP_VARINT_DECODE(69);   break;
        case 70 :  GROUP_VARINT_DECODE(70);   break;
        case 71 :  GROUP_VARINT_DECODE(71);   break;
        case 72 :  GROUP_VARINT_DECODE(72);   break;
        case 73 :  GROUP_VARINT_DECODE(73);   break;
        case 74 :  GROUP_VARINT_DECODE(74);   break;
        case 75 :  GROUP_VARINT_DECODE(75);   break;
        case 76 :  GROUP_VARINT_DECODE(76);   break;
        case 77 :  GROUP_VARINT_DECODE(77);   break;
        case 78 :  GROUP_VARINT_DECODE(78);   break;
        case 79 :  GROUP_VARINT_DECODE(79);   break;
        case 80 :  GROUP_VARINT_DECODE(80);   break;
        case 81 :  GROUP_VARINT_DECODE(81);   break;
        case 82 :  GROUP_VARINT_DECODE(82);   break;
        case 83 :  GROUP_VARINT_DECODE(83);   break;
        case 84 :  GROUP_VARINT_DECODE(84);   break;
        case 85 :  GROUP_VARINT_DECODE(85);   break;
        case 86 :  GROUP_VARINT_DECODE(86);   break;
        case 87 :  GROUP_VARINT_DECODE(87);   break;
        case 88 :  GROUP_VARINT_DECODE(88);   break;
        case 89 :  GROUP_VARINT_DECODE(89);   break;
        case 90 :  GROUP_VARINT_DECODE(90);   break;
        case 91 :  GROUP_VARINT_DECODE(91);   break;
        case 92 :  GROUP_VARINT_DECODE(92);   break;
        case 93 :  GROUP_VARINT_DECODE(93);   break;
        case 94 :  GROUP_VARINT_DECODE(94);   break;
        case 95 :  GROUP_VARINT_DECODE(95);   break;
        case 96 :  GROUP_VARINT_DECODE(96);   break;
        case 97 :  GROUP_VARINT_DECODE(97);   break;
        case 98 :  GROUP_VARINT_DECODE(98);   break;
        case 99 :  GROUP_VARINT_DECODE(99);   break;
        case 100 :  GROUP_VARINT_DECODE(100);   break;
        case 101 :  GROUP_VARINT_DECODE(101);   break;
        case 102 :  GROUP_VARINT_DECODE(102);   break;
        case 103 :  GROUP_VARINT_DECODE(103);   break;
        case 104 :  GROUP_VARINT_DECODE(104);   break;
        case 105 :  GROUP_VARINT_DECODE(105);   break;
        case 106 :  GROUP_VARINT_DECODE(106);   break;
        case 107 :  GROUP_VARINT_DECODE(107);   break;
        case 108 :  GROUP_VARINT_DECODE(108);   break;
        case 109 :  GROUP_VARINT_DECODE(109);   break;
        case 110 :  GROUP_VARINT_DECODE(110);   break;
        case 111 :  GROUP_VARINT_DECODE(111);   break;
        case 112 :  GROUP_VARINT_DECODE(112);   break;
        case 113 :  GROUP_VARINT_DECODE(113);   break;
        case 114 :  GROUP_VARINT_DECODE(114);   break;
        case 115 :  GROUP_VARINT_DECODE(115);   break;
        case 116 :  GROUP_VARINT_DECODE(116);   break;
        case 117 :  GROUP_VARINT_DECODE(117);   break;
        case 118 :  GROUP_VARINT_DECODE(118);   break;
        case 119 :  GROUP_VARINT_DECODE(119);   break;
        case 120 :  GROUP_VARINT_DECODE(120);   break;
        case 121 :  GROUP_VARINT_DECODE(121);   break;
        case 122 :  GROUP_VARINT_DECODE(122);   break;
        case 123 :  GROUP_VARINT_DECODE(123);   break;
        case 124 :  GROUP_VARINT_DECODE(124);   break;
        case 125 :  GROUP_VARINT_DECODE(125);   break;
        case 126 :  GROUP_VARINT_DECODE(126);   break;
        case 127 :  GROUP_VARINT_DECODE(127);   break;
        case 128 :  GROUP_VARINT_DECODE(128);   break;
        case 129 :  GROUP_VARINT_DECODE(129);   break;
        case 130 :  GROUP_VARINT_DECODE(130);   break;
        case 131 :  GROUP_VARINT_DECODE(131);   break;
        case 132 :  GROUP_VARINT_DECODE(132);   break;
        case 133 :  GROUP_VARINT_DECODE(133);   break;
        case 134 :  GROUP_VARINT_DECODE(134);   break;
        case 135 :  GROUP_VARINT_DECODE(135);   break;
        case 136 :  GROUP_VARINT_DECODE(136);   break;
        case 137 :  GROUP_VARINT_DECODE(137);   break;
        case 138 :  GROUP_VARINT_DECODE(138);   break;
        case 139 :  GROUP_VARINT_DECODE(139);   break;
        case 140 :  GROUP_VARINT_DECODE(140);   break;
        case 141 :  GROUP_VARINT_DECODE(141);   break;
        case 142 :  GROUP_VARINT_DECODE(142);   break;
        case 143 :  GROUP_VARINT_DECODE(143);   break;
        case 144 :  GROUP_VARINT_DECODE(144);   break;
        case 145 :  GROUP_VARINT_DECODE(145);   break;
        case 146 :  GROUP_VARINT_DECODE(146);   break;
        case 147 :  GROUP_VARINT_DECODE(147);   break;
        case 148 :  GROUP_VARINT_DECODE(148);   break;
        case 149 :  GROUP_VARINT_DECODE(149);   break;
        case 150 :  GROUP_VARINT_DECODE(150);   break;
        case 151 :  GROUP_VARINT_DECODE(151);   break;
        case 152 :  GROUP_VARINT_DECODE(152);   break;
        case 153 :  GROUP_VARINT_DECODE(153);   break;
        case 154 :  GROUP_VARINT_DECODE(154);   break;
        case 155 :  GROUP_VARINT_DECODE(155);   break;
        case 156 :  GROUP_VARINT_DECODE(156);   break;
        case 157 :  GROUP_VARINT_DECODE(157);   break;
        case 158 :  GROUP_VARINT_DECODE(158);   break;
        case 159 :  GROUP_VARINT_DECODE(159);   break;
        case 160 :  GROUP_VARINT_DECODE(160);   break;
        case 161 :  GROUP_VARINT_DECODE(161);   break;
        case 162 :  GROUP_VARINT_DECODE(162);   break;
        case 163 :  GROUP_VARINT_DECODE(163);   break;
        case 164 :  GROUP_VARINT_DECODE(164);   break;
        case 165 :  GROUP_VARINT_DECODE(165);   break;
        case 166 :  GROUP_VARINT_DECODE(166);   break;
        case 167 :  GROUP_VARINT_DECODE(167);   break;
        case 168 :  GROUP_VARINT_DECODE(168);   break;
        case 169 :  GROUP_VARINT_DECODE(169);   break;
        case 170 :  GROUP_VARINT_DECODE(170);   break;
        case 171 :  GROUP_VARINT_DECODE(171);   break;
        case 172 :  GROUP_VARINT_DECODE(172);   break;
        case 173 :  GROUP_VARINT_DECODE(173);   break;
        case 174 :  GROUP_VARINT_DECODE(174);   break;
        case 175 :  GROUP_VARINT_DECODE(175);   break;
        case 176 :  GROUP_VARINT_DECODE(176);   break;
        case 177 :  GROUP_VARINT_DECODE(177);   break;
        case 178 :  GROUP_VARINT_DECODE(178);   break;
        case 179 :  GROUP_VARINT_DECODE(179);   break;
        case 180 :  GROUP_VARINT_DECODE(180);   break;
        case 181 :  GROUP_VARINT_DECODE(181);   break;
        case 182 :  GROUP_VARINT_DECODE(182);   break;
        case 183 :  GROUP_VARINT_DECODE(183);   break;
        case 184 :  GROUP_VARINT_DECODE(184);   break;
        case 185 :  GROUP_VARINT_DECODE(185);   break;
        case 186 :  GROUP_VARINT_DECODE(186);   break;
        case 187 :  GROUP_VARINT_DECODE(187);   break;
        case 188 :  GROUP_VARINT_DECODE(188);   break;
        case 189 :  GROUP_VARINT_DECODE(189);   break;
        case 190 :  GROUP_VARINT_DECODE(190);   break;
        case 191 :  GROUP_VARINT_DECODE(191);   break;
        case 192 :  GROUP_VARINT_DECODE(192);   break;
        case 193 :  GROUP_VARINT_DECODE(193);   break;
        case 194 :  GROUP_VARINT_DECODE(194);   break;
        case 195 :  GROUP_VARINT_DECODE(195);   break;
        case 196 :  GROUP_VARINT_DECODE(196);   break;
        case 197 :  GROUP_VARINT_DECODE(197);   break;
        case 198 :  GROUP_VARINT_DECODE(198);   break;
        case 199 :  GROUP_VARINT_DECODE(199);   break;
        case 200 :  GROUP_VARINT_DECODE(200);   break;
        case 201 :  GROUP_VARINT_DECODE(201);   break;
        case 202 :  GROUP_VARINT_DECODE(202);   break;
        case 203 :  GROUP_VARINT_DECODE(203);   break;
        case 204 :  GROUP_VARINT_DECODE(204);   break;
        case 205 :  GROUP_VARINT_DECODE(205);   break;
        case 206 :  GROUP_VARINT_DECODE(206);   break;
        case 207 :  GROUP_VARINT_DECODE(207);   break;
        case 208 :  GROUP_VARINT_DECODE(208);   break;
        case 209 :  GROUP_VARINT_DECODE(209);   break;
        case 210 :  GROUP_VARINT_DECODE(210);   break;
        case 211 :  GROUP_VARINT_DECODE(211);   break;
        case 212 :  GROUP_VARINT_DECODE(212);   break;
        case 213 :  GROUP_VARINT_DECODE(213);   break;
        case 214 :  GROUP_VARINT_DECODE(214);   break;
        case 215 :  GROUP_VARINT_DECODE(215);   break;
        case 216 :  GROUP_VARINT_DECODE(216);   break;
        case 217 :  GROUP_VARINT_DECODE(217);   break;
        case 218 :  GROUP_VARINT_DECODE(218);   break;
        case 219 :  GROUP_VARINT_DECODE(219);   break;
        case 220 :  GROUP_VARINT_DECODE(220);   break;
        case 221 :  GROUP_VARINT_DECODE(221);   break;
        case 222 :  GROUP_VARINT_DECODE(222);   break;
        case 223 :  GROUP_VARINT_DECODE(223);   break;
        case 224 :  GROUP_VARINT_DECODE(224);   break;
        case 225 :  GROUP_VARINT_DECODE(225);   break;
        case 226 :  GROUP_VARINT_DECODE(226);   break;
        case 227 :  GROUP_VARINT_DECODE(227);   break;
        case 228 :  GROUP_VARINT_DECODE(228);   break;
        case 229 :  GROUP_VARINT_DECODE(229);   break;
        case 230 :  GROUP_VARINT_DECODE(230);   break;
        case 231 :  GROUP_VARINT_DECODE(231);   break;
        case 232 :  GROUP_VARINT_DECODE(232);   break;
        case 233 :  GROUP_VARINT_DECODE(233);   break;
        case 234 :  GROUP_VARINT_DECODE(234);   break;
        case 235 :  GROUP_VARINT_DECODE(235);   break;
        case 236 :  GROUP_VARINT_DECODE(236);   break;
        case 237 :  GROUP_VARINT_DECODE(237);   break;
        case 238 :  GROUP_VARINT_DECODE(238);   break;
        case 239 :  GROUP_VARINT_DECODE(239);   break;
        case 240 :  GROUP_VARINT_DECODE(240);   break;
        case 241 :  GROUP_VARINT_DECODE(241);   break;
        case 242 :  GROUP_VARINT_DECODE(242);   break;
        case 243 :  GROUP_VARINT_DECODE(243);   break;
        case 244 :  GROUP_VARINT_DECODE(244);   break;
        case 245 :  GROUP_VARINT_DECODE(245);   break;
        case 246 :  GROUP_VARINT_DECODE(246);   break;
        case 247 :  GROUP_VARINT_DECODE(247);   break;
        case 248 :  GROUP_VARINT_DECODE(248);   break;
        case 249 :  GROUP_VARINT_DECODE(249);   break;
        case 250 :  GROUP_VARINT_DECODE(250);   break;
        case 251 :  GROUP_VARINT_DECODE(251);   break;
        case 252 :  GROUP_VARINT_DECODE(252);   break;
        case 253 :  GROUP_VARINT_DECODE(253);   break;
        case 254 :  GROUP_VARINT_DECODE(254);   break;
        case 255 :  GROUP_VARINT_DECODE(255);   break;
    }

    return buf + GROUP_VARINT_IDX_ARR[ buf[0] ][4];
}

/* Deocde an arbitrarily sized array of group-varint-encoded bytes into
 * an array of uint32's (the uint32 array size is the length parameter).
 * This decoding treats the resulting uint32's as deltas of a sorted array
 * and will reconstruct the original array that the deltas were derived from.
 */
void
group_decode_sorted(const uint8_t * input, int32_t length, uint32_t * output)
{
    int i;
    uint32_t set[4] = {0};
    const uint8_t * buffer = input;
    uint32_t * target = output;
    uint32_t offset = 0;

    /* decode blocks of 4 uints and account for deltas */
    if (length > 3){
        for (i=0; i*4 < length - 3 ; i++ ){
            buffer = group_varint_decode_uint32(buffer,target);
            output[i*4] = output[i*4] + offset;
            output[i*4 + 1] = output[i*4 + 1] + output[i*4];
            output[i*4 + 2] = output[i*4 + 2] + output[i*4 + 1];
            output[i*4 + 3] = output[i*4 + 3] + output[i*4 + 2];
            offset = output[i*4 + 3];
            target += 4;
        }
    }

    /* Decode the remainder continuation bit encoded values */
    if (length % 4 == 1){
        buffer = varint_decode_uint32(buffer,&set[0]);
        output[length - 1] = set[0] + offset;
    }  else if (length % 4 == 2){
        buffer = varint_decode_uint32(buffer,&set[0]);
        buffer = varint_decode_uint32(buffer,&set[1]);
        output[length - 2] = set[0] + offset;
        output[length - 1] = set[1] + output[length - 2];
    } else if (length % 4 == 3){
        buffer = varint_decode_uint32(buffer,&set[0]);
        buffer = varint_decode_uint32(buffer,&set[1]);
        buffer = varint_decode_uint32(buffer,&set[2]);
        output[length - 3] = set[0] + offset;
        output[length - 2] = set[1] + output[length - 3];
        output[length - 1] = set[2] + output[length - 2];
    }


}

/** ZigZag encodes for converting signed values into unsigned
 */
inline uint32_t 
zigZag_encode32(int32_t  n) 
{ 
    return (n << 1) ^ (n >> 31);
}

inline uint64_t 
zigZag_encode64(int64_t  n) 
{ 
    return (n << 1) ^ (n >> 63);
}

/**
 * ZigZag decodes for converting unsigned values into signed
 */
inline int32_t  
zigZag_decode32(uint32_t n) 
{ 
    return (n >> 1) ^ -(int32_t)(n & 1);
}

inline int64_t  
zigZag_decode64(uint64_t n) 
{ 
    return (n >> 1) ^ -(int64_t)(n & 1);
}


#endif /* VARINT_H_ */
