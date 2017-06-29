/*-------------------------------------------------------------------------
 *
 * backend_cancel.h
 *		Declarations for backend cancellation messaging
 *
 * Portions Copyright (c) 1996-2017, PostgreSQL Global Development Group
 *
 *	  src/include/utils/backend_cancel.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BACKEND_CANCEL_H
#define BACKEND_CANCEL_H

#define MAX_CANCEL_MSG 128

extern Size CancelBackendMsgShmemSize(void);
extern void BackendCancelShmemInit(void);
extern void BackendCancelInit(int backend_id);

extern int SetBackendCancelMessage(pid_t backend, char *message);
extern bool HasCancelMessage(void);
extern int GetCancelMessage(char **msg, size_t len);

#endif /* BACKEND_CANCEL_H */
