/*-------------------------------------------------------------------------
 *
 * workfile_file.c
 *	 Implementation of workfile manager file operations
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/workfile_manager/workfile_file.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "utils/faultinjector.h"
#include "utils/workfile_mgr.h"

static void retrieve_file_no(workfile_set *work_set, uint32 file_no, char *workfile_name, uint32 workfile_name_len);
static void update_workset_size(workfile_set *work_set, bool delOnClose, bool created, int64 size);
static void adjust_size_temp_file_new(workfile_set *work_set, int64 size);
static void adjust_size_persistent_file_new(workfile_set *work_set, int64 size);
static void adjust_size_temp_file_existing(workfile_set *work_set, int64 size);
static void adjust_size_persistent_file_existing(workfile_set *work_set, int64 size);

/*
 * Creates a new workfile in a given set
 *
 *  The next file name in the sequence is generated
 */
ExecWorkFile *
workfile_mgr_create_file(workfile_set *work_set)
{
	Assert(NULL != work_set);

	work_set->no_files++;
	return workfile_mgr_create_fileno(work_set, work_set->no_files);
}

/*
 * Creates a new numbered workfile in a given set
 *
 *  The given file_no is used to generate the file name
 */
ExecWorkFile *
workfile_mgr_create_fileno(workfile_set *work_set, uint32 file_no)
{
	Assert(NULL != work_set);

	char file_name[MAXPGPATH];
	retrieve_file_no(work_set, file_no, file_name, sizeof(file_name));

	ExecWorkFile *ewfile = ExecWorkFile_Create(file_name,
			work_set->metadata.type,
			true /* del_on_close */,
			work_set->metadata.bfz_compress_type);

	SIMPLE_FAULT_INJECTOR(WorkfileCreationFail);

	ExecWorkfile_SetWorkset(ewfile, work_set);

	return ewfile;
}

/*
 * Closes a given workfile and updates the diskspace accordingly
 *
 *  work_set can be NULL for workfile that were created outside of the workfile manager,
 *  e.g. for ShareInputScan workfiles or window functions
 *
 *  Returns the actual size of the file on disk in bytes upon closing
 */
int64
workfile_mgr_close_file(workfile_set *work_set, ExecWorkFile *file)
{
	Assert(NULL != file);

	bool delOnClose = file->flags & EXEC_WORKFILE_DEL_ON_CLOSE;
	bool created = file->flags & EXEC_WORKFILE_CREATED;
	elog(gp_workfile_caching_loglevel, "closing file %s, delOnClose=%d", ExecWorkFile_GetFileName(file), delOnClose);

	int64 size = 0;
	PG_TRY();
	{
		size = ExecWorkFile_Close(file);
	}
	PG_CATCH();
	{
		elog(gp_workfile_caching_loglevel, "Caught exception, file=%s, returned size=" INT64_FORMAT " actual size before adjustment=" INT64_FORMAT,
				file->fileName, size, ExecWorkFile_GetSize(file));

		update_workset_size(work_set, delOnClose, created, ExecWorkFile_GetSize(file));

		PG_RE_THROW();
	}
	PG_END_TRY();

	update_workset_size(work_set, delOnClose, created, size);
	return size;
}

/*
 * Update the size of a workset after closing a member file
 *  work_set is the parent workset of the file
 *  delOnClose is set if the file is to be deleted upon closing (temporary)
 *  created is set if the file was created by this backend
 *  size is the final size of the file at the time of the close
 *
 */
static void
update_workset_size(workfile_set *work_set, bool delOnClose, bool created, int64 size)
{

	if (delOnClose)
	{
		if (created)
		{
			adjust_size_temp_file_new(work_set, size);
		}
		else
		{
			adjust_size_temp_file_existing(work_set, size);
		}
	}
	else
	{
		if (created)
		{
			adjust_size_persistent_file_new(work_set, size);
		}
		else
		{
			adjust_size_persistent_file_existing(work_set, size);
		}
	}
}

/*
 * Updating accounting of size when closing a temporary file we created
 */
static void
adjust_size_temp_file_new(workfile_set *work_set, int64 size)
{
#if USE_ASSERT_CHECKING
	bool isCached = (NULL != work_set) && Cache_IsCached(CACHE_ENTRY_HEADER(work_set));
#endif
	Assert(!isCached);
	AssertImply((NULL != work_set), work_set->size == 0);
	AssertImply((NULL != work_set), work_set->in_progress_size >= size);

	/*
	 * Decrement the size with work_set->in_progress_size or
	 * used_segspace_not_in_workfile_set
	 */
	workfile_set_update_in_progress_size(work_set, -size);

	WorkfileDiskspace_Commit(0 /* commit_bytes */, size, true /* update_query_size */);
	elog(gp_workfile_caching_loglevel, "closed and deleted temp file, subtracted size " INT64_FORMAT " from disk space", size);

	/* About to physically delete a file we created. Update the per-query file count as well */
	WorkfileQueryspace_SubtractWorkfile(1 /* nFiles */);
}

/*
 * Updating accounting of size when closing a persistent file we created
 */
static void
adjust_size_persistent_file_new(workfile_set *work_set, int64 size)
{
#if USE_ASSERT_CHECKING
	bool isCached = (NULL != work_set) && Cache_IsCached(CACHE_ENTRY_HEADER(work_set));
#endif
	Assert(NULL != work_set && !isCached);

	work_set->size += size;
	elog(gp_workfile_caching_loglevel, "closed new persistent file, added size " INT64_FORMAT " to set space", size);
}

/*
 * Updating accounting of size when closing an existing temporary file
 * we opened for reading
 */
static void
adjust_size_temp_file_existing(workfile_set *work_set, int64 size)
{

	elog(gp_workfile_caching_loglevel, "closing existing temp file, not deleting, nothing to do");
	Assert(false && "We should not open existing temp files");
	return;
}

/*
 * Updating accounting of size when closing an existing persistent file
 * we opened for reading
 */
static void
adjust_size_persistent_file_existing(workfile_set *work_set, int64 size)
{
#if USE_ASSERT_CHECKING
	bool isCached = (NULL != work_set) && Cache_IsCached(CACHE_ENTRY_HEADER(work_set));
#endif

	AssertEquivalent((NULL != work_set), isCached);

	elog(gp_workfile_caching_loglevel, "closing existing persistent file, nothing to do");

	return;
}


/*
 *  Retrieves the name of the file_no file in the workset. If the file was not
 * 	already in the work_set, it is added.
 * 	This can create gaps in the work_set if file_no > work_set->no_files.
 *
 *  workfile_name is allocated by the caller. It will contain the name for
 *  the new file.
 */
static void
retrieve_file_no(workfile_set *work_set, uint32 file_no, char *workfile_name, uint32 workfile_name_len)
{
	Assert(work_set);
	Assert(workfile_name);

	if (file_no >= work_set->no_files)
	{
		/* Retrieving file beyond the end of the set. Adjusting the size of the set */
		work_set->no_files = file_no + 1;

	}
	snprintf(workfile_name, workfile_name_len,
			"%s/spillfile_f%u", work_set->path, file_no);
}
