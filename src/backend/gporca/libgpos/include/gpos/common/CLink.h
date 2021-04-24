
//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal
//
//---------------------------------------------------------------------------
#ifndef GPOS_CLink_H
#define GPOS_CLink_H


#include <cstddef>
namespace gpos
{
// Generic link to be embedded in all classes before they can use
// allocation-less lists, e.g. in synchronized hashtables etc.
struct SLink
{
	// link forward/backward
	void *m_next{nullptr};
	void *m_prev{nullptr};
};

}  // namespace gpos

#endif	// !GPOS_CLink_H

// EOF
