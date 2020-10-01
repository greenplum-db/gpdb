//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum Inc.
//
//	@filename:
//		COstreamString.h
//
//	@doc:
//		Output string stream class;
//---------------------------------------------------------------------------
#ifndef GPOS_COstreamString_H
#define GPOS_COstreamString_H

#include "gpos/io/COstream.h"
#include "gpos/string/CWString.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		COstreamString
//
//	@doc:
//		Implements an output stream writing to a string
//
//---------------------------------------------------------------------------
class COstreamString : public COstream
{
private:
	// underlying string
	CWString *m_string;

	// private copy ctor
	COstreamString(const COstreamString &);

public:
	// please see comments in COstream.h for an explanation
	using COstream::operator<<;

	// ctor
	explicit COstreamString(CWString *);

	virtual ~COstreamString()
	{
	}

	// implement << operator on wide char array
	virtual IOstream &operator<<(const WCHAR *wc_array);

	// implement << operator on char array
	virtual IOstream &operator<<(const CHAR *c_array);

	// implement << operator on wide char
	virtual IOstream &operator<<(const WCHAR wc);

	// implement << operator on char
	virtual IOstream &operator<<(const CHAR c);
};

}  // namespace gpos

#endif	// !GPOS_COstreamString_H

// EOF
