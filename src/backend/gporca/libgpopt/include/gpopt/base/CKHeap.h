//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CKHeap.h
//
//	@doc:
//		Optimizer binary heap utility functions
//---------------------------------------------------------------------------
#ifndef GPOPT_CKHeap_H
#define GPOPT_CKHeap_H

#include "gpopt/base/CColRef.h"

namespace gpopt
{
	using namespace gpos;

	// a heap keeping the k lowest-cost objects in an array of class A
	// A is a CDynamicPtrArray
	// E is the entry type of the array and it has a method CDouble DCost()
	// See https://en.wikipedia.org/wiki/Binary_heap for details
	// (with the added feature of returning only the top k).
	template<class A, class E>
	class CKHeap : public CRefCount
	{
	private:

		A *m_topk;
		CMemoryPool *m_mp;
		ULONG m_k;
		BOOL m_is_heapified;
		ULONG m_num_returned;

		// the parent index is (ix-1)/2, except for 0
		ULONG parent(ULONG ix) { return (0 < ix ? (ix-1)/2 : m_topk->Size()); }

		// children are at indexes 2*ix + 1 and 2*ix + 2
		ULONG left_child(ULONG ix)  { return 2*ix + 1; }
		ULONG right_child(ULONG ix) { return 2*ix + 2; }

		// does the parent/child exist?
		BOOL exists(ULONG ix) { return ix < m_topk->Size(); }
		// cost of an entry (this class implements a Min-Heap)
		CDouble cost(ULONG ix) { return (*m_topk)[ix]->GetCostForHeap(); }

		// push node ix in the tree down into its child tree as much as needed
		void HeapifyDown(ULONG ix)
		{
			ULONG left_child_ix = left_child(ix);
			ULONG right_child_ix = right_child(ix);
			ULONG min_element_ix = ix;

			if (exists(left_child_ix) && cost(left_child_ix) < cost(ix))
				// left child is better than parent, it becomes the new candidate
				min_element_ix = left_child_ix;

			if (exists(right_child_ix) && cost(right_child_ix) < cost(min_element_ix))
				// right child is better than min(parent, left child)
				min_element_ix = right_child_ix;

			if (min_element_ix != ix)
			{
				// make the lowest of { parent, left child, right child } the new root
				m_topk->Swap(ix, min_element_ix);
				HeapifyDown(min_element_ix);
			}
		}

		// pull node ix in the tree up as much as needed
		void HeapifyUp(ULONG ix)
		{
			ULONG parent_ix = parent(ix);

			if (!exists(parent_ix))
				return;

			if (cost(ix) < cost(parent_ix))
			{
				m_topk->Swap(ix, parent_ix);
				HeapifyUp(parent_ix);
			}
		}

		// Convert the array into a heap, heapify-down all interior nodes of the tree, bottom-up.
		// Note that we keep all the entries, not just the top k, since our k-heaps are short-lived.
		// You can only retrieve the top k with RemoveBestElement(), though.
		void Heapify()
		{
			// the parent of the last node is the last node in the tree that is a parent
			ULONG start_ix = parent(m_topk->Size()-1);

			// now work our way up to the root, calling HeapifyDown
			for (ULONG ix=start_ix; exists(ix); ix--)
				HeapifyDown(ix);

			m_is_heapified = true;
		}

	public:

		CKHeap(CMemoryPool *mp, ULONG k)
		:
		m_mp(mp),
		m_k(k),
		m_is_heapified(false),
		m_num_returned(0)
		{
			m_topk = GPOS_NEW(m_mp) A(m_mp);
		}

		~CKHeap()
		{
			m_topk->Release();
		}

		void Insert(E *elem)
		{
			GPOS_ASSERT(NULL != elem);
			// since the cost may change as we find more expressions in the group,
			// we just append to the array now and heapify at the end
			GPOS_ASSERT(!m_is_heapified);
			m_topk->Append(elem);

			// this is dead code at the moment, but other users might want to
			// heapify and then insert additional items
			if (m_is_heapified)
			{
				HeapifyUp(m_topk->Size()-1);
			}
		}

		// remove the next of the top k elements, sorted ascending by cost
		E *RemoveBestElement()
		{
			if (0 == m_topk->Size() || m_k <= m_num_returned)
			{
				return NULL;
			}

			m_num_returned++;

			return RemoveNextElement();
		}

		// remove the next best element, without the top k limit
		E *RemoveNextElement()
		{
			if (0 == m_topk->Size())
			{
				return NULL;
			}

			if (!m_is_heapified)
				Heapify();

			// we want to remove and return the root of the tree, which is the best element

			// first, swap the root with the last element in the array
			m_topk->Swap(0, m_topk->Size()-1);

			// now remove the new last element, which is the real root
			E * result = m_topk->RemoveLast();

			// then push the new first element down to the correct place
			HeapifyDown(0);

			return result;
		}

		ULONG Size()
		{
			return m_topk->Size();
		}

		BOOL IsLimitExceeded()
		{
			return m_topk->Size() + m_num_returned > m_k;
		}

		void Clear()
		{
			m_topk->Clear();
			m_is_heapified = false;
			m_num_returned = 0;
		}

	}; // class CKHeap

} // namespace gpopt

#endif // !GPOPT_CKHeap_H

// EOF
