//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		COstream.h
//
//	@doc:
//		Partial implementation of output stream interface;
//---------------------------------------------------------------------------
#ifndef GPOS_COstream_H
#define GPOS_COstream_H

#include "gpos/base.h"
#include "gpos/io/IOstream.h"
#include "gpos/string/CWStringStatic.h"

// conversion buffer size
#define GPOS_OSTREAM_CONVBUF_SIZE (256)

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		COstream
//
//	@doc:
//		Defines all available operator interfaces; avoids having to overload
//		system stream classes or their operators/member functions;
//		When inheriting from this class, C++ hides 'all' overloaded
//		versions of a function in the subclass, by default. Therefore, the
//		compiler will not be able to 'see' the default implementations of the <<
//		operator in subclasses of COstream. Use the 'using' keyword as in
//		COstreamBasic.h to avoid the problem. Also refer to
//		Effective C++ Third Edition, pp156
//
//---------------------------------------------------------------------------

class COstream : public IOstream
{
protected:
	// constructor
	COstream();

public:
	using IOstream::operator<<;

	COstream(COstream &) = delete;

	// virtual dtor
	~COstream() override = default;

	// default implementations for the following interfaces available
	IOstream &operator<<(const CHAR *) override;
	IOstream &operator<<(const WCHAR) override;
	IOstream &operator<<(const CHAR) override;
	IOstream &operator<<(ULONG) override;
	IOstream &operator<<(ULLONG) override;
	IOstream &operator<<(INT) override;
	IOstream &operator<<(LINT) override;
	IOstream &operator<<(DOUBLE) override;
	IOstream &operator<<(const void *) override;

	// to support std:endl only
	IOstream &operator<<(WOSTREAM &(*) (WOSTREAM &) ) override;

	// set the stream modifier
	IOstream &operator<<(EStreamManipulator) override;

private:
	// formatting buffer
	WCHAR m_string_format_buffer[GPOS_OSTREAM_CONVBUF_SIZE];

	// wrapper string for formatting buffer
	CWStringStatic m_static_string_buffer;

	// current mode
	EStreamManipulator m_stream_manipulator{EsmDec};

	// append formatted string
	IOstream &AppendFormat(const WCHAR *format, ...);

	// what is the stream modifier?
	EStreamManipulator GetStreamManipulator() const;
};

}  // namespace gpos

#endif	// !GPOS_COstream_H

// EOF
