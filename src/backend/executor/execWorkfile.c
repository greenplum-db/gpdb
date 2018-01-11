/*-------------------------------------------------------------------------
 *
 * execWorkfile.c
 *    Management of temporary work files used by the executor nodes.
 *
 * WorkFiles provide a general interface to different implementations of
 * temporary files used by the executor nodes.
 *
 * Portions Copyright (c) 2010. Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/execWorkfile.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "storage/buffile.h"
#include "storage/bfz.h"
#include "executor/execWorkfile.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "utils/workfile_mgr.h"
#include "utils/memutils.h"

#include "utils/faultinjector.h"

/*
 * Number of temporary files opened during the current session;
 * this is used in generation of unique tempfile names.
 */
static uint64 temp_file_counter = 0;


static void ExecWorkFile_SetFlags(ExecWorkFile *workfile, bool delOnClose, bool created);
static void ExecWorkFile_AdjustBFZSize(ExecWorkFile *workfile, int64 file_size);

/*
 * ExecWorkFile_Create
 *    create a new work file with the specified name, the file type,
 * and the compression type.
 *
 * If this fails, NULL is returned.
 */
ExecWorkFile *
ExecWorkFile_Create(const char *fileName,
					ExecWorkFileType fileType,
					bool delOnClose,
					int compressType)
{
	ExecWorkFile *workfile;
	void	   *file;

	/* Before creating a new file, let's check the limit on number of workfile created */
	if (!WorkfileQueryspace_AddWorkfile())
	{
		/* Failed to reserve additional disk space, notify caller */
		workfile_mgr_report_error();
	}

	switch(fileType)
	{
		case BUFFILE:
			file = (void *) BufFileCreateFile(fileName, delOnClose, false /* interXact */ );
			BufFileSetWorkfile(file);
			break;
		case BFZ:
			file = (void *)bfz_create(fileName, delOnClose, compressType);
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("invalid work file type: %d", fileType)));
			return NULL;		/* keep compiler quiet */
	}

	workfile = palloc0(sizeof(ExecWorkFile));
	workfile->fileType = fileType;
	workfile->compressType = compressType;
	workfile->file = file;
	workfile->fileName = pstrdup(fileName);
	workfile->size = 0;
	ExecWorkFile_SetFlags(workfile, delOnClose, true /* created */);

	return workfile;
}

/*
 * ExecWorkfile_AddUniqueSuffix
 *   Adds a suffix to a filename to make it unique, within and across queries
 *
 *   The returned StringInfo and data are palloc-ed in the current memory context.
 */
StringInfo
ExecWorkFile_AddUniqueSuffix(const char *filename)
{
	StringInfo uniquename = makeStringInfo();
	appendStringInfo(uniquename, "%s_%d_" UINT64_FORMAT,
			filename,
			MyProcPid,
			temp_file_counter++);

	Assert(uniquename->len <= MAXPGPATH - 1 && "could not generate temporary file name");
	return uniquename;
}

/*
 * ExecWorkFile_CreateUnique
 *   create a new work file with specified name, type and compression
 *   In addition, it adds a unique suffix
 */
ExecWorkFile *
ExecWorkFile_CreateUnique(const char *filename,
		ExecWorkFileType fileType,
		bool delOnClose,
		int compressType)
{

	StringInfo uniquename = ExecWorkFile_AddUniqueSuffix(filename);
	ExecWorkFile *ewf = ExecWorkFile_Create(uniquename->data, fileType, delOnClose, compressType);
	pfree(uniquename->data);
	pfree(uniquename);

	return ewf;
}

/*
 * Opens an existing work file with the specified name, the file type,
 * and the compression type.
 *
 * If this fails, NULL is returned.
 */
ExecWorkFile *
ExecWorkFile_Open(const char *fileName,
					ExecWorkFileType fileType,
					bool delOnClose,
					int compressType)
{
	ExecWorkFile *workfile;
	void	   *file;
	int64		file_size;

	switch(fileType)
	{
		case BUFFILE:
			file = (void *)BufFileOpenFile(fileName,
					false, /* Create */
					delOnClose,
					true  /* interXact */ );
			if (!file)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open temporary file \"%s\": %m",
								fileName)));

			BufFileSetWorkfile(file);
			file_size = BufFileGetSize(file);
			break;

		case BFZ:
			file = (void *)bfz_open(fileName, delOnClose, compressType);
			if (!file)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open temporary file \"%s\": %m",
								fileName)));
			file_size = bfz_totalbytes((bfz_t *)file);
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("invalid work file type: %d", fileType)));
			return NULL;		/* keep compiler quiet */
	}

	workfile = palloc0(sizeof(ExecWorkFile));

	workfile->fileType = fileType;
	workfile->compressType = compressType;
	workfile->file = file;
	workfile->fileName = pstrdup(fileName);
	workfile->size = file_size;
	ExecWorkFile_SetFlags(workfile, delOnClose, false /* created */);

	return workfile;
}


/*
 * ExecWorkFile_Write
 *    write the given data from the end of the last write position.
 *
 * This function returns true if the write succeeds. Otherwise, return false.
 */
bool
ExecWorkFile_Write(ExecWorkFile *workfile,
				   void *data,
				   uint64 size)
{
	Assert(workfile != NULL);
	uint64 bytes;

	SIMPLE_FAULT_INJECTOR(WorkfileWriteFail);

	if (data == NULL || size == 0)
	{
		return false;
	}

	/* Test the per-query and per-segment limit */
	if ((workfile->flags & EXEC_WORKFILE_LIMIT_SIZE) &&
			!WorkfileDiskspace_Reserve(size))
	{
		/* Failed to reserve additional disk space, notify caller */
		workfile_mgr_report_error();
	}

	switch(workfile->fileType)
	{
		case BUFFILE:
			{}
			BufFile *buffile = (BufFile *)workfile->file;

			int64 current_size = BufFileGetSize(buffile);
			int64 new_size = 0;

			PG_TRY();
			{
				bytes = BufFileWrite(buffile, data, size);
			}
			PG_CATCH();
			{
				new_size = BufFileGetSize(buffile);
				workfile->size = new_size;
				WorkfileDiskspace_Commit( (new_size - current_size), size, true /* update_query_size */);

				PG_RE_THROW();
			}
			PG_END_TRY();

			new_size = BufFileGetSize(buffile);
			workfile->size = new_size;

			WorkfileDiskspace_Commit( (new_size - current_size), size, true /* update_query_size */);
			workfile_set_update_in_progress_size(workfile->work_set, new_size - current_size);

			if (bytes != size)
			{
				workfile_mgr_report_error();
			}

			break;
		case BFZ:

			PG_TRY();
			{
				bfz_append((bfz_t *)workfile->file, data, size);
			}
			PG_CATCH();
			{
				Assert(WorkfileDiskspace_IsFull());
				WorkfileDiskspace_Commit(0, size, true /* update_query_size */);

				PG_RE_THROW();
			}
			PG_END_TRY();

			/* bfz_append always adds to the file size */
			workfile->size += size;
			if ((workfile->flags & EXEC_WORKFILE_LIMIT_SIZE))
			{
				WorkfileDiskspace_Commit(size, size, true /* update_query_size */);
			}
			workfile_set_update_in_progress_size(workfile->work_set, size);

			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	return true;
}

/*
 * ExecWorkFile_Read
 *    read the data with specified size to the given buffer.
 *
 * The given buffer should contain at least the space specified by 
 * 'size'.
 *
 * If the read succeeds, this function returns the number of bytes
 * that are read. Otherwise, returns 0.
 */
uint64
ExecWorkFile_Read(ExecWorkFile *workfile,
				  void *data,
				  uint64 size)
{
	Assert(workfile != NULL);
	uint64 bytes = 0;
	
	switch(workfile->fileType)
	{
		case BUFFILE:
			bytes = BufFileRead((BufFile *)workfile->file, data, size);
			break;
			
		case BFZ:
			bytes = bfz_scan_next((bfz_t *)workfile->file, data, size);
			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}
	
	return bytes;
}

/*
 * ExecWorkFile_ReadFromBuffer
 *
 * This function provides a faster implementation of Read which applies
 * when the data is already in the underlying buffer.
 * In that case, it returns a pointer to the data in the buffer
 * If the data is not in the buffer, returns NULL and the caller must
 * call the regular ExecWorkFile_Read with a destination buffer.
 *
 * Currently only bfz supports this behavior.
 *
 */
void *
ExecWorkFile_ReadFromBuffer(ExecWorkFile *workfile,
				  uint64 size)
{
	Assert(workfile != NULL);
	void *data = NULL;

	switch(workfile->fileType)
	{
		case BFZ:
			data = bfz_scan_peek((bfz_t *)workfile->file, size);
			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	return data;
}


/*
 * ExecWorkFile_Rewind
 *    rewind the pointer position to the beginning of the file.
 *
 * This function returns true if this succeeds. Otherwise, return false.
 */
bool
ExecWorkFile_Rewind(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);

	long ret = 0;
	int64 file_size = 0;
	bfz_t *f = NULL;
	switch(workfile->fileType)
	{
		case BUFFILE:
			ret = BufFileSeek((BufFile *)workfile->file, 0 /* fileno */, 0L  /* offset */, SEEK_SET);
			/* BufFileSeek returns 0 if everything went OK */
			return (0 == ret);
		case BFZ:
			f = ((bfz_t *)workfile->file);
			if (f->mode == BFZ_MODE_APPEND)
			{
				file_size = bfz_append_end((bfz_t *)workfile->file);
				ExecWorkFile_AdjustBFZSize(workfile, file_size);
			}
			/*
			 * For BFZ, the scan mode must be preceded by "BFZ_MODE_FREED"
			 * so that SCAN can verify that the bfz buffer was freed. This
			 * state is true when we were in append mode and we called bfz_append_end().
			 * However, we may need to rewind multiple times (e.g., spill files for
			 * rescannable hash join) after we finished appending. Therefore, to
			 * satisfy the state change prerequisite, we set the mode to BFZ_MODE_FREED
			 * regardless of bfz_append_end() call.
			 */
			f->mode = BFZ_MODE_FREED;
			bfz_scan_begin((bfz_t *)workfile->file);
			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	return true;
}

/*
 * ExecWorkFile_Tell64
 *    return the value of the current file position indicator.
 */
uint64
ExecWorkFile_Tell64(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	uint64 bytes = 0;

	switch(workfile->fileType)
	{
		case BUFFILE:
			BufFileTell((BufFile *)workfile->file, NULL, (off_t *) &bytes);
			break;
			
		case BFZ:
			bytes = bfz_totalbytes((bfz_t *)workfile->file);
			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	return bytes;
}

/*
 * ExecWorkFile_Close
 *    close the work file, and release the space.
 *
 *    Returns the actual size of the file on disk upon closing
 */
int64
ExecWorkFile_Close(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	bfz_t *bfz_file = NULL;

	switch(workfile->fileType)
	{
		case BUFFILE:
			BufFileClose((BufFile *)workfile->file);
			break;
			
		case BFZ:
			bfz_file = (bfz_t *)workfile->file;
			Assert(bfz_file != NULL);

			if (bfz_file->mode == BFZ_MODE_APPEND)
			{
				/* Flush data out to disk if we were writing */
				int64 file_size = bfz_append_end(bfz_file);
				/* Adjust the size with WorkfileDiskspace to our actual size */
				ExecWorkFile_AdjustBFZSize(workfile, file_size);
			}

			bfz_close(bfz_file);
			break;
		default:
			insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	int64 size = ExecWorkFile_GetSize(workfile);

	pfree(workfile->fileName);
	pfree(workfile);

	return size;
}

/*
 * Return the file name for the given workfile.
 * Note that this might have a different meaning, depending on the underlying
 * implementation.
 * For example, BufFile can span over several physical files, and thus this
 * is just a prefix.
 */
char *
ExecWorkFile_GetFileName(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	return workfile->fileName;
}

/*
 * ExecWorkFile_Seek
 *   Result is 0 if OK, EOF if not.  Logical position is not moved if an
 *   impossible seek is attempted.
 */
int
ExecWorkFile_Seek(ExecWorkFile *workfile, uint64 offset, int whence)
{
	Assert(workfile != NULL);
	Assert((workfile->flags & EXEC_WORKFILE_RANDOM_ACCESS) != 0);
	int result = 0;

	/* Determine if this seeks beyond EOF */
	int64 additional_size = 0;
	switch (whence)
	{
		case SEEK_SET:
			if (offset > workfile->size)
			{
				additional_size = offset - workfile->size;
			}
			break;

		case SEEK_CUR:
			if (ExecWorkFile_Tell64(workfile) + offset > workfile->size)
			{
				additional_size = ExecWorkFile_Tell64(workfile) + offset - workfile->size;
			}
			break;

		default:
			elog(LOG, "invalid whence: %d", whence);
			Assert(false);
			return EOF;
	}

	/* Reserve disk space if needed */
	if (additional_size > 0)
	{
		/*
		 * We only allow seeking beyond EOF for files opened for writing
		 *  (i.e. files we created)
		 */
		if (workfile->flags & EXEC_WORKFILE_CREATED)
		{
			bool success = WorkfileDiskspace_Reserve(additional_size);
			if (!success)
			{
				/* Failed to reserve additional disk space, notify caller */
				return EOF;
			}
		}
		else
		{
			return EOF;
		}
	}

	/* Do the actual seek */
	switch(workfile->fileType)
	{
	case BUFFILE:
		result = BufFileSeek((BufFile *)workfile->file, 0 /* fileno */, offset, whence);
		if (additional_size > 0)
		{
			workfile->size = BufFileGetSize((BufFile *)workfile->file);
		}
		break;
	default:
		insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	if (additional_size > 0)
	{
		WorkfileDiskspace_Commit(additional_size, additional_size, true /* update_query_size */);
		workfile_set_update_in_progress_size(workfile->work_set, additional_size);
	}

	return result;
}

void
ExecWorkFile_Flush(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	switch(workfile->fileType)
	{
	case BUFFILE:
		BufFileFlush((BufFile *) workfile->file);
		break;
	default:
		insist_log(false, "invalid work file type: %d", workfile->fileType);
	}
}

/*
 * Suspend a file without closing it. For bfz, which allocates a buffer for
 * each open a file, this frees up that buffer but keeps the fd so we can
 * re-open this file later
 *
 * Returns the actual size of the file on disk
 */
int64
ExecWorkFile_Suspend(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	Assert((workfile->flags & EXEC_WORKFILE_SUSPENDABLE) != 0);

	int64 size = -1;
	switch(workfile->fileType)
	{
	case BFZ:
		size = bfz_append_end((bfz_t *) workfile->file);
		ExecWorkFile_AdjustBFZSize(workfile, size);
		break;
	default:
		insist_log(false, "invalid work file type: %d", workfile->fileType);
	}
	return size;
}

/*
 * Re-open a suspended file for reading. This allocates all the necessary
 * buffers and data structures to restart reading from the file
 */
void
ExecWorkFile_Restart(ExecWorkFile *workfile)
{
	Assert(workfile != NULL);
	Assert((workfile->flags & EXEC_WORKFILE_SUSPENDABLE) != 0);

	switch(workfile->fileType)
	{
	case BFZ:
		bfz_scan_begin((bfz_t *) workfile->file);
		break;
	default:
		insist_log(false, "invalid work file type: %d", workfile->fileType);
	}
}

/*
 * Returns the size of the underlying file, as tracked by this API
 */
int64
ExecWorkFile_GetSize(ExecWorkFile *workfile)
{
	return workfile->size;
}

/*
 * Sets the pointer to the parent workfile_set
 */
void
ExecWorkfile_SetWorkset(ExecWorkFile *workfile, workfile_set *work_set)
{
	Assert(NULL != workfile);
	Assert(NULL != work_set);
	Assert(NULL == workfile->work_set);

	workfile->work_set = work_set;
}

/*
 * For a new workfile, sets the capabilities flags according to
 * the known underlying file type capabilities and the method the file was created
 */
static void
ExecWorkFile_SetFlags(ExecWorkFile *workfile, bool delOnClose, bool created)
{
	Assert(workfile != NULL);
	/* Assert that only the creator of a file can delete it on close */
	AssertImply(delOnClose, created);

	switch(workfile->fileType)
	{

	case BUFFILE:
		workfile->flags |= EXEC_WORKFILE_RANDOM_ACCESS;
		break;
	case BFZ:
		workfile->flags |= EXEC_WORKFILE_SUSPENDABLE;
		break;
	default:
		insist_log(false, "invalid work file type: %d", workfile->fileType);
	}

	if (delOnClose)
	{
		workfile->flags |= EXEC_WORKFILE_DEL_ON_CLOSE;
	}

	if (created)
	{
		workfile->flags |= EXEC_WORKFILE_CREATED;
		elog(gp_workfile_caching_loglevel, "Created workfile %s, delOnClose = %d",
				ExecWorkFile_GetFileName(workfile), delOnClose);
	}
	else
	{
		elog(gp_workfile_caching_loglevel, "Opened existing workfile %s, delOnClose = %d",
				ExecWorkFile_GetFileName(workfile), delOnClose);
	}

	if ((gp_workfile_limit_per_query > 0) || (gp_workfile_limit_per_segment > 0))
	{
		workfile->flags |= EXEC_WORKFILE_LIMIT_SIZE;
	}

}

/*
 * Correct the size for a BFZ file when we are done appending to it.
 *
 * During writing to a BFZ, the amount of bytes writen to disk differs from
 * the bytes passed in to write. We can have one of two situations:
 *  - BFZ is compressed. The size on disk is smaller than the bytes written
 *  - BFZ is uncompressed, and we're using checksumming. The size on disk is
 *    slightly larger than the bytes written
 *  Make the necessary correction here.
 */
static void
ExecWorkFile_AdjustBFZSize(ExecWorkFile *workfile, int64 file_size)
{
	Assert(workfile != NULL);

#if USE_ASSERT_CHECKING
	bfz_t *bfz_file = (bfz_t *) workfile->file;
#endif

	if (file_size <= workfile->size)
	{
		/*
		 * Actual size on disk is smaller than expected. This can happen in two cases:
		 * - file on disk is compressed
		 * - we hit out of disk space
		 */
		Assert(bfz_file->compression_index > 0 || WorkfileDiskspace_IsFull());
		WorkfileDiskspace_Commit(file_size, workfile->size, true /* update_query_size */);
		workfile_set_update_in_progress_size(workfile->work_set, file_size - workfile->size);
		workfile->size = file_size;

	}
	else
	{
		int64 extra_bytes = file_size - workfile->size;
		/* Actual file on disk is bigger than expected. This can happen when:
		 *  - added checksums to an uncompressed file
		 *  - closing empty or very small compressed file (zlib header overhead larger than saved space)
		 */
		Assert( (bfz_file->has_checksum && bfz_file->compression_index == 0) || (bfz_file->compression_index > 0 && workfile->size < BFZ_BUFFER_SIZE));

		/*
		 * If we're already under disk full, don't try to reserve, as it will
		 * fail anyway. We're in cleanup code in that case, and the file
		 * will be deleted soon.
		 */
		if (!WorkfileDiskspace_IsFull())
		{
			bool reserved = WorkfileDiskspace_Reserve(extra_bytes);

			if (!reserved)
			{
				elog(gp_workfile_caching_loglevel, "Could not reserve " INT64_FORMAT " additional bytes while adjusting for BFZ addtional size",
						extra_bytes);

				workfile_mgr_report_error();
			}

			WorkfileDiskspace_Commit(extra_bytes, extra_bytes, true /* update_query_size */);
			workfile_set_update_in_progress_size(workfile->work_set, extra_bytes);
			workfile->size = file_size;
		}
	}
}
/* EOF */
