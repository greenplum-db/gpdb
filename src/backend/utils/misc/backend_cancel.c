/*-------------------------------------------------------------------------
 *
 * backend_cancel.c
 *	  Backend cancellation messaging
 *
 *
 * Module for supporting passing a user defined message to a cancelled,
 * or terminated, backend from the user/administrator.
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/backend_cancel.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "executor/instrument.h"
#include "storage/ipc.h"
#include "storage/spin.h"
#include "storage/shmem.h"
#include "utils/backend_cancel.h"

/*
 * Each backend is registered per pid in the array which is indexed by Backend
 * ID. Reading and writing the message is protected by a per-slot spinlock.
 */
typedef struct
{
	pid_t	pid;
	slock_t	mutex;
	char	message[MAX_CANCEL_MSG];
	int		len;
} BackendCancelShmemStruct;

static BackendCancelShmemStruct	*BackendCancelSlots = NULL;
static volatile BackendCancelShmemStruct *MyCancelSlot = NULL;

static void CleanupCancelBackend(int status, Datum argument);

Size
CancelBackendMsgShmemSize(void)
{
	return MaxBackends * sizeof(BackendCancelShmemStruct);
}

void
BackendCancelShmemInit(void)
{
	Size	size = CancelBackendMsgShmemSize();
	bool	found;
	int		i;

	BackendCancelSlots = (BackendCancelShmemStruct *)
		ShmemInitStruct("BackendCancelSlots", size, &found);

	if (!found)
	{
		MemSet(BackendCancelSlots, 0, size);

		for (i = 0; i < MaxBackends; i++)
			SpinLockInit(&(BackendCancelSlots[i].mutex));
	}
}

void
BackendCancelInit(int backend_id)
{
	volatile BackendCancelShmemStruct *slot;

	slot = &BackendCancelSlots[backend_id - 1];

	slot->message[0] = '\0';
	slot->len = 0;
	slot->pid = MyProcPid;

	MyCancelSlot = slot;

	on_shmem_exit(CleanupCancelBackend, Int32GetDatum(backend_id));
}

static void
CleanupCancelBackend(int status, Datum argument)
{
	int backend_id = DatumGetInt32(argument);
	volatile BackendCancelShmemStruct *slot;

	slot = &BackendCancelSlots[backend_id - 1];

	Assert(slot == MyCancelSlot);

	MyCancelSlot = NULL;

	if (slot->len > 0)
		slot->message[0] = '\0';

	slot->len = 0;
	slot->pid = 0;
}

/*
 * Sets a cancellation message for the backend with the specified pid, and
 * returns the length of message actually created. If the returned length
 * is less than the length of the message parameter, truncation has occurred.
 * If the backend wasn't found and no message was set, -1 is returned. If two
 * backends collide in setting a message, the existing message will be
 * overwritten by the last one in.
 */
int
SetBackendCancelMessage(pid_t backend, char *message)
{
	BackendCancelShmemStruct *slot;
	int		i;
	int		message_len;

	if (!message)
		return 0;

	for (i = 0; i < MaxBackends; i++)
	{
		slot = &BackendCancelSlots[i];

		if (slot->pid != 0 && slot->pid == backend)
		{
			SpinLockAcquire(&slot->mutex);
			if (slot->pid != backend)
			{
				SpinLockRelease(&slot->mutex);
				goto error;
			}

			strlcpy(slot->message, message, sizeof(slot->message));
			slot->len = strlen(slot->message);
			message_len = slot->len;
			SpinLockRelease(&slot->mutex);

			return message_len;
		}
	}

error:

	elog(LOG, "Cancellation message requested for missing backend %d by %d",
		 (int) backend, MyProcPid);

	return -1;
}

bool
HasCancelMessage(void)
{
	volatile BackendCancelShmemStruct *slot = MyCancelSlot;
	bool 	has_message = false;

	if (slot != NULL)
	{
		SpinLockAcquire(&slot->mutex);
		has_message = (slot->len > 0);
		SpinLockRelease(&slot->mutex);
	}

	return has_message;
}

/*
 * Return the configured cancellation message and its length. If the returned
 * length is greater than the size of the passed buffer, truncation has been
 * performed. The message is cleared on reading.
 */
int
GetCancelMessage(char **buffer, size_t buf_len)
{
	volatile BackendCancelShmemStruct *slot = MyCancelSlot;
	int		msg_length = 0;

	if (slot != NULL && slot->len > 0)
	{
		SpinLockAcquire(&slot->mutex);
		strlcpy(*buffer, (const char *) slot->message, buf_len);
		msg_length = slot->len;
		slot->len = 0;
		slot->message[0] = '\0';
		SpinLockRelease(&slot->mutex);
	}

	return msg_length;
}
