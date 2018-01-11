/*-------------------------------------------------------------------------
 *
 * cdbheap.c
 *
 * Generic heap functions, that can be used with any
 * data structure and comparator function
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbheap.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbheap.h"		/* me */


static void
			SiftDown(CdbHeap *hp, int iHole, void *newElement);


static inline void
CdbHeap_CopySlot(CdbHeap *hp, void *tgtSlot, const void *srcSlot)
{
	int		   *tgt = (int *) tgtSlot;
	const int  *src = (const int *) srcSlot;
	int			j = hp->wordsPerSlot;

	tgt[0] = src[0];
	if (--j > 0)
	{
		tgt[1] = src[1];
		if (--j > 0)
		{
			tgt[2] = src[2];
			while (--j > 0)
				tgt[j + 2] = src[j + 2];
		}
	}
}								/* CdbHeap_CopySlot */


/* Allocate and initialize a CdbHeap structure. */
CdbHeap *
CdbHeap_Create(CdbHeapCmpFn comparator,
			   void *comparatorContext,
			   int nSlotsMax,
			   int bytesPerSlot,
			   void *slotArray)
{
	CdbHeap    *hp = (CdbHeap *) palloc0(sizeof(*hp) + bytesPerSlot);

	Assert(comparator && nSlotsMax > 0);

	/* Initialize the CdbHeap structure. */
	hp->nSlotsUsed = 0;
	hp->nSlotsMax = nSlotsMax;
	hp->bytesPerSlot = bytesPerSlot;
	hp->wordsPerSlot = bytesPerSlot / sizeof(int);
	hp->comparator = comparator;
	hp->comparatorContext = comparatorContext;

	/* A 1-element temporary buffer immediately follows the CdbHeap struct. */
	hp->tempSlot = (char *) (hp + 1);

	/* Allocate space for array of priority queue entries. */
	hp->slotArray = slotArray;
	hp->ownSlotArray = false;
	if (!slotArray)
	{
		hp->slotArray = palloc0(nSlotsMax * bytesPerSlot);
		hp->ownSlotArray = true;
	}

	Assert(hp->wordsPerSlot > 0 &&
		   (int) (hp->wordsPerSlot * sizeof(int)) == bytesPerSlot);

	return hp;
}								/* CdbHeap_Create */


/* Free a CdbHeap structure. */
void
CdbHeap_Destroy(CdbHeap *hp)
{
	if (hp)
	{
		/* Free slotArray if we allocated it.  Don't free if caller owns it. */
		if (hp->ownSlotArray &&
			hp->slotArray)
			pfree(hp->slotArray);

		pfree(hp);
	}
}								/* CdbHeap_Destroy */


/* Arrange elements of slotArray such that the heap property is satisfied. */
void
CdbHeap_Heapify(CdbHeap *hp, int nSlotsUsed)
{
	Assert(hp &&
		   nSlotsUsed >= 0 &&
		   nSlotsUsed <= hp->nSlotsMax);

	hp->nSlotsUsed = nSlotsUsed;
	if (nSlotsUsed > 1)
	{
		int			i;

		for (i = nSlotsUsed / 2 - 1; i >= 0; i--)
		{
			/* Make a hole at slot i by moving its contents to temp area. */
			CdbHeap_CopySlot(hp, hp->tempSlot, CdbHeap_Slot(void, hp, i));

			/* Refill the hole, moving smallest descendant into slot i. */
			SiftDown(hp, i, hp->tempSlot);
		}
	}
}								/* CdbHeap_Heapify */

/* Delete the smallest element. */
void
CdbHeap_DeleteMin(CdbHeap *hp)
{
	Assert(hp && hp->nSlotsUsed > 0);

	/* Heap shrinks by one element. */
	hp->nSlotsUsed--;

	/* Sift down the rightmost element, refilling hole at root with new min. */
	if (hp->nSlotsUsed > 0)
		SiftDown(hp, 0, CdbHeap_Slot(void, hp, hp->nSlotsUsed));
}								/* CdbHeap_DeleteMin */


/* Delete the smallest element and insert a copy of the given element. */
void
CdbHeap_DeleteMinAndInsert(CdbHeap *hp, void *newElement)
{
	Assert(hp &&
		   hp->nSlotsUsed > 0 &&
		   newElement);

	/* Sift down the new element, refilling hole at root with new min. */
	SiftDown(hp, 0, newElement);
}								/* CdbHeap_DeleteMinAndInsert */


void
SiftDown(CdbHeap *hp, int iHole, void *newElement)
{
	CdbHeapCmpFn comparator = hp->comparator;
	void	   *comparatorContext = hp->comparatorContext;
	int			bytesPerSlot = hp->bytesPerSlot;
	char	   *slot0 = CdbHeap_Slot(char, hp, 0);
	char	   *slotN = CdbHeap_Slot(char, hp, hp->nSlotsUsed);
	char	   *firstLeafSlot = CdbHeap_Slot(char, hp, hp->nSlotsUsed >> 1);
	char	   *curSlot;
	char	   *kidSlot;

	Assert(iHole >= 0 &&
		   iHole <= hp->nSlotsUsed &&
		   iHole < hp->nSlotsMax &&
		   newElement);

	/* Bubble up the new min value into the hole; the hole sinks down. */
	for (curSlot = iHole * bytesPerSlot + slot0;
		 curSlot < firstLeafSlot;
		 curSlot = kidSlot)
	{
		/* Point to left child (could be a leaf). */
		kidSlot = curSlot - slot0 + bytesPerSlot + curSlot;

		/* If right child exists and has lesser value, choose it instead. */
		if (kidSlot + bytesPerSlot < slotN &&
			(*comparator) (kidSlot + bytesPerSlot, kidSlot, comparatorContext) < 0)
			kidSlot += bytesPerSlot;

		/* Hole comes to rest where new value <= all descendants. */
		if ((*comparator) (newElement, kidSlot, comparatorContext) <= 0)
			break;

		/* Hole trades places with lesser child. */
		CdbHeap_CopySlot(hp, curSlot, kidSlot);
	}

	/* Fill the hole with the given element. */
	CdbHeap_CopySlot(hp, curSlot, newElement);
}								/* SiftDown */
