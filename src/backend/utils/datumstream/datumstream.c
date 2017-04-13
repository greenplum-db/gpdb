/*-------------------------------------------------------------------------
 *
 * datumstream.c
 *
 * Portions Copyright (c) 2009, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/datumstream/datumstream.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "access/tupmacs.h"
#include "access/tuptoaster.h"

#include "catalog/pg_attribute_encoding.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/cdbappendonlystoragelayer.h"
#include "cdb/cdbappendonlystorageread.h"
#include "cdb/cdbappendonlystoragewrite.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "utils/datumstream.h"
#include "utils/guc.h"
#include "catalog/pg_compression.h"
#include "utils/faultinjector.h"

typedef enum AOCSBK
{
	AOCSBK_None = 0,
	AOCSBK_BLOCK,
	AOCSBK_BLOB,
}	AOCSBK;


static void
datumstreamread_check_large_varlena_integrity(
											  DatumStreamRead * acc,
											  uint8 * buffer,
											  int32 contentLen)
{
	struct varlena *va;

	va = (struct varlena *) buffer;

	if (contentLen < VARHDRSZ)
	{
		elog(ERROR, "Large varlena header too small.  Found %d, expected at least %d",
			 contentLen,
			 VARHDRSZ);
	}

	if (VARATT_IS_EXTERNAL(va))
	{
		elog(ERROR, "Large varlena has a toast reference but Append-Only Column Store tables do not use toast");
	}
}


static void
datumstreamread_print_large_varlena_info(
										 DatumStreamRead * acc,
										 uint8 * p)
{
	elog(LOG, "Read large varlena <%s>",
		 VarlenaInfoToString(p));
}


/*
 * Error detail and context callback for tracing or errors occurring during reading.
 */
static int
datumstreamread_detail_callback(void *arg)
{
	DatumStreamRead *acc = (DatumStreamRead *) arg;

	/*
	 * Append-Only Storage Read's detail.
	 */
	if (acc->need_close_file)
	{
		errdetail_appendonly_read_storage_content_header(&acc->ao_read);
	}
	return 0;
}

static int
datumstreamread_context_callback(void *arg)
{
	DatumStreamRead *acc = (DatumStreamRead *) arg;

	if (Debug_appendonly_print_datumstream)
		elog(LOG,
		   "datumstream_advance filePathName %s nth %u ndatum %u datump %p ",
			 acc->ao_read.bufferedRead.filePathName,
			 acc->blockRead.nth,
			 acc->blockRead.logical_row_count,
			 acc->blockRead.datump);

	/*
	 * Append-Only Storage Read's context.
	 */
	if (acc->need_close_file)
	{
		errcontext_appendonly_read_storage_block(&acc->ao_read);
	}
	else
	{
		errcontext("%s", acc->title);
	}

	return 0;
}

/*
 * Error detail and context callback for tracing or errors occurring during writing.
 */
static int
datumstreamwrite_detail_callback(void *arg)
{
/*	DatumStreamWrite *acc = (DatumStreamWrite*) arg; */

	/*
	 * Append-Only Storage Write's detail.
	 */
	/* UNDONE */

	return 0;
}

static int
datumstreamwrite_context_callback(void *arg)
{
	DatumStreamWrite *acc = (DatumStreamWrite *) arg;

	char	   *str;

	/*
	 * Append-Only Storage Write's context.
	 */
	if (acc->need_close_file)
	{
		str = AppendOnlyStorageWrite_ContextStr(&acc->ao_write);

		errcontext("%s", str);

		pfree(str);
	}
	else
	{
		errcontext("%s", acc->title);
	}

	return 0;
}

void
datumstreamread_getlarge(DatumStreamRead * acc, Datum *datum, bool *null)
{
	switch (acc->largeObjectState)
	{
		case DatumStreamLargeObjectState_HaveAoContent:
			ereport(ERROR,
				(errmsg("Advance not called on large datum stream object")));
			return;

		case DatumStreamLargeObjectState_PositionAdvanced:
			acc->largeObjectState = DatumStreamLargeObjectState_Consumed;

			/* Fall below to ~_Consumed. */

		case DatumStreamLargeObjectState_Consumed:
			{
				int32		len;

				len = VARSIZE_ANY(acc->buffer_beginp);

				/*
				 * It is ok to get the same object more than once.
				 */
				if (Debug_datumstream_read_print_varlena_info)
				{
					datumstreamread_print_large_varlena_info(
															 acc,
														 acc->buffer_beginp);
				}

				if (Debug_appendonly_print_scan_tuple)
				{


					ereport(LOG,
							(errmsg("Datum stream block read is returning large variable-length object "
									"(length %d)",
									len)));
				}

				*datum = PointerGetDatum(acc->buffer_beginp);
				*null = false;
				return;
			}

		case DatumStreamLargeObjectState_Exhausted:
			ereport(ERROR,
					(errmsg("Get called after large datum stream object already consumed")));
			return;

		default:
			ereport(FATAL,
					(errmsg("Unexpected large datum stream state %d",
							acc->largeObjectState)));
			return;
	}
}

int
datumstreamread_advancelarge(DatumStreamRead * acc)
{
	acc->blockRead.nth++;
	switch (acc->largeObjectState)
	{
		case DatumStreamLargeObjectState_HaveAoContent:
			{
				struct varlena *va;
				int32		len;

				va = (struct varlena *) acc->buffer_beginp;
				len = VARSIZE_ANY(va);

				acc->largeObjectState = DatumStreamLargeObjectState_PositionAdvanced;
				return len;
			}

		case DatumStreamLargeObjectState_PositionAdvanced:
		case DatumStreamLargeObjectState_Consumed:
			/*
			 * Second advance returns exhaustion.
			 */
			acc->largeObjectState = DatumStreamLargeObjectState_Exhausted;
			return 0;

		case DatumStreamLargeObjectState_Exhausted:
			ereport(ERROR,
					(errmsg("Advance called after large datum stream object already consumed")));
			return 0;
			/* Never gets here. */

		default:
			ereport(FATAL,
					(errmsg("Unexpected large datum stream state %d",
							acc->largeObjectState)));
			return 0;
			/* Never reaches here. */
	}
}

int
datumstreamread_nthlarge(DatumStreamRead * acc)
{
	switch (acc->largeObjectState)
	{
		case DatumStreamLargeObjectState_HaveAoContent:
			ereport(ERROR,
				(errmsg("Advance not called on large datum stream object")));
			return 0;
			/* Never gets here. */

		case DatumStreamLargeObjectState_PositionAdvanced:
		case DatumStreamLargeObjectState_Consumed:
			return 0;

		case DatumStreamLargeObjectState_Exhausted:
			ereport(ERROR,
					(errmsg("Nth called after large datum stream object already consumed")));
			return 0;
			/* Never gets here. */

		default:
			ereport(FATAL,
					(errmsg("Unexpected large datum stream state %d",
							acc->largeObjectState)));
			return 0;
			/* Never reaches here. */
	}
}


int
datumstreamwrite_put(
					 DatumStreamWrite * acc,
					 Datum d,
					 bool null,
					 void **toFree)
{
	return DatumStreamBlockWrite_Put(&acc->blockWrite, d, null, toFree);
}

int
datumstreamwrite_nth(DatumStreamWrite * acc)
{
	return DatumStreamBlockWrite_Nth(&acc->blockWrite);
}

static void
init_datumstream_typeinfo(
						  DatumStreamTypeInfo * typeInfo,
						  Form_pg_attribute attr)
{
	typeInfo->datumlen = attr->attlen;
	typeInfo->typid = attr->atttypid;
	typeInfo->align = attr->attalign;
	typeInfo->byval = attr->attbyval;
}

/*
 * DeltaRange compression supported for folowing datatypes
 * INTEGER, BIGINT, DATE, TIME and TIMESTAMP
 */
static bool
is_deltarange_compression_supported(Form_pg_attribute attr)
{
	switch (attr->atttypid)
	{
		case INT4OID:
		case INT8OID:
		case DATEOID:
		case TIMEOID:
		case TIMESTAMPOID:
		case TIMESTAMPTZOID:
			/*
			 * FIXED length attributes and max with 8bytes length
			 * supported for DeltaRange
			 */
			Assert((attr->attlen >= 4) && (attr->attlen <= 8));
			Assert(attr->attbyval);
			return true;
	}
	return false;
}

static void
init_datumstream_info(
					  DatumStreamTypeInfo * typeInfo, //OUTPUT
					  DatumStreamVersion * datumStreamVersion, //OUTPUT
					  bool *rle_compression, //OUTPUT
					  bool *delta_compression, //OUTPUT
					  AppendOnlyStorageAttributes *ao_attr, //OUTPUT
					  int32 * maxAoBlockSize, //OUTPUT
					  char *compName,
					  int32 compLevel,
					  bool checksum,
					  int32 safeFSWriteSize,
					  int32 maxsz,
					  Form_pg_attribute attr)
{
	init_datumstream_typeinfo(
							  typeInfo,
							  attr);

	/*
	 * Adjust maxsz for Append-Only Storage.
	 */
	*maxAoBlockSize = AppendOnlyStorage_GetUsableBlockSize(maxsz);

	/*
	 * Assume the folowing unless modified below.
	 */
	*rle_compression = false;
	*delta_compression = false;

	ao_attr->compress = false;
	ao_attr->compressType = NULL;
	ao_attr->compressLevel = 0;

	*datumStreamVersion = DatumStreamVersion_Original;

	ao_attr->checksum = checksum;
	/*
	 * The original version didn't bother to populate these fields...
	 */
	ao_attr->safeFSWriteSize = 0;

	if (compName != NULL && pg_strcasecmp(compName, "rle_type") == 0)
	{
		/*
		 * For RLE_TYPE, we do the compression ourselves in this module.
		 *
		 * Optionally, BULK Compression by the AppendOnlyStorage layer may be performed
		 * as a second compression on the "Access Method" (first) compressed block.
		 */
		*datumStreamVersion = DatumStreamVersion_Dense_Enhanced;
		*rle_compression = true;

		ao_attr->safeFSWriteSize = safeFSWriteSize;

		/*
		 * Use the compresslevel as a kludgy way of specifiying the BULK compression
		 * to use.
		 */
		switch (compLevel)
		{
			case 1:
				ao_attr->compress = false;
				ao_attr->compressType = "none";
				ao_attr->compressLevel = 1;
				break;

			case 2:
				ao_attr->compress = true;
				ao_attr->compressType = "zlib";
				ao_attr->compressLevel = 1;
				break;

			case 3:
				ao_attr->compress = true;
				ao_attr->compressType = "zlib";
				ao_attr->compressLevel = 5;
				break;

			case 4:
				ao_attr->compress = true;
				ao_attr->compressType = "zlib";
				ao_attr->compressLevel = 9;
				break;

			default:
				ereport(ERROR,
						(errmsg("Unexpected compresslevel %d",
								compLevel)));

		}

		/*
		 * Check if for this dataype delta encoding is supported.
		 * With RLE this layer also does Delta range encoding
		 */
		*delta_compression = is_deltarange_compression_supported(attr);

	}
	else if (compName == NULL || pg_strcasecmp(compName, "none") == 0)
	{
		/* No bulk compression. */
	}
	else
	{
		/*
		 * Bulk compression will be used by AppendOnlyStorage{Write|Read} modules.
		 */
		ao_attr->compress = true;
		ao_attr->compressType = compName;
		ao_attr->compressLevel = compLevel;
	}
}

static void
determine_datumstream_compression_overflow(
										AppendOnlyStorageAttributes *ao_attr,
								 size_t(*desiredCompSizeFunc) (size_t input),
										   int32 maxAoBlockSize)
{
	int			desiredOverflowBytes = 0;

	if (desiredCompSizeFunc != NULL)
	{
		/*
		 * Call the compression's desired size function to find out what additional
		 * space it requires for our block size.
		 */
		desiredOverflowBytes =
			(int) (desiredCompSizeFunc) (maxAoBlockSize) - maxAoBlockSize;
		Insist(desiredOverflowBytes >= 0);
	}
	ao_attr->overflowSize = desiredOverflowBytes;
}

DatumStreamWrite *
create_datumstreamwrite(
						char *compName,
						int32 compLevel,
						bool checksum,
						int32 safeFSWriteSize,
						int32 maxsz,
						Form_pg_attribute attr,
						char *relname,
						char *title)
{
	DatumStreamWrite *acc = palloc0(sizeof(DatumStreamWrite));

	int32		initialMaxDatumPerBlock;
	int32		maxDatumPerBlock;

	PGFunction *compressionFunctions;
	CompressionState *compressionState;

	CompressionState *verifyBlockCompressionState;

	init_datumstream_info(
						  &acc->typeInfo,
						  &acc->datumStreamVersion,
						  &acc->rle_want_compression,
						  &acc->delta_want_compression,
						  &acc->ao_attr,
						  &acc->maxAoBlockSize,
						  compName,
						  compLevel,
						  checksum,
						  safeFSWriteSize,
						  maxsz,
						  attr);

	compressionFunctions = NULL;
	compressionState = NULL;
	verifyBlockCompressionState = NULL;
	if (acc->ao_attr.compress)
	{
		/*
		 * BULK compression.
		 */
		compressionFunctions = get_funcs_for_compression(acc->ao_attr.compressType);
		if (compressionFunctions != NULL)
		{
			TupleDesc	td = CreateTupleDesc(1, false, &attr);
			StorageAttributes sa;

			sa.comptype = acc->ao_attr.compressType;
			sa.complevel = acc->ao_attr.compressLevel;
			sa.blocksize = acc->maxAoBlockSize;

			compressionState =
				callCompressionConstructor(
										   compressionFunctions[COMPRESSION_CONSTRUCTOR], td, &sa, /* compress */ true);

			determine_datumstream_compression_overflow(
													   &acc->ao_attr,
												compressionState->desired_sz,
													   acc->maxAoBlockSize);

			if (gp_appendonly_verify_write_block)
			{
				verifyBlockCompressionState =
					callCompressionConstructor(
											   compressionFunctions[COMPRESSION_CONSTRUCTOR], td, &sa, /* compress */ false);
			}
		}
	}

	AppendOnlyStorageWrite_Init(
								&acc->ao_write,
								 /* memoryContext */ NULL,
								acc->maxAoBlockSize,
								relname,
								title,
								&acc->ao_attr);

	acc->ao_write.compression_functions = compressionFunctions;
	acc->ao_write.compressionState = compressionState;
	acc->ao_write.verifyWriteCompressionState = verifyBlockCompressionState;
	acc->title = title;

	/*
	 * Temporarily set the firstRowNum for the block so that we can
	 * calculate the correct header length.
	 */
	AppendOnlyStorageWrite_SetFirstRowNum(&acc->ao_write, 1);

	switch (acc->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			initialMaxDatumPerBlock = MAXDATUM_PER_AOCS_ORIG_BLOCK;
			maxDatumPerBlock = MAXDATUM_PER_AOCS_ORIG_BLOCK;

			acc->maxAoHeaderSize = AoHeader_Size(
												  /* isLong */ false,
												 checksum,
												  /* hasFirstRowNum */ true);
			break;

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			initialMaxDatumPerBlock = INITIALDATUM_PER_AOCS_DENSE_BLOCK;
			maxDatumPerBlock = MAXDATUM_PER_AOCS_DENSE_BLOCK;

			acc->maxAoHeaderSize = AoHeader_Size(
												  /* isLong */ true,
												 checksum,
												  /* hasFirstRowNum */ true);
			break;

		default:
			ereport(ERROR,
					(errmsg("Unexpected datum stream version %d",
							acc->datumStreamVersion)));
			initialMaxDatumPerBlock = 0;
			/* Quiet down compiler. */
			maxDatumPerBlock = 0;
			break;
			/* Never reached. */
	}

	DatumStreamBlockWrite_Init(
							   &acc->blockWrite,
							   &acc->typeInfo,
							   acc->datumStreamVersion,
							   acc->rle_want_compression,
							   acc->delta_want_compression,
							   initialMaxDatumPerBlock,
							   maxDatumPerBlock,
							   acc->maxAoBlockSize - acc->maxAoHeaderSize,
					/* errdetailCallback */ datumstreamwrite_detail_callback,
								/* errdetailArg */ (void *) acc,
				  /* errcontextCallback */ datumstreamwrite_context_callback,
								/* errcontextArg */ (void *) acc);

	return acc;
}

DatumStreamRead *
create_datumstreamread(
					   char *compName,
					   int32 compLevel,
					   bool checksum,
					   int32 safeFSWriteSize,
					   int32 maxsz,
					   Form_pg_attribute attr,
					   char *relname,
					   char *title)
{
	DatumStreamRead *acc = palloc0(sizeof(DatumStreamRead));

	PGFunction *compressionFunctions;
	CompressionState *compressionState;

	acc->memctxt = CurrentMemoryContext;

	init_datumstream_info(
						  &acc->typeInfo,
						  &acc->datumStreamVersion,
						  &acc->rle_can_have_compression,
						  &acc->delta_can_have_compression,
						  &acc->ao_attr,
						  &acc->maxAoBlockSize,
						  compName,
						  compLevel,
						  checksum,
						  safeFSWriteSize,
						  maxsz,
						  attr);

	compressionFunctions = NULL;
	compressionState = NULL;
	if (acc->ao_attr.compress)
	{
		/*
		 * BULK compression.
		 */
		compressionFunctions = get_funcs_for_compression(acc->ao_attr.compressType);
		if (compressionFunctions != NULL)
		{
			StorageAttributes sa;

			sa.comptype = acc->ao_attr.compressType;
			sa.complevel = acc->ao_attr.compressLevel;
			sa.blocksize = acc->maxAoBlockSize;

			compressionState =
				callCompressionConstructor(
										   compressionFunctions[COMPRESSION_CONSTRUCTOR], NULL, &sa, false /* compress */ );

			determine_datumstream_compression_overflow(
													   &acc->ao_attr,
												compressionState->desired_sz,
													   acc->maxAoBlockSize);
		}
	}

	AppendOnlyStorageRead_Init(
							   &acc->ao_read,
								/* memoryContext */ NULL,
							   acc->maxAoBlockSize,
							   relname,
							   title,
							   &acc->ao_attr);

	acc->ao_read.compression_functions = compressionFunctions;
	acc->ao_read.compressionState = compressionState;

	acc->title = title;

	acc->blockFirstRowNum = 1;
	Assert(acc->blockFileOffset == 0);
	Assert(acc->blockRowCount == 0);

	DatumStreamBlockRead_Init(
							  &acc->blockRead,
							  &acc->typeInfo,
							  acc->datumStreamVersion,
							  acc->rle_can_have_compression,
					 /* errdetailCallback */ datumstreamread_detail_callback,
							   /* errdetailArg */ (void *) acc,
				   /* errcontextCallback */ datumstreamread_context_callback,
							   /* errcontextArg */ (void *) acc);

	Assert(acc->large_object_buffer == NULL);
	Assert(acc->large_object_buffer_size == 0);

	Assert(acc->largeObjectState == DatumStreamLargeObjectState_None);

	Assert(acc->eof == 0);
	Assert(acc->eofUncompress == 0);

	if (Debug_appendonly_print_scan)
	{
		if (!acc->ao_attr.compress)
		{
			ereport(LOG,
					(errmsg("Datum stream read %s created with NO bulk compression for %s"
							"(maximum Append-Only blocksize %d, "
							"checksum %s)",
						  DatumStreamVersion_String(acc->datumStreamVersion),
							acc->title,
							acc->maxAoBlockSize,
							(acc->ao_attr.checksum ? "true" : "false"))));
		}
		else
		{
			ereport(LOG,
					(errmsg("Datum stream read %s created with bulk compression for %s "
							"(maximum Append-Only blocksize %d, "
							"compression type %s, compress level %d, "
							"checksum %s)",
						  DatumStreamVersion_String(acc->datumStreamVersion),
							acc->title,
							acc->maxAoBlockSize,
							acc->ao_attr.compressType,
							acc->ao_attr.compressLevel,
							(acc->ao_attr.checksum ? "true" : "false"))));
		}
	}

	return acc;
}

void
destroy_datumstreamwrite(DatumStreamWrite * ds)
{
	DatumStreamBlockWrite_Finish(&ds->blockWrite);

	AppendOnlyStorageWrite_FinishSession(&ds->ao_write);

	if (ds->title)
	{
		pfree(ds->title);
	}
	pfree(ds);
}

void
destroy_datumstreamread(DatumStreamRead * ds)
{
	DatumStreamBlockRead_Finish(&ds->blockRead);

	if (ds->large_object_buffer)
		pfree(ds->large_object_buffer);

	AppendOnlyStorageRead_FinishSession(&ds->ao_read);

	if (ds->title)
	{
		pfree(ds->title);
	}
	pfree(ds);
}


void
datumstreamwrite_open_file(DatumStreamWrite * ds, char *fn, int64 eof, int64 eofUncompressed, RelFileNode relFileNode, int32 segmentFileNum, int version)
{
	ItemPointerData persistentTid;
	int64 persistentSerialNum;
	int64 appendOnlyNewEof;

	ds->eof = eof;
	ds->eofUncompress = eofUncompressed;

	if (ds->need_close_file)
		datumstreamwrite_close_file(ds);

	/*
	 * Segment file #0 is created when the Append-Only table is created,
	 * other segment files are created on-demand under transaction. Even
	 * segment file #0 might get deleted by a later vacuum though. So
	 * always be prepared to create the file, when starting from EOF 0.
	 */
	if (eof == 0)
	{
		AppendOnlyStorageWrite_TransactionCreateFile(
													 &ds->ao_write,
													 fn,
													 eof,
													 &relFileNode,
													 segmentFileNum,
													 &persistentTid,
													 &persistentSerialNum);
	}
	else
	{
		if (!ReadGpRelationNode(
				(relFileNode.spcNode == MyDatabaseTableSpace) ? 0:relFileNode.spcNode,
				relFileNode.relNode,
				segmentFileNum,
				&persistentTid,
				&persistentSerialNum))
		{
			elog(ERROR, "Did not find gp_relation_node entry for relfilenode %u, segment file #%d, logical eof " INT64_FORMAT,
				 relFileNode.relNode,
				 segmentFileNum,
				 eof);
		}
	}

	if (gp_appendonly_verify_eof)
	{
		appendOnlyNewEof = PersistentFileSysObj_ReadEof(
					PersistentFsObjType_RelationFile,
					&persistentTid);
		/*
		 * Verify if EOF from gp_persistent_relation_node < EOF from pg_aocsseg
		 *
		 * Note:- EOF from gp_persistent_relation_node has to be less than the
		 * EOF from pg_aocsseg because inside a transaction the actual EOF where
		 * the data is inserted has to be greater than or equal to Persistent
		 * Table (PT) stored EOF as persistent table EOF value is updated at the
		 * end of the transaction.
		 */
		if (eof < appendOnlyNewEof)
		{
			elog(ERROR, "Unexpected EOF for relfilenode %u,"
						" segment file %d: EOF from gp_persistent_relation_node "
						INT64_FORMAT " greater than current EOF " INT64_FORMAT,
						relFileNode.relNode,
						segmentFileNum,
						appendOnlyNewEof,
						eof);
		}
	}

	/*
	 * Open the existing file for write.
	 */
	AppendOnlyStorageWrite_OpenFile(&ds->ao_write,
									fn,
									version,
									eof,
									eofUncompressed,
									&relFileNode,
									segmentFileNum,
									&persistentTid,
									persistentSerialNum);

	ds->need_close_file = true;
}

void
datumstreamread_open_file(DatumStreamRead * ds, char *fn, int64 eof, int64 eofUncompressed, RelFileNode relFileNode, int32 segmentFileNum, int version)
{
	ds->eof = eof;
	ds->eofUncompress = eofUncompressed;

	if (ds->need_close_file)
		datumstreamread_close_file(ds);

	AppendOnlyStorageRead_OpenFile(&ds->ao_read, fn, version, ds->eof);

	ds->need_close_file = true;
}

void
datumstreamwrite_close_file(DatumStreamWrite * ds)
{
	AppendOnlyStorageWrite_TransactionFlushAndCloseFile(
														&ds->ao_write,
														&ds->eof,
														&ds->eofUncompress);

	/*
	 * Add the access method "compression" saving.
	 *
	 * If the savings are negative, then the compresion ratio could fall below 1.0
	 */
	ds->eofUncompress += ds->blockWrite.savings;

	ds->need_close_file = false;
}

void
datumstreamread_close_file(DatumStreamRead * ds)
{
	AppendOnlyStorageRead_CloseFile(&ds->ao_read);

	ds->need_close_file = false;
}

static int64
datumstreamwrite_block_orig(DatumStreamWrite * acc)
{
	int64		writesz = 0;
	uint8	   *buffer = NULL;
	int32		rowCount;

	/*
	 * Set the BlockFirstRowNum. Need to set this before
	 * calling AppendOnlyStorageWrite_GetBuffer.
	 */
	AppendOnlyStorageWrite_SetFirstRowNum(&acc->ao_write,
										  acc->blockFirstRowNum);

	rowCount = DatumStreamBlockWrite_Nth(&acc->blockWrite);
	Assert(rowCount <= MAXDATUM_PER_AOCS_ORIG_BLOCK);

	buffer = AppendOnlyStorageWrite_GetBuffer(
											  &acc->ao_write,
											  AoHeaderKind_SmallContent);

	writesz = DatumStreamBlockWrite_Block(
										  &acc->blockWrite,
										  buffer);

	acc->ao_write.logicalBlockStartOffset =
		BufferedAppendNextBufferPosition(&(acc->ao_write.bufferedAppend));

	/* Write it out */
	AppendOnlyStorageWrite_FinishBuffer(
										&acc->ao_write,
										(int32) writesz,
										AOCSBK_BLOCK,
										rowCount);

	/* Set up our write block information */
	DatumStreamBlockWrite_GetReady(&acc->blockWrite);

	return writesz;
}

static int64
datumstreamwrite_block_dense(DatumStreamWrite * acc)
{
	int64		writesz = 0;
	uint8	   *buffer = NULL;
	int32		rowCount;
	AoHeaderKind aoHeaderKind;

	/*
	 * Set the BlockFirstRowNum. Need to set this before
	 * calling AppendOnlyStorageWrite_GetBuffer.
	 */
	AppendOnlyStorageWrite_SetFirstRowNum(&acc->ao_write,
										  acc->blockFirstRowNum);

	rowCount = DatumStreamBlockWrite_Nth(&acc->blockWrite);

	if (rowCount <= AOSmallContentHeader_MaxRowCount)
	{
		aoHeaderKind = AoHeaderKind_SmallContent;
	}
	else if (acc->ao_attr.compress)
	{
		aoHeaderKind = AoHeaderKind_BulkDenseContent;
	}
	else
	{
		aoHeaderKind = AoHeaderKind_NonBulkDenseContent;
	}

	buffer = AppendOnlyStorageWrite_GetBuffer(
											  &acc->ao_write,
											  aoHeaderKind);

	writesz = DatumStreamBlockWrite_Block(
										  &acc->blockWrite,
										  buffer);

	acc->ao_write.logicalBlockStartOffset =
		BufferedAppendNextBufferPosition(&(acc->ao_write.bufferedAppend));

	/* Write it out */
	AppendOnlyStorageWrite_FinishBuffer(
										&acc->ao_write,
										(int32) writesz,
										AOCSBK_BLOCK,
										rowCount);

	/* Set up our write block information */
	DatumStreamBlockWrite_GetReady(&acc->blockWrite);

	return writesz;
}

int64
datumstreamwrite_block(DatumStreamWrite *acc,
					   AppendOnlyBlockDirectory *blockDirectory,
					   int columnGroupNo,
					   bool addColAction)
{
	int64 writesz;
	int itemCount = DatumStreamBlockWrite_Nth(&acc->blockWrite);

	/* Nothing to write, this is just no op */
	if (itemCount == 0)
	{
		return 0;
	}

	switch (acc->datumStreamVersion)
	{
		case DatumStreamVersion_Original:
			writesz = datumstreamwrite_block_orig(acc);
			break;

		case DatumStreamVersion_Dense:
		case DatumStreamVersion_Dense_Enhanced:
			writesz = datumstreamwrite_block_dense(acc);
			break;

		default:
			elog(ERROR, "Unexpected datum stream version %d",
				 acc->datumStreamVersion);
			return 0;
			/* Never reaches here. */
	}

	/* Insert an entry to the block directory */
	AppendOnlyBlockDirectory_InsertEntry(
		blockDirectory,
		columnGroupNo,
		acc->blockFirstRowNum,
		AppendOnlyStorageWrite_LogicalBlockStartOffset(&acc->ao_write),
		itemCount,
		addColAction);

	return writesz;
}

static void
datumstreamwrite_print_large_varlena_info(
										  DatumStreamWrite * acc,
										  uint8 * p)
{
	elog(LOG, "Write large varlena <%s>",
		 VarlenaInfoToString(p));
}

int64
datumstreamwrite_lob(DatumStreamWrite * acc,
					 Datum d,
					 AppendOnlyBlockDirectory *blockDirectory,
					 int colGroupNo,
					 bool addColAction)
{
	uint8	   *p;
	int32		varLen;

	Assert(acc);
	Assert(acc->datumStreamVersion == DatumStreamVersion_Original ||
		   acc->datumStreamVersion == DatumStreamVersion_Dense ||
		   acc->datumStreamVersion == DatumStreamVersion_Dense_Enhanced);

	if (acc->typeInfo.datumlen >= 0)
	{
		elog(ERROR, "Large object must be variable length objects (varlena)");
	}
	/*
	 * If the datum is toasted	/ compressed -- an error.
	 */
	if (VARATT_IS_EXTENDED(DatumGetPointer(d)))
	{
		elog(ERROR, "Expected large object / variable length objects (varlena) to be de-toasted and/or de-compressed at this point");
	}

	/*
	 * De-Toast Datum
	 */
	if (VARATT_IS_EXTERNAL(DatumGetPointer(d)))
	{
		d = PointerGetDatum(heap_tuple_fetch_attr(
								(struct varlena *) DatumGetPointer(d)));
	}

	p = (uint8 *) DatumGetPointer(d);
	varLen = VARSIZE_ANY(p);

	if (Debug_datumstream_write_print_large_varlena_info)
	{
		datumstreamwrite_print_large_varlena_info(
												  acc,
												  p);
	}

	/* Set the BlockFirstRowNum */
	AppendOnlyStorageWrite_SetFirstRowNum(&acc->ao_write,
										  acc->blockFirstRowNum);

	AppendOnlyStorageWrite_Content(
								   &acc->ao_write,
								   p,
								   varLen,
								   AOCSBK_BLOB,
									/* rowCount */ 1);

	/* Insert an entry to the block directory */
	AppendOnlyBlockDirectory_InsertEntry(
		blockDirectory,
		colGroupNo,
		acc->blockFirstRowNum,
		AppendOnlyStorageWrite_LogicalBlockStartOffset(&acc->ao_write),
		1, /*itemCount -- always just the lob just inserted */
		addColAction);

	return varLen;
}

static bool
datumstreamread_block_info(DatumStreamRead * acc)
{
	bool		readOK = false;

	Assert(acc);

	readOK = AppendOnlyStorageRead_GetBlockInfo(
												&acc->ao_read,
												&acc->getBlockInfo.contentLen,
											&acc->getBlockInfo.execBlockKind,
												&acc->getBlockInfo.firstRow,
												&acc->getBlockInfo.rowCnt,
												&acc->getBlockInfo.isLarge,
											&acc->getBlockInfo.isCompressed);

	if (!readOK)
		return false;

	acc->blockFirstRowNum = acc->getBlockInfo.firstRow;
	acc->blockFileOffset = acc->ao_read.current.headerOffsetInFile;
	acc->blockRowCount = acc->getBlockInfo.rowCnt;

	if (Debug_appendonly_print_scan)
		elog(LOG,
			 "Datum stream read get block typeInfo for table '%s' "
		 "(contentLen %d, execBlockKind = %d, firstRowNum " INT64_FORMAT ", "
			 "rowCount %d, isLargeContent %s, isCompressed %s, "
			 "blockFirstRowNum " INT64_FORMAT ", blockFileOffset " INT64_FORMAT ", blockRowCount %d)",
			 AppendOnlyStorageRead_RelationName(&acc->ao_read),
			 acc->getBlockInfo.contentLen,
			 acc->getBlockInfo.execBlockKind,
			 acc->getBlockInfo.firstRow,
			 acc->getBlockInfo.rowCnt,
			 (acc->getBlockInfo.isLarge ? "true" : "false"),
			 (acc->getBlockInfo.isCompressed ? "true" : "false"),
			 acc->blockFirstRowNum,
			 acc->blockFileOffset,
			 acc->blockRowCount);

	return true;
}

static void
datumstreamread_block_get_ready(DatumStreamRead * acc)
{
	/*
	 * Read header information and setup for reading the datum in the block.
	 */
	if (acc->getBlockInfo.execBlockKind == AOCSBK_BLOCK)
	{
		bool		hadToAdjustRowCount;
		int32		adjustedRowCount;

		DatumStreamBlockRead_GetReady(
									  &acc->blockRead,
									  acc->buffer_beginp,
									  acc->getBlockInfo.contentLen,
									  acc->getBlockInfo.firstRow,
									  acc->getBlockInfo.rowCnt,
									  &hadToAdjustRowCount,
									  &adjustedRowCount);
		if (hadToAdjustRowCount)
		{
			acc->blockRowCount = adjustedRowCount;
		}
	}
	else if (acc->getBlockInfo.execBlockKind == AOCSBK_BLOB)
	{
		Assert(acc->buffer_beginp == acc->large_object_buffer);
	}
	else
	{
		elog(ERROR,
			 "Unexpected Append-Only Column Store executor kind %d",
			 acc->getBlockInfo.execBlockKind);
	}
}

void
datumstreamread_block_content(DatumStreamRead * acc)
{
	Assert(acc);

	/*
	 * Clear out state from previous block.
	 */
	DatumStreamBlockRead_Reset(&acc->blockRead);

	acc->largeObjectState = DatumStreamLargeObjectState_None;

	/*
	 * Read in data.
	 */
	if (acc->getBlockInfo.execBlockKind == AOCSBK_BLOCK)
	{
		Assert(!acc->getBlockInfo.isLarge);

		if (acc->getBlockInfo.isCompressed)
		{
			/* Compressed, need to decompress to our own buffer.  */
			if (acc->large_object_buffer_size < acc->getBlockInfo.contentLen)
			{
				MemoryContext oldCtxt;

				oldCtxt = MemoryContextSwitchTo(acc->memctxt);

				if (acc->large_object_buffer)
				{
					pfree(acc->large_object_buffer);
					acc->large_object_buffer = NULL;

					SIMPLE_FAULT_INJECTOR(MallocFailure);
				}

				acc->large_object_buffer_size = acc->getBlockInfo.contentLen;
				acc->large_object_buffer = palloc(acc->getBlockInfo.contentLen);
				MemoryContextSwitchTo(oldCtxt);
			}

			AppendOnlyStorageRead_Content(
										  &acc->ao_read,
										  (uint8 *) acc->large_object_buffer,
										  acc->getBlockInfo.contentLen);

			acc->buffer_beginp = acc->large_object_buffer;
		}
		else
		{
			acc->buffer_beginp = AppendOnlyStorageRead_GetBuffer(&acc->ao_read);
		}


		if (Debug_appendonly_print_datumstream)
			elog(LOG,
				 "datumstream_read_block_content filePathName %s firstRowNum " INT64_FORMAT " rowCnt %u "
				 "ndatum %u contentLen %d datump %p",
				 acc->ao_read.bufferedRead.filePathName,
				 acc->getBlockInfo.firstRow,
				 acc->getBlockInfo.rowCnt,
				 acc->blockRead.logical_row_count,
				 acc->getBlockInfo.contentLen, acc->blockRead.datump);

	}
	else if (acc->getBlockInfo.execBlockKind == AOCSBK_BLOB)
	{
		Assert(acc->getBlockInfo.rowCnt == 1);

		if (acc->typeInfo.datumlen >= 0)
		{
			elog(ERROR, "Large object must be variable length objects (varlena)");
		}

		/*
		 * NOTE: Do not assert the content is large.  What appears to be large
		 * content
		 */
		/* NOTE: can compress into one AO storage block. */

		if (acc->large_object_buffer_size < acc->getBlockInfo.contentLen)
		{
			MemoryContext oldCtxt;

			oldCtxt = MemoryContextSwitchTo(acc->memctxt);

			if (acc->large_object_buffer)
				pfree(acc->large_object_buffer);

			acc->large_object_buffer_size = acc->getBlockInfo.contentLen;
			acc->large_object_buffer = palloc(acc->getBlockInfo.contentLen);
			MemoryContextSwitchTo(oldCtxt);
		}

		AppendOnlyStorageRead_Content(
									  &acc->ao_read,
									  acc->large_object_buffer,
									  acc->getBlockInfo.contentLen);

		acc->buffer_beginp = acc->large_object_buffer;
		acc->largeObjectState = DatumStreamLargeObjectState_HaveAoContent;

		if (Debug_datumstream_read_check_large_varlena_integrity)
		{
			datumstreamread_check_large_varlena_integrity(
														  acc,
														  acc->buffer_beginp,
											   acc->getBlockInfo.contentLen);
		}
	}
	else
	{
		elog(ERROR,
			 "Unexpected Append-Only Column Store executor kind %d",
			 acc->getBlockInfo.execBlockKind);
	}

	/*
	 * Unpack the information from the block headers and get ready to read the first datum.
	 */
	datumstreamread_block_get_ready(acc);
}


int
datumstreamread_block(DatumStreamRead * acc,
					  AppendOnlyBlockDirectory *blockDirectory,
					  int colGroupNo)
{
	bool		readOK = false;

	Assert(acc);

	acc->blockFirstRowNum += acc->blockRowCount;

	readOK = AppendOnlyStorageRead_GetBlockInfo(&acc->ao_read,
												&acc->getBlockInfo.contentLen,
											&acc->getBlockInfo.execBlockKind,
												&acc->getBlockInfo.firstRow,
												&acc->getBlockInfo.rowCnt,
												&acc->getBlockInfo.isLarge,
											&acc->getBlockInfo.isCompressed);
	if (!readOK)
		return -1;

	if (Debug_appendonly_print_datumstream)
		elog(LOG,
			 "datumstream_read_block filePathName %s ndatum %u datump %p "
			 "firstRow " INT64_FORMAT " rowCnt %u contentLen %u ",
			 acc->ao_read.bufferedRead.filePathName,
			 acc->blockRead.logical_row_count,
			 acc->blockRead.datump,
			 acc->getBlockInfo.firstRow,
			 acc->getBlockInfo.rowCnt,
			 acc->getBlockInfo.contentLen);
	/*
	 * Pre-4.0 blocks do not store firstRowNum, so the returned value
	 * for firstRow is -1. In this case, acc->blockFirstRowNum keeps
	 * its last value, i.e. is incremented by the blockRowCount of the
	 * previous block.
	 */
	if (acc->getBlockInfo.firstRow >= 0)
	{
		acc->blockFirstRowNum = acc->getBlockInfo.firstRow;
	}
	acc->blockFileOffset = acc->ao_read.current.headerOffsetInFile;
	acc->blockRowCount = acc->getBlockInfo.rowCnt;

	if (Debug_appendonly_print_scan)
		elog(LOG,
			 "Datum stream read get block typeInfo for table '%s' "
		 "(contentLen %d, execBlockKind = %d, firstRowNum " INT64_FORMAT ", "
			 "rowCount %d, isLargeContent %s, isCompressed %s, "
			 "blockFirstRowNum " INT64_FORMAT ", blockFileOffset " INT64_FORMAT ", blockRowCount %d)",
			 AppendOnlyStorageRead_RelationName(&acc->ao_read),
			 acc->getBlockInfo.contentLen,
			 acc->getBlockInfo.execBlockKind,
			 acc->getBlockInfo.firstRow,
			 acc->getBlockInfo.rowCnt,
			 (acc->getBlockInfo.isLarge ? "true" : "false"),
			 (acc->getBlockInfo.isCompressed ? "true" : "false"),
			 acc->blockFirstRowNum,
			 acc->blockFileOffset,
			 acc->blockRowCount);

	datumstreamread_block_content(acc);

	if (blockDirectory)
	{
		AppendOnlyBlockDirectory_InsertEntry(blockDirectory,
											 colGroupNo,
											 acc->blockFirstRowNum,
											 acc->blockFileOffset,
											 acc->blockRowCount,
											 false);
	}

	return 0;
}

void
datumstreamread_rewind_block(DatumStreamRead * datumStream)
{
	DatumStreamBlockRead_Reset(&datumStream->blockRead);

	datumstreamread_block_get_ready(datumStream);
}

/*
 * Find the specified row in the current block.
 *
 * Note that the values for rowNumInBlock starts with 0.
 */
void
datumstreamread_find(DatumStreamRead * datumStream,
					 int32 rowNumInBlock)
{
	/*
	 * if reading a tuple that is prior to the tuple that the datum stream
	 * is pointing to now, reset the datum stream pointers.
	 */
	if (rowNumInBlock < DatumStreamBlockRead_Nth(&datumStream->blockRead))
		datumstreamread_rewind_block(datumStream);

	Assert(rowNumInBlock >= DatumStreamBlockRead_Nth(&datumStream->blockRead) ||
		   DatumStreamBlockRead_Nth(&datumStream->blockRead) == -1);

	/*
	 * Find the right row in the block.
	 */
	while (rowNumInBlock > DatumStreamBlockRead_Nth(&datumStream->blockRead))
	{
		int			status;

		status = datumstreamread_advance(datumStream);
		if (status == 0)
		{
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("Unexpected internal error,"
								   " could not find the right row in block."
								   " rowCnt is %d"
								   " largeObjectState is %d"
								   " rowNumInBlock is %d"
								   " DatumStreamBlockRead_Nth is %d",
								   datumStream->blockRead.logical_row_count,
								   datumStream->largeObjectState,
								   rowNumInBlock,
						DatumStreamBlockRead_Nth(&datumStream->blockRead))));
		}
	}

	Assert(rowNumInBlock == DatumStreamBlockRead_Nth(&datumStream->blockRead));
}

/*
 * Find the block that contains the given row.
 */
bool
datumstreamread_find_block(DatumStreamRead * datumStream,
						   DatumStreamFetchDesc datumStreamFetchDesc,
						   int64 rowNum)
{
	Assert(datumStreamFetchDesc->datumStream == datumStream);

	if (datumStreamFetchDesc->scanNextFileOffset >=
		datumStreamFetchDesc->scanAfterFileOffset)
		return false;
	/* No more blocks requested for range */

	if (datumStreamFetchDesc->currentSegmentFile.logicalEof ==
		datumStreamFetchDesc->scanNextFileOffset)
		return false;
	/* No more blocks requested for range */

	if (datumStreamFetchDesc->currentSegmentFile.logicalEof <
		datumStreamFetchDesc->scanNextFileOffset)
		return false;
/* UNDONE:Why does our next scan position go beyond logical EOF ? */

	AppendOnlyStorageRead_SetTemporaryRange(
											&datumStream->ao_read,
									datumStreamFetchDesc->scanNextFileOffset,
								  datumStreamFetchDesc->scanAfterFileOffset);

	while (true)
	{
		if (!datumstreamread_block_info(datumStream))
			return false;

		/*
		 * Update the current block typeInfo.
		 */
		const bool	isOldBlockFormat = (datumStream->getBlockInfo.firstRow == -1);

		datumStreamFetchDesc->currentBlock.have = true;
		datumStreamFetchDesc->currentBlock.fileOffset =
			AppendOnlyStorageRead_CurrentHeaderOffsetInFile(
													  &datumStream->ao_read);
		datumStreamFetchDesc->currentBlock.overallBlockLen =
			AppendOnlyStorageRead_OverallBlockLen(&datumStream->ao_read);
		datumStreamFetchDesc->currentBlock.firstRowNum =
			(!isOldBlockFormat) ? datumStream->getBlockInfo.firstRow : datumStreamFetchDesc->scanNextRowNum;
		datumStreamFetchDesc->currentBlock.lastRowNum =
			datumStreamFetchDesc->currentBlock.firstRowNum + datumStream->getBlockInfo.rowCnt - 1;
		datumStreamFetchDesc->currentBlock.isCompressed =
			datumStream->getBlockInfo.isCompressed;
		datumStreamFetchDesc->currentBlock.isLargeContent = datumStream->getBlockInfo.isLarge;
		datumStreamFetchDesc->currentBlock.gotContents = false;

		if (Debug_appendonly_print_datumstream)
			elog(LOG,
				 "datumstream_find_block filePathName %s fileOffset " INT64_FORMAT " firstRowNum " INT64_FORMAT "  "
				 "rowCnt %u lastRowNum " INT64_FORMAT " ",
				 datumStream->ao_read.bufferedRead.filePathName,
				 datumStreamFetchDesc->currentBlock.fileOffset,
				 datumStreamFetchDesc->currentBlock.firstRowNum,
				 datumStream->getBlockInfo.rowCnt,
				 datumStreamFetchDesc->currentBlock.lastRowNum);

		if (rowNum < datumStreamFetchDesc->currentBlock.firstRowNum)
		{
			/*
			 * Since we have read a new block, the temporary
			 * range for the read needs to be adjusted
			 * accordingly. Otherwise, the underlying bufferedRead
			 * may stop reading more data because of the
			 * previously-set smaller temporary range.
			 */
			int64		beginFileOffset = datumStreamFetchDesc->currentBlock.fileOffset;
			int64		afterFileOffset = datumStreamFetchDesc->currentBlock.fileOffset +
			datumStreamFetchDesc->currentBlock.overallBlockLen;

			AppendOnlyStorageRead_SetTemporaryRange(
													&datumStream->ao_read,
													beginFileOffset,
													afterFileOffset);

			return false;
		}

		/*
		 * the fix for MPP-17061 does not need to be applied for pre-4.0 blocks,
		 * since index could only be created after upgrading to 4.x.
		 * As a result pre-4.0 blocks has no invisible rows.
		 */
		if (isOldBlockFormat)
		{
			int32		rowCnt;

			/*
			 * rowCnt may not be valid for pre-4.0 blocks, we need to
			 * read the block content to restore the correct value.
			 */
			datumstreamread_block_content(datumStream);

			rowCnt = datumStream->blockRowCount;
			datumStreamFetchDesc->currentBlock.lastRowNum =
				datumStreamFetchDesc->currentBlock.firstRowNum + rowCnt - 1;
		}

		if (Debug_appendonly_print_datumstream)
			elog(LOG,
				 "datumstream_find_block filePathName %s fileOffset " INT64_FORMAT " firstRowNum " INT64_FORMAT " "
				 "rowCnt %u lastRowNum " INT64_FORMAT " ",
				 datumStream->ao_read.bufferedRead.filePathName,
				 datumStreamFetchDesc->currentBlock.fileOffset,
				 datumStreamFetchDesc->currentBlock.firstRowNum,
				 datumStream->getBlockInfo.rowCnt,
				 datumStreamFetchDesc->currentBlock.lastRowNum);

		if (rowNum <= datumStreamFetchDesc->currentBlock.lastRowNum)
		{
			/*
			 * Found the block that contains the row.
			 * Read the block content if it was not read above.
			 */
			if (!isOldBlockFormat)
			{
				if (datumStreamFetchDesc->currentBlock.gotContents)
				{
					ereport(ERROR,
							(errmsg("Unexpected internal error,"
									" block content was already read."
									" datumstream_find_block filePathName %s"
									" fileOffset " INT64_FORMAT
									" firstRowNum " INT64_FORMAT
									" rowCnt %u lastRowNum " INT64_FORMAT
								 " ndatum %u contentLen %d gotContents true",
							  datumStream->ao_read.bufferedRead.filePathName,
							   datumStreamFetchDesc->currentBlock.fileOffset,
									datumStream->getBlockInfo.firstRow,
									datumStream->getBlockInfo.rowCnt,
							   datumStreamFetchDesc->currentBlock.lastRowNum,
									datumStream->blockRead.logical_row_count,
									datumStream->getBlockInfo.contentLen
									)));
				}
				if (Debug_appendonly_print_datumstream)
					elog(LOG,
						 "datumstream_find_block filePathName %s fileOffset " INT64_FORMAT " firstRowNum " INT64_FORMAT " "
						 "rowCnt %u lastRowNum " INT64_FORMAT " "
						 "ndatum %u contentLen %d ",
						 datumStream->ao_read.bufferedRead.filePathName,
						 datumStreamFetchDesc->currentBlock.fileOffset,
						 datumStream->getBlockInfo.firstRow,
						 datumStream->getBlockInfo.rowCnt,
						 datumStreamFetchDesc->currentBlock.lastRowNum,
						 datumStream->blockRead.logical_row_count,
						 datumStream->getBlockInfo.contentLen);

				datumstreamread_block_content(datumStream);
				datumStreamFetchDesc->currentBlock.gotContents = true;
			}
			break;
		}

		Assert(!datumStreamFetchDesc->currentBlock.gotContents);

		/* MPP-17061: reach the end of range covered by block directory entry */
		if ((datumStreamFetchDesc->currentBlock.fileOffset +
			 datumStreamFetchDesc->currentBlock.overallBlockLen) >=
			datumStreamFetchDesc->scanAfterFileOffset)
			return false;

		AppendOnlyStorageRead_SkipCurrentBlock(&datumStream->ao_read);
	}

	return true;
}
