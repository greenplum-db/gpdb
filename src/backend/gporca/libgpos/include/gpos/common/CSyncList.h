//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSyncList.h
//
//	@doc:
//		Template-based synchronized list class - no longer thread-safe
//
//		It requires that the elements are only inserted once, as their address
//		is used for identification;
//
//		In order to be useful for system programming the class must be
//		allocation-less, i.e. manage elements without additional allocation,
//		to work in exception or OOM situations;
//---------------------------------------------------------------------------
#ifndef GPOS_CSyncList_H
#define GPOS_CSyncList_H

#include "gpos/common/CList.h"
#include "gpos/types.h"

namespace gpos
{
//---------------------------------------------------------------------------
//	@class:
//		CSyncList<class T>
//
//	@doc:
//		Allocation-less list class; requires T to have a pointer to T embedded
//		and N being the offset of this member;
//
//---------------------------------------------------------------------------
template <class T>
class CSyncList
{
private:
	// underlying list
	CList<T> m_list;

public:
	CSyncList(const CSyncList &) = delete;

	// ctor
	CSyncList() = default;

	// dtor
	~CSyncList() = default;

	// init function to facilitate arrays
	void
	Init(ULONG offset)
	{
		m_list.Init(offset);
	}

	// insert element at the head of the list;
	void
	Push(T *elem)
	{
		m_list.Prepend(elem);
	}

	// remove element from the head of the list;
	T *
	Pop()
	{
		if (!m_list.IsEmpty())
		{
			return m_list.RemoveHead();
		}
		return nullptr;
	}

	// get first element
	T *
	PtFirst() const
	{
		return m_list.First();
	}

	// get next element
	T *
	Next(T *elem) const
	{
		return m_list.Next(elem);
	}

	// check if list is empty
	BOOL
	IsEmpty() const
	{
		return NULL == m_list.First();
	}

#ifdef GPOS_DEBUG

	// lookup a given element in the stack
	// this works only when no elements are removed
	GPOS_RESULT
	Find(T *elem) const
	{
		return m_list.Find(elem);
	}

	// debug print of element addresses
	// this works only when no elements are removed
	IOstream &
	OsPrint(IOstream &os) const
	{
		return m_list.OsPrint(os);
	}

#endif	// GPOS_DEBUG

};	// class CSyncList
}  // namespace gpos

#endif	// !GPOS_CSyncList_H


// EOF
