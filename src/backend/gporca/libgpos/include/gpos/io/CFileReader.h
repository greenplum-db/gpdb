//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CFileReader.h
//
//	@doc:
//		File Reader
//---------------------------------------------------------------------------

#ifndef GPOS_CFileReader_H
#define GPOS_CFileReader_H

#include <fcntl.h>
#include "gpos/io/CFileDescriptor.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CFileReader
//
//	@doc:
//		Implementation of file handler for raw input;
//		does not provide thread-safety
//
//---------------------------------------------------------------------------
class CFileReader : public CFileDescriptor
{
private:
	// file size
	ULLONG m_file_size;

	// read size
	ULLONG m_file_read_size;

	// no copy ctor
	CFileReader(const CFileReader &);

public:
	// ctor
	CFileReader();

	// dtor
	virtual ~CFileReader();

	// get file size
	ULLONG FileSize() const;

	// get file read size
	ULLONG FileReadSize() const;

	// open file for reading
	void Open(const CHAR *file_path, const ULONG permission_bits = S_IRUSR);

	// close file
	void Close();

	// read bytes to buffer
	ULONG_PTR ReadBytesToBuffer(BYTE *read_buffer,
								const ULONG_PTR file_read_size);

};	// class CFileReader

}  // namespace gpos

#endif	// !GPOS_CFileReader_H

// EOF
