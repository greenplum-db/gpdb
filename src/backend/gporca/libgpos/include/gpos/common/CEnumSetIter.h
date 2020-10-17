//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CEnumSetIter.h
//
//	@doc:
//		Implementation of iterator for enum set
//---------------------------------------------------------------------------
#ifndef GPOS_CEnumSetIter_H
#define GPOS_CEnumSetIter_H

#include "gpos/base.h"
#include "gpos/common/CEnumSet.h"
#include "gpos/common/CBitSetIter.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CEnumSetIter
//
//	@doc:
//		Template derived from CBitSetIter
//
//---------------------------------------------------------------------------
template <class T, ULONG sentinel_index>
class CEnumSetIter : public CBitSetIter
{
private:
	// private copy ctor
	CEnumSetIter<T, sentinel_index>(const CEnumSetIter<T, sentinel_index> &);

public:
	// ctor
	explicit CEnumSetIter<T, sentinel_index>(
		const CEnumSet<T, sentinel_index> &enum_set)
		: CBitSetIter(enum_set)
	{
	}

	// dtor
	~CEnumSetIter<T, sentinel_index>()
	{
	}

	// current enum
	T
	TBit() const
	{
		GPOS_ASSERT(sentinel_index > CBitSetIter::Bit() &&
					"Out of range of enum");
		return static_cast<T>(CBitSetIter::Bit());
	}

};	// class CEnumSetIter
}  // namespace gpos


#endif	// !GPOS_CEnumSetIter_H

// EOF
