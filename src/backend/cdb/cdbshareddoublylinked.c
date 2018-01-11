/*-------------------------------------------------------------------------
 *
 * cdbshareddoublylinked.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbshareddoublylinked.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "cdb/cdbshareddoublylinked.h"

void
SharedListBase_Init(
					SharedListBase *base,
					void *data,
					int size,
					int offsetToDoubleLinks)
{
	base->data = data;
	base->size = size;
	base->offsetToDoubleLinks = offsetToDoubleLinks;
}

void
SharedDoublyLinkedHead_Init(
							SharedDoublyLinkedHead *head)
{
	head->count = 0;
	head->first = -1;
	head->last = -1;
}

void
SharedDoubleLinks_Init(
					   SharedDoubleLinks *doubleLinks,
					   int index)
{
	doubleLinks->index = index;
	doubleLinks->prev = -1;
	doubleLinks->next = -1;
}

SharedDoubleLinks *
SharedDoubleLinks_FromElement(
							  SharedListBase *base,
							  void *current)
{
	uint8	   *uint8Current = (uint8 *) current;

	return (SharedDoubleLinks *) (uint8Current + base->offsetToDoubleLinks);
}

void *
SharedListBase_ToElement(
						 SharedListBase *base,
						 int index)
{
	Assert(base != NULL);
	Assert(index >= 0);

	return base->data + (index * base->size);
}

void *
SharedDoublyLinkedHead_First(
							 SharedListBase *base,
							 SharedDoublyLinkedHead *head)
{
	Assert(base != NULL);
	Assert(head != NULL);

	if (head->first != -1)
	{
		void	   *firstEle;
		SharedDoubleLinks *firstDoubleLinks;

		Assert(head->first >= 0);
		firstEle = SharedListBase_ToElement(base, head->first);
		firstDoubleLinks = SharedDoubleLinks_FromElement(base, firstEle);
		Assert(firstDoubleLinks->index == head->first);

		return firstEle;
	}
	else
		return NULL;
}

SharedDoubleLinks *
SharedListBase_ToDoubleLinks(
							 SharedListBase *base,
							 int index)
{
	SharedDoubleLinks *sharedDoubleLinks;

	Assert(base != NULL);
	Assert(index >= 0);

	sharedDoubleLinks =
		(SharedDoubleLinks *)
		(base->data + (index * base->size) + base->offsetToDoubleLinks);
	Assert(sharedDoubleLinks->index == index);

	return sharedDoubleLinks;
}

void *
SharedDoubleLinks_Next(
					   SharedListBase *base,
					   SharedDoublyLinkedHead *head,
					   void *currentEle)
{
	SharedDoubleLinks *currentDoubleLinks;

	Assert(base != NULL);
	Assert(head != NULL);
	Assert(currentEle != NULL);

	currentDoubleLinks = SharedDoubleLinks_FromElement(base, currentEle);

	if (currentDoubleLinks->next == -1)
	{
		Assert(head->last == currentDoubleLinks->index);
		return NULL;
	}
	else
	{
		void	   *nextEle;
		SharedDoubleLinks *nextDoubleLinks;

		Assert(currentDoubleLinks->next >= 0);
		nextEle = SharedListBase_ToElement(base, currentDoubleLinks->next);
		nextDoubleLinks = SharedDoubleLinks_FromElement(base, nextEle);
		Assert(nextDoubleLinks->index == currentDoubleLinks->next);

		return nextEle;
	}

}

void
SharedDoubleLinks_Remove(
						 SharedListBase *base,
						 SharedDoublyLinkedHead *head,
						 void *removeEle)
{
	SharedDoubleLinks *removeDoubleLinks;
	int			index;
	SharedDoubleLinks *prevDoubleLinks = NULL;
	SharedDoubleLinks *nextDoubleLinks = NULL;


	Assert(base != NULL);
	Assert(head != NULL);
	Assert(removeEle != NULL);

	removeDoubleLinks = SharedDoubleLinks_FromElement(base, removeEle);
	index = removeDoubleLinks->index;

	if (removeDoubleLinks->prev == -1 &&
		removeDoubleLinks->next == -1)
	{
		/*
		 * Removing the only one.
		 */
		Assert(head->first == index);
		Assert(head->last == index);
		Assert(head->count == 1);
		head->first = -1;
		head->last = -1;
	}
	else if (removeDoubleLinks->prev == -1)
	{
		/*
		 * Removing the first element.
		 */
		Assert(head->first == index);

		nextDoubleLinks =
			SharedListBase_ToDoubleLinks(base, removeDoubleLinks->next);
		Assert(nextDoubleLinks->prev == index);
		nextDoubleLinks->prev = -1;

		head->first = nextDoubleLinks->index;
	}
	else if (removeDoubleLinks->next == -1)
	{
		Assert(head->last == index);

		/*
		 * Removing the last element.
		 */
		prevDoubleLinks =
			SharedListBase_ToDoubleLinks(base, removeDoubleLinks->prev);
		Assert(prevDoubleLinks->next == index);
		prevDoubleLinks->next = -1;

		head->last = prevDoubleLinks->index;
	}
	else
	{
		/*
		 * Removing a middle element.
		 */
		nextDoubleLinks =
			SharedListBase_ToDoubleLinks(base, removeDoubleLinks->next);
		Assert(nextDoubleLinks->prev == index);
		nextDoubleLinks->prev = removeDoubleLinks->prev;

		prevDoubleLinks =
			SharedListBase_ToDoubleLinks(base, removeDoubleLinks->prev);
		Assert(prevDoubleLinks->next == index);
		prevDoubleLinks->next = removeDoubleLinks->next;
	}

	Assert(head->count >= 1);
	head->count--;

	removeDoubleLinks->prev = -1;
	removeDoubleLinks->next = -1;
}

void
SharedDoublyLinkedHead_AddFirst(
								SharedListBase *base,
								SharedDoublyLinkedHead *head,
								void *ele)
{
	SharedDoubleLinks *eleDoubleLinks;

	Assert(base != NULL);
	Assert(head != NULL);
	Assert(ele != NULL);
	eleDoubleLinks = SharedDoubleLinks_FromElement(base, ele);
	Assert(eleDoubleLinks->prev == -1);
	Assert(eleDoubleLinks->next == -1);

	if (head->first == -1 && head->last == -1)
	{
		Assert(head->count == 0);
		head->first = eleDoubleLinks->index;
		head->last = eleDoubleLinks->index;
	}
	else
	{
		SharedDoubleLinks *firstDoubleLinks;

		Assert(head->count > 0);
		firstDoubleLinks =
			SharedListBase_ToDoubleLinks(base, head->first);
		Assert(firstDoubleLinks->prev == -1);

		eleDoubleLinks->next = head->first;
		head->first = eleDoubleLinks->index;
		firstDoubleLinks->prev = eleDoubleLinks->index;
	}

	head->count++;
}

void
SharedDoublyLinkedHead_AddLast(
							   SharedListBase *base,
							   SharedDoublyLinkedHead *head,
							   void *ele)
{
	SharedDoubleLinks *eleDoubleLinks;

	Assert(base != NULL);
	Assert(head != NULL);
	Assert(ele != NULL);
	eleDoubleLinks = SharedDoubleLinks_FromElement(base, ele);
	Assert(eleDoubleLinks->prev == -1);
	Assert(eleDoubleLinks->next == -1);

	if (head->first == -1 && head->last == -1)
	{
		Assert(head->count == 0);
		head->first = eleDoubleLinks->index;
		head->last = eleDoubleLinks->index;
	}
	else
	{
		SharedDoubleLinks *lastDoubleLinks;

		Assert(head->count > 0);
		Assert(head->first >= 0);
		Assert(head->last >= 0);

		lastDoubleLinks =
			SharedListBase_ToDoubleLinks(base, head->last);
		Assert(lastDoubleLinks->next == -1);

		eleDoubleLinks->prev = lastDoubleLinks->index;
		head->last = eleDoubleLinks->index;
		lastDoubleLinks->next = eleDoubleLinks->index;
	}

	head->count++;
}

void *
SharedDoublyLinkedHead_RemoveFirst(
								   SharedListBase *base,
								   SharedDoublyLinkedHead *head)
{
	void	   *firstEle;
	SharedDoubleLinks *firstDoubleLinks;

	Assert(base != NULL);
	Assert(head != NULL);

	if (head->first == -1)
	{
		Assert(head->count == 0);
		return NULL;
	}

	Assert(head->first >= 0);
	firstEle = SharedListBase_ToElement(base, head->first);
	firstDoubleLinks = SharedDoubleLinks_FromElement(base, firstEle);
	Assert(firstDoubleLinks->index == head->first);

	SharedDoubleLinks_Remove(
							 base,
							 head,
							 firstEle);

	return firstEle;
}

/* ****************************************************************************** */
