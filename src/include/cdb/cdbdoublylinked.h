/*-------------------------------------------------------------------------
 *
 * cdbdoublylinked.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdoublylinked.h
 *
 *-------------------------------------------------------------------------
 */
 
#ifndef CDBDOUBLYLINKED_H
#define CDBDOUBLYLINKED_H

// Postgres makes lists way too hard.
typedef struct DoubleLinks DoubleLinks;

typedef struct DoublyLinkedHead
{
	DoubleLinks	*first;
	DoubleLinks	*last;

	int32		count;
	
} DoublyLinkedHead;

struct DoubleLinks
{
	DoubleLinks	*next;
	DoubleLinks	*prev;
};

extern void
DoublyLinkedHead_Init(
	DoublyLinkedHead		*head);

extern void* 
DoublyLinkedHead_First(
	int 					offsetToDoubleLinks,
	DoublyLinkedHead		*head);

extern void* 
DoublyLinkedHead_Last(
	int 					offsetToDoubleLinks,
	DoublyLinkedHead		*head);

extern void*
DoublyLinkedHead_Next(
	int						offsetToDoubleLinks,
	DoublyLinkedHead		*head,
	void					*ele);

extern void
DoublyLinkedHead_AddFirst(
	int						offsetToDoubleLinks,
	DoublyLinkedHead		*head,
	void					*ele);

extern void*
DoublyLinkedHead_RemoveLast(
	int						offsetToDoubleLinks,
	DoublyLinkedHead		*head);

extern void
DoubleLinks_Init(
	DoubleLinks		*doubleLinks);

extern void
DoubleLinks_Remove(
	int						offsetToDoubleLinks,
	DoublyLinkedHead		*head,
	void					*ele);

#endif   /* CDBDOUBLYLINKED_H */
