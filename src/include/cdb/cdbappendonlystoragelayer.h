/*-------------------------------------------------------------------------
 *
 * cdbappendonlystoragelayer.h
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbappendonlystoragelayer.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBAPPENDONLYSTORAGELAYER_H
#define CDBAPPENDONLYSTORAGELAYER_H

/*
 *
 * General Discussion
 *
 *     This section discusses general information about the Append-Only Storage Layer routines
 *     that are used by the executor.
 *
 *     Executor content is data from 1 byte to 1Gb -1 bytes in length.
 *
 *     Executor content is stored by the Append-Only Storage Layer in blocks.  The layer
 *     handles formatting the storage blocks, optional bulk-compression, and fragmenting large
 *     content into multiple blocks.
 *
 *     The typical use case is content is collected until it will no longer fit in one Append-Only
 *     Storage Block (i.e. blocksize) and then the storage layer is told to write it.
 *
 *     Larger content is fully supported, too.
 *
 *     We assume content contains an integral number of rows and in the future will provide a
 *     storage block directory that quickly locates the storage block that has a row given the
 *     row number.
 *
 *    The executor supplies a small integer value that describes the content (format) when
 *    writing.  The integer is returned on reading to aid the executor in interpreting the content.
 *
 * Session
 *
 *    The executor is responsible for allocating and keeping a small struct that holds data
 *    about a writing or reading session.  The best thing is to initially zero out the memory since
 *    we treat that as no active session.  We provide ~Init routines for starting a session.
 *
 *    UNDONE: Session close / file close responsibility.
 *    UNDONE: Who is responsible for flushing write files?
 *
 * Memory Alignment
 *
 *     For efficent memory use, content buffers will be 64-bit aligned.  There is a compatibility
 *     mode for older uses of 32-bit alignment.
 *
 *    Note that content is not required to be an integral number of 64-bit or 32-bit items.
 *    Content can be any byte length in the range 1 through 1Gb � 1.
 *
 * Logical EOF
 *
 *     For Append-Only Tables the end of data is versioned.  When a transaction successfully
 *     commits it adds a new version � that is, more data that can be read by future
 *     transactions.  The versioned EOFs are kept in a system catalog table.  Readers look
 *     through their transaction snapshot and get the logical EOF for data they are suppose
 *     to see, which can be less than what is currently committed.
 *
 *    We call it the logical EOF because the physical EOF is the wrong value to look at for
 *    both writing and reading.  For writing, the system catalog has the last committed EOF
 *    value.  The physical EOF might be beyond the logical EOF if that previous transaction
 *    aborted.  And, for reading, the EOF to use is the versioned EOF value, which is not
 *    necessarily the physical EOF value.  Thus, you will see a logicalEof parameter on the
 *    write and read routines that supply the file to use.
 *
 * Error Handling
 *
 *    Exceptions are thrown (via ereport) when errors are encountered.
 */

/*
 * This structure contains attribute values about the
 * Append-Only relation that are used by the storage
 * layer.
 */
typedef struct AppendOnlyStorageAttributes
{
	bool				compress;
			/* When true, content compression used. */

	char				*compressType;
			/*
			 * The bulk-compression used on the contents
			 * of the Append-Only Storage Blocks.
			 */
	int					compressLevel;
			/*
			 * A parameter directs the bulk-compression
			 * library on how to compress/decompress.
			 *
			 * The level can be equally important on
			 * decompression since it may specify which
			 * sub-library to use for decompress
			 * (e.g. QuickLZ).
			 */
	int					overflowSize;
			/*
			 * The additional size (in bytes) required by the bulk-compression
			 * algorithm.
			 */

	bool				checksum;
			/*
 			 * When true, checksums protect the header
			 * and content.  Otherwise, no checksums.
			 */

	int					safeFSWriteSize;
			/*
			 * The page round out with zero padding byte length.
			 * When 0, do no zero pad.
			 */

} AppendOnlyStorageAttributes;

/*
 * Safe initialization of AppendOnlyStorageAttributes struct.
 *
 * Use this define to initialize the attributes structure
 * so any future attributes get good default values.
 */
#define AppendOnlyStorageAttributes_Init(storageAttributes)\
{\
	MemSet(&storageAttributes, 0, sizeof(storageAttributes));\
}

#endif   /* CDBAPPENDONLYSTORAGELAYER_H */
