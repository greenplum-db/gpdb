//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		ioutils.cpp
//
//	@doc:
//		Implementation of I/O utilities
//---------------------------------------------------------------------------

#include "gpos/io/ioutils.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gpos/base.h"
#include "gpos/common/clibwrapper.h"
#include "gpos/error/CLogger.h"
#include "gpos/string/CStringStatic.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/task/CTaskContext.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		ioutils::CheckState
//
//	@doc:
//		Check state of file or directory
//
//---------------------------------------------------------------------------
void
gpos::ioutils::CheckState(const CHAR *file_path, SFileStat *file_state)
{
	GPOS_ASSERT(nullptr != file_path);
	GPOS_ASSERT(nullptr != file_state);

	// reset file state
	(void) clib::Memset(file_state, 0, sizeof(*file_state));

	INT res;

	res = stat(file_path, file_state);

	if (0 != res)
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiIOError, errno);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::FStat
//
//	@doc:
//		Check state of file or directory by file descriptor
//
//---------------------------------------------------------------------------
void
gpos::ioutils::CheckStateUsingFileDescriptor(const INT file_descriptor,
											 SFileStat *file_state)
{
	GPOS_ASSERT(nullptr != file_state);

	// reset file state
	(void) clib::Memset(file_state, 0, sizeof(*file_state));

	INT res;

	res = fstat(file_descriptor, file_state);

	if (0 != res)
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiIOError, errno);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::PathExists
//
//	@doc:
//		Check if path is mapped to an accessible file or directory
//
//---------------------------------------------------------------------------
BOOL
gpos::ioutils::PathExists(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);

	SFileStat fs;

	INT res = stat(file_path, &fs);

	return (0 == res);
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::IsDir
//
//	@doc:
//		Check if path is directory
//
//---------------------------------------------------------------------------
BOOL
gpos::ioutils::IsDir(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);

	SFileStat fs;
	CheckState(file_path, &fs);

	return S_ISDIR(fs.st_mode);
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::IsFile
//
//	@doc:
//		Check if path is file
//
//---------------------------------------------------------------------------
BOOL
gpos::ioutils::IsFile(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);

	SFileStat fs;
	CheckState(file_path, &fs);

	return S_ISREG(fs.st_mode);
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::FileSize
//
//	@doc:
//		Get file size by file path
//
//---------------------------------------------------------------------------
ULLONG
gpos::ioutils::FileSize(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);
	GPOS_ASSERT(IsFile(file_path));

	SFileStat fs;
	CheckState(file_path, &fs);

	return fs.st_size;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::FileSize
//
//	@doc:
//		Get file size by file descriptor
//
//---------------------------------------------------------------------------
ULLONG
gpos::ioutils::FileSize(const INT file_descriptor)
{
	SFileStat fs;
	CheckStateUsingFileDescriptor(file_descriptor, &fs);

	return fs.st_size;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::CheckFilePermissions
//
//	@doc:
//		Check permissions
//
//---------------------------------------------------------------------------
BOOL
gpos::ioutils::CheckFilePermissions(const CHAR *file_path,
									ULONG permission_bits)
{
	GPOS_ASSERT(nullptr != file_path);

	SFileStat fs;
	CheckState(file_path, &fs);

	return (permission_bits == (fs.st_mode & permission_bits));
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::CreateDir
//
//	@doc:
//		Create directory with specific permissions
//
//---------------------------------------------------------------------------
void
gpos::ioutils::CreateDir(const CHAR *file_path, ULONG permission_bits)
{
	GPOS_ASSERT(nullptr != file_path);

	INT res;

	res = mkdir(file_path, (MODE_T) permission_bits);

	if (0 != res)
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiIOError, errno);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::RemoveDir
//
//	@doc:
//		Delete directory
//
//---------------------------------------------------------------------------
void
gpos::ioutils::RemoveDir(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);
	GPOS_ASSERT(IsDir(file_path));

	INT res;

	// delete existing directory
	res = rmdir(file_path);

	if (0 != res)
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiIOError, errno);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::Unlink
//
//	@doc:
//		Delete file
//
//---------------------------------------------------------------------------
void
gpos::ioutils::Unlink(const CHAR *file_path)
{
	GPOS_ASSERT(nullptr != file_path);

	// delete existing file
	(void) unlink(file_path);
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::OpenFile
//
//	@doc:
//		Open a file;
//		It shall establish the connection between a file
//		and a file descriptor
//
//---------------------------------------------------------------------------
INT
gpos::ioutils::OpenFile(const CHAR *file_path, INT mode, INT permission_bits)
{
	GPOS_ASSERT(nullptr != file_path);

	INT res = open(file_path, mode, permission_bits);

	GPOS_ASSERT((0 <= res) || (EINVAL != errno));

	return res;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::CloseFile
//
//	@doc:
//		Close a file descriptor
//
//---------------------------------------------------------------------------
INT
gpos::ioutils::CloseFile(INT file_descriptor)
{
	INT res = close(file_descriptor);

	GPOS_ASSERT(0 == res || EBADF != errno);

	return res;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::GetFileState
//
//	@doc:
//		Get file status
//
//---------------------------------------------------------------------------
INT
gpos::ioutils::GetFileState(INT file_descriptor, SFileStat *file_state)
{
	GPOS_ASSERT(nullptr != file_state);

	INT res = fstat(file_descriptor, file_state);

	GPOS_ASSERT(0 == res || EBADF != errno);

	return res;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::Write
//
//	@doc:
//		Write to a file descriptor
//
//---------------------------------------------------------------------------
INT_PTR
gpos::ioutils::Write(INT file_descriptor, const void *buffer,
					 const ULONG_PTR ulpCount)
{
	GPOS_ASSERT(nullptr != buffer);
	GPOS_ASSERT(0 < ulpCount);
	GPOS_ASSERT(ULONG_PTR_MAX / 2 > ulpCount);

	SSIZE_T res = write(file_descriptor, buffer, ulpCount);

	GPOS_ASSERT((0 <= res) || EBADF != errno);

	return res;
}


//---------------------------------------------------------------------------
//	@function:
//		ioutils::Read
//
//	@doc:
//		Read from a file descriptor
//
//---------------------------------------------------------------------------
INT_PTR
gpos::ioutils::Read(INT file_descriptor, void *buffer, const ULONG_PTR ulpCount)
{
	GPOS_ASSERT(nullptr != buffer);
	GPOS_ASSERT(0 < ulpCount);
	GPOS_ASSERT(ULONG_PTR_MAX / 2 > ulpCount);

	SSIZE_T res = read(file_descriptor, buffer, ulpCount);

	GPOS_ASSERT((0 <= res) || EBADF != errno);

	return res;
}

//---------------------------------------------------------------------------
//	@function:
//		ioutils::CreateTempDir
//
//	@doc:
//		Create a unique temporary directory
//
//---------------------------------------------------------------------------
void
gpos::ioutils::CreateTempDir(CHAR *dir_path)
{
	GPOS_ASSERT(nullptr != dir_path);

#ifdef GPOS_DEBUG
	const SIZE_T ulNumOfCmp = 6;

	SIZE_T size = clib::Strlen(dir_path);

	GPOS_ASSERT(size > ulNumOfCmp);

	GPOS_ASSERT(0 == clib::Memcmp("XXXXXX", dir_path + (size - ulNumOfCmp),
								  ulNumOfCmp));
#endif	// GPOS_DEBUG

	CHAR *szRes;

	// check to simulate I/O error
	szRes = mkdtemp(dir_path);

	if (nullptr == szRes)
	{
		GPOS_RAISE(CException::ExmaSystem, CException::ExmiIOError, errno);
	}

	return;
}

// EOF
