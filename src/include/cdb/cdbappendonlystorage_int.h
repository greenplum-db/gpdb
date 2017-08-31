/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorage_int.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystorage_int.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBAPPENDONLYSTORAGE_INT_H
#define CDBAPPENDONLYSTORAGE_INT_H

#include "cdb/cdbappendonlystorage.h"

/*
 * For checksum protection that doesn't require examining the unchecked data,
 * we always use a 64 bit header.  
 *
 * So, 64 bit header + [ block checksum + header checksum ].  When the table
 * global flag indicates checksumming, there will be 2 32-bit checksums.  The
 * header checksum protects the header and the block checksum.  If it is
 * valid, the header fields can be used.
 *
 * The first 4 bits of the header are 1 bit reserved and 3 bits header kind.
 * The remaining 60 bits are header kind specific.
 */

/*
 * You can think of this struct as the base class for AOSmallContentHeader, etc.
 */

typedef struct AOHeader
{
/*
 * Can't seem to get this to pack nicely to 64 bits so the header is aligned
 * on an 8 byte boundary, so go to bit shifting...
 */

//  unsigned          reserved0:1;  
//                        /*
//                         * Reserved for future use.
//                         */
//  AOHeaderKind 	  headerKind:3;  
//                        /* 
//                         * Which kind of header is it?
//                         */
//
// **** Begin Header kind specific 60 bits ****
//

    uint32            header_bytes_0_3;
    uint32            header_bytes_4_7;

} AOHeader;

#define AOHeaderGet_reserved0(h)          ((h)->header_bytes_0_3>>31)                  // 1 bit
#define AOHeaderGet_headerKind(h)         (((h)->header_bytes_0_3&0x70000000)>>28)    // 3 bit

typedef struct AOSmallContentHeader
{
/*
 * The original (first) Block header.  It was used for Append-Only Row in first.  Then, later for
 * Column oriented tables.
 *
 * It was primarily designed for content that gets compressed by the Append-Only Storage
 * format layer.  Also, if the bulk compression algorithm cannot reduce the size of the original
 * buffer, this block header is still used and sets the compressionLength field to 0 indicating
 * that one block as uncompressed.
 *
 * This block was also designed to store non-compressed data, too.  
 *
 * This block has 2 uses:
 *
 *    1) Store data that all fits within the blocksize.
 *
 *    2) To store the "fragments" of large content that will not all fit within the blocksize.
 *        In this case, we write a AOLargeContentHeader (with no data) to indicate how long
 *        the large content is and then as many AOSmallContentHeader blocks as necessary
 *        to store all the large content.
 *
 * We can't seem to get this to pack nicely to 64 bits so the header is aligned
 * on an 8 byte boundary, so go to bit shifting...
 */

//
// **** These fields are common to all Append-Only Storage Header kinds.  ****
// **** See AOHeader. ****
//
//  unsigned          reserved0:1;  
//                        /*
//                         * Reserved for future use.
//                         */
//  AOHeaderKind 	  headerKind:3;  
//                        /* 
//                         * Which kind of header is it?
//                         */
//
// **** Begin Small Content Header specific 60 bits ****
//
// (NOTE: Previously, the executorBlockKind was 4 bits.  We stole one bit for the
//           hasFirstRowNum flag.  Use of that flag has to be versioned above)
//
//  unsigned          hasFirstRowNum:1;
//				  /*
//                          * When true, the first row number is stored as extra header
//                          * information.
//                          */
//
//  unsigned          executorBlockKind:3;  
//                         /* 
//                          * Executor 3 bit value.  
//                          */
//
//  unsigned          rowCount:14;
//                         /* 
//                          * The number of rows in the block. Maintained in the header so
//                          * blocks can be skipped without decompression, etc.
//                          */
//
//  unsigned          dataLength:21;
//                         /* Length of the data for non-compressed blocks, or
//                          * the uncompressed length for compressed blocks.
//                          */
//  unsigned          compressedLength:21;
//                         /* Length of the block s compressed data; 
//                          * or 0 if non-compressed.
//                          */

    uint32            smallcontent_bytes_0_3;
    uint32            smallcontent_bytes_4_7;

} AOSmallContentHeader;

#define AOSmallContentHeaderGet_reserved0(h)          ((h)->smallcontent_bytes_0_3>>31)                  // 1 bit
#define AOSmallContentHeaderGet_headerKind(h)         (((h)->smallcontent_bytes_0_3&0x70000000)>>28)    // 3 bits
#define AOSmallContentHeaderGet_hasFirstRowNum(h)     (((h)->smallcontent_bytes_0_3&0x08000000)>>27)    // 1 bits
#define AOSmallContentHeaderGet_executorBlockKind(h)  (((h)->smallcontent_bytes_0_3&0x07000000)>>24)    // 3 bits
#define AOSmallContentHeaderGet_rowCount(h)           (((h)->smallcontent_bytes_0_3&0x00FFFC00)>>10)    // 14 bits
#define AOSmallContentHeaderGet_dataLength(h) \
          ((((h)->smallcontent_bytes_0_3&0x000003FF)<<11)|(((h)->smallcontent_bytes_4_7&0xFFE00000)>>21))
                         // top 10 bits                                                          lower 11 bits                                   21 bits
#define AOSmallContentHeaderGet_compressedLength(h)   ((h)->smallcontent_bytes_4_7&0x001FFFFF)         // 21 bits

// For single bits, set or clear directly.  Otherwise, use AND to clear field then OR to set.
#define AOSmallContentHeaderSet_reserved0(h,e)         {if(e)(h)->smallcontent_bytes_0_3|=0x80000000;else(h)->smallcontent_bytes_0_3&=0x7FFFFFFF;} 
#define AOSmallContentHeaderSet_headerKind(h,e)        {(h)->smallcontent_bytes_0_3&=0x8FFFFFFF;(h)->smallcontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOSmallContentHeaderSet_hasFirstRowNum(h,e)    {if(e)(h)->smallcontent_bytes_0_3|=0x08000000;else(h)->smallcontent_bytes_0_3&=0xF7FFFFFF;}
#define AOSmallContentHeaderSet_executorBlockKind(h,e) {(h)->smallcontent_bytes_0_3&=0xF8FFFFFF;(h)->smallcontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOSmallContentHeaderSet_rowCount(h,e)          {(h)->smallcontent_bytes_0_3&=0xFF0003FF;(h)->smallcontent_bytes_0_3|=(0x00FFFC00&((e)<<10));}
#define AOSmallContentHeaderSet_dataLength(h,e)        {(h)->smallcontent_bytes_0_3&=0xFFFFFC00;(h)->smallcontent_bytes_0_3|=(0x000003FF&(e>>11));\
                                                 (h)->smallcontent_bytes_4_7&=0x001FFFFF;(h)->smallcontent_bytes_4_7|=(0xFFE00000&((e)<<21));}
#define AOSmallContentHeaderSet_compressedLength(h,e)  {(h)->smallcontent_bytes_4_7&=0xFFE00000;(h)->smallcontent_bytes_4_7|=(0x001FFFFF&(e));}

// Assume field is initially zero.
#define AOSmallContentHeaderInit_Init(h)                {(h)->smallcontent_bytes_0_3=0;(h)->smallcontent_bytes_4_7=0;} 
#define AOSmallContentHeaderInit_reserved0(h,e)         {if(e)(h)->smallcontent_bytes_0_3|=0x80000000;} 
#define AOSmallContentHeaderInit_headerKind(h,e)        {(h)->smallcontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOSmallContentHeaderInit_hasFirstRowNum(h,e)    {if(e)(h)->smallcontent_bytes_0_3|=0x08000000;} 
#define AOSmallContentHeaderInit_executorBlockKind(h,e) {(h)->smallcontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOSmallContentHeaderInit_rowCount(h,e)          {(h)->smallcontent_bytes_0_3|=(0x00FFFC00&((e)<<10));}

#define AOSmallContentHeaderInit_dataLength(h,e)        {(h)->smallcontent_bytes_0_3|=(0x000003FF&(e>>11));\
                                                  (h)->smallcontent_bytes_4_7|=(0xFFE00000&((e)<<21));}
#define AOSmallContentHeaderInit_compressedLength(h,e)  {(h)->smallcontent_bytes_4_7|=(0x001FFFFF&(e));}


typedef struct AOLargeContentHeader
{

// **** These fields are common to all Append-Only Storage Header kinds.  ****
// **** See AOHeader. ****
//
//  unsigned          reserved0:1;  
//                        /*
//                         * Reserved for future use.
//                         */
//  AOHeaderKind 	  headerKind:3;  
//                        /* 
//                         * Which kind of header is it?
//                         */
//
// **** Begin Large Content Info Header specific 60 bits ****
//
//  unsigned          hasFirstRowNum:1;
//				  /*
//                          * When true, the first row number is stored as extra header
//                          * information.
//                          */
//
//  unsigned          executorBlockKind:3;  
//                         /* 
//                          * Executor 3 bit value.  
//                          */
//
//
//  unsigned          reserved1:1;
//                        /*
//                         * Reserved for future use.
//                         */
//
//  unsigned          largeRowCount:25;
//                         /* 
//                          * The number of rows in the large content. Maintained in the header so
//                          * blocks can be skipped without decompression, etc.
//                          */
//
//  unsigned          largeContentLength:30;
//                         /* 
//                          * Total byte length of the large content, from 1 to 1Gb - 1 bytes.
//                          */

    uint32            largecontent_bytes_0_3;
    uint32            largecontent_bytes_4_7;

} AOLargeContentHeader;

#define AOLargeContentHeaderGet_reserved0(h)          ((h)->largecontent_bytes_0_3>>31)                  // 1 bit
#define AOLargeContentHeaderGet_headerKind(h)         (((h)->largecontent_bytes_0_3&0x70000000)>>28)    // 3 bits
#define AOLargeContentHeaderGet_hasFirstRowNum(h)     (((h)->largecontent_bytes_0_3&0x08000000)>>27)    // 1 bits
#define AOLargeContentHeaderGet_executorBlockKind(h)  (((h)->largecontent_bytes_0_3&0x07000000)>>24)    // 3 bits
#define AOLargeContentHeaderGet_reserved1(h)          (((h)->largecontent_bytes_0_3&0x00800000)>>23)    // 1 bit
#define AOLargeContentHeaderGet_largeRowCount(h) \
          ((((h)->largecontent_bytes_0_3&0x007FFFFF)<<2)|(((h)->largecontent_bytes_4_7&0xC0000000)>>30))
                         // top 23 bits                                                             lower 2 bits                                      25 bits
#define AOLargeContentHeaderGet_largeContentLength(h)   ((h)->largecontent_bytes_4_7&0x3FFFFFFF)         // 30 bits

// For single bits, set or clear directly.  Otherwise, use AND to clear field then OR to set.
#define AOLargeContentHeaderSet_reserved0(h,e)         {if(e)(h)->largecontent_bytes_0_3|=0x80000000;else(h)->largecontent_bytes_0_3&=0x7FFFFFFF;} 
#define AOLargeContentHeaderSet_headerKind(h,e)        {(h)->largecontent_bytes_0_3&=0x8FFFFFFF;(h)->largecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOLargeContentHeaderSet_hasFirstRowNum(h,e)    {if(e)(h)->largecontent_bytes_0_3|=0x08000000;else(h)->largecontent_bytes_0_3&=0xF7FFFFFF;}
#define AOLargeContentHeaderSet_executorBlockKind(h,e) {(h)->largecontent_bytes_0_3&=0xF8FFFFFF;(h)->largecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOLargeContentHeaderSet_reserved1(h,e)         {if(e)(h)->largecontent_bytes_0_3|=0x00800000;else(h)->largecontent_bytes_0_3&=0xFF7FFFFF;} 
#define AOLargeContentHeaderSet_largeRowCount(h,e)     {(h)->largecontent_bytes_0_3&=0xFF800000;(h)->largecontent_bytes_0_3|=(0x007FFFFF&(e>>2));\
                                                       (h)->largecontent_bytes_4_7&=0x3FFFFFFF;(h)->largecontent_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AOLargeContentHeaderSet_largeContentLength(h,e){(h)->largecontent_bytes_4_7&=0xC0000000;(h)->largecontent_bytes_4_7|=(0x3FFFFFFF&(e));}

// Assume field is initially zero.
#define AOLargeContentHeaderInit_Init(h)                {(h)->largecontent_bytes_0_3=0;(h)->largecontent_bytes_4_7=0;} 
#define AOLargeContentHeaderInit_reserved0(h,e)         {if(e)(h)->largecontent_bytes_0_3|=0x80000000;} 
#define AOLargeContentHeaderInit_headerKind(h,e)        {(h)->largecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOLargeContentHeaderInit_hasFirstRowNum(h,e)    {if(e)(h)->largecontent_bytes_0_3|=0x08000000;} 
#define AOLargeContentHeaderInit_executorBlockKind(h,e) {(h)->largecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOLargeContentHeaderInit_reserved1(h,e)         {if(e)(h)->largecontent_bytes_0_3|=0x00800000;} 
#define AOLargeContentHeaderInit_largeRowCount(h,e)     {(h)->largecontent_bytes_0_3|=(0x007FFFFF&(e>>2));\
                                                        (h)->largecontent_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AOLargeContentHeaderInit_largeContentLength(h,e){(h)->largecontent_bytes_4_7|=(0x3FFFFFFF&(e));}

typedef struct AONonBulkDenseContentHeader
{
/*
 * A new Block header intended for Column oriented tables that do their own compression
 * and need more row numbers per block.
 *
 */

//
// **** These fields are common to all Append-Only Storage Header kinds.  ****
// **** See AOHeader. ****
//
//  unsigned          reserved0:1;  
//                        /*
//                         * Reserved for future use.
//                         */
//  AOHeaderKind 	  headerKind:3;  
//                        /* 
//                         * Which kind of header is it?
//                         */
//
// **** Begin Block Header specific 60 bits ****
//
// (NOTE: Previously, the executorBlockKind was 4 bits.  We stole one bit for the
//           hasFirstRowNum flag.  Use of that flag has to be versioned above)
//
//  unsigned          hasFirstRowNum:1;
//				  /*
//                          * When true, the first row number is stored as extra header
//                          * information.
//                          */
//
//  unsigned          executorBlockKind:3;  
//                         /* 
//                          * Executor 3 bit value.  
//                          */
//
//  unsigned          reserved1:3;
//                        /*
//                         * Reserved for future use.
//                         */
//
//  unsigned          dataLength:21;
//                         /* Length of the data for non-compressed blocks, or
//                          * the uncompressed length for compressed blocks.
//                          */
//
//  unsigned          reserved2:2;
//                        /*
//                         * Reserved for future use.
//                         */
//
//  unsigned          largeRowCount:30;
//                         /* 
//                          * The number of rows in the dense content. Maintained in the header so
//                          * blocks can be skipped without decompression, etc.
//                          */
//

    uint32            nonbulkdensecontent_bytes_0_3;
    uint32            nonbulkdensecontent_bytes_4_7;

} AONonBulkDenseContentHeader;

#define AONonBulkDenseContentHeaderGet_reserved0(h)          ((h)->nonbulkdensecontent_bytes_0_3>>31)                  // 1 bit
#define AONonBulkDenseContentHeaderGet_headerKind(h)         (((h)->nonbulkdensecontent_bytes_0_3&0x70000000)>>28)    // 3 bits
#define AONonBulkDenseContentHeaderGet_hasFirstRowNum(h)     (((h)->nonbulkdensecontent_bytes_0_3&0x08000000)>>27)    // 1 bits
#define AONonBulkDenseContentHeaderGet_executorBlockKind(h)  (((h)->nonbulkdensecontent_bytes_0_3&0x07000000)>>24)    // 3 bits

#define AONonBulkDenseContentHeaderGet_reserved1(h)          (((h)->nonbulkdensecontent_bytes_0_3&0x00E00000)>>21)    // 3 bits
#define AONonBulkDenseContentHeaderGet_dataLength(h)   	  ((h)->nonbulkdensecontent_bytes_0_3&0x001FFFFF)         // 21 bits
#define AONonBulkDenseContentHeaderGet_reserved2(h)          (((h)->nonbulkdensecontent_bytes_4_7&0xC0000000)>>30)    // 2 bits
#define AONonBulkDenseContentHeaderGet_largeRowCount(h) 	  ((h)->nonbulkdensecontent_bytes_4_7&0x3FFFFFFF)         // 30 bits
                         

// For single bits, set or clear directly.  Otherwise, use AND to clear field then OR to set.
#define AONonBulkDenseContentHeaderSet_reserved0(h,e)         {if(e)(h)->nonbulkdensecontent_bytes_0_3|=0x80000000;else(h)->nonbulkdensecontent_bytes_0_3&=0x7FFFFFFF;} 
#define AONonBulkDenseContentHeaderSet_headerKind(h,e)        {(h)->nonbulkdensecontent_bytes_0_3&=0x8FFFFFFF;(h)->nonbulkdensecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AONonBulkDenseContentHeaderSet_hasFirstRowNum(h,e)    {if(e)(h)->nonbulkdensecontent_bytes_0_3|=0x08000000;else(h)->nonbulkdensecontent_bytes_0_3&=0xF7FFFFFF;}
#define AONonBulkDenseContentHeaderSet_executorBlockKind(h,e) {(h)->nonbulkdensecontent_bytes_0_3&=0xF8FFFFFF;(h)->nonbulkdensecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AONonBulkDenseContentHeaderSet_reserved1(h,e)         {(h)->nonbulkdensecontent_bytes_0_3&=0xFF1FFFFF;(h)->nonbulkdensecontent_bytes_0_3|=(0x00E00000&((e)<<21));}
#define AONonBulkDenseContentHeaderSet_dataLength(h,e)        {(h)->nonbulkdensecontent_bytes_0_3&=0xFFE00000;(h)->nonbulkdensecontent_bytes_0_3|=(0x001FFFFF&(e));}
#define AONonBulkDenseContentHeaderSet_reserved2(h,e)         {(h)->nonbulkdensecontent_bytes_4_7&=0x3FFFFFFF;(h)->nonbulkdensecontent_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AONonBulkDenseContentHeaderSet_largeRowCount(h,e)     {(h)->nonbulkdensecontent_bytes_4_7&=0xC0000000;(h)->nonbulkdensecontent_bytes_4_7|=(0x3FFFFFFF&(e));

// Assume field is initially zero.
#define AONonBulkDenseContentHeaderInit_Init(h)                {(h)->nonbulkdensecontent_bytes_0_3=0;(h)->nonbulkdensecontent_bytes_4_7=0;} 
#define AONonBulkDenseContentHeaderInit_reserved0(h,e)         {if(e)(h)->nonbulkdensecontent_bytes_0_3|=0x80000000;} 
#define AONonBulkDenseContentHeaderInit_headerKind(h,e)        {(h)->nonbulkdensecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AONonBulkDenseContentHeaderInit_hasFirstRowNum(h,e)    {if(e)(h)->nonbulkdensecontent_bytes_0_3|=0x08000000;} 
#define AONonBulkDenseContentHeaderInit_executorBlockKind(h,e) {(h)->nonbulkdensecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AONonBulkDenseContentHeaderInit_reserved1(h,e)         {(h)->nonbulkdensecontent_bytes_0_3|=(0x00F00000&((e)<<20));}
#define AONonBulkDenseContentHeaderInit_dataLength(h,e)        {(h)->nonbulkdensecontent_bytes_0_3|=(0x001FFFFF&(e));}
#define AONonBulkDenseContentHeaderInit_reserved2(h,e)         {(h)->nonbulkdensecontent_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AONonBulkDenseContentHeaderInit_largeRowCount(h,e)     {(h)->nonbulkdensecontent_bytes_4_7|=(0x3FFFFFFF&(e));}


typedef struct AOBulkDenseContentHeader
{
/*
 * A new header designed for Column oriented tables to both store a very large row count when
 * there is good access method compression and bulk compression is being used, too.
 *
 * It was primarily designed for content that gets compressed by the Append-Only Storage
 * format layer.  Also, if the bulk compression algorithm cannot reduce the size of the original
 * buffer, this block header is still used and sets the compressionLength field to 0 indicating
 * that one block as uncompressed.
 *
 * This block was also designed to store non-compressed data, too.  
 *
 * This block has 2 uses:
 *
 *    1) Store data that all fits within the blocksize.
 *
 *    2) To store the "fragments" of large content that will not all fit within the blocksize.
 *        In this case, we write a AOLargeContentHeader (with no data) to indicate how long
 *        the large content is and then as many AOBulkDenseContentHeader blocks as necessary
 *        to store all the large content.
 *
 * We can't seem to get this to pack nicely to 64 bits so the header is aligned
 * on an 8 byte boundary, so go to bit shifting...
 */

//
// **** These fields are common to all Append-Only Storage Header kinds.  ****
// **** See AOHeader. ****
//
//  unsigned          reserved0:1;  
//                        /*
//                         * Reserved for future use.
//                         */
//  AOHeaderKind 	  headerKind:3;  
//                        /* 
//                         * Which kind of header is it?
//                         */
//
// **** Begin Bulk Dense Content Header specific 60 bits ****
//
// (NOTE: Previously, the executorBlockKind was 4 bits.  We stole one bit for the
//           hasFirstRowNum flag.  Use of that flag has to be versioned above)
//
//  unsigned          hasFirstRowNum:1;
//				  /*
//                          * When true, the first row number is stored as extra header
//                          * information.
//                          */
//
//  unsigned          executorBlockKind:3;  
//                         /* 
//                          * Executor 3 bit value.  
//                          */
//
//  unsigned          reserved1:14;
//                         /* 
//                          * Reserved for future use.
//                          */
//
//  unsigned          dataLength:21;
//                         /* Length of the data for non-compressed blocks, or
//                          * the uncompressed length for compressed blocks.
//                          */
//  unsigned          compressedLength:21;
//                         /* Length of the block s compressed data; 
//                          * or 0 if non-compressed.
//                          */

    uint32            bulkdensecontent_bytes_0_3;
    uint32            bulkdensecontent_bytes_4_7;

} AOBulkDenseContentHeader;

typedef struct AOBulkDenseContentHeaderExt
{
/*
 * The 2nd 64 bit of the header that must go after the checksums as a separate part.
 */

// **** Begin 2nd 64 bits ****
//
//  unsigned          reserved2:32;
//                        /*
//                         * Reserved for future use.
//                         */
//
//  unsigned          reserved3:2;
//                        /*
//                         * Reserved for future use.
//                         */
//
//  unsigned          largeRowCount:30;
//                         /* 
//                          * The number of rows in the dense content. Maintained in the header so
//                          * blocks can be skipped without decompression, etc.
//                          */
//

    uint32            bulkdensecontent_ext_bytes_0_3;
    uint32            bulkdensecontent_ext_bytes_4_7;

} AOBulkDenseContentHeaderExt;

#define AOBulkDenseContentHeaderGet_reserved0(h)          ((h)->bulkdensecontent_bytes_0_3>>31)                  // 1 bit
#define AOBulkDenseContentHeaderGet_headerKind(h)         (((h)->bulkdensecontent_bytes_0_3&0x70000000)>>28)    // 3 bits
#define AOBulkDenseContentHeaderGet_hasFirstRowNum(h)     (((h)->bulkdensecontent_bytes_0_3&0x08000000)>>27)    // 1 bits
#define AOBulkDenseContentHeaderGet_executorBlockKind(h)  (((h)->bulkdensecontent_bytes_0_3&0x07000000)>>24)    // 3 bits
#define AOBulkDenseContentHeaderGet_reserved1(h)          (((h)->bulkdensecontent_bytes_0_3&0x00FFFC00)>>10)    // 14 bits
#define AOBulkDenseContentHeaderGet_dataLength(h) \
          ((((h)->bulkdensecontent_bytes_0_3&0x000003FF)<<11)|(((h)->bulkdensecontent_bytes_4_7&0xFFE00000)>>21))
                         // top 10 bits                                                          lower 11 bits                                   21 bits
#define AOBulkDenseContentHeaderGet_compressedLength(h)   ((h)->bulkdensecontent_bytes_4_7&0x001FFFFF)            // 21 bits

#define AOBulkDenseContentHeaderExtGet_reserved2(h)       ((h)->bulkdensecontent_ext_bytes_0_3)    				  // 32 bits
#define AOBulkDenseContentHeaderExtGet_reserved3(h)       (((h)->bulkdensecontent_ext_bytes_4_7&0xC0000000)>>30)   // 2 bits
#define AOBulkDenseContentHeaderExtGet_largeRowCount(h)   ((h)->bulkdensecontent_ext_bytes_4_7&0x3FFFFFFF)         // 30 bits

// For single bits, set or clear directly.  Otherwise, use AND to clear field then OR to set.
#define AOBulkDenseContentHeaderSet_reserved0(h,e)         {if(e)(h)->bulkdensecontent_bytes_0_3|=0x80000000;else(h)->bulkdensecontent_bytes_0_3&=0x7FFFFFFF;} 
#define AOBulkDenseContentHeaderSet_headerKind(h,e)        {(h)->bulkdensecontent_bytes_0_3&=0x8FFFFFFF;(h)->bulkdensecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOBulkDenseContentHeaderSet_hasFirstRowNum(h,e)    {if(e)(h)->bulkdensecontent_bytes_0_3|=0x08000000;else(h)->bulkdensecontent_bytes_0_3&=0xF7FFFFFF;}
#define AOBulkDenseContentHeaderSet_executorBlockKind(h,e) {(h)->bulkdensecontent_bytes_0_3&=0xF8FFFFFF;(h)->bulkdensecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOBulkDenseContentHeaderSet_reserved1(h,e)         {(h)->bulkdensecontent_bytes_0_3&=0xFF0003FF;(h)->bulkdensecontent_bytes_0_3|=(0x00FFFC00&((e)<<10));}
#define AOBulkDenseContentHeaderSet_dataLength(h,e)        {(h)->bulkdensecontent_bytes_0_3&=0xFFFFFC00;(h)->bulkdensecontent_bytes_0_3|=(0x000003FF&(e>>11));\
                                                 (h)->bulkdensecontent_bytes_4_7&=0x001FFFFF;(h)->bulkdensecontent_bytes_4_7|=(0xFFE00000&((e)<<21));}
#define AOBulkDenseContentHeaderSet_compressedLength(h,e)  {(h)->bulkdensecontent_bytes_4_7&=0xFFE00000;(h)->bulkdensecontent_bytes_4_7|=(0x001FFFFF&(e));}

#define AOBulkDenseContentHeaderExtSet_reserved2(h,e)      {(h)->bulkdensecontent_ext_bytes_0_3 =(e);}
#define AOBulkDenseContentHeaderExtSet_reserved3(h,e)      {(h)->bulkdensecontent_ext_bytes_4_7&=0x3FFFFFFF;(h)->bulkdensecontent_ext_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AOBulkDenseContentHeaderExtSet_largeRowCount(h,e)  {(h)->bulkdensecontent_ext_bytes_4_7&=0xC0000000;(h)->bulkdensecontent_ext_bytes_4_7|=(0x3FFFFFFF&(e));

// Assume field is initially zero.
#define AOBulkDenseContentHeaderInit_Init(h)                {(h)->bulkdensecontent_bytes_0_3=0;(h)->bulkdensecontent_bytes_4_7=0;} 
#define AOBulkDenseContentHeaderInit_reserved0(h,e)         {if(e)(h)->bulkdensecontent_bytes_0_3|=0x80000000;} 
#define AOBulkDenseContentHeaderInit_headerKind(h,e)        {(h)->bulkdensecontent_bytes_0_3|=(0x70000000&((e)<<28));}
#define AOBulkDenseContentHeaderInit_hasFirstRowNum(h,e)    {if(e)(h)->bulkdensecontent_bytes_0_3|=0x08000000;} 
#define AOBulkDenseContentHeaderInit_executorBlockKind(h,e) {(h)->bulkdensecontent_bytes_0_3|=(0x07000000&((e)<<24));}
#define AOBulkDenseContentHeaderInit_reserved1(h,e)         {(h)->bulkdensecontent_bytes_0_3|=(0x00FFFC00&((e)<<10));}

#define AOBulkDenseContentHeaderInit_dataLength(h,e)        {(h)->bulkdensecontent_bytes_0_3|=(0x000003FF&(e>>11));\
                                                  (h)->bulkdensecontent_bytes_4_7|=(0xFFE00000&((e)<<21));}
#define AOBulkDenseContentHeaderInit_compressedLength(h,e)  {(h)->bulkdensecontent_bytes_4_7|=(0x001FFFFF&(e));}

#define AOBulkDenseContentHeaderExtInit_Init(h)             {(h)->bulkdensecontent_ext_bytes_0_3=0;(h)->bulkdensecontent_ext_bytes_4_7=0;} 
#define AOBulkDenseContentHeaderExtInit_reserved2(h,e)      {(h)->bulkdensecontent_ext_bytes_0_3 =(e);}
#define AOBulkDenseContentHeaderExtInit_reserved3(h,e)      {(h)->bulkdensecontent_ext_bytes_4_7|=(0xC0000000&((e)<<30));}
#define AOBulkDenseContentHeaderExtInit_largeRowCount(h,e)  {(h)->bulkdensecontent_ext_bytes_4_7|=(0x3FFFFFFF&(e));}

#endif   /* CDBAPPENDONLYSTORAGE_INT_H */



