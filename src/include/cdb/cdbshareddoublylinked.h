/*-------------------------------------------------------------------------
 *
 * cdbshareddoublylinked.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbshareddoublylinked.h
 *
 *-------------------------------------------------------------------------
 */
 
#ifndef CDBSHAREDDOUBLYLINKED_H
#define CDBSHAREDDOUBLYLINKED_H

typedef struct SharedDoublyLinkedHead
{
	int		count;
	int		first;
	int		last;
	
} SharedDoublyLinkedHead;

typedef struct SharedDoubleLinks
{
	int		index;
	int		next;
	int		prev;
	
} SharedDoubleLinks;

typedef struct SharedListBase
{
	uint8		*data;
	int			size;
	int			offsetToDoubleLinks;
	
} SharedListBase;

extern void SharedListBase_Init(
	SharedListBase		*base,
	void				*data,
	int					size,
	int					offsetToDoubleLinks);

extern void SharedDoublyLinkedHead_Init(
	SharedDoublyLinkedHead	*head);

extern void SharedDoubleLinks_Init(
	SharedDoubleLinks	*doubleLinks,
	int					index);

extern SharedDoubleLinks* SharedDoubleLinks_FromElement(	
	SharedListBase				*base,
	void						*current);

extern void* SharedListBase_ToElement(	
	SharedListBase		*base,
	int					index);

extern void* SharedDoublyLinkedHead_First(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head);

extern SharedDoubleLinks* SharedListBase_ToDoubleLinks(	
	SharedListBase		*base,
	int					index);

extern void* SharedDoubleLinks_Next(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head,
	void						*currentEle);

extern void SharedDoubleLinks_Remove(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head,
	void						*removeEle);

extern void SharedDoublyLinkedHead_AddFirst(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head,
	void						*ele);

extern void SharedDoublyLinkedHead_AddLast(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head,
	void						*ele);

extern void* SharedDoublyLinkedHead_RemoveFirst(
	SharedListBase				*base,
	SharedDoublyLinkedHead		*head);

#endif   /* CDBSHAREDDOUBLYLINKED_H */

