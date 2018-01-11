/*-------------------------------------------------------------------------
 *
 * cdb_backup_status.c
 *
 * Structures and functions to manipulate and access a linked list of
 * structs that represent event notifications about the progress
 * of a backup or restore operation.
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/bin/pg_dump/cdb/cdb_backup_status.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "pqexpbuffer.h"
#include "libpq-fe.h"
#include <time.h>
#include <pthread.h>
#include "cdb_dump_util.h"
#include "cdb_backup_status.h"

/*
 * AddToStatusOpList: adds the StatusOp* to the end of the internal linked list in the StatusOpList*
 */
void
AddToStatusOpList(StatusOpList * pList, StatusOp * pOp)
{
	pthread_mutex_lock(&pList->mGuard);

	if (pList->pHead == NULL)
		pList->pHead = pOp;
	else
	{
		StatusOp   *pLastOp = pList->pHead;

		while (pLastOp->pNext != NULL)
			pLastOp = pLastOp->pNext;

		pLastOp->pNext = pOp;
	}

	pOp->pNext = NULL;

	pthread_mutex_unlock(&pList->mGuard);
}

/*
 * CreateStatusOp: mallocs memory for a new StatusOp, and
 * initializes its fields based on the passed parameters.
 */
StatusOp *
CreateStatusOp(int TaskID, int TaskRC, const char *pszSuffix, const char *pszMsg)
{
	StatusOp   *pOp = (StatusOp *) calloc(1, sizeof(StatusOp));

	if (pOp == NULL)
		return pOp;

	pOp->pNext = NULL;
	pOp->TaskID = TaskID;
	pOp->TaskRC = TaskRC;
	pOp->pszSuffix = NULL;
	if (pszSuffix != NULL)
		pOp->pszSuffix = strdup(pszSuffix);
	pOp->pszMsg = NULL;
	if (pszMsg != NULL)
		pOp->pszMsg = strdup(pszMsg);

	return pOp;
}

/*
 * CreateStatusOpList: mallocs memory for a new StatusOpList, and initializes its fields.
 * Specifically, it initializes the ionternal linked list to NULL, and the mGuard mutex
 * is initialized to have the recursive property.
 */
StatusOpList *
CreateStatusOpList(void)
{
	pthread_mutexattr_t MyAttr;
	StatusOpList *pList = (StatusOpList *) calloc(1, sizeof(StatusOpList));

	if (pList == NULL)
		return NULL;

	pList->pHead = NULL;
	pthread_mutexattr_init(&MyAttr);
	pthread_mutexattr_settype(&MyAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&pList->mGuard, &MyAttr);

	return pList;
}

/*
 * DestroyStatusOpList: makes sure all the memory in the internal linked list is freed,
 * destroys the mGaurd mutex, and frees the StatusOpList* .
 */
void
DestroyStatusOpList(StatusOpList * pList)
{
	StatusOp   *pHead;

	if (pList == NULL)
		return;

	pthread_mutex_lock(&pList->mGuard);
	pHead = pList->pHead;
	while (pHead != NULL)
	{
		pList->pHead = pHead->pNext;
		FreeStatusOp(pHead);
		pHead = pList->pHead;
	}
	pthread_mutex_unlock(&pList->mGuard);
	pthread_mutex_destroy(&pList->mGuard);
	free(pList);
}

/*
 * FreeStatusOp: frees all the memory alocated for the fields inside the StatusOp* and then
 * frees the StatusOp*.
 */
void
FreeStatusOp(StatusOp * pOp)
{
	if (pOp == NULL)
		return;

	if (pOp->pszSuffix != NULL)
		free(pOp->pszSuffix);

	if (pOp->pszMsg != NULL)
		free(pOp->pszMsg);

	free(pOp);
}

/*
 * Lock: forces a lock of the mGuard mutex
 */
void
Lock(StatusOpList * pList)
{
	pthread_mutex_lock(&pList->mGuard);
}

/*
 * TakeFromStatusOpList: returns the head of the linked list, and makes the
 * second element the new head.
 */
StatusOp *
TakeFromStatusOpList(StatusOpList * pList)
{
	StatusOp   *pOp;

	pthread_mutex_lock(&pList->mGuard);

	pOp = pList->pHead;

	if (pOp != NULL)
	{
		pList->pHead = pOp->pNext;
	}

	pthread_mutex_unlock(&pList->mGuard);

	return pOp;
}

/*
 * Unlock: unlocls the mGuard mutex
 */
void
Unlock(StatusOpList * pList)
{
	pthread_mutex_unlock(&pList->mGuard);
}
