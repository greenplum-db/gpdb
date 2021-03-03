//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 - 2010 Greenplum, Inc.
//
//	@filename:
//		COstreamBasic.h
//
//	@doc:
//		Output stream base class;
//---------------------------------------------------------------------------
#ifndef GPOS_COstreamBasic_H
#define GPOS_COstreamBasic_H

#include "gpos/io/COstream.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		COstreamBasic
//
//	@doc:
//		Implements a basic write thru interface over a std::WOSTREAM
//
//---------------------------------------------------------------------------

class COstreamBasic : public COstream
{
private:
	// underlying stream
	WOSTREAM *m_ostream;

public:
	COstreamBasic(const COstreamBasic &) = delete;

	// please see comments in COstream.h for an explanation
	using COstream::operator<<;

	// ctor
	explicit COstreamBasic(WOSTREAM *ostream);

	~COstreamBasic() override = default;

	// implement << operator
	IOstream &operator<<(const WCHAR *) override;

	// implement << operator
	IOstream &operator<<(const WCHAR) override;
};

}  // namespace gpos

#endif	// !GPOS_COstreamBasic_H

// EOF
